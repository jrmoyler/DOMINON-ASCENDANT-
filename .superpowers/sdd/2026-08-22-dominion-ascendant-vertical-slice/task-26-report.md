# Task 26 report — art, VFX, and audio integration gates

## Status

Source implementation is complete at base `63e98d954fb56306ea43c7f80638bfdaaf9c8721`.
All executable non-Unreal coverage is GREEN. Unreal 5.8 is absent, so UHT/UBT
compilation, Automation execution, generated package creation, Asset Registry
runtime validation, import, and cook are explicitly unverified and are not
claimed.

## Implementation

### Frozen source coverage

- Added `VerticalSlicePresentation.json` as the single v1.1 presentation
  coverage root. Its combined canonical SHA-1,
  `58750762d440011ccf757ac218300e791349f0a3`, covers the root plus all five
  subordinate source documents rather than merely naming them.
- Added strict closed schemas and source recipes for the exact first fifty v0.6
  production assets: 37 building/environment/fusion rows and 13
  character/unit/vehicle rows. Every row has a stable numbered ID, generated
  package path, authored silhouette/material/signature or rig/variant recipe,
  and required construction/damage states where applicable.
- Added exactly 25 core VFX definitions. Each has anticipation, active,
  resolution, gameplay-radius, and faction-signature source data. Added exactly
  9 music cues, 12 ambient loops, and 60 distinct authored SFX events before
  variation layers. The SFX rows are divided across all twelve v1.1 families.
- Added exact source documents beneath `Content/Buildings`,
  `Content/Characters`, `Content/VFX`, and `Content/Audio`; canonical JSON
  retains LF checkout policy.

### Runtime and authoritative-hook binding

- Added `FDAPresentationContentPipeline`, a strict C++ parser shared by runtime,
  Automation, and editor generation. It rejects unknown/missing keys, wrong
  types/identities/counts, duplicate IDs or paths, incomplete numbering,
  unresolved cue references, stale combined fingerprints, shared construction
  grammars, instant recolor, gameplay-gated Ascension, and non-skippable
  cinematic policy.
- Added `UDAPresentationDefinition` and
  `UDAPresentationRegistrySubsystem`. Generated cache is accepted only at
  complete 156-row ID/path/recipe/source-fingerprint parity. Otherwise the
  subsystem strongly owns transient definitions built from packaged canonical
  JSON. Fallback rows retain full canonical recipe payloads but always report
  generated provenance as false.
- Construction consumes the existing
  `UDAConstructionComponent::OnStageChanged` state. Synara uses micro-drone
  frame/self-aligning panels; Forgeweave uses tracked platforms, cranes, molten
  casting, bolts, welding, and pressure testing; Eden uses root lattices,
  branching timber frames, living-shell growth, water/mycelium connection, and
  canopy balance. Their exact five-stage geometry signatures are disjoint.
- `UDAStructuralDamageComponent` now emits
  `OnDamageStateChanged` only after canonical damage mutation. Runtime
  projection selects the v0.6 Synara fractured-ceramic/exposed-lattice,
  Forgeweave soot/bent-steel/heat, or Eden torn-fiber/scorched-living-material
  language.
- `UDACaptureComponent` now emits a read-only committed snapshot after begin,
  progress/reset, completion, and resolved outcome publication. Presentation
  begins from original architecture and orders old-sign removal, integration
  scaffold, new-sign installation, then integrated operation. Authoritative
  ownership may already be committed; presentation never substitutes instant
  faction recolor.
- Daxton projection consumes `FDADaxtonCampaignState` and maps the three real
  encounter phases plus resolution. Phase II binds powered-armor Overdrive.
  Ascension projection consumes the existing
  `FDAAscensionPresentationState`, exact cinematic path, and exact five beats.
  Skip mutates only `FDAPresentationPlaybackPlan::bSkipped`; the authority
  object is passed const and remains unchanged.
- Blueprint-facing registry methods resolve definitions and build construction,
  damage, capture, Daxton, and Ascension plans without moving gameplay state
  into presentation Blueprints.

### Editor generation, validation, and production graph

- Added `DAPresentationContent` commandlet. It parses and fingerprints every
  source before package mutation, preflights every target against foreign
  ownership, idempotently creates or refreshes
  `UDAPresentationDefinition` packages, stores generator/source/recipe/ID
  provenance, and independently reloads every expected package through the
  Asset Registry for exact class/path/content parity.
