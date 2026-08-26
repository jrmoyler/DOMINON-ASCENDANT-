#pragma once

#include "CoreMinimal.h"

#include "DACardInstance.generated.h"

UENUM(BlueprintType)
enum class EDAAcquisitionSource : uint8
{
    StarterDeck,
    QuestReward,
    WorldDiscovery,
    Crafting,
    Replication,
    Conquest
};

UENUM(BlueprintType)
enum class EDARecoveryState : uint8
{
    Available,
    Deployed,
    Ruined,
    RecoveryPool
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAUpgradeState
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 Level = 1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FName SelectedBranchId;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FCardInstance
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FGuid InstanceId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FName DefinitionId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int64 AcquisitionWorldTick = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    EDAAcquisitionSource AcquisitionSource = EDAAcquisitionSource::StarterDeck;

    /** Valid only for Replication acquisitions; exact source-instance provenance. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FGuid SourceCardInstanceId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 MasteryXp = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FDAUpgradeState UpgradeState;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FName CosmeticVariantId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    EDARecoveryState RecoveryState = EDARecoveryState::Available;

    // An invalid GUID means this instance is not currently represented by a world asset.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FGuid WorldAssetId;
};
