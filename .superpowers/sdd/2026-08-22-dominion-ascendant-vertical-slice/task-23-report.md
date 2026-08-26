# Task 23 report — conquest meters and four Forgeweave routes

## Implementation

- Added the exact player-facing `Military Sovereignty`, `Economic Autonomy`, `Civic Legitimacy`, and `Alliance Readiness` authority to the canonical campaign snapshot. Its immutable mutation ledger replays every meter, caps every default Economic action at 15, forbids stored Influence as a civic-zero source, and persists exact Force/Economic/Influence/Alliance weight history after each synchronization.
- `FDAConquestSystem` consumes existing campaign authorities only. Force reads Ironheart control-zone tags, completed capture records, elite-defeat history, and disabled shield/command structural records. Economic reads real completed contracts plus delivery records, canonical freight-corridor ownership, Forgeweave relationship Dependence, and the Task-22 emergency-finance crisis resolution. Influence reads the completed Workers' Signal quest, Task-22 Mara/worker history, the real Foundry service-crisis resolution, credibility history, and persisted faction support. Stored wallet Influence is never read.
- Alliance Readiness is the exact average of Trust, Shared Interest (Forgeweave Compatibility), Crisis Resolution, and Respect. Union completion requires average `>=80`, every component `>=65`, no relationship/history Major Grievance, completed Third Foundry, successful joint crisis, and voluntary Relic transfer.
- Added route-specific completion gates: Force requires sovereignty/surrender, Operation Iron Veil, Daxton resolution, changed Ironheart ownership, calculated post-war Loyalty, and persistent damage; Economic requires autonomy threshold, fulfilled trade, Supply Noose, and restructuring; Influence requires legitimacy threshold, worker endorsement, and a real service-crisis solution; Alliance uses the hard union gates. Completion is a copy/validate/commit mutation with one stable action ID and exact route history tag.
- Added a frozen, strict five-quest source manifest for quests 20–24, exact package paths, JSON schema, transient build path, real editor DataAsset commandlet, ValidateOnly cache parity, and pre-cook script/BuildGraph. No `.uasset` file was fabricated or checked in.
- Kept scope at three civilizations, 24 authored quests so far (Task 25 owns the specified 25th, Convergence Authority), six events, 64 definitions, and the existing 27 UI surfaces.

## Files

```text
.superpowers/sdd/2026-08-22-dominion-ascendant-vertical-slice/task-23-report.md
Build/Graph/VerticalSliceForgeweaveConquest.xml
Build/Scripts/PreCookForgeweaveConquest.sh
Content/DA/Manifests/ForgeweaveConquest.json
Content/DA/Manifests/ForgeweaveConquest.schema.json
Content/DA/Manifests/README.md
Source/DominionCore/Private/Campaign/DAConquestCampaignState.cpp
Source/DominionCore/Private/Save/DACampaignSaveGame.cpp
Source/DominionCore/Private/Save/DASaveService.cpp
Source/DominionCore/Public/Campaign/DAConquestCampaignState.h
Source/DominionCore/Public/Save/DACampaignSaveGame.h
Source/DominionCore/Public/Save/DASaveJsonFields.h
Source/DominionCore/Public/Save/DASaveSchema.h
Source/DominionEditor/Private/DAForgeweaveConquestContentCommandlet.cpp
Source/DominionEditor/Public/DAForgeweaveConquestContentCommandlet.h
Source/DominionTests/Private/Core/SaveMigrationSpec.cpp
Source/DominionTests/Private/World/ConquestRoutesSpec.cpp
Source/DominionWorld/Private/Conquest/DAConquestSystem.cpp
Source/DominionWorld/Public/Conquest/DAConquestState.h
Source/DominionWorld/Public/Conquest/DAConquestSystem.h
Tests/Content/test_first_hour_quest_manifest.py
Tests/Content/test_forgeweave_conquest_manifest.py
```

## Strict TDD evidence

The manifest contract and the real C++ behavior specs were authored before production code. The corrected executable static RED was the intended missing-feature failure:

