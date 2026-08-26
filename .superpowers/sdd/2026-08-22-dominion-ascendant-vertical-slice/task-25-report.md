# Task 25 report — First Ascension, Replication, Forgeweave unlocks, and Autonomous Factory

## Implementation

- Added one save-owned `FDAAscensionCampaignState` to the existing `FDACampaignSnapshot`; no new campaign subsystem or facility state was introduced. `UDAWorldStateSubsystem::CompleteFirstAscension` copies the sole owner state, re-validates the completed conquest and persisted Daxton outcome, applies every reward to one candidate, runs full snapshot validation, and publishes with the existing narrative/signal/World-Tick compare-and-swap. Missing proof, stale authority, capacity failure, or any invalid result publishes nothing.
- The unique transaction captures conquest and Leader action provenance, grants the Forge Relic, exposes the exact 15 Forgeweave definitions, unlocks Replication and Fusion/Autonomous Factory, adds the exact v0.8 `+10 Influence` and `+10 Insight`, advances Founder Hall to visual state 3 / Relic slot 1 / hidden chamber open, writes `first_relic_acquired` and `convergence_authority_1_of_20`, completes quest 25, and projects the exact `CONVERGENCE AUTHORITY: 1/20` presentation label. Presentation reads only committed state and cinematic skipping never gates gameplay.
- Replication is clock-bound to a persisted 12-Development-Cycle cadence. It accepts an owned, placeable, non-Legendary Asset definition, derives a deterministic new instance from the globally unused action ID, persists `AcquisitionSource=Replication` and the exact source instance GUID, rejects early/new-action/overflow/collision attempts, and treats exact action replay as idempotent across save/reload.
- Autonomous Factory uses existing authorities: jobs applies `-80%` workforce, economy contributes one idempotent `+25%` named throughput modifier, adjacency contributes one `+15%` Industrial-neighbor construction modifier, and each committed cycle routes `Dependency +0.20` through `UDASystemicPressureSystem` while the canonical Forgeweave state receives `Resource Hunger +0.15`. Save-owned records audit the exact world asset, cycle, before/after values, and dependency reason without duplicating facility lifecycle state.
- Advanced save schema 17 to 18. Authentic v17 documents materialize only inactive Ascension defaults; pre-v18 Ascension and Replication-provenance future fields are rejected; current documents require `ascensionState`; semantic validation rejects refreshed-checksum reward/provenance/cadence/pressure tampering while permitting later canonical campaign evolution.
- Added strict source manifest/schema, exact fingerprint parity, doctrine and quest DataAsset generation, real StaticMesh/Texture import tasks, LevelSequence generation, source preflight, exact Asset Registry class/path/fingerprint validation, AlwaysCook coverage, and the existing Forgeweave production graph hook. Missing building or shot-list sources fail before package creation. No `.uasset` or fabricated binary was added.

## TDD evidence

The four Task 25 manifest/count/source-generation tests were written first and produced executable RED:

```text
$ python3 -m unittest Tests.Content.test_first_ascension_manifest
FFFF
Ran 4 tests
FAILED (failures=4)
RED_EXIT=1
```

The C++ owner transaction, rollback, idempotence, cadence/replay/save-load, canonical Factory paths, post-Ascension reload/evolution, schema migration, future-field, and recomputed-checksum tamper specs were also authored before their production paths. The intended executable RED and final Automation runs were blocked before discovery because UE is absent; no C++ pass is claimed:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject -unattended -nop4 -nosplash '-ExecCmds=Automation RunTests Dominion.World.Ascension.First; Automation RunTests Dominion.Core.Save.Migration; Quit' '-TestExit=Automation Test Queue Empty'
/bin/bash: line 1: UnrealEditor-Cmd: command not found
UE_EXIT=127
```

## Available GREEN and final checks

```text
$ python3 -m unittest Tests.Content.test_first_ascension_manifest
....
Ran 4 tests
OK

