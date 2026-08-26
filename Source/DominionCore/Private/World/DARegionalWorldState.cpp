#include "World/DARegionalWorldState.h"

namespace
{
    bool TryAddNonNegativeInt64(const int64 Left, const int64 Right, int64& OutValue)
    {
        if (Left < 0 || Right < 0 || Left > MAX_int64 - Right)
        {
            return false;
        }
        OutValue = Left + Right;
        return true;
    }

    bool TryMultiplyNonNegativeInt64(const int64 Left, const int64 Right, int64& OutValue)
    {
        if (Left < 0 || Right < 0 || (Right != 0 && Left > MAX_int64 / Right))
        {
            return false;
        }
        OutValue = Left * Right;
        return true;
    }

    bool HasReached(const EDATravelHandoffStage Current, const EDATravelHandoffStage Required)
    {
        return static_cast<uint8>(Current) >= static_cast<uint8>(Required);
    }

    bool IsFiniteRelationship(const FDADiplomaticRelationship& Relationship)
    {
        return FMath::IsFinite(Relationship.Trust)
            && FMath::IsFinite(Relationship.Respect)
            && FMath::IsFinite(Relationship.Fear)
            && FMath::IsFinite(Relationship.Dependence)
            && FMath::IsFinite(Relationship.Grievance)
            && FMath::IsFinite(Relationship.Compatibility);
    }

    bool IsContractDefinitionValid(const FDATradeContractState& Contract)
    {
        return !Contract.ContractId.IsNone()
            && !Contract.RelationshipId.IsNone()
            && !Contract.RouteId.IsNone()
            && !Contract.SourceRegionId.IsNone()
            && !Contract.DestinationRegionId.IsNone()
            && Contract.SourceRegionId != Contract.DestinationRegionId
            && !Contract.GoodId.IsNone()
            && Contract.QuantityPerWorldTick > 0
            && Contract.StartWorldTick > 0
            && Contract.DurationWorldTicks >= 5
            && Contract.DurationWorldTicks <= 20
            && FMath::IsFinite(Contract.RelationshipMagnitudePerDelivery)
            && Contract.RelationshipMagnitudePerDelivery != 0.f
            && !Contract.RelationshipReasonSource.IsNone();
    }
}

bool FDADiplomaticRelationship::Validate(FString& OutError) const
{
    if (RelationshipId.IsNone() || !IsFiniteRelationship(*this))
    {
        OutError = TEXT("Diplomatic relationships require a stable id and finite aggregate values.");
        return false;
    }

    float Explained[6] = {};
    TSet<FName> MutationIds;
    for (const FDADiplomaticReason& Reason : ReasonLedger)
    {
        const int32 MetricIndex = static_cast<int32>(Reason.Metric);
        if (Reason.MutationId.IsNone()
            || MutationIds.Contains(Reason.MutationId)
            || Reason.SourceTag.IsNone()
            || !FMath::IsFinite(Reason.Magnitude)
            || Reason.Magnitude == 0.f
            || Reason.WorldTick < 0
            || MetricIndex < 0
            || MetricIndex >= UE_ARRAY_COUNT(Explained))
        {
            OutError = TEXT("Diplomatic reasons require unique ids, a source, metric, finite non-zero magnitude, and non-negative World Tick.");
            return false;
        }
        MutationIds.Add(Reason.MutationId);
        Explained[MetricIndex] += Reason.Magnitude;
    }

    const float Aggregates[] = {Trust, Respect, Fear, Dependence, Grievance, Compatibility};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Aggregates); ++Index)
    {
        if (!FMath::IsNearlyEqual(Aggregates[Index], Explained[Index], 0.001f))
        {
            OutError = TEXT("Every diplomatic aggregate must be exactly explainable by its durable reason ledger.");
            return false;
        }
    }
    return true;
}

FDADiplomaticRelationship* FDADiplomacyWorldState::FindRelationship(const FName RelationshipId)
{
    return Relationships.FindByPredicate([RelationshipId](const FDADiplomaticRelationship& Relationship)
    {
        return Relationship.RelationshipId == RelationshipId;
    });
}

const FDADiplomaticRelationship* FDADiplomacyWorldState::FindRelationship(const FName RelationshipId) const
{
    return Relationships.FindByPredicate([RelationshipId](const FDADiplomaticRelationship& Relationship)
    {
        return Relationship.RelationshipId == RelationshipId;
    });
}

