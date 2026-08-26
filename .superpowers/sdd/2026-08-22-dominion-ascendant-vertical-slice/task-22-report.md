# Task 22 report — systemic regional crisis campaign

## Delivered

- Added a strict JSON manifest/schema with the exact six systemic events and ten named regional quests, frozen SHA-1 semantic fingerprint, exact expected package paths, and no checked-in placeholder `.uasset` files. The runtime parser rejects unknown keys, malformed shapes, narrowing/number errors, and fingerprint drift. A strong registry accepts generated assets only at complete field/path/fingerprint parity; otherwise it builds transient manifest-backed definitions. The editor commandlet creates real DataAssets at the manifest paths.
- Promoted `FDACampaignSnapshot` to schema 12 with persisted canonical city-grid claims, real market-price mutation records, ecology reason authority, and `FDARegionalCrisisCampaignState`. Migration rejects injected future fields, creates deterministic regional defaults, seeds canonical Tal/Mara/Ori citizen IDs, and validates before serialization.
- Implemented Foundry Shortage on the vetoable post-simulation World Tick candidate. Resource Hunger strictly above 70 emits one warning, machine-components pricing progresses through +20%, +35%, and +60%, ignored play collapses deterministically, and recovery returns the real market modifier to baseline over the authored 4–8 ticks. The committed clock is captured before publication.
- Implemented four player resolutions plus collapse as copy/validate/commit transactions over real trade capacity, ecology ledger, diplomacy reasons, Resource Hunger, market state, history tags, and exact Tal/Mara/Ori outcomes. Wallets never receive fabricated quest currency. Action GUIDs are idempotent and cannot be reused for a different choice. Save validation reconciles every effect to its exact authored projection; Daxton/Amara conditions consume the durable Mara/Ori outcomes.
- Closed carried authority prerequisites: placement reconstructs only persisted claims and exact authored footprint/timing; HUD authority requires registered live producers and clears stale capture; accessibility preflights all six concrete downstream contracts and rolls every consumer back on failure; history/metrics/founder interactions route through typed handlers or return `ServiceUnavailable`; placement rotation and crafting quantity validate before narrowing with bounded costs; device indices select exact records; treaty requests use fresh mutation IDs; invalid facility/travel fixtures now use real `FDAWorldAssetRecord` authorities and exact errors.

## TDD and verification evidence

The new Task 22 static contract was run before implementation and failed all six tests because the manifest was absent (`STATIC_RED_EXIT=1`). The UE Automation attempt was also recorded at RED/unavailable with exit 127.

Final available GREEN:

```text
Tests/Content/test_first_hour_quest_manifest.py       PASS
Tests/Content/test_regional_crisis_manifest.py       PASS (6 tests)
Tests/Content/test_vertical_slice_manifest.py        PASS
Tests/UI/test_task21_authority_contract.py            PASS (7 tests)
Tests/UI/test_vertical_slice_ui_manifest.py           PASS
STATIC_SUITE_EXIT=0
git diff --check                                     exit_code=0
```

The C++ Automation sources cover strict parser/cache parity, threshold/stage timing, ignored escalation save/reload, four atomic resolutions, idempotence, citizen/dialogue persistence, migration/tamper rejection, snapshot-backed placement reconstruction, bounds, typed service errors, accessibility fanout, and exact device-index records. They are not claimed GREEN in this environment.

## UE runner caveat

```text
$ UnrealEditor-Cmd DominionAscendant.uproject -run=Automation -Test=Dominion.Content.RegionalCrisis -unattended -nop4 -NullRHI
/bin/bash: UnrealEditor-Cmd: command not found
UE_AUTOMATION_EXIT=127

$ UnrealEditor-Cmd DominionAscendant.uproject -run=DARegionalCrisisContent -ValidateOnly -unattended -nop4
/bin/bash: UnrealEditor-Cmd: command not found
UE_COMMANDLET_EXIT=127
```

This is not a UHT, UBT, Automation, or commandlet pass. A UE5.8 editor runner must compile the editor target, execute `DARegionalCrisisContent -ValidateOnly`, generate the real assets when desired, and run `Dominion.Content.RegionalCrisis` plus the affected Core/UI suites.

## Fix round 3 — manifest runtime and accessible input dispatch

### Changes

