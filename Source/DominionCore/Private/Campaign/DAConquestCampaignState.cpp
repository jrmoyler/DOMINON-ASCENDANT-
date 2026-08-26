#include "Campaign/DAConquestCampaignState.h"

#include "Save/DACampaignSaveGame.h"

namespace
{
    bool IsBounded(const double Value)
    {
        return FMath::IsFinite(Value) && Value >= 0.0 && Value <= 100.0;
    }

    double ApplyMeter(const double Value, const double Delta)
    {
        return FMath::Clamp(Value + Delta, 0.0, 100.0);
    }

    bool IsCompletedQuest(const FDACampaignSnapshot& Campaign, const FName QuestId)
    {
        const FDAQuestSaveState* Quest = Campaign.NarrativeState.FindQuestState(QuestId);
        return Quest != nullptr && Quest->ProgressState == EDAQuestProgressState::Completed;
    }

    bool HasHistory(const FDACampaignSnapshot& Campaign, const FName Tag)
    {
        return Campaign.HistoryTags.Contains(Tag);
    }

    bool HasMajorGrievance(const FDACampaignSnapshot& Campaign)
    {
        const FDADiplomaticRelationship* Forge = Campaign.WorldState.Diplomacy.FindRelationship(
            TEXT("relationship.synara.forgeweave"));
        return (Forge != nullptr && Forge->Grievance >= FDAConquestRules::MajorGrievanceThreshold)
            || Campaign.HistoryTags.ContainsByPredicate([](const FName Tag)
            {
                return Tag.ToString().StartsWith(TEXT("major_grievance."));
            });
    }

    bool HasWorkerEndorsement(const FDACampaignSnapshot& Campaign)
    {
        return IsCompletedQuest(Campaign, TEXT("quest.workers_signal"))
            && (HasHistory(Campaign, TEXT("workers_protected"))
                || HasHistory(Campaign, TEXT("mara_numbers_worker_coalition")));
    }

    bool HasSuccessfulServiceCrisis(const FDACampaignSnapshot& Campaign)
    {
        return Campaign.RegionalCrisis.Resolution != EDAFoundryShortageResolution::None
            && Campaign.RegionalCrisis.Resolution != EDAFoundryShortageResolution::Collapse;
    }

    bool HasFulfilledForgeweaveContract(const FDACampaignSnapshot& Campaign)
    {
        return Campaign.WorldState.Trade.Contracts.ContainsByPredicate([&Campaign](const FDATradeContractState& Contract)
        {
            return Contract.RelationshipId == TEXT("relationship.synara.forgeweave")
                && (Contract.SourceRegionId == TEXT("region.ironheart")
                    || Contract.DestinationRegionId == TEXT("region.ironheart"))
                && Contract.bCompleted && Contract.SuccessfulDeliveryCount > 0
                && Campaign.WorldState.Trade.Deliveries.ContainsByPredicate([&Contract](const FDATradeDeliveryRecord& Delivery)
                {
                    return Delivery.ContractId == Contract.ContractId && Delivery.Quantity > 0;
                });
        });
    }