bool FDADiplomacyWorldState::Validate(FString& OutError) const
{
    TSet<FName> RelationshipIds;
    for (const FDADiplomaticRelationship& Relationship : Relationships)
    {
        if (RelationshipIds.Contains(Relationship.RelationshipId) || !Relationship.Validate(OutError))
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("Diplomatic relationship ids must be unique.");
            }
            return false;
        }
        RelationshipIds.Add(Relationship.RelationshipId);
    }
    return true;
}

FDARegionalTradeInventory* FDATradeWorldState::FindInventory(const FName RegionId)
{
    return Inventories.FindByPredicate([RegionId](const FDARegionalTradeInventory& Inventory)
    {
        return Inventory.RegionId == RegionId;
    });
}

const FDARegionalTradeInventory* FDATradeWorldState::FindInventory(const FName RegionId) const
{
    return Inventories.FindByPredicate([RegionId](const FDARegionalTradeInventory& Inventory)
    {
        return Inventory.RegionId == RegionId;
    });
}

FDATradeRouteState* FDATradeWorldState::FindRoute(const FName RouteId)
{
    return Routes.FindByPredicate([RouteId](const FDATradeRouteState& Route)
    {
        return Route.RouteId == RouteId;
    });
}

const FDATradeRouteState* FDATradeWorldState::FindRoute(const FName RouteId) const
{
    return Routes.FindByPredicate([RouteId](const FDATradeRouteState& Route)
    {
        return Route.RouteId == RouteId;
    });
}

FDATradeContractState* FDATradeWorldState::FindContract(const FName ContractId)
{
    return Contracts.FindByPredicate([ContractId](const FDATradeContractState& Contract)
    {
        return Contract.ContractId == ContractId;
    });
}

const FDATradeSpotOrderState* FDATradeWorldState::FindSpotOrder(const FName OrderId) const
{
    return SpotOrders.FindByPredicate([OrderId](const FDATradeSpotOrderState& Order)
    {
        return Order.OrderId == OrderId;
    });
}

double FDATradeWorldState::GetMarketPriceModifier(const FName GoodId) const
{
    for (int32 Index = MarketPriceModifiers.Num() - 1; Index >= 0; --Index)
        if (MarketPriceModifiers[Index].GoodId == GoodId)
            return MarketPriceModifiers[Index].Modifier;
    return 0.0;
}

