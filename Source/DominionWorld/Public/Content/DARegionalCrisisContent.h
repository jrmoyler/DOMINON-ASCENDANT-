#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "World/DARegionalWorldState.h"

#include "DARegionalCrisisContent.generated.h"

USTRUCT(BlueprintType)
struct DOMINIONWORLD_API FDARegionalCrisisStageDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName StageId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 DurationWorldTicks = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) double PriceModifier = 0.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> NextStageIds;
};

USTRUCT(BlueprintType)
struct DOMINIONWORLD_API FDARegionalCrisisResolutionDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ResolutionId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RecoveryWorldTicks = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) double MarketModifier = 0.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int64 TradeRouteCapacityDelta = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) double EcologyDelta = 0.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName RelationshipId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDADiplomaticMetric RelationshipMetric = EDADiplomaticMetric::Trust;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float RelationshipDelta = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ResourceHungerDelta = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> HistoryTags;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FName, FName> CitizenOutcomes;
};

USTRUCT(BlueprintType)
struct DOMINIONWORLD_API FDARegionalWorldEventEntry
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName EventId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Title;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString AssetPath;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Scope;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName TriggerMetric;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName TriggerComparison;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) double TriggerThreshold = 0.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 WarningDurationWorldTicks = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName InitialStageId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName IgnoredResolutionId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FDARegionalCrisisStageDefinition> Stages;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> Systems;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FDARegionalCrisisResolutionDefinition> Resolutions;
    const FDARegionalCrisisResolutionDefinition* FindResolution(FName ResolutionId) const;
};

USTRUCT(BlueprintType)
struct DOMINIONWORLD_API FDARegionalQuestEdgeEntry
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Branch;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Target;
};

USTRUCT(BlueprintType)
struct DOMINIONWORLD_API FDARegionalQuestNodeEntry
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName NodeId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName NodeType;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FDARegionalQuestEdgeEntry> Edges;
};

USTRUCT(BlueprintType)
struct DOMINIONWORLD_API FDARegionalQuestEntry
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName QuestId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Title;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString AssetPath;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> CitizenIds;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Trigger;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> Choices;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> Systems;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> OutcomeTags;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FName, FName> ChoiceOutcomeTags;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> DialogueConditions;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FDARegionalQuestNodeEntry> Nodes;
};

struct DOMINIONWORLD_API FDARegionalCrisisManifest
{
    int32 SchemaVersion = 0;
    FName CampaignId;
    FString Fingerprint;
    FString SemanticFingerprint;
    TArray<FDARegionalWorldEventEntry> Events;
    TArray<FDARegionalQuestEntry> Quests;
    const FDARegionalWorldEventEntry* FindEvent(FName EventId) const;
    const FDARegionalQuestEntry* FindQuest(FName QuestId) const;
};

UCLASS(BlueprintType)
class DOMINIONWORLD_API UDARegionalWorldEventDefinition final : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FDARegionalWorldEventEntry Event;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString SourceFingerprint;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly) bool bRuntimeManifestFallback = false;
    virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};

UCLASS(BlueprintType)
class DOMINIONWORLD_API UDARegionalQuestDefinition final : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FDARegionalQuestEntry Quest;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString SourceFingerprint;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly) bool bRuntimeManifestFallback = false;
    virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};

class DOMINIONWORLD_API FDARegionalCrisisPipeline
{
public:
    static FString GetCanonicalManifestPath();
    static bool LoadCanonical(FDARegionalCrisisManifest& OutManifest, TArray<FText>& Errors);
    static bool LoadFile(const FString& Filename, FDARegionalCrisisManifest& OutManifest, TArray<FText>& Errors);
    static bool ParseJson(const FString& Json, FDARegionalCrisisManifest& OutManifest, TArray<FText>& Errors);
    static bool Validate(const FDARegionalCrisisManifest& Manifest, TArray<FText>& Errors);
    static bool BuildAssets(const FDARegionalCrisisManifest& Manifest,
        TArray<UDARegionalWorldEventDefinition*>& OutEvents,
        TArray<UDARegionalQuestDefinition*>& OutQuests, TArray<FText>& Errors);
    static bool ValidateGeneratedCache(const FDARegionalCrisisManifest& Manifest,
        const TArray<UDARegionalWorldEventDefinition*>& Events,
        const TArray<UDARegionalQuestDefinition*>& Quests, TArray<FText>& Errors);
};