$ python3 -m unittest discover -s Tests/Content -p 'test_*.py'
..........................................................
Ran 58 tests
OK
```

JSON manifest/schema parsing, XML parsing, `bash -n Build/Scripts/PreCookFirstAscension.sh`, and `git diff --check` exited 0. Locked-count inspection returned exactly:

```text
{'definitions': 64, 'starter_instances': 60, 'quests': 25, 'events': 6, 'screens': 27, 'forgeweave_unlocks': 15}
```

`find . -type f -name '*.uasset' -print` returned no paths; the numstat binary scan passed; `progress.md` was unchanged.

## Self-review and UE handoff

- Reviewed the diff for partial Ascension publication, action reuse across conquest/crisis/narrative/Daxton, duplicate quests/history/unlocks, replay-created cards, cadence overflow, source-instance drift, transient clock cadence, modifier stacking, duplicate per-cycle pressure, later-evolution invalidation, historical future-field leakage, stale checksums, permissive content parsing, fabricated assets, and locked-count drift.
- Corrected Factory cycle ordering to stable world-asset GUID order, made economy/adjacency effects idempotent, reconciled every pressure audit to the canonical world asset and dependency reason, and made schema 18 the only migration step that materializes both Ascension and Replication provenance.
- `UnrealEditor-Cmd` is absent, so this task does not claim UHT/UBT compilation, C++ Automation GREEN, real import, generated package creation, Asset Registry runtime validation, or cook GREEN. The final focused run and production pre-cook both returned 127. On a UE5.8 runner, place the declared real building and shot-list sources under `ContentSource`, then run the two commands recorded above and `Build/Scripts/PreCookFirstAscension.sh DominionAscendant.uproject`.

---

## Review fix round 1 (2026-08-26)

### Findings resolved

- The Ascension presentation projection now exposes the exact ordered sequence `systems halt/react -> Forge Relic emerges -> world transit -> Founder Hall receives Relic -> unlocks`, the committed Founder Hall state, the canonical cinematic soft path, and explicit skippable/non-authoritative playback flags through `UDAWorldStateSubsystem`. The content commandlet now requires exactly five ordered authored shot rows, resolves each referenced nonempty shot `ULevelSequence`, builds a real `UMovieSceneCinematicShotTrack` with one ranged section per shot, fingerprints that shot source, validates the saved composite, and fails generation when any source/section/range is absent. This checkout still contains no authored source files or generated `.uasset`; no generation success is claimed.
- Autonomous Factory authorship is now exact and production-owned: 8 construction cycles, 80 Capital/0 Insight craft cost, operational Replication Forge requirement, and Power 24/Data 24. First Ascension exposes the blueprint without auto-granting an instance; the world owner crafts concrete `AcquiredBy=Crafting` instances, the existing placement transaction creates construction records, and Development Cycle construction resolves persisted fractional progress. Jobs applies the authored -80% requirement at signal publication, facility economy applies +25% throughput, and actual footprint-aware adjacency applies +15% construction speed.
- Development Cycle precommit now previews every requested cycle and vetoes before clock mutation if the exact economy/construction/pressure candidate cannot publish. Each cycle aggregates every operational Factory in stable GUID order into one audit row, allowing multiple cycles in one World Tick without artificial time advancement; dependency reasons accept same-tick ordered chains. Transient pressure thresholds update only after the campaign publication point. Capacity rejection is covered as a clock-does-not-advance case.
- Replication rarity filtering now rejects only `Legendary` after placeable Asset-card/type binding; the real owned `synara.the_thinking_spire` Wonder is covered. Founder Hall now persists, validates, reloads, and projects exactly 20 one-based positions, with only slot 1 active/occupied by `relic.forge`.
- Exact content remains 64 definitions and 60 starter instances; Autonomous Factory remains outside the starter deck and therefore optional.

### Strict TDD evidence

Covering tests were authored first in `Tests/Content/test_first_ascension_manifest.py`, `Source/DominionTests/Private/Content/VerticalSliceContentSpec.cpp`, and `Source/DominionTests/Private/World/AscensionSpec.cpp`. The frozen-value compatibility expectation was updated in `Tests/Content/test_vertical_slice_manifest.py` after its full-suite RED exposed the newly required fields.

Focused executable RED before production changes:

```text
$ python3 -m unittest Tests.Content.test_first_ascension_manifest
.FF.
Ran 4 tests in 0.002s
FAILED (failures=2)
RED_EXIT=1
```

The failures were the missing exact Factory construction/crafting/utility values and the absent `UMovieSceneCinematicShotTrack` conversion markers. The C++ RED launch was blocked before test discovery, so it is not claimed as executed:

```text
$ UnrealEditor-Cmd "$(pwd)/DominionAscendant.uproject" -unattended -nop4 -NullRHI '-ExecCmds=Automation RunTests Dominion.World.Ascension.First; Automation RunTests Dominion.Content.VerticalSlice; Quit' '-TestExit=Automation Test Queue Empty'
/bin/bash: line 1: UnrealEditor-Cmd: command not found
UE_RED_EXIT=127
```

Focused GREEN:

```text
$ python3 -m unittest Tests.Content.test_first_ascension_manifest
....
Ran 4 tests in 0.001s
OK
GREEN_EXIT=0
```

The first full content run then correctly exposed the stale exact-value fixture:

```text
$ python3 -m unittest discover -s Tests/Content -p 'test_*.py'
...................................................F......
Ran 58 tests in 0.020s
FAILED (failures=1)
CONTENT_GREEN_EXIT=1
```

After adding the same Factory values to that frozen expectation, final available suites were GREEN:

```text
$ python3 -m unittest discover -s Tests/Content -p 'test_*.py'
..........................................................
Ran 58 tests in 0.019s
OK

