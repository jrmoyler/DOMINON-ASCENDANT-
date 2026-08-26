#include "World/DAForgeweaveCityState.h"

namespace
{
    bool IsFiniteNonNegative(const float Value)
    {
        return FMath::IsFinite(Value) && Value >= 0.f;
    }

    bool IsValidatedCrisisCause(const FName Cause)
    {
        return Cause == TEXT("crisis.unaffordable")
            || Cause == TEXT("crisis.utility_capacity")
            || Cause == TEXT("crisis.no_valid_placement");
    }

}

const TArray<FName>& FDAForgeweaveCityState::GetVerticalSliceBuildPool()
{
    static const TArray<FName> FrozenPool = {
        TEXT("forgeweave.worker_arcology"),
        TEXT("forgeweave.production_directorate"),
        TEXT("forgeweave.industrial_exchange"),
        TEXT("forgeweave.infinite_foundry"),
        TEXT("forgeweave.freight_furnace"),
        TEXT("forgeweave.smog_reclaimer")
    };
    return FrozenPool;
}

bool FDAForgeweaveCityState::IsVerticalSliceBuildCard(const FName CardDefinitionId)
{
    return GetVerticalSliceBuildPool().Contains(CardDefinitionId);
}

FDAForgeweaveCityState FDAForgeweaveCityState::MakeVerticalSliceInitialState(
    const int32 InCampaignSeed,
    const int64 InitialWorldTick)
{
    FDAForgeweaveCityState State;
    State.bInitialized = true;
    State.CampaignSeed = InCampaignSeed;
    State.LastProcessedWorldTick = InitialWorldTick;
    State.GridWidth = IronheartGridWidth;
    State.GridHeight = IronheartGridHeight;
    State.AvailableCardIds = GetVerticalSliceBuildPool();
    State.Capital = 60.f;
    State.ProductionReserve = 20.f;
    State.Population = 30;
    State.HousingCapacity = 8;
    State.DesiredIndustrialOutput = 30.f;
    State.ActiveIndustrialThroughput = 4.f;
    State.MaterialScarcity = 25.f;
    State.UtilitySupply = 48.f;
    return State;
}

