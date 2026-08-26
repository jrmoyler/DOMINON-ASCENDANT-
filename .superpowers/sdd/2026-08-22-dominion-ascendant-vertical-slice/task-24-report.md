# Task 24 report — Daxton Rhe final encounter and Leader outcomes

## Implementation

- Added one save-owned `FDADaxtonCampaignState` and a stateless gameplay facade. Phase I persists powered armor, hardened cover, Forge Guard reinforcement, and Grand Forge production actions; the exact transition occurs at 60% armor integrity. Phase II persists Overdrive, Heat, Coolant Stability, Resource Hunger, and all five interactions (damage, coolant disable, supply redirect, production hack, worker shutdown) in one replayable action ledger. Phase III requires at least two distinct systemic interactions, exposes the exact four choice objectives, and cannot resolve from zero armor alone.
- The encounter consumes the canonical Broker route history (and reconciles it with a completed `ConquestState.ResolvedRoute` when present), `relationship.synara.forgeweave` Trust/Respect/Grievance, the existing Forgeweave production/reserve/logistics/hunger authority, Grand Forge world asset and module-damage records, and live citizen/job signals. It does not add a subsystem, encounter-local resource ledger, duplicate relationship, or duplicate world asset.
- Added the exact six persistent Leader outcomes: `Governor`, `IndustrialAdvisor`, `AlliedForgeLord`, `Exile`, `Prisoner`, and `Dead`. The route/relationship/objective matrix makes all six reachable and re-proves the selected outcome during campaign validation. Resolution uses a candidate snapshot, one durable action ID, one exact outcome history tag, and one terminal World Tick.
- Advanced the canonical save schema from 15 to 16. Authentic v15 documents receive an inactive/unresolved Daxton authority only in the v15-to-v16 migration; schema 1–15 future-field injection is rejected; current saves require the object; and the encounter phase, meters, action identities, interaction replay, objectives, route matrix, systemic authorities, and outcome tag are validated after load.
- Carried the Task 23 breaker prerequisite before adding Daxton persistence. Every JSON object is recursively scanned before Unreal's last-wins parser; escaped member names are decoded (including Unicode surrogate pairs), and duplicate decoded keys are rejected. Recomputed-checksum regressions cover literal-plus-escaped `conquestState`, `regionalCrisis`, `resolutionRecords`, and `jointCrisisHistoryRevisionAtResolution` spellings. The exact-integer causal-proof gate remains active for every source schema `>=15`.
- Added a strict fingerprinted source manifest, JSON schema, editor commandlet, real FBX import tasks, generated Leader DataAsset path, ValidateOnly parity, pre-cook script, and BuildGraph hook. Missing artist sources fail closed. No `.uasset` placeholder or other binary asset was created.
- Preserved the locked scope: 64 definitions, 24 quests, six events, and 27 UI screens. Task 25 still owns Convergence Authority.

## Outcome matrix

| Leader state | Canonical route | Systemic/objective and relationship gate |
|---|---|---|
| Governor | Influence | workers evacuated, union stabilized, Trust/Respect `>=50`, Grievance `<50` |
| Industrial Advisor | Economic | Forge saved, union stabilized, Respect `>=50`, Grievance `<70` |
| Allied Forge Lord | Alliance | Forge saved, workers evacuated, union stabilized, Trust/Respect `>=65`, Grievance `<50` |
| Exile | Force or Influence | Daxton defeated plus at least one systemic objective |
| Prisoner | Force | Daxton defeated and workers evacuated |
| Dead | Force | Daxton defeated plus at least one systemic objective |

## Files