$ python3 -m unittest discover -s Tests/UI -p 'test_*.py'
..................
Ran 18 tests in 0.004s
OK
```

Final manifest fingerprint/count inspection returned:

```text
{'fingerprint': 'a179ed0ed1e9735624dfb0aceb90c30216586c3a', 'definitions': 64, 'starter_instances': 60}
FINAL_AVAILABLE_GREEN_EXIT=0
```

### Final unavailable checks and self-review

```text
$ UnrealEditor-Cmd "$(pwd)/DominionAscendant.uproject" -unattended -nop4 -NullRHI '-ExecCmds=Automation RunTests Dominion.World.Ascension.First; Automation RunTests Dominion.Content.VerticalSlice; Automation RunTests Dominion.Core.Save.Migration; Quit' '-TestExit=Automation Test Queue Empty'
/bin/bash: line 1: UnrealEditor-Cmd: command not found
FINAL_UE_EXIT=127

$ Build/Scripts/PreCookFirstAscension.sh "$(pwd)/DominionAscendant.uproject"
Build/Scripts/PreCookFirstAscension.sh: line 7: UnrealEditor-Cmd: command not found
PRECOOK_EXIT=127
```

- Re-reviewed all six findings for candidate-only authority, same-tick ordering, per-cycle/per-factory aggregation, clock vetoes, modifier stacking, footprint adjacency, blueprint ownership, exact rarity/type eligibility, presentation gating, shot-section usability, save validation, and locked counts. Corrected transient pressure mutation to occur only after publication and removed false cross-cycle pressure continuity assumptions because other canonical World-Tick owners may legitimately change those values between Factory cycles.
- Replaced uncertain MovieScene/playback and numeric parsing usages with stable range/integer checks, preserved existing adjacency callers through default 1x1 footprints, and preserved existing integer construction behavior while persisting fractional speed progress.
- `git diff --check`, JSON parsing, XML parsing, `bash -n`, fingerprint/count inspection, and the no-binary numstat scan exited 0. `find . -type f -name '*.uasset' -print` returned no paths. No Unreal compilation, Automation pass, import, generated package, Asset Registry validation, or cook pass is claimed in this environment.

---

## Review fix round 2 (2026-08-26)

### Ascension cinematic source and generator

- Added `ContentSource/Cinematics/ForgeweaveAscension.shotlist.json`, a complete source-authored 30 fps / 510-frame specification with exactly five ordered beats: systems halt/react, Forge Relic emergence, world transit, Founder Hall receipt, and unlocks. Every shot has an exact generated package path, duration, moving camera transform, field of view, ordered action target/payloads, and timed audio/VFX cue identifiers. The companion `ForgeweaveAscension.shotlist.schema.json` closes all root and nested objects and constrains identity, rates, counts, paths, frames, transforms, payloads, and cue namespaces.
- Replaced the commandlet's external child-`ULevelSequence` loading dependency with source conversion. It now generates and saves one child sequence per beat, including a spawned Cine Camera, exact playback/spawn/transform/camera-cut ranges, linear camera keys, constant authored FOV, and locked, timed MovieScene markers carrying every action/audio/VFX payload. The composite `CS_ForgeweaveAscension` is then built from the five saved child sequences in source order.
- Preflight rejects unknown fields, wrong identity/rate/order/path/duration, duplicate packages, invalid camera ranges, missing/extra/out-of-order actions or cues, and malformed payloads before cinematic package mutation. Saved child and composite packages receive source/shot/payload fingerprints; validation reloads them through the Asset Registry and requires exact class, path, metadata, ranges, tracks, camera binding, transform key counts, and timed payload markers.
- Added `Build/Scripts/GenerateFirstAscensionCinematic.sh`, which runs generation then independent `-CinematicOnly -ValidateOnly`. This lets an Unreal-equipped runner generate the complete cinematic without the unrelated still-absent building import sources. The production full-content commandlet continues to generate and validate the same children and composite during pre-cook.

### Strict TDD evidence

The covering contract was authored first in `Tests/Content/test_first_ascension_manifest.py`. Initial RED proved that both the complete cinematic source and source-to-camera generator were absent:

```text
$ python3 -m unittest Tests.Content.test_first_ascension_manifest
..FF
Ran 4 tests
FAILED (failures=2)
ROUND2_RED_EXIT=1
```

After adding the exact source/schema and generator, the focused static contract was GREEN:

```text
$ python3 -m unittest Tests.Content.test_first_ascension_manifest
....
Ran 4 tests
OK
ROUND2_STATIC_GREEN_EXIT=0
```

Self-review then identified that action/audio/VFX payloads were fingerprinted but not yet projected into the generated sequences. A second test-first subcycle added exact timed-marker and dedicated-script requirements before implementation:

```text
$ python3 -m unittest Tests.Content.test_first_ascension_manifest
..F.
Ran 4 tests
FAILED (failures=1)
ROUND2_PAYLOAD_RED_EXIT=1
```

After payload marker conversion and the generation script, focused GREEN returned 4/4. Final available suites and integrity checks were:

```text
$ python3 -m unittest discover -s Tests/Content -p 'test_*.py'
..........................................................
Ran 58 tests in 0.021s
OK