bool FDATradeWorldState::Validate(FString& OutError) const
{
    if (LastProcessedWorldTick < 0)
    {
        OutError = TEXT("Trade World Tick authority cannot be negative.");
        return false;
    }

    TSet<FName> InventoryIds;
    for (const FDARegionalTradeInventory& Inventory : Inventories)
    {
        if (Inventory.RegionId.IsNone() || InventoryIds.Contains(Inventory.RegionId))
        {
            OutError = TEXT("Trade inventories require unique stable region ids.");
            return false;
        }
        InventoryIds.Add(Inventory.RegionId);
        for (const TPair<FName, int64>& StockEntry : Inventory.Stock)
        {
            if (StockEntry.Key.IsNone() || StockEntry.Value < 0)
            {
                OutError = TEXT("Trade inventory stock requires stable good ids and non-negative quantities.");
                return false;
            }
        }
    }

    TSet<FName> RouteIds;
    for (const FDATradeRouteState& Route : Routes)
    {
        if (Route.RouteId.IsNone()
            || RouteIds.Contains(Route.RouteId)
            || Route.SourceRegionId.IsNone()
            || Route.DestinationRegionId.IsNone()
            || Route.SourceRegionId == Route.DestinationRegionId
            || Route.CapacityPerWorldTick < 0
            || Route.ReservedCapacityThisTick < 0
            || Route.ReservedCapacityThisTick > Route.CapacityPerWorldTick
            || Route.CapacityWorldTick < 0
            || Route.CapacityWorldTick > LastProcessedWorldTick
            || (Route.CapacityWorldTick != LastProcessedWorldTick && Route.ReservedCapacityThisTick != 0))
        {
            OutError = TEXT("Trade routes require unique ids, distinct endpoints, and bounded capacity reservations.");
            return false;
        }
        RouteIds.Add(Route.RouteId);
    }

    TSet<FName> ContractIds;
    for (const FDATradeContractState& Contract : Contracts)
    {
        int64 ExpectedDeliveredQuantity = 0;
        int64 EndWorldTickPlusOne = 0;
        const int64 AttemptCount = static_cast<int64>(Contract.SuccessfulDeliveryCount)
            + static_cast<int64>(Contract.FailedDeliveryCount);
        const FDATradeRouteState* Route = FindRoute(Contract.RouteId);
        const FDARegionalTradeInventory* SourceInventory = FindInventory(Contract.SourceRegionId);
        const FDARegionalTradeInventory* DestinationInventory = FindInventory(Contract.DestinationRegionId);
        if (!IsContractDefinitionValid(Contract)
            || ContractIds.Contains(Contract.ContractId)
            || Route == nullptr
            || SourceInventory == nullptr
            || DestinationInventory == nullptr
            || !SourceInventory->Stock.Contains(Contract.GoodId)
            || !DestinationInventory->Stock.Contains(Contract.GoodId)
            || Route->SourceRegionId != Contract.SourceRegionId
            || Route->DestinationRegionId != Contract.DestinationRegionId
            || Contract.DeliveredQuantity < 0
            || Contract.SuccessfulDeliveryCount < 0
            || Contract.FailedDeliveryCount < 0
            || !TryMultiplyNonNegativeInt64(
                Contract.SuccessfulDeliveryCount,
                Contract.QuantityPerWorldTick,
                ExpectedDeliveredQuantity)
            || Contract.DeliveredQuantity != ExpectedDeliveredQuantity
            || !TryAddNonNegativeInt64(
                Contract.StartWorldTick,
                static_cast<int64>(Contract.DurationWorldTicks),
                EndWorldTickPlusOne)
            || static_cast<int64>(Contract.ProcessedWorldTicks.Num()) != AttemptCount
            || Contract.ProcessedWorldTicks.Num() > Contract.DurationWorldTicks)
        {
            OutError = TEXT("Trade contracts require unique ids, real endpoint inventories/routes, and coherent fulfillment totals.");
            return false;
        }

        TSet<int64> ProcessedTicks;
        const int64 EndWorldTick = EndWorldTickPlusOne - 1;
        for (const int64 Tick : Contract.ProcessedWorldTicks)
        {
            if (Tick < Contract.StartWorldTick
                || Tick > EndWorldTick
                || Tick > LastProcessedWorldTick
                || ProcessedTicks.Contains(Tick))
            {
                OutError = TEXT("Contract attempt World Ticks must be unique and inside the scheduled duration.");
                return false;
            }
            ProcessedTicks.Add(Tick);
        }
        const int64 ElapsedScheduledTicks = LastProcessedWorldTick < Contract.StartWorldTick
            ? 0
            : FMath::Min<int64>(LastProcessedWorldTick - Contract.StartWorldTick + 1, Contract.DurationWorldTicks);
        if (Contract.ProcessedWorldTicks.Num() != ElapsedScheduledTicks
            || Contract.bCompleted != (LastProcessedWorldTick >= EndWorldTick))
        {
            OutError = TEXT("Contract attempt history and completion must match elapsed scheduled World Ticks.");
            return false;
        }
        ContractIds.Add(Contract.ContractId);
    }

    TSet<FName> DeliveryIds;
    TSet<FName> SpotOrderIds;
    for (const FDATradeSpotOrderState& Order : SpotOrders)
    {
        const FDATradeRouteState* Route = FindRoute(Order.RouteId);
        if (Order.OrderId.IsNone()
            || SpotOrderIds.Contains(Order.OrderId)
            || ContractIds.Contains(Order.OrderId)
            || Route == nullptr
            || Route->SourceRegionId != Order.SourceRegionId
            || Route->DestinationRegionId != Order.DestinationRegionId
            || FindInventory(Order.SourceRegionId) == nullptr
            || FindInventory(Order.DestinationRegionId) == nullptr
            || Order.GoodId.IsNone()
            || Order.Quantity <= 0
            || Order.WorldTick <= 0
            || Order.WorldTick > LastProcessedWorldTick)
        {
            OutError = TEXT("Spot orders require unique ids, a real route and inventories, positive quantity, and committed World Tick.");
            return false;
        }
        SpotOrderIds.Add(Order.OrderId);
    }
    for (const FDATradeDeliveryRecord& Delivery : Deliveries)
    {
        if (Delivery.DeliveryId.IsNone()
            || DeliveryIds.Contains(Delivery.DeliveryId)
            || (!ContractIds.Contains(Delivery.ContractId) && !SpotOrderIds.Contains(Delivery.ContractId))
            || Delivery.GoodId.IsNone()
            || Delivery.Quantity <= 0
            || Delivery.WorldTick <= 0
            || Delivery.WorldTick > LastProcessedWorldTick)
        {
            OutError = TEXT("Trade deliveries require unique ids, a real contract, good, quantity, and World Tick.");
            return false;
        }
        DeliveryIds.Add(Delivery.DeliveryId);
    }

    for (const FDATradeSpotOrderState& Order : SpotOrders)
    {
        const FDATradeDeliveryRecord* Delivery = Deliveries.FindByPredicate(
            [&Order](const FDATradeDeliveryRecord& Record) { return Record.ContractId == Order.OrderId; });
        int32 MatchingDeliveries = 0;
        for (const FDATradeDeliveryRecord& Record : Deliveries)
        {
            MatchingDeliveries += Record.ContractId == Order.OrderId ? 1 : 0;
        }
        if (Delivery == nullptr
            || Delivery->GoodId != Order.GoodId
            || Delivery->Quantity != Order.Quantity
            || Delivery->WorldTick != Order.WorldTick
            || MatchingDeliveries != 1)
        {
            OutError = TEXT("Every spot order must reconcile with exactly one durable delivery.");
            return false;
        }
    }

    for (const FDATradeContractState& Contract : Contracts)
    {
        int32 DeliveryCount = 0;
        int64 DeliveryQuantity = 0;
        for (const FDATradeDeliveryRecord& Delivery : Deliveries)
        {
            if (Delivery.ContractId != Contract.ContractId)
            {
                continue;
            }
            if (Delivery.GoodId != Contract.GoodId
                || Delivery.Quantity != Contract.QuantityPerWorldTick
                || !Contract.ProcessedWorldTicks.Contains(Delivery.WorldTick))
            {
                OutError = TEXT("Delivery records must match contract good, whole-tick quantity, and processed World Tick.");
                return false;
            }
            ++DeliveryCount;
            if (!TryAddNonNegativeInt64(DeliveryQuantity, Delivery.Quantity, DeliveryQuantity))
            {
                OutError = TEXT("Delivery quantity totals cannot overflow.");
                return false;
            }
        }
        if (DeliveryCount != Contract.SuccessfulDeliveryCount || DeliveryQuantity != Contract.DeliveredQuantity)
        {
            OutError = TEXT("Contract fulfillment totals must reconcile with durable delivery records.");
            return false;
        }
    }
    TSet<FName> MarketMutationIds;
    TMap<FName, int64> LatestMarketTicks;
    for (const FDAMarketPriceModifierRecord& Record : MarketPriceModifiers)
    {
        const int64 PreviousTick = LatestMarketTicks.FindRef(Record.GoodId);
        if (Record.MutationId.IsNone() || MarketMutationIds.Contains(Record.MutationId)
            || Record.GoodId.IsNone() || Record.SourceEventId.IsNone()
            || !FMath::IsFinite(Record.Modifier) || Record.Modifier < 0.0 || Record.Modifier > 1.0
            || Record.WorldTick < 0 || Record.WorldTick > LastProcessedWorldTick
            || (LatestMarketTicks.Contains(Record.GoodId) && Record.WorldTick < PreviousTick))
        {
            OutError = TEXT("Market price modifiers require unique mutations, bounded values, and committed World Ticks.");
            return false;
        }
        MarketMutationIds.Add(Record.MutationId);
        LatestMarketTicks.Add(Record.GoodId, Record.WorldTick);
    }
    return true;
}

