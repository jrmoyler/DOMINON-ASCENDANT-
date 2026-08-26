# Task 21 report — production UI integration, input graph, and accessibility

## Round 4 delivered

- Added `UDAUICommandEndpointSubsystem`, a production `UGameInstanceSubsystem` implementing `IDAGameplayUICommandEndpoint`, and install it by default for every `UDAUISubsystem`. The endpoint owns reflected compound payloads and command-specific validation for building card/city/grid/footprint/rotation, citizen/job/facility, squad/order/destination/target, typed treaty terms, crafting quantity, deck instances/rules, conquest routes, saves, and the remaining frozen targets. It routes through the canonical city-grid, collection/content/deck, campaign CAS, command, GAS, clock, diplomacy, world travel, accessibility persistence, and save services. A missing endpoint is lazily restored from the GameInstance rather than being the default runtime result.
- Moved all Back and eight overlay toggles into the manifest action graph. Runtime creates one context per active screen, validates the complete effective keyboard/controller graph for collisions, removes the prior context before adding the next, uses `Started` for Boolean/discrete actions and `Triggered` only for Axis1D actions, and applies known-ID/device-correct remaps with same-active-screen conflict rejection. The old raw Back/overlay key bindings and CommonUI Back handler are disabled.
- Expanded the projection contract so all 27 surfaces have screen-specific typed state. It exposes exact collection/card acquisition fields, deck rule results, crafting requirements, world-asset and facility economy contribution traces, citizen/job/facility triples, Synara and diplomacy reasons, quest/objective bindings, action/promise history, world-map authority, research eligibility, conquest route progress/rewards/reasons, leader outcomes, ascension proof/rewards, and save-slot validation. Campaign commits, GAS attribute changes, camera mode changes, and accessibility policy changes invalidate projections; runtime refresh reads the command, camera, GAS, content, and economy services.
- Replaced overlay counts with distinct typed utility nodes, employment assignments/openings/reasons, housing assignments/reasons, citizen happiness/reason ledgers, Dependency values/reasons, and explicit adjacency edges/grid-distance reasons. Widgets render the record-level values.
- Added one focusable control per frozen accessibility option plus typed remap and hold/toggle setters. Apply payloads contain the complete draft and the production endpoint validates and persists it. Strong consumers apply subtitle/speaker/background state, marker/color/high-contrast policy, reduced motion/flash, fractional shake readback, FOV/motion blur, per-screen remaps, hold/toggle, Tactical Pause, aim assist, build snap, tutorial recall, and tooltip mode. Text scale is applied only at the Slate boundary, eliminating the former widget-plus-Slate double scale.
- Startup now prefers an exact generated cache but creates native source widgets/actions/per-screen contexts from the packaged manifest when the cache is absent or stale. `Build/Scripts/PreCookUI.sh`, `Build/Graph/VerticalSliceUI.xml`, and `Content/UI/README.md` document optional commandlet/BuildGraph generation and `-ValidateOnly`; cache generation is not a cook/startup prerequisite.

## Round 4 TDD and verification evidence

The pre-existing static suite first ran GREEN at 9 tests. Round 4 then added the active-surface Back/overlay collision contract and cache-independent packaging/pre-cook contract before production edits. The intended RED was observed as two failures: the manifest had no `ui.back`/`ui.overlay.*` graph actions, and the native fallback/pre-cook contract was undocumented and had no script.

Final available GREEN:

```text
$ python3 Tests/UI/test_vertical_slice_ui_manifest.py -v
Ran 10 tests in 0.003s
OK

$ git diff --check
exit_code=0
```

The C++ Automation sources additionally construct a real fixture-owned GameInstance and LocalPlayer, resolve the default endpoint, instantiate real CommonUI stacks/widgets/buttons, cover all 27 projections and eight distinct overlays, parse compound payloads, exercise action dispatch, verify accessibility controls/side-effect adapters, validate cache provenance/staleness, and assert exact keyboard/controller mappings and navigation. These sources were written or tightened before their corresponding implementation, but cannot be called GREEN in this environment.

## UE runner caveat

Final Automation and editor-commandlet attempts were both blocked exactly as follows:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject -run=Automation -Test=Dominion.UI -unattended -nop4 -NullRHI
/bin/bash: UnrealEditor-Cmd: command not found
exit_code=127

