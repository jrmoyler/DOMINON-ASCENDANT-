# Task 27 report — performance, simulation LOD, and 1,000-cycle soak

## Status

Implementation and all executable non-Unreal verification are complete from
base `8a38bebe52c6d3b15d74aa8a85f04459fca5da5f`. UE5.8 is absent, so UHT/UBT,
C++ Automation, the real 1,000-cycle owner soak, benchmark-map materialization,
frame/hitch/transition capture, cook, and reference-hardware results are not
claimed. Checked evidence is deliberately `NOT_READY`, contains null
measurements, and has no passing claims.

## Implementation

### Four simulation LODs and stable named citizens

- Added `UDASimulationLODSubsystem` with the frozen mapping: LOD0 Full for the
  Founder district/combat, LOD1 Detailed for nearby loaded districts, LOD2
  Aggregated for the distant active city, and LOD3 Strategic for unloaded major
  regions.
- Added explicit citizen representations for individuals, important named
  individuals, cohorts, persistent records, cohort aggregates, and district
  aggregates. A named citizen always retains the same `CitizenId`; only its
  transient representation changes.
- Rebuild after save/load consumes the existing canonical
  `FDACampaignLiveSignalState` rather than persisting visual state or creating a
  second citizen authority. Restore validates the complete signal set and
  replaces the transient map atomically.
- Extracted the dependency-free `DASimulationLODPolicy.h`. The Unreal subsystem
  and executable native harness call the same production resolver; the portable
  harness is not a campaign simulator.

### Canonical 1,000 Development Cycle owner soak

- Added `Dominion.Performance.SimulationSoak`, which creates the real
  `UGameInstance` subsystem graph, seeds the existing `UDAWorldStateSubsystem`,
  and advances exactly 200 canonical World Ticks through
  `AdvanceWorldTicks`/`UDASimulationClockSubsystem` = exactly 1,000 Development
  Cycles. It does not call Forgeweave, trade, economy, or event systems as
  shadow simulators.
- The owner test enables the canonical player live economy/population,
  Forgeweave strategy, trade processing, the regional coordinator, and exactly
  the six authored event definitions. It asserts exact clock/economy/AI/trade
  progress and full final campaign validation.
- The soak rejects non-finite money, negative population, duplicate WorldAsset
  IDs, invalid utility/campaign authority, more event states than the six
  enabled definitions, structurally stuck active quests, final saves above
  8 MiB, and save growth above 2 MiB. A second complete run from the same seed
  must serialize byte-for-byte identically.
- The save bounds are production acceptance budgets, repeated in
  `DefaultScalability.ini` and the benchmark budget. No gameplay behavior was
  removed to meet them.

### Benchmark source, generator, runner, and evidence

- Added a strict scene source and schema for exactly 50 gameplay assets, 120
  citizens across all four LODs (30/30/40/20), active weather, three allied
  squads, three enemy squads, one vehicle, the declared Task 26 Synara
  construction Niagara artifact path, one damaged building, and City → Command
  → Founder.
- The deterministic generator expands the source into 50 unique stable GUID
  placements and 120 stable CitizenID rows. Sixty LOD0/1 citizens require
  visible actors; LOD2/3 retain aggregate representations. It emits a plan, not
  a fabricated `.uasset`.
- Added C++ benchmark workload validation plus runtime sample instrumentation
  for frame seconds, Development Cycle boundary seconds, and the exact three
  transition durations. A pass requires at least 600 frame samples, 60 average
  FPS, 30 worst-frame FPS, no Development Cycle sample above the explicitly
  defined 100 ms severe-hitch threshold, all three transitions below 1.5 s,
  and declared reference-class hardware.
- Added a closed evidence schema and fail-closed runner. Absent UE returns 127
  and `NOT_READY`; successful Automation without a complete measured payload
  also remains `NOT_READY`. Uncaptured evidence cannot contain measurements or
  any true claim. Task 28 can consume
  `Build/Performance/Task27PerformanceEvidence.json` directly.
- No optimization was attempted because there is no measured bottleneck. The
  scalability file defines acceptance only and applies no tuning.

## Strict TDD evidence

The native policy test was written first and failed on the missing production
header:

```text
$ g++ -std=c++17 -ISource/DominionSimulation/Public \
    Tests/Performance/SimulationLODPolicyTest.cpp \
    -o /tmp/da_task27_lod_policy_test
Tests/Performance/SimulationLODPolicyTest.cpp:1:10: fatal error:
LOD/DASimulationLODPolicy.h: No such file or directory
compilation terminated.
RED_NATIVE_EXIT=1
```

