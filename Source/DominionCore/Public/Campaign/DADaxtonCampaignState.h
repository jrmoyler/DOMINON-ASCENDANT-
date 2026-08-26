#pragma once

#include "CoreMinimal.h"

#include "DADaxtonCampaignState.generated.h"

UENUM(BlueprintType)
enum class EDADaxtonEncounterPhase : uint8
{
    Inactive,
    PhaseOne,
    PhaseTwo,
    PhaseThree,
    Resolved
};

UENUM(BlueprintType)
enum class EDADaxtonInteraction : uint8
{
    Damage,
    DisableCoolant,
    RedirectSupply,
    HackProduction,
    WorkerShutdown
};

UENUM(BlueprintType)
enum class EDADaxtonChoiceObjective : uint8
{
    DefeatDaxton,
    SaveGrandForge,
    EvacuateWorkers,
    StabilizeProductionOfferUnion
};

/** The six exact supported post-conflict outcomes. Unresolved is represented by bLeaderResolved=false. */
UENUM(BlueprintType)
enum class EDADaxtonLeaderState : uint8
{
    Governor,
    IndustrialAdvisor,
    AlliedForgeLord,
    Exile,
    Prisoner,
    Dead
};

UENUM()
enum class EDADaxtonCanonicalActionKind : uint8
{
    AdvanceProduction,
    DeployHardenedCover,
    ReinforceForgeGuard,
    CompletePhaseOneIndustrialObjective,
    SystemInteraction,
    EnterChoicePhase,
    CompleteChoiceObjective,
    ResolveLeader
};

USTRUCT()
struct DOMINIONCORE_API FDADaxtonModuleProof
{
    GENERATED_BODY()
    UPROPERTY() FName ModuleId;
    UPROPERTY() float CurrentHealth = 0.f;
    UPROPERTY() float MaximumHealth = 0.f;
    UPROPERTY() bool bDisablesProduction = false;
    UPROPERTY() uint8 DamageState = 0;
};

USTRUCT()
struct DOMINIONCORE_API FDADaxtonCitizenProof
{
    GENERATED_BODY()
    UPROPERTY() FName CitizenId;
    UPROPERTY() FName CityId;
    UPROPERTY() FGuid HomeWorldAssetId;
    UPROPERTY() FName JobId;
};

USTRUCT()
struct DOMINIONCORE_API FDADaxtonJobOpeningProof
{
    GENERATED_BODY()
    UPROPERTY() FName JobId;
    UPROPERTY() FName CityId;
    UPROPERTY() FGuid FacilityWorldAssetId;
    UPROPERTY() int32 OpenPositions = 0;
};

USTRUCT()
struct DOMINIONCORE_API FDADaxtonJobAssignmentProof
{
    GENERATED_BODY()
    UPROPERTY() FName CitizenId;
    UPROPERTY() FName JobId;
    UPROPERTY() FGuid FacilityWorldAssetId;
};

/** Exact canonical authorities consumed by one encounter action, persisted for deterministic replay. */
USTRUCT()
struct DOMINIONCORE_API FDADaxtonCanonicalProjection
{
    GENERATED_BODY()
    UPROPERTY() float ProductionReserve = 0.f;
    UPROPERTY() float LogisticsEfficiency = 0.f;
    UPROPERTY() float IndustrialThroughput = 0.f;
    UPROPERTY() float ResourceHunger = 0.f;
    UPROPERTY() float DefensePressure = 0.f;
    UPROPERTY() bool bOverdrive = false;
    UPROPERTY() float Trust = 0.f;
    UPROPERTY() float Respect = 0.f;
    UPROPERTY() float Grievance = 0.f;
    UPROPERTY() FGuid GrandForgeWorldAssetId;
    UPROPERTY() float GrandForgeStructuralIntegrity = 0.f;
    UPROPERTY() uint8 GrandForgeConstructionState = 0;
    UPROPERTY() bool bProductionDisabled = false;
    UPROPERTY() int64 LiveSignalsRevision = 0;
    UPROPERTY() TArray<FDADaxtonModuleProof> GrandForgeModules;
    UPROPERTY() TArray<FDADaxtonCitizenProof> Citizens;
    UPROPERTY() TArray<FDADaxtonJobOpeningProof> JobOpenings;
    UPROPERTY() TArray<FDADaxtonJobAssignmentProof> JobAssignments;
    UPROPERTY() TArray<FName> HistoryTags;
};

