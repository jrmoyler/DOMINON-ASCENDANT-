#include "City/DACityGridSubsystem.h"
#include "Misc/AutomationTest.h"

BEGIN_DEFINE_SPEC(FDACityGridSpec, "Dominion.Simulation.CityGrid",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FDACityGridSpec)

namespace
{
    FDAWorldAssetId MakeAssetId(const uint32 A, const uint32 B, const uint32 C, const uint32 D)
    {
        return FDAWorldAssetId(FGuid(A, B, C, D));
    }

    FDACityGridSubsystem MakeClaimedGrid()
    {
        FDACityGridSubsystem Grid(32, 32);
        Grid.SetAllCellsClaimed(true);
        return Grid;
    }

    FDACardPlacementRequest MakePlacementRequest(
        const FDAWorldAssetId& AssetId,
        const FIntPoint& Origin,
        const FIntPoint& Footprint,
        const EGridRotation Rotation)
    {
        FDACardPlacementRequest Request;
        Request.AssetId = AssetId;
        Request.Origin = Origin;
        Request.Footprint = Footprint;
        Request.Rotation = Rotation;
        return Request;
    }
}

void FDACityGridSpec::Define()
{
    It("accepts a claimed 2 by 2 footprint at the origin", [this]()
    {
        FDACityGridSubsystem Grid = MakeClaimedGrid();
        const FDAWorldAssetId AssetId = MakeAssetId(1, 0, 0, 0);

        const FDAPlacementResult Result = Grid.ValidatePlacement(MakePlacementRequest(AssetId, FIntPoint(0, 0), FIntPoint(2, 2), EGridRotation::Zero));

        TestTrue("Origin footprint is valid", Result.bCanPlace);
        TestTrue("Success has no failure code", Result.FailureReason == EDAPlacementFailureReason::None);
    });

    It("rejects a 2 by 2 footprint that extends beyond a 32 by 32 grid", [this]()
    {
        FDACityGridSubsystem Grid = MakeClaimedGrid();
        const FDAPlacementResult Result = Grid.ValidatePlacement(MakePlacementRequest(MakeAssetId(2, 0, 0, 0), FIntPoint(31, 31), FIntPoint(2, 2), EGridRotation::Zero));

        TestFalse("Corner footprint is invalid", Result.bCanPlace);
        TestTrue("Bounds failure is machine-readable", Result.FailureReason == EDAPlacementFailureReason::OutOfBounds);
    });

    It("rejects an overlapping reservation without changing the original owner", [this]()
    {
        FDACityGridSubsystem Grid = MakeClaimedGrid();
        const FDAWorldAssetId FirstAsset = MakeAssetId(3, 0, 0, 0);
        const FDAWorldAssetId SecondAsset = MakeAssetId(4, 0, 0, 0);

        TestTrue("First reservation succeeds", Grid.ReserveFootprint(FirstAsset, FIntPoint(5, 5), FIntPoint(2, 2), EGridRotation::Zero));

        const FDAPlacementResult Result = Grid.ValidatePlacement(MakePlacementRequest(SecondAsset, FIntPoint(6, 6), FIntPoint(1, 1), EGridRotation::Zero));

        TestFalse("Overlapping reservation is invalid", Result.bCanPlace);
        TestTrue("Overlap failure is machine-readable", Result.FailureReason == EDAPlacementFailureReason::Occupied);
        TestTrue("Original reservation keeps ownership", Grid.GetOccupant(FIntPoint(6, 6)) == FirstAsset);
    });

    It("rotates a 1 by 2 footprint into the expected cells", [this]()
    {
        FDACityGridSubsystem Grid = MakeClaimedGrid();
        const FDAWorldAssetId AssetId = MakeAssetId(5, 0, 0, 0);

        TestTrue("Rotated reservation succeeds", Grid.ReserveFootprint(AssetId, FIntPoint(10, 10), FIntPoint(1, 2), EGridRotation::Ninety));

        TestTrue("First horizontal cell is occupied", Grid.GetOccupant(FIntPoint(10, 10)) == AssetId);
        TestTrue("Second horizontal cell is occupied", Grid.GetOccupant(FIntPoint(11, 10)) == AssetId);
        TestTrue("Unrotated vertical cell remains free", Grid.GetOccupant(FIntPoint(10, 11)) == FDAWorldAssetId());
    });

    It("rejects a footprint containing an unclaimed cell", [this]()
    {
        FDACityGridSubsystem Grid = MakeClaimedGrid();
        Grid.SetCellClaimed(FIntPoint(4, 4), false);

        const FDAPlacementResult Result = Grid.ValidatePlacement(MakePlacementRequest(MakeAssetId(6, 0, 0, 0), FIntPoint(4, 4), FIntPoint(1, 1), EGridRotation::Zero));

        TestFalse("Unclaimed cell is invalid", Result.bCanPlace);
        TestTrue("Ownership failure is machine-readable", Result.FailureReason == EDAPlacementFailureReason::Unclaimed);
    });

    It("releases only the footprint owned by the requested asset", [this]()
    {
        FDACityGridSubsystem Grid = MakeClaimedGrid();
        const FDAWorldAssetId FirstAsset = MakeAssetId(7, 0, 0, 0);
        const FDAWorldAssetId SecondAsset = MakeAssetId(8, 0, 0, 0);

        TestTrue("First reservation succeeds", Grid.ReserveFootprint(FirstAsset, FIntPoint(1, 1), FIntPoint(1, 1), EGridRotation::Zero));
        TestTrue("Second reservation succeeds", Grid.ReserveFootprint(SecondAsset, FIntPoint(3, 3), FIntPoint(1, 1), EGridRotation::Zero));

        Grid.ReleaseFootprint(FirstAsset);

        TestTrue("Released cell is free", Grid.GetOccupant(FIntPoint(1, 1)) == FDAWorldAssetId());
        TestTrue("Other owner is preserved", Grid.GetOccupant(FIntPoint(3, 3)) == SecondAsset);
    });

    It("reports terrain, road, and utility requirements with distinct machine-readable reasons", [this]()
    {
        FDACityGridSubsystem Grid = MakeClaimedGrid();

        FDACardPlacementRequest TerrainRequest;
        TerrainRequest.AssetId = MakeAssetId(9, 0, 0, 0);
        TerrainRequest.Origin = FIntPoint(12, 12);
        TerrainRequest.Footprint = FIntPoint(1, 1);
        TerrainRequest.RequiredTerrainLayerMask = 0x01;

        FDACardPlacementRequest RoadRequest;
        RoadRequest.AssetId = MakeAssetId(10, 0, 0, 0);
        RoadRequest.Origin = FIntPoint(14, 14);
        RoadRequest.Footprint = FIntPoint(1, 1);
        RoadRequest.bRequiresRoad = true;

        FDACardPlacementRequest UtilityRequest;
        UtilityRequest.AssetId = MakeAssetId(11, 0, 0, 0);
        UtilityRequest.Origin = FIntPoint(16, 16);
        UtilityRequest.Footprint = FIntPoint(1, 1);
        UtilityRequest.RequiredUtilityLayerMask = 0x02;

        TestTrue("Terrain requirement failure", Grid.ValidatePlacement(TerrainRequest).FailureReason == EDAPlacementFailureReason::TerrainInvalid);
        TestTrue("Road requirement failure", Grid.ValidatePlacement(RoadRequest).FailureReason == EDAPlacementFailureReason::RoadMissing);
        TestTrue("Utility requirement failure", Grid.ValidatePlacement(UtilityRequest).FailureReason == EDAPlacementFailureReason::UtilityUnavailable);
    });
}
