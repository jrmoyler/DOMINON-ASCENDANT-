#pragma once

#include "CoreMinimal.h"
#include "City/DAPlacementTypes.h"
#include "World/DACityGridMetadata.h"

struct DOMINIONSIMULATION_API FDAGridCell
{
    FDAWorldAssetId Occupant;
    uint8 LayerMask = 0;
    bool bRoad = false;
    bool bClaimed = false;
};

class DOMINIONSIMULATION_API FDACityGridSubsystem
{
public:
    static constexpr int32 DefaultGridWidth = FDACityGridMetadata::Width;
    static constexpr int32 DefaultGridHeight = FDACityGridMetadata::Height;

    explicit FDACityGridSubsystem(int32 InWidth = DefaultGridWidth, int32 InHeight = DefaultGridHeight);

    FDAPlacementResult ValidatePlacement(const FDACardPlacementRequest& Request) const;
    bool ReserveFootprint(const FDAWorldAssetId& AssetId, const FIntPoint& Origin, const FIntPoint& Footprint, EGridRotation Rotation);
    void ReleaseFootprint(const FDAWorldAssetId& AssetId);

    bool SetCellClaimed(const FIntPoint& Cell, bool bClaimed);
    void SetAllCellsClaimed(bool bClaimed);
    bool SetCellLayerMask(const FIntPoint& Cell, uint8 LayerMask);
    bool SetCellRoad(const FIntPoint& Cell, bool bRoad);
    FDAWorldAssetId GetOccupant(const FIntPoint& Cell) const;

private:
    bool IsInBounds(const FIntPoint& Cell) const;
    int32 GetCellIndex(const FIntPoint& Cell) const;
    FIntPoint GetRotatedFootprint(const FIntPoint& Footprint, EGridRotation Rotation) const;
    bool HasRoadAccess(const FIntPoint& Origin, const FIntPoint& RotatedFootprint) const;

    int32 Width = 0;
    int32 Height = 0;
    TArray<FDAGridCell> Cells;
};
