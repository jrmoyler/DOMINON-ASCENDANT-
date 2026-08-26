# Task 15 report — structural damage, capture, surrender, and ruins

## Delivered

- Added persistence-source-bound structural damage records and `UDAStructuralDamageComponent`.
- Implemented canonical structure integrity bands: Operational 100–51%, Damaged 50–26%, Disabled 25–1%, and Ruined at 0%.
- Module damage is surgical: destroying a production-critical Control Center disables production without reducing total Structural Integrity or creating a ruin. Only zero total integrity creates the persistent Ruined state.
- Frozen full modular destruction eligibility to exactly eight stable definitions: Synthetic Fabrication Node, Swarm Foundry, Infinite Foundry, Replication Forge, Smog Reclaimer, Freight Furnace, Grand Forge, and Autonomous Factory.
- Added persistence-source-bound capture records and `UDACaptureComponent` with Disabled/10%-integrity/actor/contest/security preconditions, a 20-second base capture duration, the canonical Engineer -35% modifier, and contested interruption that clears progress and stale interaction ownership.
- Added atomic, exact-once outcome transactions. Preserve retains the source definition and transfers ownership; Convert sanctions a persistent conversion operation; Study removes operational use for authored Insight; Salvage ruins the structure for authored Capital/materials; permitted Gift transfers to an allied recipient for authored Influence/Loyalty.
- Reward values remain content-authored because the binding bibles specify reward types but no numeric tuning. Outcome state and reward-granted guards prevent duplicate grants across component reconstruction.
- Added Forge Guard surrender eligibility for Breaking/Rout morale at Military Sovereignty 25 or lower. Acceptance persists squad/campaign history, improves authored Influence/Loyalty/future-surrender likelihood, and cannot replay by flag or history.
- Added no fake assets or presentation-only state.

## TDD evidence

`StructuralDamageSpec.cpp` and `CaptureSpec.cpp` were created before the production component files. The tests cover:

1. Control Center destruction disables a Synthetic Fabrication Node without ruining it; zero total integrity ruins it.
2. Exact integrity band transitions and exactly eight full-modular definitions.
3. Disabled Infinite Foundry capture at the inclusive 10% boundary and exact base 20-second timing.
4. Actor, contest, security, state, and integrity preconditions; reentry rejection; contested interruption/reset.
5. Preserve/Convert/Study/Salvage/Gift state, ownership, reward-type, atomic-failure, and no-double-reward invariants.
6. Breaking/Routed Forge Guard surrender eligibility, authoritative history, post-conflict mutation, and no-repeat behavior.

### Focused RED attempt

```text
/bin/bash: line 2: UnrealEditor-Cmd: command not found
AUTOMATION_RED_EXIT=127
```

The tests preceded production code, but UE 5.8 / `UnrealEditor-Cmd` is unavailable in this workspace. Test discovery and the expected compile failure were blocked. Exit 127 is not a passing or observed Automation RED result.

### Regression RED attempts

The capture-record invariant, surrender-history replay, and capture-reentry regressions were added before their corresponding guards. Each focused attempt was likewise blocked with command-not-found / exit 127; none is represented as a passing result.

### Focused GREEN attempts

```text
/bin/bash: line 2: UnrealEditor-Cmd: command not found
AUTOMATION_FINAL_GREEN_EXIT=127
```

This remains environment-blocked, not Unreal Automation GREEN.

## Static verification

- `git diff --cached --check` — exit 0.
- Generated-header ordering check — every new or changed UHT header keeps its `.generated.h` include last.
- Frozen modular-definition count check — exactly 8.
- Manual API/source review checked USTRUCT/UCLASS export macros, generated headers, module dependencies, finite/nonnegative validation, pre-mutation result calculation, exact-once flags/history, and lifetime-tracked stable-record binding.

Static checks do not replace UHT, UBT, linking, or Automation execution.

## Remaining verification

On a UE 5.8-capable runner, build the editor target and execute:

```text
UnrealEditor-Cmd DominionAscendant.uproject -unattended -nop4 -NullRHI -ExecCmds="Automation RunTests Dominion.Gameplay.StructuralDamage+Dominion.Gameplay.Capture;Quit" -TestExit="Automation Test Queue Empty"
```

That is required before claiming UHT/C++ compilation or real Automation GREEN.

---

## Review-fix round 1

### Blockers resolved

