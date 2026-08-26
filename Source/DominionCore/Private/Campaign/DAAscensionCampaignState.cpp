#include "Campaign/DAAscensionCampaignState.h"

#include "Campaign/DAConquestCampaignState.h"
#include "Cards/DACardInstance.h"
#include "Narrative/DANarrativeRecords.h"
#include "Save/DACampaignSaveGame.h"

namespace
{
    bool NamesExactAndSorted(const TArray<FName>& Actual, const TArray<FName>& Expected)
    {
        if (Actual != Expected) return false;
        for (int32 Index = 0; Index < Actual.Num(); ++Index)
            if (Actual[Index].IsNone() || (Index > 0 && !Actual[Index - 1].LexicalLess(Actual[Index]))) return false;
        return true;
    }

    bool IsAscensionQuestExact(const FDACampaignSnapshot& Campaign, const int64 WorldTick)
    {
        const FDAQuestSaveState* Quest = Campaign.NarrativeState.FindQuestState(
            TEXT("quest.convergence_authority"));
        return Quest != nullptr
            && Quest->ProgressState == EDAQuestProgressState::Completed
            && Quest->DefinitionVersion == 1
            && Quest->DefinitionManifest.SourceDefinitionId == TEXT("quest.convergence_authority.v1")
            && Quest->DefinitionManifest.StartNodeId == TEXT("ascension_committed")
            && Quest->CompletedNodeIds == TArray<FName>{TEXT("ascension_committed")}
            && Quest->CurrentNodeId == TEXT("resolution.convergence_authority_1_of_20")
            && Quest->LastTransitionWorldTick == WorldTick;
    }

    bool AreFounderHallRelicPositionsExact(
        const TArray<FDAFounderHallRelicPosition>& Positions)
    {
        if (Positions.Num() != FDAAscensionCampaignState::ConvergenceMaximum) return false;
        for (int32 Index = 0; Index < Positions.Num(); ++Index)
        {
            const FDAFounderHallRelicPosition& Position = Positions[Index];
            const bool bFirst = Index == 0;
            if (Position.SlotIndex != Index + 1
                || Position.bActive != bFirst
                || Position.bOccupied != bFirst
                || Position.RelicId != (bFirst ? FName(TEXT("relic.forge")) : NAME_None))
                return false;
        }
        return true;
    }
}

const TArray<FName>& FDAAscensionAuthority::ForgeweaveDefinitionIds()
{
    static const TArray<FName> Values = {
        TEXT("forgeweave.forge_guard"),
        TEXT("forgeweave.forge_lord_daxton_rhe"),
        TEXT("forgeweave.forge_quarters"),
        TEXT("forgeweave.freight_furnace"),
        TEXT("forgeweave.industrial_design_bureau"),
        TEXT("forgeweave.industrial_exchange"),
        TEXT("forgeweave.infinite_foundry"),
        TEXT("forgeweave.machine_parts_market"),
        TEXT("forgeweave.mechanist_crew"),
        TEXT("forgeweave.production_directorate"),
        TEXT("forgeweave.replication_forge"),
        TEXT("forgeweave.smog_reclaimer"),
        TEXT("forgeweave.the_grand_forge"),
        TEXT("forgeweave.worker_arcology"),
        TEXT("forgeweave.workers_canteen")};
    return Values;
}

FGuid FDAAscensionAuthority::MakeReplicationInstanceId(const FGuid ActionId)
{
    return FGuid(ActionId.A ^ 0x5245504Cu, ActionId.B ^ 0x49434154u,
        ActionId.C ^ 0x494F4E00u, ActionId.D ^ 0xDA250001u);
}

