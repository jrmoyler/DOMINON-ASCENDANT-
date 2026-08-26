#pragma once

#include "CoreMinimal.h"

#include "DANarrativeRecords.generated.h"

struct FDAWorldAssetRecord;

UENUM(BlueprintType)
enum class EDAQuestNodeType : uint8
{
    Start, Dialogue, Objective, Investigation, Build, Deliver, Explore, Combat, Defend, Capture,
    Choice, Wait, Timer, WorldCondition, CitizenCondition, FactionCondition, EconomyCondition,
    RelationshipCondition, EventTrigger, Reward, Failure, Resolution
};

UENUM(BlueprintType)
enum class EDAQuestEdgeCondition : uint8
{
    Always,
    WorldAssetAvailable,
    WorldAssetDestroyed
};

UENUM(BlueprintType)
enum class EDAQuestPayloadVariant : uint8
{
    None,
    Timer,
    World,
    WorldAsset,
    Citizen,
    Faction,
    Economy,
    Relationship
};

UENUM(BlueprintType)
enum class EDAQuestComparison : uint8
{
    Equal,
    NotEqual,
    GreaterOrEqual,
    LessOrEqual
};

UENUM(BlueprintType)
enum class EDAWorldEventScope : uint8
{
    Local,
    Regional,
    Global,
    Axiom
};

UENUM(BlueprintType)
enum class EDAQuestProgressState : uint8
{
    Active,
    Completed,
    Failed,
    Abandoned
};

UENUM(BlueprintType)
enum class EDAWorldEventProgressState : uint8
{
    Active,
    Resolved,
    Expired
};