- Added `PreCookPresentation.sh`, which runs generation and then a separate
  `-ValidateOnly` process. The existing Forgeweave production BuildGraph now
  requires that gate after First Ascension.
- Packaging stages all canonical source recipes as UFS, always cooks the four
  generated namespaces, and scans them as the `DAPresentation` primary asset
  type with `AlwaysCook`.
- No `.uasset` or other fabricated binary package was added.

## Strict TDD evidence

The Python executable contract and C++ Automation spec were authored before any
Task 26 manifest, parser, registry, commandlet, or event hook.

Focused executable RED:

```text
$ python3 -m unittest Tests.Content.test_presentation_manifest
FFFFFF
Ran 6 tests in 0.001s
FAILED (failures=6)
RED_EXIT=1
```

All six failures were the intended missing production boundary:

```text
AssertionError: False is not true : executable presentation manifest parser is missing
```

The intended C++ RED could not reach discovery:

```text
$ UnrealEditor-Cmd "$(pwd)/DominionAscendant.uproject" -unattended -nop4 -NullRHI \
  '-ExecCmds=Automation RunTests Dominion.Content.PresentationCoverage; Quit' \
  '-TestExit=Automation Test Queue Empty'
/bin/bash: line 1: UnrealEditor-Cmd: command not found
UE_RED_EXIT=127
```

After the first minimal parser/source implementation, focused GREEN exposed one
test-fixture ordering issue: duplicate-ID mutation was correctly rejected by
the stronger numbered-ID check before the duplicate diagnostic. The parser was
then minimally adjusted to diagnose duplicates first. Final focused GREEN:

```text
$ python3 -m unittest Tests.Content.test_presentation_manifest
......
Ran 6 tests in 0.204s
OK
FOCUSED_GREEN_EXIT=0
```

The C++ spec covers real parser resolution, rejection of partial generated
cache, runtime fallback provenance, five-stage faction construction behavior,
committed damage/capture record projection, original-owner signage integration,
Daxton Phase II Overdrive, exact Ascension beats/path, and presentation-only
skip. It remains unexecuted because Unreal is unavailable.

## Final available verification

```text
$ python3 -m unittest discover -s Tests/Content -p 'test_*.py'
................................................................
Ran 64 tests in 0.197s
OK

$ python3 -m unittest discover -s Tests/UI -p 'test_*.py'
..................
Ran 18 tests in 0.004s
OK
```

Executable source resolver:

```text
$ python3 Build/Tools/PresentationManifestTool.py validate
{"ambientLoops": 12, "constructionGrammars": 3, "coreVfx": 25,
 "musicCues": 9, "primaryAssets": 50, "sfxEvents": 60,
 "totalGeneratedDefinitions": 156}
```

Generation-plan inspection returned 156 unique IDs and 156 unique package paths:

```text
Counter({'SFX': 60, 'PrimaryAsset': 50, 'CoreVFX': 25,
         'Ambient': 12, 'Music': 9})
```

Script behavior without Unreal was checked through a harmless editor substitute:

```text
$ UNREAL_EDITOR_CMD=/bin/echo Build/Scripts/PreCookPresentation.sh DominionAscendant.uproject
DominionAscendant.uproject -run=DAPresentationContent -unattended -nop4
DominionAscendant.uproject -run=DAPresentationContent -ValidateOnly -unattended -nop4
```

Additional checks all exited 0:

- Python compilation for the validator and tests;
- parse of 12 new JSON manifests/schemas and the production XML;
- `bash -n Build/Scripts/PreCookPresentation.sh`;
- `git diff --check`;
- locked-scope inspection: 3 civilizations, 64 definitions, 60 starter
  instances, 50 primary assets, 25 VFX, 9 music, 12 ambient, 60 SFX;
- binary numstat scan;
- `find . -type f -name '*.uasset' -print`, which returned no paths.

## Unreal handoff boundary

Fresh final commands were blocked before compilation/test discovery or package
generation:

```text
$ UnrealEditor-Cmd ... 'Automation RunTests Dominion.Content.PresentationCoverage; ...'
/bin/bash: line 1: UnrealEditor-Cmd: command not found
FINAL_UE_AUTOMATION_EXIT=127

$ Build/Scripts/PreCookPresentation.sh "$(pwd)/DominionAscendant.uproject"
Build/Scripts/PreCookPresentation.sh: line 7: UnrealEditor-Cmd: command not found
FINAL_PRESENTATION_PRECOOK_EXIT=127
```