- Replaced event/quest-definition identity switches with evaluators driven by each row's `TriggerMetric`, `TriggerComparison`, `TriggerThreshold`, and `Trigger` expression. Foundry now takes its initial stage, cumulative stage durations, and price modifiers from the event manifest. Non-Foundry events traverse authored stage edges and ignored resolutions, while event-backed quest choices traverse both the authored quest graph and the matching event resolution.
- Added one atomic non-Foundry resolution projection for authored market, relief-route capacity, ecology reasons, diplomacy reasons, Resource Hunger, history tags, and citizen outcomes. The transaction re-resolves the exact authored resolution row, validates the full snapshot, and commits only through the existing world-state authority.
- Added explicit `Press` alongside `Hold` and `Toggle`; persisted modes now use the canonical campaign-action IDs. Player mapping rebuild maps generated/native runtime input-action names back to stable action IDs, installs a one-shot hold trigger or pressed trigger, and production dispatch consumes toggle state before reaching the command router.
- Replaced the immediately rounded placement calculation with `ResolveBuildGridOrigin`: snap strength now changes the cell threshold, so fractional strength has an observable, deterministic effect on the authoritative integer grid origin.
- Added behavior-level C++ Automation coverage for renamed manifest rows, OR quest predicates, complete regional effect transactions, ignored-event and event-backed-quest lifecycles, stable-ID input mapping, Press/Hold/Toggle consumption, production toggle dispatch, and fractional snap.

Files changed:

```text
.superpowers/sdd/2026-08-22-dominion-ascendant-vertical-slice/task-22-report.md
Source/DominionTests/Private/Content/RegionalCrisisSpec.cpp
Source/DominionTests/Private/UI/ControllerNavigationSpec.cpp
Source/DominionTests/Private/UI/UIScreenCoverageSpec.cpp
Source/DominionUI/Private/Accessibility/DAAccessibilitySettings.cpp
Source/DominionUI/Private/Commands/DAUICommandEndpoint.cpp
Source/DominionUI/Private/Subsystems/DAUISubsystem.cpp
Source/DominionUI/Private/Widgets/DAGrayboxWidgets.cpp
Source/DominionUI/Public/Accessibility/DAAccessibilitySettings.h
Source/DominionUI/Public/Subsystems/DAUISubsystem.h
Source/DominionWorld/Private/Narrative/DAFoundryShortageRuntime.cpp
Source/DominionWorld/Private/Narrative/DARegionalCampaignCoordinatorSubsystem.cpp
Source/DominionWorld/Public/Narrative/DARegionalCampaignCoordinatorSubsystem.h
```

### RED / executable behavioral runner limitation

The focused behavioral C++ tests were authored before their corresponding production changes. The required RED invocation could not launch, so no C++ assertion failure or pass is claimed:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject -run=Automation -Test=Dominion.Content.RegionalCrisis,Dominion.UI.ControllerNavigation,Dominion.UI.ScreenCoverage -unattended -nop4 -NullRHI
/bin/bash: line 1: UnrealEditor-Cmd: command not found
EXIT_CODE=127
```

Production breaks those tests are designed to catch: trigger decisions coupled to event/quest IDs; incomplete non-Foundry effect/lifecycle projection; runtime input-action names that cannot find persisted modes; missing Press semantics; toggle pulses bypassing production consumption; and fractional snap erased by unconditional integer rounding.

### GREEN / available verification

Focused regional static suite (the prior report's six-test count was stale; the current count is eleven):

```text
$ python3 -m unittest Tests.Content.test_regional_crisis_manifest
...........
----------------------------------------------------------------------
Ran 11 tests in 0.009s

OK
EXIT_CODE=0
```

Current relevant static selection:

```text
$ python3 -m unittest Tests.Content.test_first_hour_quest_manifest Tests.Content.test_regional_crisis_manifest Tests.Content.test_vertical_slice_manifest Tests.UI.test_task21_authority_contract Tests.UI.test_vertical_slice_ui_manifest
................................................................
----------------------------------------------------------------------
Ran 64 tests in 0.022s

