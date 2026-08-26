#pragma once

#include "CoreMinimal.h"

#include "DARegionalCrisisCampaignState.generated.h"

UENUM(BlueprintType)
enum class EDAFoundryShortageStage : uint8
{
    Inactive,
    ShortageWarning,
    MarketSpike,
    EcologicalDispute,
    EmergencyOverdrive,
    Resolved
};

UENUM(BlueprintType)
enum class EDAFoundryShortageResolution : uint8
{
    None,
    IndustrialSupport,
    EdenRestriction,
    BrokeredCompact,
    MarketExploitation,
    Collapse
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAFoundryShortageResolutionRecord
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FGuid ActionId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EDAFoundryShortageResolution Resolution = EDAFoundryShortageResolution::None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString ManifestFingerprint;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int64 WorldTick = 0;
    /** Exact 1-based conquest revision already containing joint-crisis history, or zero when absent. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int64 JointCrisisHistoryRevisionAtResolution = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int64 TradeCapacityBefore = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int64 TradeCapacityAfter = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double EcologyBefore = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double EcologyAfter = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float ResourceHungerBefore = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float ResourceHungerAfter = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double MarketModifierAfter = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double CapitalBefore = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double CapitalAfter = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double InsightBefore = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double InsightAfter = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double InfluenceBefore = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double InfluenceAfter = 0.0;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDARegionalCrisisCampaignState
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bTriggered = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int64 TriggerWorldTick = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EDAFoundryShortageStage FoundryStage = EDAFoundryShortageStage::Inactive;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 WarningEmissionCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int64 LastTransitionWorldTick = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EDAFoundryShortageResolution Resolution = EDAFoundryShortageResolution::None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int64 ResolvedWorldTick = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 RecoveryWorldTicks = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString ManifestFingerprint;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TMap<FName, FName> CitizenOutcomes;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FDAFoundryShortageResolutionRecord> ResolutionRecords;

    bool Validate(const struct FDAWorldCampaignState& WorldState,
        const struct FDACampaignLiveSignalState& LiveSignals,
        const TArray<FName>& HistoryTags, FString& OutError) const;
};