An Unreal 5.8 runner must execute UHT/UBT, the named Automation coverage, and
`Build/Scripts/PreCookPresentation.sh`. Until then there is no claim of C++
compilation, rendered art/VFX/audio package usability, generated Asset Registry
coverage, or cook success.

## Files

- Canonical source: `Content/DA/Manifests/VerticalSlicePresentation*.json`,
  `Content/Buildings/Presentation/*`,
  `Content/Characters/Presentation/*`, `Content/VFX/Presentation/*`,
  and `Content/Audio/Presentation/*`.
- Runtime: `Source/DominionWorld/{Public,Private}/Presentation/*` plus the
  committed damage/capture presentation delegates in `DominionGameplay`.
- Editor/production: `DAPresentationContentCommandlet.*`,
  `PreCookPresentation.sh`, `VerticalSliceForgeweaveConquest.xml`,
  `DefaultGame.ini`, and `.gitattributes`.
- Tests: `PresentationCoverageSpec.cpp` and
  `test_presentation_manifest.py`.

## Self-review and concerns

- Reviewed for duplicate/partial cache acceptance, source-only provenance
  confusion, package-path collisions, foreign package overwrite, mutation
  before preflight, non-deterministic recipe identity, cue cross-reference
  gaps, construction grammars differing only by label, capture recolor,
  presentation-owned gameplay state, damage/capture pre-publication broadcasts,
  cinematic skip authority, locked gameplay/UI counts, and fabricated binary
  artifacts.
- Corrected commandlet idempotence to reuse owned objects and added an all-target
  foreign-package preflight before the first save. Retained full canonical
  recipe payloads in runtime/generated definitions rather than discarding
  source behavior after hashing.
- Remaining concern is solely the unavailable UE execution boundary described
  above. The checked-in sources define/generate presentation-definition
  packages; no claim is made that Unreal has compiled them or produced/rendered
  the corresponding binary caches in this environment.

---

## Review fix round 1/5 — 2026-08-26

This section supersedes the original report wherever the review identified a
gap. The canonical presentation fingerprint is now
`e8086c8af38b2f85dff0006d35c062eb21392088`, covering six subordinate source
documents, including the artifact source contract. The checked-in canonical
scope remains 50 primary definitions, 25 VFX IDs, 9 music cues, 12 ambient
loops, and 60 SFX events; 60 is now correctly a minimum rather than a maximum.

### Fixes implemented

1. Added `UDAPresentationEventAdapterComponent`, a real runtime listener. It
   binds with `AddUniqueDynamic` to the existing construction, structural
   damage, and capture multicast delegates. Construction broadcasts feed the
   registry construction-plan builder; damage broadcasts re-read and verify the
   committed world record before feeding the damage-plan builder; capture
   broadcasts feed a committed-snapshot registry builder. The adapter publishes
   only non-authoritative playback plans.
2. Added complete, source-hashed artifact inputs and a strict artifact source
   manifest. The executable plan now contains 288 unique artifact package paths:
   50 `StaticMesh`, 50 `MaterialInstanceConstant`, 25 `NiagaraSystem`, 81
   `SoundWave`, 81 `SoundCue`, and the existing Task 25 `LevelSequence`.
   Definition rows carry runtime-loadable artifact bindings rather than only
   prose recipes.
3. Expanded `DAPresentationContent` generation to import the OBJ mesh source,
   decode/import valid PCM WAV source, generate material instances, Niagara
   systems, and linked sound cues, and validate the externally generated first
   Ascension sequence. Validation independently checks Asset Registry object
   path, exact class, source/recipe provenance, and applicable content (mesh
   LOD, material parent, positive wave duration, linked cue node, system/sequence
   class). The production graph already orders Task 25 before this gate.
4. Removed exact-156 assumptions from the Python parser, C++ validation,
   fallback, and cache coverage. Counts are derived from the resolved arrays;
   a fingerprint-valid 61-SFX fixture produces 157 definitions and is accepted.
   ID/path uniqueness and the minimum of 60 remain fail-closed.
5. Capture plans now preserve top-level original and committed target owners.
   Every integration/signage stage carries both owners and its displayed owner;
   `install_new_signage` explicitly carries the target signage owner. The test
   guard checks array length before stage indexing.
