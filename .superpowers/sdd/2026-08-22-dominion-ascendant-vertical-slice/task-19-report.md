# Task 19 report — frozen vertical-slice definitions and starter deck

## Delivered

- Added `Content/DA/Manifests/VerticalSliceContent.json` as the single checked-in content authority and a draft-2020-12 schema that freezes exactly 64 definition rows, 32 starter-deck rows, and the 15 Synara / 15 Forgeweave / 15 Eden Circuit / 17 Universal / 1 Fusion / 1 Special split.
- Authored all exact display names from production spec v1.1 Sections 16-21 and every exact quantity from Section 22. The shared factory expands the 32 quantities into 60 owned `FCardInstance`s with deterministic stable GUIDs and binds the same IDs into `FDADeckState`.
- Added the Fusion Autonomous Factory, Founder Hall, and `UDA_DeckDefinition` representations. Founder Hall is present among the 64 persistent definitions and explicitly excluded from the 60-card deck.
- Added one strict runtime parser/validation/mapping pipeline used by the Automation spec, runtime registry fallback, and editor generation commandlet. It rejects missing, wrong-type, and unknown keys at root and every nested object; malformed IDs/prefab paths; wrong frozen counts; duplicate IDs/names; missing deck references; duplicate deck rows; bad quantities; negative/non-finite costs; missing placeable prefabs; authored-presence mismatches; and Dominion cache eligibility.
- Added public runtime registry rebuilding for mapped definitions while preserving Task 2's real definition and registry validation. The Task 19 spec then routes the built starter deck through Task 3's actual `FDADeckRules`, including a focused over-copy mutation.
- Added source-resolvable C++ graybox building, unit, leader, Wonder, and Founder Hall Actor classes. Every placeable manifest row points at a real `/Script/DominionGameplay.*` class; units and Leaders also carry representation paths but are not city-grid placeables.
- Added the `DominionEditor` module and `UDAContentManifestCommandlet`. In UE5.8 it validates the manifest through the same pipeline and can create the 64 expected card/building Data Assets plus `/Game/DA/Decks/DA_Deck_SynaraStarter60`.
- Configured Asset Manager scans for the generated card/deck assets and staged `DA/Manifests` as UFS so packaged runtime fallback can load the canonical source when generated binary assets are absent.
- Added an authored-value presence mask spanning deployment costs in all currencies, craft costs/throughput/facility, maintenance, per-cycle outputs, both faction pressures, all three Fusion modifiers, construction cycles, three utilities, and housing capacity. Raw fields are private and guarded `TryGet...` accessors return false without authored presence, so an omitted value cannot be consumed as a free zero or implicit one; explicitly authored zero remains distinguishable and valid.
- Made card and deck primary asset IDs depend on stable `DefinitionId`/`DeckId`, not UObject/package names. The manifest fingerprint is persisted on every generated card and the starter deck.
- Made the manifest authoritative even when generated assets exist. The registry uses a generated cache only when it is the exact 64-card stable-ID/category/name/value/authored-mask set and exact deck for the current fingerprint; partial, duplicate, stale-name, stale-value, stale-fingerprint, or stale-deck caches are rejected in favor of a fresh canonical fallback.
- Closed the remaining gameplay-readable definition surface: every manifest row now explicitly owns normalized `tags` and `upgradeBranchIds` arrays (currently empty by authority), cache parity compares both, and optional combat is omitted unless strictly authored. Combat raw data/presence are private, exposed only by `TryGetCombatDefinition`, and unknown nested combat keys cannot establish authorship.
- Audited every `UDA_CardDefinition` field: identity/category/rarity/placement/prefab/cache fields were already compared, all raw balance fields are private and compared, and combat/tags/upgrades are now covered. `CardArt` is the sole intentionally excluded field because it is presentation-only rather than gameplay-readable.
- Removed the public construction-cycle authoring setter. All authored raw fields and presence bits now mutate only inside the friend manifest mapper/import pipeline; tests obtain authored definitions through manifest fixtures.
- Normalized CRLF and bare-CR input to LF before hashing explicit UTF-8 bytes, and pinned manifest/schema JSON to LF in `.gitattributes`. LF, CRLF, and bare-CR inputs therefore target one SHA-1 cache identity.

## Numerical authority

No unsupported per-card economy tuning was invented. Properties absent from v0.8 are omitted from the manifest and unavailable through the runtime definition API. Legitimate authored zero is represented by property presence plus its authored bit. The mapped values that v0.8 actually authors are:

- Founder Hall: 1 Capital, 0.15 Insight, and 0.10 Influence per Development Cycle, plus explicit temporary housing capacity 24 (Section 6).
- Adaptive Habitat: 12 lifecycle Capital split into 4 craft and 8 deploy, with 0.04 maintenance derived from the v0.8 0.5% Residential formula (Sections 24 and 51).
- Guardian Drone Cohort: 8 craft Capital, 2 Insight, 5 Production Throughput, Synthetic Fabrication Node requirement, and 2 cycles (Section 52).
- Autonomous Exchange, Agency Forum, and active Thinking Spire Dependency values of +0.10, -0.20, and +0.35 per cycle (Section 47).
- Autonomous Factory: 24 Insight, -80% workforce requirement, +25% Industrial Throughput, +15% adjacent Industrial construction speed, +0.20 Dependency/cycle, and +0.15 Resource Hunger/cycle (Section 135).
- Thinking Spire: 220 Capital, 30 Insight, 25 Influence, and 12 cycles (Section 136).
- Grand Forge: 240 Capital, 20 Insight, 20 Influence, and 14 cycles (Section 137).
- Worldgarden: 200 Capital, 28 Insight, 30 Influence, and 14 cycles (Section 138).

