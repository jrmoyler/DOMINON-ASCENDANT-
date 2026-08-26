#include "Ascension/DAAscensionSystem.h"

#include "Adjacency/DAAdjacencySubsystem.h"
#include "Campaign/DAConquestCampaignState.h"
#include "Campaign/DADaxtonCampaignState.h"
#include "Cards/DACollectionState.h"
#include "Content/DACardDefinition.h"
#include "Content/DAContentRegistrySubsystem.h"
#include "Narrative/DANarrativeRecords.h"

namespace
{
    constexpr double AscensionInfluenceReward = 10.0;
    constexpr double AscensionInsightReward = 10.0;
    constexpr double FactoryDependencyPerCycle = 0.20;
    constexpr float FactoryResourceHungerPerCycle = 0.15f;

    void AddHistory(FDACampaignSnapshot& Campaign, const FName Tag)
    {
        Campaign.HistoryTags.AddUnique(Tag);
        Campaign.HistoryTags.Sort([](const FName Left, const FName Right)
        { return Left.LexicalLess(Right); });
    }

    void BuildCompletedConvergenceQuest(FDACampaignSnapshot& Campaign, const int64 WorldTick)
    {
        FDAQuestSaveState& Quest = Campaign.NarrativeState.QuestStates.Emplace_GetRef();
        Quest.QuestId = TEXT("quest.convergence_authority");
        Quest.DefinitionManifest.QuestId = Quest.QuestId;
        Quest.DefinitionManifest.SourceDefinitionId = TEXT("quest.convergence_authority.v1");
        Quest.DefinitionManifest.StartNodeId = TEXT("ascension_committed");
        FDAQuestNodeDefinition& Start = Quest.DefinitionManifest.Nodes.Emplace_GetRef();
        Start.NodeId = TEXT("ascension_committed");
        Start.Type = EDAQuestNodeType::Start;
        Start.SourceDefinitionId = TEXT("quest.convergence_authority.v1.start");
        FDAQuestEdgeDefinition& Edge = Start.Edges.Emplace_GetRef();
        Edge.BranchTag = TEXT("first_relic_acquired");
        Edge.TargetNodeId = TEXT("resolution.convergence_authority_1_of_20");
        FDAQuestNodeDefinition& Resolution = Quest.DefinitionManifest.Nodes.Emplace_GetRef();
        Resolution.NodeId = TEXT("resolution.convergence_authority_1_of_20");
        Resolution.Type = EDAQuestNodeType::Resolution;
        Resolution.SourceDefinitionId = TEXT("quest.convergence_authority.v1.resolution");
        Quest.DefinitionManifest.RefreshFingerprint();
        Quest.DefinitionVersion = 1;
        Quest.CurrentNodeId = Resolution.NodeId;
        Quest.ProgressState = EDAQuestProgressState::Completed;
        Quest.StartedWorldTick = WorldTick;
        Quest.LastTransitionWorldTick = WorldTick;
        Quest.CurrentNodeEnteredWorldTick = WorldTick;
        Quest.CompletedNodeIds = {Start.NodeId};
        FDAQuestNodeTransitionRecord& Transition = Quest.NodeTransitionRecords.Emplace_GetRef();
        Transition.CompletedNodeId = Start.NodeId;
        Transition.EnteredNodeId = Resolution.NodeId;
        Transition.WorldTick = WorldTick;
    }

    bool IsEligibleAsset(const UDA_CardDefinition& Definition)
    {
        return Definition.bPlaceable
            && Definition.CardType != EDACardType::Leader
            && Definition.CardType != EDACardType::Support
            && Definition.CardType != EDACardType::Special
            && Definition.CardType != EDACardType::Unit
            && Definition.Rarity != EDARarity::Legendary;
    }
}

