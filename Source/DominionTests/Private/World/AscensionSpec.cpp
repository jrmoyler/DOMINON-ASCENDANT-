#include "Ascension/DAAscensionSystem.h"
#include "Adjacency/DAAdjacencySubsystem.h"
#include "Campaign/DADaxtonCampaignState.h"
#include "Citizens/DAJobSystem.h"
#include "Content/DACardDefinition.h"
#include "Content/DAContentRegistrySubsystem.h"
#include "Diplomacy/DADiplomacySystem.h"
#include "Economy/DAEconomySubsystem.h"
#include "Fixtures/DAGameInstanceSubsystemFixture.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Regions/DAWorldStateSubsystem.h"
#include "Save/DASaveService.h"
#include "Time/DASimulationClockSubsystem.h"

namespace
{
    void LinkFixtureAsset(FDACampaignSnapshot& Campaign, FDAWorldAssetRecord& Asset,
        const FGuid CardInstanceId)
    {
        const bool bCanonicalFixture = Campaign.WorldState.ClockAuthority.bCaptured
            || Campaign.CitySimulationState.bInitialized
            || Campaign.DeckState.GetInstanceIds().Num() == FDADeckState::RequiredDeckSize;
        if (!bCanonicalFixture) return;
        Asset.CardInstanceId = CardInstanceId;
        Campaign.CollectionState.AddInstanceWithId(CardInstanceId, Asset.CardDefinitionId,
            EDAAcquisitionSource::Conquest, Campaign.WorldState.CurrentWorldTick);
        if (FCardInstance* Card = Campaign.CollectionState.FindInstance(CardInstanceId))
        {
            Card->WorldAssetId = Asset.WorldAssetId;
            Card->RecoveryState = EDARecoveryState::Deployed;
        }
        if (Campaign.CitySimulationState.bInitialized
            && Asset.OwnerCivilizationId == TEXT("civilization.synara"))
        {
            FDAFacilityContext& Facility =
                Campaign.CitySimulationState.Facilities.Emplace_GetRef();
            Facility.AssetRecord = Asset;
        }
    }

    void AddHistory(FDACampaignSnapshot& Campaign, const FName Tag)
    {
        Campaign.HistoryTags.AddUnique(Tag);
        Campaign.HistoryTags.Sort([](const FName Left, const FName Right)
        { return Left.LexicalLess(Right); });
    }

    void AddRelationshipReason(FDADiplomaticRelationship& Relationship,
        const EDADiplomaticMetric Metric, const float Magnitude, const FName MutationId,
        const int64 WorldTick)
    {
        FDADiplomaticReason& Reason = Relationship.ReasonLedger.Emplace_GetRef();
        Reason.MutationId = MutationId;
        Reason.SourceTag = TEXT("test.ascension.canonical");
        Reason.Metric = Metric;
        Reason.Magnitude = Magnitude;
        Reason.WorldTick = WorldTick;
        if (Metric == EDADiplomaticMetric::Trust) Relationship.Trust += Magnitude;
        else if (Metric == EDADiplomaticMetric::Respect) Relationship.Respect += Magnitude;
        else if (Metric == EDADiplomaticMetric::Compatibility) Relationship.Compatibility += Magnitude;
    }

    void AddValidCompletedQuest(FDACampaignSnapshot& Campaign, const FName QuestId)
    {
        FDAQuestSaveState& Quest = Campaign.NarrativeState.QuestStates.Emplace_GetRef();
        Quest.QuestId = QuestId;
        Quest.DefinitionManifest.QuestId = QuestId;
        Quest.DefinitionManifest.SourceDefinitionId = FName(*(TEXT("test.definition.") + QuestId.ToString()));
        Quest.DefinitionManifest.StartNodeId = TEXT("start");
        FDAQuestNodeDefinition& Start = Quest.DefinitionManifest.Nodes.Emplace_GetRef();
        Start.NodeId = TEXT("start");
        Start.Type = EDAQuestNodeType::Start;
        Start.SourceDefinitionId = TEXT("test.node.start");
        FDAQuestEdgeDefinition& Edge = Start.Edges.Emplace_GetRef();
        Edge.BranchTag = TEXT("complete");
        Edge.TargetNodeId = TEXT("resolution");
        FDAQuestNodeDefinition& Resolution = Quest.DefinitionManifest.Nodes.Emplace_GetRef();
        Resolution.NodeId = TEXT("resolution");
        Resolution.Type = EDAQuestNodeType::Resolution;
        Resolution.SourceDefinitionId = TEXT("test.node.resolution");
        Quest.DefinitionManifest.RefreshFingerprint();
        Quest.DefinitionVersion = 1;
        Quest.CurrentNodeId = TEXT("resolution");
        Quest.ProgressState = EDAQuestProgressState::Completed;
        Quest.CompletedNodeIds.Add(TEXT("start"));
        FDAQuestNodeTransitionRecord& Transition = Quest.NodeTransitionRecords.Emplace_GetRef();
        Transition.CompletedNodeId = TEXT("start");
        Transition.EnteredNodeId = TEXT("resolution");
    }

