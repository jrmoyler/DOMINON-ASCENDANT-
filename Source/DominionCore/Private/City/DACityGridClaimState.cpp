#include "City/DACityGridClaimState.h"

FDACityGridClaimState FDACityGridClaimState::MakeCanonicalPlayerCapital()
{
    FDACityGridClaimState Result;
    Result.CityId = TEXT("player_capital");
    Result.ClaimedCells.Reserve(Result.Width * Result.Height);
    for (int32 Y = 0; Y < Result.Height; ++Y)
        for (int32 X = 0; X < Result.Width; ++X)
            Result.ClaimedCells.Add(FIntPoint(X, Y));
    return Result;
}

bool FDACityGridClaimState::Contains(const FIntPoint Cell) const
{
    return ClaimedCells.Contains(Cell);
}

bool FDACityGridClaimState::Validate(FString& OutError) const
{
    if (CityId.IsNone() || Width != FDACityGridMetadata::Width || Height != FDACityGridMetadata::Height)
    {
        OutError = TEXT("City grid claims require a canonical city identity and authored grid dimensions.");
        return false;
    }
    TSet<FIntPoint> Unique;
    int32 PreviousIndex = INDEX_NONE;
    for (const FIntPoint Cell : ClaimedCells)
    {
        const int32 Index = Cell.Y * Width + Cell.X;
        if (Cell.X < 0 || Cell.Y < 0 || Cell.X >= Width || Cell.Y >= Height
            || Unique.Contains(Cell) || Index <= PreviousIndex)
        {
            OutError = TEXT("City grid claims must be bounded, unique, and deterministically row-major ordered.");
            return false;
        }
        Unique.Add(Cell);
        PreviousIndex = Index;
    }
    return true;
}
