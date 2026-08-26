#pragma once

#include "Ascension/DAAscensionSystem.h"
#include "Campaign/DACampaignAuthority.h"
#include "CoreMinimal.h"
#include "Diplomacy/DADiplomacySystem.h"
#include "Regions/DARegionState.h"
#include "Save/DACampaignSaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Trade/DATradeSystem.h"
#include "World/DARegionalWorldState.h"
#include "City/DAPlacementTypes.h"
#include "Narrative/DAFoundryShortageRuntime.h"
#include "Economy/DAEconomyTypes.h"
#include "UObject/Interface.h"

#include "DAWorldStateSubsystem.generated.h"

class UDASimulationClockSubsystem;
class UDAEconomySubsystem;
class UDAForgeweaveStrategy;
class FDAWorldTickVeto;
class FDACityGridSubsystem;
class UDAContentRegistrySubsystem;
class UDARegionalCrisisContentRegistrySubsystem;
class UDASystemicPressureSystem;
class UDAJobSystem;
class UDAMigrationSystem;

class DOMINIONWORLD_API IDARegionTravelRuntime
{
public:
    virtual ~IDARegionTravelRuntime() = default;

    virtual bool SnapshotPersistentDelta(
        const FDARegionState& SourceRegion,
        FDARegionPersistentDelta& OutDelta) = 0;
    virtual bool UnloadRegionRepresentation(FName RegionId) = 0;
    virtual bool EnsureRegionLoaded(FName RequestId, FName RegionId) = 0;
    virtual bool EnsureRegionReconstructed(
        FName RequestId,
        const FDARegionState& Region,
        const FDACampaignSnapshot& Campaign) = 0;
};

/** Registered streaming/reconstruction bridge supplied by the live world representation. */
UINTERFACE(meta=(CannotImplementInterfaceInBlueprint))
class DOMINIONWORLD_API UDARegionTravelRuntimeService : public UInterface
{
    GENERATED_BODY()
};

class DOMINIONWORLD_API IDARegionTravelRuntimeService
{
    GENERATED_BODY()
public:
    virtual bool SnapshotPersistentDelta(
        const FDARegionState& SourceRegion, FDARegionPersistentDelta& OutDelta) = 0;
    virtual bool UnloadRegionRepresentation(FName RegionId) = 0;
    virtual bool EnsureRegionLoaded(FName RequestId, FName RegionId) = 0;
    virtual bool EnsureRegionReconstructed(FName RequestId, const FDARegionState& Region,
        const FDACampaignSnapshot& Campaign) = 0;
};

UENUM()
enum class EDATravelResult : uint8
{
    Completed,
    AlreadyCompleted,
    RuntimeFailure,
    Rejected
};

DECLARE_MULTICAST_DELEGATE_OneParam(FDATravelStageEvent, EDATravelHandoffStage /* Stage */);
using FDACommittedCampaignSnapshot = TSharedRef<const FDACampaignSnapshot>;
DECLARE_MULTICAST_DELEGATE_OneParam(
    FDAWorldTickStateCommittedEvent,
    FDACommittedCampaignSnapshot /* CommittedState */);
DECLARE_MULTICAST_DELEGATE_OneParam(
    FDAFoundryShortageWarningEvent,
    FDACommittedCampaignSnapshot /* CommittedState */);