    bool ProveRouteGates(const EDAForgeweaveRoute Route,
        const FDACampaignSnapshot& Campaign, FString& OutError)
    {
        const FDARegionState* Ironheart = Campaign.WorldState.FindRegion(TEXT("region.ironheart"));
        switch (Route)
        {
        case EDAForgeweaveRoute::Force:
        {
            const bool bSovereignty = Campaign.ConquestState.MilitarySovereignty
                    <= FDAConquestRules::ForceCompletionThreshold
                || (Campaign.ConquestState.MilitarySovereignty
                        <= FDAConquestRules::ConditionalSurrenderThreshold
                    && HasHistory(Campaign, TEXT("daxton_surrendered")));
            if (bSovereignty && IsCompletedQuest(Campaign, TEXT("quest.operation_iron_veil"))
                && HasHistory(Campaign, TEXT("daxton_encounter_resolved")) && Ironheart != nullptr
                && Ironheart->OwnerId != TEXT("civilization.forgeweave")
                && !Campaign.OperationConflict.StructuralDamageRecords.IsEmpty()
                && FMath::IsFinite(Campaign.OperationConflict.Resources.PostConflictLoyalty)
                && Campaign.OperationConflict.Resources.PostConflictLoyalty > 0.f
                && Campaign.OperationConflict.Resources.PostConflictLoyalty <= 100.f) return true;
            OutError = TEXT("Force requires sovereignty, Daxton, Ironheart ownership, loyalty, and persistent damage authorities.");
            return false;
        }
        case EDAForgeweaveRoute::Economic:
            if (Campaign.ConquestState.EconomicAutonomy <= FDAConquestRules::EconomicCompletionThreshold
                && HasFulfilledForgeweaveContract(Campaign)
                && IsCompletedQuest(Campaign, TEXT("quest.supply_noose"))
                && HasHistory(Campaign, TEXT("daxton_restructuring_resolved"))) return true;
            OutError = TEXT("Economic completion requires real freight/component leverage, fulfilled trade, threshold, and restructuring.");
            return false;
        case EDAForgeweaveRoute::Influence:
            if (Campaign.ConquestState.CivicLegitimacy <= FDAConquestRules::InfluenceCompletionThreshold
                && HasWorkerEndorsement(Campaign) && HasSuccessfulServiceCrisis(Campaign)) return true;
            OutError = TEXT("Influence completion requires worker endorsement, service-crisis action, and systemic legitimacy.");
            return false;
        case EDAForgeweaveRoute::Alliance:
            if (Campaign.ConquestState.AllianceComponents.Average()
                    >= FDAConquestRules::AllianceAverageThreshold
                && Campaign.ConquestState.AllianceComponents.Minimum()
                    >= FDAConquestRules::AllianceComponentFloor
                && !HasMajorGrievance(Campaign)
                && IsCompletedQuest(Campaign, TEXT("quest.third_foundry"))
                && HasHistory(Campaign, TEXT("joint_forgeweave_crisis_success"))
                && HasHistory(Campaign, TEXT("forge_relic_voluntary_transfer"))) return true;
            OutError = TEXT("Alliance requires average 80, every component 65, no Major Grievance, Third Foundry, joint crisis, and voluntary transfer.");
            return false;
        default:
            OutError = TEXT("Unknown Forgeweave route.");
            return false;
        }
    }
}

const FDAConquestMeterMutation* FDAConquestCampaignState::FindMutation(const FName MutationId) const
{
    return Mutations.FindByPredicate([MutationId](const FDAConquestMeterMutation& Row)
    {
        return Row.MutationId == MutationId;
    });
}

int64 FDAConquestCampaignState::FindMutationRevision(const FName MutationId) const
{
    const int32 Index = Mutations.IndexOfByPredicate([MutationId](const FDAConquestMeterMutation& Row)
    {
        return Row.MutationId == MutationId;
    });
    return Index == INDEX_NONE ? 0 : static_cast<int64>(Index) + 1;
}