```text
$ python3 -m unittest Tests.Content.test_forgeweave_conquest_manifest -v
test_assets_are_generated_not_fabricated ... FAIL
test_exact_five_quests_complete_the_twenty_four_authored_so_far ... FAIL
test_manifest_fingerprint_and_graphs_fail_closed ... FAIL
test_routes_and_system_evidence_are_explicit ... FAIL
Ran 4 tests in 0.001s
FAILED (failures=4)
STATIC_RED_EXIT=1
```

The focused C++ RED invocation could not launch because this environment has no UE runner; no Automation assertion failure is claimed:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject -run=Automation -Test=Dominion.World.Conquest.Routes -unattended -nop4 -NullRHI
/bin/bash: line 1: UnrealEditor-Cmd: command not found
UE_RED_EXIT=127
```

Save v14 migration/default/tamper/source-authority tests were also written before their migration and semantic-source implementation. Their focused RED invocation was blocked by the same exact exit 127, so it is not represented as an executed failing C++ test.

## Available GREEN and final verification

```text
$ python3 -m unittest Tests.Content.test_first_hour_quest_manifest Tests.Content.test_regional_crisis_manifest Tests.Content.test_vertical_slice_manifest Tests.Content.test_forgeweave_conquest_manifest Tests.UI.test_task21_authority_contract Tests.UI.test_vertical_slice_ui_manifest
....................................................................
----------------------------------------------------------------------
Ran 68 tests in 0.020s

OK
STATIC_REGRESSION_EXIT=0

$ python3 -m json.tool Content/DA/Manifests/ForgeweaveConquest.json >/dev/null && python3 -m json.tool Content/DA/Manifests/ForgeweaveConquest.schema.json >/dev/null && bash -n Build/Scripts/PreCookForgeweaveConquest.sh
CONTENT_SYNTAX_EXIT=0
```

```text
$ git diff --cached --check
(no output)
STAGED_DIFF_CHECK_EXIT=0
```

## Save/versioning behavior

- Advanced the canonical save schema from 13 to 14. Schema v14 serializes `conquestState` atomically with the campaign checksum.
- Authentic schema v13 receives deterministic 100/100/100/0 meter defaults, zero weights, an empty mutation/weight history, and no resolution. Every older migration step strips the future field so its historical payload remains exact.
- Any schema 1–13 payload containing an injected `conquestState` is rejected before current-DTO parsing. A current schema document must contain an object-valued `conquestState`.
- Validation replays all meter deltas and every historical weight checkpoint, checks exact revision/tick continuity, bounds all components, reconciles resolution tags, and proves each mutation against the canonical capture/damage/region/trade/diplomacy/history/crisis/quest/faction authority. Recomputing the envelope checksum cannot make a replay-incoherent meter or fabricated contract source load successfully.

## Self-review

- Reviewed the complete diff for a second conquest owner, mission-only trade/capture/faction copies, currency-to-legitimacy shortcuts, generic purchase paths, enum/version compatibility, migration future-field leakage, non-atomic state writes, route-history replay gaps, generated-cache semantic/path drift, fake binary assets, and scope-count changes.
- Corrected synchronization rollback so failed validation restores the original conquest state; route completion now commits from a candidate snapshot. Historical route-weight rows are checked at their exact mutation revision rather than only checking the newest row. Mutation validation also rejects replay-coherent records whose claimed canonical source does not exist.
- `EDAForgeweaveRoute` is a new schema-v14 enum and has no prior serialized numeric contract to preserve. Existing enums and prior save versions were not reordered or rewritten.
- `progress.md` was not edited. There are zero checked-in quest `.uasset` files.

## Remaining concern / required UE handoff

`UnrealEditor-Cmd` is absent. Therefore this task does **not** claim UHT, UBT, UE compilation, C++ Automation, commandlet generation, or commandlet ValidateOnly GREEN. On a UE5.8 runner execute:

```text
UnrealEditor-Cmd DominionAscendant.uproject -run=Automation -Test=Dominion.World.Conquest.Routes,Dominion.Core.Save.Migration -unattended -nop4 -NullRHI
UnrealEditor-Cmd DominionAscendant.uproject -run=DAForgeweaveConquestContent -unattended -nop4
UnrealEditor-Cmd DominionAscendant.uproject -run=DAForgeweaveConquestContent -ValidateOnly -unattended -nop4
```

The final two attempted commands here both returned `/bin/bash: line 1: UnrealEditor-Cmd: command not found` with exit 127.

## Fix round 1

### Review findings addressed

- The existing `UDAWorldStateSubsystem` remains the one production campaign/world owner. Its normal vetoable world-tick preparation now synchronizes conquest evidence before candidate validation, and `CompleteForgeweaveConquestRoute` synchronizes, proves, and compare-and-swap commits a resolution atomically through that owner. No conquest subsystem, actor, or second persisted state was added. The integration behavior spec drives a normal `AdvanceWorldTicks(1)` and attempts completion through this production entry point.
- Every ledger mutation now has a deterministic ID derived from the exact canonical authority it consumes. Trade/capture/damage/history identities remain direct; dependence and Alliance components bind exact diplomatic reason mutation IDs; faction support binds its exact value-reason action ID; and crisis effects bind the exact Foundry Shortage resolution action GUID. Validation enforces route, meter, delta, identity, authority existence, and one-time source/effect consumption. Alliance is rebuilt from the relationship reason ledger plus the exact crisis record instead of repeated generic relationship snapshots.
- `FDAConquestAuthorityValidator` is Core-owned and shared by runtime completion and `FDACampaignSnapshot::Validate`. A persisted resolution must have its exact route tag and re-pass the full selected Force/Economic/Influence/Alliance gates from canonical campaign state. Save regressions forge all four tagged resolutions behind recomputed current-schema checksums and expect semantic rejection.

### Focused tests-first evidence

The production-owner integration, duplicate-source tamper, and exhaustive forged-resolution tests were written before the fix implementation. The focused RED could not execute because the UE runner is absent:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject -run=Automation -Test=Dominion.World.Conquest.Routes,Dominion.Core.Save.Migration -unattended -nop4 -NullRHI
/bin/bash: line 2: UnrealEditor-Cmd: command not found
FIX_ROUND_1_RED_EXIT=127
```

