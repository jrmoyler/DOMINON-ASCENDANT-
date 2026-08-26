#pragma once

#include "CoreMinimal.h"
#include "Diplomacy/DADiplomacySystem.h"
#include "UObject/Object.h"
#include "World/DARegionalWorldState.h"

#include "DATradeSystem.generated.h"

UENUM()
enum class EDATradeTickResult : uint8
{
    Processed,
    DuplicateOrOutOfOrder,
    InvalidState
};

UCLASS()
class DOMINIONWORLD_API UDATradeSystem final : public UObject
{
    GENERATED_BODY()

public:
    bool AddContract(FDATradeWorldState& TradeState, const FDATradeContractState& Contract) const;
    static bool ExecuteSpotTrade(
        FDATradeWorldState& TradeState,
        FName OrderId,
        FName RouteId,
        FName GoodId,
        int64 Quantity,
        int64 WorldTick);

    EDATradeTickResult ProcessWorldTick(
        FDATradeWorldState& TradeState,
        FDADiplomacyWorldState& DiplomacyState,
        const UDADiplomacySystem& DiplomacySystem,
        int64 WorldTick) const;
};
