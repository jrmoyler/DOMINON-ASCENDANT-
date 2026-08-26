#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "DAContentTypes.generated.h"

UENUM(BlueprintType)
enum class EDACardType : uint8
{
    Land,
    Residential,
    Production,
    Civic,
    Utility,
    Military,
    Wonder,
    Support,
    Retail,
    Office,
    Research,
    Industrial,
    Infrastructure,
    Defense,
    Unit,
    Leader,
    Special
};

UENUM(BlueprintType)
enum class EDARarity : uint8
{
    Common,
    Specialized,
    Rare,
    Epic,
    Dominion,
    Elite,
    Legendary,
    Mythic,
    Wonder,
    Leader
};

/** Presence bits for authored gameplay values. Raw zero is never presence. */
UENUM()
enum class EDAAuthoredCardValue : uint8
{
    DeploymentCapital,
    DeploymentInsight,
    DeploymentInfluence,
    CraftCapital,
    CraftInsight,
    CraftProductionThroughput,
    RequiredCraftingFacilityId,
    MaintenanceCapitalPerCycle,
    BaseCapitalPerCycle,
    BaseInsightPerCycle,
    BaseInfluencePerCycle,
    SynaraDependencyPerCycle,
    ForgeweaveResourceHungerPerCycle,
    WorkforceRequirementModifier,
    IndustrialThroughputModifier,
    AdjacentIndustrialConstructionSpeedModifier,
    ConstructionCycles,
    UtilityPower,
    UtilityWater,
    UtilityData,
    HousingCapacity
};

constexpr uint64 DAAuthoredValueBit(const EDAAuthoredCardValue Value)
{
    return uint64(1) << static_cast<uint8>(Value);
}

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDACardUtilityDemand
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    int32 Power = 0;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    int32 Water = 0;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    int32 Data = 0;

    bool HasNegativeDemand() const
    {
        return Power < 0 || Water < 0 || Data < 0;
    }
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDACombatDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    float StructuralIntegrity = 0.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    float Armor = 0.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    float CyberIntegrity = 0.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    bool bCapturable = false;
};
