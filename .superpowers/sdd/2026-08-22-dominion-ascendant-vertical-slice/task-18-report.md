# Task 18 report — persistent quest/event history and promise runtime

## Delivered

- Added the exact v1.0 quest-node taxonomy as reflected tagged variants: Start, Dialogue, Objective, Investigation, Build, Deliver, Explore, Combat, Defend, Capture, Choice, Wait, Timer, WorldCondition, CitizenCondition, FactionCondition, EconomyCondition, RelationshipCondition, EventTrigger, Reward, Failure, and Resolution. Every node carries an authored `SourceDefinitionId`; runtime code contains no quest-specific source asset or named-card fabrication.
- Added quest DAG validation for stable/unique IDs, exactly one referenced Start, at least one Resolution, canonical node/edge condition enums, source definitions, valid targets, terminal/non-terminal shape, unique branch tags, declared WorldAsset bindings, cycles, and all-node reachability.
- Added persistent quest runtime state and persistent world-event graph/state records. Each instance now owns an immutable reflected definition manifest and canonical deterministic fingerprint covering source identity, start/initial stage, scope, required bindings, nodes/stages, payloads, edges, and terminal flags. Same-version authored replacements are rejected on start, replay, evaluation, and advance.
- Bound dynamic quest conditions through named binding keys to real campaign `WorldAssetID`s. Destroying the referenced Cognitive Operations Tower selects the data-authored `ReconstructOrReplace` edge while retaining the same canonical ruin record and asset count; no quest-only asset is spawned.
- Added `FDAPromiseLedger::GetBrokenPromises(ActionTags)` with exact `FName` matching and deterministic PromiseID ordering. Promise registration persists immutable definition/source/conflict/fulfillment data and is stable-ID idempotent even after fulfillment or breach.
- Added guarded action commit using semantic records (`ActionId`, normalized tags, world tick, fulfilled PromiseIDs, breached PromiseIDs) rather than ID-only keys. Only the same normalized input replays as `AlreadyApplied`; conflicting stable-ID reuse is rejected before and after save/load. Rejections publish no partial promise, action, history, or revision mutation.
- Made `FDACampaignSnapshot::HistoryTags` the sole campaign history-tag authority. The duplicate operation-conflict array is removed; surrender and capture outcome mutations clone the full campaign, write top-level history plus per-record audit, validate the candidate, and publish only on success.
- Added reflected timer/world/WorldAsset/citizen/faction/economy/relationship payload variants and evaluation context. Choice nodes require explicit branch selection, while multiple automatic matches return `AmbiguousTransition` without mutation.
- Strengthened Core validation for exact enum ranges, graph/stage membership, instance-manifest identity, fingerprint integrity, terminal-state consistency, complete binding resolution, event scope, promise-appropriate resolution tags, and semantic action result identity/ticks.
- Added bidirectional promise/action integrity. Every nonlegacy resolved promise names exactly one semantic action result, and that result must agree on ActionID, result kind, normalized tag, and world tick. Every nonlegacy action tag must also exist in the sole top-level history ledger, so a checksummed removal is rejected and an identical replay cannot bless missing history.
- Added exact completed-path validation for quests and events. Persisted completion arrays must be the ordered authored path from Start/Initial to immediately before Current; duplicate, skipped, reordered, terminal, nonancestor, and branch-exclusive entries are rejected, including explicit Choice paths.
- Replaced ANSI/locale-sensitive definition fingerprints with canonical length-delimited UTF-8 bytes and finite IEEE-754 bit encodings. Array counts and scalar fields are explicit, Unicode source identities remain distinct, and negative zero is normalized to positive zero.
- Persisted fulfillment/breach results now mirror `CommitAction` exactly: the resolution tag is the lexical first exact tag in its own tag class, and the resolving action contains no exact tag from the opposite class. Simultaneous matches and noncanonical result tags invalidate the whole snapshot before replay.
- Canonical v3 fingerprints encode only the active tagged-payload fields. Inactive timer/condition/WorldAsset storage must remain at finite canonical defaults, so None, Timer, and WorldAsset payloads cannot hide NaN, infinity, or semantically irrelevant garbage. The locale/ANSI v1 hash exists only as a read-only schema-v8 compatibility verifier and is never used for new fingerprints.
- Advanced the canonical wire schema from v7 to v9. v7→v8 promotes operation-conflict history into the top-level ledger and converts ID-only actions into explicit legacy-identity records without inventing missing tags or result semantics. It stamps provenance on each resolved promise, and v8→v9 trusts that marker only when the migration invocation started directly at schema v7. Native/mixed-era v8 orphans cannot borrow an unrelated legacy ActionID. Before publishing a v8 fingerprint refresh, migration validates definition semantics and requires the stored hash to match either the legacy v1 algorithm or current canonical bytes. A v7 active quest/event lacking immutable definition data fails migration with a trusted-registry requirement instead of fabricating source definitions.