OK
EXIT_CODE=0
```

Whitespace verification:

```text
$ git diff --check
(no output)
EXIT_CODE=0
```

### UE5.8 limitation

`UnrealEditor-Cmd` is absent from this environment. Therefore this round does not claim UHT, UBT, UE compilation, or C++ Automation GREEN. The exact blocked focused UE command and exit 127 are recorded above; it must be rerun with a UE5.8 editor installation.

### Self-review

- Reviewed the full fix diff for duplicate campaign authority, manifest count changes, fake packages, persistence enum compatibility, transaction boundaries, and C++ compile hazards. `Press` was appended so existing serialized Hold/Toggle numeric values remain stable; invalid/legacy action-key maps recover through the existing validated defaults.
- Corrected a potential `TObjectPtr` Automation compile hazard by casting `Trigger.Get()`, re-resolved the authored resolution instead of trusting a caller-provided value copy, allowed signed nonzero authored market modifiers, and retained overflow/reserved-capacity/snapshot validation before assignment.
- Found no edits to the six-event/25-total-quest manifests, no duplicate state owner, no checked-in `.uasset`, no Windows-scope expansion, and no edit to the SDD ledger. Remaining concern: the unavailable UE5.8 toolchain means the touched C++ still requires UHT/UBT and Automation execution on a configured editor runner.

## Fix round 4 — ignored quest terminal state and direct placement presses

### Changes

- Event predicates now require the linked saved event to be active. The coordinator derives the exact event link and ignored terminal from each quest/event manifest row; when that ignored resolution is reached, it starts the authored quest graph if needed and abandons the active choice through `FDAQuestRuntime::AbandonQuest`. No ignored choice, duplicate event effect, or terminal-event reopen is synthesized.
- Quest choice submission now preflights an active authored Choice node before attempting an event transition, so an abandoned event-backed quest cannot touch an already-resolved event.
- `city.place_building` keeps its canonical stable action ID but defaults to `Press`. Direct actions dispatch every trigger even when loading a legacy persisted `Toggle`; persistent toggle state is restricted to the genuinely stateful canonical `ui.tactical_pause` action. Existing one-shot Hold and Press behavior remains intact.
- Added behavioral Automation coverage for Green Line and Foundry ignored quest abandonment, repeated synchronization/event-effect idempotence, terminal choice rejection, two real authoritative placements from consecutive presses, legacy direct-toggle compatibility, and distinct Press/Hold/Toggle behavior.

Files changed:

```text
Source/DominionTests/Private/Content/RegionalCrisisSpec.cpp
Source/DominionTests/Private/UI/ControllerNavigationSpec.cpp
Source/DominionTests/Private/UI/UIScreenCoverageSpec.cpp
Source/DominionUI/Private/Accessibility/DAAccessibilitySettings.cpp
Source/DominionWorld/Private/Narrative/DAQuestRuntime.cpp
Source/DominionWorld/Private/Narrative/DARegionalCampaignCoordinatorSubsystem.cpp
Source/DominionWorld/Public/Narrative/DAQuestRuntime.h
```

### RED / executable behavioral runner limitation

The focused behavioral tests were written before production changes. The RED runner was unavailable, so no assertion failure is claimed:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject -run=Automation -Test=Dominion.Content.RegionalCrisis,Dominion.UI.ControllerNavigation,Dominion.UI.ScreenCoverage -unattended -nop4 -NullRHI
/bin/bash: line 1: UnrealEditor-Cmd: command not found
EXIT_CODE=127
```

The same post-implementation invocation also exited 127 with the identical output. This is not a GREEN C++ Automation, UHT, UBT, or editor compilation result.

### GREEN / available verification

```text
$ python3 -m unittest Tests.Content.test_first_hour_quest_manifest Tests.Content.test_regional_crisis_manifest Tests.Content.test_vertical_slice_manifest Tests.UI.test_task21_authority_contract Tests.UI.test_vertical_slice_ui_manifest
................................................................
----------------------------------------------------------------------
Ran 64 tests in 0.021s

OK
EXIT_CODE=0
```

```text
$ git diff --check
(no output)
EXIT_CODE=0
```

### Self-review

- `AbandonQuest` remains in the existing plain C++ quest-runtime authority; it copy-validates, revision-checks, and commits the terminal state without adding a reflected/UHT surface. Coordinator pointers into candidates are not reused after copy/move mutations.
- The event link, ignored resolution, quest graph, and stable action IDs are taken from canonical manifest/settings data. Terminal submissions are rejected before event advancement, and repeated synchronization cannot reapply ignored event effects.
- No campaign manifest, fingerprint, event/quest count, `.uasset`, or `progress.md` was changed. The repository still carries exactly six regional events and 25 total quests. The remaining concern is the unavailable UE5.8 runner, so all touched C++ still needs configured UHT/UBT and Automation execution.
