#pragma once

#include "Content/DAContentTypes.h"
#include "Engine/DataAsset.h"

#include "DACardDefinition.generated.h"

class AActor;
class UTexture2D;

UCLASS(BlueprintType)
class DOMINIONCORE_API UDA_CardDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    static const FPrimaryAssetType AssetType;

    virtual FPrimaryAssetId GetPrimaryAssetId() const override;

    bool Validate(TArray<FText>& Errors) const;
    bool HasAuthoredValue(EDAAuthoredCardValue Value) const;

    bool TryGetDeploymentCapital(int32& OutValue) const;
    bool TryGetDeploymentInsight(int32& OutValue) const;
    bool TryGetDeploymentInfluence(int32& OutValue) const;
    bool TryGetCraftCapital(int32& OutValue) const;
    bool TryGetCraftInsight(int32& OutValue) const;
    bool TryGetCraftProductionThroughput(int32& OutValue) const;
    bool TryGetRequiredCraftingFacilityId(FName& OutValue) const;
    bool TryGetMaintenanceCapitalPerCycle(float& OutValue) const;
    bool TryGetBaseCapitalPerCycle(float& OutValue) const;
    bool TryGetBaseInsightPerCycle(float& OutValue) const;
    bool TryGetBaseInfluencePerCycle(float& OutValue) const;
    bool TryGetSynaraDependencyPerCycle(float& OutValue) const;
    bool TryGetForgeweaveResourceHungerPerCycle(float& OutValue) const;
    bool TryGetWorkforceRequirementModifier(float& OutValue) const;
    bool TryGetIndustrialThroughputModifier(float& OutValue) const;
    bool TryGetAdjacentIndustrialConstructionSpeedModifier(float& OutValue) const;
    bool TryGetConstructionCycles(int32& OutValue) const;
    bool TryGetUtilityPower(int32& OutValue) const;
    bool TryGetUtilityWater(int32& OutValue) const;
    bool TryGetUtilityData(int32& OutValue) const;
    bool TryGetHousingCapacity(int32& OutValue) const;
    bool TryGetCombatDefinition(FDACombatDefinition& OutValue) const;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
    FName DefinitionId;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
    FText DisplayName;

    UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Identity")
    FString SourceManifestFingerprint;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
    FPrimaryAssetId CivilizationId;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
    EDACardType CardType = EDACardType::Land;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
    EDARarity Rarity = EDARarity::Common;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement")
    FIntPoint Footprint = FIntPoint::ZeroValue;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement")
    bool bPlaceable = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Representation")
    TSoftClassPtr<AActor> WorldPrefab;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Representation")
    TSoftObjectPtr<UTexture2D> CardArt;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
    TArray<FGameplayTag> Tags;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrades")
    TArray<FName> UpgradeBranchIds;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Acquisition")
    bool bRandomCacheEligible = false;

private:
    friend class FDAContentManifestPipeline;

    bool TryGetIntValue(EDAAuthoredCardValue Field, int32 RawValue, int32& OutValue) const;
    bool TryGetFloatValue(EDAAuthoredCardValue Field, float RawValue, float& OutValue) const;

    UPROPERTY(EditDefaultsOnly, Category = "Economy|Authorship", meta = (AllowPrivateAccess = "true"))
    uint64 AuthoredValueMask = 0;

    UPROPERTY(EditDefaultsOnly, Category = "Economy|Raw", meta = (AllowPrivateAccess = "true"))
    int32 DeploymentCapital = 0;
    UPROPERTY(EditDefaultsOnly, Category = "Economy|Raw", meta = (AllowPrivateAccess = "true"))
    int32 DeploymentInsight = 0;
    UPROPERTY(EditDefaultsOnly, Category = "Economy|Raw", meta = (AllowPrivateAccess = "true"))
    int32 DeploymentInfluence = 0;
    UPROPERTY(EditDefaultsOnly, Category = "Economy|Raw", meta = (AllowPrivateAccess = "true"))
    int32 CraftCapital = 0;
    UPROPERTY(EditDefaultsOnly, Category = "Economy|Raw", meta = (AllowPrivateAccess = "true"))
    int32 CraftInsight = 0;
    UPROPERTY(EditDefaultsOnly, Category = "Economy|Raw", meta = (AllowPrivateAccess = "true"))
    int32 CraftProductionThroughput = 0;
    UPROPERTY(EditDefaultsOnly, Category = "Economy|Raw", meta = (AllowPrivateAccess = "true"))
    FName RequiredCraftingFacilityId;
    UPROPERTY(EditDefaultsOnly, Category = "Economy|Raw", meta = (AllowPrivateAccess = "true"))
    float MaintenanceCapitalPerCycle = 0.f;
    UPROPERTY(EditDefaultsOnly, Category = "Economy|Raw", meta = (AllowPrivateAccess = "true"))
    float BaseCapitalPerCycle = 0.f;
    UPROPERTY(EditDefaultsOnly, Category = "Economy|Raw", meta = (AllowPrivateAccess = "true"))
    float BaseInsightPerCycle = 0.f;
    UPROPERTY(EditDefaultsOnly, Category = "Economy|Raw", meta = (AllowPrivateAccess = "true"))
    float BaseInfluencePerCycle = 0.f;
    UPROPERTY(EditDefaultsOnly, Category = "Economy|Raw", meta = (AllowPrivateAccess = "true"))
    float SynaraDependencyPerCycle = 0.f;
    UPROPERTY(EditDefaultsOnly, Category = "Economy|Raw", meta = (AllowPrivateAccess = "true"))
    float ForgeweaveResourceHungerPerCycle = 0.f;
    UPROPERTY(EditDefaultsOnly, Category = "Economy|Raw", meta = (AllowPrivateAccess = "true"))
    float WorkforceRequirementModifier = 0.f;
    UPROPERTY(EditDefaultsOnly, Category = "Economy|Raw", meta = (AllowPrivateAccess = "true"))
    float IndustrialThroughputModifier = 0.f;
    UPROPERTY(EditDefaultsOnly, Category = "Economy|Raw", meta = (AllowPrivateAccess = "true"))
    float AdjacentIndustrialConstructionSpeedModifier = 0.f;
    UPROPERTY(EditDefaultsOnly, Category = "Economy|Raw", meta = (AllowPrivateAccess = "true"))
    int32 ConstructionCycles = 0;
    UPROPERTY(EditDefaultsOnly, Category = "Utilities|Raw", meta = (AllowPrivateAccess = "true"))
    FDACardUtilityDemand UtilityDemand;
    UPROPERTY(EditDefaultsOnly, Category = "Population|Raw", meta = (AllowPrivateAccess = "true"))
    int32 HousingCapacity = 0;
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Authorship", meta = (AllowPrivateAccess = "true"))
    bool bHasAuthoredCombat = false;
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Raw", meta = (AllowPrivateAccess = "true"))
    FDACombatDefinition Combat;
};