UENUM(BlueprintType)
enum class EDAPromiseState : uint8
{
    Active,
    Fulfilled,
    Breached
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAQuestTimerPayload
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest", meta = (ClampMin = "1"))
    int64 DurationWorldTicks = 1;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAQuestConditionPayload
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    FName EvaluationKey;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    EDAQuestComparison Comparison = EDAQuestComparison::GreaterOrEqual;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    double ExpectedValue = 0.0;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAQuestNodePayload
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    EDAQuestPayloadVariant Variant = EDAQuestPayloadVariant::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    FDAQuestTimerPayload Timer;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    FDAQuestConditionPayload Condition;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    FName WorldAssetBindingId;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAQuestEdgeDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    FName BranchTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    FName TargetNodeId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    EDAQuestEdgeCondition Condition = EDAQuestEdgeCondition::Always;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    FName WorldAssetBindingId;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAQuestNodeDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    FName NodeId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    EDAQuestNodeType Type = EDAQuestNodeType::Objective;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    FName SourceDefinitionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    FDAQuestNodePayload Payload;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    TArray<FDAQuestEdgeDefinition> Edges;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAQuestDefinitionManifest
{
    GENERATED_BODY()

    const FDAQuestNodeDefinition* FindNode(FName NodeId) const;
    bool Validate(FString& OutError) const;
    FString ComputeFingerprint() const;
    FString ComputeLegacyFingerprintV1() const;
    void RefreshFingerprint();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    FName QuestId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    FName SourceDefinitionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest", meta = (ClampMin = "1"))
    int32 Version = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    FName StartNodeId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    TArray<FName> RequiredWorldAssetBindingIds;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    TArray<FDAQuestNodeDefinition> Nodes;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Narrative|Quest")
    FString DefinitionFingerprint;
};

/** Core-owned immutable identity policy; runtime content may verify but never redefine it. */
struct DOMINIONCORE_API FDAFirstHourFrozenPolicy
{
    static const FString& ManifestFingerprint();
    static const TMap<FName, FString>& QuestDefinitionFingerprints();
    static const TMap<FName, FName>& QuestSourceDefinitionIds();
    static bool ValidatePinnedQuestDefinition(const FDAQuestDefinitionManifest& Definition, FString& OutError);
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAWorldEventStageDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Event")
    FName StageId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Event")
    FName SourceDefinitionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Event")
    TArray<FName> AllowedNextStageIds;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Event")
    bool bResolution = false;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAWorldEventDefinitionManifest
{
    GENERATED_BODY()

    const FDAWorldEventStageDefinition* FindStage(FName StageId) const;
    bool Validate(FString& OutError) const;
    FString ComputeFingerprint() const;
    FString ComputeLegacyFingerprintV1() const;
    void RefreshFingerprint();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Event")
    FName EventId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Event")
    FName SourceDefinitionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Event", meta = (ClampMin = "1"))
    int32 Version = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Event")
    EDAWorldEventScope Scope = EDAWorldEventScope::Local;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Event")
    FName InitialStageId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Event")
    TArray<FDAWorldEventStageDefinition> Stages;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Narrative|Event")
    FString DefinitionFingerprint;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAQuestEvaluationContext
{
    GENERATED_BODY()

    bool SetMetric(EDAQuestPayloadVariant Variant, FName Key, double Value);
    bool TryGetMetric(EDAQuestPayloadVariant Variant, FName Key, double& OutValue) const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest", meta = (ClampMin = "0"))
    int64 WorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    TMap<FName, double> WorldValues;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    TMap<FName, double> CitizenValues;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    TMap<FName, double> FactionValues;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    TMap<FName, double> EconomyValues;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    TMap<FName, double> RelationshipValues;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAQuestWorldAssetBinding
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    FName BindingId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    FGuid WorldAssetId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    int64 BindWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    FString QuestDefinitionFingerprint;
};

/** Immutable timing evidence for one authored node transition. */
USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAQuestNodeTransitionRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    FName CompletedNodeId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    FName EnteredNodeId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest", meta = (ClampMin = "0"))
    int64 WorldTick = 0;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAQuestSaveState
{
    GENERATED_BODY()

    const FDAQuestWorldAssetBinding* FindWorldAssetBinding(FName BindingId) const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    FName QuestId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    FDAQuestDefinitionManifest DefinitionManifest;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest", meta = (ClampMin = "1"))
    int32 DefinitionVersion = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    FName CurrentNodeId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    EDAQuestProgressState ProgressState = EDAQuestProgressState::Active;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest", meta = (ClampMin = "0"))
    int64 StartedWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest", meta = (ClampMin = "0"))
    int64 LastTransitionWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest", meta = (ClampMin = "0"))
    int64 CurrentNodeEnteredWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    TArray<FName> CompletedNodeIds;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    TArray<FDAQuestNodeTransitionRecord> NodeTransitionRecords;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    TArray<FDAQuestWorldAssetBinding> WorldAssetBindings;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAWorldEventSaveState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Event")
    FName EventId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Event")
    FDAWorldEventDefinitionManifest DefinitionManifest;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Event", meta = (ClampMin = "1"))
    int32 DefinitionVersion = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Event")
    FName CurrentStageId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Event")
    EDAWorldEventProgressState ProgressState = EDAWorldEventProgressState::Active;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Event", meta = (ClampMin = "0"))
    int64 StartedWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Event", meta = (ClampMin = "0"))
    int64 LastTransitionWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Event")
    TArray<FName> CompletedStageIds;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAPromiseRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Promise")
    FGuid PromiseId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Promise")
    FName PromiseDefinitionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Promise")
    FName PromiserId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Promise")
    TArray<FName> ConflictActionTags;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Promise")
    TArray<FName> FulfillmentActionTags;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Promise")
    EDAPromiseState State = EDAPromiseState::Active;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Promise", meta = (ClampMin = "0"))
    int64 CreatedWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Promise", meta = (ClampMin = "0"))
    int64 ResolvedWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Promise")
    FName ResolutionActionTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Promise")
    FGuid ResolutionActionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Promise")
    bool bLegacyResolutionWithoutAction = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Promise", meta = (ClampMin = "0"))
    int32 LegacyResolutionSourceSchemaVersion = 0;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDANarrativeActionRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative")
    FGuid ActionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative")
    TArray<FName> NormalizedActionTags;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative", meta = (ClampMin = "0"))
    int64 WorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative")
    TArray<FGuid> FulfilledPromiseIds;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative")
    TArray<FGuid> BreachedPromiseIds;

    // Schema-v7 migration can retain an old ID, but its original semantic input was not persisted.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative")
    bool bLegacyIdentityOnly = false;
};

UENUM(BlueprintType)
enum class EDAQuestContentUnlockType : uint8
{
    CityMode, CardInstance, Blueprint, UtilitySystems, OperatorXp, InsightReward,
    IntelligenceAuditorPath, AxiomArchiveFragment, DiplomacyContact, EdenTradeAccess
};

UENUM(BlueprintType)
enum class EDASynaraCapitalEfficiencyState : uint8
{
    Baseline, SmallerIncrease, Increased
};

UENUM(BlueprintType)
enum class EDAAgencyPetitionResolution : uint8
{
    None, AgencyForum, RetrainWorkers, AutomationCap, Rejected
};

UENUM(BlueprintType)
enum class EDAIronBorderResolution : uint8
{
    None, Accepted, Refused, FavorableTerms, Deferred
};

UENUM(BlueprintType)
enum class EDADiplomaticTrend : uint8
{
    Unchanged, Increased, Decreased
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDASynaraValueReason
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ActionId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName SubjectId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) double Baseline = 0.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) double Delta = 0.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) double Result = 0.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int64 WorldTick = 0;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDASynaraCitizenEmployment
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CitizenId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName JobId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGuid FacilityWorldAssetId;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDASynaraPolicyReason
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ActionId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName AuthorityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Baseline = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Result = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int64 WorldTick = 0;
};