The four executable benchmark/generator/evidence tests were also written first
and failed at the intended missing production boundary:

```text
$ python3 -m unittest Tests.Performance.test_performance_benchmark -v
Ran 4 tests in 0.012s
FAILED (failures=1, errors=3)

FileNotFoundError: Build/Tools/PerformanceBenchmarkTool.py
RED_PORTABLE_EXIT=1
```

The UE owner/functional specs were authored before the subsystem
implementation. Their RED launch could not reach compilation or discovery:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject -unattended -nop4 -nullrhi \
  '-ExecCmds=Automation RunTests Dominion.Performance;Quit' \
  '-TestExit=Automation Test Queue Empty'
/bin/bash: line 1: UnrealEditor-Cmd: command not found
UE_RED_EXIT=127
```

## GREEN and final available verification

Focused native/shared-production GREEN:

```text
$ g++ -std=c++17 -Wall -Wextra -pedantic \
  -ISource/DominionSimulation/Public \
  Tests/Performance/SimulationLODPolicyTest.cpp \
  -o /tmp/da_task27_lod_policy_test && /tmp/da_task27_lod_policy_test
FOCUSED_NATIVE_EXIT=0
```

Focused benchmark GREEN:

```text
$ python3 -m unittest discover -s Tests/Performance -p 'test_*.py' -v
test_checked_in_evidence_cannot_claim_measurements_which_were_not_run ... ok
test_cli_validates_and_generates_the_same_plan ... ok
test_generator_is_deterministic_and_marks_uncaptured_evidence_not_ready ... ok
test_source_freezes_the_exact_vertical_slice_load ... ok
Ran 4 tests in 0.049s
OK
FOCUSED_PORTABLE_EXIT=0
```

Existing full available suites:

```text
$ python3 -m unittest discover -s Tests/Content -p 'test_*.py'
.....................................................................
Ran 69 tests in 0.879s
OK

$ python3 -m unittest discover -s Tests/UI -p 'test_*.py'
..................
Ran 18 tests in 0.004s
OK
AVAILABLE_FULL_EXIT=0
```

Additional checks all exited 0: Python compilation, scene/budget/evidence JSON
parsing, the executable source validator, and `git diff --check`. Locked scope
remains exactly 64 definitions, 60 starter instances, six events, ten regional
quests, and 27 screens. `find . -type f -name '*.uasset' -print` returned no
paths.

The non-UE runner was exercised end to end. It generated a deterministic plan
and returned exactly this honest state:

```text
state=NOT_READY
measurements=null
performanceTargetsPassed=false
soakPassed=false
cookPassed=false
automation.exitCode=127
cook.exitCode=127
RUNNER_EXIT=127
```

## Fresh unavailable checks

Both required UE commands were attempted independently and returned 127:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject -unattended -nop4 -nullrhi \
  '-ExecCmds=Automation RunTests Dominion.Performance;Quit' \
  '-TestExit=Automation Test Queue Empty'
/bin/bash: line 1: UnrealEditor-Cmd: command not found
FINAL_AUTOMATION_EXIT=127

$ UnrealEditor-Cmd DominionAscendant.uproject -run=cook \
  -targetplatform=Windows -unattended -nop4
/bin/bash: line 1: UnrealEditor-Cmd: command not found
FINAL_COOK_EXIT=127
```

## Files

```text
.superpowers/sdd/2026-08-22-dominion-ascendant-vertical-slice/task-27-report.md
Build/Performance/PerformanceEvidence.schema.json
Build/Performance/README.md
Build/Performance/Task27PerformanceEvidence.json
Build/Performance/VerticalSliceBenchmark.budget.json
Build/Performance/VerticalSliceBenchmark.budget.schema.json
Build/Performance/VerticalSliceBenchmark.scene.json
Build/Performance/VerticalSliceBenchmark.scene.schema.json
Build/Tools/PerformanceBenchmarkTool.py
Config/DefaultScalability.ini
Source/DominionSimulation/DominionSimulation.Build.cs
Source/DominionSimulation/Private/LOD/DASimulationLODSubsystem.cpp
Source/DominionSimulation/Public/LOD/DASimulationLODPolicy.h
Source/DominionSimulation/Public/LOD/DASimulationLODSubsystem.h
Source/DominionTests/Private/Performance/BenchmarkFunctionalTest.cpp
Source/DominionTests/Private/Performance/SimulationSoakSpec.cpp
Tests/Performance/SimulationLODPolicyTest.cpp
Tests/Performance/test_performance_benchmark.py
```