The post-implementation focused attempt has the same environmental limitation; this is not a C++ GREEN claim:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject -run=Automation -Test=Dominion.World.Conquest.Routes,Dominion.Core.Save.Migration -unattended -nop4 -NullRHI
/bin/bash: line 2: UnrealEditor-Cmd: command not found
FIX_ROUND_1_GREEN_ATTEMPT_EXIT=127
```

Available executable regression and final diff validation:

```text
$ python3 -m unittest Tests.Content.test_first_hour_quest_manifest Tests.Content.test_regional_crisis_manifest Tests.Content.test_vertical_slice_manifest Tests.Content.test_forgeweave_conquest_manifest Tests.UI.test_task21_authority_contract Tests.UI.test_vertical_slice_ui_manifest
....................................................................
----------------------------------------------------------------------
Ran 68 tests in 0.023s

OK

$ git diff --check
(no output)
DIFF_CHECK_EXIT=0
```

### Save/versioning and tamper behavior

- The current schema remains v14; no serialized enum order or earlier migration payload was rewritten. Authentic schema 1–13 behavior and the deterministic v13-to-v14 empty authority remain unchanged. An empty migrated conquest projection is allowed until the production tick synchronizes available canonical evidence.
- Current-schema validation now rejects replay-coherent duplicate source consumption even when an attacker changes the duplicate mutation ID, result, meter/weight totals, route history, revision, and envelope checksum coherently.
- Alliance projection rows use exact diplomatic reason IDs and exact crisis resolution GUIDs. Generic relationship reuse, fabricated alliance snapshot ticks, generic faction-map sources, and generic crisis sources are not accepted.
- Resolution validation no longer trusts the route history tag: the shared Core proof rechecks every selected-route threshold, quest, trade, crisis, ownership, damage, loyalty, relationship, grievance, and history gate as applicable.

### Self-review and concerns

- Reviewed the Core/World dependency direction: Core owns only persisted validation/rules and consumes the canonical snapshot aggregate; World owns projection and normal gameplay orchestration; `UDAWorldStateSubsystem` remains the sole mutable campaign owner. The completion adapter publishes only through existing compare-and-swap validation and broadcasts.
- Reviewed exact-once semantics across relationship reasons, crisis effects, history tags, contracts, captures, damage, regions, quests, and faction reasons. A single crisis resolution may legitimately have one distinct effect per route, so its effect-specific source authority is unique while retaining the same canonical action GUID.
- Reviewed rollback and post-resolution ticking: failed projection restores the original conquest state, failed completion commits nothing, and resolved campaigns validate without trying to append new evidence on every later world tick.
- `progress.md` was not edited. No scope counts or binary assets changed.
- Remaining concern: `UnrealEditor-Cmd` is unavailable, so UHT/UBT compilation and the new C++ Automation behaviors still require the UE5.8 runner command above. No UE pass is claimed.

## Fix round 2

### Implementation

- Added a successful production-owner integration behavior. It restores a fully validated canonical campaign with exact diplomatic reasons, a fingerprinted completed Third Foundry quest, and the two required joint/voluntary history authorities; invokes `UDAWorldStateSubsystem::CompleteForgeweaveConquestRoute`; and asserts exactly one publication containing the durable action ID, Alliance route, resolution state, and `forgeweave_allied` history tag. The existing rejected-completion transaction coverage remains.
- Made signed diplomatic projection associative with the canonical aggregate. For each Trust/Compatibility/Respect reason prefix, projection now tracks the raw signed sum separately from its clamped component value and emits only the exact marginal change in `Clamp(raw sum)`. Save validation recomputes the same raw-prefix marginal from the exact reason mutation ID. Thus `-10,+25` projects Trust `15` and Alliance Readiness `3.75`, rather than sequentially losing the negative reason and projecting `6.25`.
- Kept worker endorsements immutable. A stored worker mutation may retain either exact canonical `mara_numbers_worker_coalition` or `workers_protected` source if that tag exists and Workers' Signal is completed. Later stronger evidence does not rewrite the earlier source. The existing unique mutation ID/source cardinality checks still reject reuse, and the new regression explicitly rejects a fabricated worker source.
- Generalized the exact joint-crisis history marginal from `(100 - canonical crisis base) / 4`, allowing the canonical joint-success history authority to supply readiness even when there is no separate Foundry Shortage resolution record. This supports the legitimate production-owner Alliance fixture without introducing a route-local crisis copy.

### Strict TDD and verification evidence

The successful owner commit, signed `-10,+25`, and immutable worker-source regressions were authored first. The focused RED remained environmentally blocked:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject -run=Automation -Test=Dominion.World.Conquest.Routes,Dominion.Core.Save.Migration -unattended -nop4 -NullRHI
/bin/bash: line 2: UnrealEditor-Cmd: command not found
FIX_ROUND_2_RED_EXIT=127
```

