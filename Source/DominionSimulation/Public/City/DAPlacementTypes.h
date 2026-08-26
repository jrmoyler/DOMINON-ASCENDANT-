#pragma once

#include "CoreMinimal.h"

#include "DAPlacementTypes.generated.h"

UENUM(BlueprintType)
enum class EGridRotation : uint8
{
    Zero,
    Ninety,
    OneEighty,
    TwoSeventy
};

UENUM(BlueprintType)
enum class EDAPlacementFailureReason : uint8
{
    None,
    OutOfBounds,
    Occupied,
    Unclaimed,
    TerrainInvalid,
    RoadMissing,
    UtilityUnavailable
};

USTRUCT(BlueprintType)
struct DOMINIONSIMULATION_API FDAWorldAssetId
{
    GENERATED_BODY()

    FDAWorldAssetId() = default;

    explicit FDAWorldAssetId(const FGuid& InValue)
        : Value(InValue)
    {
    }

    bool IsValid() const
    {
        return Value.IsValid();
    }

    friend bool operator==(const FDAWorldAssetId& Left, const FDAWorldAssetId& Right)
    {
        return Left.Value == Right.Value;
    }

    friend bool operator!=(const FDAWorldAssetId& Left, const FDAWorldAssetId& Right)
    {
        return !(Left == Right);
    }

    friend uint32 GetTypeHash(const FDAWorldAssetId& AssetId)
    {
        return GetTypeHash(AssetId.Value);
    }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    FGuid Value;
};

USTRUCT(BlueprintType)
struct DOMINIONSIMULATION_API FDACardPlacementRequest
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
    FDAWorldAssetId AssetId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
    FIntPoint Origin = FIntPoint::ZeroValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
    FIntPoint Footprint = FIntPoint(1, 1);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
    EGridRotation Rotation = EGridRotation::Zero;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
    uint8 RequiredTerrainLayerMask = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
    bool bRequiresRoad = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
    uint8 RequiredUtilityLayerMask = 0;
};

USTRUCT(BlueprintType)
struct DOMINIONSIMULATION_API FDAPlacementResult
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Placement")
    bool bCanPlace = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Placement")
    EDAPlacementFailureReason FailureReason = EDAPlacementFailureReason::None;

    static FDAPlacementResult Success()
    {
        FDAPlacementResult Result;
        Result.bCanPlace = true;
        return Result;
    }

    static FDAPlacementResult Failure(const EDAPlacementFailureReason InFailureReason)
    {
        FDAPlacementResult Result;
        Result.FailureReason = InFailureReason;
        return Result;
    }
};