    void AddGrandForge(FDACampaignSnapshot& Campaign)
    {
        FDAWorldAssetRecord& Forge = Campaign.WorldAssets.Emplace_GetRef();
        Forge.WorldAssetId = FGuid(25, 1, 1, 1);
        Forge.CardDefinitionId = TEXT("forgeweave.grand_forge");
        Forge.CityId = TEXT("city.ironheart");
        Forge.OwnerCivilizationId = TEXT("civilization.forgeweave");
        Forge.ConstructionState = EDAConstructionState::Operational;
        LinkFixtureAsset(Campaign, Forge, FGuid(25, 1, 1, 2));
        FDAStructuralDamageRecord& Damage = Campaign.OperationConflict.StructuralDamageRecords.Emplace_GetRef();
        Damage.WorldAssetId = Forge.WorldAssetId;
        Damage.CardDefinitionId = Forge.CardDefinitionId;
        Damage.Modules = {
            FDAStructureModuleHealthRecord(TEXT("module.coolant"), 100.f, true),
            FDAStructureModuleHealthRecord(TEXT("module.production"), 100.f, true),
            FDAStructureModuleHealthRecord(TEXT("module.structure"), 100.f, false)};
        FDACampaignCitizenSignal* Worker = Campaign.LiveSignals.Citizens.FindByPredicate(
            [](const FDACampaignCitizenSignal& Row)
            { return Row.CitizenId == TEXT("citizen.forgeweave.mara_kest"); });
        if (Worker == nullptr)
        {
            Worker = &Campaign.LiveSignals.Citizens.Emplace_GetRef();
            Worker->CitizenId = TEXT("citizen.forgeweave.mara_kest");
        }
        Worker->CityId = TEXT("city.ironheart");
        Worker->JobId = TEXT("job.forgeweave.grand_forge.worker");
        FDACitizenRecord* CityWorker = Campaign.CitySimulationState.Citizens.FindByPredicate(
            [](const FDACitizenRecord& Row)
            { return Row.CitizenId == TEXT("citizen.forgeweave.mara_kest"); });
        if (Campaign.CitySimulationState.bInitialized && CityWorker != nullptr)
        {
            CityWorker->CityId = Worker->CityId;
            CityWorker->JobId = Worker->JobId;
        }
        FDACampaignJobOpeningSignal& Opening = Campaign.LiveSignals.JobOpenings.Emplace_GetRef();
        Opening.JobId = Worker->JobId;
        Opening.CityId = Worker->CityId;
        Opening.FacilityWorldAssetId = Forge.WorldAssetId;
        Opening.OpenPositions = 4;
        FDACampaignJobAssignmentSignal& Assignment = Campaign.LiveSignals.JobAssignments.Emplace_GetRef();
        Assignment.CitizenId = Worker->CitizenId;
        Assignment.JobId = Worker->JobId;
        Assignment.FacilityWorldAssetId = Forge.WorldAssetId;
        if (Campaign.CitySimulationState.bInitialized)
        {
            FDAJobOpening& CityOpening = Campaign.CitySimulationState.JobOpenings.Emplace_GetRef();
            CityOpening.JobId = Opening.JobId;
            CityOpening.CityId = Opening.CityId;
            CityOpening.FacilityWorldAssetId = Opening.FacilityWorldAssetId;
            CityOpening.OpenPositions = Opening.OpenPositions;
            FDAJobAssignment& CityAssignment =
                Campaign.CitySimulationState.JobAssignments.Emplace_GetRef();
            CityAssignment.CitizenId = Assignment.CitizenId;
            CityAssignment.JobId = Assignment.JobId;
            CityAssignment.FacilityWorldAssetId = Assignment.FacilityWorldAssetId;
            CityAssignment.MatchQuality = EDAJobMatchQuality::Acceptable;
            CityAssignment.OutputMultiplier = 0.85f;
        }
        ++Campaign.LiveSignals.MutationRevision;
    }