bool FDAConquestCampaignState::Validate(FString& OutError) const
{
    if (!IsBounded(MilitarySovereignty) || !IsBounded(EconomicAutonomy)
        || !IsBounded(CivicLegitimacy) || !IsBounded(AllianceReadiness)
        || !IsBounded(AllianceComponents.Trust) || !IsBounded(AllianceComponents.SharedInterest)
        || !IsBounded(AllianceComponents.CrisisResolution) || !IsBounded(AllianceComponents.Respect)
        || !FMath::IsFinite(ForceWeight) || ForceWeight < 0.0
        || !FMath::IsFinite(EconomicWeight) || EconomicWeight < 0.0
        || !FMath::IsFinite(InfluenceWeight) || InfluenceWeight < 0.0
        || !FMath::IsFinite(AllianceWeight) || AllianceWeight < 0.0
        || MutationRevision != Mutations.Num()
        || !FMath::IsNearlyEqual(AllianceReadiness, AllianceComponents.Average(), 0.0001))
    {
        OutError = TEXT("Conquest meters, components, weights, and revision must be finite, bounded, and coherent.");
        return false;
    }

    double ReplayedMilitary = 100.0;
    double ReplayedEconomic = 100.0;
    double ReplayedCivic = 100.0;
    double ReplayedAlliance = 0.0;
    double ReplayedWeights[4] = {0.0, 0.0, 0.0, 0.0};
    TSet<FName> MutationIds;
    int64 PreviousWorldTick = 0;
    for (const FDAConquestMeterMutation& Mutation : Mutations)
    {
        const int32 RouteIndex = static_cast<int32>(Mutation.Route);
        const int32 MeterIndex = static_cast<int32>(Mutation.Meter);
        double* Meter = MeterIndex == 0 ? &ReplayedMilitary : MeterIndex == 1 ? &ReplayedEconomic
            : MeterIndex == 2 ? &ReplayedCivic : MeterIndex == 3 ? &ReplayedAlliance : nullptr;
        if (Mutation.MutationId.IsNone() || MutationIds.Contains(Mutation.MutationId)
            || Mutation.SourceAuthority.IsNone() || Mutation.SourceId.IsNone()
            || Meter == nullptr || RouteIndex < 0 || RouteIndex >= UE_ARRAY_COUNT(ReplayedWeights)
            || !FMath::IsFinite(Mutation.Delta) || Mutation.Delta == 0.0
            || !IsBounded(Mutation.Result) || Mutation.WorldTick < PreviousWorldTick)
        {
            OutError = TEXT("Conquest mutations require unique evidence, valid route/meter data, and ordered World Ticks.");
            return false;
        }
        if (Mutation.Meter == EDAConquestMeter::EconomicAutonomy && Mutation.Delta < -15.0)
        {
            OutError = TEXT("A default conquest action cannot remove more than 15 Economic Autonomy.");
            return false;
        }
        const double ExpectedResult = ApplyMeter(*Meter, Mutation.Delta);
        if (!FMath::IsNearlyEqual(ExpectedResult, Mutation.Result, 0.0001)
            || (Mutation.Meter == EDAConquestMeter::CivicLegitimacy
                && Mutation.SourceAuthority == TEXT("currency.influence") && ExpectedResult <= 0.0))
        {
            OutError = TEXT("Conquest mutation results must replay exactly and stored Influence cannot zero Civic Legitimacy.");
            return false;
        }
        *Meter = ExpectedResult;
        ReplayedWeights[RouteIndex] += FMath::Abs(Mutation.Delta);
        MutationIds.Add(Mutation.MutationId);
        PreviousWorldTick = Mutation.WorldTick;
    }

    if (!FMath::IsNearlyEqual(MilitarySovereignty, ReplayedMilitary, 0.0001)
        || !FMath::IsNearlyEqual(EconomicAutonomy, ReplayedEconomic, 0.0001)
        || !FMath::IsNearlyEqual(CivicLegitimacy, ReplayedCivic, 0.0001)
        || !FMath::IsNearlyEqual(AllianceReadiness, ReplayedAlliance, 0.0001)
        || !FMath::IsNearlyEqual(ForceWeight, ReplayedWeights[0], 0.0001)
        || !FMath::IsNearlyEqual(EconomicWeight, ReplayedWeights[1], 0.0001)
        || !FMath::IsNearlyEqual(InfluenceWeight, ReplayedWeights[2], 0.0001)
        || !FMath::IsNearlyEqual(AllianceWeight, ReplayedWeights[3], 0.0001))
    {
        OutError = TEXT("Persisted conquest meters and weights must exactly replay their mutation ledger.");
        return false;
    }

    int64 PreviousRevision = 0;
    int64 PreviousHistoryTick = 0;
    for (const FDAConquestRouteWeightRecord& Record : RouteWeightHistory)
    {
        double ExpectedWeights[4] = {0.0, 0.0, 0.0, 0.0};
        for (int64 Index = 0; Index < Record.Revision && Index < Mutations.Num(); ++Index)
            ExpectedWeights[static_cast<int32>(Mutations[static_cast<int32>(Index)].Route)]
                += FMath::Abs(Mutations[static_cast<int32>(Index)].Delta);
        if (Record.Revision <= PreviousRevision || Record.Revision > MutationRevision
            || Record.WorldTick < PreviousHistoryTick || !FMath::IsFinite(Record.Force) || Record.Force < 0.0
            || !FMath::IsFinite(Record.Economic) || Record.Economic < 0.0
            || !FMath::IsFinite(Record.Influence) || Record.Influence < 0.0
            || !FMath::IsFinite(Record.Alliance) || Record.Alliance < 0.0
            || Record.WorldTick != Mutations[static_cast<int32>(Record.Revision - 1)].WorldTick
            || !FMath::IsNearlyEqual(Record.Force, ExpectedWeights[0], 0.0001)
            || !FMath::IsNearlyEqual(Record.Economic, ExpectedWeights[1], 0.0001)
            || !FMath::IsNearlyEqual(Record.Influence, ExpectedWeights[2], 0.0001)
            || !FMath::IsNearlyEqual(Record.Alliance, ExpectedWeights[3], 0.0001))
        {
            OutError = TEXT("Route-weight history requires increasing revisions and finite non-negative weights.");
            return false;
        }
        PreviousRevision = Record.Revision;
        PreviousHistoryTick = Record.WorldTick;
    }
    if ((Mutations.IsEmpty() && !RouteWeightHistory.IsEmpty())
        || (!Mutations.IsEmpty() && (RouteWeightHistory.IsEmpty()
            || RouteWeightHistory.Last().Revision != MutationRevision
            || !FMath::IsNearlyEqual(RouteWeightHistory.Last().Force, ForceWeight, 0.0001)
            || !FMath::IsNearlyEqual(RouteWeightHistory.Last().Economic, EconomicWeight, 0.0001)
            || !FMath::IsNearlyEqual(RouteWeightHistory.Last().Influence, InfluenceWeight, 0.0001)
            || !FMath::IsNearlyEqual(RouteWeightHistory.Last().Alliance, AllianceWeight, 0.0001))))
    {
        OutError = TEXT("Newest route-weight history must exactly match the conquest authority.");
        return false;
    }
    if (bForgeweaveResolved != ResolutionActionId.IsValid()
        || (!bForgeweaveResolved && ResolvedWorldTick != 0)
        || (bForgeweaveResolved && ResolvedWorldTick < 0))
    {
        OutError = TEXT("Forgeweave resolution requires one durable action identity and World Tick.");
        return false;
    }
    return true;
}