UCLASS()
class DOMINIONWORLD_API UDAWorldStateSubsystem final
    : public UGameInstanceSubsystem, public IDACampaignAuthority
{
    GENERATED_BODY()

public:
    FDATravelStageEvent OnTravelStageCompleted;
    FDAWorldTickStateCommittedEvent OnWorldTickStateCommitted;
    /** Production notification emitted exactly when the persisted warning count advances. */
    FDAFoundryShortageWarningEvent OnFoundryShortageWarning;

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    bool InitializeVerticalSliceState(FName InitialRegionId, int64 InitialWorldTick = 0);
    bool RestorePersistentCampaign(const FDACampaignSnapshot& InCampaign);
    bool RestorePersistentState(const FDAWorldCampaignState& InState);

    const FDAWorldCampaignState& GetPersistentState() const
    {
        return PersistentCampaign.WorldState;
    }

    virtual const FDACampaignSnapshot& GetPersistentCampaign() const override { return PersistentCampaign; }
    const FDACampaignLiveSignalState& GetLiveSignals() const { return PersistentCampaign.LiveSignals; }
    int64 GetCurrentWorldTick() const { return PersistentCampaign.WorldState.CurrentWorldTick; }

    /** Compare-and-swap commit for production systems sharing the single campaign owner. */
    virtual bool TryCommitPersistentCampaign(const FDACampaignSnapshot& Candidate,
        int64 ExpectedNarrativeRevision, int64 ExpectedSignalRevision,
        int64 ExpectedWorldTick) override;
    UFUNCTION(BlueprintCallable, Category="Dominion|Campaign")
    bool SubmitCitizenSignal(const FDACampaignCitizenSignal& Signal);
    UFUNCTION(BlueprintCallable, Category="Dominion|Campaign")
    bool SubmitJobOpeningSignal(const FDACampaignJobOpeningSignal& Signal);
    UFUNCTION(BlueprintCallable, Category="Dominion|Campaign")
    bool SubmitJobAssignmentSignal(const FDACampaignJobAssignmentSignal& Signal);
    UFUNCTION(BlueprintCallable, Category="Dominion|Campaign")
    bool SubmitUtilitySignal(const FDACampaignUtilitySignal& Signal);

    bool AdvanceWorldTicks(int32 WorldTickCount);

    EDATravelResult Travel(
        FName RequestId,
        FName DestinationRegionId,
        int32 TravelWorldTicks,
        IDARegionTravelRuntime& Runtime);

    bool RegisterTravelRuntime(UObject* Runtime, FString& OutError);
    void UnregisterTravelRuntime(UObject* Runtime);
    EDATravelResult TravelUsingRegisteredRuntime(FName RequestId,
        FName DestinationRegionId, int32 TravelWorldTicks, FString& OutError);

    /** Atomic authored-card -> claimed-city-grid -> WorldAsset construction transaction. */
    bool TryPlacePlayerWorldAsset(FGuid CardInstanceId, FName CityId, FIntPoint Origin,
        FIntPoint RequestedFootprint, EGridRotation Rotation, FGuid& OutWorldAssetId,
        FString& OutError);

    /** Atomically removes a non-operational placement and returns its deck member to one legal zone. */
    bool TryCancelPlayerWorldAsset(FGuid WorldAssetId, FString& OutError);

    /** Materializes a committed unlocked blueprint into concrete owned Card Instances. */
    bool CraftUnlockedBlueprint(FName BlueprintId, int32 Quantity,
        TArray<FGuid>& OutCardInstanceIds, FString& OutError);

    /** Production facility adapter: persisted WorldAsset -> authored economy definition -> output. */
    bool ResolvePlayerFacilityOutput(FGuid WorldAssetId, const FDAWalletValues& BaseOutput,
        FDAFacilityOutput& OutOutput, FString& OutError);

    /** Rebuilds occupancy only from persisted claims and exact authored footprint/timing records. */
    static bool ReconstructClaimedCityGrid(const FDACampaignSnapshot& Campaign, FName CityId,
        UDAContentRegistrySubsystem& Registry, FDACityGridSubsystem& OutGrid, FString& OutError);

    EDAFoundryShortageActionResult ResolveFoundryShortage(FGuid ActionId,
        EDAFoundryShortageResolution Resolution);

    /** Atomically synchronizes canonical evidence and commits one gated Forgeweave resolution. */
    UFUNCTION(BlueprintCallable, Category="Dominion|Campaign")
    bool CompleteForgeweaveConquestRoute(FGuid ActionId, EDAForgeweaveRoute Route,
        FString& OutError);

    /** Production-owned Daxton commands: candidate mutation, full validation, then one CAS publish. */
    bool StartDaxtonEncounter(FGuid ActionId, FString& OutError);
    bool AdvanceDaxtonGrandForgeProduction(FGuid ActionId, FString& OutError);
    bool DeployDaxtonHardenedCover(FGuid ActionId, FString& OutError);
    bool ReinforceDaxtonForgeGuard(FGuid ActionId, FString& OutError);
    bool CompleteDaxtonPhaseOneIndustrialObjective(FGuid ActionId, FString& OutError);
    bool ApplyDaxtonInteraction(FGuid ActionId, EDADaxtonInteraction Interaction,
        float Strength, FString& OutError);
    bool EnterDaxtonChoicePhase(FGuid ActionId, FString& OutError);
    bool CompleteDaxtonChoiceObjective(FGuid ActionId, EDADaxtonChoiceObjective Objective,
        FString& OutError);
    bool ResolveDaxtonLeaderState(FGuid ActionId, EDADaxtonLeaderState State,
        FString& OutError);

    /** One atomic owner transaction; cinematic presentation is never a gameplay gate. */
    bool CompleteFirstAscension(FGuid ActionId, FString& OutError);
    /** Replicates through the save-owned cadence ledger bound to the simulation clock. */
    bool ReplicateCard(FGuid ActionId, FGuid SourceCardInstanceId,
        FGuid& OutReplicatedCardInstanceId, FString& OutError);

    /** Presentation may start only from committed owner state and can always be skipped. */
    UFUNCTION(BlueprintPure, Category="Dominion|Campaign|Ascension")
    FDAAscensionPresentationState GetAscensionPresentationState() const;

private:
    void EnsureSystems();
    bool CompleteTravelStage(EDATravelHandoffStage Stage);
    void HandleSimulationWorldTickPreCommit(int64 WorldTick, FDAWorldTickVeto& Veto);
    void HandleSimulationWorldTickAborted(int64 WorldTick);
    void HandleSimulationWorldTick(int64 WorldTick);
    void HandleDevelopmentCycle(int64 CycleIndex);
    void HandleDevelopmentCyclesPreCommit(int64 FirstCycle, int64 CycleCount, FDAWorldTickVeto& Veto);
    void HandleClockAuthorityChanged(const FDASimulationClockAuthorityState& Authority);
    bool TryCommitDaxtonCandidate(const FDACampaignSnapshot& Candidate,
        int64 ExpectedNarrativeRevision, int64 ExpectedSignalRevision,
        int64 ExpectedWorldTick, FString& OutError);
    bool AdvancePlayerConstruction(FDACampaignSnapshot& Candidate, FString& OutError) const;
    bool PreviewDevelopmentCycles(int64 FirstCycle, int64 CycleCount, FString& OutError) const;

    UPROPERTY()
    FDACampaignSnapshot PersistentCampaign;

    UPROPERTY(Transient)
    TObjectPtr<UDATradeSystem> TradeSystem;

    UPROPERTY(Transient)
    TObjectPtr<UDADiplomacySystem> DiplomacySystem;

    UPROPERTY(Transient)
    TObjectPtr<UDASimulationClockSubsystem> SimulationClock;

    UPROPERTY(Transient)
    TObjectPtr<UDAEconomySubsystem> EconomySubsystem;

    UPROPERTY(Transient)
    TObjectPtr<UDAForgeweaveStrategy> ForgeweaveStrategy;

    UPROPERTY(Transient)
    TObjectPtr<UDARegionalCrisisContentRegistrySubsystem> RegionalCrisisContentRegistry;

    UPROPERTY(Transient)
    TObjectPtr<UDAContentRegistrySubsystem> ContentRegistry;

    UPROPERTY(Transient)
    TObjectPtr<UDASystemicPressureSystem> SystemicPressureSystem;

    UPROPERTY(Transient)
    TObjectPtr<UDAJobSystem> JobSystem;

    UPROPERTY(Transient)
    TObjectPtr<UDAMigrationSystem> MigrationSystem;

    UPROPERTY(Transient)
    TWeakObjectPtr<UObject> RegisteredTravelRuntime;

    FDelegateHandle WorldTickHandle;
    FDelegateHandle WorldTickPreCommitHandle;
    FDelegateHandle WorldTickAbortHandle;
    FDelegateHandle DevelopmentCycleHandle;
    FDelegateHandle DevelopmentCyclesPreCommitHandle;
    FDelegateHandle ClockAuthorityHandle;
    TOptional<FDACampaignSnapshot> PreparedWorldTickState;
    int64 PreparedWorldTick = INDEX_NONE;
};