Where v0.6 supplies an explicit footprint, the manifest cites it. Remaining 1x1 footprints are labeled graybox placeholders and are not balance authority.

## TDD evidence

`VerticalSliceContentSpec.cpp` was authored before the parser, manifest, deterministic factory, graybox classes, registry entry point, deck Data Asset schema, and commandlet. The blocker-correction tests were also written first. They cover all 21 gameplay-value presence categories (including an authored-zero case), exact v0.8 mappings including Founder Hall housing 24, stable primary IDs, exact/partial/stale cache behavior, strict JSON mutations at every object level, exact category/name/order counts, every literal Section 22 quantity, deterministic 60-instance ownership/deck binding, source-resolving prefabs, and real registry/deck-rule mutations.

### RED attempts

The initial missing-header test invocation and the later v0.8/asset-factory test invocation both stopped at the environment boundary:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject -ExecCmds="Automation RunTests Dominion.Content.VerticalSlice;Quit" -unattended -nop4
/bin/bash: UnrealEditor-Cmd: command not found
exit_code=127
```

UE5.8 is absent, so the intended compile/assertion failures could not be observed. Exit 127 is environment-blocked and is not an observed Automation RED.

For the blocker-correction round, the new authored-presence/static-schema tests were run before implementation and failed with two errors because the schema/manifest had no `authoredValues` contract. That is the observed static RED. The corresponding Unreal RED attempt again stopped at exit 127 before UHT, compilation, or assertions.

For correction round two, the new canonical tags/upgrades/combat schema test failed as intended because `tags` was absent from the required definition contract (`Ran 9 tests`, one failure, exit 1). Unreal coverage for guarded combat, cache parity, normalized fingerprints, and manifest-fixture construction was attempted before implementation but again stopped at exit 127.

### Final GREEN attempt

```text
$ UnrealEditor-Cmd DominionAscendant.uproject -unattended -nop4 -NullRHI -NoSound -ExecCmds="Automation RunTests Dominion.Content.VerticalSlice;Quit" -TestExit="Automation Test Queue Empty"
/bin/bash: UnrealEditor-Cmd: command not found
exit_code=127
```

This is environment-blocked and is **not** a passing UHT, UBT, commandlet, or Automation result.

The final commandlet validation attempt was likewise blocked:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject -run=DAContentManifest -ValidateOnly
/bin/bash: UnrealEditor-Cmd: command not found
exit_code=127
```

## Supplementary static evidence

```text
$ python3 -m unittest Tests.Content.test_vertical_slice_manifest -v
Ran 9 tests in 0.001s
OK

$ static prefab/identity check
STATIC_PREFAB_AND_ID_CHECKS=PASS
PLACEABLE_PREFAB_CLASSES=DAFounderHallGrayboxActor,DAGrayboxBuildingActor,DAGrayboxWonderActor

$ static line-ending/authority and public-mutator audits
STATIC_LINE_ENDING_AND_AUTHORITY_CHECKS=PASS
NORMALIZED_SHA1=6fac0d2d0aa87fb4884480a61f2e7661ca7f4e28
PUBLIC_AUTHORED_MUTATOR_AUDIT=PASS

$ git diff --check
exit_code=0
```

The Python environment does not contain `jsonschema` (`ModuleNotFoundError`), so the checked-in custom strict-schema test ran instead. These checks establish JSON/schema counts and nested shape, exact Section 22 quantities, exact v0.8 authored values with no extra supplied values, explicit empty tags/upgrades, strict optional-combat shape, Founder Hall housing 24, unique identities, nonnegative costs, cache constraints, checked-in C++ prefab declarations, and whitespace hygiene. They do not replace UE compilation or class loading.

## Missing binary generation caveat

No `.uasset` file was fabricated or checked in because this environment has no UE5.8 editor. The canonical manifests, schemas, source-resolvable prefab classes, and generation commandlet are present, but binary Data Asset generation remains pending on a UE5.8-capable runner:

```text
UnrealEditor-Cmd.exe DominionAscendant.uproject -run=DAContentManifest
UnrealEditor-Cmd.exe DominionAscendant.uproject -run=DAContentManifest -ValidateOnly
UnrealEditor-Cmd.exe DominionAscendant.uproject -unattended -nop4 -NullRHI -NoSound -ExecCmds="Automation RunTests Dominion.Content.VerticalSlice;Quit" -TestExit="Automation Test Queue Empty"
```

That run is required before claiming generated binary assets, UHT/API correctness, successful linking, commandlet execution, prefab class resolution in-engine, or real Automation GREEN.
