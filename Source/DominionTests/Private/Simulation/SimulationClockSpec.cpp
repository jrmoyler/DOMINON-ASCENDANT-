#include "Fixtures/DAGameInstanceSubsystemFixture.h"
#include "Misc/AutomationTest.h"
#include "Time/DASimulationClockSubsystem.h"
#include "UObject/UObjectGlobals.h"

#include <limits>

BEGIN_DEFINE_SPEC(FDASimulationClockSpec, "Dominion.Simulation.Clock",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FDASimulationClockSpec)

void FDASimulationClockSpec::Define()
{
    It("emits five development cycles and one world tick after 150 active seconds", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDASimulationClockSubsystem* Clock = Fixture.GetSubsystem<UDASimulationClockSubsystem>();
        TestNotNull("Clock comes from an initialized GameInstance", Clock);
        if (Clock == nullptr)
        {
            return;
        }
        TArray<int64> DevelopmentCycles;
        TArray<int64> WorldTicks;
        Clock->OnDevelopmentCycle.AddLambda([&DevelopmentCycles](const int64 CycleIndex)
        {
            DevelopmentCycles.Add(CycleIndex);
        });
        Clock->OnWorldTick.AddLambda([&WorldTicks](const int64 WorldTickIndex)
        {
            WorldTicks.Add(WorldTickIndex);
        });

        Clock->Tick(150.f);

        if (TestEqual("Five cycles resolve", DevelopmentCycles.Num(), 5))
        {
            TestEqual("First cycle index is one", DevelopmentCycles[0], 1LL);
            TestEqual("Last cycle index is five", DevelopmentCycles[4], 5LL);
        }

        if (TestEqual("One world tick resolves", WorldTicks.Num(), 1))
        {
            TestEqual("World tick index is one", WorldTicks[0], 1LL);
        }
    });

    It("does not advance while paused", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDASimulationClockSubsystem* Clock = Fixture.GetSubsystem<UDASimulationClockSubsystem>();
        TestNotNull("Clock comes from an initialized GameInstance", Clock);
        if (Clock == nullptr)
        {
            return;
        }
        int32 DevelopmentCycleCount = 0;
        Clock->OnDevelopmentCycle.AddLambda([&DevelopmentCycleCount](const int64)
        {
            ++DevelopmentCycleCount;
        });

        Clock->SetPaused(true);
        Clock->Tick(150.f);

        TestEqual("Paused time resolves no cycles", DevelopmentCycleCount, 0);
    });

    It("applies four-times simulation only while fast-forward is allowed", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDASimulationClockSubsystem* Clock = Fixture.GetSubsystem<UDASimulationClockSubsystem>();
        TestNotNull("Clock comes from an initialized GameInstance", Clock);
        if (Clock == nullptr)
        {
            return;
        }
        int32 DevelopmentCycleCount = 0;
        Clock->OnDevelopmentCycle.AddLambda([&DevelopmentCycleCount](const int64)
        {
            ++DevelopmentCycleCount;
        });

        Clock->SetFastForwardAllowed(false);
        Clock->SetSimulationRate(4.f);
        Clock->Tick(30.f);
        TestEqual("Disallowed fast-forward remains real-time", DevelopmentCycleCount, 1);

        Clock->SetFastForwardAllowed(true);
        Clock->SetSimulationRate(4.f);
        Clock->Tick(7.5f);
        TestEqual("Allowed four-times fast-forward resolves proportionally", DevelopmentCycleCount, 2);
    });

    It("restores strategic authority and broadcasts every explicitly advanced World Tick", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDASimulationClockSubsystem* Clock = Fixture.GetSubsystem<UDASimulationClockSubsystem>();
        TestNotNull("Clock comes from an initialized GameInstance", Clock);
        if (Clock == nullptr)
        {
            return;
        }
        TArray<FString> ObservedEvents;
        Clock->OnDevelopmentCycle.AddLambda([&ObservedEvents](const int64 Cycle)
        {
            ObservedEvents.Add(FString::Printf(TEXT("cycle:%lld"), Cycle));
        });
        Clock->OnWorldTick.AddLambda([&ObservedEvents](const int64 WorldTick)
        {
            ObservedEvents.Add(FString::Printf(TEXT("world:%lld"), WorldTick));
        });

        TestTrue("Strategic authority restores at World Tick seven", Clock->RestoreWorldTickAuthority(7));
        TestEqual("Restore aligns the World Tick index", Clock->GetCurrentWorldTick(), 7LL);
        TestEqual("Restore aligns the Development Cycle index", Clock->GetCurrentDevelopmentCycle(), 35LL);
        TestTrue("Two strategic World Ticks advance", Clock->AdvanceStrategicWorldTicks(2));

        const TCHAR* ExpectedEvents[] = {
            TEXT("cycle:36"), TEXT("cycle:37"), TEXT("cycle:38"), TEXT("cycle:39"), TEXT("cycle:40"), TEXT("world:8"),
            TEXT("cycle:41"), TEXT("cycle:42"), TEXT("cycle:43"), TEXT("cycle:44"), TEXT("cycle:45"), TEXT("world:9")
        };
        TestEqual("Normal cycle and World Tick listeners see every strategic event", ObservedEvents.Num(), static_cast<int32>(UE_ARRAY_COUNT(ExpectedEvents)));
        for (int32 Index = 0; Index < ObservedEvents.Num() && Index < UE_ARRAY_COUNT(ExpectedEvents); ++Index)
        {
            TestEqual(
                *FString::Printf(TEXT("Strategic event %d preserves cycle-before-world ordering"), Index),
                ObservedEvents[Index],
                FString(ExpectedEvents[Index]));
        }
        TestEqual("Strategic authority ends at World Tick nine", Clock->GetCurrentWorldTick(), 9LL);
        TestEqual("Development Cycle authority remains aligned", Clock->GetCurrentDevelopmentCycle(), 45LL);
    });

    It("does not advance or broadcast any canonical time when World Tick pre-commit rejects", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDASimulationClockSubsystem* Clock = Fixture.GetSubsystem<UDASimulationClockSubsystem>();
        TestNotNull("Clock comes from an initialized GameInstance", Clock);
        if (Clock == nullptr)
        {
            return;
        }

        int32 CycleBroadcasts = 0;
        int32 WorldTickBroadcasts = 0;
        Clock->OnDevelopmentCycle.AddLambda([&CycleBroadcasts](const int64) { ++CycleBroadcasts; });
        Clock->OnWorldTick.AddLambda([&WorldTickBroadcasts](const int64) { ++WorldTickBroadcasts; });
        const FDelegateHandle RejectHandle = Clock->OnWorldTickPreCommit.AddLambda(
            [](const int64 WorldTick, FDAWorldTickVeto& Veto)
            {
                if (WorldTick == 1)
                {
                    Veto.Reject();
                }
            });

        TestFalse("Rejected strategic advancement reports failure", Clock->AdvanceStrategicWorldTicks(1));
        TestEqual("Rejected time keeps Development Cycle authority at zero", Clock->GetCurrentDevelopmentCycle(), 0LL);
        TestEqual("Rejected time keeps World Tick authority at zero", Clock->GetCurrentWorldTick(), 0LL);
        TestEqual("Rejected time broadcasts no Development Cycles", CycleBroadcasts, 0);
        TestEqual("Rejected time broadcasts no World Tick", WorldTickBroadcasts, 0);

        Clock->OnWorldTickPreCommit.Remove(RejectHandle);
        TestTrue("The same World Tick remains recoverable after its blocker clears", Clock->AdvanceStrategicWorldTicks(1));
        TestEqual("Recovery broadcasts the five canonical cycles once", CycleBroadcasts, 5);
        TestEqual("Recovery broadcasts the World Tick once", WorldTickBroadcasts, 1);
        TestEqual("Recovered clock reaches World Tick one", Clock->GetCurrentWorldTick(), 1LL);
    });

    It("accepts only World Tick authorities whose five-cycle representation is restorable", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDASimulationClockSubsystem* Clock = Fixture.GetSubsystem<UDASimulationClockSubsystem>();
        TestNotNull("Clock comes from an initialized GameInstance", Clock);
        if (Clock == nullptr)
        {
            return;
        }

        constexpr int64 MaximumRestorableWorldTick = MAX_int64 / 5;
        TestTrue("The exact restorable World Tick boundary is accepted", Clock->RestoreWorldTickAuthority(MaximumRestorableWorldTick));
        TestEqual("Boundary restoration preserves the World Tick exactly", Clock->GetCurrentWorldTick(), MaximumRestorableWorldTick);
        TestFalse("One World Tick beyond the five-cycle boundary is rejected", Clock->RestoreWorldTickAuthority(MaximumRestorableWorldTick + 1));
        TestEqual("Rejected restoration leaves the prior authority unchanged", Clock->GetCurrentWorldTick(), MaximumRestorableWorldTick);
    });

    It("makes a pre-commit rejection monotonic across listener ordering", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDASimulationClockSubsystem* Clock = Fixture.GetSubsystem<UDASimulationClockSubsystem>();
        TestNotNull("Clock exists", Clock);
        if (Clock == nullptr)
        {
            return;
        }

        bool bLaterListenerObservedRejection = false;
        Clock->OnWorldTickPreCommit.AddLambda([](const int64, FDAWorldTickVeto& Veto)
        {
            Veto.Reject();
        });
        Clock->OnWorldTickPreCommit.AddLambda([&bLaterListenerObservedRejection](const int64, FDAWorldTickVeto& Veto)
        {
            bLaterListenerObservedRejection = Veto.IsRejected();
        });

        TestFalse("A later listener cannot undo the earlier veto", Clock->AdvanceStrategicWorldTicks(1));
        TestTrue("Later listeners observe the monotonic rejected state", bLaterListenerObservedRejection);
        TestEqual("Vetoed ordering advances no cycle", Clock->GetCurrentDevelopmentCycle(), 0LL);
        TestEqual("Vetoed ordering advances no World Tick", Clock->GetCurrentWorldTick(), 0LL);
    });

    It("rejects every recursive clock mutation during precommit cycles commit and abort", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDASimulationClockSubsystem* Clock = Fixture.GetSubsystem<UDASimulationClockSubsystem>();
        TestNotNull("Clock exists", Clock);
        if (Clock == nullptr)
        {
            return;
        }

        bool bRecursiveAdvanceAccepted = true;
        bool bRecursiveRestoreAccepted = true;
        int32 RecursiveWorldBroadcasts = 0;
        Clock->OnWorldTickPreCommit.AddLambda(
            [Clock, &bRecursiveAdvanceAccepted, &bRecursiveRestoreAccepted](const int64, FDAWorldTickVeto&)
            {
                bRecursiveAdvanceAccepted = Clock->AdvanceStrategicWorldTicks(1);
                bRecursiveRestoreAccepted = Clock->RestoreWorldTickAuthority(9);
                Clock->AdvanceSimulation(150.0);
                Clock->SetPaused(true);
            });
        Clock->OnDevelopmentCycle.AddLambda([Clock](const int64)
        {
            Clock->AdvanceSimulation(30.0);
            Clock->RestoreWorldTickAuthority(20);
        });
        Clock->OnWorldTick.AddLambda([Clock, &RecursiveWorldBroadcasts](const int64)
        {
            ++RecursiveWorldBroadcasts;
            Clock->AdvanceStrategicWorldTicks(1);
            Clock->RestoreWorldTickAuthority(30);
        });

        TestTrue("The outer transition commits", Clock->AdvanceStrategicWorldTicks(1));
        TestFalse("Recursive strategic advance is rejected", bRecursiveAdvanceAccepted);
        TestFalse("Recursive restore is rejected", bRecursiveRestoreAccepted);
        TestEqual("Reentrant callbacks produce one World Tick only", RecursiveWorldBroadcasts, 1);
        TestEqual("Reentrant callbacks cannot change the committed World Tick", Clock->GetCurrentWorldTick(), 1LL);
        TestEqual("Reentrant callbacks cannot change the five committed cycles", Clock->GetCurrentDevelopmentCycle(), 5LL);

        Clock->OnWorldTickPreCommit.Clear();
        Clock->OnDevelopmentCycle.Clear();
        Clock->OnWorldTick.Clear();
        Clock->Tick(30.f);
        TestEqual("A reentrant pause request was ignored", Clock->GetCurrentDevelopmentCycle(), 6LL);
    });

    It("broadcasts an explicit abort clears staging and retries the same transition", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDASimulationClockSubsystem* Clock = Fixture.GetSubsystem<UDASimulationClockSubsystem>();
        TestNotNull("Clock exists", Clock);
        if (Clock == nullptr)
        {
            return;
        }

        TArray<int64> AbortedTicks;
        bool bRecursiveAbortAdvanceAccepted = true;
        bool bRecursiveAbortRestoreAccepted = true;
        Clock->OnWorldTickTransitionAborted.AddLambda(
            [Clock, &AbortedTicks, &bRecursiveAbortAdvanceAccepted, &bRecursiveAbortRestoreAccepted](const int64 Tick)
            {
                AbortedTicks.Add(Tick);
                bRecursiveAbortAdvanceAccepted = Clock->AdvanceStrategicWorldTicks(1);
                bRecursiveAbortRestoreAccepted = Clock->RestoreWorldTickAuthority(12);
                Clock->AdvanceSimulation(150.0);
                Clock->SetPaused(true);
            });
        const FDelegateHandle VetoHandle = Clock->OnWorldTickPreCommit.AddLambda(
            [](const int64, FDAWorldTickVeto& Veto) { Veto.Reject(); });

        TestFalse("Vetoed transition aborts", Clock->AdvanceStrategicWorldTicks(1));
        TestEqual("Abort broadcasts exactly once", AbortedTicks.Num(), 1);
        if (AbortedTicks.Num() == 1)
        {
            TestEqual("Abort identifies the staged World Tick", AbortedTicks[0], 1LL);
        }
        TestFalse("Clock mutations are rejected during abort callbacks", bRecursiveAbortAdvanceAccepted);
        TestFalse("Clock restoration is rejected during abort callbacks", bRecursiveAbortRestoreAccepted);
        Clock->OnWorldTickPreCommit.Remove(VetoHandle);
        TestTrue("Cleared staging permits retry of the same World Tick", Clock->AdvanceStrategicWorldTicks(1));
        TestEqual("Retry commits World Tick one", Clock->GetCurrentWorldTick(), 1LL);
    });

    It("uses checked arithmetic and rejects non-finite natural time at authority boundaries", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDASimulationClockSubsystem* Clock = Fixture.GetSubsystem<UDASimulationClockSubsystem>();
        TestNotNull("Clock exists", Clock);
        if (Clock == nullptr)
        {
            return;
        }

        constexpr int64 MaximumRestorableWorldTick = MAX_int64 / UDASimulationClockSubsystem::DevelopmentCyclesPerWorldTick;
        TestTrue("Boundary authority restores", Clock->RestoreWorldTickAuthority(MaximumRestorableWorldTick));
        const int64 BoundaryCycle = Clock->GetCurrentDevelopmentCycle();
        TestFalse("Strategic advancement beyond cycle storage rejects", Clock->AdvanceStrategicWorldTicks(1));
        TestEqual("Rejected boundary advance preserves World Tick", Clock->GetCurrentWorldTick(), MaximumRestorableWorldTick);
        TestEqual("Rejected boundary advance preserves cycle", Clock->GetCurrentDevelopmentCycle(), BoundaryCycle);

        Clock->AdvanceSimulation(std::numeric_limits<double>::quiet_NaN());
        Clock->AdvanceSimulation(std::numeric_limits<double>::infinity());
        Clock->AdvanceSimulation(TNumericLimits<double>::Max());
        TestEqual("Non-finite or overflowing natural time preserves World Tick", Clock->GetCurrentWorldTick(), MaximumRestorableWorldTick);
        TestEqual("Non-finite or overflowing natural time preserves cycle", Clock->GetCurrentDevelopmentCycle(), BoundaryCycle);
    });

    It("rejects finite deltas whose cycle floor is not exactly representable before any int64 cast", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDASimulationClockSubsystem* Clock = Fixture.GetSubsystem<UDASimulationClockSubsystem>();
        TestNotNull("Clock exists", Clock);
        if (Clock == nullptr) return;
        const FDASimulationClockAuthorityState Before = Clock->CaptureAuthorityState();
        const double UnsafeFiniteDelta = 9007199254740992.0
            * UDASimulationClockSubsystem::DevelopmentCycleDurationSeconds;
        Clock->AdvanceSimulation(UnsafeFiniteDelta);
        TestEqual("Unsafe finite floor preserves cycle", Clock->GetCurrentDevelopmentCycle(), Before.CurrentDevelopmentCycle);
        TestEqual("Unsafe finite floor preserves tick", Clock->GetCurrentWorldTick(), Before.CurrentWorldTick);
        TestEqual("Unsafe finite floor preserves accumulator", Clock->GetAccumulatedSimulationSeconds(),
            Before.AccumulatedSimulationSeconds);
    });

    It("captures and restores the exact cycle accumulator and pending natural-time authority", [this]()
    {
        FDAGameInstanceSubsystemFixture SourceFixture;
        UDASimulationClockSubsystem* SourceClock = SourceFixture.GetSubsystem<UDASimulationClockSubsystem>();
        TestNotNull("Source clock exists", SourceClock);
        if (SourceClock == nullptr)
        {
            return;
        }

        SourceClock->AdvanceSimulation(75.25);
        const FDASimulationClockAuthorityState SavedAuthority = SourceClock->CaptureAuthorityState();
        TestEqual("Captured authority retains two resolved cycles", SavedAuthority.CurrentDevelopmentCycle, 2LL);
        TestEqual("Captured authority retains World Tick zero", SavedAuthority.CurrentWorldTick, 0LL);
        TestEqual("Captured authority retains the fractional pending cycle", SavedAuthority.AccumulatedSimulationSeconds, 15.25);

        FDAGameInstanceSubsystemFixture RestoredFixture;
        UDASimulationClockSubsystem* RestoredClock = RestoredFixture.GetSubsystem<UDASimulationClockSubsystem>();
        TestNotNull("Restored clock exists", RestoredClock);
        if (RestoredClock == nullptr)
        {
            return;
        }
        TestTrue("Exact clock authority restores", RestoredClock->RestoreAuthorityState(SavedAuthority));
        TestEqual("Restored cycle is exact", RestoredClock->GetCurrentDevelopmentCycle(), 2LL);
        TestEqual("Restored World Tick is exact", RestoredClock->GetCurrentWorldTick(), 0LL);
        TestEqual("Restored accumulator is exact", RestoredClock->GetAccumulatedSimulationSeconds(), 15.25);

        SourceClock->AdvanceSimulation(14.75);
        RestoredClock->AdvanceSimulation(14.75);
        TestEqual("Original and restored clocks resolve the same next cycle",
            RestoredClock->GetCurrentDevelopmentCycle(), SourceClock->GetCurrentDevelopmentCycle());
        TestEqual("Original and restored clocks retain the same accumulator",
            RestoredClock->GetAccumulatedSimulationSeconds(), SourceClock->GetAccumulatedSimulationSeconds());

        FDASimulationClockAuthorityState Invalid = SavedAuthority;
        Invalid.CurrentWorldTick = 1;
        TestFalse("A cycle index behind its World Tick cannot restore", RestoredClock->RestoreAuthorityState(Invalid));
        TestEqual("Rejected restore preserves the prior exact cycle", RestoredClock->GetCurrentDevelopmentCycle(), 3LL);
    });
}
