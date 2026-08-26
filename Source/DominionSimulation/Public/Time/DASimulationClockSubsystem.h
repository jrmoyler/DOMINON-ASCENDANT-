#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "World/DARegionalWorldState.h"

#include "DASimulationClockSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FDADevelopmentCycleEvent, int64 /* CycleIndex */);
DECLARE_MULTICAST_DELEGATE_OneParam(FDAWorldTickEvent, int64 /* WorldTickIndex */);

class DOMINIONSIMULATION_API FDAWorldTickVeto final
{
public:
    FDAWorldTickVeto() = default;
    FDAWorldTickVeto(const FDAWorldTickVeto&) = delete;
    FDAWorldTickVeto(FDAWorldTickVeto&&) = delete;
    FDAWorldTickVeto& operator=(const FDAWorldTickVeto&) = delete;
    FDAWorldTickVeto& operator=(FDAWorldTickVeto&&) = delete;

    void Reject() { bRejected = true; }
    bool IsRejected() const { return bRejected; }

private:
    bool bRejected = false;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(
    FDAWorldTickPreCommitEvent,
    int64 /* ProposedWorldTickIndex */,
    FDAWorldTickVeto& /* Veto */);
DECLARE_MULTICAST_DELEGATE_ThreeParams(
    FDADevelopmentCyclesPreCommitEvent,
    int64 /* FirstProposedCycle */,
    int64 /* CycleCount */,
    FDAWorldTickVeto& /* Veto */);
DECLARE_MULTICAST_DELEGATE_OneParam(FDAWorldTickTransitionAbortedEvent, int64 /* ProposedWorldTickIndex */);
DECLARE_MULTICAST_DELEGATE_OneParam(
    FDAClockAuthorityChangedEvent,
    const FDASimulationClockAuthorityState& /* StableAuthority */);

/**
 * The sole simulation-time authority. It converts elapsed active time into
 * discrete Development Cycle and World Tick events for simulation consumers.
 */
UCLASS()
class DOMINIONSIMULATION_API UDASimulationClockSubsystem final : public UGameInstanceSubsystem, public FTickableGameObject
{
    GENERATED_BODY()

public:
    static constexpr double DevelopmentCycleDurationSeconds = 30.0;
    static constexpr int64 DevelopmentCyclesPerWorldTick = 5;
    static constexpr float MinimumSimulationRate = 1.f;
    static constexpr float MaximumSimulationRate = 4.f;

    FDADevelopmentCycleEvent OnDevelopmentCycle;
    FDADevelopmentCyclesPreCommitEvent OnDevelopmentCyclesPreCommit;
    FDAWorldTickPreCommitEvent OnWorldTickPreCommit;
    FDAWorldTickTransitionAbortedEvent OnWorldTickTransitionAborted;
    FDAWorldTickEvent OnWorldTick;
    FDAClockAuthorityChangedEvent OnAuthorityChanged;

    void SetSimulationRate(float Rate);
    void SetPaused(bool bInPaused);
    void SetFastForwardAllowed(bool bInFastForwardAllowed);

    void AdvanceSimulation(double DeltaSeconds);
    bool RestoreWorldTickAuthority(int64 WorldTick);
    bool RestoreAuthorityState(const FDASimulationClockAuthorityState& Authority);
    bool AdvanceStrategicWorldTicks(int32 WorldTickCount);

    FDASimulationClockAuthorityState CaptureAuthorityState() const;

    int64 GetCurrentDevelopmentCycle() const
    {
        return CurrentCycleIndex;
    }

    int64 GetCurrentWorldTick() const
    {
        return CurrentWorldTickIndex;
    }

    double GetAccumulatedSimulationSeconds() const
    {
        return AccumulatedSimulationSeconds;
    }

    bool IsPaused() const { return bPaused; }

    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool IsTickable() const override;
    virtual bool IsTickableWhenPaused() const override;

private:
    double AccumulatedSimulationSeconds = 0.0;
    int64 CurrentCycleIndex = 0;
    int64 CurrentWorldTickIndex = 0;
    float SimulationRate = MinimumSimulationRate;
    bool bPaused = false;
    bool bFastForwardAllowed = true;
    bool bTransitionInProgress = false;
};
