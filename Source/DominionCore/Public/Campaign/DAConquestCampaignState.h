#pragma once

#include "CoreMinimal.h"

#include "DAConquestCampaignState.generated.h"

UENUM(BlueprintType)
enum class EDAForgeweaveRoute : uint8
{
    Force,
    Economic,
    Influence,
    Alliance
};

UENUM(BlueprintType)
enum class EDAConquestMeter : uint8
{
    MilitarySovereignty,
    EconomicAutonomy,
    CivicLegitimacy,
    AllianceReadiness
};

/** Frozen vertical-slice acceptance thresholds shared by Core validation and runtime projection. */
struct DOMINIONCORE_API FDAConquestRules
{
    static constexpr double EconomicActionLossCap = 15.0;
    static constexpr double ForceCompletionThreshold = 0.0;
    static constexpr double ConditionalSurrenderThreshold = 25.0;
    static constexpr double EconomicCompletionThreshold = 20.0;
    static constexpr double InfluenceCompletionThreshold = 20.0;
    static constexpr double AllianceAverageThreshold = 80.0;
    static constexpr double AllianceComponentFloor = 65.0;
    static constexpr double MajorGrievanceThreshold = 50.0;
};

/** One durable, idempotent projection from an already-persisted campaign authority. */
USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAConquestMeterMutation
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName MutationId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EDAForgeweaveRoute Route = EDAForgeweaveRoute::Force;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EDAConquestMeter Meter = EDAConquestMeter::MilitarySovereignty;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName SourceAuthority;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName SourceId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double Delta = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double Result = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int64 WorldTick = 0;
};

/** Audit snapshot after one synchronization; the newest row must equal the live route weights. */
USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAConquestRouteWeightRecord
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int64 WorldTick = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int64 Revision = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double Force = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double Economic = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double Influence = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double Alliance = 0.0;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAAllianceReadinessComponents
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double Trust = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double SharedInterest = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double CrisisResolution = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double Respect = 0.0;

    double Average() const { return (Trust + SharedInterest + CrisisResolution + Respect) / 4.0; }
    double Minimum() const { return FMath::Min(FMath::Min(Trust, SharedInterest), FMath::Min(CrisisResolution, Respect)); }
};

/** Canonical persisted Forgeweave conquest authority; no runtime actors or route-local duplicates. */
USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAConquestCampaignState
{
    GENERATED_BODY()

    const FDAConquestMeterMutation* FindMutation(FName MutationId) const;
    int64 FindMutationRevision(FName MutationId) const;
    bool Validate(FString& OutError) const;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double MilitarySovereignty = 100.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double EconomicAutonomy = 100.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double CivicLegitimacy = 100.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double AllianceReadiness = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FDAAllianceReadinessComponents AllianceComponents;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double ForceWeight = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double EconomicWeight = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double InfluenceWeight = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double AllianceWeight = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FDAConquestMeterMutation> Mutations;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FDAConquestRouteWeightRecord> RouteWeightHistory;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int64 MutationRevision = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bForgeweaveResolved = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EDAForgeweaveRoute ResolvedRoute = EDAForgeweaveRoute::Force;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FGuid ResolutionActionId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int64 ResolvedWorldTick = 0;
};

struct FDACampaignSnapshot;

/** Core-safe proof shared by runtime completion and persisted snapshot validation. */
struct DOMINIONCORE_API FDAConquestAuthorityValidator
{
    static bool CanCompleteRoute(EDAForgeweaveRoute Route,
        const FDACampaignSnapshot& Campaign, FString& OutError);
    static bool ValidateResolvedRoute(const FDACampaignSnapshot& Campaign, FString& OutError);
};