## Self-review and remaining concern

- Reviewed module direction: runtime LOD/instrumentation remains in
  `DominionSimulation`, depends only on Core/CoreUObject/Engine and the existing
  `DominionCore`, and does not point back to Gameplay or World. The canonical
  soak lives only in `DominionTests`, which already depends on all owner
  modules.
- Reviewed for duplicate campaign authority, LOD enum drift, CitizenID
  rewriting, partial restore mutation, fake simulators, wrong cycle/tick ratio,
  missing event count, unbounded save/event growth, fabricated evidence,
  fabricated `.uasset`, unmeasured tuning, and claims inferred from synthetic
  samples. Synthetic evaluator samples are explicitly non-reference and cannot
  set the pass flag.
- The remaining concern is the unavailable UE boundary. Nothing here proves
  that UHT accepts the new reflected types, that UBT compiles the new C++ files,
  that the 1,000-cycle owner soak passes, that the declared generated Niagara
  package exists, that a scene/map has been materialized, or that any FPS,
  hitch, transition, save-size, repeatability, cook, or reference-hardware
  target passes. Task 28 must keep evidence `NOT_READY` until those exact UE5.8
  runs produce complete captured data.

## Fix round 1 — Important findings

This section supersedes the earlier descriptions of the runner, benchmark
materialization, and bulk-World-Tick soak. The checked evidence remains
honestly `NOT_READY`; this round adds the executable production path that can
create a measured candidate when UE5.8 is installed, but does not claim that it
ran here.

### Fail-closed evidence and runner

- The Python reader now rejects JSON `NaN`/`Infinity`, booleans masquerading as
  numbers, negative counters/timers/save sizes, non-finite metrics, non-lowercase
  or non-64-character SHA-256 values, unknown fields, and malformed hardware or
  command objects.
- `CAPTURED` reconciliation hashes the exact generated plan bytes, requires the
  exact reference hardware object for a performance pass, requires successful
  presentation preparation, Automation, and cook commands, and derives all
  three claims from validated measurements and observed exits. Supplied claims
  cannot disagree with the derivation.
- UE writes an internal closed `CANDIDATE` sidecar with no commands and no true
  claims. The runner fully validates that sidecar, reconciles commands, derives
  claims, validates the completed payload again, and only then replaces the
  public evidence with `CAPTURED`. Every process, hash, candidate, or validation
  failure resets the public payload to clean `NOT_READY` with null hardware and
  measurements and all claims false.
- The evidence schema closes hardware, measurements, commands, and claims and
  constrains `CAPTURED`, `CANDIDATE`, and uncaptured states separately. The
  budget schema also freezes the exact Ryzen 5 5600 / RTX 3060 or RX 6700 XT /
  16 GiB / SSD reference class.

Regression coverage mutates a valid capture to contain infinite FPS, infinite
or negative transitions, a negative hitch, negative initial and final save
sizes, a fake digest, fake hardware, a mismatched plan hash, failed preparation,
Automation, or cook, and each inconsistent passing claim. Every mutation is
rejected. A fake process is used only in a temporary runner-handshake test; its
numbers are never written to repository evidence and never exercise the
production benchmark as a substitute.

### Executable measured benchmark path

- Added `Dominion.Performance.RuntimeBenchmark.MeasuredVerticalSlice`. With the
  runner-owned plan, budget, and candidate paths it creates a transient UE
  world and materializes exactly 50 real `ADAWorldAsset` gameplay actors, each
  with a generated authored presentation mesh; exactly 120 stable named
  citizens at LOD counts 30/30/40/20, including 60 visible LOD0/1 actors and
  LOD2/3 aggregate records; active exponential-height weather; three allied and
  three enemy real squad entities and representations; one real vehicle entity
  and representation; the authored generated Synara construction Niagara
  system; and one damaged building.
- The runtime applies 1920x1080 Windowed and every High scalability group, then
  drives the production camera controller through City → Command → Founder.
  A latent Automation command records at least 600 real `FPlatformTime` frame
  deltas and all three production transition durations. The same production
  instrumentation records every canonical Development Cycle boundary duration.
