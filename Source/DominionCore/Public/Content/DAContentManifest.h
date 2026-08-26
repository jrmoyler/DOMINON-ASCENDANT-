#pragma once

#include "Cards/DACollectionState.h"
#include "Cards/DADeckState.h"
#include "CoreMinimal.h"
#include "Content/DAContentTypes.h"

class UDA_CardDefinition;
class UDA_DeckDefinition;

enum class EDAContentFaction : uint8
{
    Synara,
    Forgeweave,
    EdenCircuit,
    Universal,
    Fusion,
    Special
};

struct DOMINIONCORE_API FDAManifestCardDefinition
{
    FName DefinitionId;
    FString DisplayName;
    EDAContentFaction Faction = EDAContentFaction::Universal;
    EDACardType CardType = EDACardType::Land;
    EDARarity Rarity = EDARarity::Common;
    FIntPoint Footprint = FIntPoint::ZeroValue;
    bool bPlaceable = false;
    FString WorldPrefabClassPath;
    bool bRandomCacheEligible = false;
    TArray<FName> Tags;
    TArray<FName> UpgradeBranchIds;
    bool bTagsProvided = false;
    bool bUpgradeBranchIdsProvided = false;
    bool bHasAuthoredCombat = false;
    bool bCombatProvided = false;
    FDACombatDefinition Combat;
    uint64 AuthoredValueMask = 0;
    uint64 ProvidedValueMask = 0;
    int32 DeploymentCapital = 0;
    int32 DeploymentInsight = 0;
    int32 DeploymentInfluence = 0;
    int32 CraftCapital = 0;
    int32 CraftInsight = 0;
    int32 CraftProductionThroughput = 0;
    int32 ConstructionCycles = 0;
    FName RequiredCraftingFacilityId;
    float MaintenanceCapitalPerCycle = 0.f;
    float BaseCapitalPerCycle = 0.f;
    float BaseInsightPerCycle = 0.f;
    float BaseInfluencePerCycle = 0.f;
    float SynaraDependencyPerCycle = 0.f;
    float ForgeweaveResourceHungerPerCycle = 0.f;
    float WorkforceRequirementModifier = 0.f;
    float IndustrialThroughputModifier = 0.f;
    float AdjacentIndustrialConstructionSpeedModifier = 0.f;
    int32 UtilityPower = 0;
    int32 UtilityWater = 0;
    int32 UtilityData = 0;
    int32 HousingCapacity = 0;
};

struct DOMINIONCORE_API FDAManifestDeckEntry
{
    FName DefinitionId;
    int32 Quantity = 0;
};

struct DOMINIONCORE_API FDAVerticalSliceContentManifest
{
    int32 SchemaVersion = 0;
    FString SourceFingerprint;
    TArray<FDAManifestCardDefinition> Definitions;
    TArray<FDAManifestDeckEntry> StarterDeck;
};

struct DOMINIONCORE_API FDABuiltManifestContent
{
    TArray<UDA_CardDefinition*> Definitions;
    FDACollectionState Collection;
    FDADeckState Deck;
    UDA_DeckDefinition* DeckAsset = nullptr;

    UDA_CardDefinition* FindDefinition(FName DefinitionId) const;
};

/** One parser and mapper shared by runtime fallback, automation, and editor generation. */
class DOMINIONCORE_API FDAContentManifestPipeline
{
public:
    static FString GetCanonicalManifestPath();
    static bool LoadCanonical(FDAVerticalSliceContentManifest& OutManifest, TArray<FText>& Errors);
    static bool LoadFile(const FString& Filename, FDAVerticalSliceContentManifest& OutManifest, TArray<FText>& Errors);
    static bool ParseJson(const FString& Json, FDAVerticalSliceContentManifest& OutManifest, TArray<FText>& Errors);
    static bool ValidateManifest(const FDAVerticalSliceContentManifest& Manifest, TArray<FText>& Errors);
    static bool ValidateGeneratedCache(
        const FDAVerticalSliceContentManifest& Manifest,
        const TArray<UDA_CardDefinition*>& CandidateDefinitions,
        const UDA_DeckDefinition* CandidateDeck,
        TArray<FText>& Errors);
    static FString GetGeneratedPackageName(const UDA_CardDefinition& Definition);
    static bool BuildRuntimeContent(
        const FDAVerticalSliceContentManifest& Manifest,
        FDABuiltManifestContent& OutContent,
        TArray<FText>& Errors);
};