```text
.superpowers/sdd/2026-08-22-dominion-ascendant-vertical-slice/task-24-report.md
Build/Graph/VerticalSliceDaxtonEncounter.xml
Build/Scripts/PreCookDaxtonEncounter.sh
Content/DA/Manifests/DaxtonEncounter.json
Content/DA/Manifests/DaxtonEncounter.schema.json
Content/DA/Manifests/README.md
Source/DominionCore/Private/Campaign/DADaxtonCampaignState.cpp
Source/DominionCore/Private/Save/DACampaignSaveGame.cpp
Source/DominionCore/Private/Save/DASaveService.cpp
Source/DominionCore/Public/Campaign/DADaxtonCampaignState.h
Source/DominionCore/Public/Save/DACampaignSaveGame.h
Source/DominionCore/Public/Save/DASaveJsonFields.h
Source/DominionCore/Public/Save/DASaveSchema.h
Source/DominionEditor/DominionEditor.Build.cs
Source/DominionEditor/Private/DADaxtonContentCommandlet.cpp
Source/DominionEditor/Public/DADaxtonContentCommandlet.h
Source/DominionGameplay/Private/Boss/Daxton/DADaxtonEncounter.cpp
Source/DominionGameplay/Public/Boss/Daxton/DADaxtonEncounter.h
Source/DominionTests/Private/Core/SaveMigrationSpec.cpp
Source/DominionTests/Private/Gameplay/DaxtonEncounterSpec.cpp
Tests/Content/test_daxton_encounter_manifest.py
Tests/Content/test_first_hour_quest_manifest.py
```

## Strict TDD evidence

The four manifest/source-generation tests were written first and produced the intended executable missing-feature RED:

```text
$ python3 -m unittest Tests.Content.test_daxton_encounter_manifest
FFFF
Ran 4 tests
FAILED (failures=4)
RED_EXIT=1
```

The encounter phase/interaction/objective/outcome behaviors, v15 migration, future-authority injection, forged Leader outcome, and escaped decoded-key/container tamper regressions were also authored before their production implementations. Their focused RED could not launch because the UE runner is absent; no C++ assertion failure is claimed:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject -unattended -nop4 -nosplash -ExecCmds="Automation RunTests Dominion.Gameplay.Daxton.Encounter; Automation RunTests Dominion.Core.Save.Migration; Quit" -TestExit="Automation Test Queue Empty"
/bin/bash: line 1: UnrealEditor-Cmd: command not found
UE_RED_EXIT=127
```

## Available GREEN and final verification

The relevant static suite was run once after implementation:

```text
$ python3 -m unittest discover -s Tests/Content -p 'test_*.py'
......................................................
----------------------------------------------------------------------
Ran 54 tests

OK
STATIC_SUITE_EXIT=0
```

The fresh final focused check was also green:

```text
$ python3 -m unittest Tests.Content.test_daxton_encounter_manifest
....
----------------------------------------------------------------------
Ran 4 tests in 0.023s

OK
FOCUSED_STATIC_EXIT=0
```

Additional final checks:

```text
$ python3 -m json.tool Content/DA/Manifests/DaxtonEncounter.json
JSON_MANIFEST_EXIT=0
$ python3 -m json.tool Content/DA/Manifests/DaxtonEncounter.schema.json
JSON_SCHEMA_EXIT=0
$ bash -n Build/Scripts/PreCookDaxtonEncounter.sh
SHELL_SYNTAX_EXIT=0
$ git diff --check
(no output)
DIFF_CHECK_EXIT=0
```

The locked-count inspection returned:

```text
{'definitions': 64, 'quests': 24, 'events': 6, 'screens': 27}
```

`find Content/DA/Leaders Content/Characters/Daxton -type f -name '*.uasset' -print` returned no paths. `git diff --name-only -- .../progress.md` also returned no path.

An attempted broad `python3 -m unittest discover -s Tests` found no tests because the repository's Python tests are under the explicit `Tests/Content` discovery root; it exited 5 and is not cited as GREEN evidence.

## Save-integrity prerequisite coverage

- `Dominion.Core.Save.Migration` now has current-schema (v16 `SaveCampaign`) recomputed-checksum cases for all required escaped decoded-key and ambiguous-container variants. They exercise the real load boundary and expect `InvalidDocument` before permissive conversion.
- Those are executable UE Automation tests, but they could not execute in this environment because `UnrealEditor-Cmd` is absent. The available Python suite verifies the static content boundary only; it is not represented as runtime save-integrity coverage.
- The generic decoded-member scanner runs before checksum/migration parsing on every loaded document. Thus literal and Unicode-escaped aliases cannot reach Unreal JSON last-wins conversion, while the existing raw exact-integer causal-proof rules continue to reject missing, fractional, exponent, overflow, and ambiguous numeric forms.

## Self-review

- Reviewed the staged diff for duplicate persistent authorities, zero-health-only completion, phase-threshold drift, action-ID reuse, interaction replay gaps, post-objective meter drift, non-atomic outcome resolution, route/outcome-tag ambiguity, historical future-field leakage, decoded-key parser gaps, commandlet no-op behavior, fabricated binaries, and locked-scope changes.
- Corrected Phase III transition/objective actions to persist unique IDs and participate in standalone replay validation. Corrected Phase I action counters to reconcile with their durable IDs, prevented production-loop mutation after the interaction ledger starts, and kept v16 as the only migration step that materializes Daxton state.
- The six-state enum is new in schema v16; no existing enum was reordered. Copy/move/assignment paths include the new state. `progress.md` was not edited. No subagent was used.

## Remaining concern / required UE handoff

`UnrealEditor-Cmd` is absent. Therefore this task does **not** claim UHT, UBT, UE compilation, C++ Automation, real FBX import, Leader package generation, or commandlet ValidateOnly GREEN. The final focused Automation attempt and pre-cook attempt both returned exit 127:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject -unattended -nop4 -nosplash -ExecCmds="Automation RunTests Dominion.Gameplay.Daxton.Encounter; Automation RunTests Dominion.Core.Save.Migration; Quit" -TestExit="Automation Test Queue Empty"
/bin/bash: line 1: UnrealEditor-Cmd: command not found
FINAL_AUTOMATION_EXIT=127

$ Build/Scripts/PreCookDaxtonEncounter.sh DominionAscendant.uproject
Build/Scripts/PreCookDaxtonEncounter.sh: line 7: UnrealEditor-Cmd: command not found
FINAL_PRECOOK_EXIT=127
```