- After capture, the runtime invokes the shared canonical owner soak twice,
  checks byte-identical deterministic save output, populates the production
  `FDABenchmarkEvidenceCapture`, and calls its C++ candidate writer. The Python
  runner first invokes `DAPresentationContent`, removes the former `-nullrhi`
  capture path, passes all required file arguments, then cooks. Installing UE is
  therefore sufficient to attempt a real `CAPTURED` result without synthetic
  constants or a fabricated map/package. Storage defaults to `UNVERIFIED`; that
  still permits a capture but correctly prevents the reference-hardware pass
  unless the operator supplies `--storage-class SSD` after physical
  verification.
- No optimization was introduced because no measured capture exists.

### Strengthened canonical soak

- `RunCanonicalSoak` now calls the real simulation clock once for each of
  exactly 1,000 Development Cycles instead of asking the world owner for one
  bulk 200-World-Tick advance. Player economy runs all 1,000 boundaries and the
  real trade and Forgeweave owners still run exactly 200 World Ticks.
- The seed adds two stable campaign WorldAssets and a real connected
  `FDAUtilityNetwork` power topology. Both source and demand resolve as fully
  supplied, and the corresponding canonical utility signals persist through
  the world owner and final save.
- The new dependency-free production `DA::Soak::ProgressGuard` observes every
  cycle. It counts unique event emissions and state transitions, rejects
  duplicate or more-than-six event rows, and rejects transitions beyond each
  event's authored stage bound. It also tracks each required active quest's
  current node and fails a valid-but-unchanged node after the explicit 25-cycle
  bound.
- Active authored choices are resolved deterministically through the real
  regional coordinator. The special Foundry Shortage choice uses its required
  real `UDAWorldStateSubsystem::ResolveFoundryShortage` owner with a
  deterministic action ID. Population is checked at every boundary. Final
  save bytes, the 8 MiB absolute bound, 2 MiB growth bound, and SHA-256
  repeatability are all asserted from real save files.

### Strict TDD evidence for the fix

The new evidence tests were written before the validator API and runner
reconciliation. The first focused run failed at the intended missing boundary:

```text
$ python3 -m unittest Tests.Performance.test_performance_benchmark -v
TypeError: load_and_validate_evidence() takes 2 positional arguments but 3 were given
```

The candidate-handshake regression then failed before `CANDIDATE` support and
full runner reconciliation:

```text
AssertionError: 3 != 0
```

A final command-line resolution regression was also observed RED before the
runner pinned the real process launch settings:

```text
AssertionError: '-ResX=1920' not found in automation_argv
```

The per-cycle guard executable was written before its production header:

```text
$ g++ -std=c++17 -Wall -Wextra -pedantic \
    -ISource/DominionSimulation/Public \
    Tests/Performance/SoakProgressGuardTest.cpp \
    -o /tmp/da_soak_progress_guard_test
Tests/Performance/SoakProgressGuardTest.cpp:1:10: fatal error:
Performance/DASoakProgressGuard.h: No such file or directory
compilation terminated.
```

The runtime owner/writer Automation test was also authored before production
wiring. Its RED boundary remains the unavailable engine rather than a claimed
compile result:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject -unattended -nop4 \
  '-ExecCmds=Automation RunTests Dominion.Performance;Quit' \
  '-TestExit=Automation Test Queue Empty' \
  -DABenchmarkPlan=Intermediate/Performance/VerticalSliceBenchmark.plan.json \
  -DABenchmarkBudget=Build/Performance/VerticalSliceBenchmark.budget.json \
  -DABenchmarkEvidenceCandidate=Build/Performance/Task27PerformanceEvidence.json.candidate
/bin/bash: line 1: UnrealEditor-Cmd: command not found
UE_RED_EXIT=127
```

Minimal production changes then produced these fresh local GREEN results:

```text
$ python3 -m unittest Tests.Performance.test_performance_benchmark -v
Ran 6 tests in 0.223s
OK

$ g++ -std=c++17 -Wall -Wextra -pedantic \
    -ISource/DominionSimulation/Public \
    Tests/Performance/SimulationLODPolicyTest.cpp \
    -o /tmp/da_lod_policy_test && /tmp/da_lod_policy_test
exit 0

$ g++ -std=c++17 -Wall -Wextra -pedantic \
    -ISource/DominionSimulation/Public \
    Tests/Performance/SoakProgressGuardTest.cpp \
    -o /tmp/da_soak_progress_guard_test && /tmp/da_soak_progress_guard_test