bool FDAEcologyWorldState::ApplyReason(const FName MutationId, const FName SourceTag,
    const double Delta, const int64 WorldTick)
{
    if (MutationId.IsNone() || SourceTag.IsNone() || !FMath::IsFinite(Delta) || Delta == 0.0
        || WorldTick < 0 || ReasonLedger.ContainsByPredicate([MutationId](const FDAEcologyReason& Row)
            { return Row.MutationId == MutationId; })) return false;
    FDAEcologyReason Reason;
    Reason.MutationId = MutationId; Reason.SourceTag = SourceTag; Reason.Baseline = Balance;
    Reason.Delta = Delta; Reason.Result = FMath::Clamp(Balance + Delta, 0.0, 100.0);
    Reason.WorldTick = WorldTick; Balance = Reason.Result; ReasonLedger.Add(Reason); return true;
}

bool FDAEcologyWorldState::Validate(FString& OutError) const
{
    double Expected = 100.0;
    TSet<FName> MutationIds;
    for (const FDAEcologyReason& Reason : ReasonLedger)
    {
        if (Reason.MutationId.IsNone() || MutationIds.Contains(Reason.MutationId)
            || Reason.SourceTag.IsNone() || Reason.WorldTick < 0
            || !FMath::IsFinite(Reason.Baseline) || !FMath::IsFinite(Reason.Delta)
            || !FMath::IsFinite(Reason.Result) || Reason.Delta == 0.0
            || Reason.Baseline != Expected
            || Reason.Result != FMath::Clamp(Reason.Baseline + Reason.Delta, 0.0, 100.0))
        {
            OutError = TEXT("Ecology reason ledger must form one exact durable mutation chain.");
            return false;
        }
        MutationIds.Add(Reason.MutationId); Expected = Reason.Result;
    }
    if (!FMath::IsFinite(Balance) || Balance < 0.0 || Balance > 100.0 || Balance != Expected)
    {
        OutError = TEXT("Ecology aggregate must equal its durable reason ledger.");
        return false;
    }
    return true;
}

