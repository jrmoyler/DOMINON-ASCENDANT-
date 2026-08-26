# Task 14 report — squads, morale, cover, Command Points, and tactical orders

## Delivered

- Added `UDASquadEntity`, the tactical-state owner for formation, destination, target, morale, suppression, supply, and active order.
- Added the required `EDACommandOrder`: Move, Attack, Hold, Defend, Capture, Escort, Retreat, and Ability.
- Implemented canonical morale clamping and states: Inspired (75–100), Steady (50–74), Shaken (25–49), Breaking (1–24), and Rout (0).
- Added game-instance-owned `UDACommandSubsystem`, with an idempotent squad registry, a three-squad direct-selection cap, a separate single-vehicle slot, and doctrine fallback for units outside direct control.
- Added tactical order validation: only registered, directly selected entities receive direct orders.
- Added world-owned `UDACoverSubsystem`. It accepts authored sockets, selected ruin cover, and deployable cover as Partial, Full, Hardened, or Destructible. Stable IDs make repeated equivalent registration idempotent; conflicting or unnamed records are rejected.
- Implemented temporary CP with a baseline maximum of 6. Positive Control Zone awards clamp to that maximum; zero/negative rewards and zero/negative/over-budget spends fail without mutating CP. Valid spends deduct exactly once.

## TDD evidence

`Source/DominionTests/Private/Gameplay/CommandModeSpec.cpp` was written before the Task 14 production files. It specifies:

1. The fourth squad cannot enter direct selection after three squads and retains its AI doctrine.
2. One vehicle can be directly selected in addition to the three squad slots.
3. The exact 100 → 74 → 49 → 24 → 0 morale sequence maps to Inspired → Steady → Shaken → Breaking → Rout.
4. Direct tactical orders are rejected for doctrine-controlled squads and retained by a directly selected squad.
5. Authored, ruin, and deployable cover registration is queryable, deterministic, idempotent, and rejects conflicts/unnamed IDs.
6. CP starts at zero, has baseline max 6, earns only positive Control Zone awards, clamps at max, and treats invalid spends atomically.

### Focused RED attempt

```text
$ UnrealEditor-Cmd "$(pwd)/DominionAscendant.uproject" -unattended -nop4 -NullRHI -ExecCmds="Automation RunTests Dominion.Gameplay.CommandMode;Quit" -TestExit="Automation Test Queue Empty"
/bin/bash: line 2: UnrealEditor-Cmd: command not found
AUTOMATION_RED_EXIT=127
```

UE 5.8 / `UnrealEditor-Cmd` is unavailable in this workspace, so test discovery and the intended pre-implementation compiler/test failure were blocked. Exit 127 is not a passing result.

### Focused GREEN attempt

```text
$ UnrealEditor-Cmd "$(pwd)/DominionAscendant.uproject" -unattended -nop4 -NullRHI -ExecCmds="Automation RunTests Dominion.Gameplay.CommandMode;Quit" -TestExit="Automation Test Queue Empty"
/bin/bash: line 2: UnrealEditor-Cmd: command not found
AUTOMATION_GREEN_EXIT=127
```

This is likewise environment-blocked, not an Unreal build or Automation GREEN result.

## Static verification

```text
git diff --check  # exit 0
```

Source review also verifies the exact command-order list, morale comparisons, `3 + 1` selection constants, CP cap/spend guards, and all three cover registration paths. These checks do not replace UHT, UBT, or Automation execution.

## Remaining verification

On a UE 5.8-capable runner, build the editor target and execute:

```text
UnrealEditor-Cmd DominionAscendant.uproject -unattended -nop4 -NullRHI -ExecCmds="Automation RunTests Dominion.Gameplay.CommandMode;Quit" -TestExit="Automation Test Queue Empty"
```

That is required before claiming UHT/C++ compilation and real Automation GREEN.

---

## Review-fix cycle

### Findings resolved

1. Command membership is now owned by exactly one `UDACommandSubsystem`. Registration claims that owner idempotently, foreign registration fails, and every direct-selection/order path checks both ownership and the owning registry. A squad's vehicle/squad classification and doctrine are immutable while that ownership exists.
2. `UnregisterSquad` now removes the entity from both possible selection containers, clears direct control, removes registry membership, and releases ownership. Explicit reclassification and registration by another command subsystem are permitted only after that cleanup.
3. Cover has source-aware removal APIs for authored sockets, ruins, and deployables. Removal requires an existing ID from the matching source, removes exactly once, and permits stable re-registration afterward.
4. Command Automation fixtures now retain a real `UGameInstance` as each command subsystem's outer. They no longer construct a `UGameInstanceSubsystem` directly under the transient package.
5. Added regression coverage for immutable registered classification, foreign-subsystem selection/cap bypass, post-unregister reclassification, mismatched cover source removal, exact-once removal, and deterministic re-registration.

### Review-fix RED attempt

```text
/bin/bash: line 2: UnrealEditor-Cmd: command not found
AUTOMATION_REVIEW_FIX_RED_EXIT=127
```

The new regression spec was present before this implementation. The unavailable UE command runner prevented discovery/compiler failure, so exit 127 is blocked and not a test pass.

### Review-fix GREEN attempt

```text
/bin/bash: line 2: UnrealEditor-Cmd: command not found
AUTOMATION_REVIEW_FIX_FINAL_EXIT=127
```

This final focused run remains blocked—not GREEN. `git diff --check` returned exit 0 after the review fixes; UHT/UBT and Automation still require UE 5.8.

---

## Review-fix round 2

### Findings resolved

1. `UDASquadEntity` now stores command ownership as `TWeakObjectPtr<UObject>` in its public header. The `.cpp` includes `Command/DACommandSubsystem.h` and performs owner comparisons only there, where the subsystem type is complete. This avoids incomplete-type template/API hazards without changing ownership semantics.
2. Added the single `ResetToDoctrineFallback` path. It clears direct control, active order (to `Hold`), destination, and target while deliberately preserving morale, supply, suppression, formation, and doctrine.
3. Both `ReleaseDirectSelection` and `UnregisterSquad` invoke that same reset path. Unregister still removes both potential selection entries and then releases command ownership.
4. Added behavior specs that issue direct orders, then release or unregister, asserting the fallback order/state and exact morale/supply preservation in both cases.

### Round 2 RED attempt

```text
/bin/bash: line 2: UnrealEditor-Cmd: command not found
AUTOMATION_ROUND2_RED_EXIT=127
```

The fallback regression spec was added before this implementation. UE test discovery/compiler execution is blocked; exit 127 is not a RED or passing result.

### Round 2 GREEN attempt

```text
/bin/bash: line 2: UnrealEditor-Cmd: command not found
AUTOMATION_ROUND2_GREEN_EXIT=127
```

The final focused attempt is blocked, not Automation GREEN. UHT/UBT and real test execution remain required on a UE 5.8 runner.
