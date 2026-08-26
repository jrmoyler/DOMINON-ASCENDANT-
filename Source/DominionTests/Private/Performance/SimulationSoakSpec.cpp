#include "Content/DARegionalCrisisContentRegistrySubsystem.h"
#include "Performance/DACanonicalSoakHarness.h"
#include "Fixtures/DAGameInstanceSubsystemFixture.h"
#include "HAL/FileManager.h"
#include "LOD/DASimulationLODSubsystem.h"
#include "Networks/DAUtilityNetwork.h"
#include "Performance/DASoakProgressGuard.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "HAL/PlatformTime.h"
#include "Narrative/DARegionalCampaignCoordinatorSubsystem.h"
#include "Regions/DAWorldStateSubsystem.h"
#include "Save/DASaveService.h"
#include "Time/DASimulationClockSubsystem.h"

BEGIN_DEFINE_SPEC(FDASimulationSoakSpec, "Dominion.Performance.SimulationSoak",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FDASimulationSoakSpec)

namespace DA::PerformanceTests
{
    std::string NarrowName(const FName Name)
    {
        return std::string(TCHAR_TO_UTF8(*Name.ToString()));
    }

    EDACampaignUtilitySupply ToCampaignSupply(const EDAUtilityState State)
    {
        switch (State)
        {
        case EDAUtilityState::FullySupplied: return EDACampaignUtilitySupply::FullySupplied;
        case EDAUtilityState::MinorDeficit: return EDACampaignUtilitySupply::MinorDeficit;
        case EDAUtilityState::SignificantDeficit: return EDACampaignUtilitySupply::SignificantDeficit;
        case EDAUtilityState::Critical: return EDACampaignUtilitySupply::Critical;
        default: return EDACampaignUtilitySupply::Offline;
        }
    }

    FString Sha256Bytes(const TArray<uint8>& Bytes)
    {
        uint8 Digest[32] = {};
        FSHA256 Hasher;
        Hasher.Update(Bytes.GetData(), static_cast<uint64>(Bytes.Num()));
        Hasher.Final();
        Hasher.GetHash(Digest);
        FString Result;
        Result.Reserve(64);
        for (const uint8 Byte : Digest) Result += FString::Printf(TEXT("%02x"), Byte);
        return Result;
    }

    bool HasDuplicateWorldAssetId(const FDACampaignSnapshot& Campaign)
    {
        TSet<FGuid> Ids;
        for (const FDAWorldAssetRecord& Asset : Campaign.WorldAssets)
        {
            if (Ids.Contains(Asset.WorldAssetId)) return true;
            Ids.Add(Asset.WorldAssetId);
        }
        return false;
    }

    bool HasStuckRequiredQuest(const FDACampaignSnapshot& Campaign)
    {
        for (const FDAQuestSaveState& Quest : Campaign.NarrativeState.QuestStates)
        {
            if (Quest.ProgressState != EDAQuestProgressState::Active) continue;
            const FDAQuestNodeDefinition* Node = Quest.DefinitionManifest.FindNode(Quest.CurrentNodeId);
            if (Node == nullptr || Node->Type == EDAQuestNodeType::Resolution
                || Node->Edges.IsEmpty()) return true;
        }
        return false;
    }