After implementation, the same focused invocation remained unavailable and is not claimed as GREEN:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject -run=Automation -Test=Dominion.World.Conquest.Routes,Dominion.Core.Save.Migration -unattended -nop4 -NullRHI
/bin/bash: line 2: UnrealEditor-Cmd: command not found
FIX_ROUND_2_GREEN_ATTEMPT_EXIT=127
```

Available regression:

```text
$ python3 -m unittest Tests.Content.test_first_hour_quest_manifest Tests.Content.test_regional_crisis_manifest Tests.Content.test_vertical_slice_manifest Tests.Content.test_forgeweave_conquest_manifest Tests.UI.test_task21_authority_contract Tests.UI.test_vertical_slice_ui_manifest
....................................................................
----------------------------------------------------------------------
Ran 68 tests in 0.019s

OK

$ git diff --check
(no output)
FIX_ROUND_2_DIFF_CHECK_EXIT=0
```

### Self-review

- Rechecked production ownership: success and rejection both traverse the same `UDAWorldStateSubsystem` candidate/validate/compare-and-swap path, and completion publishes one owned immutable snapshot only after the full campaign validates.
- Rechecked signed histories at lower and upper clamps, later appended reasons, and save replay. Raw-prefix marginal calculation is identical in World projection and Core save validation; zero-impact reasons create no conquest mutation, while every emitted row retains its exact canonical reason ID and remains one-time consumable.
- Rechecked worker history evolution: the immutable mutation is validated against its stored tag, not whichever eligible tag is strongest today; Workers' Signal remains required; fabricated tags and duplicate sources remain rejected.
- Schema stays v14 with no enum reorder or migration change. `progress.md` and binary assets were not edited.
- Remaining concern: UE compilation and Automation still require a UE5.8 runner because `UnrealEditor-Cmd` is absent here.

## Fix round 3

### Implementation

- Made the Alliance crisis component order-independent and exactly saturated. Runtime now totals only canonical Alliance crisis mutations already persisted, computes the desired component for the next exact source, and appends only the positive remaining marginal. A history-first joint success records `25`; a later non-collapse/Brokered resolution records nothing because the component is already saturated. A crisis-first non-collapse resolution records `16.25`, and later joint history records exactly `8.75`.
- Preserved immutable rows and exact identities. Existing history or resolution mutations are never rewritten. Repeated synchronization adds no duplicate mutation, and any replay-coherent ledger whose crisis-source deltas exceed the canonical `25` component is rejected before synchronization can normalize it.
- Save validation now replays crisis-source contribution in persisted mutation/revision order. It also proves that the exact crisis resolution record existed by the mutation's `WorldTick`; joint history requires the crisis base available at its own recorded tick to have already been consumed. This accepts both legitimate evidence orders while rejecting retroactive reorder, omitted-base, duplicate, and saturation tampering.
- Added focused behaviors for history-first, crisis-first, exact `16.25 + 8.75` marginal completion, repeated-sync exact-once behavior, and an internally replay-coherent `41.25` double-count fixture that synchronization must reject.

### Strict TDD evidence

The ordering, saturation, duplicate, and replay-coherent tamper behaviors were written first. The focused RED invocation was blocked by the absent runner:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject -run=Automation -Test=Dominion.World.Conquest.Routes -unattended -nop4 -NullRHI
/bin/bash: line 2: UnrealEditor-Cmd: command not found
FIX_ROUND_3_RED_EXIT=127
```