bool FDAAscensionAuthority::IsActionIdInUse(const FDACampaignSnapshot& Campaign,
    const FGuid ActionId)
{
    if (!ActionId.IsValid()) return true;
    if (Campaign.ConquestState.ResolutionActionId == ActionId
        || Campaign.DaxtonState.StartActionId == ActionId
        || Campaign.DaxtonState.PhaseOneObjectiveActionId == ActionId
        || Campaign.DaxtonState.PhaseThreeActionId == ActionId
        || Campaign.DaxtonState.ResolutionActionId == ActionId
        || Campaign.AscensionState.FirstAscension.ActionId == ActionId
        || Campaign.NarrativeState.FindActionRecord(ActionId) != nullptr) return true;
    if (Campaign.DaxtonState.PhaseOneActionIds.Contains(ActionId)
        || Campaign.DaxtonState.ObjectiveActionIds.Contains(ActionId)
        || Campaign.DaxtonState.InteractionRecords.ContainsByPredicate(
            [ActionId](const FDADaxtonInteractionRecord& Record)
            { return Record.ActionId == ActionId; })
        || Campaign.DaxtonState.CanonicalActionRecords.ContainsByPredicate(
            [ActionId](const FDADaxtonCanonicalActionRecord& Record)
            { return Record.ActionId == ActionId; })
        || Campaign.AscensionState.ReplicationRecords.ContainsByPredicate(
            [ActionId](const FDAReplicationRecord& Record)
            { return Record.ActionId == ActionId; })
        || Campaign.RegionalCrisis.ResolutionRecords.ContainsByPredicate(
            [ActionId](const FDAFoundryShortageResolutionRecord& Record)
            { return Record.ActionId == ActionId; })) return true;
    if (Campaign.NarrativeState.PromiseRecords.ContainsByPredicate(
            [ActionId](const FDAPromiseRecord& Record)
            { return Record.ResolutionActionId == ActionId; })
        || Campaign.NarrativeState.CitizenStoryTransitionRecords.ContainsByPredicate(
            [ActionId](const FDACitizenStoryTransitionRecord& Record)
            { return Record.SourceActionId == ActionId; })
        || Campaign.NarrativeState.AuditEligibilitySourceRecords.ContainsByPredicate(
            [ActionId](const FDAAuditEligibilitySourceRecord& Record)
            { return Record.SourceActionId == ActionId; })
        || Campaign.NarrativeState.QuestEligibilityProofRecords.ContainsByPredicate(
            [ActionId](const FDAQuestEligibilityProofRecord& Record)
            { return Record.SourceActionId == ActionId; })
        || Campaign.NarrativeState.QuestCrisisCompletionRecords.ContainsByPredicate(
            [ActionId](const FDAQuestCrisisCompletionRecord& Record)
            { return Record.NarrativeActionId == ActionId; })) return true;
    return false;
}

const FDAReplicationRecord* FDAAscensionCampaignState::FindReplication(
    const FGuid ActionId) const
{
    return ReplicationRecords.FindByPredicate([ActionId](const FDAReplicationRecord& Record)
    { return Record.ActionId == ActionId; });
}

