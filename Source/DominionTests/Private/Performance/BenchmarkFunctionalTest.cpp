#include "LOD/DASimulationLODSubsystem.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    bool ValidateGeneratedPlan(const FString& Path, FString& OutError)
    {
        FString Document;
        TSharedPtr<FJsonObject> Root;
        const TArray<TSharedPtr<FJsonValue>>* Assets = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Citizens = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Squads = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Vehicles = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Transitions = nullptr;
        const TSharedPtr<FJsonObject>* Weather = nullptr;
        int32 DevelopmentCycles = 0;
        int32 EnabledEvents = 0;
        if (!FFileHelper::LoadFileToString(Document, *Path)
            || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Document), Root)
            || !Root.IsValid()
            || !Root->TryGetArrayField(TEXT("assets"), Assets) || Assets == nullptr
            || !Root->TryGetArrayField(TEXT("citizens"), Citizens) || Citizens == nullptr
            || !Root->TryGetArrayField(TEXT("squads"), Squads) || Squads == nullptr
            || !Root->TryGetArrayField(TEXT("vehicles"), Vehicles) || Vehicles == nullptr
            || !Root->TryGetArrayField(TEXT("modeTransition"), Transitions) || Transitions == nullptr
            || !Root->TryGetObjectField(TEXT("weather"), Weather) || Weather == nullptr
            || !Root->TryGetNumberField(TEXT("developmentCycles"), DevelopmentCycles)
            || !Root->TryGetNumberField(TEXT("enabledEventCount"), EnabledEvents))
        {
            OutError = TEXT("Benchmark runner plan is missing or malformed.");
            return false;
        }

        TSet<FString> AssetIds;
        int32 DamagedBuildings = 0;
        int32 ConstructionEffects = 0;
        for (const TSharedPtr<FJsonValue>& Value : *Assets)
        {
            const TSharedPtr<FJsonObject> Asset = Value.IsValid() ? Value->AsObject() : nullptr;
            FString Id;
            FString State;
            bool bConstructionEffect = false;
            if (!Asset.IsValid() || !Asset->TryGetStringField(TEXT("worldAssetId"), Id)
                || !Asset->TryGetStringField(TEXT("constructionState"), State)
                || !Asset->TryGetBoolField(TEXT("constructionEffect"), bConstructionEffect)
                || Id.IsEmpty() || AssetIds.Contains(Id))
            {
                OutError = TEXT("Benchmark asset plan requires 50 unique valid WorldAsset rows.");
                return false;
            }
            AssetIds.Add(Id);
            DamagedBuildings += State == TEXT("Damaged") ? 1 : 0;
            ConstructionEffects += bConstructionEffect ? 1 : 0;
        }

        int32 CitizensByLOD[4] = {};
        TSet<FString> CitizenIds;
        for (const TSharedPtr<FJsonValue>& Value : *Citizens)
        {
            const TSharedPtr<FJsonObject> Citizen = Value.IsValid() ? Value->AsObject() : nullptr;
            FString Id;
            int32 LOD = INDEX_NONE;
            if (!Citizen.IsValid() || !Citizen->TryGetStringField(TEXT("citizenId"), Id)
                || !Citizen->TryGetNumberField(TEXT("lod"), LOD)
                || Id.IsEmpty() || CitizenIds.Contains(Id) || LOD < 0 || LOD > 3)
            {
                OutError = TEXT("Benchmark citizen plan requires 120 unique CitizenIDs across LOD0-3.");
                return false;
            }
            CitizenIds.Add(Id);
            ++CitizensByLOD[LOD];
        }

        int32 AlliedSquads = 0;
        int32 EnemySquads = 0;
        TSet<FString> SquadIds;
        for (const TSharedPtr<FJsonValue>& Value : *Squads)
        {
            const TSharedPtr<FJsonObject> Squad = Value.IsValid() ? Value->AsObject() : nullptr;
            FString Id;
            FString Side;
            if (!Squad.IsValid() || !Squad->TryGetStringField(TEXT("squadId"), Id)
                || !Squad->TryGetStringField(TEXT("side"), Side)
                || Id.IsEmpty() || SquadIds.Contains(Id)
                || (Side != TEXT("allied") && Side != TEXT("enemy")))
            {
                OutError = TEXT("Benchmark squad plan requires unique allied/enemy rows.");
                return false;
            }
            SquadIds.Add(Id);
            AlliedSquads += Side == TEXT("allied") ? 1 : 0;
            EnemySquads += Side == TEXT("enemy") ? 1 : 0;
        }

        bool bWeatherActive = false;
        const bool bExactTransition = Transitions->Num() == 3
            && (*Transitions)[0]->AsString() == TEXT("City")
            && (*Transitions)[1]->AsString() == TEXT("Command")
            && (*Transitions)[2]->AsString() == TEXT("Founder");
        if (Assets->Num() != 50 || AssetIds.Num() != 50 || DamagedBuildings != 1
            || ConstructionEffects != 1 || Citizens->Num() != 120
            || CitizensByLOD[0] != 30 || CitizensByLOD[1] != 30
            || CitizensByLOD[2] != 40 || CitizensByLOD[3] != 20
            || Squads->Num() != 6 || AlliedSquads != 3 || EnemySquads != 3
            || Vehicles->Num() != 1
            || !(*Weather)->TryGetBoolField(TEXT("active"), bWeatherActive)
            || !bWeatherActive || !bExactTransition || DevelopmentCycles != 1000
            || EnabledEvents != 6)
        {
            OutError = TEXT("Generated plan drifted from the exact vertical-slice benchmark workload.");
            return false;
        }
        OutError.Reset();
        return true;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDABenchmarkSceneContractTest,
    "Dominion.Performance.BenchmarkScene.ExactVerticalSliceLoad",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDABenchmarkSceneContractTest::RunTest(const FString&)
{
    const FDABenchmarkSceneLoad ExactLoad = FDABenchmarkSceneLoad::VerticalSlice();
    FString Error;
    TestTrue("The deterministic scene load is internally valid", ExactLoad.Validate(Error));
    TestEqual("Scene places 50 gameplay assets", ExactLoad.GameplayAssetCount, 50);
    TestEqual("Scene represents 120 citizens across all LODs", ExactLoad.GetCitizenCount(), 120);
    TestEqual("Scene has three allied squads", ExactLoad.AlliedSquadCount, 3);
    TestEqual("Scene has three enemy squads", ExactLoad.EnemySquadCount, 3);
    TestEqual("Scene has one vehicle", ExactLoad.VehicleCount, 1);
    TestEqual("Scene has one construction effect", ExactLoad.ConstructionEffectCount, 1);
    TestEqual("Scene has one damaged building", ExactLoad.DamagedBuildingCount, 1);
    TestTrue("Weather is active", ExactLoad.bWeatherActive);
    TestTrue("Transition order is City to Command to Founder",
        ExactLoad.ModeTransition == TArray<FName>({TEXT("City"), TEXT("Command"), TEXT("Founder")}));

    FString PlanPath;
    if (FParse::Value(FCommandLine::Get(), TEXT("DABenchmarkPlan="), PlanPath))
    {
        TestTrue("Runner-generated plan retains the exact source contract",
            ValidateGeneratedPlan(PlanPath, Error));
    }
    return true;
}