    bool BuildAscensionPrerequisites(UDAWorldStateSubsystem& World, FGuid& OutSourceCard)
    {
        FDACampaignSnapshot Campaign = World.GetPersistentCampaign();
        FDADiplomaticRelationship* Forge = Campaign.WorldState.Diplomacy.FindRelationship(
            TEXT("relationship.synara.forgeweave"));
        if (Forge == nullptr)
        {
            Forge = &Campaign.WorldState.Diplomacy.Relationships.Emplace_GetRef();
            Forge->RelationshipId = TEXT("relationship.synara.forgeweave");
        }
        AddRelationshipReason(*Forge, EDADiplomaticMetric::Trust, 85.f,
            TEXT("reason.ascension.alliance.trust"), Campaign.WorldState.CurrentWorldTick);
        AddRelationshipReason(*Forge, EDADiplomaticMetric::Respect, 85.f,
            TEXT("reason.ascension.alliance.respect"), Campaign.WorldState.CurrentWorldTick);
        AddRelationshipReason(*Forge, EDADiplomaticMetric::Compatibility, 85.f,
            TEXT("reason.ascension.alliance.compatibility"), Campaign.WorldState.CurrentWorldTick);
        AddValidCompletedQuest(Campaign, TEXT("quest.third_foundry"));
        AddHistory(Campaign, TEXT("forge_relic_voluntary_transfer"));
        AddHistory(Campaign, TEXT("joint_forgeweave_crisis_success"));
        OutSourceCard = FGuid(25, 2, 1, 1);
        if (!Campaign.CollectionState.AddInstanceWithId(OutSourceCard,
            TEXT("synara.adaptive_habitat"), EDAAcquisitionSource::WorldDiscovery)) return false;
        if (!World.RestorePersistentCampaign(Campaign)) return false;

        FString Error;
        if (!World.CompleteForgeweaveConquestRoute(FGuid(25, 2, 2, 1),
            EDAForgeweaveRoute::Alliance, Error)) return false;

        Campaign = World.GetPersistentCampaign();
        Campaign.WorldState.Forgeweave.Population = FMath::Max(60, Campaign.WorldState.Forgeweave.Population);
        Campaign.WorldState.Forgeweave.ProductionReserve = 50.f;
        Campaign.WorldState.Forgeweave.ActiveIndustrialThroughput = 20.f;
        Campaign.WorldState.Forgeweave.ResourceHunger = 10.f;
        Campaign.WorldState.Forgeweave.LogisticsEfficiency = 80.f;
        AddGrandForge(Campaign);
        if (!World.RestorePersistentCampaign(Campaign)) return false;

        return World.StartDaxtonEncounter(FGuid(25, 3, 1, 1), Error)
            && World.ApplyDaxtonInteraction(FGuid(25, 3, 1, 2), EDADaxtonInteraction::Damage, 40.f, Error)
            && World.ApplyDaxtonInteraction(FGuid(25, 3, 1, 3), EDADaxtonInteraction::DisableCoolant, 1.f, Error)
            && World.ApplyDaxtonInteraction(FGuid(25, 3, 1, 4), EDADaxtonInteraction::RedirectSupply, 10.f, Error)
            && World.ApplyDaxtonInteraction(FGuid(25, 3, 1, 5), EDADaxtonInteraction::HackProduction, 10.f, Error)
            && World.ApplyDaxtonInteraction(FGuid(25, 3, 1, 6), EDADaxtonInteraction::WorkerShutdown, 1.f, Error)
            && World.ApplyDaxtonInteraction(FGuid(25, 3, 1, 7), EDADaxtonInteraction::Damage, 60.f, Error)
            && World.EnterDaxtonChoicePhase(FGuid(25, 3, 1, 8), Error)
            && World.CompleteDaxtonChoiceObjective(FGuid(25, 3, 1, 9), EDADaxtonChoiceObjective::DefeatDaxton, Error)
            && World.CompleteDaxtonChoiceObjective(FGuid(25, 3, 1, 10), EDADaxtonChoiceObjective::SaveGrandForge, Error)
            && World.CompleteDaxtonChoiceObjective(FGuid(25, 3, 1, 11), EDADaxtonChoiceObjective::EvacuateWorkers, Error)
            && World.CompleteDaxtonChoiceObjective(FGuid(25, 3, 1, 12),
                EDADaxtonChoiceObjective::StabilizeProductionOfferUnion, Error)
            && World.ResolveDaxtonLeaderState(FGuid(25, 3, 1, 13),
                EDADaxtonLeaderState::AlliedForgeLord, Error);
    }
}

BEGIN_DEFINE_SPEC(FDAAscensionSpec, "Dominion.World.Ascension.First",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FDAAscensionSpec)

