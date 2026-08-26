#include "Camera/DACameraModeController.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Founder/DAFounderCharacter.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "LOD/DASimulationLODSubsystem.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Performance/DACanonicalSoakHarness.h"
#include "Scalability.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Tests/AutomationEditorCommon.h"
#include "Units/DASquadEntity.h"
#include "UnrealClient.h"
#include "World/DAWorldAsset.h"

namespace
{
    bool ReadPlan(const FString& Path, TSharedPtr<FJsonObject>& OutPlan, FString& OutError)
    {
        FString Document;
        if (!FFileHelper::LoadFileToString(Document, *Path)
            || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Document), OutPlan)
            || !OutPlan.IsValid())
        {
            OutError = TEXT("Runtime benchmark could not load its generated plan.");
            return false;
        }
        return true;
    }

    FDASimulationLODContext ContextForLOD(const int32 LOD)
    {
        switch (LOD)
        {
        case 0: return FDASimulationLODContext::FounderDistrict();
        case 1: return FDASimulationLODContext::NearbyDistrict();
        case 2: return FDASimulationLODContext::DistantActiveCity();
        default: return FDASimulationLODContext::UnloadedMajorRegion();
        }
    }

    AActor* SpawnPrimitive(UWorld& World, UStaticMesh& MeshAsset, const FVector& Location,
        const FVector& Scale)
    {
        AActor* Actor = World.SpawnActor<AActor>(
            AActor::StaticClass(), Location, FRotator::ZeroRotator);
        UStaticMeshComponent* Mesh = Actor == nullptr ? nullptr
            : NewObject<UStaticMeshComponent>(Actor);
        if (Actor == nullptr || Mesh == nullptr) return nullptr;
        Mesh->SetStaticMesh(&MeshAsset);
        Mesh->SetWorldScale3D(Scale);
        Actor->AddInstanceComponent(Mesh);
        Actor->SetRootComponent(Mesh);
        Mesh->RegisterComponent();
        return Actor;
    }

    struct FDARuntimeBenchmarkContext
    {
        bool Materialize(const FString& InPlanPath, const FString& InEvidencePath,
            UWorld* InWorld, FString& OutError)
        {
            PlanPath = InPlanPath;
            EvidencePath = InEvidencePath;
            World = InWorld;
            TSharedPtr<FJsonObject> Plan;
            if (World == nullptr || !ReadPlan(PlanPath, Plan, OutError)) return false;

            const TArray<TSharedPtr<FJsonValue>>* Assets = nullptr;
            const TArray<TSharedPtr<FJsonValue>>* Citizens = nullptr;
            const TArray<TSharedPtr<FJsonValue>>* Squads = nullptr;
            const TArray<TSharedPtr<FJsonValue>>* Vehicles = nullptr;
            const TArray<TSharedPtr<FJsonValue>>* ModeTransitions = nullptr;
            FString EffectPath;
            if (!Plan->TryGetArrayField(TEXT("assets"), Assets) || Assets == nullptr
                || !Plan->TryGetArrayField(TEXT("citizens"), Citizens) || Citizens == nullptr
                || !Plan->TryGetArrayField(TEXT("squads"), Squads) || Squads == nullptr
                || !Plan->TryGetArrayField(TEXT("vehicles"), Vehicles) || Vehicles == nullptr
                || !Plan->TryGetArrayField(TEXT("modeTransition"), ModeTransitions)
                || ModeTransitions == nullptr
                || !Plan->TryGetStringField(TEXT("constructionEffectAsset"), EffectPath))
            {
                OutError = TEXT("Runtime benchmark plan is missing materialization fields.");
                return false;
            }

            int32 DamagedCount = 0;
            int32 EffectCount = 0;
            FVector EffectLocation = FVector::ZeroVector;
            for (const TSharedPtr<FJsonValue>& Value : *Assets)
            {
                const TSharedPtr<FJsonObject> Row = Value.IsValid() ? Value->AsObject() : nullptr;
                const TSharedPtr<FJsonObject>* Grid = nullptr;
                FString Id;
                FString DefinitionId;
                FString PresentationAsset;
                FString ConstructionState;
                bool bConstructionEffect = false;
                int32 X = 0;
                int32 Y = 0;
                if (!Row.IsValid() || !Row->TryGetStringField(TEXT("worldAssetId"), Id)
                    || !Row->TryGetStringField(TEXT("definitionId"), DefinitionId)
                    || !Row->TryGetStringField(TEXT("presentationAsset"), PresentationAsset)
                    || !Row->TryGetStringField(TEXT("constructionState"), ConstructionState)
                    || !Row->TryGetBoolField(TEXT("constructionEffect"), bConstructionEffect)
                    || !Row->TryGetObjectField(TEXT("grid"), Grid) || Grid == nullptr
                    || !(*Grid)->TryGetNumberField(TEXT("x"), X)
                    || !(*Grid)->TryGetNumberField(TEXT("y"), Y))
                {
                    OutError = TEXT("Runtime benchmark received a malformed WorldAsset row.");
                    return false;
                }
                FDAWorldAssetRecord Record;
                if (!FGuid::Parse(Id, Record.WorldAssetId))
                {
                    OutError = TEXT("Runtime benchmark received an invalid WorldAssetId.");
                    return false;
                }
                Record.CardDefinitionId = FName(*DefinitionId);
                Record.CityId = TEXT("player_capital");
                Record.GridOrigin = FIntPoint(X, Y);
                Record.ConstructionState = ConstructionState == TEXT("Damaged")
                    ? EDAConstructionState::Damaged : EDAConstructionState::Operational;
                Record.StructuralIntegrity = ConstructionState == TEXT("Damaged") ? 50.f : 100.f;
                Record.OwnerCivilizationId = TEXT("civilization.synara");
                const FVector Location(X * 400.f, Y * 400.f, 0.f);
                ADAWorldAsset* Actor = World->SpawnActor<ADAWorldAsset>(
                    ADAWorldAsset::StaticClass(), Location, FRotator::ZeroRotator);
                if (Actor == nullptr)
                {
                    OutError = TEXT("Runtime benchmark failed to spawn a gameplay WorldAsset.");
                    return false;
                }
                Actor->InitializeFromRecord(Record);
                UStaticMesh* MeshAsset = LoadObject<UStaticMesh>(nullptr, *PresentationAsset);
                UStaticMeshComponent* Mesh = MeshAsset == nullptr ? nullptr
                    : NewObject<UStaticMeshComponent>(Actor);
                if (Mesh == nullptr)
                {
                    OutError = TEXT("Runtime benchmark could not load an authored gameplay presentation mesh.");
                    return false;
                }
                Mesh->SetStaticMesh(MeshAsset);
                Actor->AddInstanceComponent(Mesh);
                Actor->SetRootComponent(Mesh);
                Mesh->RegisterComponent();
                WorldAssets.Add(Actor);
                if (ConstructionState == TEXT("Damaged")) ++DamagedCount;
                if (bConstructionEffect)
                {
                    ++EffectCount;
                    EffectLocation = Location;
                }
            }

            Instrumentation = TStrongObjectPtr<UDASimulationLODSubsystem>(
                NewObject<UDASimulationLODSubsystem>(GetTransientPackage()));
            UStaticMesh* CitizenMesh = LoadObject<UStaticMesh>(
                nullptr, TEXT("/Engine/BasicShapes/Capsule.Capsule"));
            UStaticMesh* TacticalMesh = LoadObject<UStaticMesh>(
                nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
            if (CitizenMesh == nullptr || TacticalMesh == nullptr)
            {
                OutError = TEXT("Runtime benchmark could not load engine primitive representation meshes.");
                return false;
            }
            int32 LODCounts[4] = {};
            for (const TSharedPtr<FJsonValue>& Value : *Citizens)
            {
                const TSharedPtr<FJsonObject> Row = Value.IsValid() ? Value->AsObject() : nullptr;
                FString Id;
                int32 LOD = INDEX_NONE;
                if (!Row.IsValid() || !Row->TryGetStringField(TEXT("citizenId"), Id)
                    || !Row->TryGetNumberField(TEXT("lod"), LOD) || LOD < 0 || LOD > 3
                    || !Instrumentation->RegisterNamedCitizen(FName(*Id))
                    || !Instrumentation->TransitionNamedCitizen(FName(*Id), ContextForLOD(LOD)))
                {
                    OutError = TEXT("Runtime benchmark failed to materialize a named citizen LOD.");
                    return false;
                }
                ++LODCounts[LOD];
                if (LOD <= 1)
                {
                    AActor* CitizenActor = SpawnPrimitive(*World, *CitizenMesh,
                        FVector(LOD * 5000.f, LODCounts[LOD] * 100.f, 50.f),
                        FVector(0.35f, 0.35f, 0.9f));
                    if (CitizenActor == nullptr)
                    {
                        OutError = TEXT("Runtime benchmark failed to create a visible LOD0/1 citizen.");
                        return false;
                    }
                    VisibleCitizenActors.Add(CitizenActor);
                }
            }

            int32 Allied = 0;
            int32 Enemy = 0;
            for (const TSharedPtr<FJsonValue>& Value : *Squads)
            {
                const TSharedPtr<FJsonObject> Row = Value.IsValid() ? Value->AsObject() : nullptr;
                FString Side;
                if (!Row.IsValid() || !Row->TryGetStringField(TEXT("side"), Side))
                {
                    OutError = TEXT("Runtime benchmark failed to read a squad side.");
                    return false;
                }
                TStrongObjectPtr<UDASquadEntity> Squad(NewObject<UDASquadEntity>(GetTransientPackage()));
                if (!Squad->Initialize(false, Side == TEXT("allied")
                    ? EDASquadDoctrine::ProtectInfrastructure : EDASquadDoctrine::AggressiveAssault))
                {
                    OutError = TEXT("Runtime benchmark failed to initialize a tactical squad.");
                    return false;
                }
                Side == TEXT("allied") ? ++Allied : ++Enemy;
                AActor* SquadActor = SpawnPrimitive(*World, *TacticalMesh,
                    FVector(Side == TEXT("allied") ? -1500.f : 5500.f,
                        TacticalEntities.Num() * 500.f, 100.f),
                    FVector(1.5f, 1.5f, 1.f));
                if (SquadActor == nullptr)
                {
                    OutError = TEXT("Runtime benchmark failed to spawn a squad representation.");
                    return false;
                }
                TacticalActors.Add(SquadActor);
                TacticalEntities.Add(MoveTemp(Squad));
            }
            for (const TSharedPtr<FJsonValue>& Value : *Vehicles)
            {
                if (!Value.IsValid() || !Value->AsObject().IsValid())
                {
                    OutError = TEXT("Runtime benchmark received a malformed vehicle row.");
                    return false;
                }
                TStrongObjectPtr<UDASquadEntity> Vehicle(NewObject<UDASquadEntity>(GetTransientPackage()));
                if (!Vehicle->Initialize(true, EDASquadDoctrine::Hold))
                {
                    OutError = TEXT("Runtime benchmark failed to initialize its vehicle.");
                    return false;
                }
                AActor* VehicleActor = SpawnPrimitive(*World, *TacticalMesh,
                    FVector(-2500.f, 1000.f, 100.f), FVector(3.f, 1.5f, 1.f));
                if (VehicleActor == nullptr)
                {
                    OutError = TEXT("Runtime benchmark failed to spawn its vehicle representation.");
                    return false;
                }
                TacticalActors.Add(VehicleActor);
                TacticalEntities.Add(MoveTemp(Vehicle));
            }

            Weather = World->SpawnActor<AExponentialHeightFog>();
            UNiagaraSystem* Effect = LoadObject<UNiagaraSystem>(nullptr, *EffectPath);
            ConstructionEffect = Effect == nullptr ? nullptr
                : UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Effect, EffectLocation);
            if (ConstructionEffect != nullptr)
            {
                ConstructionEffect->SetAutoDestroy(false);
                ConstructionEffect->Activate(true);
            }
            Founder = World->SpawnActor<ADAFounderCharacter>();
            ModeController = Founder == nullptr ? nullptr
                : NewObject<UDACameraModeController>(Founder, TEXT("DABenchmarkModeController"));
            if (ModeController != nullptr)
            {
                ModeController->RegisterComponent();
            }

            if (WorldAssets.Num() != 50 || DamagedCount != 1 || EffectCount != 1
                || LODCounts[0] != 30 || LODCounts[1] != 30 || LODCounts[2] != 40
                || LODCounts[3] != 20 || VisibleCitizenActors.Num() != 60
                || Allied != 3 || Enemy != 3 || Vehicles->Num() != 1
                || TacticalEntities.Num() != 7 || TacticalActors.Num() != 7
                || Weather == nullptr || ConstructionEffect == nullptr
                || Founder == nullptr || ModeController == nullptr
                || !ModeController->InitializeContext(Founder, World, nullptr)
                || ModeTransitions->Num() != 3
                || (*ModeTransitions)[0]->AsString() != TEXT("City")
                || (*ModeTransitions)[1]->AsString() != TEXT("Command")
                || (*ModeTransitions)[2]->AsString() != TEXT("Founder"))
            {
                OutError = TEXT("Runtime world did not materialize the exact 50/120/weather/3+3/vehicle/VFX/damage load.");
                return false;
            }

            FSystemResolution::RequestResolutionChange(1920, 1080, EWindowMode::Windowed);
            Scalability::FQualityLevels Quality;
            Quality.SetFromSingleQualityLevel(2);
            Quality.ResolutionQuality = 100.f;
            Scalability::SetQualityLevels(Quality);
            const Scalability::FQualityLevels Applied = Scalability::GetQualityLevels();
            if (GSystemResolution.ResX != 1920 || GSystemResolution.ResY != 1080
                || Applied.ViewDistanceQuality != 2 || Applied.AntiAliasingQuality != 2
                || Applied.ShadowQuality != 2 || Applied.GlobalIlluminationQuality != 2
                || Applied.ReflectionQuality != 2 || Applied.PostProcessQuality != 2
                || Applied.TextureQuality != 2 || Applied.EffectsQuality != 2
                || Applied.FoliageQuality != 2 || Applied.ShadingQuality != 2)
            {
                OutError = TEXT("Runtime benchmark could not apply exact 1920x1080 High settings.");
                return false;
            }

            Instrumentation->BeginBenchmarkCapture();
            LastFrameSeconds = FPlatformTime::Seconds();
            TransitionIndex = 0;
            if (!ModeController->RequestMode(EDAPlayMode::City))
            {
                OutError = TEXT("Runtime benchmark could not begin City mode transition.");
                return false;
            }
            StartedSeconds = LastFrameSeconds;
            return true;
        }

        bool Tick(FAutomationTestBase& Test)
        {
            const double Now = FPlatformTime::Seconds();
            const double Delta = Now - LastFrameSeconds;
            LastFrameSeconds = Now;
            if (!Instrumentation->RecordFrameSeconds(Delta))
            {
                Test.AddError(TEXT("Runtime benchmark rejected a real frame timer sample."));
                return true;
            }
            ++FrameSamples;
            ModeController->TickComponent(static_cast<float>(Delta), LEVELTICK_All, nullptr);
            if (!ModeController->IsTransitioning() && TransitionIndex < 3)
            {
                if (!Instrumentation->RecordModeTransitionSeconds(
                        ModeController->GetLastTransitionDurationSeconds()))
                {
                    Test.AddError(TEXT("Runtime benchmark rejected a real mode-transition timer."));
                    return true;
                }
                ++TransitionIndex;
                if (TransitionIndex < 3)
                {
                    static const EDAPlayMode Sequence[] = {
                        EDAPlayMode::City, EDAPlayMode::Command, EDAPlayMode::Founder};
                    if (!ModeController->RequestMode(Sequence[TransitionIndex]))
                    {
                        Test.AddError(TEXT("Runtime benchmark transition sequence was rejected."));
                        return true;
                    }
                }
            }
            if (Now - StartedSeconds > 120.0)
            {
                Test.AddError(TEXT("Runtime benchmark timed out before collecting 600 frames and all transitions."));
                return true;
            }
            if (FrameSamples < 600 || TransitionIndex != 3)
            {
                return false;
            }
            return Complete(Test);
        }

        bool Complete(FAutomationTestBase& Test)
        {
            const FString FirstDirectory = FPaths::Combine(FPaths::ProjectIntermediateDir(),
                TEXT("DA_Task27_RuntimeSoakA"));
            const FString SecondDirectory = FPaths::Combine(FPaths::ProjectIntermediateDir(),
                TEXT("DA_Task27_RuntimeSoakB"));
            IFileManager::Get().DeleteDirectory(*FirstDirectory, false, true);
            IFileManager::Get().DeleteDirectory(*SecondDirectory, false, true);
            const DA::PerformanceTests::FDASoakResult First =
                DA::PerformanceTests::RunCanonicalSoak(FirstDirectory, Instrumentation.Get());
            const DA::PerformanceTests::FDASoakResult Second =
                DA::PerformanceTests::RunCanonicalSoak(SecondDirectory);
            const FDABenchmarkSummary Performance = Instrumentation->FinishBenchmarkCapture(false);
            if (!First.bAdvanced || !Second.bAdvanced || First.FinalSave != Second.FinalSave
                || Performance.FrameSampleCount < 600
                || Performance.DevelopmentCycleSampleCount != 1000
                || Performance.ModeTransitionSeconds.Num() != 3)
            {
                Test.AddError(TEXT("Runtime benchmark did not complete its measured canonical owners."));
                return true;
            }

            FDABenchmarkEvidenceCapture Capture;
            Capture.Performance = Performance;
            FString StorageClass;
            FParse::Value(FCommandLine::Get(), TEXT("DABenchmarkStorage="), StorageClass);
            Capture.Hardware = FDABenchmarkHardwareCapture::Detect(StorageClass);
            Capture.Soak.DevelopmentCycles = static_cast<int32>(
                First.Campaign.WorldState.ClockAuthority.CurrentDevelopmentCycle);
            Capture.Soak.EnabledEventCount = First.EnabledEventCount;
            Capture.Soak.PlayerEconomyDevelopmentCycles = static_cast<int32>(
                First.Campaign.LiveSignals.ResolvedDevelopmentCycles);
            Capture.Soak.TradeWorldTicks = static_cast<int32>(
                First.Campaign.WorldState.Trade.LastProcessedWorldTick);
            Capture.Soak.ForgeweaveWorldTicks = static_cast<int32>(
                First.Campaign.WorldState.Forgeweave.LastProcessedWorldTick);
            Capture.Soak.UtilityTopologyNodeCount = First.UtilityTopologyNodeCount;
            Capture.Soak.bUtilityTopologyResolved = First.bUtilityTopologyResolved;
            Capture.Soak.bPopulationStayedNonnegative = First.bPopulationStayedNonnegative;
            Capture.Soak.EventEmissionCount = First.EventEmissionCount;
            Capture.Soak.EventTransitionCount = First.EventTransitionCount;
            Capture.Soak.bEventRunawayDetected = First.bEventRunawayDetected;
            Capture.Soak.bStaleRequiredQuestDetected = First.bStaleRequiredQuestDetected;
            Capture.Soak.bRequiredQuestResolutionFailed = First.bQuestResolutionFailed;
            Capture.Soak.InitialSaveBytes = First.InitialSaveBytes;
            Capture.Soak.FinalSaveBytes = First.FinalSaveBytes;
            Capture.Soak.RepeatabilityDigestA = First.FinalSaveSha256;
            Capture.Soak.RepeatabilityDigestB = Second.FinalSaveSha256;
            FString Error;
            if (!Capture.WriteCandidate(EvidencePath, PlanPath, Error))
            {
                Test.AddError(TEXT("Production evidence writer rejected the runtime capture: ") + Error);
            }
            IFileManager::Get().DeleteDirectory(*FirstDirectory, false, true);
            IFileManager::Get().DeleteDirectory(*SecondDirectory, false, true);
            return true;
        }

        FString PlanPath;
        FString EvidencePath;
        UWorld* World = nullptr;
        TStrongObjectPtr<UDASimulationLODSubsystem> Instrumentation;
        TArray<TObjectPtr<ADAWorldAsset>> WorldAssets;
        TArray<TObjectPtr<AActor>> VisibleCitizenActors;
        TArray<TStrongObjectPtr<UDASquadEntity>> TacticalEntities;
        TArray<TObjectPtr<AActor>> TacticalActors;
        TObjectPtr<AExponentialHeightFog> Weather = nullptr;
        TObjectPtr<UNiagaraComponent> ConstructionEffect = nullptr;
        TObjectPtr<ADAFounderCharacter> Founder = nullptr;
        TObjectPtr<UDACameraModeController> ModeController = nullptr;
        double LastFrameSeconds = 0.0;
        double StartedSeconds = 0.0;
        int32 TransitionIndex = 0;
        int32 FrameSamples = 0;
    };

    DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FDARunRuntimeBenchmark,
        TSharedRef<FDARuntimeBenchmarkContext>, Context, FAutomationTestBase*, Test);

    bool FDARunRuntimeBenchmark::Update()
    {
        return Test == nullptr || Context->Tick(*Test);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDARuntimeBenchmarkSpec,
    "Dominion.Performance.RuntimeBenchmark.MeasuredVerticalSlice",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDARuntimeBenchmarkSpec::RunTest(const FString&)
{
    FString PlanPath;
    FString EvidencePath;
    FString BudgetPath;
    if (!FParse::Value(FCommandLine::Get(), TEXT("DABenchmarkPlan="), PlanPath)
        || !FParse::Value(FCommandLine::Get(), TEXT("DABenchmarkBudget="), BudgetPath)
        || !FParse::Value(FCommandLine::Get(), TEXT("DABenchmarkEvidenceCandidate="), EvidencePath))
    {
        AddInfo(TEXT("Runtime capture remains NOT_RUN without runner-owned plan/budget/evidence arguments."));
        return true;
    }
    if (!FPaths::FileExists(BudgetPath))
    {
        AddError(TEXT("Runtime benchmark budget path is missing."));
        return false;
    }
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    TSharedRef<FDARuntimeBenchmarkContext> Context = MakeShared<FDARuntimeBenchmarkContext>();
    FString Error;
    if (!Context->Materialize(PlanPath, EvidencePath, World, Error))
    {
        AddError(Error);
        return false;
    }
    ADD_LATENT_AUTOMATION_COMMAND(FDARunRuntimeBenchmark(Context, this));
    return true;
}