exit 0

$ g++ -std=c++17 -Wall -Wextra -pedantic \
    -ISource/DominionEditor/Private \
    Tests/Content/Native/PresentationNiagaraEmitterValidationTest.cpp \
    -o /tmp/da_presentation_niagara_test && /tmp/da_presentation_niagara_test
exit 0
```

Fresh full available Python suites:

```text
$ python3 -m unittest discover -s Tests/Content -p 'test_*.py' -v
Ran 69 tests in 0.878s
OK

$ python3 -m unittest discover -s Tests/UI -p 'test_*.py' -v
Ran 18 tests in 0.006s
OK
```

Python compilation, JSON parsing, tool validation, checked evidence validation
against the exact generated plan SHA-256, and `git diff --check` all exited 0.
The generated plan was inspected as exactly 50 assets with 50 authored
presentation paths, 120 citizens at 30/30/40/20, 3+3 squads, one vehicle, one
damaged asset, one construction effect, 1920x1080 High, City → Command →
Founder, 1,000 cycles, and six events. Content and UI suites retain the locked
64 definitions, 60 starter instances, six events, ten regional quests, and 27
screens. No `.uasset` is checked in.

### Fresh unavailable checks and honest evidence

The complete non-UE runner generated the plan and reset checked evidence to:

```text
state=NOT_READY
prepare.exitCode=127
automation.exitCode=127
cook.exitCode=127
measurements=null
performanceTargetsPassed=false
soakPassed=false
cookPassed=false
RUNNER_EXIT=127
```

Preparation, Automation without `-nullrhi`, and cook were also attempted
directly. Each printed `UnrealEditor-Cmd: command not found` and exited 127.
Consequently this round does not claim a generated map or presentation package,
UHT/UBT compilation, Automation, the real owner soak result, cook, FPS, hitch,
transition, save-size, repeatability, or reference-hardware result. Task 28 must
continue to consume the checked `NOT_READY` state until the runner succeeds on
UE5.8.

### Files added or changed in fix round 1

```text
.superpowers/sdd/2026-08-22-dominion-ascendant-vertical-slice/task-27-report.md
Build/Performance/PerformanceEvidence.schema.json
Build/Performance/README.md
Build/Performance/Task27PerformanceEvidence.json
Build/Performance/VerticalSliceBenchmark.budget.schema.json
Build/Performance/VerticalSliceBenchmark.scene.json
Build/Performance/VerticalSliceBenchmark.scene.schema.json
Build/Tools/PerformanceBenchmarkTool.py
Source/DominionSimulation/DominionSimulation.Build.cs
Source/DominionSimulation/Private/LOD/DASimulationLODSubsystem.cpp
Source/DominionSimulation/Public/LOD/DASimulationLODSubsystem.h
Source/DominionSimulation/Public/Performance/DASoakProgressGuard.h
Source/DominionTests/DominionTests.Build.cs
Source/DominionTests/Private/Performance/DACanonicalSoakHarness.h
Source/DominionTests/Private/Performance/RuntimeBenchmarkSpec.cpp
Source/DominionTests/Private/Performance/SimulationSoakSpec.cpp
Tests/Performance/SoakProgressGuardTest.cpp
Tests/Performance/test_performance_benchmark.py
```

### Fix-round self-review and remaining concern

- Reviewed state handling for every command and candidate failure, claim
  derivation, exact plan hashing, strict numerical and digest validation,
  unknown fields, exact hardware matching, generated content preparation,
  renderer availability, runtime workload counts, real instrumentation calls,
  specialized Foundry authority, utility ownership, per-cycle population,
  event transition bounds, quest staleness, deterministic save bytes, module
  direction, fixed content counts, and absence of fabricated `.uasset` files.
- The only remaining blocker is UE5.8 absence. UHT/UBT, the new C++ Automation
  specs, transient-world capture, production candidate writer, canonical soak,
  cook, and real hardware targets are all `NOT_RUN`. No bottleneck was measured,
  so no optimization was attempted.

## Fix round 2 — public evidence identity reconciliation

### Finding and fix

Ordinary `load_and_validate_evidence` calls previously accepted a `CAPTURED`
payload without an authoritative plan source. Consequently `planSha256: null`
or any syntactically valid 64-character digest could avoid exact byte
reconciliation when the caller omitted `plan_path`. The same public path also
did not enforce the canonical scene identity in code.

The validator now unconditionally requires
`sceneId == benchmark.vertical_slice.synara_capital`. Every `CAPTURED` payload,
and the runner's internal `CANDIDATE`, must contain a syntactically valid
lowercase SHA-256 and must be validated with an authoritative plan path. The
validator hashes those exact plan bytes and requires equality. It fails closed
when the source is omitted, even when the supplied digest happens to have the
right shape. Clean `NOT_READY`/`NOT_RUN` evidence remains valid without a plan,
so the checked no-UE handoff and Task 28 behavior are unchanged.

### Strict TDD evidence

Focused public-API regressions were added first. The initial run demonstrated
all three escaped checks:

```text
$ python3 -m unittest \
  Tests.Performance.test_performance_benchmark.PerformanceBenchmarkContractTest.test_public_captured_validation_rejects_unreconciled_plan_digests \
  Tests.Performance.test_performance_benchmark.PerformanceBenchmarkContractTest.test_public_evidence_validation_rejects_noncanonical_scene_id -v