The post-implementation focused attempt remained unavailable and is not claimed as GREEN:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject -run=Automation -Test=Dominion.World.Conquest.Routes -unattended -nop4 -NullRHI
/bin/bash: line 2: UnrealEditor-Cmd: command not found
FIX_ROUND_3_GREEN_ATTEMPT_EXIT=127
```

Available executable regression:

```text
$ python3 -m unittest Tests.Content.test_first_hour_quest_manifest Tests.Content.test_regional_crisis_manifest Tests.Content.test_vertical_slice_manifest Tests.Content.test_forgeweave_conquest_manifest Tests.UI.test_task21_authority_contract Tests.UI.test_vertical_slice_ui_manifest
....................................................................
----------------------------------------------------------------------
Ran 68 tests in 0.019s

OK

$ git diff --check
(no output)
FIX_ROUND_3_DIFF_CHECK_EXIT=0
```

### Self-review

- Checked history-first, crisis-first, same-tick crisis-before-history, Brokered saturation, non-collapse marginal completion, later repeated synchronization, and over-saturated tamper paths. All converge to one canonical crisis contribution of `Crisis Resolution / 4`, never more than `25`.
- Validation uses immutable conquest ledger order plus exact source `WorldTick`, rather than current evidence strength alone. A later crisis record cannot retroactively change an earlier joint-history delta, while a forged history row cannot pretend an already-available crisis base did not exist.
- No new authority, persisted field, schema version, enum, route, quest, event, definition, UI surface, or binary asset was added. `UDAWorldStateSubsystem` remains the sole production campaign owner.
- `progress.md` was not edited. Remaining concern is unchanged: UE5.8 UHT/UBT and Automation require a runner unavailable here.

## Fix round 4

### Implementation

- Added a minimal durable same-tick causal proof to the exact Foundry Shortage resolution record: `JointCrisisHistoryRevisionAtResolution` is zero when no joint-history conquest mutation existed as the crisis action began, otherwise it stores that mutation's exact 1-based immutable ledger revision. The regional-crisis action remains the authority; this field only binds its observed conquest revision and creates no second owner.
- Runtime records the proof from the canonical conquest ledger before applying the crisis action. Save validation resolves the exact crisis action/source identity and compares explicit mutation revisions. A same-tick record precedes a mutation only when the stored joint-history revision is zero or the mutation's explicit revision is later. It no longer infers causality from `Record.WorldTick <= Mutation.WorldTick` or container iteration order.
- History-first at tick T therefore retains its immutable `25` row when a non-collapse crisis resolves later at T. Crisis-first at T retains the exact `16.25` crisis row followed by the exact `8.75` history marginal. Repeated synchronization remains saturated at `25`, and crisis-derived Economic/Influence mutations can still consume the same exact action later in the tick when their revisions prove they follow it.
- Advanced the current save schema from 14 to 15. Authentic v14 saves receive a deterministic proof: an earlier-tick joint-history row is bound by its exact revision; same-tick v14 data is crisis-first because the v14 validator could not accept the history-first shape. Raw v14 payloads that inject the schema-v15 field are rejected before DTO parsing.

### Strict TDD evidence

The production-owner full-validation regression, both same-tick orders, save/reload, recomputed-checksum tamper, v14 migration, and v14 future-field injection behaviors were authored before production code. The focused RED remained blocked by the unavailable UE runner, so no Automation assertion failure is claimed:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject -run=Automation -Test=Dominion.World.Conquest.Routes,Dominion.Core.Save.Migration -unattended -nop4 -NullRHI
/bin/bash: line 1: UnrealEditor-Cmd: command not found
FIX_ROUND_4_FULL_RED_EXIT=127
```