6. Rejected non-empty generated-cache diagnostics are retained on the registry,
   returned to the caller as degraded-cache warnings, and logged as warnings at
   subsystem initialization while canonical fallback remains ready and honest.
7. Split the former 1,017-line mixed implementation into a 958-line strict
   parser/validator, a 133-line definition/cache unit, and a 148-line runtime
   projection unit. The delegate adapter is separate from all three.

No `.uasset` was created or checked in. Artifact paths and Asset Registry
expectations are generation contracts until an Unreal runner produces and
validates the binary packages.

### Review-round strict TDD evidence

The review behavior tests were written first. Focused RED was:

```text
$ python3 -m unittest Tests.Content.test_presentation_manifest -v
test_artifact_plan_covers_real_importable_classes_and_checked_in_sources ... FAIL
test_sixty_one_unique_sfx_are_compliant_and_definition_totals_are_derived ... FAIL
Ran 8 tests in 0.212s
FAILED (failures=2)
```

The failures named the real missing contracts:

```text
invalid choice: 'emit-artifact-plan' (choose from validate, emit-plan)
presentation contract error: generated definition plan does not contain exactly 156 rows
```

C++ Automation RED cases were also authored first for actual component delegate
broadcasts, committed owner/signage payload, degraded-cache diagnostics, and
dynamic fallback/cache totals. As in the original implementation, Unreal was
absent and those cases could not reach compilation or discovery.

Final focused GREEN:

```text
$ python3 -m unittest Tests.Content.test_presentation_manifest -v
Ran 8 tests in 0.351s
OK
```

The artifact-plan execution reported:

```text
{'artifacts': 288, 'paths': 288,
 'classes': {'LevelSequence': 1, 'MaterialInstanceConstant': 50,
             'NiagaraSystem': 25, 'SoundCue': 81, 'SoundWave': 81,
             'StaticMesh': 50}}
```

The canonical definition plan remains 156 unique IDs and paths. The separate
61-SFX fixture verifies 157 definitions and, through the C++ spec, derived
runtime fallback/cache cardinality.

### Final available verification for this round

```text
$ python3 -m unittest discover -s Tests/Content -p 'test_*.py'
..................................................................
Ran 66 tests in 0.363s
OK

$ python3 -m unittest discover -s Tests/UI -p 'test_*.py'
..................
Ran 18 tests in 0.005s
OK
```

Additional successful checks:

- Python byte compilation for the validator and focused test;
- executable validation of OBJ structure, all six material palettes, the five
  Niagara recipe parameters, base64 decoding, and non-empty mono 16-bit WAV;
- SHA-1 drift rejection for every raw artifact source;
- parse of every presentation JSON document and production BuildGraph XML;
- `bash -n Build/Scripts/PreCookPresentation.sh` and harmless `/bin/echo`
  generation/ValidateOnly argument inspection;
- 156 unique definition paths, 288 unique artifact paths, `git diff --check`,
  and a repository scan returning no `.uasset` files.

The optional third-party `jsonschema` Python package was not installed
(`ModuleNotFoundError`). This does not replace or weaken the GREEN executable
validator, which enforces closed keys and types directly; no library-schema
validation claim is made.

Fresh Unreal evidence remains honestly blocked:

```text
$ UnrealEditor-Cmd ... 'Automation RunTests Dominion.Content.PresentationCoverage; ...'
/bin/bash: line 1: UnrealEditor-Cmd: command not found
EXIT_CODE=127

$ Build/Scripts/PreCookPresentation.sh "$(pwd)/DominionAscendant.uproject"
Build/Scripts/PreCookPresentation.sh: line 7: UnrealEditor-Cmd: command not found
EXIT_CODE=127
```

Therefore UHT/UBT compilation, delegate Automation execution, source import,
generated artifact usability, Asset Registry validation, and cook remain the
single handoff boundary. They are not claimed as successful.

### Round self-review

Reviewed all five Important findings and both Minors against the implementation
and tests. Specifically checked that bindings are executable delegate
subscriptions rather than hook-name strings; capture target ownership survives
through signage; partial cache diagnostics are not discarded; totals have no
exact-60/156 production guard; actual artifact paths/classes reach runtime
definitions; external sequence generation is ordered before Task 26; no
presentation code writes gameplay authority; preflight covers artifact targets;
and no source manifest or transient fallback reports generated-package
provenance. The remaining concern is only the unavailable Unreal 5.8 execution
boundary above.