    FDASoakResult RunCanonicalSoak(const FString& SaveDirectory,
        UDASimulationLODSubsystem* Instrumentation)
    {
        FDASoakResult Result;
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        UDASimulationClockSubsystem* Clock = Fixture.GetSubsystem<UDASimulationClockSubsystem>();
        UDARegionalCampaignCoordinatorSubsystem* Coordinator =
            Fixture.GetSubsystem<UDARegionalCampaignCoordinatorSubsystem>();
        UDARegionalCrisisContentRegistrySubsystem* Events =
            Fixture.GetSubsystem<UDARegionalCrisisContentRegistrySubsystem>();
        if (World == nullptr || Clock == nullptr || Coordinator == nullptr || Events == nullptr
            || !Events->IsReady() || !World->InitializeVerticalSliceState(
                TEXT("region.synara_frontier"), 0)) return Result;

        FDACampaignSnapshot Seed = World->GetPersistentCampaign();
        Seed.CitySimulationState.Population = 120;
        Seed.CitySimulationState.Wallet = FDAWalletValues(250.f, 50.f, 25.f);
        Seed.LiveSignals.Population = Seed.CitySimulationState.Population;
        Seed.LiveSignals.Capital = Seed.CitySimulationState.Wallet.Capital;
        Seed.LiveSignals.Insight = Seed.CitySimulationState.Wallet.Insight;
        Seed.LiveSignals.Influence = Seed.CitySimulationState.Wallet.Influence;
        const FGuid UtilitySourceId(0xDA270001, 0x00000001, 0x00000001, 0x00000001);
        const FGuid UtilityDemandId(0xDA270001, 0x00000001, 0x00000001, 0x00000002);
        Seed.WorldAssets.Reserve(Seed.WorldAssets.Num() + 2);
        FDAWorldAssetRecord& UtilitySource = Seed.WorldAssets.Emplace_GetRef();
        UtilitySource.WorldAssetId = UtilitySourceId;
        UtilitySource.CardDefinitionId = TEXT("universal.microgrid_station");
        UtilitySource.CityId = TEXT("city.soak_utility");
        UtilitySource.ConstructionState = EDAConstructionState::Operational;
        UtilitySource.OwnerCivilizationId = TEXT("civilization.synara");
        UtilitySource.CardInstanceId = FGuid(0xDA270002, 0x00000001, 0x00000001, 0x00000001);
        FDAWorldAssetRecord& UtilityDemand = Seed.WorldAssets.Emplace_GetRef();
        UtilityDemand.WorldAssetId = UtilityDemandId;
        UtilityDemand.CardDefinitionId = TEXT("universal.water_reclaimer");
        UtilityDemand.CityId = TEXT("city.soak_utility");
        UtilityDemand.ConstructionState = EDAConstructionState::Operational;
        UtilityDemand.OwnerCivilizationId = TEXT("civilization.synara");
        UtilityDemand.CardInstanceId = FGuid(0xDA270002, 0x00000001, 0x00000001, 0x00000002);
        for (const FDAWorldAssetRecord* Asset : {&UtilitySource, &UtilityDemand})
        {
            if (!Seed.CollectionState.AddInstanceWithId(Asset->CardInstanceId,
                Asset->CardDefinitionId, EDAAcquisitionSource::Conquest,
                Seed.WorldState.CurrentWorldTick)) return Result;
            FCardInstance* Card = Seed.CollectionState.FindInstance(Asset->CardInstanceId);
            if (Card == nullptr) return Result;
            Card->WorldAssetId = Asset->WorldAssetId;
            Card->RecoveryState = EDARecoveryState::Deployed;
            FDAFacilityContext& Facility = Seed.CitySimulationState.Facilities.Emplace_GetRef();
            Facility.AssetRecord = *Asset;
        }

        FDAUtilityNetwork UtilityTopology;
        UtilityTopology.RegisterNode(EDAUtilityType::Power, UtilitySourceId, 25.f, 0.f);
        UtilityTopology.RegisterNode(EDAUtilityType::Power, UtilityDemandId, 0.f, 10.f);
        if (!UtilityTopology.ConnectNodes(EDAUtilityType::Power, UtilitySourceId, UtilityDemandId))
            return Result;
        const FDAUtilityResolution SourceResolution =
            UtilityTopology.ResolveUtility(EDAUtilityType::Power, UtilitySourceId);
        const FDAUtilityResolution DemandResolution =
            UtilityTopology.ResolveUtility(EDAUtilityType::Power, UtilityDemandId);
        Result.UtilityTopologyNodeCount = 2;
        Result.bUtilityTopologyResolved =
            SourceResolution.State == EDAUtilityState::FullySupplied
            && DemandResolution.State == EDAUtilityState::FullySupplied;
        for (const TPair<FGuid, EDAUtilityState>& Utility : {
            TPair<FGuid, EDAUtilityState>(UtilitySourceId, SourceResolution.State),
            TPair<FGuid, EDAUtilityState>(UtilityDemandId, DemandResolution.State)})
        {
            FDACampaignUtilitySignal Signal;
            Signal.WorldAssetId = Utility.Key;
            Signal.Utility = EDACampaignUtilityKind::Power;
            Signal.Supply = ToCampaignSupply(Utility.Value);
            Seed.LiveSignals.UtilitySignals.Add(Signal);
            Seed.CitySimulationState.UtilitySignals.Add(Signal);
        }
        if (!World->RestorePersistentCampaign(Seed) || !Coordinator->SynchronizeNow()) return Result;

        const FDASaveService Saves(SaveDirectory);
        if (!Saves.SaveCampaign(World->GetPersistentCampaign(), TEXT("initial")).IsSuccess())
            return Result;
        Result.InitialSaveBytes = IFileManager::Get().FileSize(
            *FPaths::Combine(SaveDirectory, TEXT("initial.dasave")));
        Result.EnabledEventCount = Events->GetManifest().Events.Num();
        DA::Soak::ProgressGuard ProgressGuard(
            static_cast<std::size_t>(Result.EnabledEventCount),
            MaximumRequiredQuestStaleDevelopmentCycles);
        bool bAllCyclesAdvanced = true;
        for (int32 Cycle = 1; Cycle <= SoakDevelopmentCycles; ++Cycle)
        {
            const int64 PreviousCycle = Clock->GetCurrentDevelopmentCycle();
            const double StartedAt = FPlatformTime::Seconds();
            Clock->AdvanceSimulation(UDASimulationClockSubsystem::DevelopmentCycleDurationSeconds);
            const double Elapsed = FPlatformTime::Seconds() - StartedAt;
            Result.WorstDevelopmentCycleSeconds = FMath::Max(
                Result.WorstDevelopmentCycleSeconds, Elapsed);
            ++Result.DevelopmentCycleSamples;
            if (Instrumentation != nullptr
                && !Instrumentation->RecordDevelopmentCycleSeconds(Elapsed))
            {
                bAllCyclesAdvanced = false;
                break;
            }
            if (Clock->GetCurrentDevelopmentCycle() != PreviousCycle + 1)
            {
                bAllCyclesAdvanced = false;
                break;
            }

            const TArray<FDAQuestSaveState> QuestStates =
                World->GetPersistentCampaign().NarrativeState.QuestStates;
            for (const FDAQuestSaveState& Quest : QuestStates)
            {
                if (Quest.ProgressState != EDAQuestProgressState::Active) continue;
                const FDAQuestNodeDefinition* Node = Quest.DefinitionManifest.FindNode(Quest.CurrentNodeId);
                if (Node == nullptr || Node->Type != EDAQuestNodeType::Choice) continue;
                if (Quest.QuestId == TEXT("quest.foundry_shortage"))
                {
                    const FGuid ActionId(0xDA270027, 0x00000001, 0x00000001,
                        static_cast<uint32>(Cycle));
                    if (World->ResolveFoundryShortage(ActionId,
                            EDAFoundryShortageResolution::IndustrialSupport)
                        != EDAFoundryShortageActionResult::Applied)
                    {
                        Result.bQuestResolutionFailed = true;
                    }
                    continue;
                }
                const FDARegionalQuestEntry* Entry = Events->GetManifest().FindQuest(Quest.QuestId);
                if (Entry == nullptr || Entry->Choices.IsEmpty()
                    || Coordinator->SubmitQuestChoice(Quest.QuestId, Entry->Choices[0])
                        != EDAQuestRuntimeResult::Applied)
                {
                    Result.bQuestResolutionFailed = true;
                }
            }

            const FDACampaignSnapshot& CycleCampaign = World->GetPersistentCampaign();
            Result.bPopulationStayedNonnegative &= CycleCampaign.LiveSignals.Population >= 0
                && CycleCampaign.WorldState.Forgeweave.Population >= 0;
            std::vector<DA::Soak::EventSnapshot> EventSnapshots;
            EventSnapshots.reserve(CycleCampaign.NarrativeState.EventStates.Num());
            for (const FDAWorldEventSaveState& Event : CycleCampaign.NarrativeState.EventStates)
            {
                EventSnapshots.push_back({NarrowName(Event.EventId), NarrowName(Event.CurrentStageId),
                    Event.ProgressState == EDAWorldEventProgressState::Active,
                    static_cast<std::size_t>(Event.DefinitionManifest.Stages.Num())});
            }
            std::vector<DA::Soak::QuestSnapshot> QuestSnapshots;
            QuestSnapshots.reserve(CycleCampaign.NarrativeState.QuestStates.Num());
            for (const FDAQuestSaveState& Quest : CycleCampaign.NarrativeState.QuestStates)
            {
                QuestSnapshots.push_back({NarrowName(Quest.QuestId), NarrowName(Quest.CurrentNodeId),
                    Quest.ProgressState == EDAQuestProgressState::Active, true});
            }
            ProgressGuard.ObserveCycle(Cycle, EventSnapshots, QuestSnapshots);
        }
        Result.EventEmissionCount = static_cast<int32>(ProgressGuard.EventEmissionCount());
        Result.EventTransitionCount = static_cast<int32>(ProgressGuard.EventTransitionCount());
        Result.bEventRunawayDetected = ProgressGuard.EventRunawayDetected();
        Result.bStaleRequiredQuestDetected = ProgressGuard.StaleRequiredQuestDetected();
        Result.bAdvanced = bAllCyclesAdvanced
            && Clock->GetCurrentDevelopmentCycle() == SoakDevelopmentCycles;
        Result.Campaign = World->GetPersistentCampaign();
        if (!Result.bAdvanced
            || !Saves.SaveCampaign(Result.Campaign, TEXT("final")).IsSuccess()) return Result;
        const FString FinalPath = FPaths::Combine(SaveDirectory, TEXT("final.dasave"));
        Result.FinalSaveBytes = IFileManager::Get().FileSize(*FinalPath);
        FFileHelper::LoadFileToString(Result.FinalSave, *FinalPath);
        TArray<uint8> SavedBytes;
        if (FFileHelper::LoadFileToArray(SavedBytes, *FinalPath))
            Result.FinalSaveSha256 = Sha256Bytes(SavedBytes);
        return Result;
    }
}

