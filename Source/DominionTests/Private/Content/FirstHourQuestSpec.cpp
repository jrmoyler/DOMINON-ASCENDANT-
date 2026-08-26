#include "Content/DAFirstHourQuestContent.h"
#include "Content/DAFirstHourContentRegistrySubsystem.h"
#include "Citizens/DAJobSystem.h"
#include "Economy/DAEconomyTypes.h"
#include "Economy/DAEconomySubsystem.h"
#include "Factions/DAFactionSystem.h"
#include "Factions/DASystemicPressureSystem.h"
#include "Fixtures/DAGameInstanceSubsystemFixture.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "HAL/FileManager.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Narrative/DAFirstHourCampaignRuntime.h"
#include "Narrative/DAFirstHourCampaignCoordinatorSubsystem.h"
#include "Narrative/DAPromiseLedger.h"
#include "Narrative/DAQuestRuntime.h"
#include "Networks/DAUtilityNetwork.h"
#include "Save/DASaveService.h"
#include "Save/DASaveJsonFields.h"
#include "Regions/DAWorldStateSubsystem.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Time/DASimulationClockSubsystem.h"
#include "UObject/Package.h"
#include "UObject/GarbageCollection.h"

BEGIN_DEFINE_SPEC(FDAFirstHourQuestSpec, "Dominion.Content.FirstHourQuests",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FDAFirstHourQuestSpec)

namespace
{
    FDAWorldAssetRecord& AddAsset(FDACampaignSnapshot& Campaign, const FName DefinitionId,
        const int32 Index, const EDAConstructionState State = EDAConstructionState::Operational)
    {
        FDAWorldAssetRecord& Asset = Campaign.WorldAssets.Emplace_GetRef();
        Asset.WorldAssetId = FGuid(0xDA200000u, Index, Index + 1, Index + 2);
        Asset.CardDefinitionId = DefinitionId; Asset.CityId = TEXT("player_capital");
        Asset.OwnerCivilizationId = TEXT("civilization.synara"); Asset.ConstructionState = State;
        return Asset;
    }

    FDAFirstHourProgressionContext Context(const int64 Tick, const FDACitySimulationState& City,
        const FDAUtilityNetwork& Utilities, const TArray<FDAFactionState>& Factions,
        const UDASystemicPressureSystem& Pressure, const EDAFirstHourPlayerAction Action = EDAFirstHourPlayerAction::None)
    {
        (void)Factions; (void)Pressure;
        FDAFirstHourProgressionContext Result; Result.WorldTick = Tick; Result.PlayerAction = Action;
        Result.CityState = &City; Result.UtilityNetwork = &Utilities; return Result;
    }

    bool Apply(FDACampaignSnapshot& Campaign, const FDAFirstHourQuestManifest& Manifest, const FName QuestId,
        FDAFirstHourProgressionContext Signal)
    { return FDAFirstHourCampaignRuntime::AdvanceQuest(Manifest, QuestId, Signal, Campaign) == EDAFirstHourCampaignResult::Applied; }