AssertionError: ValueError not raised  # null digest without plan
AssertionError: ValueError not raised  # wrong digest without plan
AssertionError: ValueError not raised  # wrong sceneId with authoritative plan
Ran 2 tests in 0.007s
FAILED (failures=3)
RED_EXIT=1
```

An additional RED run proved even a correctly shaped/current digest was
accepted when the authoritative plan source was omitted:

```text
$ python3 -m unittest \
  Tests.Performance.test_performance_benchmark.PerformanceBenchmarkContractTest.test_public_captured_validation_rejects_unreconciled_plan_digests -v
AssertionError: ValueError not raised
Ran 1 test in 0.004s
FAILED (failures=1)
RED_MISSING_PLAN_EXIT=1
```

After the minimal validator change, the two focused regressions passed:

```text
Ran 2 tests in 0.010s
OK
FOCUSED_GREEN_EXIT=0
```

Fresh complete focused Performance verification:

```text
$ python3 -m unittest Tests.Performance.test_performance_benchmark -v
Ran 8 tests in 0.385s
OK
PERFORMANCE_GREEN_EXIT=0
```

Relevant full/static/native checks were also fresh:

```text
$ python3 -m unittest discover -s Tests/Content -p 'test_*.py'
Ran 69 tests in 0.884s
OK

$ python3 -m unittest discover -s Tests/UI -p 'test_*.py'
Ran 18 tests in 0.005s
OK

$ g++ ... Tests/Performance/SimulationLODPolicyTest.cpp && /tmp/da_lod_policy_test
$ g++ ... Tests/Performance/SoakProgressGuardTest.cpp && /tmp/da_soak_progress_guard_test
$ g++ ... Tests/Content/Native/PresentationNiagaraEmitterValidationTest.cpp && /tmp/da_presentation_niagara_test
NATIVE_EXIT=0
```

Python compilation, all benchmark JSON parses, source/budget validation,
`git diff --check`, and the no-checked-`.uasset` check exited 0.

### No-UE state and limitation

The complete runner was re-executed. It returned 127 and preserved the public
evidence as clean `NOT_READY`; ordinary public validation of that uncaptured
payload still succeeds without a plan:

```text
state=NOT_READY
prepare.exitCode=127
automation.exitCode=127
cook.exitCode=127
measurements=null
performanceTargetsPassed=false
soakPassed=false
cookPassed=false
RUNNER_EXIT=127
```

The runtime Automation command was also attempted directly with the exact
1920x1080 arguments and returned:

```text
/bin/bash: line 1: UnrealEditor-Cmd: command not found
AUTOMATION_EXIT=127
```

UE5.8 remains absent. This fix therefore makes no new claim about UHT/UBT,
Automation, the canonical soak, runtime capture, cook, FPS, hitches,
transitions, saves, repeatability, or hardware. The only changed files are the
public validator, its focused regressions, and this appended report section.

### Self-review

- Confirmed the exact digest comparison cannot be bypassed by null, malformed,
  well-shaped-but-wrong, or unverified digests for `CAPTURED`/`CANDIDATE`.
- Confirmed a wrong scene ID fails whether or not a plan is supplied.
- Confirmed runner candidate validation already supplies the generated plan,
  while checked `NOT_READY` evidence remains consumable without one.
- No scene, budget, generated plan/spec, simulation, runtime benchmark, schema,
  or checked evidence content was changed.
