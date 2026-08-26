#pragma once

#include "Campaign/DADaxtonCampaignState.h"
#include "CoreMinimal.h"

#include "DAAscensionCampaignState.generated.h"

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAReplicationRecord
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FGuid ActionId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FGuid SourceCardInstanceId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FGuid ReplicatedCardInstanceId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName DefinitionId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int64 DevelopmentCycle = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int64 WorldTick = 0;
};

/** Per-cycle audit that binds Factory pressures to the canonical campaign authorities. */
USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAAutonomousFactoryPressureRecord
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName ActionId;
    /** Stable GUID order for every operational Factory aggregated into this cycle. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FGuid> FactoryWorldAssetIds;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int64 DevelopmentCycle = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int64 WorldTick = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double DependencyBefore = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double DependencyAfter = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float ResourceHungerBefore = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float ResourceHungerAfter = 0.f;
};

/** Persisted Founder Hall projection target; slot identity is one-based and immutable. */
USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAFounderHallRelicPosition
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 SlotIndex = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bActive = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bOccupied = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName RelicId;
};

/** Immutable proof captured by the one first-Ascension transaction. */
USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAFirstAscensionRecord
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FGuid ActionId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FGuid ConquestResolutionActionId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FGuid DaxtonResolutionActionId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EDADaxtonLeaderState LeaderState = EDADaxtonLeaderState::Governor;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int64 WorldTick = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int64 DevelopmentCycle = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double InfluenceBefore = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double InfluenceAfter = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double InsightBefore = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double InsightAfter = 0.0;
};

/** Save-owned post-Ascension authority. It stores no actor or presentation references. */
USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAAscensionCampaignState
{
    GENERATED_BODY()

    static constexpr int64 ReplicationCadenceDevelopmentCycles = 12;
    static constexpr int32 ConvergenceMaximum = 20;

    bool Validate(const struct FDACampaignSnapshot& Campaign, FString& OutError) const;
    const FDAReplicationRecord* FindReplication(FGuid ActionId) const;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bForgeweaveAscended = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FDAFirstAscensionRecord FirstAscension;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FName> RelicIds;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FName> UnlockedForgeweaveDefinitionIds;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bReplicationUnlocked = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int64 NextReplicationEligibleCycle = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FDAReplicationRecord> ReplicationRecords;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bFusionEligible = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FName> UnlockedBlueprintIds;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 FounderHallVisualState = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FDAFounderHallRelicPosition> FounderHallRelicPositions;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 ActiveRelicSlotCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bHiddenChamberOpen = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 ConvergenceAuthority = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 ConvergenceAuthorityMaximum = ConvergenceMaximum;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FDAAutonomousFactoryPressureRecord> FactoryPressureRecords;
};

struct DOMINIONCORE_API FDAAscensionAuthority
{
    static const TArray<FName>& ForgeweaveDefinitionIds();
    static FGuid MakeReplicationInstanceId(FGuid ActionId);
    static bool IsActionIdInUse(const struct FDACampaignSnapshot& Campaign, FGuid ActionId);
};