using namespace DA::PerformanceTests;

void FDASimulationSoakSpec::Define()
{
    Describe("the canonical 1,000 Development Cycle owner soak", [this]()
    {
        It("advances economy population Forgeweave trade and exactly six events without invalid state", [this]()
        {
            const FString Directory = FPaths::Combine(FPaths::ProjectIntermediateDir(),
                TEXT("DA_Task27_Soak_") + FGuid::NewGuid().ToString(EGuidFormats::Digits));
            const FDASoakResult Result = RunCanonicalSoak(Directory);

            TestTrue("Canonical world owner advances the soak", Result.bAdvanced);
            TestEqual("Clock resolves exactly 1,000 Development Cycles",
                Result.Campaign.WorldState.ClockAuthority.CurrentDevelopmentCycle,
                static_cast<int64>(SoakDevelopmentCycles));
            TestEqual("Player economy owner resolves every cycle",
                Result.Campaign.LiveSignals.ResolvedDevelopmentCycles,
                static_cast<int64>(SoakDevelopmentCycles));
            TestEqual("Player population remains possible", Result.Campaign.LiveSignals.Population, 120);
            TestEqual("Exactly six authored events are enabled", Result.EnabledEventCount, 6);
            TestEqual("All 1,000 cycle boundaries are individually timed",
                Result.DevelopmentCycleSamples, SoakDevelopmentCycles);
            TestEqual("Trade advances with the canonical World Tick owner",
                Result.Campaign.WorldState.Trade.LastProcessedWorldTick,
                static_cast<int64>(SoakWorldTicks));
            TestEqual("Forgeweave AI advances with the canonical World Tick owner",
                Result.Campaign.WorldState.Forgeweave.LastProcessedWorldTick,
                static_cast<int64>(SoakWorldTicks));

            FString ValidationError;
            TestTrue("Final campaign authority validates", Result.Campaign.Validate(ValidationError));
            TestFalse("Player money is finite",
                !FMath::IsFinite(Result.Campaign.LiveSignals.Capital)
                    || !FMath::IsFinite(Result.Campaign.LiveSignals.Insight)
                    || !FMath::IsFinite(Result.Campaign.LiveSignals.Influence));
            TestFalse("Forgeweave money is finite",
                !FMath::IsFinite(Result.Campaign.WorldState.Forgeweave.Capital));
            TestTrue("Population is never impossible", Result.Campaign.LiveSignals.Population >= 0
                && Result.Campaign.WorldState.Forgeweave.Population >= 0);
            TestTrue("Population remains nonnegative at every cycle boundary",
                Result.bPopulationStayedNonnegative);
            TestFalse("WorldAsset identities remain unique", HasDuplicateWorldAssetId(Result.Campaign));
            TestEqual("Canonical soak seeds a nonempty two-node utility topology",
                Result.UtilityTopologyNodeCount, 2);
            TestTrue("Canonical utility topology resolves and persists supplied signals",
                Result.bUtilityTopologyResolved
                    && Result.Campaign.LiveSignals.UtilitySignals.Num() == 2);
            TestTrue("Event emissions remain bounded over every cycle",
                Result.EventEmissionCount <= Result.EnabledEventCount);
            TestFalse("Per-cycle event transitions never run away",
                Result.bEventRunawayDetected);
            TestFalse("Required active quest nodes do not exceed the explicit 25-cycle stale bound",
                Result.bStaleRequiredQuestDetected);
            TestFalse("Deterministic required quest choices resolve through the canonical coordinator",
                Result.bQuestResolutionFailed);
            TestFalse("No required quest is permanently stuck",
                HasStuckRequiredQuest(Result.Campaign));
            TestTrue("Final save stays within the 8 MiB soak budget",
                Result.FinalSaveBytes >= 0 && Result.FinalSaveBytes <= MaximumSaveBytes);
            TestTrue("Save growth stays within the 2 MiB soak delta budget",
                Result.InitialSaveBytes >= 0 && Result.FinalSaveBytes - Result.InitialSaveBytes
                    <= MaximumSaveGrowthBytes);

            IFileManager::Get().DeleteDirectory(*Directory, false, true);
        });

        It("repeats deterministically from the same canonical campaign seed", [this]()
        {
            const FString FirstDirectory = FPaths::Combine(FPaths::ProjectIntermediateDir(),
                TEXT("DA_Task27_RepeatA_") + FGuid::NewGuid().ToString(EGuidFormats::Digits));
            const FString SecondDirectory = FPaths::Combine(FPaths::ProjectIntermediateDir(),
                TEXT("DA_Task27_RepeatB_") + FGuid::NewGuid().ToString(EGuidFormats::Digits));

            const FDASoakResult First = RunCanonicalSoak(FirstDirectory);
            const FDASoakResult Second = RunCanonicalSoak(SecondDirectory);
            TestTrue("First canonical soak advances", First.bAdvanced);
            TestTrue("Second canonical soak advances", Second.bAdvanced);
            TestEqual("Canonical final save bytes repeat exactly", First.FinalSave, Second.FinalSave);
            TestEqual("Canonical final save SHA-256 repeats exactly",
                First.FinalSaveSha256, Second.FinalSaveSha256);

            IFileManager::Get().DeleteDirectory(*FirstDirectory, false, true);
            IFileManager::Get().DeleteDirectory(*SecondDirectory, false, true);
        });
    });

    Describe("simulation LOD and benchmark instrumentation", [this]()
    {
        It("preserves one named CitizenID through all LODs and canonical save reconstruction", [this]()
        {
            UDASimulationLODSubsystem* LOD = NewObject<UDASimulationLODSubsystem>();
            const FName CitizenId(TEXT("citizen.synara.nia_vale"));
            TestTrue("Named citizen registers", LOD->RegisterNamedCitizen(CitizenId));

            const TArray<FDASimulationLODContext> Contexts = {
                FDASimulationLODContext::FounderDistrict(),
                FDASimulationLODContext::NearbyDistrict(),
                FDASimulationLODContext::DistantActiveCity(),
                FDASimulationLODContext::UnloadedMajorRegion()};
            for (int32 Index = 0; Index < Contexts.Num(); ++Index)
            {
                TestTrue("Named citizen changes representation", LOD->TransitionNamedCitizen(CitizenId, Contexts[Index]));
                const FDANamedCitizenLODState* State = LOD->FindNamedCitizen(CitizenId);
                TestNotNull("Named citizen retains a representation state", State);
                if (State != nullptr)
                {
                    TestEqual("CitizenID never changes", State->CitizenId, CitizenId);
                    TestEqual("Exact simulation LOD is selected", static_cast<uint8>(State->LOD),
                        static_cast<uint8>(Index));
                }
            }

            FDACampaignSnapshot Campaign;
            FDACampaignCitizenSignal Citizen;
            Citizen.CitizenId = CitizenId;
            Citizen.CityId = TEXT("player_capital");
            Campaign.LiveSignals.Citizens.Add(Citizen);
            const FString Directory = FPaths::Combine(FPaths::ProjectIntermediateDir(),
                TEXT("DA_Task27_LOD_") + FGuid::NewGuid().ToString(EGuidFormats::Digits));
            const FDASaveService Saves(Directory);
            TestTrue("Campaign saves the stable CitizenID",
                Saves.SaveCampaign(Campaign, TEXT("lod-citizen")).IsSuccess());
            const TResult<FDACampaignSnapshot, FDASaveError> Loaded =
                Saves.LoadCampaign(TEXT("lod-citizen"));
            TestTrue("Campaign reloads the stable CitizenID", Loaded.HasValue());
            UDASimulationLODSubsystem* Restored = NewObject<UDASimulationLODSubsystem>();
            TestTrue("Save projection reconstructs named citizens", Loaded.HasValue()
                && Restored->RebuildNamedCitizensFromCampaign(Loaded.GetValue().LiveSignals,
                    FDASimulationLODContext::UnloadedMajorRegion()));
            TestNotNull("Restored CitizenID remains logically stable",
                Restored->FindNamedCitizen(CitizenId));
            IFileManager::Get().DeleteDirectory(*Directory, false, true);
        });

        It("evaluates recorded samples against explicit budgets without inventing a capture", [this]()
        {
            UDASimulationLODSubsystem* Instrumentation = NewObject<UDASimulationLODSubsystem>();
            const FDABenchmarkSummary NotRun = Instrumentation->FinishBenchmarkCapture(false);
            TestTrue("No sample remains NOT_RUN", NotRun.State == EDABenchmarkEvidenceState::NotRun);
            TestFalse("No sample cannot claim a pass", NotRun.bPerformanceTargetsPassed);

            Instrumentation->BeginBenchmarkCapture();
            bool bRecordedFrames = true;
            for (int32 Frame = 0; Frame < 600; ++Frame)
                bRecordedFrames &= Instrumentation->RecordFrameSeconds(1.0 / 60.0);
            TestTrue("Records 600 valid frames", bRecordedFrames);
            TestTrue("Records a Development Cycle boundary", Instrumentation->RecordDevelopmentCycleSeconds(0.050));
            TestTrue("Records City transition", Instrumentation->RecordModeTransitionSeconds(0.7));
            TestTrue("Records Command transition", Instrumentation->RecordModeTransitionSeconds(0.8));
            TestTrue("Records Founder transition", Instrumentation->RecordModeTransitionSeconds(0.9));
            const FDABenchmarkSummary Captured = Instrumentation->FinishBenchmarkCapture(false);
            TestTrue("Recorded sample becomes CAPTURED", Captured.State == EDABenchmarkEvidenceState::Captured);
            TestFalse("Synthetic samples never claim reference-hardware targets",
                Captured.bPerformanceTargetsPassed);
            TestEqual("Severe hitch is explicitly 100 ms", Captured.SevereHitchThresholdMilliseconds, 100.0);
            TestEqual("Transition budget is exactly 1.5 seconds", Captured.TransitionBudgetSeconds, 1.5);
        });

        It("writes measured instrumentation and canonical soak output through the production evidence writer", [this]()
        {
            const FString Directory = FPaths::Combine(FPaths::ProjectIntermediateDir(),
                TEXT("DA_Task27_EvidenceWriter_") + FGuid::NewGuid().ToString(EGuidFormats::Digits));
            const FString PlanPath = FPaths::Combine(Directory, TEXT("plan.json"));
            const FString EvidencePath = FPaths::Combine(Directory, TEXT("candidate.json"));
            IFileManager::Get().MakeDirectory(*Directory, true);
            TestTrue("Test plan bytes exist", FFileHelper::SaveStringToFile(
                TEXT("{\"sceneId\":\"benchmark.vertical_slice.synara_capital\"}\n"), *PlanPath));

            UDASimulationLODSubsystem* Instrumentation = NewObject<UDASimulationLODSubsystem>();
            Instrumentation->BeginBenchmarkCapture();
            for (int32 Frame = 0; Frame < 600; ++Frame)
                Instrumentation->RecordFrameSeconds(1.0 / 60.0);
            for (int32 Cycle = 0; Cycle < 1000; ++Cycle)
                Instrumentation->RecordDevelopmentCycleSeconds(0.050);
            Instrumentation->RecordModeTransitionSeconds(0.6);
            Instrumentation->RecordModeTransitionSeconds(0.45);
            Instrumentation->RecordModeTransitionSeconds(0.35);

            FDABenchmarkEvidenceCapture Capture;
            Capture.Performance = Instrumentation->FinishBenchmarkCapture(true);
            Capture.Hardware.Cpu = TEXT("Ryzen 5 5600 or equivalent");
            Capture.Hardware.Gpu = TEXT("RTX 3060 or RX 6700 XT class");
            Capture.Hardware.SystemMemoryGiB = 16;
            Capture.Hardware.Storage = TEXT("SSD");
            Capture.Soak.DevelopmentCycles = 1000;
            Capture.Soak.EnabledEventCount = 6;
            Capture.Soak.PlayerEconomyDevelopmentCycles = 1000;
            Capture.Soak.TradeWorldTicks = 200;
            Capture.Soak.ForgeweaveWorldTicks = 200;
            Capture.Soak.UtilityTopologyNodeCount = 2;
            Capture.Soak.bUtilityTopologyResolved = true;
            Capture.Soak.bPopulationStayedNonnegative = true;
            Capture.Soak.EventEmissionCount = 4;
            Capture.Soak.EventTransitionCount = 8;
            Capture.Soak.InitialSaveBytes = 1000;
            Capture.Soak.FinalSaveBytes = 1100;
            Capture.Soak.RepeatabilityDigestA = FString::ChrN(64, TEXT('a'));
            Capture.Soak.RepeatabilityDigestB = Capture.Soak.RepeatabilityDigestA;

            FString Error;
            TestTrue("Production writer serializes a closed candidate payload",
                Capture.WriteCandidate(EvidencePath, PlanPath, Error));
            FString Written;
            TestTrue("Candidate is readable", FFileHelper::LoadFileToString(Written, *EvidencePath));
            TestTrue("Candidate carries production frame samples",
                Written.Contains(TEXT("\"frameSampleCount\": 600")));
            TestTrue("Candidate carries canonical soak cycles",
                Written.Contains(TEXT("\"developmentCycles\": 1000")));
            IFileManager::Get().DeleteDirectory(*Directory, false, true);
        });
    });
}