bool FDAAscensionSystem::ApplyFirstAscension(const FGuid ActionId,
    FDACampaignSnapshot& InOutCampaign, FString& OutError)
{
    if (InOutCampaign.AscensionState.bForgeweaveAscended)
    {
        if (ActionId.IsValid()
            && InOutCampaign.AscensionState.FirstAscension.ActionId == ActionId)
        {
            OutError.Reset();
            return true;
        }
        OutError = TEXT("First Ascension was already committed by another action.");
        return false;
    }
    if (!ActionId.IsValid() || FDAAscensionAuthority::IsActionIdInUse(InOutCampaign, ActionId))
    {
        OutError = TEXT("First Ascension requires a globally unused valid action id.");
        return false;
    }
    if (!FDAConquestAuthorityValidator::ValidateResolvedRoute(InOutCampaign, OutError)
        || !FDADaxtonAuthorityValidator::ValidateCampaignState(InOutCampaign, OutError)
        || !InOutCampaign.DaxtonState.bLeaderResolved)
    {
        return false;
    }
    if (InOutCampaign.LiveSignals.MutationRevision == MAX_int64
        || InOutCampaign.NarrativeState.MutationRevision == MAX_int64
        || !FMath::IsFinite(InOutCampaign.LiveSignals.Influence)
        || !FMath::IsFinite(InOutCampaign.LiveSignals.Insight)
        || InOutCampaign.LiveSignals.Influence
            > TNumericLimits<double>::Max() - AscensionInfluenceReward
        || InOutCampaign.LiveSignals.Insight
            > TNumericLimits<double>::Max() - AscensionInsightReward)
    {
        OutError = TEXT("First Ascension reward or revision capacity is exhausted.");
        return false;
    }

    const int64 Cycle = InOutCampaign.WorldState.ClockAuthority.bCaptured
        ? InOutCampaign.WorldState.ClockAuthority.CurrentDevelopmentCycle
        : InOutCampaign.LiveSignals.ResolvedDevelopmentCycles;
    if (Cycle < 0 || Cycle > MAX_int64 - FDAAscensionCampaignState::ReplicationCadenceDevelopmentCycles)
    {
        OutError = TEXT("First Ascension cannot establish a safe Replication cadence.");
        return false;
    }

    FDACampaignSnapshot Candidate = InOutCampaign;
    FDAAscensionCampaignState& State = Candidate.AscensionState;
    State.bForgeweaveAscended = true;
    State.FirstAscension.ActionId = ActionId;
    State.FirstAscension.ConquestResolutionActionId = Candidate.ConquestState.ResolutionActionId;
    State.FirstAscension.DaxtonResolutionActionId = Candidate.DaxtonState.ResolutionActionId;
    State.FirstAscension.LeaderState = Candidate.DaxtonState.LeaderState;
    State.FirstAscension.WorldTick = Candidate.WorldState.CurrentWorldTick;
    State.FirstAscension.DevelopmentCycle = Cycle;
    State.FirstAscension.InfluenceBefore = Candidate.CitySimulationState.Wallet.Influence;
    State.FirstAscension.InsightBefore = Candidate.CitySimulationState.Wallet.Insight;
    Candidate.CitySimulationState.Wallet.Influence += AscensionInfluenceReward;
    Candidate.CitySimulationState.Wallet.Insight += AscensionInsightReward;
    Candidate.LiveSignals.Influence = Candidate.CitySimulationState.Wallet.Influence;
    Candidate.LiveSignals.Insight = Candidate.CitySimulationState.Wallet.Insight;
    State.FirstAscension.InfluenceAfter = Candidate.CitySimulationState.Wallet.Influence;
    State.FirstAscension.InsightAfter = Candidate.CitySimulationState.Wallet.Insight;
    ++Candidate.LiveSignals.MutationRevision;

    State.RelicIds = {TEXT("relic.forge")};
    State.UnlockedForgeweaveDefinitionIds = FDAAscensionAuthority::ForgeweaveDefinitionIds();
    State.bReplicationUnlocked = true;
    State.NextReplicationEligibleCycle = Cycle
        + FDAAscensionCampaignState::ReplicationCadenceDevelopmentCycles;
    State.bFusionEligible = true;
    State.UnlockedBlueprintIds = {TEXT("fusion.autonomous_factory")};
    State.FounderHallVisualState = 3;
    State.FounderHallRelicPositions.Reserve(FDAAscensionCampaignState::ConvergenceMaximum);
    for (int32 SlotIndex = 1;
        SlotIndex <= FDAAscensionCampaignState::ConvergenceMaximum; ++SlotIndex)
    {
        FDAFounderHallRelicPosition& Position =
            State.FounderHallRelicPositions.Emplace_GetRef();
        Position.SlotIndex = SlotIndex;
        Position.bActive = SlotIndex == 1;
        Position.bOccupied = SlotIndex == 1;
        Position.RelicId = SlotIndex == 1 ? FName(TEXT("relic.forge")) : NAME_None;
    }
    State.ActiveRelicSlotCount = 1;
    State.bHiddenChamberOpen = true;
    State.ConvergenceAuthority = 1;
    State.ConvergenceAuthorityMaximum = FDAAscensionCampaignState::ConvergenceMaximum;
    AddHistory(Candidate, TEXT("first_relic_acquired"));
    AddHistory(Candidate, TEXT("convergence_authority_1_of_20"));
    BuildCompletedConvergenceQuest(Candidate, Candidate.WorldState.CurrentWorldTick);
    ++Candidate.NarrativeState.MutationRevision;

    if (!Candidate.Validate(OutError)) return false;
    InOutCampaign = MoveTemp(Candidate);
    OutError.Reset();
    return true;
}