    bool TamperEffectWithValidChecksum(const FString& Directory, const FString& Slot)
    {
        const FString Path = FPaths::Combine(Directory, Slot + TEXT(".dasave")); FString Json; TSharedPtr<FJsonObject> Root;
        if (!FFileHelper::LoadFileToString(Json, *Path)) return false;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
        if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return false;
        const TSharedPtr<FJsonObject>* Campaign = nullptr; const TSharedPtr<FJsonObject>* Narrative = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Effects = nullptr;
        if (!Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign) || Campaign == nullptr
            || !(*Campaign)->TryGetObjectField(FDASaveJsonFields::NarrativeState, Narrative) || Narrative == nullptr
            || !(*Narrative)->TryGetArrayField(TEXT("questContentEffectRecords"), Effects) || Effects == nullptr || Effects->IsEmpty()) return false;
        (*Effects)[0]->AsObject()->SetNumberField(TEXT("dependencyDelta"), 99.0);
        const TSharedRef<FJsonObject> Material = MakeShared<FJsonObject>();
        Material->SetNumberField(FDASaveJsonFields::SchemaVersion, FDASaveService::CurrentSchemaVersion);
        Material->SetNumberField(FDASaveJsonFields::ContentVersion,
            FDASaveService::CurrentContentVersion);
        Material->SetNumberField(FDASaveJsonFields::BuildVersion,
            FDASaveService::CurrentBuildVersion);
        Material->SetObjectField(FDASaveJsonFields::Campaign, Campaign->ToSharedRef()); FString MaterialJson;
        const TSharedRef<TJsonWriter<>> MaterialWriter = TJsonWriterFactory<>::Create(&MaterialJson);
        FJsonSerializer::Serialize(Material, MaterialWriter); FTCHARToUTF8 Utf8(*MaterialJson);
        Root->SetStringField(FDASaveJsonFields::Checksum, FString::Printf(TEXT("%08X"), FCrc::MemCrc32(Utf8.Get(), Utf8.Length())));
        Json.Reset(); const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
        if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer)) return false;
        return FFileHelper::SaveStringToFile(Json, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }

    bool DriveToReplacementChoice(FDACampaignSnapshot& Campaign, const FDAFirstHourQuestManifest& Manifest,
        FDACitySimulationState& City, FDAUtilityNetwork& Utilities, TArray<FDAFactionState>& Factions,
        UDASystemicPressureSystem& Pressure)
    {
        int64 Tick = 1;
        FDAWorldAssetRecord& Hall = AddAsset(Campaign, TEXT("special.founder_hall"), 1, EDAConstructionState::Foundation);
        Utilities.RegisterNode(EDAUtilityType::Power, Hall.WorldAssetId, 5.f, 1.f);
        if (FDAFirstHourCampaignRuntime::TryStartQuest(Manifest, TEXT("quest.wake_the_hall"), Context(Tick++, City, Utilities, Factions, Pressure), Campaign) != EDAFirstHourCampaignResult::Applied
            || !Apply(Campaign, Manifest, TEXT("quest.wake_the_hall"), Context(Tick++, City, Utilities, Factions, Pressure))
            || !Apply(Campaign, Manifest, TEXT("quest.wake_the_hall"), Context(Tick++, City, Utilities, Factions, Pressure, EDAFirstHourPlayerAction::ReachFounderHall))
            || !Apply(Campaign, Manifest, TEXT("quest.wake_the_hall"), Context(Tick++, City, Utilities, Factions, Pressure, EDAFirstHourPlayerAction::RestoreFounderHallPower))) return false;
        Hall.ConstructionState = EDAConstructionState::Operational;
        FDAFirstHourProgressionContext WakeChoice = Context(Tick++, City, Utilities, Factions, Pressure);
        WakeChoice.ChoiceBranchTag = TEXT("continue_activation");
        if (!Apply(Campaign, Manifest, TEXT("quest.wake_the_hall"), WakeChoice)
            || !Apply(Campaign, Manifest, TEXT("quest.wake_the_hall"), Context(Tick++, City, Utilities, Factions, Pressure, EDAFirstHourPlayerAction::ActivateFounderHallCore))) return false;

        if (FDAFirstHourCampaignRuntime::TryStartQuest(Manifest, TEXT("quest.a_place_to_stay"), Context(Tick++, City, Utilities, Factions, Pressure), Campaign) != EDAFirstHourCampaignResult::Applied
            || !Apply(Campaign, Manifest, TEXT("quest.a_place_to_stay"), Context(Tick++, City, Utilities, Factions, Pressure))
            || !Apply(Campaign, Manifest, TEXT("quest.a_place_to_stay"), Context(Tick++, City, Utilities, Factions, Pressure, EDAFirstHourPlayerAction::InspectAdaptiveHabitatCard))) return false;
        FDAWorldAssetRecord& Habitat = AddAsset(Campaign, TEXT("synara.adaptive_habitat"), 2, EDAConstructionState::Foundation);
        const FGuid HabitatId = Habitat.WorldAssetId;
        if (!Apply(Campaign, Manifest, TEXT("quest.a_place_to_stay"), Context(Tick++, City, Utilities, Factions, Pressure, EDAFirstHourPlayerAction::PlaceAdaptiveHabitat))) return false;
        Habitat.ConstructionState = EDAConstructionState::Operational;
        if (!Apply(Campaign, Manifest, TEXT("quest.a_place_to_stay"), Context(Tick++, City, Utilities, Factions, Pressure))) return false;
        FDACitizenRecord Nia = UDAJobSystem::CreateNamedCitizenRoster().FilterByPredicate([](const FDACitizenRecord& C) { return C.CitizenId == TEXT("citizen.synara.nia_vale"); })[0];
        Nia.HomeAssetId = HabitatId; Nia.CityId = TEXT("player_capital"); City.Citizens.Add(Nia);
        if (!Apply(Campaign, Manifest, TEXT("quest.a_place_to_stay"), Context(Tick++, City, Utilities, Factions, Pressure))) return false;

        if (FDAFirstHourCampaignRuntime::TryStartQuest(Manifest, TEXT("quest.power_water_people"), Context(Tick++, City, Utilities, Factions, Pressure), Campaign) != EDAFirstHourCampaignResult::Applied
            || !Apply(Campaign, Manifest, TEXT("quest.power_water_people"), Context(Tick++, City, Utilities, Factions, Pressure))) return false;
        const FGuid GridId = AddAsset(Campaign, TEXT("universal.microgrid_station"), 3).WorldAssetId;
        if (!Apply(Campaign, Manifest, TEXT("quest.power_water_people"), Context(Tick++, City, Utilities, Factions, Pressure))) return false;
        const FGuid WaterId = AddAsset(Campaign, TEXT("universal.water_reclaimer"), 4).WorldAssetId;
        if (!Apply(Campaign, Manifest, TEXT("quest.power_water_people"), Context(Tick++, City, Utilities, Factions, Pressure))) return false;
        FDAFirstHourProgressionContext CoverageChoice = Context(Tick++, City, Utilities, Factions, Pressure);
        CoverageChoice.ChoiceBranchTag = TEXT("within_coverage");
        if (!Apply(Campaign, Manifest, TEXT("quest.power_water_people"), CoverageChoice)) return false;
        Utilities.RegisterNode(EDAUtilityType::Power, GridId, 10.f, 0.f); Utilities.RegisterNode(EDAUtilityType::Power, HabitatId, 0.f, 2.f); Utilities.ConnectNodes(EDAUtilityType::Power, GridId, HabitatId);
        Utilities.RegisterNode(EDAUtilityType::Water, WaterId, 10.f, 0.f); Utilities.RegisterNode(EDAUtilityType::Water, HabitatId, 0.f, 2.f); Utilities.ConnectNodes(EDAUtilityType::Water, WaterId, HabitatId);
        if (!Apply(Campaign, Manifest, TEXT("quest.power_water_people"), Context(Tick++, City, Utilities, Factions, Pressure))
            || !Apply(Campaign, Manifest, TEXT("quest.power_water_people"), Context(Tick++, City, Utilities, Factions, Pressure))) return false;

        if (FDAFirstHourCampaignRuntime::TryStartQuest(Manifest, TEXT("quest.nia_needs_a_job"), Context(Tick++, City, Utilities, Factions, Pressure), Campaign) != EDAFirstHourCampaignResult::Applied
            || !Apply(Campaign, Manifest, TEXT("quest.nia_needs_a_job"), Context(Tick++, City, Utilities, Factions, Pressure))
            || !Apply(Campaign, Manifest, TEXT("quest.nia_needs_a_job"), Context(Tick++, City, Utilities, Factions, Pressure, EDAFirstHourPlayerAction::SpeakToNia))) return false;
        FDAWorldAssetRecord& Tower = AddAsset(Campaign, TEXT("synara.cognitive_operations_tower"), 5);
        if (!Apply(Campaign, Manifest, TEXT("quest.nia_needs_a_job"), Context(Tick++, City, Utilities, Factions, Pressure))) return false;
        FDAJobOpening Opening; Opening.JobId = TEXT("job.synara.cognitive_operations_tower.operator"); Opening.CityId = TEXT("player_capital");
        Opening.FacilityWorldAssetId = Tower.WorldAssetId; Opening.OpenPositions = 2; City.JobOpenings.Add(Opening);
        FDAJobAssignment NiaAssignment; NiaAssignment.CitizenId = TEXT("citizen.synara.nia_vale"); NiaAssignment.JobId = Opening.JobId;
        NiaAssignment.FacilityWorldAssetId = Tower.WorldAssetId; City.JobAssignments.Add(NiaAssignment);
        City.Citizens[0].JobId = Opening.JobId;
        if (!Apply(Campaign, Manifest, TEXT("quest.nia_needs_a_job"), Context(Tick++, City, Utilities, Factions, Pressure))
            || !Apply(Campaign, Manifest, TEXT("quest.nia_needs_a_job"), Context(Tick++, City, Utilities, Factions, Pressure))) return false;

        Campaign.CollectionState.AddInstance(TEXT("synara.autonomous_exchange"), EDAAcquisitionSource::WorldDiscovery, Tick);
        if (!Campaign.SynaraState.ApplyDependencyReason(TEXT("action.test.canonical_dependency_signal"), 30.0, Tick)) return false;
        Pressure.SetDependency(Pressure.GetDependency(Campaign));
        if (FDAFirstHourCampaignRuntime::TryStartQuest(Manifest, TEXT("quest.replacement_model"), Context(Tick++, City, Utilities, Factions, Pressure), Campaign) != EDAFirstHourCampaignResult::Applied
            || !Apply(Campaign, Manifest, TEXT("quest.replacement_model"), Context(Tick++, City, Utilities, Factions, Pressure))
            || !Apply(Campaign, Manifest, TEXT("quest.replacement_model"), Context(Tick++, City, Utilities, Factions, Pressure, EDAFirstHourPlayerAction::DiscoverReplacementModel))) return false;
        return true;
    }

    bool DriveFinalFour(FDACampaignSnapshot& Campaign, const FDAFirstHourQuestManifest& Manifest,
        FDACitySimulationState& City, FDAUtilityNetwork& Utilities, TArray<FDAFactionState>& Factions,
        UDASystemicPressureSystem& Pressure)
    {
        int64 Tick = 200;
        if (FDAFirstHourCampaignRuntime::TryStartQuest(Manifest, TEXT("quest.agency_has_a_price"),
                Context(Tick++, City, Utilities, Factions, Pressure), Campaign) != EDAFirstHourCampaignResult::Applied
            || !Apply(Campaign, Manifest, TEXT("quest.agency_has_a_price"), Context(Tick++, City, Utilities, Factions, Pressure))) return false;
        FDAFirstHourProgressionContext AgencyChoice = Context(Tick++, City, Utilities, Factions, Pressure);
        AgencyChoice.ChoiceBranchTag = TEXT("retrain");
        if (!Apply(Campaign, Manifest, TEXT("quest.agency_has_a_price"), AgencyChoice)
            || !Apply(Campaign, Manifest, TEXT("quest.agency_has_a_price"),
                Context(Tick++, City, Utilities, Factions, Pressure, EDAFirstHourPlayerAction::RetrainWorkers))) return false;

        AddAsset(Campaign, TEXT("synara.autonomous_exchange"), 6);
        AddAsset(Campaign, TEXT("universal.corner_exchange"), 7);
        if (FDAFirstHourCampaignRuntime::TryStartQuest(Manifest, TEXT("quest.signal_in_foundation"),
                Context(Tick++, City, Utilities, Factions, Pressure), Campaign) != EDAFirstHourCampaignResult::Applied
            || !Apply(Campaign, Manifest, TEXT("quest.signal_in_foundation"), Context(Tick++, City, Utilities, Factions, Pressure))
            || !Apply(Campaign, Manifest, TEXT("quest.signal_in_foundation"), Context(Tick++, City, Utilities, Factions, Pressure, EDAFirstHourPlayerAction::EnterUtilityTunnel))
            || !Apply(Campaign, Manifest, TEXT("quest.signal_in_foundation"), Context(Tick++, City, Utilities, Factions, Pressure, EDAFirstHourPlayerAction::RestoreAncientNode))
            || !Apply(Campaign, Manifest, TEXT("quest.signal_in_foundation"), Context(Tick++, City, Utilities, Factions, Pressure, EDAFirstHourPlayerAction::DefeatMaintenanceDrones))
            || !Apply(Campaign, Manifest, TEXT("quest.signal_in_foundation"), Context(Tick++, City, Utilities, Factions, Pressure, EDAFirstHourPlayerAction::InspectUnknownSymbol))) return false;

        if (FDAFirstHourCampaignRuntime::TryStartQuest(Manifest, TEXT("quest.iron_at_border"),
                Context(Tick++, City, Utilities, Factions, Pressure), Campaign) != EDAFirstHourCampaignResult::Applied
            || !Apply(Campaign, Manifest, TEXT("quest.iron_at_border"), Context(Tick++, City, Utilities, Factions, Pressure))) return false;
        FDAFirstHourProgressionContext IronChoice = Context(Tick++, City, Utilities, Factions, Pressure);
        IronChoice.ChoiceBranchTag = TEXT("accept");
        if (!Apply(Campaign, Manifest, TEXT("quest.iron_at_border"), IronChoice)) return false;

        if (FDAFirstHourCampaignRuntime::TryStartQuest(Manifest, TEXT("quest.basin_speaks"),
                Context(Tick++, City, Utilities, Factions, Pressure), Campaign) != EDAFirstHourCampaignResult::Applied
            || !Apply(Campaign, Manifest, TEXT("quest.basin_speaks"), Context(Tick++, City, Utilities, Factions, Pressure))
            || !Apply(Campaign, Manifest, TEXT("quest.basin_speaks"), Context(Tick++, City, Utilities, Factions, Pressure, EDAFirstHourPlayerAction::ReachEdenBasin))
            || !Apply(Campaign, Manifest, TEXT("quest.basin_speaks"), Context(Tick++, City, Utilities, Factions, Pressure, EDAFirstHourPlayerAction::InspectWaterQuality))
            || !Apply(Campaign, Manifest, TEXT("quest.basin_speaks"), Context(Tick++, City, Utilities, Factions, Pressure, EDAFirstHourPlayerAction::SpeakToAmara))
            || !Apply(Campaign, Manifest, TEXT("quest.basin_speaks"), Context(Tick++, City, Utilities, Factions, Pressure, EDAFirstHourPlayerAction::SpeakToOri))) return false;
        return true;
    }

    bool CompleteAgencyChoice(FDACampaignSnapshot& Campaign, const FDAFirstHourQuestManifest& Manifest,
        FDACitySimulationState& City, FDAUtilityNetwork& Utilities, TArray<FDAFactionState>& Factions,
        UDASystemicPressureSystem& Pressure, const FName Branch, int64 Tick)
    {
        FDAFirstHourProgressionContext Choice = Context(Tick++, City, Utilities, Factions, Pressure);
        Choice.ChoiceBranchTag = Branch;
        if (!Apply(Campaign, Manifest, TEXT("quest.agency_has_a_price"), Choice)) return false;
        if (Branch == TEXT("agency_forum"))
        {
            AddAsset(Campaign, TEXT("synara.agency_forum"), 9);
            return Apply(Campaign, Manifest, TEXT("quest.agency_has_a_price"), Context(Tick, City, Utilities, Factions, Pressure));
        }
        if (Branch == TEXT("retrain"))
            return Apply(Campaign, Manifest, TEXT("quest.agency_has_a_price"), Context(Tick, City, Utilities, Factions, Pressure, EDAFirstHourPlayerAction::RetrainWorkers));
        if (Branch == TEXT("automation_cap"))
            return Apply(Campaign, Manifest, TEXT("quest.agency_has_a_price"), Context(Tick, City, Utilities, Factions, Pressure, EDAFirstHourPlayerAction::EnactAutomationCap));
        return Branch == TEXT("reject");
    }

    bool DriveToIronChoice(FDACampaignSnapshot& Campaign, const FDAFirstHourQuestManifest& Manifest,
        FDACitySimulationState& City, FDAUtilityNetwork& Utilities, TArray<FDAFactionState>& Factions,
        UDASystemicPressureSystem& Pressure)
    {
        int64 Tick = 300;
        AddAsset(Campaign, TEXT("synara.autonomous_exchange"), 6);
        AddAsset(Campaign, TEXT("universal.corner_exchange"), 7);
        if (FDAFirstHourCampaignRuntime::TryStartQuest(Manifest, TEXT("quest.signal_in_foundation"), Context(Tick++, City, Utilities, Factions, Pressure), Campaign) != EDAFirstHourCampaignResult::Applied
            || !Apply(Campaign, Manifest, TEXT("quest.signal_in_foundation"), Context(Tick++, City, Utilities, Factions, Pressure))
            || !Apply(Campaign, Manifest, TEXT("quest.signal_in_foundation"), Context(Tick++, City, Utilities, Factions, Pressure, EDAFirstHourPlayerAction::EnterUtilityTunnel))
            || !Apply(Campaign, Manifest, TEXT("quest.signal_in_foundation"), Context(Tick++, City, Utilities, Factions, Pressure, EDAFirstHourPlayerAction::RestoreAncientNode))
            || !Apply(Campaign, Manifest, TEXT("quest.signal_in_foundation"), Context(Tick++, City, Utilities, Factions, Pressure, EDAFirstHourPlayerAction::DefeatMaintenanceDrones))
            || !Apply(Campaign, Manifest, TEXT("quest.signal_in_foundation"), Context(Tick++, City, Utilities, Factions, Pressure, EDAFirstHourPlayerAction::InspectUnknownSymbol))) return false;
        return FDAFirstHourCampaignRuntime::TryStartQuest(Manifest, TEXT("quest.iron_at_border"), Context(Tick++, City, Utilities, Factions, Pressure), Campaign) == EDAFirstHourCampaignResult::Applied
            && Apply(Campaign, Manifest, TEXT("quest.iron_at_border"), Context(Tick, City, Utilities, Factions, Pressure));
    }
}

