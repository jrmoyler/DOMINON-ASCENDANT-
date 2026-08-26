#include "Trade/DATradeSystem.h"

namespace
{
    FName MakeTickRecordId(const TCHAR* Prefix, const FName ContractId, const int64 WorldTick)
    {
        return FName(*FString::Printf(TEXT("%s.%s.tick.%lld"), Prefix, *ContractId.ToString(), WorldTick));
    }
}

bool UDATradeSystem::AddContract(FDATradeWorldState& TradeState, const FDATradeContractState& Contract) const
{
    if (TradeState.FindContract(Contract.ContractId) != nullptr
        || Contract.StartWorldTick <= TradeState.LastProcessedWorldTick
        || Contract.DeliveredQuantity != 0
        || Contract.SuccessfulDeliveryCount != 0
        || Contract.FailedDeliveryCount != 0
        || !Contract.ProcessedWorldTicks.IsEmpty()
        || Contract.bCompleted)
    {
        return false;
    }

    FDATradeWorldState Candidate = TradeState;
    Candidate.Contracts.Add(Contract);
    FString Error;
    if (!Candidate.Validate(Error))
    {
        return false;
    }
    TradeState = MoveTemp(Candidate);
    return true;
}

bool UDATradeSystem::ExecuteSpotTrade(
    FDATradeWorldState& TradeState,
    const FName OrderId,
    const FName RouteId,
    const FName GoodId,
    const int64 Quantity,
    const int64 WorldTick)
{
    FString Error;
    if (OrderId.IsNone()
        || GoodId.IsNone()
        || Quantity <= 0
        || WorldTick != TradeState.LastProcessedWorldTick
        || TradeState.FindSpotOrder(OrderId) != nullptr
        || TradeState.FindContract(OrderId) != nullptr
        || !TradeState.Validate(Error))
    {
        return false;
    }

    FDATradeWorldState Candidate = TradeState;
    FDATradeRouteState* Route = Candidate.FindRoute(RouteId);
    if (Route == nullptr || Route->CapacityWorldTick != WorldTick
        || Route->CapacityPerWorldTick - Route->ReservedCapacityThisTick < Quantity)
    {
        return false;
    }
    FDARegionalTradeInventory* Source = Candidate.FindInventory(Route->SourceRegionId);
    FDARegionalTradeInventory* Destination = Candidate.FindInventory(Route->DestinationRegionId);
    int64* SourceStock = Source != nullptr ? Source->Stock.Find(GoodId) : nullptr;
    int64* DestinationStock = Destination != nullptr ? Destination->Stock.Find(GoodId) : nullptr;
    if (SourceStock == nullptr
        || DestinationStock == nullptr
        || *SourceStock < Quantity
        || *DestinationStock > MAX_int64 - Quantity)
    {
        return false;
    }

    *SourceStock -= Quantity;
    *DestinationStock += Quantity;
    Route->ReservedCapacityThisTick += Quantity;

    FDATradeSpotOrderState Order;
    Order.OrderId = OrderId;
    Order.RouteId = RouteId;
    Order.SourceRegionId = Route->SourceRegionId;
    Order.DestinationRegionId = Route->DestinationRegionId;
    Order.GoodId = GoodId;
    Order.Quantity = Quantity;
    Order.WorldTick = WorldTick;
    Candidate.SpotOrders.Add(Order);

    FDATradeDeliveryRecord Delivery;
    Delivery.DeliveryId = FName(*FString::Printf(TEXT("delivery.%s"), *OrderId.ToString()));
    Delivery.ContractId = OrderId;
    Delivery.GoodId = GoodId;
    Delivery.Quantity = Quantity;
    Delivery.WorldTick = WorldTick;
    Candidate.Deliveries.Add(Delivery);

    if (!Candidate.Validate(Error))
    {
        return false;
    }
    TradeState = MoveTemp(Candidate);
    return true;
}

