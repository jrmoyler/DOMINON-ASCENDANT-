#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "DAAscensionContent.generated.h"

UCLASS(BlueprintType)
class DOMINIONWORLD_API UDAReplicationDoctrineDefinition final : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    static const FPrimaryAssetType AssetType;
    virtual FPrimaryAssetId GetPrimaryAssetId() const override;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FName DoctrineId;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) int32 CadenceDevelopmentCycles = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString SourceFingerprint;
};

struct DOMINIONWORLD_API FDAFirstAscensionBuildingImport
{
    FString AssetPath;
    FString SourcePath;
    FName AssetClass;
};

struct DOMINIONWORLD_API FDAFirstAscensionContentManifest
{
    int32 SchemaVersion = 0;
    FName ContentId;
    FString Fingerprint;
    FName QuestId;
    FString QuestTitle;
    FString QuestAssetPath;
    FName QuestCompletionHistory;
    FName DoctrineId;
    FString DoctrineAssetPath;
    int32 ReplicationCadenceDevelopmentCycles = 0;
    FName FusionDefinitionId;
    FString FusionAssetPath;
    int32 ConstructionCycles = 0;
    int32 CraftCapital = 0;
    int32 CraftInsight = 0;
    FName RequiredCraftingFacilityId;
    int32 UtilityPower = 0;
    int32 UtilityData = 0;
    float WorkforceRequirementModifier = 0.f;
    float IndustrialThroughputModifier = 0.f;
    float AdjacentIndustrialConstructionSpeedModifier = 0.f;
    float DependencyPerCycle = 0.f;
    float ResourceHungerPerCycle = 0.f;
    TArray<FName> ForgeweaveDefinitionIds;
    TArray<FDAFirstAscensionBuildingImport> BuildingImports;
    FString CinematicAssetPath;
    FString CinematicShotSourcePath;
    bool bCinematicGameplayGate = true;
};

/** Strict frozen manifest authority shared by generation and ValidateOnly coverage. */
class DOMINIONWORLD_API FDAFirstAscensionContentPipeline
{
public:
    static FString GetCanonicalManifestPath();
    static bool LoadFile(const FString& Filename, FDAFirstAscensionContentManifest& OutManifest,
        TArray<FText>& Errors);
    static bool ParseJson(const FString& Json, FDAFirstAscensionContentManifest& OutManifest,
        TArray<FText>& Errors);
    static bool Validate(const FDAFirstAscensionContentManifest& Manifest,
        TArray<FText>& Errors);
};
