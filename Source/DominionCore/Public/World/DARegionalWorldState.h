#pragma once

#include "CoreMinimal.h"
#include "World/DAForgeweaveCityState.h"

#include "DARegionalWorldState.generated.h"

UENUM(BlueprintType)
enum class EDADiplomaticMetric : uint8
{
    Trust,
    Respect,
    Fear,
    Dependence,
    Grievance,
    Compatibility
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDADiplomaticReason
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Diplomacy")
    FName MutationId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Diplomacy")
    FName SourceTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Diplomacy")
    EDADiplomaticMetric Metric = EDADiplomaticMetric::Trust;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Diplomacy")
    float Magnitude = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Diplomacy")
    int64 WorldTick = 0;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDADiplomaticRelationship
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Diplomacy")
    FName RelationshipId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diplomacy")
    float Trust = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diplomacy")
    float Respect = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diplomacy")
    float Fear = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diplomacy")
    float Dependence = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diplomacy")
    float Grievance = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diplomacy")
    float Compatibility = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diplomacy")
    TArray<FDADiplomaticReason> ReasonLedger;

    bool Validate(FString& OutError) const;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDADiplomacyWorldState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Diplomacy")
    TArray<FDADiplomaticRelationship> Relationships;

    FDADiplomaticRelationship* FindRelationship(FName RelationshipId);
    const FDADiplomaticRelationship* FindRelationship(FName RelationshipId) const;
    bool Validate(FString& OutError) const;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDARegionalTradeInventory
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade")
    FName RegionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade")
    TMap<FName, int64> Stock;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDATradeRouteState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade")
    FName RouteId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade")
    FName SourceRegionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade")
    FName DestinationRegionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade")
    int64 CapacityPerWorldTick = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trade")
    int64 CapacityWorldTick = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trade")
    int64 ReservedCapacityThisTick = 0;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDATradeContractState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade")
    FName ContractId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade")
    FName RelationshipId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade")
    FName RouteId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade")
    FName SourceRegionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade")
    FName DestinationRegionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade")
    FName GoodId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade")
    int64 QuantityPerWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade")
    int64 StartWorldTick = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade")
    int32 DurationWorldTicks = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade")
    EDADiplomaticMetric RelationshipMetric = EDADiplomaticMetric::Trust;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade")
    float RelationshipMagnitudePerDelivery = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade")
    FName RelationshipReasonSource;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trade")
    int64 DeliveredQuantity = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trade")
    int32 SuccessfulDeliveryCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trade")
    int32 FailedDeliveryCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trade")
    TArray<int64> ProcessedWorldTicks;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trade")
    bool bCompleted = false;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDATradeDeliveryRecord
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trade")
    FName DeliveryId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trade")
    FName ContractId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trade")
    FName GoodId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trade")
    int64 Quantity = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trade")
    int64 WorldTick = 0;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDATradeSpotOrderState
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trade")
    FName OrderId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trade")
    FName RouteId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trade")
    FName SourceRegionId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trade")
    FName DestinationRegionId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trade")
    FName GoodId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trade")
    int64 Quantity = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trade")
    int64 WorldTick = 0;
};

/** Durable market mutation; the newest record for a good is its active price modifier. */
USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAMarketPriceModifierRecord
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Trade|Market") FName MutationId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Trade|Market") FName GoodId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Trade|Market") FName SourceEventId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Trade|Market") double Modifier = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Trade|Market") int64 WorldTick = 0;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDATradeWorldState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade")
    int64 LastProcessedWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade")
    TArray<FDARegionalTradeInventory> Inventories;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade")
    TArray<FDATradeRouteState> Routes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade")
    TArray<FDATradeContractState> Contracts;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade")
    TArray<FDATradeSpotOrderState> SpotOrders;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade")
    TArray<FDATradeDeliveryRecord> Deliveries;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trade|Market")
    TArray<FDAMarketPriceModifierRecord> MarketPriceModifiers;

    FDARegionalTradeInventory* FindInventory(FName RegionId);
    const FDARegionalTradeInventory* FindInventory(FName RegionId) const;
    FDATradeRouteState* FindRoute(FName RouteId);
    const FDATradeRouteState* FindRoute(FName RouteId) const;
    FDATradeContractState* FindContract(FName ContractId);
    const FDATradeSpotOrderState* FindSpotOrder(FName OrderId) const;
    double GetMarketPriceModifier(FName GoodId) const;
    bool Validate(FString& OutError) const;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAEcologyReason
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ecology") FName MutationId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ecology") FName SourceTag;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ecology") double Baseline = 100.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ecology") double Delta = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ecology") double Result = 100.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ecology") int64 WorldTick = 0;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAEcologyWorldState
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ecology") double Balance = 100.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ecology") TArray<FDAEcologyReason> ReasonLedger;
    bool ApplyReason(FName MutationId, FName SourceTag, double Delta, int64 WorldTick);
    bool Validate(FString& OutError) const;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDARegionActorState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region")
    FName ActorId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region")
    FName DefinitionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region")
    FGuid WorldAssetId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region")
    FTransform Transform = FTransform::Identity;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDARegionPersistentDelta
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region")
    int64 Revision = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region")
    TArray<FDARegionActorState> LocalActors;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region")
    TMap<FName, int32> ResourceQuantities;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region")
    TArray<FName> StateTags;

    bool Validate(FString& OutError) const;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDARegionState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region")
    FName RegionId;

    /** Authored world map used by the production streaming handoff. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region")
    FSoftObjectPath MapAssetPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region")
    FName OwnerId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region")
    TArray<FName> SettlementIds;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region")
    FDARegionPersistentDelta PersistentDelta;

    bool Validate(FString& OutError) const;
};

UENUM(BlueprintType)
enum class EDATravelHandoffStage : uint8
{
    None,
    Started,
    SourceSnapshotted,
    SourceUnloaded,
    TimeAdvanced,
    DestinationLoaded,
    DestinationReconstructed,
    Completed
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDATravelHandoffState
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Travel")
    FName RequestId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Travel")
    FName SourceRegionId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Travel")
    FName DestinationRegionId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Travel")
    int64 DepartureWorldTick = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Travel")
    int32 TravelWorldTicks = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Travel")
    EDATravelHandoffStage Stage = EDATravelHandoffStage::None;
};

/** Exact restorable simulation-clock authority, including pending sub-cycle time. */
USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDASimulationClockAuthorityState
{
    GENERATED_BODY()

    bool Validate(FString& OutError) const;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World|Time")
    bool bCaptured = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World|Time")
    int64 CurrentDevelopmentCycle = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World|Time")
    int64 CurrentWorldTick = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World|Time")
    double AccumulatedSimulationSeconds = 0.0;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAWorldCampaignState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World")
    bool bInitialized = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World")
    int64 CurrentWorldTick = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World|Time")
    FDASimulationClockAuthorityState ClockAuthority;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World")
    FName CurrentRegionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World")
    TArray<FDARegionState> Regions;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World")
    FDATradeWorldState Trade;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World")
    FDADiplomacyWorldState Diplomacy;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World")
    FDAEcologyWorldState Ecology;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World")
    FDAForgeweaveCityState Forgeweave;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World")
    FDATravelHandoffState TravelHandoff;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World")
    TArray<FName> CompletedTravelRequestIds;

    FDARegionState* FindRegion(FName RegionId);
    const FDARegionState* FindRegion(FName RegionId) const;
    void NormalizeEphemeralTravelState();
    bool Validate(FString& OutError) const;
};
