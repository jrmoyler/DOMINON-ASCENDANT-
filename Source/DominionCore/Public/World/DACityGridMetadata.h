#pragma once

#include "CoreMinimal.h"

/** Canonical logical-grid metadata shared by simulation, persistence, and reconstruction. */
struct DOMINIONCORE_API FDACityGridMetadata
{
    static constexpr int32 Width = 32;
    static constexpr int32 Height = 32;
    static constexpr float CellSizeMeters = 8.f;
    static constexpr float UnrealUnitsPerMeter = 100.f;

    static FVector CellToWorld(const FIntPoint Cell)
    {
        return FVector(
            static_cast<float>(Cell.X) * CellSizeMeters * UnrealUnitsPerMeter,
            static_cast<float>(Cell.Y) * CellSizeMeters * UnrealUnitsPerMeter,
            0.f);
    }
};