After implementation, the same focused command remained unavailable and is not represented as C++ GREEN:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject -run=Automation -Test=Dominion.World.Conquest.Routes,Dominion.Core.Save.Migration -unattended -nop4 -NullRHI
/bin/bash: line 1: UnrealEditor-Cmd: command not found
FIX_ROUND_4_GREEN_ATTEMPT_EXIT=127
```

Available executable regression:

```text
$ python3 -m unittest Tests.Content.test_first_hour_quest_manifest Tests.Content.test_regional_crisis_manifest Tests.Content.test_vertical_slice_manifest Tests.Content.test_forgeweave_conquest_manifest Tests.UI.test_task21_authority_contract Tests.UI.test_vertical_slice_ui_manifest
....................................................................
----------------------------------------------------------------------
Ran 68 tests in 0.022s

OK
```

### Save/tamper and owner coverage

- A real `UDAWorldStateSubsystem` tick triggers the crisis and synchronizes joint history first. `ResolveFoundryShortage` then commits at the same canonical tick, the full snapshot validates, current-schema save/reload preserves `25`, and the next normal `AdvanceWorldTicks(1)` preparation commits without adding a duplicate mutation.
- A separate crisis-first same-tick fixture fully validates with `16.25 + 8.75`, saves, and supplies the authentic v14 migration case. Exact action GUIDs, mutation IDs, source authorities, source IDs, and one-time source consumption remain required.
- A checksummed current-schema fixture changes the history-first resolution proof from its exact prior revision to zero and recomputes the envelope checksum. Load rejects it as `InvalidDocument`; changing the checksum cannot convert a later same-tick action into an earlier authority.

### Self-review

- Rechecked history-first and crisis-first at the same tick, earlier/later ticks, Brokered saturation, non-collapse marginal completion, same-tick crisis-derived route effects, repeated synchronization, save/reload, and schema-v14 promotion. Immutable conquest rows are never rewritten.
- Rechecked ownership and dependency direction: the persisted proof lives on the canonical crisis action record, references the canonical conquest mutation revision, and is populated inside the existing world-owned crisis transaction. There is no new subsystem, actor, route-local state, global clock, or iteration-order authority.
- Rechecked tamper boundaries: a nonzero proof must name the exact joint-history mutation revision at or before the resolution tick; zero cannot omit an earlier-tick joint row; and a same-tick crisis-source mutation must explicitly fall after the recorded boundary. Duplicate and fabricated source checks are unchanged.
- No enum was reordered. Schema 15 is an honest new field with a registered v14 migration and raw-v14 future-field rejection. `progress.md`, scope counts, manifests, and binary assets were not edited.
- Remaining concern: `UnrealEditor-Cmd` is absent, so UHT/UBT compilation and the C++ Automation behaviors still require the UE5.8 runner command above. No UE pass is claimed.

## Fix round 5

### Implementation

- Current schema-v15 loads now validate the original checksummed JSON before migration or permissive UStruct conversion. The canonical `regionalCrisis.resolutionRecords` array must be structurally unambiguous, every record must contain exactly one numeric `jointCrisisHistoryRevisionAtResolution` property, and each raw property value must be a nonnegative base-10 integer with no fraction, exponent, coercion, leading-zero ambiguity, duplicate property, or value beyond the exact JSON integer bound. The existing semantic validation still binds the parsed value to the exact conquest mutation revision.
- Schema v14 bypasses this source-v15 shape gate and retains the registered v14-to-v15 migration, which deterministically materializes the missing proof before conversion of the final current snapshot.
- Added the crisis-first production regression missing from round 4: a crisis contributes `16.25`, later same-tick joint history contributes `8.75`, the fully validated campaign saves and reloads at exactly `25`, the reloaded snapshot restores into `UDAWorldStateSubsystem`, and a subsequent normal owner tick preserves the proof and mutation revision without duplicate evidence.

### Strict TDD evidence

The current-v15 missing/fractional/wrong-type recomputed-checksum cases and the crisis-first save/reload/retick behavior were authored before the load gate. The focused RED invocation remained environmentally blocked, so no Automation assertion failure is claimed:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject -run=Automation -Test=Dominion.World.Conquest.Routes,Dominion.Core.Save.Migration -unattended -nop4 -NullRHI
/bin/bash: line 1: UnrealEditor-Cmd: command not found
FIX_ROUND_5_RED_EXIT=127
```