On a UE5.8 runner, execute those same two commands after placing the declared real authored FBX files under `ContentSource/Characters/Daxton`.

---

## Fix round 1 — production ownership and replay proof (2026-08-25)

Base: `de9df8bf6153cf42704ba9700c255b765b6e2961`

### Review findings addressed

- `UDAWorldStateSubsystem` now exposes the production encounter commands. Every command copies the single persisted campaign, applies the gameplay mutation to that candidate, calls full `FDACampaignSnapshot::Validate`, and then uses the existing narrative/signal/World-Tick compare-and-swap. A failed, duplicate, stale, or overflowing action never replaces or publishes the owner state.
- Hardened cover is a real `module.hardened_cover` Grand Forge structural module. Reinforcement is a real Grand Forge job opening plus canonical defense pressure, with one live-signal revision. Worker shutdown clears the canonical Ironheart citizen jobs and Forgeweave assignments and increments `LiveSignals.MutationRevision` exactly once.
- Every post-start action persists an exact before/after canonical projection. Deterministic replay covers reserve, logistics, throughput, Hunger, Overdrive, defense pressure, Grand Forge structure/modules/production-disabled state, all citizen/job authorities, relationship Trust/Respect/Grievance, route/history, and live-signal revision. Terminal comparison is performed for active and resolved saves; resolved saves no longer skip throughput/Hunger reconciliation.
- Phase I now has a second durable, non-damage completion path. Production, hardened cover, and Forge Guard reinforcement must all be proven before the industrial objective transitions to Phase II at full armor. The original exact 60% armor transition remains intact.
- The permissive fabricated matrix test was replaced with owner-level end-to-end outcomes: Influence/Governor, Economic/Industrial Advisor, Alliance/Allied Forge Lord, and Force/Prisoner. Each path asserts every command, validates snapshots, save-loads Phase I/II/III and the resolved outcome, and rejects duplicate resolution without publishing.
- Daxton generation is now part of the existing Forgeweave production cook graph. `/Game/DA/Leaders` and `/Game/Characters/Daxton` are always cooked; `DALeader` is an `AlwaysCook` primary asset scan. Commandlet validation verifies exact package, class, loadability, and Asset Registry visibility for the Leader and all real imported character packages.

### Strict TDD evidence

The cook/AssetManager assertions were added before config/graph production changes and produced executable RED:

```text
$ python3 -m unittest Tests.Content.test_daxton_encounter_manifest
..F.
======================================================================
FAIL: test_generation_is_registered_with_the_production_cook_and_asset_manager
AssertionError: '+DirectoriesToAlwaysCook=(Path="/Game/DA/Leaders")' not found
----------------------------------------------------------------------
Ran 4 tests
FAILED (failures=1)
RED_EXIT=1
```

The production-owner success/rejection/idempotence/failure-atomicity tests, the alternate Phase-I objective, four outcome paths, phase/outcome save-loads, and seven refreshed-checksum tamper cases were also authored before their production implementation. The focused executable RED attempt was blocked before test discovery because the UE runner is absent:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject -unattended -nop4 -nosplash '-ExecCmds=Automation RunTests Dominion.Gameplay.Daxton.Encounter; Quit' '-TestExit=Automation Test Queue Empty'
/bin/bash: line 1: UnrealEditor-Cmd: command not found
UE_RED_EXIT=127
```

### Final verification

```text
$ python3 -m unittest discover -s Tests/Content -p 'test_*.py'
......................................................
----------------------------------------------------------------------
Ran 54 tests in 0.021s

OK
STATIC_SUITE_EXIT=0
```

```text
$ python3 -m unittest Tests.Content.test_daxton_encounter_manifest
....
----------------------------------------------------------------------
Ran 4 tests in 0.002s

OK
FOCUSED_STATIC_EXIT=0
```

The locked-count check returned exactly:

```text
{'definitions': 64, 'quests': 24, 'events': 6, 'screens': 27}
```

JSON manifest/schema parsing, `bash -n Build/Scripts/PreCookDaxtonEncounter.sh`, and `git diff --check` all exited 0. No `.uasset` appeared under the exact Daxton package roots, and no progress file changed.

The prerequisite decoded-key breaker remains covered by executable/current-schema UE Automation regressions in `SaveMigrationSpec`: refreshed-checksum Unicode-escaped duplicate decoded keys, ambiguous escaped containers, and an escaped duplicate current-schema causal-proof key. The final runner attempt remains honestly blocked:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject -unattended -nop4 -nosplash '-ExecCmds=Automation RunTests Dominion.Gameplay.Daxton.Encounter; Automation RunTests Dominion.Core.Save.Migration; Quit' '-TestExit=Automation Test Queue Empty'
/bin/bash: line 1: UnrealEditor-Cmd: command not found
UE_FINAL_EXIT=127

$ Build/Scripts/PreCookDaxtonEncounter.sh DominionAscendant.uproject
Build/Scripts/PreCookDaxtonEncounter.sh: line 7: UnrealEditor-Cmd: command not found
PRECOOK_EXIT=127
```

### Self-review and remaining verification boundary

- Reviewed owner revision semantics: reinforcement and worker shutdown each mutate live signals once; non-signal actions preserve the revision; CAS requires either exact unchanged signals/revision or one changed revision.
- Reviewed replay/action bijection: every Phase-I mechanic, industrial objective, systemic interaction, Phase-III transition, choice objective, and resolution has one unique proof record; counters and exact action IDs are reconciled.
- Reviewed terminal/objective authority: route and relationships cannot drift from the initial proof; Save Grand Forge, Evacuate Workers, union stabilization, and Leader resolution re-prove their canonical preconditions; resolved canonical projection is compared without an early-out.
- Reviewed UHT/module boundaries: save-owned proof structs remain in `DominionCore`; `DominionGameplay` depends only on Core; the production owner resides in `DominionWorld`, which already publicly depends on Gameplay; tests already depend on Core/Gameplay/World.
- `UnrealEditor-Cmd` is absent, so this fix round does not claim UHT, UBT, C++ Automation execution, real FBX import, generated Leader package creation, Asset Registry runtime validation, or a real cook GREEN.

---

## Fix round 2 — post-resolution campaign evolution (2026-08-25)

Base: `f810243cab03e72efe398f3239ee588056f5b5e2`

### Regression and fix

Round 1 correctly replayed every encounter action, but then compared the entire current campaign projection to the resolution-time projection forever. That incorrectly made unrelated later citizen/job signals, history, relationship reasons, live-signal revisions, and Forgeweave economy ticks invalidate a resolved campaign.