---

## Review fix round 2/5 — 2026-08-26

This section supersedes the round-1 statements about runtime bootstrap and
material/Niagara artifact content. The canonical presentation fingerprint is
now `93bd4af7367c1160a8402882103b73f49f722880`. Frozen coverage remains 50
primary definitions, 25 VFX IDs, 9 music cues, 12 ambient loops, and 60
canonical SFX events, with 60 still enforced as a minimum.

### Fixes implemented

1. Added the shipped `UDAPresentationBindingWorldSubsystem`, a
   `UTickableWorldSubsystem` automatically constructed by Unreal for Game,
   PIE, and GamePreview worlds. It obtains the ready game-instance
   presentation registry, scans map actors at initialization and world
   BeginPlay, handles newly spawned actors, and continues discovering
   dynamically registered or streamed construction, damage, and capture
   components. It creates actor-owned adapters, binds the real authoritative
   multicast delegates, aggregates presentation plans, and never writes
   gameplay state.
2. The subsystem retains only weak source keys, prunes invalid, unregistered,
   destroyed, and cross-world sources, explicitly unbinds delegates, destroys
   adapters, removes its actor-spawn delegate during deinitialization, and
   rediscovers a source after it is registered again. The existing adapter
   binding API is also Blueprint-callable with null-safe object parameters.
   Construction now exposes the same read-only committed-record copy already
   used by structural damage, allowing faction inference without adding
   presentation-owned authority.
3. Replaced the default-material placeholder with definition-specific material
   generation. For each primary definition, the commandlet parses its faction
   entry from `FactionPalettes.json`, creates an owned `UMaterial` graph with
   connected `DA.BaseColor`, `DA.EmissiveColor`, `DA.Metallic`, and
   `DA.Roughness` parameters, and creates a
   `UMaterialInstanceConstant` whose overrides exactly match the authored
   palette. A default material parent, missing graph input, or mismatched parent
   or instance value now fails validation.
4. Replaced empty `UNiagaraSystem` construction with duplication of the
   source-selected, compiled Niagara
   `NS_SimpleSpriteBurst.NS_SimpleSpriteBurst` template. The checked-in source
   requires an enabled `SimpleSpriteBurst` CPU emitter with an enabled
   `NiagaraSpriteRendererProperties` renderer and one-shot behavior. The
   generated system receives exact per-definition gameplay radius, burst count,
   lifetime, and sprite-size user parameters plus authored fixed bounds and
   warmup. Validation independently checks enabled emitter name, renderer class,
   CPU target, one-shot behavior, emitter count, exposed parameter values,
   bounds, and warmup, so an empty system cannot pass.
5. Material and Niagara artifact requirements now retain canonical,
   definition-specific source-content payloads and SHA-1 fingerprints.
   Generation persists both provenance values; Asset Registry validation loads
   the exact class/path and then compares real asset content to the independently
   parsed requirement as well as checking provenance. The executable artifact
   plan exposes the same source content for all 50 materials and 25 Niagara
   systems. No generated package or Asset Registry success is inferred from
   these source rows.

No `.uasset` was authored or checked in.

### Strict TDD evidence

The executable artifact behaviors were written first. Focused RED:

```text
$ python3 -m unittest \
  Tests.Content.test_presentation_manifest.PresentationManifestTests.test_artifact_plan_covers_real_importable_classes_and_checked_in_sources \
  Tests.Content.test_presentation_manifest.PresentationManifestTests.test_artifact_plan_changes_with_palette_source_and_rejects_empty_niagara_content -v
... ERROR
KeyError: 'sourceContent'
... ERROR
KeyError: 'sourceContent'
Ran 2 tests in 0.090s
FAILED (errors=2)
```

Those tests require every material row to carry its definition faction's exact
palette, every Niagara row to carry the exact non-empty template/emitter and
parameter contract, a hash-refreshed palette mutation to change the emitted
plan, and a hash-refreshed empty Niagara source to be rejected.

Focused GREEN after the minimal parser/source implementation:

```text
Ran 2 tests in 0.131s
OK
```

