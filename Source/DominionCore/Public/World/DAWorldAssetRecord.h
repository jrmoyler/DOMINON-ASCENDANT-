#pragma once

#include "CoreMinimal.h"

#include "DAWorldAssetRecord.generated.h"

UENUM(BlueprintType)
enum class EDAConstructionState : uint8
{
    Preview,
    Foundation,
    Frame,
    Shell,
    Systems,
    Operational,
    Damaged,
    Disabled,
    Ruined,
    Reconstructing
};

// Authoritative persistent world state. Runtime actors are reconstructed from this record and are never saved.
USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAWorldAssetRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    FGuid WorldAssetId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    FGuid CardInstanceId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    FName CardDefinitionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location")
    FName CityId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location")
    FIntPoint GridOrigin = FIntPoint::ZeroValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location")
    uint8 Rotation = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction")
    EDAConstructionState ConstructionState = EDAConstructionState::Preview;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction")
    int32 ConstructionCyclesCompleted = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction")
    int32 ConstructionCyclesRequired = 0;

    /** Fractional deterministic progress supports authored construction-speed modifiers. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction")
    float ConstructionProgressCycles = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
    float StructuralIntegrity = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ownership")
    FName OwnerCivilizationId;
};