The full persisted before/after action ledger is still replayed without relaxation. Only the resolved-to-current reconciliation is now scoped to Daxton-owned provenance:

- the exact Grand Forge identity and exact terminal values for action-owned `module.coolant`, `module.production`, and (when deployed) `module.hardened_cover`;
- every Ironheart citizen protected by the worker-shutdown action and the absence of assignments removed by that action;
- the exact Forge Guard reinforcement opening when that Phase-I action occurred;
- nondecreasing live-signal revision from the terminal proof;
- the exact canonical route tag, worker/Forge objective tags, encounter-resolution tag, outcome tag, and persistent Leader state;
- outcome eligibility evaluated from the immutable resolution-time Trust/Respect/Grievance proof, rather than mutable later relationship aggregates.

Unrelated citizens, openings, assignments, history tags, later relationship reasons/aggregates, revisions, and Forgeweave economy values may append/evolve through their normal canonical validation and owner CAS paths. Unresolved encounter states retain exact whole-projection equality.

### Strict TDD evidence

The new UE Automation regression resolves Allied Forge Lord through `UDAWorldStateSubsystem`, then asserts successful owner commits for a new citizen, job opening, assignment, unrelated history, a canonical `-30 Trust` diplomacy reason that makes current Trust fall below the old resolution threshold, and one Forgeweave World Tick. It requires full `Validate` and save/load after the evolution. A second fixture performs the same evolution before refreshed-checksum tampering of a protected worker, action-owned coolant/production modules, worker proof history, route, and outcome history.

Those tests were authored before the scoped production change. The available RED attempt could not reach the assertions because the executable runner is absent:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject -unattended -nop4 -nosplash '-ExecCmds=Automation RunTests Dominion.Gameplay.Daxton.Encounter; Quit' '-TestExit=Automation Test Queue Empty'
/bin/bash: line 1: UnrealEditor-Cmd: command not found
UE_RED_EXIT=127
```

### Final verification

```text
$ python3 -m unittest discover -s Tests/Content -p 'test_*.py'
......................................................
----------------------------------------------------------------------
Ran 54 tests in 0.019s

OK
STATIC_SUITE_EXIT=0

$ python3 -m unittest Tests.Content.test_daxton_encounter_manifest
....
----------------------------------------------------------------------
Ran 4 tests in 0.003s

OK
FOCUSED_STATIC_EXIT=0
```

`git diff --check` exited 0. Locked counts remained `{'definitions': 64, 'quests': 24, 'events': 6, 'screens': 27}`. No progress file or content definitions changed.

The final executable attempt remains honestly blocked:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject -unattended -nop4 -nosplash '-ExecCmds=Automation RunTests Dominion.Gameplay.Daxton.Encounter; Automation RunTests Dominion.Core.Save.Migration; Quit' '-TestExit=Automation Test Queue Empty'
/bin/bash: line 1: UnrealEditor-Cmd: command not found
UE_FINAL_EXIT=127
```

### Self-review / remaining boundary

- Mutation review: deleting resolved scoping makes the first post-resolution signal fail; using current relationship values makes the `-30 Trust` evolved Allied outcome fail; dropping module/worker/tag provenance checks makes refreshed-checksum tamper cases load.
- The resolved validator does not accept arbitrary partial state: all new state still passes the normal campaign, LiveSignals, diplomacy, Forgeweave, structural, save-envelope, and checksum validators before scoped Daxton reconciliation.
- Action-owned module checks are deliberately per exact module ID, not a frozen copy of every unrelated Grand Forge module. Protected-worker checks are deliberately per citizen/removed assignment, not a frozen copy of all later citizens and jobs.
- `UnrealEditor-Cmd` is absent, so this round does not claim UHT, UBT, C++ Automation execution, or runtime save/load GREEN.

---

## Fix round 3 — resolution relationship provenance (2026-08-25)

Base: `7b27ec6f48a6d4a5d02be9acf7e56331feebf448`

### Regression and scoped proof

The resolution-time relationship eligibility proof no longer derives its authority only
from the mutable Daxton projection. Schema v17 persists the exact ordered prefix of
canonical Forgeweave diplomatic reason mutation IDs and its count at the existing
`ResolvedWorldTick` boundary. Validation now:

- requires every protected prefix ID to equal the canonical reason at that exact index;
- requires protected reasons to exist no later than the resolution tick and later
  appended reasons to exist strictly after it;
- replays Trust, Respect, and Grievance from that canonical prefix;
- requires those replayed aggregates to equal the fully replayed terminal Daxton action
  projection and uses only the replayed aggregates for Leader outcome eligibility.

Later canonical reasons still append and current relationship aggregates may evolve.
Changing all embedded before/after relationship aggregates coherently no longer changes
the eligibility authority. Direct mutation of the protected reason prefix is also
rejected.

Resolution records the prefix through the production encounter path before validation
and owner CAS publication. Schema v16 promotes to v17 by deriving the ordered prefix
from reasons at or before `ResolvedWorldTick`; non-prefix reasons must already be beyond
that boundary. An authentic inactive v16 payload migrates to empty provenance, while a
v16 payload containing either v17 field is rejected as future-field injection even
behind a recomputed checksum.

### Strict TDD evidence

Before production changes, the gameplay regression was expanded to resolve through
`UDAWorldStateSubsystem` after actually advancing Grand Forge production, deploying
hardened cover, reinforcing the Forge Guard, and completing the non-damage Phase-I
objective. It then performs unrelated post-resolution citizen/opening/assignment,
Forgeweave tick, history, and canonical diplomacy evolution. Refreshed-checksum cases
cover the protected worker, restored removed assignment, production/coolant/cover
modules, reinforcement opening, live-signal baseline, route, outcome history,
`LeaderState`, a coherent rewrite of every embedded relationship projection, and a
direct protected-prefix rewrite. Schema migration tests cover authentic v16 defaults
and injected v17 provenance.

The executable static schema assertion produced the expected RED:

```text
$ python3 -m unittest Tests.Content.test_first_hour_quest_manifest
FAIL: test_round_five_schema_v11_and_unconditional_v9_future_scan
AssertionError: 'CurrentSchemaVersion = 17' not found in ...
Ran 26 tests in 0.007s
FAILED (failures=1)
STATIC_RED_EXIT=1
```

The focused C++ RED attempt remained blocked honestly:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject -unattended -nop4 -nosplash '-ExecCmds=Automation RunTests Dominion.Gameplay.Daxton.Encounter; Automation RunTests Dominion.Core.Save.Migration; Quit' '-TestExit=Automation Test Queue Empty'
/bin/bash: line 1: UnrealEditor-Cmd: command not found
UE_RED_EXIT=127
```

### Final verification

```text
$ python3 -m unittest discover -s Tests/Content -p 'test_*.py'
......................................................
----------------------------------------------------------------------
Ran 54 tests in 0.017s

OK
STATIC_SUITE_EXIT=0

$ python3 -m unittest Tests.Content.test_daxton_encounter_manifest
....
----------------------------------------------------------------------
Ran 4 tests in 0.002s

OK
FOCUSED_STATIC_EXIT=0

$ git diff --check
DIFF_CHECK_EXIT=0
```

Locked counts remain exactly `{'definitions': 64, 'quests': 24, 'events': 6,
'screens': 27}`; no definition, quest, event, UI, generated `.uasset`, or progress file
changed. The decoded-key save-integrity prerequisite remains covered in current-schema
`SaveMigrationSpec` cases for refreshed-checksum Unicode-escaped duplicate keys,
escaped container spellings, and causal-proof keys.

The final executable attempt remains honestly unavailable:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject -unattended -nop4 -nosplash '-ExecCmds=Automation RunTests Dominion.Gameplay.Daxton.Encounter; Automation RunTests Dominion.Core.Save.Migration; Quit' '-TestExit=Automation Test Queue Empty'
/bin/bash: line 1: UnrealEditor-Cmd: command not found
UE_FINAL_EXIT=127
```

### Self-review / remaining boundary

- The protected relationship authority is a scoped ordered reason prefix; later reasons,
  unrelated relationship reasons, citizens, jobs, history, economy, and revisions are
  not frozen.