The production lifecycle Automation behavior was also authored first. It builds
a real `UWorld` with a real `UGameInstance`, obtains the automatically
created binding subsystem, initializes construction, damage, and capture
components before BeginPlay, broadcasts all three real authoritative delegates,
adds a damage component after bootstrap, then destroys its actor and verifies
pruning. Unreal is absent, so this behavior test could not reach UHT,
compilation, discovery, or execution; no false GREEN is claimed.

### Final available verification

```text
$ python3 -m unittest discover -s Tests/Content -p 'test_*.py'
...................................................................
Ran 67 tests in 0.502s
OK

$ python3 -m unittest discover -s Tests/UI -p 'test_*.py'
..................
Ran 18 tests in 0.004s
OK
```

The emitted production contract reports:

```text
{'artifacts': 288, 'paths': 288, 'sourceContentRows': 75,
 'classes': {'LevelSequence': 1, 'MaterialInstanceConstant': 50,
             'NiagaraSystem': 25, 'SoundCue': 81, 'SoundWave': 81,
             'StaticMesh': 50}}
```

Additional successful checks: Python byte compilation, all repository JSON
parsing, presentation source/hash validation, shell syntax for the pre-cook
gate, `git diff --check`, and a repository scan reporting `uassets=0`.

Fresh Unreal evidence remains blocked:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject \
  -ExecCmds="Automation RunTests Dominion.Content.PresentationCoverage;Quit" \
  -unattended -nop4 -nullrhi
/bin/bash: line 1: UnrealEditor-Cmd: command not found
exit=127

$ Build/Scripts/PreCookPresentation.sh "$(pwd)/DominionAscendant.uproject"
Build/Scripts/PreCookPresentation.sh: line 7: UnrealEditor-Cmd: command not found
exit=127
```

Therefore UHT/UBT compilation, the new real-world delegate Automation case,
template duplication, material/Niagara package generation, Asset Registry
content validation, and cook still require the UE 5.8 handoff runner. They are
not claimed as successful.

### Round self-review

Reviewed production discovery versus test-only manual binding, late component
registration, actor destruction and streaming cleanup, world teardown,
duplicate delegate binding, registry fallback readiness, module dependency
direction, gameplay authority mutation, foreign sibling material ownership,
default-material acceptance, empty/disabled/GPU/rendererless Niagara
acceptance, source-content drift, definition-specific radius/palette selection,
and fabricated package evidence. The remaining concern is the unavailable
Unreal compile/generation boundary; the source-selected engine template path
must be confirmed by that runner before generated VFX usability can be claimed.

---

## Review fix round 3/5 — 2026-08-26

This narrow fix closes the cross-emitter Niagara validation bypass. The prior
validator accumulated the expected emitter name and expected renderer class in
two system-wide booleans. Consequently, an enabled emitter named
`SimpleSpriteBurst` with no sprite renderer plus a different enabled emitter
with `NiagaraSpriteRendererProperties` could pass as if one source-selected
emitter satisfied both requirements.

### Strict TDD evidence

The executable native regression was added first. It explicitly constructs the
two-emitter bypass above, confirms that the legacy aggregate accepts that
invalid characterization, and requires the production predicate to reject it.
Before the production predicate existed, focused RED was:

```text
$ python3 -m unittest \
  Tests.Content.test_presentation_manifest.PresentationManifestTests.test_niagara_validation_cannot_combine_identity_and_renderer_across_emitters -v
... FAIL
fatal error: DAPresentationNiagaraEmitterValidation.h: No such file or directory
Ran 1 test in 0.008s
FAILED (failures=1)
```

After the minimal implementation, focused GREEN was:

```text
$ python3 -m unittest \
  Tests.Content.test_presentation_manifest.PresentationManifestTests.test_niagara_validation_cannot_combine_identity_and_renderer_across_emitters -v
... ok
Ran 1 test in 0.248s
OK
```

The same native executable additionally proves that a single exact authored
emitter passes, while disabled, GPU-targeted, looping, and wrong-renderer
versions of that selected emitter fail.

### Fix implemented

Added a dependency-free, header-only single-emitter validation predicate used
directly by the production commandlet and compiled by the native regression.
The commandlet now creates one validation view per real
`FNiagaraEmitterHandle`. A match is returned only when the same view is valid,
enabled, named `SimpleSpriteBurst`, has the source-authored `CPUSim` target, is
one-shot when `autoDeactivate` requires it, and owns an enabled renderer whose
class is `NiagaraSpriteRendererProperties`. Evidence from a second emitter can
no longer complete any part of that conjunction.

`simulationTarget` is now parsed from each canonical Niagara source-content
payload and compared to the selected emitter's `FVersionedNiagaraEmitterData`,
rather than being inferred only from a system-wide absence of GPU emitters.
The existing independent checks for exact source-derived exposed parameters,
fixed bounds, warmup, emitter count, source fingerprints, and Asset Registry
class/path remain unchanged.

### Final available verification

```text
$ python3 -m unittest discover -s Tests/Content -p 'test_presentation_manifest.py' -v
Ran 10 tests in 0.641s
OK