$ UnrealEditor-Cmd DominionAscendant.uproject -run=DAUIAsset -ValidateOnly -unattended -nop4
/bin/bash: UnrealEditor-Cmd: command not found
exit_code=127
```

This is not a UHT, UBT, commandlet, controller-playtest, or Automation pass. A UE5.8 runner must execute the commandlet generation/`-ValidateOnly`, compile the editor target, and run `Dominion.UI` before those results or generated binary caches can be claimed.

## Round 5 final authority closure

- Removed all endpoint-owned selection/research/conquest state and every endpoint-local success-only branch. Card/building/quest/history/metrics selections and card inspection rotation now mutate the snapshot-backed view-model authority; the endpoint rejects commands for which it has no real authority. Research and Tasks 23-25 use `UDAUIAuthoritativeFeatureRegistrySubsystem`: commands dispatch to the registered service, projections capture the same typed service snapshot, and missing registration produces `ServiceUnavailable` with `bAuthoritative=false`.
- Moved player construction into `UDAWorldStateSubsystem::TryPlacePlayerWorldAsset`. One CAS transaction resolves the owned card and content definition, reconstructs existing authored footprints/rotations on the claimed player-capital grid, validates the exact requested definition footprint, constructs the stable WorldAsset/card deployment records, spends the canonical wallet, and commits. No fresh endpoint grid or 1x1 fallback remains.
- Made compound payload parsing presence-strict for grid coordinates/footprint/rotation, crafting quantity, every destination coordinate, accessibility scalars/maps, and every treaty-term field. Treaty values are verified as JSON objects before dereference. Per-binding payload arrays now use the generated binding index for hand cards, abilities, squads, routes, and directional rotation; input dispatch passes index/device/axis once through the JSON builder.
- Enforced descriptor BackTarget exactly. `__exit__` clears/deactivates its layer without revealing older routes, while named BackTargets clear history and replace it deterministically. Founder/city/command mode routes use the same replacement path.
- Added registered founder/command live-source interfaces for abilities/interactions and zones/enemies/sovereignty/founder status/tactical alerts, retained GAS/command/camera authority for live combat/order fields, updated stable selections on successful selection, and replaced campaign-derived leader/ascension projections with typed late-service records. Graybox rows now render all overlay records rather than only the descriptor's first channel slot.
- Added explicit subtitle, flash, camera, input, gameplay, and tutorial downstream runtime interfaces and a fail-closed bridge. Apply now mutates subtitles/speaker/background, reduced flash, fractional shake/FOV/motion blur, remaps/hold modes, Tactical Pause/aim/snap, and tutorial/tooltips before persistence. The three formerly readback-only option controls now mutate draft maps/modes.
- Job reassignment now rejects same-assignment no-ops, releases the prior opening, decrements the chosen opening, binds the citizen/assignment, increments one revision, and CAS-commits atomically. World travel now requires a registered live `IDARegionTravelRuntimeService` and calls the existing staged snapshot/unload/clock/load/reconstruct authority; the always-true UI runtime was removed.

## Round 5 TDD and verification evidence

`Tests/UI/test_task21_authority_contract.py` was added before production edits. The intended RED was observed as six failures and one missing-file error. After implementation:

```text
$ python3 Tests/Content/test_first_hour_quest_manifest.py
Ran 26 tests in 0.006s
OK

$ python3 Tests/Content/test_vertical_slice_manifest.py
Ran 9 tests in 0.001s
OK

$ python3 Tests/UI/test_vertical_slice_ui_manifest.py
Ran 10 tests in 0.003s
OK

$ python3 Tests/UI/test_task21_authority_contract.py
Ran 7 tests in 0.001s
OK

$ git diff --check
exit_code=0
```

C++ Automation coverage was extended for strict missing quantity/destination/malformed treaty values, indexed player input, exact city/founder BackTargets, missing late/travel services, canonical placement, atomic job capacity, multirow overlay rendering, and all downstream accessibility targets. The environment still has no UE runner, so these sources are not claimed GREEN:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject -run=Automation -Test=Dominion.UI -unattended -nop4 -NullRHI
/bin/bash: line 1: UnrealEditor-Cmd: command not found
exit_code=127
```
