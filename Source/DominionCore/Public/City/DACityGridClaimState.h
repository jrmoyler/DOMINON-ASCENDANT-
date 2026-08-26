#pragma once

#include "CoreMinimal.h"
#include "World/DACityGridMetadata.h"

#include "DACityGridClaimState.generated.h"

/** Persisted canonical claim mask used to reconstruct a player city grid after reload. */
USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDACityGridClaimState
{
    GENERATED_BODY()

    static FDACityGridClaimState MakeCanonicalPlayerCapital();
    bool Contains(FIntPoint Cell) const;
    bool Validate(FString& OutError) const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Grid")
    FName CityId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Grid")
    int32 Width = FDACityGridMetadata::Width;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Grid")
    int32 Height = FDACityGridMetadata::Height;

    /** Row-major, unique, explicitly persisted cells. No implicit claim-all fallback exists. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Grid")
    TArray<FIntPoint> ClaimedCells;
};