bool FDAForgeweaveCityState::Validate(FString& OutError) const
{
    if (!bInitialized)
    {
        if (CampaignSeed != 0
            || LastProcessedWorldTick != 0
            || GridWidth != 0
            || GridHeight != 0
            || !AvailableCardIds.IsEmpty()
            || !Buildings.IsEmpty()
            || Capital != 0.f
            || ProductionReserve != 0.f
            || Population != 0
            || HousingCapacity != 0
            || DesiredIndustrialOutput != 0.f
            || ActiveIndustrialThroughput != 0.f
            || MaterialScarcity != 0.f
            || ResourceHunger != 0.f
            || LogisticsEfficiency != 0.f
            || Recycling != 0.f
            || EdenRegenerativeInputs != 0.f
            || ProductionReduction != 0.f
            || bOverdrive
            || DefensePressure != 0.f
            || UtilitySupply != 0.f
            || UtilityDemand != 0.f
            || ConsecutiveUnexplainedIdleWorldTicks != 0
            || !DecisionHistory.IsEmpty()
            || !ActionTransactions.IsEmpty())
        {
            OutError = TEXT("An uninitialized Forgeweave city must be fully canonical empty.");
            return false;
        }
        return true;
    }

    if (LastProcessedWorldTick < 0
        || LastProcessedWorldTick > MAX_int64 / 5
        || GridWidth != IronheartGridWidth
        || GridHeight != IronheartGridHeight
        || Population < 0 || HousingCapacity < 0
        || ConsecutiveUnexplainedIdleWorldTicks < 0
        || !IsFiniteNonNegative(Capital)
        || !IsFiniteNonNegative(ProductionReserve)
        || !IsFiniteNonNegative(DesiredIndustrialOutput)
        || !IsFiniteNonNegative(ActiveIndustrialThroughput)
        || !IsFiniteNonNegative(MaterialScarcity)
        || !IsFiniteNonNegative(ResourceHunger) || ResourceHunger > 100.f
        || !IsFiniteNonNegative(LogisticsEfficiency)
        || !IsFiniteNonNegative(Recycling)
        || !IsFiniteNonNegative(EdenRegenerativeInputs)
        || !IsFiniteNonNegative(ProductionReduction)
        || !IsFiniteNonNegative(DefensePressure)
        || !IsFiniteNonNegative(UtilitySupply)
        || !IsFiniteNonNegative(UtilityDemand)
        || UtilityDemand > UtilitySupply + KINDA_SMALL_NUMBER)
    {
        OutError = TEXT("Forgeweave economy, Hunger, utility, population, grid, and time must remain finite and physically possible.");
        return false;
    }

    TSet<FName> AvailableIds;
    for (const FName CardId : AvailableCardIds)
    {
        if (!IsVerticalSliceBuildCard(CardId) || AvailableIds.Contains(CardId))
        {
            OutError = TEXT("Forgeweave available Card ids must be unique members of the frozen six-card pool.");
            return false;
        }
        AvailableIds.Add(CardId);
    }
    if (AvailableIds.Num() != GetVerticalSliceBuildPool().Num())
    {
        OutError = TEXT("Initialized Forgeweave authority must expose exactly the frozen six-card pool.");
        return false;
    }

    TSet<FName> BuildingIds;
    float ReconciledUtilityDemand = 0.f;
    for (int32 Index = 0; Index < Buildings.Num(); ++Index)
    {
        const FDAForgeweaveBuildingState& Building = Buildings[Index];
        if (Building.BuildingId.IsNone()
            || BuildingIds.Contains(Building.BuildingId)
            || !Building.WorldAssetId.IsValid()
            || !Building.ProvenanceId.IsValid()
            || Building.CardDefinitionId.IsNone()
            || !IsVerticalSliceBuildCard(Building.CardDefinitionId)
            || Building.Footprint.X <= 0 || Building.Footprint.Y <= 0
            || Building.Footprint.X > GridWidth || Building.Footprint.Y > GridHeight
            || !IsFiniteNonNegative(Building.DeploymentCapital)
            || !IsFiniteNonNegative(Building.UtilityDemand))
        {
            OutError = TEXT("Forgeweave buildings require unique ids, valid footprints, bounded integrity, and non-negative costs.");
            return false;
        }
        BuildingIds.Add(Building.BuildingId);
        ReconciledUtilityDemand += Building.UtilityDemand;
    }

    if (!FMath::IsNearlyEqual(UtilityDemand, ReconciledUtilityDemand, 0.001f))
    {
        OutError = TEXT("Forgeweave utility demand must reconcile exactly with durable buildings.");
        return false;
    }

    int64 PreviousDecisionTick = 0;
    int32 ReconciledTrailingUnexplained = 0;
    for (const FDAForgeweaveDecisionRecord& Decision : DecisionHistory)
    {
        const uint8 DecisionType = static_cast<uint8>(Decision.Type);
        if (DecisionType > static_cast<uint8>(EDARivalDecisionType::Fortify)
            || Decision.WorldTick <= PreviousDecisionTick
            || Decision.WorldTick > LastProcessedWorldTick
            || !FMath::IsFinite(Decision.CapitalSpent)
            || Decision.CapitalSpent < 0.f
            || !FMath::IsFinite(Decision.ProductionSpent)
            || Decision.ProductionSpent < 0.f
            || (Decision.Type == EDARivalDecisionType::Construct
                && !IsVerticalSliceBuildCard(Decision.CardDefinitionId))
            || (Decision.Type != EDARivalDecisionType::None && Decision.TargetBuildingId.IsNone())
            || (Decision.Type == EDARivalDecisionType::None
                && (!Decision.CardDefinitionId.IsNone() || !Decision.TargetBuildingId.IsNone()))
            || (Decision.Type != EDARivalDecisionType::None && !Decision.CrisisExplanation.IsNone())
            || (Decision.Type == EDARivalDecisionType::None
                && !Decision.CrisisExplanation.IsNone()
                && !IsValidatedCrisisCause(Decision.CrisisExplanation)))
        {
            OutError = TEXT("Forgeweave decision history must be ordered and may explain idle only with validated causes.");
            return false;
        }
        PreviousDecisionTick = Decision.WorldTick;
    }
    for (int32 Index = DecisionHistory.Num() - 1; Index >= 0; --Index)
    {
        const FDAForgeweaveDecisionRecord& Decision = DecisionHistory[Index];
        if (Decision.Type != EDARivalDecisionType::None || !Decision.CrisisExplanation.IsNone())
        {
            break;
        }
        ++ReconciledTrailingUnexplained;
    }
    if (ReconciledTrailingUnexplained > 10
        || ConsecutiveUnexplainedIdleWorldTicks != ReconciledTrailingUnexplained)
    {
        OutError = TEXT("Forgeweave unexplained idle authority must reconcile and cannot exceed ten World Ticks.");
        return false;
    }

    int64 PreviousTransactionTick = 0;
    for (const FDAForgeweaveActionTransaction& Transaction : ActionTransactions)
    {
        const bool bFiniteBalances = FMath::IsFinite(Transaction.CapitalBefore)
            && FMath::IsFinite(Transaction.CapitalAfter)
            && FMath::IsFinite(Transaction.ProductionBefore)
            && FMath::IsFinite(Transaction.ProductionAfter)
            && FMath::IsFinite(Transaction.IntegrityBefore)
            && FMath::IsFinite(Transaction.IntegrityAfter);
        bool bTypeSpecificAuthorityIsCanonical = false;
        if (Transaction.Type == EDARivalDecisionType::Trade)
        {
            bTypeSpecificAuthorityIsCanonical = Transaction.SourceQuantityBefore >= 0
                && Transaction.SourceQuantityAfter >= 0
                && Transaction.DestinationQuantityBefore >= 0
                && Transaction.DestinationQuantityAfter >= 0
                && Transaction.SourceQuantityBefore > Transaction.SourceQuantityAfter
                && Transaction.DestinationQuantityAfter > Transaction.DestinationQuantityBefore
                && Transaction.IntegrityBefore == 0.f
                && Transaction.IntegrityAfter == 0.f
                && Transaction.ModuleDeltas.IsEmpty()
                && Transaction.CoverTypeId.IsNone()
                && Transaction.ActorTransform.Equals(FTransform::Identity);
        }
        else if (Transaction.Type == EDARivalDecisionType::Repair)
        {
            TSet<FName> RepairedModuleIds;
            bTypeSpecificAuthorityIsCanonical = Transaction.SourceQuantityBefore == 0
                && Transaction.SourceQuantityAfter == 0
                && Transaction.DestinationQuantityBefore == 0
                && Transaction.DestinationQuantityAfter == 0
                && Transaction.IntegrityAfter > Transaction.IntegrityBefore
                && Transaction.IntegrityAfter <= 100.f
                && !Transaction.ModuleDeltas.IsEmpty()
                && Transaction.CoverTypeId.IsNone()
                && Transaction.ActorTransform.Equals(FTransform::Identity);
            for (const FDAForgeweaveModuleRepairDelta& Delta : Transaction.ModuleDeltas)
            {
                if (Delta.ModuleId.IsNone()
                    || RepairedModuleIds.Contains(Delta.ModuleId)
                    || !FMath::IsFinite(Delta.HealthBefore)
                    || !FMath::IsFinite(Delta.HealthAfter)
                    || Delta.HealthBefore < 0.f
                    || Delta.HealthAfter < Delta.HealthBefore)
                {
                    bTypeSpecificAuthorityIsCanonical = false;
                    break;
                }
                RepairedModuleIds.Add(Delta.ModuleId);
            }
        }
        else if (Transaction.Type == EDARivalDecisionType::Fortify)
        {
            bTypeSpecificAuthorityIsCanonical = Transaction.SourceQuantityBefore == 0
                && Transaction.SourceQuantityAfter == 0
                && Transaction.DestinationQuantityBefore == 0
                && Transaction.DestinationQuantityAfter == 0
                && Transaction.IntegrityBefore == 0.f
                && Transaction.IntegrityAfter == 0.f
                && Transaction.ModuleDeltas.IsEmpty()
                && !Transaction.CoverTypeId.IsNone()
                && !Transaction.ActorTransform.ContainsNaN();
        }
        if (Transaction.WorldTick <= PreviousTransactionTick
            || Transaction.WorldTick > LastProcessedWorldTick
            || Transaction.AuthorityId.IsNone()
            || (Transaction.Type != EDARivalDecisionType::Trade
                && Transaction.Type != EDARivalDecisionType::Repair
                && Transaction.Type != EDARivalDecisionType::Fortify)
            || !bFiniteBalances
            || Transaction.CapitalBefore < Transaction.CapitalAfter
            || Transaction.CapitalAfter < 0.f
            || Transaction.ProductionBefore < Transaction.ProductionAfter
            || Transaction.ProductionAfter < 0.f
            || !bTypeSpecificAuthorityIsCanonical)
        {
            OutError = TEXT("Forgeweave action transactions require ordered stable authority and finite opening/closing balances.");
            return false;
        }
        PreviousTransactionTick = Transaction.WorldTick;
    }
    return true;
}