bool FDAAscensionCampaignState::Validate(const FDACampaignSnapshot& Campaign,
    FString& OutError) const
{
    if (!bForgeweaveAscended)
    {
        if (FirstAscension.ActionId.IsValid() || !RelicIds.IsEmpty()
            || !UnlockedForgeweaveDefinitionIds.IsEmpty() || bReplicationUnlocked
            || NextReplicationEligibleCycle != 0 || !ReplicationRecords.IsEmpty()
            || bFusionEligible || !UnlockedBlueprintIds.IsEmpty()
            || FounderHallVisualState != 0 || !FounderHallRelicPositions.IsEmpty()
            || ActiveRelicSlotCount != 0
            || bHiddenChamberOpen || ConvergenceAuthority != 0
            || ConvergenceAuthorityMaximum != ConvergenceMaximum
            || !FactoryPressureRecords.IsEmpty())
        {
            OutError = TEXT("Inactive Ascension authority cannot contain future reward state.");
            return false;
        }
        return true;
    }

    if (!FirstAscension.ActionId.IsValid() || FirstAscension.WorldTick < 0
        || FirstAscension.DevelopmentCycle < 0
        || !FMath::IsFinite(FirstAscension.InfluenceBefore)
        || !FMath::IsFinite(FirstAscension.InfluenceAfter)
        || !FMath::IsFinite(FirstAscension.InsightBefore)
        || !FMath::IsFinite(FirstAscension.InsightAfter)
        || FirstAscension.InfluenceAfter != FirstAscension.InfluenceBefore + 10.0
        || FirstAscension.InsightAfter != FirstAscension.InsightBefore + 10.0
        || !Campaign.ConquestState.bForgeweaveResolved
        || Campaign.ConquestState.ResolutionActionId != FirstAscension.ConquestResolutionActionId
        || Campaign.ConquestState.ResolvedWorldTick > FirstAscension.WorldTick
        || !Campaign.DaxtonState.bLeaderResolved
        || Campaign.DaxtonState.ResolutionActionId != FirstAscension.DaxtonResolutionActionId
        || Campaign.DaxtonState.LeaderState != FirstAscension.LeaderState
        || Campaign.DaxtonState.ResolvedWorldTick > FirstAscension.WorldTick)
    {
        OutError = TEXT("First Ascension requires exact conquest, Leader, time, and v0.8 reward provenance.");
        return false;
    }
    FDACampaignSnapshot PriorCampaign = Campaign;
    PriorCampaign.AscensionState = FDAAscensionCampaignState{};
    if (FDAAscensionAuthority::IsActionIdInUse(PriorCampaign, FirstAscension.ActionId))
    {
        OutError = TEXT("First Ascension action id collides with an earlier campaign authority.");
        return false;
    }
    if (!NamesExactAndSorted(RelicIds, {TEXT("relic.forge")})
        || !NamesExactAndSorted(UnlockedForgeweaveDefinitionIds,
            FDAAscensionAuthority::ForgeweaveDefinitionIds())
        || !bReplicationUnlocked || !bFusionEligible
        || !NamesExactAndSorted(UnlockedBlueprintIds, {TEXT("fusion.autonomous_factory")})
        || FounderHallVisualState != 3
        || !AreFounderHallRelicPositionsExact(FounderHallRelicPositions)
        || ActiveRelicSlotCount != 1
        || !bHiddenChamberOpen || ConvergenceAuthority != 1
        || ConvergenceAuthorityMaximum != ConvergenceMaximum
        || !Campaign.HistoryTags.Contains(TEXT("first_relic_acquired"))
        || !Campaign.HistoryTags.Contains(TEXT("convergence_authority_1_of_20"))
        || !IsAscensionQuestExact(Campaign, FirstAscension.WorldTick))
    {
        OutError = TEXT("First Ascension reward, Founder Hall, history, or quest authority is incomplete.");
        return false;
    }

    int64 ExpectedNext = FirstAscension.DevelopmentCycle;
    if (ExpectedNext > MAX_int64 - ReplicationCadenceDevelopmentCycles)
    {
        OutError = TEXT("Replication cadence cannot be represented without overflow.");
        return false;
    }
    ExpectedNext += ReplicationCadenceDevelopmentCycles;
    TSet<FGuid> Actions;
    TSet<FGuid> Replicas;
    int64 PreviousCycle = INDEX_NONE;
    for (const FDAReplicationRecord& Record : ReplicationRecords)
    {
        const FCardInstance* Source = Campaign.CollectionState.FindInstance(Record.SourceCardInstanceId);
        const FCardInstance* Replica = Campaign.CollectionState.FindInstance(Record.ReplicatedCardInstanceId);
        if (!Record.ActionId.IsValid() || Actions.Contains(Record.ActionId)
            || Record.ActionId == FirstAscension.ActionId
            || FDAAscensionAuthority::IsActionIdInUse(PriorCampaign, Record.ActionId)
            || !Record.SourceCardInstanceId.IsValid() || !Record.ReplicatedCardInstanceId.IsValid()
            || Replicas.Contains(Record.ReplicatedCardInstanceId)
            || Record.ReplicatedCardInstanceId != FDAAscensionAuthority::MakeReplicationInstanceId(Record.ActionId)
            || Record.DefinitionId.IsNone() || Source == nullptr || Replica == nullptr
            || Source->DefinitionId != Record.DefinitionId || Replica->DefinitionId != Record.DefinitionId
            || Replica->AcquisitionSource != EDAAcquisitionSource::Replication
            || Replica->SourceCardInstanceId != Record.SourceCardInstanceId
            || Replica->AcquisitionWorldTick != Record.WorldTick
            || Record.DevelopmentCycle < ExpectedNext
            || (PreviousCycle != INDEX_NONE
                && Record.DevelopmentCycle - PreviousCycle < ReplicationCadenceDevelopmentCycles))
        {
            OutError = TEXT("Replication records require unique cadence-safe actions and exact instance provenance.");
            return false;
        }
        Actions.Add(Record.ActionId);
        Replicas.Add(Record.ReplicatedCardInstanceId);
        PreviousCycle = Record.DevelopmentCycle;
        if (Record.DevelopmentCycle > MAX_int64 - ReplicationCadenceDevelopmentCycles)
        {
            OutError = TEXT("Replication cadence overflow is invalid.");
            return false;
        }
        ExpectedNext = Record.DevelopmentCycle + ReplicationCadenceDevelopmentCycles;
    }
    if (NextReplicationEligibleCycle != ExpectedNext)
    {
        OutError = TEXT("Replication next-cycle authority does not match its exact cadence ledger.");
        return false;
    }

    TSet<FName> PressureActions;
    int64 PreviousPressureCycle = INDEX_NONE;
    for (const FDAAutonomousFactoryPressureRecord& Record : FactoryPressureRecords)
    {
        const FDASynaraValueReason* DependencyReason = Campaign.SynaraState.DependencyReasons.FindByPredicate(
            [&Record](const FDASynaraValueReason& Reason)
            { return Reason.ActionId == Record.ActionId; });
        bool bFactoriesExact = !Record.FactoryWorldAssetIds.IsEmpty();
        FGuid PreviousFactoryId;
        for (const FGuid FactoryWorldAssetId : Record.FactoryWorldAssetIds)
        {
            const FDAWorldAssetRecord* Factory = Campaign.FindWorldAssetRecord(FactoryWorldAssetId);
            if (!FactoryWorldAssetId.IsValid()
                || (PreviousFactoryId.IsValid()
                    && FactoryWorldAssetId.ToString().Compare(PreviousFactoryId.ToString()) <= 0)
                || Factory == nullptr
                || Factory->CardDefinitionId != TEXT("fusion.autonomous_factory"))
            {
                bFactoriesExact = false;
                break;
            }
            PreviousFactoryId = FactoryWorldAssetId;
        }
        const double DependencyDelta = 0.20
            * static_cast<double>(Record.FactoryWorldAssetIds.Num());
        const float HungerDelta = 0.15f
            * static_cast<float>(Record.FactoryWorldAssetIds.Num());
        if (Record.ActionId.IsNone() || PressureActions.Contains(Record.ActionId)
            || !bFactoriesExact || Record.DevelopmentCycle < 0
            || Record.WorldTick < 0 || !FMath::IsFinite(Record.DependencyBefore)
            || !FMath::IsFinite(Record.DependencyAfter)
            || !FMath::IsFinite(Record.ResourceHungerBefore)
            || !FMath::IsFinite(Record.ResourceHungerAfter)
            || (PreviousPressureCycle != INDEX_NONE
                && Record.DevelopmentCycle <= PreviousPressureCycle)
            || Record.DependencyAfter != FMath::Clamp(
                Record.DependencyBefore + DependencyDelta, 0.0, 100.0)
            || !FMath::IsNearlyEqual(Record.ResourceHungerAfter,
                FMath::Clamp(Record.ResourceHungerBefore + HungerDelta, 0.f, 100.f), 0.0001f)
            || DependencyReason == nullptr || DependencyReason->Baseline != Record.DependencyBefore
            || DependencyReason->Delta != DependencyDelta
            || DependencyReason->Result != Record.DependencyAfter
            || DependencyReason->WorldTick != Record.WorldTick)
        {
            OutError = TEXT("Autonomous Factory pressure records must reconcile with canonical pressure authorities.");
            return false;
        }
        PressureActions.Add(Record.ActionId);
        PreviousPressureCycle = Record.DevelopmentCycle;
    }
    return true;
}
