#pragma once

#include "CoreMinimal.h"
#include "World/DACityGridMetadata.h"

#include "DAForgeweaveCityState.generated.h"

UENUM(BlueprintType)
enum class EDARivalDecisionType : uint8
{
    None,
    Construct,
    Repair,
    Trade,
    Fortify
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAForgeweaveBuildingState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forgeweave")
    FName BuildingId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forgeweave")
    FGuid WorldAssetId;

    /** Rival-owned construction provenance; never a player Collection CardInstance. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forgeweave")
    FGuid ProvenanceId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forgeweave")
    FName CardDefinitionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forgeweave")
    FIntPoint Footprint = FIntPoint(1, 1);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forgeweave", meta = (ClampMin = "0.0"))
    float DeploymentCapital = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forgeweave", meta = (ClampMin = "0.0"))
    float UtilityDemand = 0.f;

};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAForgeweaveModuleRepairDelta
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forgeweave")
    FName ModuleId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forgeweave")
    float HealthBefore = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forgeweave")
    float HealthAfter = 0.f;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAForgeweaveActionTransaction
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forgeweave")
    int64 WorldTick = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forgeweave")
    EDARivalDecisionType Type = EDARivalDecisionType::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forgeweave")
    FName AuthorityId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forgeweave")
    float CapitalBefore = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forgeweave")
    float CapitalAfter = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forgeweave")
    float ProductionBefore = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forgeweave")
    float ProductionAfter = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forgeweave")
    int64 SourceQuantityBefore = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forgeweave")
    int64 SourceQuantityAfter = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forgeweave")
    int64 DestinationQuantityBefore = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forgeweave")
    int64 DestinationQuantityAfter = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forgeweave")
    float IntegrityBefore = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forgeweave")
    float IntegrityAfter = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forgeweave")
    TArray<FDAForgeweaveModuleRepairDelta> ModuleDeltas;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forgeweave")
    FTransform ActorTransform = FTransform::Identity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forgeweave")
    FName CoverTypeId;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAForgeweaveDecisionRecord
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forgeweave")
    int64 WorldTick = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forgeweave")
    EDARivalDecisionType Type = EDARivalDecisionType::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forgeweave")
    FName CardDefinitionId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forgeweave")
    FName TargetBuildingId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forgeweave")
    FIntPoint Origin = FIntPoint::ZeroValue;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forgeweave")
    float CapitalSpent = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forgeweave")
    float ProductionSpent = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forgeweave")
    FName CrisisExplanation;
};

/** Compact authoritative state for Ironheart's unloaded strategic simulation. */
USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAForgeweaveCityState
{
    GENERATED_BODY()

    static const TArray<FName>& GetVerticalSliceBuildPool();
    static bool IsVerticalSliceBuildCard(FName CardDefinitionId);
    static FDAForgeweaveCityState MakeVerticalSliceInitialState(int32 CampaignSeed, int64 InitialWorldTick);
    static constexpr int32 IronheartGridWidth = FDACityGridMetadata::Width;
    static constexpr int32 IronheartGridHeight = FDACityGridMetadata::Height;
    static constexpr float IronheartCellSizeMeters = FDACityGridMetadata::CellSizeMeters;
    static FVector GridCellToWorld(FIntPoint Cell)
    {
        return FDACityGridMetadata::CellToWorld(Cell);
    }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forgeweave")
    bool bInitialized = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forgeweave")
    int32 CampaignSeed = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forgeweave")
    int64 LastProcessedWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forgeweave")
    int32 GridWidth = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forgeweave")
    int32 GridHeight = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forgeweave")
    TArray<FName> AvailableCardIds;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forgeweave")
    TArray<FDAForgeweaveBuildingState> Buildings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forgeweave")
    float Capital = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forgeweave")
    float ProductionReserve = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forgeweave")
    int32 Population = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forgeweave")
    int32 HousingCapacity = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forgeweave")
    float DesiredIndustrialOutput = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forgeweave")
    float ActiveIndustrialThroughput = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forgeweave")
    float MaterialScarcity = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forgeweave")
    float ResourceHunger = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forgeweave")
    float LogisticsEfficiency = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forgeweave")
    float Recycling = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forgeweave")
    float EdenRegenerativeInputs = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forgeweave")
    float ProductionReduction = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forgeweave")
    bool bOverdrive = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forgeweave")
    float DefensePressure = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forgeweave")
    float UtilitySupply = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forgeweave")
    float UtilityDemand = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forgeweave")
    int32 ConsecutiveUnexplainedIdleWorldTicks = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forgeweave")
    TArray<FDAForgeweaveDecisionRecord> DecisionHistory;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forgeweave")
    TArray<FDAForgeweaveActionTransaction> ActionTransactions;

    bool Validate(FString& OutError) const;
};