## TDD evidence

`QuestGraphSpec.cpp`, `PromiseSpec.cpp`, `CaptureSpec.cpp`, and migration regressions were authored before the corresponding production changes. New RED cases cover same-version start/source/edge/stage replacement; strict persisted-state validation; reflected condition/timer/World/WorldAsset payloads; choice and automatic ambiguity; semantic ActionID conflict before/after save; bidirectional promise/action identity; canonical result-tag classification and opposite-class exclusion; missing result, ActionID, record, and canonical history; exact completed paths; UTF-8 and negative-zero fingerprint behavior; inactive NaN/infinity payloads; one history authority; per-promise v7 provenance; native-v8 link promotion; mixed-era orphan rejection; and checksummed manifest/scope/history tampering.

### RED attempts

The initial missing-production-code invocation and later focused invalid-edge, ambiguous-action, and stable-ID/idempotence invocations all reached the same tool boundary:

```text
$ UnrealEditor DominionAscendant.uproject -ExecCmds='Automation RunTests Dominion.World.Narrative; Quit' -unattended -nop4 -NullRHI -NoSound
/bin/bash: line 1: UnrealEditor: command not found
exit_code=127
```

UE 5.8 is absent, so the intended compile/assertion failures could not be observed. Exit 127 is environment-blocked and is not an observed Automation RED or a pass.

The review-fix RED command was also blocked identically:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject -ExecCmds='Automation RunTests Dominion.World.Narrative;Quit' -unattended -nop4 -nosplash
/bin/bash: line 1: UnrealEditor-Cmd: command not found
exit_code=127
```

The round-two RED command was likewise blocked before compilation:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject -unattended -nop4 -NullRHI -NoSound -ExecCmds='Automation RunTests Dominion.World.Narrative+Dominion.Core.Save.Migration;Quit' -TestExit='Automation Test Queue Empty'
/bin/bash: UnrealEditor-Cmd: command not found
exit_code=127
```

The round-three RED command was blocked before the new assertions could execute:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject -unattended -nop4 -NullRHI -NoSound -ExecCmds='Automation RunTests Dominion.World.Narrative.Promise+Dominion.World.Narrative.QuestGraph+Dominion.Core.Save.Migration;Quit' -TestExit='Automation Test Queue Empty'
/bin/bash: UnrealEditor-Cmd: command not found
exit_code=127
```

### Final GREEN attempt

```text
$ UnrealEditor-Cmd DominionAscendant.uproject -unattended -nop4 -NullRHI -NoSound -ExecCmds='Automation RunTests Dominion.World.Narrative+Dominion.Core.Save.Migration+Dominion.Gameplay.Capture;Quit' -TestExit='Automation Test Queue Empty'
/bin/bash: line 1: UnrealEditor-Cmd: command not found
exit_code=127
```

This remains environment-blocked and is **not** a passing UHT, UBT, or Automation result.

The final round-two verification repeated the focused narrative/migration command and was blocked at the same boundary:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject -unattended -nop4 -NullRHI -NoSound -ExecCmds='Automation RunTests Dominion.World.Narrative+Dominion.Core.Save.Migration;Quit' -TestExit='Automation Test Queue Empty'
/bin/bash: UnrealEditor-Cmd: command not found
exit_code=127
```