bool FDAAscensionSystem::ReplicateCard(const FGuid ActionId, const FGuid SourceCardInstanceId,
    const UDAContentRegistrySubsystem& Registry, const int64 DevelopmentCycle,
    FDACampaignSnapshot& InOutCampaign, FGuid& OutReplicatedCardInstanceId, FString& OutError)
{
    OutReplicatedCardInstanceId.Invalidate();
    if (const FDAReplicationRecord* Existing = InOutCampaign.AscensionState.FindReplication(ActionId))
    {
        if (Existing->SourceCardInstanceId != SourceCardInstanceId)
        {
            OutError = TEXT("Replication action replay changed its source instance.");
            return false;
        }
        OutReplicatedCardInstanceId = Existing->ReplicatedCardInstanceId;
        OutError.Reset();
        return true;
    }
    const FCardInstance* Source = InOutCampaign.CollectionState.FindInstance(SourceCardInstanceId);
    const UDA_CardDefinition* Definition = Source == nullptr
        ? nullptr : Registry.GetCardDefinition(Source->DefinitionId);
    const FDAAscensionCampaignState& Authority = InOutCampaign.AscensionState;
    if (!Authority.bForgeweaveAscended || !Authority.bReplicationUnlocked
        || !ActionId.IsValid() || FDAAscensionAuthority::IsActionIdInUse(InOutCampaign, ActionId)
        || Source == nullptr || Definition == nullptr || !IsEligibleAsset(*Definition)
        || DevelopmentCycle < Authority.NextReplicationEligibleCycle
        || DevelopmentCycle > MAX_int64 - FDAAscensionCampaignState::ReplicationCadenceDevelopmentCycles)
    {
        OutError = TEXT("Replication requires its committed doctrine cadence and an owned non-Legendary Asset Card.");
        return false;
    }

    FDACampaignSnapshot Candidate = InOutCampaign;
    const FGuid ReplicaId = FDAAscensionAuthority::MakeReplicationInstanceId(ActionId);
    if (!Candidate.CollectionState.AddInstanceWithId(ReplicaId, Source->DefinitionId,
        EDAAcquisitionSource::Replication, Candidate.WorldState.CurrentWorldTick))
    {
        OutError = TEXT("Replication could not create its deterministic card instance.");
        return false;
    }
    Candidate.CollectionState.FindInstance(ReplicaId)->SourceCardInstanceId = SourceCardInstanceId;
    FDAReplicationRecord& Record = Candidate.AscensionState.ReplicationRecords.Emplace_GetRef();
    Record.ActionId = ActionId;
    Record.SourceCardInstanceId = SourceCardInstanceId;
    Record.ReplicatedCardInstanceId = ReplicaId;
    Record.DefinitionId = Source->DefinitionId;
    Record.DevelopmentCycle = DevelopmentCycle;
    Record.WorldTick = Candidate.WorldState.CurrentWorldTick;
    Candidate.AscensionState.NextReplicationEligibleCycle = DevelopmentCycle
        + FDAAscensionCampaignState::ReplicationCadenceDevelopmentCycles;
    if (!Candidate.Validate(OutError)) return false;
    InOutCampaign = MoveTemp(Candidate);
    OutReplicatedCardInstanceId = ReplicaId;
    OutError.Reset();
    return true;
}

