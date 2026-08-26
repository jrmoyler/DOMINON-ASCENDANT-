#include "Time/DASimulationClockSubsystem.h"

void UDASimulationClockSubsystem::SetSimulationRate(const float Rate)
{
    if (bTransitionInProgress)
    {
        return;
    }
    SimulationRate = FMath::IsFinite(Rate)
        ? FMath::Clamp(Rate, MinimumSimulationRate, MaximumSimulationRate)
        : MinimumSimulationRate;
}

void UDASimulationClockSubsystem::SetPaused(const bool bInPaused)
{
    if (bTransitionInProgress)
    {
        return;
    }
    bPaused = bInPaused;
}

void UDASimulationClockSubsystem::SetFastForwardAllowed(const bool bInFastForwardAllowed)
{
    if (bTransitionInProgress)
    {
        return;
    }
    bFastForwardAllowed = bInFastForwardAllowed;
}

void UDASimulationClockSubsystem::AdvanceSimulation(const double DeltaSeconds)
{
    if (bTransitionInProgress || bPaused || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0)
    {
        return;
    }

    const double EffectiveRate = bFastForwardAllowed ? static_cast<double>(SimulationRate) : static_cast<double>(MinimumSimulationRate);
    const double ScaledDelta = DeltaSeconds * EffectiveRate;
    if (!FMath::IsFinite(ScaledDelta)
        || !FMath::IsFinite(AccumulatedSimulationSeconds)
        || AccumulatedSimulationSeconds < 0.0
        || AccumulatedSimulationSeconds >= DevelopmentCycleDurationSeconds
        || ScaledDelta > TNumericLimits<double>::Max() - AccumulatedSimulationSeconds)
    {
        return;
    }
    const double ProposedAccumulatedSeconds = AccumulatedSimulationSeconds + ScaledDelta;
    const double ProposedCycleCountDouble = FMath::FloorToDouble(
        ProposedAccumulatedSeconds / DevelopmentCycleDurationSeconds);
    // A double represents every integer only through 2^53-1. Reject larger finite
    // batches before conversion so rounding can never authorize an int64 cast or
    // capacity check for a different number of cycles.
    constexpr double MaximumExactDoubleInteger = 9007199254740991.0;
    if (!FMath::IsFinite(ProposedCycleCountDouble)
        || ProposedCycleCountDouble < 0.0
        || ProposedCycleCountDouble > MaximumExactDoubleInteger)
    {
        return;
    }
    if (ProposedCycleCountDouble > 0.0)
    {
        const int64 ProposedCycleCount = static_cast<int64>(ProposedCycleCountDouble);
        if (ProposedCycleCount > MAX_int64 - CurrentCycleIndex) return;
        FDAWorldTickVeto Veto;
        OnDevelopmentCyclesPreCommit.Broadcast(CurrentCycleIndex + 1, ProposedCycleCount, Veto);
        if (Veto.IsRejected()) return;
    }
    AccumulatedSimulationSeconds = ProposedAccumulatedSeconds;

    while (AccumulatedSimulationSeconds >= DevelopmentCycleDurationSeconds)
    {
        if (CurrentCycleIndex == MAX_int64)
        {
            return;
        }
        const int64 ProposedCycle = CurrentCycleIndex + 1;
        const bool bCompletesWorldTick = ProposedCycle % DevelopmentCyclesPerWorldTick == 0;
        if (bCompletesWorldTick)
        {
            if (CurrentWorldTickIndex == MAX_int64)
            {
                return;
            }
            const int64 ProposedWorldTick = CurrentWorldTickIndex + 1;
            bTransitionInProgress = true;
            FDAWorldTickVeto Veto;
            OnWorldTickPreCommit.Broadcast(ProposedWorldTick, Veto);
            if (Veto.IsRejected())
            {
                OnWorldTickTransitionAborted.Broadcast(ProposedWorldTick);
                bTransitionInProgress = false;
                const FDASimulationClockAuthorityState Authority = CaptureAuthorityState();
                OnAuthorityChanged.Broadcast(Authority);
                return;
            }

            AccumulatedSimulationSeconds -= DevelopmentCycleDurationSeconds;
            CurrentCycleIndex = ProposedCycle;
            OnDevelopmentCycle.Broadcast(CurrentCycleIndex);
            CurrentWorldTickIndex = ProposedWorldTick;
            OnWorldTick.Broadcast(CurrentWorldTickIndex);
            bTransitionInProgress = false;
            continue;
        }

        AccumulatedSimulationSeconds -= DevelopmentCycleDurationSeconds;
        bTransitionInProgress = true;
        CurrentCycleIndex = ProposedCycle;
        OnDevelopmentCycle.Broadcast(CurrentCycleIndex);
        bTransitionInProgress = false;
    }

    const FDASimulationClockAuthorityState Authority = CaptureAuthorityState();
    OnAuthorityChanged.Broadcast(Authority);
}