void FDAAscensionSpec::Define()
{
    It("atomically commits the exact first Ascension reward and presentation state", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        FGuid SourceCard;
        TestTrue("Canonical conquest and Leader prerequisites resolve", World != nullptr
            && BuildAscensionPrerequisites(*World, SourceCard));
        if (World == nullptr) return;
        const double InsightBefore = World->GetLiveSignals().Insight;
        const double InfluenceBefore = World->GetLiveSignals().Influence;
        const FGuid ActionId(25, 4, 1, 1);
        FString Error;
        TestTrue("One owner transaction ascends Forgeweave",
            World->CompleteFirstAscension(ActionId, Error));
        const FDACampaignSnapshot& State = World->GetPersistentCampaign();
        TestTrue("Ascension resolves, grants Relic and unlocks Replication/Fusion",
            State.AscensionState.bForgeweaveAscended
            && State.AscensionState.RelicIds == TArray<FName>{TEXT("relic.forge")}
            && State.AscensionState.bReplicationUnlocked
            && State.AscensionState.bFusionEligible
            && State.AscensionState.UnlockedBlueprintIds.Contains(TEXT("fusion.autonomous_factory")));
        TestEqual("All fifteen Forgeweave definitions unlock",
            State.AscensionState.UnlockedForgeweaveDefinitionIds.Num(), 15);
        TestEqual("Influence receives exact v0.8 base reward", State.LiveSignals.Influence,
            InfluenceBefore + 10.0);
        TestEqual("Insight receives exact v0.8 base reward", State.LiveSignals.Insight,
            InsightBefore + 10.0);
        TestTrue("Founder Hall commits state three, slot one, chamber and Axiom reveal",
            State.AscensionState.FounderHallVisualState == 3
            && State.AscensionState.ActiveRelicSlotCount == 1
            && State.AscensionState.bHiddenChamberOpen
            && State.AscensionState.ConvergenceAuthority == 1
            && State.AscensionState.ConvergenceAuthorityMaximum == 20);
        TestTrue("Exact quest and history complete in the same transaction",
            State.HistoryTags.Contains(TEXT("first_relic_acquired"))
            && State.HistoryTags.Contains(TEXT("convergence_authority_1_of_20"))
            && State.NarrativeState.FindQuestState(TEXT("quest.convergence_authority")) != nullptr
            && State.NarrativeState.FindQuestState(TEXT("quest.convergence_authority"))->ProgressState
                == EDAQuestProgressState::Completed);
        const FDAAscensionPresentationState Presentation =
            World->GetAscensionPresentationState();
        const TArray<EDAAscensionPresentationBeat> ExpectedBeats = {
            EDAAscensionPresentationBeat::SystemsHaltAndReact,
            EDAAscensionPresentationBeat::ForgeRelicEmerges,
            EDAAscensionPresentationBeat::WorldTransit,
            EDAAscensionPresentationBeat::FounderHallReceivesRelic,
            EDAAscensionPresentationBeat::Unlocks};
        TestTrue("Runtime presentation hook reads only committed state and remains skippable",
            Presentation.bAscended && Presentation.FounderHallVisualState == 3
            && Presentation.bCinematicMayBeSkipped
            && Presentation.bShouldPlayCinematic
            && Presentation.CinematicSequenceAsset.ToString()
                == TEXT("/Game/Cinematics/CS_ForgeweaveAscension.CS_ForgeweaveAscension")
            && Presentation.OrderedBeats == ExpectedBeats
            && Presentation.ConvergenceAuthorityLabel == TEXT("CONVERGENCE AUTHORITY: 1/20"));
        TestEqual("Founder Hall persists exactly twenty Relic positions",
            Presentation.RelicPositions.Num(), 20);
        for (int32 Index = 0; Index < Presentation.RelicPositions.Num(); ++Index)
        {
            const FDAFounderHallRelicPosition& Position = Presentation.RelicPositions[Index];
            TestEqual("Relic position retains one-based slot identity", Position.SlotIndex, Index + 1);
            TestEqual("Only slot one is active", Position.bActive, Index == 0);
            TestEqual("Only slot one is occupied", Position.bOccupied, Index == 0);
            TestEqual("Only slot one owns the Forge Relic", Position.RelicId,
                Index == 0 ? FName(TEXT("relic.forge")) : NAME_None);
        }
        const int64 Revision = State.NarrativeState.MutationRevision;
        TestTrue("Same action is idempotent", World->CompleteFirstAscension(ActionId, Error));
        TestEqual("Idempotent replay creates no revision", World->GetPersistentCampaign().NarrativeState.MutationRevision, Revision);
        TestFalse("A different action cannot replay the unique first Ascension",
            World->CompleteFirstAscension(FGuid(25, 4, 1, 2), Error));
    });

    It("rolls back every reward when conquest or persistent Leader proof is missing", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        if (World == nullptr) return;
        const FDACampaignSnapshot Before = World->GetPersistentCampaign();
        FString Error;
        TestFalse("Unresolved campaign cannot ascend",
            World->CompleteFirstAscension(FGuid(25, 5, 1, 1), Error));
        const FDACampaignSnapshot& After = World->GetPersistentCampaign();
        TestFalse("No partial Ascension authority publishes", After.AscensionState.bForgeweaveAscended);
        TestEqual("No wallet reward leaks", After.LiveSignals.Insight, Before.LiveSignals.Insight);
        TestEqual("No history leaks", After.HistoryTags, Before.HistoryTags);
    });

    It("replicates one eligible Asset every twelve committed Development Cycles with provenance", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        UDASimulationClockSubsystem* Clock = Fixture.GetSubsystem<UDASimulationClockSubsystem>();
        FGuid SourceCard;
        if (World == nullptr || Clock == nullptr || !BuildAscensionPrerequisites(*World, SourceCard)) return;
        FString Error;
        TestTrue("Ascension commits", World->CompleteFirstAscension(FGuid(25, 6, 1, 1), Error));
        FGuid NewCard;
        TestFalse("Replication is unavailable before the twelfth cycle",
            World->ReplicateCard(FGuid(25, 6, 1, 2), SourceCard, NewCard, Error));
        Clock->AdvanceSimulation(UDASimulationClockSubsystem::DevelopmentCycleDurationSeconds * 12.0);
        TestTrue("Twelfth-cycle Replication commits",
            World->ReplicateCard(FGuid(25, 6, 1, 2), SourceCard, NewCard, Error));
        const FCardInstance* Replica = World->GetPersistentCampaign().CollectionState.FindInstance(NewCard);
        TestTrue("Replica is a new instance with exact acquisition and source provenance",
            Replica != nullptr && Replica->InstanceId != SourceCard
            && Replica->AcquisitionSource == EDAAcquisitionSource::Replication
            && Replica->SourceCardInstanceId == SourceCard);
        const int32 Count = World->GetPersistentCampaign().CollectionState.Instances.Num();
        FGuid Duplicate;
        TestTrue("Exact action replay is idempotent",
            World->ReplicateCard(FGuid(25, 6, 1, 2), SourceCard, Duplicate, Error)
            && Duplicate == NewCard);
        TestEqual("Replay cannot duplicate collection state",
            World->GetPersistentCampaign().CollectionState.Instances.Num(), Count);
        TestFalse("A new action cannot bypass the next cadence",
            World->ReplicateCard(FGuid(25, 6, 1, 3), SourceCard, Duplicate, Error));
        const FString Directory = FPaths::Combine(FPaths::ProjectIntermediateDir(),
            TEXT("ReplicationTests"), FGuid::NewGuid().ToString(EGuidFormats::Digits));
        FDASaveService Saves(Directory);
        TestTrue("Replication provenance and cadence save",
            Saves.SaveCampaign(World->GetPersistentCampaign(), TEXT("replication")).IsSuccess());
        const TResult<FDACampaignSnapshot, FDASaveError> Loaded = Saves.LoadCampaign(TEXT("replication"));
        TestTrue("Replication provenance and cadence reload", Loaded.HasValue());
        if (Loaded.HasValue())
        {
            TestTrue("Reload restores through the sole owner", World->RestorePersistentCampaign(Loaded.GetValue()));
            TestTrue("Reloaded action replay remains idempotent",
                World->ReplicateCard(FGuid(25, 6, 1, 2), SourceCard, Duplicate, Error)
                && Duplicate == NewCard);
        }
        FPlatformFileManager::Get().GetPlatformFile().DeleteDirectoryRecursively(*Directory);
    });

    It("replicates the actual owned non-Legendary Wonder Asset", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        UDASimulationClockSubsystem* Clock = Fixture.GetSubsystem<UDASimulationClockSubsystem>();
        FGuid PrerequisiteCard;
        if (World == nullptr || Clock == nullptr
            || !BuildAscensionPrerequisites(*World, PrerequisiteCard)) return;
        FDACampaignSnapshot Candidate = World->GetPersistentCampaign();
        const FGuid WonderCard(25, 6, 2, 1);
        TestTrue("Actual Thinking Spire Wonder is owned",
            Candidate.CollectionState.AddInstanceWithId(WonderCard,
                TEXT("synara.the_thinking_spire"), EDAAcquisitionSource::WorldDiscovery,
                Candidate.WorldState.CurrentWorldTick)
            && World->RestorePersistentCampaign(Candidate));
        FString Error;
        TestTrue("Ascension commits", World->CompleteFirstAscension(FGuid(25, 6, 2, 2), Error));
        Clock->AdvanceSimulation(UDASimulationClockSubsystem::DevelopmentCycleDurationSeconds * 12.0);
        FGuid ReplicaId;
        TestTrue("Non-Legendary Wonder is eligible for Replication",
            World->ReplicateCard(FGuid(25, 6, 2, 3), WonderCard, ReplicaId, Error));
        const FCardInstance* Replica = World->GetPersistentCampaign().CollectionState.FindInstance(ReplicaId);
        TestTrue("Wonder replica retains actual definition and provenance",
            Replica != nullptr && Replica->DefinitionId == TEXT("synara.the_thinking_spire")
            && Replica->SourceCardInstanceId == WonderCard);
    });

    It("crafts and places the optional Autonomous Factory through the gameplay owner", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        UDAContentRegistrySubsystem* Registry = Fixture.GetSubsystem<UDAContentRegistrySubsystem>();
        UDASimulationClockSubsystem* Clock = Fixture.GetSubsystem<UDASimulationClockSubsystem>();
        UDA_CardDefinition* Factory = Registry != nullptr
            ? Registry->GetCardDefinition(TEXT("fusion.autonomous_factory")) : nullptr;
        FString Error;
        TestNotNull("Canonical Autonomous Factory definition exists", Factory);
        if (World == nullptr || Factory == nullptr || Clock == nullptr) return;
        int32 Authored = 0;
        TestTrue("Factory authors nonzero construction cycles",
            Factory->TryGetConstructionCycles(Authored) && Authored == 8);
        TestTrue("Factory authors high Power demand",
            Factory->TryGetUtilityPower(Authored) && Authored == 24);
        TestTrue("Factory authors high Data demand",
            Factory->TryGetUtilityData(Authored) && Authored == 24);

        FGuid Source;
        if (!BuildAscensionPrerequisites(*World, Source)
            || !World->CompleteFirstAscension(FGuid(25, 7, 2, 1), Error)) return;
        FDACampaignSnapshot Candidate = World->GetPersistentCampaign();
        Candidate.CitySimulationState.Wallet.Capital = 1000.f;
        Candidate.CitySimulationState.Wallet.Insight = 1000.f;
        Candidate.LiveSignals.Capital = Candidate.CitySimulationState.Wallet.Capital;
        Candidate.LiveSignals.Insight = Candidate.CitySimulationState.Wallet.Insight;
        if (!Candidate.WorldAssets.ContainsByPredicate([](const FDAWorldAssetRecord& Asset)
            { return Asset.CardDefinitionId == TEXT("forgeweave.replication_forge")
                && Asset.ConstructionState == EDAConstructionState::Operational; }))
        {
            FDAWorldAssetRecord& Forge = Candidate.WorldAssets.Emplace_GetRef();
            Forge.WorldAssetId = FGuid(25, 7, 2, 2);
            Forge.CardDefinitionId = TEXT("forgeweave.replication_forge");
            Forge.CityId = TEXT("city.ironheart");
            Forge.OwnerCivilizationId = TEXT("civilization.synara");
            Forge.ConstructionState = EDAConstructionState::Operational;
            LinkFixtureAsset(Candidate, Forge, FGuid(25, 7, 2, 3));
        }
        TestTrue("Crafting prerequisites restore through the campaign owner",
            World->RestorePersistentCampaign(Candidate));
        TArray<FGuid> Crafted;
        TestTrue("Unlocked blueprint crafts a concrete owned card instance",
            World->CraftUnlockedBlueprint(TEXT("fusion.autonomous_factory"), 2, Crafted, Error));
        TestEqual("Crafting creates exactly two optional instances", Crafted.Num(), 2);
        if (Crafted.Num() != 2) return;
        for (const FGuid InstanceId : Crafted)
        {
            const FCardInstance* Instance = World->GetPersistentCampaign().CollectionState.FindInstance(InstanceId);
            TestTrue("Crafted Factory instance has canonical acquisition",
                Instance != nullptr && Instance->DefinitionId == Factory->DefinitionId
                && Instance->AcquisitionSource == EDAAcquisitionSource::Crafting);
        }
        FGuid OperationalFactoryId;
        FGuid ConstructingFactoryId;
        TestTrue("First Factory places end to end",
            World->TryPlacePlayerWorldAsset(Crafted[0], TEXT("player_capital"), FIntPoint(0, 0),
                Factory->Footprint, EGridRotation::Zero, OperationalFactoryId, Error));
        TestTrue("Second adjacent Factory places end to end",
            World->TryPlacePlayerWorldAsset(Crafted[1], TEXT("player_capital"), FIntPoint(3, 0),
                Factory->Footprint, EGridRotation::Zero, ConstructingFactoryId, Error));
        Candidate = World->GetPersistentCampaign();
        FDAWorldAssetRecord* Operational = Candidate.FindWorldAssetRecord(OperationalFactoryId);
        if (Operational == nullptr) return;
        Operational->ConstructionCyclesCompleted = Operational->ConstructionCyclesRequired;
        Operational->ConstructionProgressCycles = static_cast<float>(Operational->ConstructionCyclesRequired);
        Operational->ConstructionState = EDAConstructionState::Operational;
        TestTrue("Operational Factory state restores through the owner", World->RestorePersistentCampaign(Candidate));

        FDACampaignJobOpeningSignal Opening;
        Opening.JobId = TEXT("job.autonomous_factory.operators");
        Opening.CityId = TEXT("player_capital");
        Opening.FacilityWorldAssetId = OperationalFactoryId;
        Opening.OpenPositions = 100;
        TestTrue("Gameplay owner publishes the Factory job opening", World->SubmitJobOpeningSignal(Opening));
        const FDACampaignJobOpeningSignal* StoredOpening = World->GetLiveSignals().JobOpenings.FindByPredicate(
            [OperationalFactoryId](const FDACampaignJobOpeningSignal& Row)
            { return Row.FacilityWorldAssetId == OperationalFactoryId; });
        TestTrue("Gameplay jobs owner applies minus eighty percent workforce",
            StoredOpening != nullptr && StoredOpening->OpenPositions == 20);

        FDAWalletValues BaseOutput; BaseOutput.Capital = 100.f;
        FDAFacilityOutput Output;
        TestTrue("Gameplay owner resolves the operational Factory economy",
            World->ResolvePlayerFacilityOutput(OperationalFactoryId, BaseOutput, Output, Error));
        TestEqual("Gameplay economy owner applies plus twenty-five percent throughput",
            Output.GrossOutput.Capital, 125.f, 0.001f);

        Clock->AdvanceSimulation(UDASimulationClockSubsystem::DevelopmentCycleDurationSeconds * 7.0);
        const FDAWorldAssetRecord* Completed =
            World->GetPersistentCampaign().FindWorldAssetRecord(ConstructingFactoryId);
        TestTrue("Gameplay construction/adjacency owner applies plus fifteen percent speed",
            Completed != nullptr && Completed->ConstructionState == EDAConstructionState::Operational
            && Completed->ConstructionCyclesCompleted == Completed->ConstructionCyclesRequired);
    });

    It("publishes deterministic aggregate Factory pressure for multiple factories and same-tick cycles", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        UDASimulationClockSubsystem* Clock = Fixture.GetSubsystem<UDASimulationClockSubsystem>();
        FGuid Source;
        if (World == nullptr || Clock == nullptr || !BuildAscensionPrerequisites(*World, Source)) return;
        FString Error;
        if (!World->CompleteFirstAscension(FGuid(25, 7, 3, 1), Error)) return;
        FDACampaignSnapshot Candidate = World->GetPersistentCampaign();
        const TArray<FGuid> FactoryIds = {FGuid(25, 7, 3, 2), FGuid(25, 7, 3, 3)};
        for (const FGuid FactoryId : FactoryIds)
        {
            FDAWorldAssetRecord& Factory = Candidate.WorldAssets.Emplace_GetRef();
            Factory.WorldAssetId = FactoryId;
            Factory.CardDefinitionId = TEXT("fusion.autonomous_factory");
            Factory.CityId = TEXT("city.ironheart");
            Factory.OwnerCivilizationId = TEXT("civilization.synara");
            Factory.ConstructionState = EDAConstructionState::Operational;
            LinkFixtureAsset(Candidate, Factory,
                FGuid(FactoryId.A ^ 0xDA250001u, FactoryId.B,
                    FactoryId.C, FactoryId.D ^ 0xC4A4D001u));
        }
        TestTrue("Two operational Factories restore through the owner", World->RestorePersistentCampaign(Candidate));
        const double DependencyBefore = Candidate.SynaraState.Dependency;
        const float HungerBefore = Candidate.WorldState.Forgeweave.ResourceHunger;
        Clock->AdvanceSimulation(UDASimulationClockSubsystem::DevelopmentCycleDurationSeconds * 2.0);
        const FDACampaignSnapshot& Published = World->GetPersistentCampaign();
        TestEqual("Clock publishes both requested same-World-Tick cycles",
            Clock->GetCurrentDevelopmentCycle(), 2LL);
        TestEqual("Both cycles publish aggregate audit records",
            Published.AscensionState.FactoryPressureRecords.Num(), 2);
        TestEqual("Two Factories across two cycles add exact Dependency",
            Published.SynaraState.Dependency, DependencyBefore + 0.80, 0.0001);
        TestEqual("Two Factories across two cycles add exact Resource Hunger",
            Published.WorldState.Forgeweave.ResourceHunger,
            FMath::Clamp(HungerBefore + 0.60f, 0.f, 100.f), 0.0001f);
        for (int32 Index = 0; Index < Published.AscensionState.FactoryPressureRecords.Num(); ++Index)
        {
            const FDAAutonomousFactoryPressureRecord& Record =
                Published.AscensionState.FactoryPressureRecords[Index];
            TestEqual("Audit cycle order is stable", Record.DevelopmentCycle, Index + 1LL);
            TestEqual("Audit aggregates both sorted Factory identities",
                Record.FactoryWorldAssetIds, FactoryIds);
            TestEqual("Same-tick audit retains the committed World Tick", Record.WorldTick, 0LL);
        }

        Candidate = Published;
        Candidate.LiveSignals.MutationRevision = MAX_int64;
        TestTrue("Capacity-bound pressure publication state restores for veto proof",
            World->RestorePersistentCampaign(Candidate));
        const int64 CycleBeforeRejectedPublication = Clock->GetCurrentDevelopmentCycle();
        Clock->AdvanceSimulation(UDASimulationClockSubsystem::DevelopmentCycleDurationSeconds);
        TestEqual("Rejected publication does not silently advance the clock",
            Clock->GetCurrentDevelopmentCycle(), CycleBeforeRejectedPublication);
        TestEqual("Rejected publication creates no pressure audit",
            World->GetPersistentCampaign().AscensionState.FactoryPressureRecords.Num(), 2);
    });

    It("save-loads the full Ascension and allows legitimate later owner evolution", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        FGuid Source;
        if (World == nullptr || !BuildAscensionPrerequisites(*World, Source)) return;
        FString Error;
        TestTrue("First Ascension commits", World->CompleteFirstAscension(FGuid(25, 8, 1, 1), Error));
        const FString Directory = FPaths::Combine(FPaths::ProjectIntermediateDir(),
            TEXT("AscensionTests"), FGuid::NewGuid().ToString(EGuidFormats::Digits));
        FDASaveService Saves(Directory);
        TestTrue("Post-Ascension campaign saves",
            Saves.SaveCampaign(World->GetPersistentCampaign(), TEXT("post-ascension")).IsSuccess());
        const TResult<FDACampaignSnapshot, FDASaveError> Loaded = Saves.LoadCampaign(TEXT("post-ascension"));
        TestTrue("Post-Ascension campaign reloads", Loaded.HasValue());
        if (Loaded.HasValue())
        {
            TestTrue("Reload retains Relic, cards, doctrine, Leader, Fusion, Hall and 1/20",
                Loaded.GetValue().AscensionState.RelicIds.Contains(TEXT("relic.forge"))
                && Loaded.GetValue().AscensionState.UnlockedForgeweaveDefinitionIds.Num() == 15
                && Loaded.GetValue().AscensionState.bReplicationUnlocked
                && Loaded.GetValue().DaxtonState.bLeaderResolved
                && Loaded.GetValue().AscensionState.UnlockedBlueprintIds.Contains(TEXT("fusion.autonomous_factory"))
                && Loaded.GetValue().AscensionState.FounderHallVisualState == 3
                && Loaded.GetValue().AscensionState.FounderHallRelicPositions.Num() == 20
                && Loaded.GetValue().AscensionState.FounderHallRelicPositions[0].bActive
                && Loaded.GetValue().AscensionState.FounderHallRelicPositions[0].bOccupied
                && Loaded.GetValue().AscensionState.FounderHallRelicPositions[0].RelicId == TEXT("relic.forge")
                && Loaded.GetValue().AscensionState.FounderHallRelicPositions.FilterByPredicate(
                    [](const FDAFounderHallRelicPosition& Position)
                    { return Position.bActive || Position.bOccupied || !Position.RelicId.IsNone(); }).Num() == 1
                && Loaded.GetValue().AscensionState.ConvergenceAuthority == 1);
            TestTrue("Reload restores through sole owner", World->RestorePersistentCampaign(Loaded.GetValue()));
            TestTrue("Legitimate later world evolution remains valid", World->AdvanceWorldTicks(1));
        }
        FPlatformFileManager::Get().GetPlatformFile().DeleteDirectoryRecursively(*Directory);
    });
}