bool FDAAscensionSystem::ApplyAutonomousFactoryDevelopmentCycle(
    const TArray<FGuid>& FactoryWorldAssetIds, const UDA_CardDefinition& Definition,
    const int64 DevelopmentCycle, FDACampaignSnapshot& InOutCampaign, FString& OutError)
{
    float DependencyDelta = 0.f;
    float HungerDelta = 0.f;
    const FName ActionId(*FString::Printf(TEXT("factory.pressure.cycle.%lld"),
        DevelopmentCycle));
    if (const FDAAutonomousFactoryPressureRecord* Existing =
        InOutCampaign.AscensionState.FactoryPressureRecords.FindByPredicate(
        [ActionId](const FDAAutonomousFactoryPressureRecord& Record)
        { return Record.ActionId == ActionId; }))
    {
        if (Existing->FactoryWorldAssetIds != FactoryWorldAssetIds)
        {
            OutError = TEXT("Autonomous Factory pressure replay changed the aggregated Factory set.");
            return false;
        }
        OutError.Reset();
        return true;
    }
    bool bFactoriesExact = !FactoryWorldAssetIds.IsEmpty();
    FGuid PreviousFactoryId;
    for (const FGuid FactoryWorldAssetId : FactoryWorldAssetIds)
    {
        const FDAWorldAssetRecord* FactoryAsset = InOutCampaign.FindWorldAssetRecord(
            FactoryWorldAssetId);
        if (!FactoryWorldAssetId.IsValid()
            || (PreviousFactoryId.IsValid()
                && FactoryWorldAssetId.ToString().Compare(PreviousFactoryId.ToString()) <= 0)
            || FactoryAsset == nullptr
            || FactoryAsset->CardDefinitionId != TEXT("fusion.autonomous_factory")
            || FactoryAsset->ConstructionState != EDAConstructionState::Operational)
        {
            bFactoriesExact = false;
            break;
        }
        PreviousFactoryId = FactoryWorldAssetId;
    }
    if (!InOutCampaign.AscensionState.bForgeweaveAscended
        || !InOutCampaign.AscensionState.UnlockedBlueprintIds.Contains(TEXT("fusion.autonomous_factory"))
        || !bFactoriesExact || DevelopmentCycle < 0
        || Definition.DefinitionId != TEXT("fusion.autonomous_factory")
        || !Definition.TryGetSynaraDependencyPerCycle(DependencyDelta)
        || !Definition.TryGetForgeweaveResourceHungerPerCycle(HungerDelta)
        || !FMath::IsNearlyEqual(DependencyDelta, static_cast<float>(FactoryDependencyPerCycle), 0.0001f)
        || !FMath::IsNearlyEqual(HungerDelta, FactoryResourceHungerPerCycle, 0.0001f))
    {
        OutError = TEXT("Autonomous Factory pressure requires its exact unlocked authored definition.");
        return false;
    }

    FDACampaignSnapshot Candidate = InOutCampaign;
    FDAAutonomousFactoryPressureRecord& Record = Candidate.AscensionState.FactoryPressureRecords.Emplace_GetRef();
    Record.ActionId = ActionId;
    Record.FactoryWorldAssetIds = FactoryWorldAssetIds;
    Record.DevelopmentCycle = DevelopmentCycle;
    Record.WorldTick = Candidate.WorldState.CurrentWorldTick;
    Record.DependencyBefore = Candidate.SynaraState.Dependency;
    Record.ResourceHungerBefore = Candidate.WorldState.Forgeweave.ResourceHunger;
    const double AggregateDependencyDelta = FactoryDependencyPerCycle
        * static_cast<double>(FactoryWorldAssetIds.Num());
    const float AggregateHungerDelta = FactoryResourceHungerPerCycle
        * static_cast<float>(FactoryWorldAssetIds.Num());
    if (!Candidate.SynaraState.ApplyDependencyReason(ActionId,
        AggregateDependencyDelta, Candidate.WorldState.CurrentWorldTick))
    {
        OutError = TEXT("Canonical systemic-pressure authority rejected Autonomous Factory Dependency.");
        return false;
    }
    Record.DependencyAfter = Candidate.SynaraState.Dependency;
    Candidate.WorldState.Forgeweave.ResourceHunger = FMath::Clamp(
        Candidate.WorldState.Forgeweave.ResourceHunger + AggregateHungerDelta, 0.f, 100.f);
    Record.ResourceHungerAfter = Candidate.WorldState.Forgeweave.ResourceHunger;
    if (!Candidate.Validate(OutError)) return false;
    InOutCampaign = MoveTemp(Candidate);
    OutError.Reset();
    return true;
}