void FDAFirstHourQuestSpec::Define()
{
    It("uses the world subsystem as production owner and retains runtime content across GC", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        UDAFirstHourCampaignCoordinatorSubsystem* Coordinator =
            Fixture.GetSubsystem<UDAFirstHourCampaignCoordinatorSubsystem>();
        UDAFirstHourContentRegistrySubsystem* Registry =
            Fixture.GetSubsystem<UDAFirstHourContentRegistrySubsystem>();
        TestNotNull("World authority exists", World);
        TestNotNull("Production first-hour coordinator exists", Coordinator);
        TestNotNull("Canonical content registry exists", Registry);
        if (World == nullptr || Coordinator == nullptr || Registry == nullptr || !Registry->IsReady()) return;
        TestTrue("Canonical world initializes", World->InitializeVerticalSliceState(TEXT("region.synara_frontier")));
        const FDAWorldAssetRecord* FounderHall = World->GetPersistentCampaign().WorldAssets.FindByPredicate(
            [](const FDAWorldAssetRecord& Asset)
            {
                return Asset.CardDefinitionId == TEXT("special.founder_hall")
                    && Asset.OwnerCivilizationId == TEXT("civilization.synara");
            });
        TestNotNull("Authored bootstrap already owns the Founder Hall", FounderHall);
        const FDAQuestSaveState* Wake = World->GetPersistentCampaign().NarrativeState.FindQuestState(
            TEXT("quest.wake_the_hall"));
        TestTrue("Clock/authority handler automatically starts the now-satisfied quest", Wake != nullptr);
        TestTrue("Coordinator exposes the same campaign object",
            Coordinator->GetCampaignSnapshot() == &World->GetPersistentCampaign());
        TestTrue("Coordinator uses the save-owned live signal object",
            &World->GetPersistentCampaign().LiveSignals == &World->GetLiveSignals());

        TWeakObjectPtr<UDA_FirstHourQuestDefinition> RetainedQuest = Registry->GetContent().Quests[0];
        TWeakObjectPtr<UDA_CitizenDefinition> RetainedCitizen = Registry->GetContent().Citizen;
        CollectGarbage(RF_NoFlags);
        TestTrue("Strong quest registry survives GC", RetainedQuest.IsValid());
        TestTrue("Strong citizen registry survives GC", RetainedCitizen.IsValid());
    });

    It("commits live utility economy and citizen signals through the production owner and reloads them", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        UDAFirstHourCampaignCoordinatorSubsystem* Coordinator =
            Fixture.GetSubsystem<UDAFirstHourCampaignCoordinatorSubsystem>();
        UDAEconomySubsystem* Economy = Fixture.GetSubsystem<UDAEconomySubsystem>();
        if (World == nullptr || Coordinator == nullptr || Economy == nullptr) return;
        TestTrue("Deterministic campaign bootstrap is present", World->GetPersistentCampaign().WorldState.bInitialized);
        const FDAWorldAssetRecord* Hall = World->GetPersistentCampaign().WorldAssets.FindByPredicate(
            [](const FDAWorldAssetRecord& Asset)
            {
                return Asset.CardDefinitionId == TEXT("special.founder_hall")
                    && Asset.OwnerCivilizationId == TEXT("civilization.synara");
            });
        TestNotNull("Canonical Founder Hall is present", Hall);
        if (Hall == nullptr) return;

        FDACampaignUtilitySignal Power;
        Power.WorldAssetId = Hall->WorldAssetId; Power.Utility = EDACampaignUtilityKind::Power;
        Power.Supply = EDACampaignUtilitySupply::FullySupplied;
        TestTrue("Utility mutation commits through world authority", World->SubmitUtilitySignal(Power));
        TestEqual("Reflected production action reaches Hall", Coordinator->SubmitPlayerAction(
            TEXT("quest.wake_the_hall"), EDAFirstHourPlayerAction::ReachFounderHall,
            NAME_None, FGuid(), FGuid()), EDAFirstHourCampaignResult::Applied);
        TestEqual("Persisted utility signal satisfies power objective", Coordinator->SubmitPlayerAction(
            TEXT("quest.wake_the_hall"), EDAFirstHourPlayerAction::RestoreFounderHallPower,
            NAME_None, FGuid(), FGuid()), EDAFirstHourCampaignResult::Applied);

        FDACampaignCitizenSignal Nia;
        Nia.CitizenId = TEXT("citizen.synara.nia_vale"); Nia.CityId = TEXT("player_capital");
        TestTrue("Citizen mutation commits through world authority", World->SubmitCitizenSignal(Nia));
        FDACampaignSnapshot EconomyCandidate = World->GetPersistentCampaign();
        Economy->ResolveDevelopmentCycle(EconomyCandidate.CitySimulationState);
        TestEqual("Economy calculator mutates only the full city candidate",
            EconomyCandidate.CitySimulationState.ResolvedDevelopmentCycles, int64(1));

        const FString SaveDirectory = FPaths::Combine(FPaths::ProjectSavedDir(),
            TEXT("Automation/Task20LiveSignals-") + FGuid::NewGuid().ToString(EGuidFormats::Digits));
        FDASaveService SaveService(SaveDirectory);
        TestTrue("Schema-v11 campaign saves", SaveService.SaveCampaign(
            World->GetPersistentCampaign(), TEXT("live-signals")).IsSuccess());
        const TResult<FDACampaignSnapshot, FDASaveError> Reloaded = SaveService.LoadCampaign(TEXT("live-signals"));
        TestTrue("Schema-v11 campaign reloads", Reloaded.HasValue());
        if (Reloaded.HasValue())
        {
            TestEqual("Utility signal survives reload", Reloaded.GetValue().LiveSignals.UtilitySignals.Num(), 1);
            TestEqual("Complete named citizen roster survives reload",
                Reloaded.GetValue().CitySimulationState.Citizens.Num(), 20);
            TestEqual("Unpublished calculator candidate cannot change the owner",
                Reloaded.GetValue().LiveSignals.ResolvedDevelopmentCycles, int64(0));
        }
        IFileManager::Get().DeleteDirectory(*SaveDirectory, false, true);
    });

    It("resolves every natural and strategic Development Cycle through the staged campaign authority", [this]()
    {
        FDAGameInstanceSubsystemFixture NaturalFixture;
        UDAWorldStateSubsystem* NaturalWorld = NaturalFixture.GetSubsystem<UDAWorldStateSubsystem>();
        UDASimulationClockSubsystem* NaturalClock = NaturalFixture.GetSubsystem<UDASimulationClockSubsystem>();
        FDAGameInstanceSubsystemFixture StrategicFixture;
        UDAWorldStateSubsystem* StrategicWorld = StrategicFixture.GetSubsystem<UDAWorldStateSubsystem>();
        TestNotNull("Natural world exists", NaturalWorld);
        TestNotNull("Natural clock exists", NaturalClock);
        TestNotNull("Strategic world exists", StrategicWorld);
        if (NaturalWorld == nullptr || NaturalClock == nullptr || StrategicWorld == nullptr) return;

        NaturalClock->AdvanceSimulation(UDASimulationClockSubsystem::DevelopmentCycleDurationSeconds * 5.0);
        TestEqual("The fifth natural cycle survives staged World Tick commit",
            NaturalWorld->GetLiveSignals().ResolvedDevelopmentCycles, 5LL);
        TestEqual("Every natural cycle has one live-signal revision",
            NaturalWorld->GetLiveSignals().MutationRevision, 5LL);

        NaturalClock->AdvanceSimulation(UDASimulationClockSubsystem::DevelopmentCycleDurationSeconds * 5.0);
        TestTrue("Two strategic World Ticks advance through the production owner", StrategicWorld->AdvanceWorldTicks(2));
        TestEqual("Natural time resolves ten campaign economy cycles",
            NaturalWorld->GetLiveSignals().ResolvedDevelopmentCycles, 10LL);
        TestEqual("Strategic time resolves the same ten campaign economy cycles",
            StrategicWorld->GetLiveSignals().ResolvedDevelopmentCycles, 10LL);
        TestEqual("Natural and strategic live-signal revisions are identical",
            StrategicWorld->GetLiveSignals().MutationRevision,
            NaturalWorld->GetLiveSignals().MutationRevision);

        NaturalClock->AdvanceSimulation(15.25);
        const FDACampaignSnapshot SavedCampaign = NaturalWorld->GetPersistentCampaign();
        FDAGameInstanceSubsystemFixture RestoredFixture;
        UDAWorldStateSubsystem* RestoredWorld = RestoredFixture.GetSubsystem<UDAWorldStateSubsystem>();
        UDASimulationClockSubsystem* RestoredClock = RestoredFixture.GetSubsystem<UDASimulationClockSubsystem>();
        TestNotNull("Restored world exists", RestoredWorld);
        TestNotNull("Restored clock exists", RestoredClock);
        if (RestoredWorld != nullptr && RestoredClock != nullptr)
        {
            TestTrue("Campaign restoration accepts exact clock authority",
                RestoredWorld->RestorePersistentCampaign(SavedCampaign));
            TestEqual("Campaign restoration retains the exact Development Cycle",
                RestoredClock->GetCurrentDevelopmentCycle(), 10LL);
            TestEqual("Campaign restoration retains pending natural time",
                RestoredClock->GetAccumulatedSimulationSeconds(), 15.25);
        }
    });

    It("requires every campaign CAS to check a single monotonic live-signal revision", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        TestNotNull("World exists", World);
        if (World == nullptr) return;
        const int64 Tick = World->GetCurrentWorldTick();
        const int64 NarrativeRevision = World->GetPersistentCampaign().NarrativeState.MutationRevision;
        const int64 SignalRevision = World->GetLiveSignals().MutationRevision;

        FDACampaignSnapshot UnversionedMutation = World->GetPersistentCampaign();
        UnversionedMutation.LiveSignals.Population = 1;
        TestFalse("Campaign CAS cannot mutate live signals without advancing their revision",
            World->TryCommitPersistentCampaign(UnversionedMutation, NarrativeRevision, SignalRevision, Tick));

        FDACampaignSnapshot JumpedMutation = UnversionedMutation;
        JumpedMutation.LiveSignals.MutationRevision = SignalRevision + 2;
        TestFalse("Campaign CAS cannot jump the live-signal revision",
            World->TryCommitPersistentCampaign(JumpedMutation, NarrativeRevision, SignalRevision, Tick));

        FDACampaignSnapshot CheckedMutation = UnversionedMutation;
        CheckedMutation.LiveSignals.MutationRevision = SignalRevision + 1;
        TestTrue("Campaign CAS accepts one checked live-signal mutation",
            World->TryCommitPersistentCampaign(CheckedMutation, NarrativeRevision, SignalRevision, Tick));
        TestFalse("A stale expected live-signal revision cannot replay",
            World->TryCommitPersistentCampaign(CheckedMutation, NarrativeRevision, SignalRevision, Tick));

        const int64 CheckedRevision = World->GetLiveSignals().MutationRevision;
        FDACampaignSnapshot RewoundMutation = World->GetPersistentCampaign();
        RewoundMutation.LiveSignals.Population += 1;
        RewoundMutation.LiveSignals.MutationRevision = CheckedRevision - 1;
        TestFalse("Campaign CAS cannot rewind the live-signal revision",
            World->TryCommitPersistentCampaign(RewoundMutation, NarrativeRevision, CheckedRevision, Tick));
        FDACampaignSnapshot RevisionOnlyBypass = World->GetPersistentCampaign();
        RevisionOnlyBypass.LiveSignals.MutationRevision = CheckedRevision + 1;
        TestFalse("Campaign CAS cannot advance a revision without an actual signal mutation",
            World->TryCommitPersistentCampaign(RevisionOnlyBypass, NarrativeRevision, CheckedRevision, Tick));

        FDACampaignSnapshot OverflowAuthority = World->GetPersistentCampaign();
        OverflowAuthority.LiveSignals.MutationRevision = MAX_int64;
        TestTrue("A validated save authority at the revision boundary restores",
            World->RestorePersistentCampaign(OverflowAuthority));
        FDACampaignCitizenSignal OverflowCandidate;
        OverflowCandidate.CitizenId = TEXT("citizen.test.overflow");
        OverflowCandidate.CityId = TEXT("player_capital");
        TestFalse("Canonical city ingress rejects revision overflow without wrapping",
            World->SubmitCitizenSignal(OverflowCandidate));
    });

    It("preflights Development Cycle capacity and aborts natural and strategic batches atomically", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        UDASimulationClockSubsystem* Clock = Fixture.GetSubsystem<UDASimulationClockSubsystem>();
        TestNotNull("World exists", World); TestNotNull("Clock exists", Clock);
        if (World == nullptr || Clock == nullptr) return;
        FDACampaignSnapshot NearBoundary = World->GetPersistentCampaign();
        NearBoundary.LiveSignals.MutationRevision = MAX_int64 - 2;
        NearBoundary.LiveSignals.ResolvedDevelopmentCycles = MAX_int64 - 2;
        TestTrue("Near-boundary authority restores", World->RestorePersistentCampaign(NearBoundary));
        const FDASimulationClockAuthorityState Before = Clock->CaptureAuthorityState();
        Clock->AdvanceSimulation(90.0);
        TestEqual("Natural overflow preflight resolves no campaign cycle",
            World->GetLiveSignals().ResolvedDevelopmentCycles, MAX_int64 - 2);
        TestEqual("Natural overflow preflight preserves signal revision",
            World->GetLiveSignals().MutationRevision, MAX_int64 - 2);
        TestEqual("Natural overflow preflight preserves clock cycle",
            Clock->GetCurrentDevelopmentCycle(), Before.CurrentDevelopmentCycle);
        TestEqual("Natural overflow preflight preserves pending time",
            Clock->GetAccumulatedSimulationSeconds(), Before.AccumulatedSimulationSeconds);
        TestFalse("Strategic overflow preflight rejects the whole World Tick", World->AdvanceWorldTicks(1));
        TestEqual("Strategic overflow preflight preserves World Tick", Clock->GetCurrentWorldTick(), Before.CurrentWorldTick);

        FDACampaignSnapshot ExactBoundary = World->GetPersistentCampaign();
        ExactBoundary.LiveSignals.MutationRevision = MAX_int64;
        ExactBoundary.LiveSignals.ResolvedDevelopmentCycles = MAX_int64;
        TestTrue("Exact-boundary authority restores", World->RestorePersistentCampaign(ExactBoundary));
        Clock->AdvanceSimulation(30.0);
        TestEqual("Exact boundary never silently advances natural time", Clock->GetCurrentDevelopmentCycle(), Before.CurrentDevelopmentCycle);
        TestFalse("Exact boundary never silently advances strategic time", World->AdvanceWorldTicks(1));
    });

    It("requires general campaign CAS candidates to preserve live clock authority exactly", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        UDASimulationClockSubsystem* Clock = Fixture.GetSubsystem<UDASimulationClockSubsystem>();
        TestNotNull("World exists", World); TestNotNull("Clock exists", Clock);
        if (World == nullptr || Clock == nullptr) return;
        Clock->AdvanceSimulation(7.25);
        FDACampaignSnapshot RewrittenTime = World->GetPersistentCampaign();
        RewrittenTime.WorldState.ClockAuthority.AccumulatedSimulationSeconds = 0.0;
        TestFalse("Caller cannot rewrite clock authority through campaign CAS",
            World->TryCommitPersistentCampaign(RewrittenTime,
                RewrittenTime.NarrativeState.MutationRevision,
                RewrittenTime.LiveSignals.MutationRevision,
                RewrittenTime.WorldState.CurrentWorldTick));
    });

    It("loads the one frozen semantic authority and rejects manifest/cache mutation", [this]()
    {
        FDAFirstHourQuestManifest Manifest; TArray<FText> Errors;
        TestTrue("Canonical load", FDAFirstHourQuestPipeline::LoadCanonical(Manifest, Errors));
        TestEqual("Nine quests", Manifest.Quests.Num(), 9); TestEqual("Canonical Nia", Manifest.Citizen.CitizenId, FName(TEXT("citizen.synara.nia_vale")));
        TestEqual("Declared canonical SHA", Manifest.Fingerprint, FString(TEXT("2c82c47966c118441564e717007e451326bfdea4")));
        for (const FDAFirstHourQuestEntry& Entry : Manifest.Quests)
        {
            FString PolicyError;
            TestTrue("Every first-hour definition matches the Core frozen policy",
                FDAFirstHourFrozenPolicy::ValidatePinnedQuestDefinition(
                    Entry.Definition.BuildManifest(), PolicyError));
        }
        FDAQuestDefinitionManifest SelfConsistentMutation = Manifest.Quests[0].Definition.BuildManifest();
        SelfConsistentMutation.Nodes[0].SourceDefinitionId = TEXT("quest.changed.but_rehashed");
        SelfConsistentMutation.RefreshFingerprint(); FString PolicyError;
        TestFalse("A changed graph remains rejected even after recomputing its own fingerprint",
            FDAFirstHourFrozenPolicy::ValidatePinnedQuestDefinition(SelfConsistentMutation, PolicyError));
        TArray<UDA_FirstHourQuestDefinition*> Assets; UDA_CitizenDefinition* Citizen = nullptr;
        TestTrue("Build generated projection", FDAFirstHourQuestPipeline::BuildAssets(Manifest, Assets, Citizen, Errors));
        TestFalse("Transient fallback is never accepted as generated cache",
            FDAFirstHourQuestPipeline::ValidateGeneratedCache(Manifest, Assets, Citizen, Errors));
        Assets.Reset();
        for (const FDAFirstHourQuestEntry& Entry : Manifest.Quests)
        {
            UPackage* Package = CreatePackage(*Entry.AssetPath);
            const FName AssetName(*FPackageName::GetLongPackageAssetName(Entry.AssetPath));
            UDA_FirstHourQuestDefinition* Asset = NewObject<UDA_FirstHourQuestDefinition>(Package, AssetName,
                RF_Public | RF_Standalone);
            Asset->Quest = Entry; Asset->SourceFingerprint = Manifest.Fingerprint; Assets.Add(Asset);
        }
        UPackage* CitizenPackage = CreatePackage(*Manifest.Citizen.AssetPath);
        Citizen = NewObject<UDA_CitizenDefinition>(CitizenPackage,
            FName(*FPackageName::GetLongPackageAssetName(Manifest.Citizen.AssetPath)), RF_Public | RF_Standalone);
        Citizen->Citizen = Manifest.Citizen; Citizen->SourceFingerprint = Manifest.Fingerprint;
        TArray<FText> ExactErrors;
        TestTrue("Exact packaged cache parity", FDAFirstHourQuestPipeline::ValidateGeneratedCache(
            Manifest, Assets, Citizen, ExactErrors));
        UPackage* WrongPackage = CreatePackage(TEXT("/Game/DA/Quests/Q_WrongCachePath"));
        UDA_FirstHourQuestDefinition* WrongPath = NewObject<UDA_FirstHourQuestDefinition>(
            WrongPackage, TEXT("Q_WrongCachePath"), RF_Public | RF_Standalone);
        WrongPath->Quest = Manifest.Quests[0]; WrongPath->SourceFingerprint = Manifest.Fingerprint;
        UDA_FirstHourQuestDefinition* CorrectFirst = Assets[0]; Assets[0] = WrongPath;
        TArray<FText> PathErrors;
        TestFalse("Wrong generated UObject package path is rejected",
            FDAFirstHourQuestPipeline::ValidateGeneratedCache(Manifest, Assets, Citizen, PathErrors));
        Assets[0] = CorrectFirst;
        Assets[0]->SourceFingerprint = TEXT("0000000000000000000000000000000000000000");
        TestFalse("Stale cache rejected", FDAFirstHourQuestPipeline::ValidateGeneratedCache(Manifest, Assets, Citizen, Errors));
        Assets[0]->SourceFingerprint = Manifest.Fingerprint;
        Assets[0]->Quest.WorldAssetBindings[0].OwnerId = TEXT("civilization.forgeweave");
        TArray<FText> BindingErrors;
        TestFalse("Binding semantic cache mutation rejected", FDAFirstHourQuestPipeline::ValidateGeneratedCache(Manifest, Assets, Citizen, BindingErrors));
        Assets[0]->Quest = Manifest.Quests[0]; Assets[0]->Quest.Nodes[0].RuntimeNode.Edges[0].TargetNodeId = TEXT("resolution");
        TArray<FText> GraphErrors;
        TestFalse("Typed graph cache mutation rejected", FDAFirstHourQuestPipeline::ValidateGeneratedCache(Manifest, Assets, Citizen, GraphErrors));
        Assets[0]->Quest = Manifest.Quests[0]; Citizen->Citizen.TowerJobId = TEXT("job.fabricated");
        TArray<FText> CitizenErrors;
        TestFalse("Nia semantic cache mutation rejected", FDAFirstHourQuestPipeline::ValidateGeneratedCache(Manifest, Assets, Citizen, CitizenErrors));
        FString Json; TestTrue("Read manifest", FFileHelper::LoadFileToString(Json, *FDAFirstHourQuestPipeline::GetCanonicalManifestPath()));
        Json.ReplaceInline(TEXT("Wake the Hall"), TEXT("Wake the Hall!")); FDAFirstHourQuestManifest Mutated; TArray<FText> MutationErrors;
        TestFalse("Override cannot alter frozen canon", FDAFirstHourQuestPipeline::ParseJson(Json, Mutated, MutationErrors));
    });

    It("refuses every early quest and evaluates concrete Founder Hall signals", [this]()
    {
        FDAFirstHourQuestManifest Manifest; TArray<FText> Errors; TestTrue("Manifest", FDAFirstHourQuestPipeline::LoadCanonical(Manifest, Errors));
        FDACampaignSnapshot Campaign; FDACitySimulationState City; FDAUtilityNetwork Utilities; TArray<FDAFactionState> Factions = UDAFactionSystem::CreateSynaraFactions();
        UDASystemicPressureSystem* Pressure = NewObject<UDASystemicPressureSystem>();
        for (int32 Index = 1; Index < Manifest.Quests.Num(); ++Index)
            TestEqual("Early prerequisite refusal", FDAFirstHourCampaignRuntime::TryStartQuest(Manifest, Manifest.Quests[Index].Definition.QuestId, Context(Index, City, Utilities, Factions, *Pressure), Campaign), EDAFirstHourCampaignResult::PrerequisiteMissing);
        TestEqual("No Founder Hall", FDAFirstHourCampaignRuntime::TryStartQuest(Manifest, TEXT("quest.wake_the_hall"), Context(1, City, Utilities, Factions, *Pressure), Campaign), EDAFirstHourCampaignResult::ConditionUnsatisfied);
        AddAsset(Campaign, TEXT("special.founder_hall"), 1, EDAConstructionState::Foundation);
        TestEqual("Real Founder Hall satisfies start", FDAFirstHourCampaignRuntime::TryStartQuest(Manifest, TEXT("quest.wake_the_hall"), Context(1, City, Utilities, Factions, *Pressure), Campaign), EDAFirstHourCampaignResult::Applied);
        TestEqual("Wrong real action cannot advance", FDAFirstHourCampaignRuntime::AdvanceQuest(Manifest, TEXT("quest.wake_the_hall"), Context(2, City, Utilities, Factions, *Pressure, EDAFirstHourPlayerAction::SpeakToNia), Campaign), EDAFirstHourCampaignResult::Applied);
        TestEqual("Reach objective refuses wrong signal", FDAFirstHourCampaignRuntime::AdvanceQuest(Manifest, TEXT("quest.wake_the_hall"), Context(3, City, Utilities, Factions, *Pressure, EDAFirstHourPlayerAction::SpeakToNia), Campaign), EDAFirstHourCampaignResult::ConditionUnsatisfied);
    });

    It("applies each Replacement Model semantic transaction exactly once across save reload", [this]()
    {
        FDAFirstHourQuestManifest Manifest; TArray<FText> Errors; TestTrue("Manifest", FDAFirstHourQuestPipeline::LoadCanonical(Manifest, Errors));
        for (const FName Branch : {FName(TEXT("accept")), FName(TEXT("modify")), FName(TEXT("audit")), FName(TEXT("reject"))})
        {
            FDACampaignSnapshot Campaign; FDACitySimulationState City; FDAUtilityNetwork Utilities; TArray<FDAFactionState> Factions = UDAFactionSystem::CreateSynaraFactions();
            for (FDAFactionState& Faction : Factions) if (Faction.FactionId == TEXT("faction.synara.human_agency")) Faction.Support = 50.f;
            UDASystemicPressureSystem* Pressure = NewObject<UDASystemicPressureSystem>();
            TestTrue("Production progression reaches explicit choice", DriveToReplacementChoice(Campaign, Manifest, City, Utilities, Factions, *Pressure));
            FDAFirstHourProgressionContext Choice = Context(100, City, Utilities, Factions, *Pressure); Choice.ChoiceBranchTag = Branch;
            if (Branch == TEXT("audit"))
            {
                Choice.VisionActionId = FGuid(0xDA20, 0xA0, 0x01, 0x01);
                FDAPromiseLedger ProofLedger(Campaign); TestEqual("Vision provenance commits", ProofLedger.CommitAction(Choice.VisionActionId,
                    {FName(TEXT("capability.vision.available"))}, 99, ProofLedger.GetRevision(), false), EDAPromiseMutationResult::Applied);
                TestEqual("Typed Vision authority commits", FDAFirstHourCampaignRuntime::RecordAuditEligibilitySource(
                    TEXT("Vision"), Choice.VisionActionId, TEXT("capability.vision.available"), 99, Campaign),
                    EDAFirstHourCampaignResult::Applied);
            }
            TestEqual("Branch applies", FDAFirstHourCampaignRuntime::AdvanceQuest(Manifest, TEXT("quest.replacement_model"), Choice, Campaign), EDAFirstHourCampaignResult::Applied);
            TestEqual("One full outcome audit", Campaign.NarrativeState.QuestContentEffectRecords.Num(), 1);
            const int32 UnlockCount = Campaign.NarrativeState.QuestContentUnlockRecords.Num();
            TestEqual("Replay exact once", FDAFirstHourCampaignRuntime::AdvanceQuest(Manifest, TEXT("quest.replacement_model"), Choice, Campaign), EDAFirstHourCampaignResult::AlreadyApplied);
            if (Branch == TEXT("audit"))
            {
                FDAFirstHourProgressionContext WrongProofReplay = Choice;
                WrongProofReplay.VisionActionId = FGuid(0xDA20, 0xA0, 0x01, 0x02);
                TestEqual("Completed audit requires the identical persisted proof source",
                    FDAFirstHourCampaignRuntime::AdvanceQuest(Manifest, TEXT("quest.replacement_model"),
                        WrongProofReplay, Campaign), EDAFirstHourCampaignResult::ConflictingReplay);
                FDACampaignSnapshot ProofTickTamper = Campaign;
                ++ProofTickTamper.NarrativeState.QuestEligibilityProofRecords[0].SourceWorldTick;
                FString ProofError;
                TestFalse("Audit source tick tamper is rejected", ProofTickTamper.Validate(ProofError));
            }
            TestEqual("No duplicate rewards", Campaign.NarrativeState.QuestContentUnlockRecords.Num(), UnlockCount);
            FDAFirstHourProgressionContext WrongTickReplay = Choice; ++WrongTickReplay.WorldTick;
            TestEqual("Replay at a different supplied tick conflicts", FDAFirstHourCampaignRuntime::AdvanceQuest(
                Manifest, TEXT("quest.replacement_model"), WrongTickReplay, Campaign), EDAFirstHourCampaignResult::ConflictingReplay);
            if (Branch == TEXT("accept"))
            {
                TestTrue("Later dependency authority change is ledgered", Campaign.SynaraState.ApplyDependencyReason(
                    TEXT("action.test.later_dependency_change"), 1.0, 101));
                TestEqual("Historical Replacement replay survives later aggregate change", FDAFirstHourCampaignRuntime::AdvanceQuest(
                    Manifest, TEXT("quest.replacement_model"), Choice, Campaign), EDAFirstHourCampaignResult::AlreadyApplied);
            }
            FDAFirstHourProgressionContext Conflict = Choice;
            Conflict.ChoiceBranchTag = Branch == TEXT("accept") ? FName(TEXT("modify")) : FName(TEXT("accept"));
            TestEqual("Different choice replay conflicts", FDAFirstHourCampaignRuntime::AdvanceQuest(
                Manifest, TEXT("quest.replacement_model"), Conflict, Campaign), EDAFirstHourCampaignResult::ConflictingReplay);
            const FString Directory = FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("Task20"), FGuid::NewGuid().ToString(EGuidFormats::Digits)); FDASaveService Saves(Directory);
            TestTrue("Checksummed save", Saves.SaveCampaign(Campaign, TEXT("branch")).IsSuccess()); const auto Loaded = Saves.LoadCampaign(TEXT("branch")); TestTrue("Reload", Loaded.HasValue());
            if (Loaded.HasValue()) { TestEqual("Effect survives reload", Loaded.GetValue().NarrativeState.QuestContentEffectRecords.Num(), 1); TestEqual("Rewards survive reload", Loaded.GetValue().NarrativeState.QuestContentUnlockRecords.Num(), UnlockCount); }
            if (Branch == TEXT("accept"))
            {
                TestTrue("Semantic tamper receives a valid checksum", TamperEffectWithValidChecksum(Directory, TEXT("branch")));
                TestFalse("Checksummed semantic tamper is rejected", Saves.LoadCampaign(TEXT("branch")).HasValue());
            }
        }
    });

    It("requires exact Nia story plus a real completed Task18 Human Override action", [this]()
    {
        FDAFirstHourQuestManifest Manifest; TArray<FText> Errors; TestTrue("Manifest", FDAFirstHourQuestPipeline::LoadCanonical(Manifest, Errors));
        FDACampaignSnapshot Campaign; const FGuid ActionId(0xDA20, 0x01, 0x02, 0x03);
        TestEqual("Names alone cannot fabricate crisis completion", FDAFirstHourCampaignRuntime::RecordCrisisCompletion(
            Manifest, TEXT("quest.human_override"), TEXT("action.quest.human_override.completed"), ActionId, 3, Campaign), EDAFirstHourCampaignResult::ConditionUnsatisfied);

        FDAQuestDefinition Crisis; Crisis.QuestId = TEXT("quest.human_override"); Crisis.SourceDefinitionId = TEXT("quest.human_override.v1"); Crisis.StartNodeId = TEXT("start");
        FDAQuestNodeDefinition Start; Start.NodeId = TEXT("start"); Start.Type = EDAQuestNodeType::Start; Start.SourceDefinitionId = TEXT("quest.human_override.start");
        FDAQuestEdgeDefinition Edge; Edge.BranchTag = TEXT("complete"); Edge.TargetNodeId = TEXT("resolution"); Start.Edges.Add(Edge);
        FDAQuestNodeDefinition Resolution; Resolution.NodeId = TEXT("resolution"); Resolution.Type = EDAQuestNodeType::Resolution; Resolution.SourceDefinitionId = TEXT("quest.human_override.resolution"); Crisis.Nodes = {Start, Resolution};
        TestEqual("Canonical Human Override definition fingerprint is pinned", Crisis.BuildManifest().DefinitionFingerprint,
            Manifest.Citizen.ChampionEligibility.RequiredCrisisDefinitionFingerprint);
        TestEqual("Real Task18 crisis starts", FDAQuestRuntime::StartQuest(Crisis, {}, 1, Campaign), EDAQuestRuntimeResult::Applied);
        FDAQuestEvaluationContext Evaluation; Evaluation.WorldTick = 2; FName Branch;
        TestEqual("Real Task18 crisis completes", FDAQuestRuntime::EvaluateCurrentNode(Crisis, Evaluation, Campaign, Branch), EDAQuestRuntimeResult::Applied);
        FDAPromiseLedger Ledger(Campaign); TestEqual("Real semantic completion action commits", Ledger.CommitAction(ActionId,
            {FName(TEXT("action.quest.human_override.completed"))}, 3, Ledger.GetRevision(), false), EDAPromiseMutationResult::Applied);
        TestEqual("Canonical Human Override action advances Nia story", FDAFirstHourCampaignRuntime::CommitNiaStoryTransition(Manifest,
            ActionId, TEXT("story.nia.human_override.supported"), 3, Campaign), EDAFirstHourCampaignResult::Applied);
        TestEqual("Identical story transition replay precedes monotonic rejection",
            FDAFirstHourCampaignRuntime::CommitNiaStoryTransition(Manifest, ActionId,
                TEXT("story.nia.human_override.supported"), 3, Campaign), EDAFirstHourCampaignResult::AlreadyApplied);
        TestEqual("Crisis record must be strictly later than its causal action", FDAFirstHourCampaignRuntime::RecordCrisisCompletion(Manifest,
            TEXT("quest.human_override"), TEXT("action.quest.human_override.completed"), ActionId, 3, Campaign), EDAFirstHourCampaignResult::ConditionUnsatisfied);
        TestEqual("Proven crisis record commits", FDAFirstHourCampaignRuntime::RecordCrisisCompletion(Manifest,
            TEXT("quest.human_override"), TEXT("action.quest.human_override.completed"), ActionId, 4, Campaign), EDAFirstHourCampaignResult::Applied);
        TestTrue("Both exact gates are eligible", FDAFirstHourCampaignRuntime::IsNiaChampionEligible(Manifest, Campaign));
        Campaign.NarrativeState.CitizenStoryStates[TEXT("citizen.synara.nia_vale")] = TEXT("story.nia.replacement_model.jobs_preserved");
        TestFalse("Wrong Nia story state is ineligible", FDAFirstHourCampaignRuntime::IsNiaChampionEligible(Manifest, Campaign));
    });

    It("commits every Agency and Iron choice to canonical authorities across reload", [this]()
    {
        FDAFirstHourQuestManifest Manifest; TArray<FText> Errors;
        TestTrue("Manifest", FDAFirstHourQuestPipeline::LoadCanonical(Manifest, Errors));
        FDACampaignSnapshot AgencyBase; FDACitySimulationState City; FDAUtilityNetwork Utilities;
        TArray<FDAFactionState> Factions = UDAFactionSystem::CreateSynaraFactions();
        UDASystemicPressureSystem* Pressure = NewObject<UDASystemicPressureSystem>();
        TestTrue("Reach Replacement choice", DriveToReplacementChoice(AgencyBase, Manifest, City, Utilities, Factions, *Pressure));
        FDAFirstHourProgressionContext Replacement = Context(150, City, Utilities, Factions, *Pressure);
        Replacement.ChoiceBranchTag = TEXT("accept");
        TestTrue("Complete Replacement", Apply(AgencyBase, Manifest, TEXT("quest.replacement_model"), Replacement));
        TestEqual("Agency starts", FDAFirstHourCampaignRuntime::TryStartQuest(Manifest, TEXT("quest.agency_has_a_price"),
            Context(200, City, Utilities, Factions, *Pressure), AgencyBase), EDAFirstHourCampaignResult::Applied);
        TestTrue("Agency reaches choice", Apply(AgencyBase, Manifest, TEXT("quest.agency_has_a_price"), Context(201, City, Utilities, Factions, *Pressure)));
        const FString Directory = FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("Task20Choices"), FGuid::NewGuid().ToString(EGuidFormats::Digits));
        FDASaveService Saves(Directory);
        for (const FName Branch : {FName(TEXT("agency_forum")), FName(TEXT("retrain")), FName(TEXT("automation_cap")), FName(TEXT("reject"))})
        {
            FDACampaignSnapshot BranchCampaign = AgencyBase;
            TestTrue("Agency branch completes", CompleteAgencyChoice(BranchCampaign, Manifest, City, Utilities, Factions, *Pressure, Branch, 210));
            const FDAQuestContentEffectRecord* Effect = BranchCampaign.NarrativeState.QuestContentEffectRecords.FindByPredicate(
                [](const FDAQuestContentEffectRecord& Record) { return Record.QuestId == TEXT("quest.agency_has_a_price"); });
            TestTrue("Agency effect is durable", Effect != nullptr && Effect->ChoiceBranchTag == Branch);
            const FString Slot = FString(TEXT("agency-")) + Branch.ToString();
            TestTrue("Agency branch saves", Saves.SaveCampaign(BranchCampaign, Slot).IsSuccess());
            TestTrue("Agency branch reloads", Saves.LoadCampaign(Slot).HasValue());
        }

        FDACampaignSnapshot IronBase = AgencyBase;
        TestTrue("Canonical Agency route completes", CompleteAgencyChoice(IronBase, Manifest, City, Utilities, Factions, *Pressure, TEXT("retrain"), 220));
        TestTrue("Reach Iron choice", DriveToIronChoice(IronBase, Manifest, City, Utilities, Factions, *Pressure));
        for (const FName Branch : {FName(TEXT("accept")), FName(TEXT("refuse")), FName(TEXT("favorable_terms")), FName(TEXT("defer"))})
        {
            FDACampaignSnapshot BranchCampaign = IronBase;
            FDAFirstHourProgressionContext Choice = Context(350, City, Utilities, Factions, *Pressure); Choice.ChoiceBranchTag = Branch;
            TestTrue("Iron branch completes", Apply(BranchCampaign, Manifest, TEXT("quest.iron_at_border"), Choice));
            const FDAQuestContentEffectRecord* Effect = BranchCampaign.NarrativeState.QuestContentEffectRecords.FindByPredicate(
                [](const FDAQuestContentEffectRecord& Record) { return Record.QuestId == TEXT("quest.iron_at_border"); });
            TestTrue("Iron effect is durable", Effect != nullptr && Effect->ChoiceBranchTag == Branch);
            const FString Slot = FString(TEXT("iron-")) + Branch.ToString();
            TestTrue("Iron branch saves", Saves.SaveCampaign(BranchCampaign, Slot).IsSuccess());
            TestTrue("Iron branch reloads", Saves.LoadCampaign(Slot).HasValue());
        }
    });

    It("runs the complete nine-quest production chain and reloads canonical authorities", [this]()
    {
        FDAFirstHourQuestManifest Manifest; TArray<FText> Errors;
        TestTrue("Manifest", FDAFirstHourQuestPipeline::LoadCanonical(Manifest, Errors));
        FDACampaignSnapshot Campaign; FDACitySimulationState City; FDAUtilityNetwork Utilities;
        TArray<FDAFactionState> Factions = UDAFactionSystem::CreateSynaraFactions();
        UDASystemicPressureSystem* Pressure = NewObject<UDASystemicPressureSystem>();
        TestTrue("First five reach Replacement choice", DriveToReplacementChoice(Campaign, Manifest, City, Utilities, Factions, *Pressure));
        FDAFirstHourProgressionContext Replacement = Context(150, City, Utilities, Factions, *Pressure);
        Replacement.ChoiceBranchTag = TEXT("accept");
        TestTrue("Replacement completes", Apply(Campaign, Manifest, TEXT("quest.replacement_model"), Replacement));
        TestTrue("Final four complete", DriveFinalFour(Campaign, Manifest, City, Utilities, Factions, *Pressure));
        for (const FDAFirstHourQuestEntry& Quest : Manifest.Quests)
        {
            const FDAQuestSaveState* State = Campaign.NarrativeState.FindQuestState(Quest.Definition.QuestId);
            TestTrue("Every canonical quest completed", State != nullptr && State->ProgressState == EDAQuestProgressState::Completed);
        }
        TestEqual("Agency authority is canonical", Campaign.SynaraState.AgencyPetition, EDAAgencyPetitionResolution::RetrainWorkers);
        TestEqual("Iron authority is canonical", Campaign.SynaraState.IronBorder, EDAIronBorderResolution::Accepted);
        FString ValidationError; TestTrue("Full aggregate validates", Campaign.Validate(ValidationError));
        FDACampaignSnapshot RewardTamper = Campaign;
        RewardTamper.NarrativeState.QuestContentUnlockRecords[0].Quantity = 2;
        TestFalse("Exact reward payload tamper rejected", RewardTamper.Validate(ValidationError));
        FDACampaignSnapshot CollectionTamper = Campaign;
        const FDAQuestContentUnlockRecord* CardReward = CollectionTamper.NarrativeState.QuestContentUnlockRecords.FindByPredicate(
            [](const FDAQuestContentUnlockRecord& Unlock) { return Unlock.Type == EDAQuestContentUnlockType::CardInstance; });
        CollectionTamper.CollectionState.Instances[CardReward->GrantedCardInstanceIds[0]].AcquisitionSource = EDAAcquisitionSource::Crafting;
        TestFalse("Reward collection acquisition tamper rejected", CollectionTamper.Validate(ValidationError));
        FDACampaignSnapshot BindingTamper = Campaign;
        const FDAQuestObjectiveAssetBindingRecord& Binding = BindingTamper.NarrativeState.QuestObjectiveAssetBindings[0];
        BindingTamper.FindWorldAssetRecord(Binding.WorldAssetId)->OwnerCivilizationId = TEXT("civilization.forgeweave");
        TestFalse("Objective binding authority tamper rejected", BindingTamper.Validate(ValidationError));
        FDACampaignSnapshot FingerprintTamper = Campaign;
        FingerprintTamper.NarrativeState.QuestObjectiveAssetBindings[0].QuestDefinitionFingerprint = TEXT("tampered");
        TestFalse("Objective binding definition fingerprint tamper rejected", FingerprintTamper.Validate(ValidationError));
        FDACampaignSnapshot BindTickTamper = Campaign;
        BindTickTamper.NarrativeState.QuestObjectiveAssetBindings[0].BindWorldTick = MAX_int64;
        TestFalse("Objective binding timing tamper rejected", BindTickTamper.Validate(ValidationError));
        FDACampaignSnapshot WorldMapTamper = Campaign;
        WorldMapTamper.NarrativeState.WorldMapAuthorityRecords[0].WorldTick += 1;
        TestFalse("Typed world-map provenance tamper rejected", WorldMapTamper.Validate(ValidationError));
        FDACampaignSnapshot AggregateTamper = Campaign;
        AggregateTamper.SynaraState.Dependency += 1.0;
        TestFalse("Aggregate result tamper rejected", AggregateTamper.Validate(ValidationError));
        const FString Directory = FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("Task20Full"), FGuid::NewGuid().ToString(EGuidFormats::Digits));
        FDASaveService Saves(Directory); TestTrue("Full chain save", Saves.SaveCampaign(Campaign, TEXT("full")).IsSuccess());
        const auto Loaded = Saves.LoadCampaign(TEXT("full"));
        TestTrue("Full chain reload", Loaded.HasValue());
        if (Loaded.HasValue()) TestEqual("All nine survive reload", Loaded.GetValue().NarrativeState.QuestStates.Num(), 9);
    });
}