$ python3 -m unittest discover -s Tests/Content -p 'test_*.py'
Ran 68 tests in 0.681s
OK

$ python3 -m unittest discover -s Tests/UI -p 'test_*.py'
Ran 18 tests in 0.004s
OK
```

Additional GREEN checks: strict `-std=c++17 -Wall -Wextra -Werror` native
compilation and execution of the shared predicate regression; Python byte
compilation; canonical manifest validation at 50 primary, 25 VFX, 9 music, 12
ambient, and 60 minimum-compliant authored SFX; the 288-row artifact plan with
25 source-backed Niagara systems and 75 exact source-content rows; no legacy
cross-emitter aggregate remains; `git diff --check`; and no `.uasset` files are
present or claimed.

Fresh Unreal execution remains honestly blocked:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject \
  -ExecCmds='Automation RunTests Dominion.Content.PresentationCoverage;Quit' \
  -unattended -nop4 -nullrhi
/bin/bash: line 1: UnrealEditor-Cmd: command not found
exit=127
```

Therefore UHT/UBT compilation and real Asset Registry/package validation still
require the UE 5.8 handoff runner; no Unreal GREEN is claimed.

### Round self-review

Reviewed the helper and its real-handle adapter for accidental system-wide
name/renderer accumulation, disabled or invalid handles, renderer enabledness,
selected-emitter simulation target, source-target drift, one-shot enforcement,
and preservation of parameter/bounds/warmup checks. The one-shot result comes
from Niagara's public compiled system lifecycle query and is evaluated inside
the selected emitter conjunction; the generated source template is itself a
single one-shot sprite-burst system. The only remaining concern is confirmation
against the unavailable UE 5.8 compiler/runtime boundary above.

---

## Review fix round 4/5 — 2026-08-26

This round replaces the last system-wide Niagara lifecycle check with exact
selected-emitter authoring and inspection. The canonical Niagara source now
declares `emitter.lifecycle.mode = Self` and
`emitter.lifecycle.loopBehavior = Once`; that content is included in every one
of the 25 VFX artifact requirements, covered by the source SHA-1 and root
manifest fingerprint, and rejected by both the Python and C++ source parsers if
it drifts.

Generation duplicates the selected real engine template, resolves the named
`SimpleSpriteBurst` emitter's enabled `EmitterState` module through UE 5.8's
`UNiagaraExternalEditUtilities`, and authors its `LifeCycleMode` and
`LoopBehavior` enum inputs through `SetStackInputData`. The two writes use fresh
edit contexts because the first input gates visibility/editability of the
second. After compilation the commandlet independently walks that same named
emitter's Emitter Update topology, reads the persisted enum inputs with
`GetModuleInputValues`, and requires exact `Self` plus `Once`. The normal
post-save Asset Registry validation invokes the same independent reader again.
There is no `UNiagaraSystem::IsLooping()` lifecycle path remaining.

### Strict TDD evidence

The source-contract regression and dependency-free production mapper regression
were written first. The source test requires exact `Self/Once` lifecycle content
on all 25 emitted VFX rows and mutates each field independently after refreshing
both content hashes. Focused RED was:

```text
$ python3 -m unittest \
  Tests.Content.test_presentation_manifest.PresentationManifestTests.test_niagara_source_requires_selected_emitter_self_once_lifecycle -v
ERROR: test_niagara_source_requires_selected_emitter_self_once_lifecycle
KeyError: 'lifecycle'
Ran 1 test
FAILED (errors=1)
```

The native adapter-boundary regression uses persisted module records keyed
separately as `system`, `selected-emitter`, and `other-emitter`. System and
sibling evidence are exact `Self/Once`, while the selected emitter is
`Self/Infinite`. Before the production mapper existed, focused RED was:

```text
$ python3 -m unittest \
  Tests.Content.test_presentation_manifest.PresentationManifestTests.test_niagara_validation_cannot_combine_identity_and_renderer_across_emitters -v
error: 'FEmitterLifecycle' in namespace 'DA::Presentation::Validation' does not name a type
error: 'ReadEmitterLifecycle' is not a member of 'DA::Presentation::Validation'
Ran 1 test
FAILED (failures=1)
```

After the minimal shared mapper, source contract, and UE adapter implementation,
focused GREEN was:

```text
$ python3 -m unittest \
  Tests.Content.test_presentation_manifest.PresentationManifestTests.test_niagara_source_requires_selected_emitter_self_once_lifecycle \
  Tests.Content.test_presentation_manifest.PresentationManifestTests.test_niagara_validation_cannot_combine_identity_and_renderer_across_emitters -v
test_niagara_source_requires_selected_emitter_self_once_lifecycle ... ok
test_niagara_validation_cannot_combine_identity_and_renderer_across_emitters ... ok
Ran 2 tests in 0.402s
OK
```

The mapper is compiled directly by the native test with
`-std=c++17 -Wall -Wextra -Werror` and is the same header called by the real
commandlet adapter. It fails closed for missing, duplicated, disabled, or
ambiguous lifecycle module/input evidence. The regression explicitly
demonstrates that injecting system lifecycle would recreate the old false
positive, then proves that reading the selected-emitter key rejects it even
when both the system and another emitter are one-shot. An independently read
exact selected emitter remains the positive control.

### Final available verification

```text
$ python3 -m unittest discover -s Tests/Content -p 'test_presentation_manifest.py' -v
Ran 11 tests in 0.946s
OK

$ python3 -m unittest discover -s Tests/Content -p 'test_*.py'
Ran 69 tests in 0.931s
OK

$ python3 -m unittest discover -s Tests/UI -p 'test_*.py'
Ran 18 tests in 0.004s
OK

$ python3 Build/Tools/PresentationManifestTool.py validate
{"ambientLoops": 12, "constructionGrammars": 3, "coreVfx": 25,
 "musicCues": 9, "primaryAssets": 50, "sfxEvents": 60,
 "totalGeneratedDefinitions": 156}
```

The emitted artifact plan contains 288 unique paths: 50 static meshes, 50
material instances, 25 Niagara systems, 81 waves, 81 cues, and one level
sequence. It contains 75 exact source-content rows, and all 25 Niagara rows
carry the exact selected-emitter `Self/Once` lifecycle. The authored Niagara
source SHA-1 is `75478bfdc4fd948c66e131e062d89950245a53c0`.

Additional successful checks: Python byte compilation, parsing all 33
repository JSON files, pre-cook shell syntax, `git diff --check`, a source scan
confirming no commandlet `IsLooping()` call, and a repository scan finding no
`.uasset` files. No generated package or Asset Registry success is inferred
from checked-in JSON.

Fresh Unreal execution remains honestly blocked:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject \
  -ExecCmds='Automation RunTests Dominion.Content.PresentationCoverage;Quit' \
  -unattended -nop4 -nullrhi
/bin/bash: line 1: UnrealEditor-Cmd: command not found
exit=127

$ Build/Scripts/PreCookPresentation.sh \
  /workspace/scratch/17e0aee09308/DOMINION_ASCENDANT_repo/.worktrees/vertical-slice/DominionAscendant.uproject
Build/Scripts/PreCookPresentation.sh: line 7: UnrealEditor-Cmd: command not found
exit=127
```

Therefore UHT/UBT compilation and real UE 5.8 generation, package save/reload,
Asset Registry validation, Automation execution, and cook still require the
handoff runner; no Unreal GREEN is claimed.

### Round self-review

Reviewed exact emitter ownership across generation and validation, enabled
module selection, input namespace/enum normalization, duplicate-module and
duplicate-input ambiguity, write ordering, system and sibling-emitter leakage,
CPU/renderer conjunction preservation, source/hash/fingerprint drift, post-copy
compilation and re-read, and fabricated package evidence. Writes target only the
new duplicated system's source-selected emitter, never the engine template.
The remaining concern is the unavailable UE compile/runtime boundary, including
confirmation that the stock 5.8 template exposes the expected editable
`EmitterState` inputs; all external-edit types and calls used here are from the
5.8 NiagaraEditor public API, but they cannot be compiled in this environment.