bool FDAAscensionSystem::ConfigureAutonomousFactoryAdjacency(
    FDAAdjacencySubsystem& Adjacency, const UDA_CardDefinition& Definition)
{
    float Amount = 0.f;
    if (Definition.DefinitionId != TEXT("fusion.autonomous_factory")
        || !Definition.TryGetAdjacentIndustrialConstructionSpeedModifier(Amount)
        || !FMath::IsNearlyEqual(Amount, 0.15f, 0.0001f)) return false;
    Adjacency.AddTypeRule(EDACardType::Industrial, Definition.DefinitionId,
        TEXT("AutonomousFactory.AdjacentIndustrialConstructionSpeed"), Amount);
    return true;
}

FDAAscensionPresentationState FDAAscensionSystem::BuildPresentationState(
    const FDACampaignSnapshot& CommittedCampaign)
{
    FDAAscensionPresentationState Result;
    Result.bAscended = CommittedCampaign.AscensionState.bForgeweaveAscended;
    if (!Result.bAscended) return Result;
    Result.FounderHallVisualState = CommittedCampaign.AscensionState.FounderHallVisualState;
    Result.RelicPositions = CommittedCampaign.AscensionState.FounderHallRelicPositions;
    Result.ActiveRelicSlotCount = CommittedCampaign.AscensionState.ActiveRelicSlotCount;
    Result.bHiddenChamberOpen = CommittedCampaign.AscensionState.bHiddenChamberOpen;
    Result.bShouldPlayCinematic = true;
    Result.CinematicSequenceAsset = FSoftObjectPath(
        TEXT("/Game/Cinematics/CS_ForgeweaveAscension.CS_ForgeweaveAscension"));
    Result.OrderedBeats = {
        EDAAscensionPresentationBeat::SystemsHaltAndReact,
        EDAAscensionPresentationBeat::ForgeRelicEmerges,
        EDAAscensionPresentationBeat::WorldTransit,
        EDAAscensionPresentationBeat::FounderHallReceivesRelic,
        EDAAscensionPresentationBeat::Unlocks};
    Result.ConvergenceAuthorityLabel = FString::Printf(TEXT("CONVERGENCE AUTHORITY: %d/%d"),
        CommittedCampaign.AscensionState.ConvergenceAuthority,
        CommittedCampaign.AscensionState.ConvergenceAuthorityMaximum);
    return Result;
}