$ python3 -m unittest discover -s Tests/UI -p 'test_*.py'
..................
Ran 18 tests in 0.006s
OK

JSON_PARSE_OK=4
ROUND2_AVAILABLE_EXIT content=0 ui=0 json=0 shell=0
```

### Honest UE boundary and self-review

```text
$ UnrealEditor-Cmd "$(pwd)/DominionAscendant.uproject" ...
/bin/bash: line 2: UnrealEditor-Cmd: command not found
ROUND2_UE_EXIT=127

$ Build/Scripts/GenerateFirstAscensionCinematic.sh "$(pwd)/DominionAscendant.uproject"
Build/Scripts/GenerateFirstAscensionCinematic.sh: line 7: UnrealEditor-Cmd: command not found
ROUND2_CINEMATIC_GENERATION_EXIT=127
```

- Re-reviewed source-to-package ownership, exact beat order and total duration, package uniqueness, camera binding and section ranges, payload timing/order, stale cache detection, cinematic-only isolation, full pre-cook integration, and fail-before-package behavior. Replaced explicit channel-view types with API-deduced views to reduce UE 5.8 const-overload fragility and validated the 5.8 MovieScene APIs used against Epic's API reference.
- `git diff --check`, JSON parsing, `bash -n` for both scripts, all available suites, and the no-`.uasset` scan passed. This source-controlled checkout now contains the complete cinematic authoring input, schema, conversion/validation code, and runner script—but still no Unreal-generated binary package. Because UE 5.8 is absent, this round does not claim UHT/UBT compilation, Automation GREEN, commandlet execution, Asset Registry runtime validation, cook GREEN, or a usable generated `.uasset`; those remain the UE-runner handoff.