EDATradeTickResult UDATradeSystem::ProcessWorldTick(
    FDATradeWorldState& TradeState,
    FDADiplomacyWorldState& DiplomacyState,
    const UDADiplomacySystem& DiplomacySystem,
    const int64 WorldTick) const
{
    if (TradeState.LastProcessedWorldTick == MAX_int64
        || WorldTick != TradeState.LastProcessedWorldTick + 1)
    {
        return EDATradeTickResult::DuplicateOrOutOfOrder;
    }

    FString Error;
    if (!TradeState.Validate(Error) || !DiplomacyState.Validate(Error))
    {
        return EDATradeTickResult::InvalidState;
    }

    FDATradeWorldState CandidateTrade = TradeState;
    FDADiplomacyWorldState CandidateDiplomacy = DiplomacyState;
    for (FDATradeRouteState& Route : CandidateTrade.Routes)
    {
        Route.CapacityWorldTick = WorldTick;
        Route.ReservedCapacityThisTick = 0;
    }

    TArray<FName> ContractIds;
    for (const FDATradeContractState& Contract : CandidateTrade.Contracts)
    {
        ContractIds.Add(Contract.ContractId);
    }
    ContractIds.Sort([](const FName Left, const FName Right)
    {
        return Left.ToString() < Right.ToString();
    });

    for (const FName ContractId : ContractIds)
    {
        FDATradeContractState* Contract = CandidateTrade.FindContract(ContractId);
        if (Contract == nullptr || Contract->bCompleted)
        {
            continue;
        }

        const int64 EndWorldTick = Contract->StartWorldTick + static_cast<int64>(Contract->DurationWorldTicks) - 1;
        if (WorldTick < Contract->StartWorldTick || WorldTick > EndWorldTick)
        {
            continue;
        }

        Contract->ProcessedWorldTicks.Add(WorldTick);
        FDARegionalTradeInventory* SourceInventory = CandidateTrade.FindInventory(Contract->SourceRegionId);
        FDARegionalTradeInventory* DestinationInventory = CandidateTrade.FindInventory(Contract->DestinationRegionId);
        FDATradeRouteState* Route = CandidateTrade.FindRoute(Contract->RouteId);
        FDADiplomaticRelationship* Relationship = CandidateDiplomacy.FindRelationship(Contract->RelationshipId);
        int64* SourceStock = SourceInventory != nullptr ? SourceInventory->Stock.Find(Contract->GoodId) : nullptr;
        int64* DestinationStock = DestinationInventory != nullptr ? DestinationInventory->Stock.Find(Contract->GoodId) : nullptr;
        const bool bHasRouteCapacity = Route != nullptr
            && Route->CapacityPerWorldTick - Route->ReservedCapacityThisTick >= Contract->QuantityPerWorldTick;
        const bool bHasSupply = SourceStock != nullptr && *SourceStock >= Contract->QuantityPerWorldTick;
        const bool bHasDestinationSink = DestinationStock != nullptr
            && Contract->QuantityPerWorldTick <= MAX_int64 - *DestinationStock;
        const bool bCanRecordFulfillment = Contract->QuantityPerWorldTick <= MAX_int64 - Contract->DeliveredQuantity;

        const FName DiplomaticMutationId = MakeTickRecordId(TEXT("reason.trade"), Contract->ContractId, WorldTick);
        if (Relationship == nullptr
            || !bHasRouteCapacity
            || !bHasSupply
            || !bHasDestinationSink
            || !bCanRecordFulfillment
            || !DiplomacySystem.ApplyReason(
                *Relationship,
                Contract->RelationshipMetric,
                Contract->RelationshipReasonSource,
                Contract->RelationshipMagnitudePerDelivery,
                WorldTick,
                DiplomaticMutationId))
        {
            ++Contract->FailedDeliveryCount;
            Contract->bCompleted = WorldTick == EndWorldTick;
            continue;
        }

        *SourceStock -= Contract->QuantityPerWorldTick;
        *DestinationStock += Contract->QuantityPerWorldTick;
        Route->ReservedCapacityThisTick += Contract->QuantityPerWorldTick;
        Contract->DeliveredQuantity += Contract->QuantityPerWorldTick;
        ++Contract->SuccessfulDeliveryCount;

        FDATradeDeliveryRecord Delivery;
        Delivery.DeliveryId = MakeTickRecordId(TEXT("delivery"), Contract->ContractId, WorldTick);
        Delivery.ContractId = Contract->ContractId;
        Delivery.GoodId = Contract->GoodId;
        Delivery.Quantity = Contract->QuantityPerWorldTick;
        Delivery.WorldTick = WorldTick;
        CandidateTrade.Deliveries.Add(MoveTemp(Delivery));
        Contract->bCompleted = WorldTick == EndWorldTick;
    }

    CandidateTrade.LastProcessedWorldTick = WorldTick;
    if (!CandidateTrade.Validate(Error) || !CandidateDiplomacy.Validate(Error))
    {
        return EDATradeTickResult::InvalidState;
    }
    TradeState = MoveTemp(CandidateTrade);
    DiplomacyState = MoveTemp(CandidateDiplomacy);
    return EDATradeTickResult::Processed;
}
