#include "City/DACityGridSubsystem.h"

FDACityGridSubsystem::FDACityGridSubsystem(const int32 InWidth, const int32 InHeight)
    : Width(FMath::Max(0, InWidth))
    , Height(FMath::Max(0, InHeight))
{
    Cells.SetNum(Width * Height);
}

FDAPlacementResult FDACityGridSubsystem::ValidatePlacement(const FDACardPlacementRequest& Request) const
{
    const FIntPoint RotatedFootprint = GetRotatedFootprint(Request.Footprint, Request.Rotation);
    if (RotatedFootprint.X <= 0 || RotatedFootprint.Y <= 0)
    {
        return FDAPlacementResult::Failure(EDAPlacementFailureReason::OutOfBounds);
    }

    for (int32 Y = 0; Y < RotatedFootprint.Y; ++Y)
    {
        for (int32 X = 0; X < RotatedFootprint.X; ++X)
        {
            const FIntPoint CellPosition = Request.Origin + FIntPoint(X, Y);
            if (!IsInBounds(CellPosition))
            {
                return FDAPlacementResult::Failure(EDAPlacementFailureReason::OutOfBounds);
            }

            const FDAGridCell& Cell = Cells[GetCellIndex(CellPosition)];
            if (!Cell.bClaimed)
            {
                return FDAPlacementResult::Failure(EDAPlacementFailureReason::Unclaimed);
            }

            if (Cell.Occupant.IsValid())
            {
                return FDAPlacementResult::Failure(EDAPlacementFailureReason::Occupied);
            }

            if ((Cell.LayerMask & Request.RequiredTerrainLayerMask) != Request.RequiredTerrainLayerMask)
            {
                return FDAPlacementResult::Failure(EDAPlacementFailureReason::TerrainInvalid);
            }

            if ((Cell.LayerMask & Request.RequiredUtilityLayerMask) != Request.RequiredUtilityLayerMask)
            {
                return FDAPlacementResult::Failure(EDAPlacementFailureReason::UtilityUnavailable);
            }
        }
    }

    if (Request.bRequiresRoad && !HasRoadAccess(Request.Origin, RotatedFootprint))
    {
        return FDAPlacementResult::Failure(EDAPlacementFailureReason::RoadMissing);
    }

    return FDAPlacementResult::Success();
}

bool FDACityGridSubsystem::ReserveFootprint(
    const FDAWorldAssetId& AssetId,
    const FIntPoint& Origin,
    const FIntPoint& Footprint,
    const EGridRotation Rotation)
{
    if (!AssetId.IsValid())
    {
        return false;
    }

    FDACardPlacementRequest Request;
    Request.AssetId = AssetId;
    Request.Origin = Origin;
    Request.Footprint = Footprint;
    Request.Rotation = Rotation;

    if (!ValidatePlacement(Request).bCanPlace)
    {
        return false;
    }

    const FIntPoint RotatedFootprint = GetRotatedFootprint(Footprint, Rotation);
    for (int32 Y = 0; Y < RotatedFootprint.Y; ++Y)
    {
        for (int32 X = 0; X < RotatedFootprint.X; ++X)
        {
            Cells[GetCellIndex(Origin + FIntPoint(X, Y))].Occupant = AssetId;
        }
    }

    return true;
}

void FDACityGridSubsystem::ReleaseFootprint(const FDAWorldAssetId& AssetId)
{
    for (FDAGridCell& Cell : Cells)
    {
        if (Cell.Occupant == AssetId)
        {
            Cell.Occupant = FDAWorldAssetId();
        }
    }
}

bool FDACityGridSubsystem::SetCellClaimed(const FIntPoint& Cell, const bool bClaimed)
{
    if (!IsInBounds(Cell))
    {
        return false;
    }

    Cells[GetCellIndex(Cell)].bClaimed = bClaimed;
    return true;
}

void FDACityGridSubsystem::SetAllCellsClaimed(const bool bClaimed)
{
    for (FDAGridCell& Cell : Cells)
    {
        Cell.bClaimed = bClaimed;
    }
}

bool FDACityGridSubsystem::SetCellLayerMask(const FIntPoint& Cell, const uint8 LayerMask)
{
    if (!IsInBounds(Cell))
    {
        return false;
    }

    Cells[GetCellIndex(Cell)].LayerMask = LayerMask;
    return true;
}

bool FDACityGridSubsystem::SetCellRoad(const FIntPoint& Cell, const bool bRoad)
{
    if (!IsInBounds(Cell))
    {
        return false;
    }

    Cells[GetCellIndex(Cell)].bRoad = bRoad;
    return true;
}

FDAWorldAssetId FDACityGridSubsystem::GetOccupant(const FIntPoint& Cell) const
{
    return IsInBounds(Cell) ? Cells[GetCellIndex(Cell)].Occupant : FDAWorldAssetId();
}

bool FDACityGridSubsystem::IsInBounds(const FIntPoint& Cell) const
{
    return Cell.X >= 0 && Cell.X < Width && Cell.Y >= 0 && Cell.Y < Height;
}

int32 FDACityGridSubsystem::GetCellIndex(const FIntPoint& Cell) const
{
    return Cell.Y * Width + Cell.X;
}

FIntPoint FDACityGridSubsystem::GetRotatedFootprint(const FIntPoint& Footprint, const EGridRotation Rotation) const
{
    switch (Rotation)
    {
    case EGridRotation::Ninety:
    case EGridRotation::TwoSeventy:
        return FIntPoint(Footprint.Y, Footprint.X);

    case EGridRotation::Zero:
    case EGridRotation::OneEighty:
    default:
        return Footprint;
    }
}

bool FDACityGridSubsystem::HasRoadAccess(const FIntPoint& Origin, const FIntPoint& RotatedFootprint) const
{
    for (int32 Y = -1; Y <= RotatedFootprint.Y; ++Y)
    {
        for (int32 X = -1; X <= RotatedFootprint.X; ++X)
        {
            const bool bCorner = (X < 0 || X >= RotatedFootprint.X) && (Y < 0 || Y >= RotatedFootprint.Y);
            if (bCorner)
            {
                continue;
            }

            const FIntPoint CellPosition = Origin + FIntPoint(X, Y);
            if (IsInBounds(CellPosition) && Cells[GetCellIndex(CellPosition)].bRoad)
            {
                return true;
            }
        }
    }

    return false;
}