bool FDARegionPersistentDelta::Validate(FString& OutError) const
{
    if (Revision < 0)
    {
        OutError = TEXT("Region delta revision cannot be negative.");
        return false;
    }
    TSet<FName> ActorIds;
    TSet<FGuid> WorldAssetIds;
    for (const FDARegionActorState& Actor : LocalActors)
    {
        if (Actor.ActorId.IsNone()
            || ActorIds.Contains(Actor.ActorId)
            || Actor.DefinitionId.IsNone()
            || Actor.Transform.ContainsNaN()
            || (Actor.WorldAssetId.IsValid() && WorldAssetIds.Contains(Actor.WorldAssetId)))
        {
            OutError = TEXT("Persistent regional actors require unique stable actor/WorldAsset ids, definitions, and finite transforms.");
            return false;
        }
        ActorIds.Add(Actor.ActorId);
        if (Actor.WorldAssetId.IsValid())
        {
            WorldAssetIds.Add(Actor.WorldAssetId);
        }
    }
    for (const TPair<FName, int32>& Resource : ResourceQuantities)
    {
        if (Resource.Key.IsNone() || Resource.Value < 0)
        {
            OutError = TEXT("Regional resources require stable ids and non-negative quantities.");
            return false;
        }
    }
    TSet<FName> Tags;
    for (const FName Tag : StateTags)
    {
        if (Tag.IsNone() || Tags.Contains(Tag))
        {
            OutError = TEXT("Regional state tags must be stable and unique.");
            return false;
        }
        Tags.Add(Tag);
    }
    return true;
}

bool FDARegionState::Validate(FString& OutError) const
{
    if (RegionId.IsNone() || OwnerId.IsNone() || !PersistentDelta.Validate(OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Regions require stable region and owner ids.");
        }
        return false;
    }
    TSet<FName> SettlementSet;
    for (const FName SettlementId : SettlementIds)
    {
        if (SettlementId.IsNone() || SettlementSet.Contains(SettlementId))
        {
            OutError = TEXT("Region settlement ids must be stable and unique.");
            return false;
        }
        SettlementSet.Add(SettlementId);
    }
    return true;
}

FDARegionState* FDAWorldCampaignState::FindRegion(const FName RegionId)
{
    return Regions.FindByPredicate([RegionId](const FDARegionState& Region) { return Region.RegionId == RegionId; });
}

const FDARegionState* FDAWorldCampaignState::FindRegion(const FName RegionId) const
{
    return Regions.FindByPredicate([RegionId](const FDARegionState& Region) { return Region.RegionId == RegionId; });
}

void FDAWorldCampaignState::NormalizeEphemeralTravelState()
{
    if (TravelHandoff.Stage == EDATravelHandoffStage::DestinationLoaded
        || TravelHandoff.Stage == EDATravelHandoffStage::DestinationReconstructed)
    {
        TravelHandoff.Stage = EDATravelHandoffStage::TimeAdvanced;
    }
}