/** The one Core-owned Synara authority consumed by simulation, narrative and world adapters. */
USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDASynaraCampaignState
{
    GENERATED_BODY()
    FDASynaraCampaignState()
    {
        FactionSupport.Add(TEXT("faction.synara.ascendants"), 25.0);
        FactionSupport.Add(TEXT("faction.synara.human_agency"), 25.0);
        FactionSupport.Add(TEXT("faction.synara.synthetic_rights"), 25.0);
        FactionSupport.Add(TEXT("faction.synara.moderates"), 25.0);
        CitizenRelationships.Add(TEXT("citizen.synara.nia_vale"), 0.0);
    }
    bool Validate(FString& OutError) const;
    bool ApplyDependencyReason(FName ActionId, double Delta, int64 WorldTick);
    bool ApplyFactionSupportReason(FName ActionId, FName FactionId, double Delta, int64 WorldTick);
    bool ApplyCitizenRelationshipReason(FName ActionId, FName CitizenId, double Delta, int64 WorldTick);
    bool ApplyPolicyReason(FName ActionId, FName AuthorityId, int32 Result, int64 WorldTick);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign|Synara") double Dependency = 0.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign|Synara") TMap<FName, double> FactionSupport;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign|Synara") TMap<FName, double> CitizenRelationships;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign|Synara") TArray<FDASynaraCitizenEmployment> CitizenEmployment;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign|Synara") TArray<FDASynaraValueReason> DependencyReasons;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign|Synara") TArray<FDASynaraValueReason> FactionSupportReasons;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign|Synara") TArray<FDASynaraValueReason> CitizenRelationshipReasons;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign|Synara") TArray<FDASynaraPolicyReason> PolicyReasons;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign|Synara") EDASynaraCapitalEfficiencyState CapitalEfficiency = EDASynaraCapitalEfficiencyState::Baseline;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign|Synara") EDAAgencyPetitionResolution AgencyPetition = EDAAgencyPetitionResolution::None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign|Synara") EDAIronBorderResolution IronBorder = EDAIronBorderResolution::None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign|Synara") EDADiplomaticTrend ForgeweaveTrust = EDADiplomaticTrend::Unchanged;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign|Synara") EDADiplomaticTrend ForgeweaveRespect = EDADiplomaticTrend::Unchanged;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign|Synara") EDADiplomaticTrend ForgeweaveDependence = EDADiplomaticTrend::Unchanged;
};