1. Structural, capture, surrender, reward, and operation-history authority now lives in `DominionCore` DTOs. Structural/capture records are keyed by `WorldAssetId`; surrender records are keyed by `SquadId`; both gameplay components reject mismatched stable keys. `FDACampaignSnapshot::OperationConflict` persists the aggregate, resources, and history. Snapshot validation rejects missing/duplicate/invalid keys before save and after load without introducing a Core dependency on Gameplay or Simulation.
2. Save schema is now v3. The v2→v3 migration creates an empty reflected operation-conflict aggregate and refreshes the full-envelope checksum. Tests cover v2 public migration, duplicate-key rejection, conflict-payload checksum tampering, active capture save/reload, and exact-once damage/capture/surrender reconstruction.
3. Overall `EDAConstructionState` is derived only from total Structural Integrity. Destroying a production-critical module at 100% integrity leaves the structure Operational while persisting `bProductionDisabled`; only integrity bands select Damaged/Disabled/Ruined.
4. Gift resolution now requires the target to exist in the persisted allowed-recipient list with `Allied` or `LocalAuthority` relationship. Hostile, neutral, unrelated, original-owner, and captor recipients are rejected before any ownership/reward mutation.
5. Active capture now persists `ActiveInteractionId`, `ActiveCaptureActorId`, and `ActiveCaptureRole`. Every advance revalidates stable identity, role, actor presence, contest, active security, facility state, and integrity; invalidation atomically clears active identity/progress. Engineer timing is covered at the exact 13-second boundary.
6. Surrender runtime context must match the persistent record's `SquadId`. Breaking/Rout eligibility is covered at Military Sovereignty 25 inclusive and 25.01 exclusive; accepted records and operation history prevent reward replay after save/load.

### Review-fix TDD evidence

The blocker regressions were written before their fixes. The focused RED command was attempted and returned:

```text
/bin/bash: line 2: UnrealEditor-Cmd: command not found
AUTOMATION_REVIEW_FIX_RED_EXIT=127
```

The focused GREEN and stable-squad-key RED attempts returned the same command-not-found / exit 127 result. These are environment-blocked attempts, not Automation failures or passes.

An additional save-validation RED was written for contradictory integrity bands and incomplete durable outcome flags. Its focused attempt returned:

```text
/bin/bash: line 1: UnrealEditor-Cmd: command not found
AUTOMATION_PERSISTENCE_INVARIANT_RED_EXIT=127
```

Core validation now rejects a structural record whose global construction state contradicts total integrity, and rejects resolved capture records whose owner, outcome-specific functional flags, ruin state, permitted Gift relationship, or durable history do not form one complete transaction. Runtime reconstruction calls the same Core outcome invariant.

### Review-fix verification status

- Focused UE 5.8 Automation remains blocked because `UnrealEditor-Cmd` is unavailable.
- The final combined Core Save, StructuralDamage, and Capture GREEN attempt returned `AUTOMATION_REVIEW_FIX_FINAL_GREEN_EXIT=127`; it is blocked, not passing.
- Static diff, generated-header ordering, reflected DTO ownership, schema/migration routing, stable-key guards, and module dependency direction are checked locally.
- UHT, UBT, link, and Automation verification remain required on the UE 5.8 runner.

---

## Review-fix round 2

### Blockers resolved

1. Salvage is now calculated and committed as one campaign-owned aggregate transaction. It requires the matching stable-key structural record, sets total integrity to zero and global state to Ruined, marks every module Ruined, disables production, records the capture outcome, and grants resources together. Core validation rejects persisted Salvage without the matching structural ruin authority. A combined Salvage save/reload test covers the asset, capture guard, structural modules, production flag, Capital, and materials.
2. Capture outcome mutation no longer accepts a detached capture record or resource state. The component owns a lifetime-tracked campaign authority and resolves the capture record, resources, Gift authorization, world asset, and optional structural record from that one aggregate. Surrender mutation accepts one `FDAOperationConflictSnapshot` and resolves its stable `SquadId`, resources, and history internally; independent mutable record/resource/history parameters were removed. Transient aggregate mutation is proven not to reach authoritative state, and a mismatched aggregate is rejected without reward.
3. Capture and structural-damage components now cache only `WorldAssetId` plus a reflected weak `UDACampaignSaveGame` owner. Every operation re-resolves world/conflict array records, so array relocation cannot leave dangling `TArray` element pointers. Both components have array-growth regression coverage.
4. The frozen exact-eight modular-destruction policy now lives in `DominionCore`. Core save/load validation and gameplay reconstruction call the same policy. Tests cover the exact set, non-eligible reconstruction rejection, pre-save rejection, and rejection of a checksummed non-eligible load.

### Round-2 TDD evidence

The aggregate API, Salvage round trip, array-growth, transient/mismatch, and Core-policy regressions were written before their production changes. The focused RED attempt returned:

```text
/bin/bash: line 1: UnrealEditor-Cmd: command not found
AUTOMATION_REVIEW_FIX_ROUND2_RED_EXIT=127
```

The missing-structural Salvage validation regression was also written before its Core guard. Its focused RED attempt returned `AUTOMATION_SALVAGE_AGGREGATE_RED_EXIT=127` with the same command-not-found blocker. Neither is an observed Automation failure or pass.

### Round-2 verification status

- UE 5.8 Automation remains unavailable in this workspace; final GREEN evidence must come from a UE runner.
- The final combined Core Save, StructuralDamage, and Capture attempt returned `AUTOMATION_REVIEW_FIX_ROUND2_GREEN_EXIT=127`; it is environment-blocked, not passing.
- Static verification covers diff whitespace, generated-header order, one-way module dependencies, exact-eight Core policy count, aggregate-only mutation signatures, stable-id/weak-owner component state, and absence of cached array-element pointers.