USTRUCT()
struct DOMINIONCORE_API FDADaxtonCanonicalActionRecord
{
    GENERATED_BODY()
    UPROPERTY() FGuid ActionId;
    UPROPERTY() EDADaxtonCanonicalActionKind Kind = EDADaxtonCanonicalActionKind::AdvanceProduction;
    UPROPERTY() EDADaxtonInteraction Interaction = EDADaxtonInteraction::Damage;
    UPROPERTY() EDADaxtonChoiceObjective Objective = EDADaxtonChoiceObjective::DefeatDaxton;
    UPROPERTY() EDADaxtonLeaderState LeaderState = EDADaxtonLeaderState::Governor;
    UPROPERTY() float Strength = 0.f;
    UPROPERTY() int64 WorldTick = 0;
    UPROPERTY() EDADaxtonEncounterPhase PhaseBefore = EDADaxtonEncounterPhase::Inactive;
    UPROPERTY() EDADaxtonEncounterPhase PhaseAfter = EDADaxtonEncounterPhase::Inactive;
    UPROPERTY() float ArmorBefore = 0.f;
    UPROPERTY() float ArmorAfter = 0.f;
    UPROPERTY() float HeatBefore = 0.f;
    UPROPERTY() float HeatAfter = 0.f;
    UPROPERTY() float CoolantBefore = 0.f;
    UPROPERTY() float CoolantAfter = 0.f;
    UPROPERTY() FDADaxtonCanonicalProjection Before;
    UPROPERTY() FDADaxtonCanonicalProjection After;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDADaxtonInteractionRecord
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FGuid ActionId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EDADaxtonInteraction Interaction = EDADaxtonInteraction::Damage;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Strength = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float ArmorIntegrityBefore = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float ArmorIntegrityAfter = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float HeatBefore = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float HeatAfter = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float CoolantBefore = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float CoolantAfter = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float ProductionBefore = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float ProductionAfter = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float ResourceHungerBefore = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float ResourceHungerAfter = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int64 WorldTick = 0;
};

/** Save-owned encounter authority. Runtime encounter objects only hold a reference to this campaign record. */
USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDADaxtonCampaignState
{
    GENERATED_BODY()

    bool ValidateStandalone(FString& OutError) const;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EDADaxtonEncounterPhase Phase = EDADaxtonEncounterPhase::Inactive;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FGuid StartActionId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int64 StartedWorldTick = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float ArmorIntegrity = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Heat = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float CoolantStability = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bPoweredArmorActive = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 ForgeGuardReinforcements = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 HardenedCoverDeployments = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 GrandForgeProductionCycles = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FGuid> PhaseOneActionIds;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bPhaseOneIndustrialObjectiveCompleted = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FGuid PhaseOneObjectiveActionId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FDADaxtonInteractionRecord> InteractionRecords;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FGuid PhaseThreeActionId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<EDADaxtonChoiceObjective> CompletedObjectives;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FGuid> ObjectiveActionIds;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bLeaderResolved = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EDADaxtonLeaderState LeaderState = EDADaxtonLeaderState::Governor;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FGuid ResolutionActionId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int64 ResolvedWorldTick = 0;
    /** Exact ordered prefix of canonical Forgeweave relationship reasons present at resolution. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 ResolutionRelationshipReasonCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FName> ResolutionRelationshipReasonMutationIds;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FDADaxtonCanonicalProjection InitialCanonicalProjection;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FDADaxtonCanonicalActionRecord> CanonicalActionRecords;
};

struct FDACampaignSnapshot;
enum class EDAForgeweaveRoute : uint8;

/** Core-safe outcome proof shared by gameplay resolution and save validation. */
struct DOMINIONCORE_API FDADaxtonAuthorityValidator
{
    static bool ResolveCanonicalRoute(const FDACampaignSnapshot& Campaign,
        EDAForgeweaveRoute& OutRoute, FString& OutError);
    static bool CanResolveLeaderState(EDADaxtonLeaderState State,
        const FDACampaignSnapshot& Campaign, FString& OutError);
    static bool ValidateCampaignState(const FDACampaignSnapshot& Campaign, FString& OutError);
    static bool CaptureCanonicalProjection(const FDACampaignSnapshot& Campaign,
        FDADaxtonCanonicalProjection& OutProjection, FString& OutError);
    static FName GetLeaderHistoryTag(EDADaxtonLeaderState State);
};
