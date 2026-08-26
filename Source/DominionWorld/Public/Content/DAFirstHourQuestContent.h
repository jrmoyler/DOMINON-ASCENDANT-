#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Narrative/DAQuestDefinition.h"

#include "DAFirstHourQuestContent.generated.h"

UENUM(BlueprintType)
enum class EDAFirstHourBindWhen : uint8 { Start, Objective };

UENUM(BlueprintType)
enum class EDAFirstHourStartCondition : uint8
{
    NewCampaign, PrerequisitesMet, HabitatOccupied, AutonomousExchangeAvailable,
    DependencyAbove25, SixPlayerBuildings, WorldMapUnlocked, ForgeweaveContact
};

UENUM(BlueprintType)
enum class EDAFirstHourNodeCondition : uint8
{
    NewCampaign, PrerequisitesMet, FounderReached, FounderHallPowered, FounderHallOnline,
    CustodianMarkingsInspected, AdaptiveHabitatInspected, AdaptiveHabitatPlaced,
    HabitatConstructionComplete, BoundAssetOperational, NiaPresent, HabitatOccupied,
    UtilityExpansionAcknowledged, HabitatPowerFullySupplied, HabitatWaterFullySupplied, NiaSpokenTo, TowerHalfStaffed,
    NiaAssignedToTower, AutonomousExchangeAvailable, ReplacementModelDiscovered,
    ExplicitChoice, DependencyAbove25, WorkersRetrained, AutomationCapEnacted,
    WorldMapUnlocked, ForgeweaveContact, EnteredUtilityTunnel, AncientNodeRestored,
    MaintenanceDronesDefeated, UnknownSymbolInspected, EdenBasinReached,
    WaterQualityInspected, AmaraSpokenTo, OriSpokenTo, Resolved
};

UENUM(BlueprintType)
enum class EDAFirstHourRewardType : uint8
{
    CityMode, CardInstance, Blueprint, UtilitySystems, OperatorXp, InsightReward,
    IntelligenceAuditorPath, AxiomArchiveFragment, DiplomacyContact, EdenTradeAccess
};

UENUM(BlueprintType)
enum class EDAFirstHourEligibility : uint8 { Vision, ResearchAction };

USTRUCT(BlueprintType)
struct DOMINIONWORLD_API FDAFirstHourWorldAssetRequirement
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName BindingId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName DefinitionId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDAFirstHourBindWhen BindWhen = EDAFirstHourBindWhen::Start;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName OwnerId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bRequireOperational = false;
};

USTRUCT(BlueprintType)
struct DOMINIONWORLD_API FDAFirstHourReward
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ActionId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDAFirstHourRewardType Type = EDAFirstHourRewardType::CityMode;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName DefinitionId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ContentId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Quantity = 1;
};

USTRUCT(BlueprintType)
struct DOMINIONWORLD_API FDAFirstHourOutcome
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ActionId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> HistoryTags;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName StoryState;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> SemanticEffects;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<EDAFirstHourEligibility> EligibilityAny;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bHasDependencyDelta = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 DependencyDelta = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bHasHumanAgencySupportDelta = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 HumanAgencySupportDelta = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bHasNiaTrustDelta = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 NiaTrustDelta = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FDAFirstHourReward> Rewards;
};

USTRUCT(BlueprintType)
struct DOMINIONWORLD_API FDAFirstHourNode
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FDAQuestNodeDefinition RuntimeNode;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDAFirstHourNodeCondition Condition = EDAFirstHourNodeCondition::Resolved;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName BindingId;
};

USTRUCT(BlueprintType)
struct DOMINIONWORLD_API FDAFirstHourQuestEntry
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Title;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString AssetPath;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> PrerequisiteQuestIds;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDAFirstHourStartCondition StartCondition = EDAFirstHourStartCondition::PrerequisitesMet;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FDAFirstHourWorldAssetRequirement> WorldAssetBindings;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FDAFirstHourNode> Nodes;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> CompletionHistoryTags;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FDAFirstHourReward> Rewards;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FDAQuestDefinition Definition;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FName, FDAFirstHourOutcome> Outcomes;

    const FDAFirstHourNode* FindNode(FName NodeId) const;
    const FDAFirstHourWorldAssetRequirement* FindBinding(FName BindingId) const;
};

USTRUCT(BlueprintType)
struct DOMINIONWORLD_API FDAChampionEligibilityDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName RequiredStoryState;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName RequiredCrisisQuestId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName RequiredCrisisSourceDefinitionId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString RequiredCrisisDefinitionFingerprint;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName RequiredCrisisCompletionActionId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ChampionDefinitionId;
};

USTRUCT(BlueprintType)
struct DOMINIONWORLD_API FDAFirstHourCitizenDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CitizenId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString AssetPath;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Origin;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Occupation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString StartingClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FString> Traits;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName TowerJobId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FDAChampionEligibilityDefinition ChampionEligibility;
};

struct DOMINIONWORLD_API FDAFirstHourQuestManifest
{
    int32 SchemaVersion = 0;
    FName CampaignId;
    FString Fingerprint;
    FString SemanticFingerprint;
    TArray<FDAFirstHourQuestEntry> Quests;
    FDAFirstHourCitizenDefinition Citizen;
    const FDAFirstHourQuestEntry* FindQuest(FName QuestId) const;
};

UCLASS(BlueprintType)
class DOMINIONWORLD_API UDA_FirstHourQuestDefinition final : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FDAFirstHourQuestEntry Quest;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString SourceFingerprint;
    /** True only for the transient canonical-manifest fallback; never valid as generated cache. */
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly) bool bRuntimeManifestFallback = false;
    virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};

UCLASS(BlueprintType)
class DOMINIONWORLD_API UDA_CitizenDefinition final : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FDAFirstHourCitizenDefinition Citizen;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString SourceFingerprint;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly) bool bRuntimeManifestFallback = false;
    virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};

struct DOMINIONWORLD_API FDABuiltFirstHourContent
{
    TArray<UDA_FirstHourQuestDefinition*> Quests;
    UDA_CitizenDefinition* Citizen = nullptr;
};

/** Strict single parser shared by Automation, runtime fallback, and the editor generator. */
class DOMINIONWORLD_API FDAFirstHourQuestPipeline
{
public:
    static FString GetCanonicalManifestPath();
    static bool LoadCanonical(FDAFirstHourQuestManifest& OutManifest, TArray<FText>& Errors);
    static bool LoadFile(const FString& Filename, FDAFirstHourQuestManifest& OutManifest, TArray<FText>& Errors);
    static bool ParseJson(const FString& Json, FDAFirstHourQuestManifest& OutManifest, TArray<FText>& Errors);
    static bool Validate(const FDAFirstHourQuestManifest& Manifest, TArray<FText>& Errors);
    static bool BuildAssets(const FDAFirstHourQuestManifest& Manifest, TArray<UDA_FirstHourQuestDefinition*>& OutQuests,
        UDA_CitizenDefinition*& OutCitizen, TArray<FText>& Errors);
    static bool ValidateGeneratedCache(const FDAFirstHourQuestManifest& Manifest,
        const TArray<UDA_FirstHourQuestDefinition*>& CandidateQuests, const UDA_CitizenDefinition* CandidateCitizen,
        TArray<FText>& Errors);
    static bool BuildRuntimeContent(const FDAFirstHourQuestManifest& Manifest,
        const TArray<UDA_FirstHourQuestDefinition*>& CandidateQuests, UDA_CitizenDefinition* CandidateCitizen,
        FDABuiltFirstHourContent& OutContent, TArray<FText>& Errors);
};