bool FDAConquestAuthorityValidator::CanCompleteRoute(const EDAForgeweaveRoute Route,
    const FDACampaignSnapshot& Campaign, FString& OutError)
{
    if (Campaign.ConquestState.bForgeweaveResolved)
    {
        OutError = TEXT("Forgeweave is already resolved.");
        return false;
    }
    return ProveRouteGates(Route, Campaign, OutError);
}

bool FDAConquestAuthorityValidator::ValidateResolvedRoute(
    const FDACampaignSnapshot& Campaign, FString& OutError)
{
    if (!Campaign.ConquestState.bForgeweaveResolved) return true;
    static const FName RouteTags[] = {TEXT("forgeweave_forced"), TEXT("forgeweave_economic_union"),
        TEXT("forgeweave_influence_transfer"), TEXT("forgeweave_allied")};
    const int32 RouteIndex = static_cast<int32>(Campaign.ConquestState.ResolvedRoute);
    if (RouteIndex < 0 || RouteIndex >= UE_ARRAY_COUNT(RouteTags)
        || !Campaign.HistoryTags.Contains(RouteTags[RouteIndex]))
    {
        OutError = TEXT("Resolved Forgeweave conquest requires its exact durable route history tag.");
        return false;
    }
    return ProveRouteGates(Campaign.ConquestState.ResolvedRoute, Campaign, OutError);
}