bool FDASimulationClockAuthorityState::Validate(FString& OutError) const
{
    if (!bCaptured)
    {
        if (CurrentDevelopmentCycle != 0 || CurrentWorldTick != 0
            || AccumulatedSimulationSeconds != 0.0)
        {
            OutError = TEXT("An uncaptured simulation clock authority must contain only legacy defaults.");
            return false;
        }
        return true;
    }

    if (CurrentDevelopmentCycle < 0 || CurrentWorldTick < 0
        || CurrentWorldTick > MAX_int64 / 5
        || !FMath::IsFinite(AccumulatedSimulationSeconds)
        || AccumulatedSimulationSeconds < 0.0
        || AccumulatedSimulationSeconds >= 30.0)
    {
        OutError = TEXT("Captured simulation clock authority requires finite non-negative time.");
        return false;
    }
    const int64 WorldTickCycle = CurrentWorldTick * 5;
    if (CurrentDevelopmentCycle < WorldTickCycle
        || CurrentDevelopmentCycle - WorldTickCycle >= 5)
    {
        OutError = TEXT("Development Cycle authority must remain inside its current World Tick.");
        return false;
    }
    return true;
}

bool FDAWorldCampaignState::Validate(FString& OutError) const
{
    const uint8 HandoffStageValue = static_cast<uint8>(TravelHandoff.Stage);
    if (HandoffStageValue > static_cast<uint8>(EDATravelHandoffStage::Completed))
    {
        OutError = TEXT("Travel handoff stage is outside the serialized enum range.");
        return false;
    }

    if (!bInitialized)
    {
        if (CurrentWorldTick != 0
            || !ClockAuthority.Validate(OutError)
            || ClockAuthority.bCaptured
            || !CurrentRegionId.IsNone()
            || !Regions.IsEmpty()
            || Trade.LastProcessedWorldTick != 0
            || !Trade.Inventories.IsEmpty()
            || !Trade.Routes.IsEmpty()
            || !Trade.Contracts.IsEmpty()
            || !Trade.SpotOrders.IsEmpty()
            || !Trade.Deliveries.IsEmpty()
            || !Trade.MarketPriceModifiers.IsEmpty()
            || !Diplomacy.Relationships.IsEmpty()
            || !Ecology.Validate(OutError) || Ecology.Balance != 100.0 || !Ecology.ReasonLedger.IsEmpty()
            || !Forgeweave.Validate(OutError)
            || !TravelHandoff.RequestId.IsNone()
            || !TravelHandoff.SourceRegionId.IsNone()
            || !TravelHandoff.DestinationRegionId.IsNone()
            || TravelHandoff.DepartureWorldTick != 0
            || TravelHandoff.TravelWorldTicks != 0
            || TravelHandoff.Stage != EDATravelHandoffStage::None
            || !CompletedTravelRequestIds.IsEmpty())
        {
            OutError = TEXT("An uninitialized regional world must contain no authoritative state.");
            return false;
        }
        return true;
    }

    if (CurrentWorldTick < 0
        || CurrentWorldTick > MAX_int64 / 5
        || !ClockAuthority.Validate(OutError)
        || (ClockAuthority.bCaptured && ClockAuthority.CurrentWorldTick != CurrentWorldTick)
        || CurrentRegionId.IsNone()
        || Trade.LastProcessedWorldTick != CurrentWorldTick)
    {
        OutError = TEXT("World state requires coherent non-negative time and current-region authority.");
        return false;
    }

    TSet<FName> RegionIds;
    TSet<FName> SettlementIds;
    for (const FDARegionState& Region : Regions)
    {
        if (!Region.Validate(OutError) || RegionIds.Contains(Region.RegionId))
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("World region ids must be unique.");
            }
            return false;
        }
        RegionIds.Add(Region.RegionId);
        for (const FName SettlementId : Region.SettlementIds)
        {
            if (SettlementIds.Contains(SettlementId))
            {
                OutError = TEXT("A settlement may belong to only one region.");
                return false;
            }
            SettlementIds.Add(SettlementId);
        }
    }

    if (!RegionIds.Contains(CurrentRegionId)
        || !Trade.Validate(OutError)
        || !Diplomacy.Validate(OutError)
        || !Ecology.Validate(OutError)
        || !Forgeweave.bInitialized
        || !Forgeweave.Validate(OutError)
        || Forgeweave.LastProcessedWorldTick != CurrentWorldTick)
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Current region, trade, diplomacy, or Forgeweave authority state is invalid.");
        }
        return false;
    }

    const FDARegionState* Ironheart = FindRegion(TEXT("region.ironheart"));
    if (Ironheart == nullptr)
    {
        OutError = TEXT("Initialized Forgeweave authority requires the Ironheart region.");
        return false;
    }

    if (!Forgeweave.Buildings.IsEmpty())
    {
        for (const FDAForgeweaveBuildingState& Building : Forgeweave.Buildings)
        {
            const FDARegionActorState* Actor = Ironheart->PersistentDelta.LocalActors.FindByPredicate(
                [&Building](const FDARegionActorState& Entry)
                {
                    return Entry.ActorId == Building.BuildingId;
                });
            if (Actor == nullptr
                || Actor->WorldAssetId != Building.WorldAssetId)
            {
                OutError = TEXT("Every durable Forgeweave building must reconcile with its Ironheart reconstruction actor.");
                return false;
            }
        }
    }

    for (const FDATradeContractState& Contract : Trade.Contracts)
    {
        const FDADiplomaticRelationship* Relationship = Diplomacy.FindRelationship(Contract.RelationshipId);
        if (Relationship == nullptr)
        {
            OutError = TEXT("Every trade contract must target a durable diplomatic relationship.");
            return false;
        }
        for (const FDATradeDeliveryRecord& Delivery : Trade.Deliveries)
        {
            if (Delivery.ContractId != Contract.ContractId)
            {
                continue;
            }
            const FName ExpectedMutationId(*FString::Printf(TEXT("reason.trade.%s.tick.%lld"), *Contract.ContractId.ToString(), Delivery.WorldTick));
            const FDADiplomaticReason* Reason = Relationship->ReasonLedger.FindByPredicate(
                [ExpectedMutationId](const FDADiplomaticReason& Candidate) { return Candidate.MutationId == ExpectedMutationId; });
            if (Reason == nullptr
                || Reason->SourceTag != Contract.RelationshipReasonSource
                || Reason->Metric != Contract.RelationshipMetric
                || !FMath::IsNearlyEqual(Reason->Magnitude, Contract.RelationshipMagnitudePerDelivery, 0.001f)
                || Reason->WorldTick != Delivery.WorldTick)
            {
                OutError = TEXT("Every real trade delivery must reconcile with its exact diplomatic reason mutation.");
                return false;
            }
        }
    }

    TSet<FName> CompletedRequestIds;
    for (const FName CompletedRequestId : CompletedTravelRequestIds)
    {
        if (CompletedRequestId.IsNone() || CompletedRequestIds.Contains(CompletedRequestId))
        {
            OutError = TEXT("Completed travel request ids must be stable and unique.");
            return false;
        }
        CompletedRequestIds.Add(CompletedRequestId);
    }

    const FDATravelHandoffState& Handoff = TravelHandoff;
    if (Handoff.Stage != EDATravelHandoffStage::None)
    {
        int64 ArrivalWorldTick = 0;
        if (Handoff.RequestId.IsNone()
            || Handoff.SourceRegionId.IsNone()
            || Handoff.DestinationRegionId.IsNone()
            || Handoff.SourceRegionId == Handoff.DestinationRegionId
            || !RegionIds.Contains(Handoff.SourceRegionId)
            || !RegionIds.Contains(Handoff.DestinationRegionId)
            || Handoff.DepartureWorldTick < 0
            || Handoff.TravelWorldTicks <= 0
            || !TryAddNonNegativeInt64(Handoff.DepartureWorldTick, Handoff.TravelWorldTicks, ArrivalWorldTick)
            || (HasReached(Handoff.Stage, EDATravelHandoffStage::TimeAdvanced) && CurrentWorldTick != ArrivalWorldTick)
            || (static_cast<uint8>(Handoff.Stage) < static_cast<uint8>(EDATravelHandoffStage::SourceUnloaded)
                && CurrentWorldTick != Handoff.DepartureWorldTick)
            || (Handoff.Stage == EDATravelHandoffStage::SourceUnloaded
                && (CurrentWorldTick < Handoff.DepartureWorldTick || CurrentWorldTick > ArrivalWorldTick))
            || (Handoff.Stage == EDATravelHandoffStage::Completed && CurrentRegionId != Handoff.DestinationRegionId)
            || (Handoff.Stage == EDATravelHandoffStage::Completed && !CompletedRequestIds.Contains(Handoff.RequestId))
            || (Handoff.Stage != EDATravelHandoffStage::Completed && CurrentRegionId != Handoff.SourceRegionId))
        {
            OutError = TEXT("Persistent travel checkpoints must retain coherent ids, time, stage, and region authority.");
            return false;
        }
    }
    return true;
}