bool UDASimulationClockSubsystem::RestoreWorldTickAuthority(const int64 WorldTick)
{
    if (bTransitionInProgress
        || WorldTick < 0
        || WorldTick > MAX_int64 / DevelopmentCyclesPerWorldTick)
    {
        return false;
    }

    FDASimulationClockAuthorityState Authority;
    Authority.bCaptured = true;
    Authority.CurrentWorldTick = WorldTick;
    Authority.CurrentDevelopmentCycle = WorldTick * DevelopmentCyclesPerWorldTick;
    return RestoreAuthorityState(Authority);
}

bool UDASimulationClockSubsystem::RestoreAuthorityState(const FDASimulationClockAuthorityState& Authority)
{
    FString Error;
    if (bTransitionInProgress || !Authority.bCaptured || !Authority.Validate(Error))
    {
        return false;
    }
    AccumulatedSimulationSeconds = Authority.AccumulatedSimulationSeconds;
    CurrentCycleIndex = Authority.CurrentDevelopmentCycle;
    CurrentWorldTickIndex = Authority.CurrentWorldTick;
    return true;
}

FDASimulationClockAuthorityState UDASimulationClockSubsystem::CaptureAuthorityState() const
{
    FDASimulationClockAuthorityState Authority;
    Authority.bCaptured = true;
    Authority.CurrentDevelopmentCycle = CurrentCycleIndex;
    Authority.CurrentWorldTick = CurrentWorldTickIndex;
    Authority.AccumulatedSimulationSeconds = AccumulatedSimulationSeconds;
    return Authority;
}

bool UDASimulationClockSubsystem::AdvanceStrategicWorldTicks(const int32 WorldTickCount)
{
    const int64 WorldTickCount64 = static_cast<int64>(WorldTickCount);
    if (bTransitionInProgress
        || WorldTickCount < 0
        || WorldTickCount64 > MAX_int64 / DevelopmentCyclesPerWorldTick)
    {
        return false;
    }
    const int64 CycleCount = WorldTickCount64 * DevelopmentCyclesPerWorldTick;
    if (CurrentWorldTickIndex > MAX_int64 - WorldTickCount64
        || CurrentCycleIndex > MAX_int64 - CycleCount)
    {
        return false;
    }
    if (CycleCount > 0)
    {
        FDAWorldTickVeto DevelopmentVeto;
        OnDevelopmentCyclesPreCommit.Broadcast(CurrentCycleIndex + 1, CycleCount, DevelopmentVeto);
        if (DevelopmentVeto.IsRejected()) return false;
    }

    for (int32 Index = 0; Index < WorldTickCount; ++Index)
    {
        const int64 ProposedWorldTick = CurrentWorldTickIndex + 1;
        bTransitionInProgress = true;
        FDAWorldTickVeto Veto;
        OnWorldTickPreCommit.Broadcast(ProposedWorldTick, Veto);
        if (Veto.IsRejected())
        {
            OnWorldTickTransitionAborted.Broadcast(ProposedWorldTick);
            bTransitionInProgress = false;
            const FDASimulationClockAuthorityState Authority = CaptureAuthorityState();
            OnAuthorityChanged.Broadcast(Authority);
            return false;
        }
        for (int32 Cycle = 0; Cycle < DevelopmentCyclesPerWorldTick; ++Cycle)
        {
            ++CurrentCycleIndex;
            OnDevelopmentCycle.Broadcast(CurrentCycleIndex);
        }
        CurrentWorldTickIndex = ProposedWorldTick;
        OnWorldTick.Broadcast(CurrentWorldTickIndex);
        bTransitionInProgress = false;
    }
    const FDASimulationClockAuthorityState Authority = CaptureAuthorityState();
    OnAuthorityChanged.Broadcast(Authority);
    return true;
}

void UDASimulationClockSubsystem::Tick(const float DeltaTime)
{
    AdvanceSimulation(static_cast<double>(DeltaTime));
}

TStatId UDASimulationClockSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UDASimulationClockSubsystem, STATGROUP_Tickables);
}

bool UDASimulationClockSubsystem::IsTickable() const
{
    return !IsTemplate();
}

bool UDASimulationClockSubsystem::IsTickableWhenPaused() const
{
    return true;
}