After implementation, the same focused invocation remained unavailable and is not represented as C++ GREEN:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject -run=Automation -Test=Dominion.World.Conquest.Routes,Dominion.Core.Save.Migration -unattended -nop4 -NullRHI
/bin/bash: line 1: UnrealEditor-Cmd: command not found
FIX_ROUND_5_GREEN_ATTEMPT_EXIT=127
```

Available executable regression:

```text
$ python3 -m unittest Tests.Content.test_first_hour_quest_manifest Tests.Content.test_regional_crisis_manifest Tests.Content.test_vertical_slice_manifest Tests.Content.test_forgeweave_conquest_manifest Tests.UI.test_task21_authority_contract Tests.UI.test_vertical_slice_ui_manifest
....................................................................
----------------------------------------------------------------------
Ran 68 tests in 0.028s

OK

$ git diff --check
(no output)
FIX_ROUND_5_DIFF_CHECK_EXIT=0
```

### Self-review

- Rechecked every field introduced by schema v15: the causal revision is the only new persisted field. Current-v15 preconversion parsing now requires the campaign envelope through checksum validation, the existing canonical world and conquest objects, one regional-crisis object, one resolution-record array, object-valued records, and one exact causal integer on every record. Empty record arrays remain valid and resolved records still re-pass full semantic causality after conversion.
- Rechecked missing, fractional, signed, string/boolean/null, exponent, overflow, duplicate-key, duplicate-container, and encoded/ambiguous property shapes. None can be defaulted or coerced to zero before semantic validation. Exact zero remains valid for crisis-first data, and a nonzero value remains bounded again by the canonical conquest mutation revision.
- Rechecked the v14 path independently: raw v14 future-field injection remains a migration failure, authentic v14 omits the field, and the v14-to-v15 step remains the only authority that synthesizes it.
- Rechecked crisis-first same-tick persistence across validation, save, reload, authoritative-owner restore, subsequent world preparation, exact `16.25 + 8.75 = 25`, mutation cardinality, and stable zero proof. `progress.md`, schema number, enums, scope counts, manifests, and binary assets were not edited.
- Remaining concern is unchanged: the environment has no `UnrealEditor-Cmd`, so UE5.8 compilation and C++ Automation still require the runner command above. No UE pass is claimed.