The final round-three verification was also blocked before UHT/UBT or Automation:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject -unattended -nop4 -NullRHI -NoSound -ExecCmds='Automation RunTests Dominion.World.Narrative.Promise+Dominion.World.Narrative.QuestGraph+Dominion.Core.Save.Migration;Quit' -TestExit='Automation Test Queue Empty'
/bin/bash: UnrealEditor-Cmd: command not found
exit_code=127
```

## Behavior coverage

1. Unreachable Resolution, cyclic graphs, invalid edge targets/conditions, and exact taxonomy.
2. Real WorldAsset binding, destruction adaptation, exact `ReconstructOrReplace`, and no duplicate asset.
3. Stable-ID/idempotent quest and event starts plus conflicting version reuse rejection.
4. Canonical save round trip for active quest, world event, and promise records.
5. Exact promise conflict tags with stable result ordering and no prefix matching.
6. Warning-before-commit, explicit confirmed breach, fulfillment, ambiguous-action rejection, stale revision rejection, atomic invalid-input rejection, and stable ActionID replay.
7. Promise definition re-registration after breach does not reset durable state.
8. Checksummed schema-v6 migration to empty narrative state plus schema-v7 history/action-authority and explicit legacy-resolution provenance migration to v9.
9. Immutable manifest/fingerprint persistence and same-version start/source/edge/stage replacement rejection.
10. Core rejection of unknown enums, missing graph membership/bindings, terminal mismatches, invalid event scope, and wrong promise resolution tags.
11. Timer plus citizen/faction/economy/relationship payload evaluation, explicit Choice selection, and ambiguous automatic-transition rejection.
12. Semantic action replay equivalence/conflict before and after canonical save/load.
13. Checksummed quest-manifest fingerprint and event-scope tamper rejection.
14. Full-snapshot capture/surrender publication with top-level history and exact-once replay guards.
15. Bidirectional promise/action/result linkage, canonical-history reconciliation, and checksummed missing-history rejection.
16. Exact authored completed-node/stage paths, including Choice branch exclusivity, ordering, terminal exclusion, and immediate-predecessor shape.
17. Canonical length-delimited UTF-8 fingerprints with deterministic finite-double bits and negative-zero normalization.
18. Typed WorldCondition and real bound WorldAsset payload evaluation without asset fabrication.
19. Exact `CommitAction` result classification: canonical first same-class tag and no opposite-class match, with invalid replay refusal.
20. Per-promise schema-v7 provenance, native-v8 semantic link promotion, mixed-era orphan rejection, and stale-v8 fingerprint rejection before publication.
21. Active-only tagged-payload fingerprints plus finite canonical inactive fields for None, Timer, and WorldAsset variants.

## Supplementary static verification

The review source audit verifies reflected generated-header ordering, schema v9 with case-7 and case-8 migrations, exactly one reflected `HistoryTags` authority, no production `AppliedActionIds`, exact result classification, per-promise direct-v7 provenance and ActionID linkage, canonical v3 UTF-8 hashing with active-only payload fields, a separately named v1 compatibility verifier, exact path validators, manifest/action/payload/choice symbols, no narrative actor/object spawning or `WorldAssets.Add`, no conflict markers, and clean whitespace.

```text
STATIC_TASK18_REVIEW_CHECKS=PASS
```

These checks establish source structure and whitespace only. They do not replace Unreal compilation or Automation.

## Remaining verification

On a UE 5.8-capable Windows runner, build the editor target and execute:

```text
UnrealEditor-Cmd.exe DominionAscendant.uproject -unattended -nop4 -NullRHI -NoSound -ExecCmds="Automation RunTests Dominion.World.Narrative+Dominion.Core.Save.Migration;Quit" -TestExit="Automation Test Queue Empty"
```

That run is required before claiming UHT/API correctness, successful linking, or real GREEN.