/** Exact-once durable grant; the action ID is the idempotence key and the payload is replay-checked. */
USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAQuestContentUnlockRecord
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content") FName ActionId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content") FName QuestId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content") EDAQuestContentUnlockType Type = EDAQuestContentUnlockType::CityMode;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content") FName DefinitionId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content") FName ContentId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content") int32 Quantity = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content") TArray<FGuid> GrantedCardInstanceIds;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content") FString SourceFingerprint;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content", meta = (ClampMin = "0")) int64 WorldTick = 0;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAQuestObjectiveAssetBindingRecord
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content") FName QuestId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content") FName BindingId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content") FName DefinitionId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content") FGuid WorldAssetId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content") int64 BindWorldTick = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content") FString QuestDefinitionFingerprint;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDACitizenStoryTransitionRecord
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CitizenId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName BaselineStoryState;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ResultStoryState;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGuid SourceActionId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName SourceActionTag;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int64 WorldTick = 0;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAWorldMapAuthorityRecord
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName SourceQuestId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName SourceActionId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int64 WorldTick = 0;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAAuditEligibilitySourceRecord
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName EligibilityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGuid SourceActionId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName SourceActionTag;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int64 WorldTick = 0;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAQuestEligibilityProofRecord
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName QuestId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName BranchTag;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName EligibilityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGuid SourceActionId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName SourceActionTag;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int64 SourceWorldTick = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int64 WorldTick = 0;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAQuestCrisisCompletionRecord
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content") FName QuestId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content") FName CompletionActionId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content") FGuid NarrativeActionId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content") FString QuestDefinitionFingerprint;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content", meta = (ClampMin = "0")) int64 WorldTick = 0;
};

/** Durable audit for one authored quest outcome. The full semantic payload is replay-checked. */
USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAQuestContentEffectRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content")
    FName ActionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content")
    FName QuestId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content")
    FName ChoiceBranchTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content")
    FString SourceFingerprint;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content") TArray<FName> SemanticEffects;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content") bool bHasCitizenRelationshipDelta = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content") int32 CitizenRelationshipDelta = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content") bool bHasHumanAgencySupportDelta = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content")
    int32 HumanAgencySupportDelta = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content") bool bHasDependencyDelta = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content") int32 DependencyDelta = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content") double BaselineDependency = 0.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content") double ResultDependency = 0.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content") double BaselineHumanAgencySupport = 0.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content") double ResultHumanAgencySupport = 0.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content") double BaselineNiaTrust = 0.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content") double ResultNiaTrust = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content")
    FName CitizenStoryState;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content")
    TArray<FName> HistoryTags;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content", meta = (ClampMin = "0"))
    int64 WorldTick = 0;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDANarrativeCampaignState
{
    GENERATED_BODY()

    FDAQuestSaveState* FindQuestState(FName QuestId);
    const FDAQuestSaveState* FindQuestState(FName QuestId) const;
    FDAWorldEventSaveState* FindEventState(FName EventId);
    const FDAWorldEventSaveState* FindEventState(FName EventId) const;
    FDAPromiseRecord* FindPromiseRecord(FGuid PromiseId);
    const FDAPromiseRecord* FindPromiseRecord(FGuid PromiseId) const;
    const FDANarrativeActionRecord* FindActionRecord(FGuid ActionId) const;
    bool Validate(
        const TArray<FDAWorldAssetRecord>& WorldAssets,
        const TArray<FName>& CampaignHistoryTags,
        FString& OutError) const;
    bool IsFirstHourTransactionInProgress() const { return bFirstHourTransactionInProgress; }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative")
    TArray<FDAQuestSaveState> QuestStates;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative")
    TArray<FDAWorldEventSaveState> EventStates;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative")
    TArray<FDAPromiseRecord> PromiseRecords;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative")
    TArray<FDANarrativeActionRecord> ActionRecords;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content")
    TArray<FDAQuestContentUnlockRecord> QuestContentUnlockRecords;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content")
    TArray<FDAQuestObjectiveAssetBindingRecord> QuestObjectiveAssetBindings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content")
    TArray<FDAQuestCrisisCompletionRecord> QuestCrisisCompletionRecords;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content")
    TMap<FName, FName> CitizenStoryStates;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content")
    TArray<FDACitizenStoryTransitionRecord> CitizenStoryTransitionRecords;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content")
    TArray<FDAWorldMapAuthorityRecord> WorldMapAuthorityRecords;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content")
    TArray<FDAAuditEligibilitySourceRecord> AuditEligibilitySourceRecords;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content")
    TArray<FDAQuestEligibilityProofRecord> QuestEligibilityProofRecords;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Content")
    TArray<FDAQuestContentEffectRecord> QuestContentEffectRecords;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative", meta = (ClampMin = "0"))
    int64 MutationRevision = 0;

private:
    friend class FDAFirstHourCampaignRuntime;

    // Non-persistent and inaccessible to save callers. It exists only while Task18 commits a
    // terminal transition; the friend coordinator clears it before mandatory final validation.
    bool bFirstHourTransactionInProgress = false;
};