- All previous resolved action-owned reconciliation remains active, including exact
  cover/coolant/production modules, reinforcement opening, signal baseline, protected
  citizen and removed assignment, route/outcome tags, and Leader state.
- Schema v17 is additive: no enum order or content count changed. The v16 future-field
  scan runs before migration and the v16 migration validates the promoted full campaign.
- Reviewed UHT/module boundaries: the two reflected fields use Core types in the existing
  Core save-owned struct; Gameplay already consumes Core relationship types; SaveService
  already includes the campaign/world authorities used for migration.
- `UnrealEditor-Cmd` is absent, so this round does not claim UHT, UBT, C++ Automation
  execution, runtime schema-v16 load, or runtime tamper GREEN.

---

## Fix round 4 — same-tick ordered relationship suffix compatibility (2026-08-25)

Base: `345a17a97d580391a01470ba3c7752d6d6f0c281`

### Regression and migration repair

- Schema v17 now treats the persisted count and ordered mutation-ID array as the exact
  terminal relationship prefix. Protected reasons still replay by exact index and ID
  and cannot postdate resolution. A suffix must remain after that prefix in the
  canonical ledger and cannot predate resolution, but it may be appended later in the
  same World Tick.
- Schema v16 no longer assumes that every reason at or before `ResolvedWorldTick`
  belongs to the terminal proof. Migration replays every ordered candidate prefix,
  matches its Trust/Respect/Grievance projection against the terminal Resolve Leader
  action proof, and rechecks the persisted Leader outcome eligibility. Exactly one
  boundary must match; no-match and ambiguous documents fail migration. The chosen
  prefix IDs are copied in their original canonical order and the suffix is untouched.
- The save schema remains 17. No reflected type, enum order, content manifest, locked
  count, or progress file changed.

### Strict TDD evidence

The migration Automation regressions were authored before production changes. The
authentic fixture resolves Allied Forge Lord through `UDAWorldStateSubsystem`, appends
an unrelated canonical reason through `UDADiplomacySystem` and the owner CAS later in
the same resolution tick, converts the saved envelope to v16, migrates/loads/validates,
then advances the restored production owner. It asserts the exact two-ID prefix and
preserved third suffix. Separate cases require rejection after a checksummed protected
reason tamper produces no matching boundary and after canceling same-tick suffixes
produce two eligible matching boundaries.

The focused RED attempt was blocked before test discovery because the UE runner is not
installed; no C++ assertion RED is claimed:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject -unattended -nop4 -nosplash '-ExecCmds=Automation RunTests Dominion.Core.Save.Migration; Quit' '-TestExit=Automation Test Queue Empty'
/bin/bash: line 1: UnrealEditor-Cmd: command not found
UE_RED_EXIT=127
```

### Available verification

```text
$ python3 -m unittest discover -s Tests/Content -p 'test_*.py'
......................................................
----------------------------------------------------------------------
Ran 54 tests in 0.019s

OK
STATIC_SUITE_EXIT=0
```

`git diff --check` exited 0. The final UE Automation attempt remained unavailable:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject -unattended -nop4 -nosplash '-ExecCmds=Automation RunTests Dominion.Gameplay.Daxton.Encounter; Automation RunTests Dominion.Core.Save.Migration; Quit' '-TestExit=Automation Test Queue Empty'
/bin/bash: line 1: UnrealEditor-Cmd: command not found
UE_FINAL_EXIT=127
```

### Self-review / remaining boundary

- Mutation review: restoring the strict-greater suffix boundary rejects the authentic
  owner CAS; taking every same-tick reason reintroduces terminal projection failure;
  accepting the first or last matching boundary makes one ambiguity assertion fail;
  skipping projection replay admits the checksummed reason tamper; skipping outcome
  eligibility admits an ineligible migrated proof.
- Prefix timestamps remain causally bounded, suffixes remain append-only by exact array
  position, and schema-v17 identity/replay validation is unchanged for protected rows.
  Later-tick round-3 evolution remains valid as a strict superset of the same-tick rule.
- `UnrealEditor-Cmd` is absent, so this round does not claim UHT, UBT, C++ Automation
  execution, runtime schema-v16 migration GREEN, or runtime tamper/ambiguity GREEN.
