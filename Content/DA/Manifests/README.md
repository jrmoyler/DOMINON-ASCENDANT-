# Vertical-slice content source

`VerticalSliceContent.json` is the single checked-in authority for Task 19. Generated `.uasset` files are editor caches and must not be hand-authored or treated as a second source.

The manifest freezes:

- 64 definitions in the exact 15 Synara / 15 Forgeweave / 15 Eden Circuit / 17 Universal / 1 Fusion / 1 Special split from production spec v1.1;
- the exact 32-row, 60-instance Synara starter deck from v1.1 Section 22;
- only gameplay values actually authored by economy bible v0.8, including Founder Hall's temporary housing capacity of 24. Unauthored properties are omitted and cannot be read through the runtime API as free/zero/one balance values;
- explicit `tags` and `upgradeBranchIds` arrays on every definition, including canonical empty arrays. Combat is strictly optional and omitted until an authority supplies it; omitted combat cannot be read through the guarded runtime API;
- temporary `/Script/DominionGameplay.*` C++ graybox classes for source-resolvable prefab references.

Generate or validate the editor Data Assets in UE5.8:

```text
UnrealEditor-Cmd DominionAscendant.uproject -run=DAContentManifest
UnrealEditor-Cmd DominionAscendant.uproject -run=DAContentManifest -ValidateOnly
```

Generation writes the 64 definition assets beneath `/Game/DA/Cards` and `/Game/DA/Buildings`, plus `/Game/DA/Decks/DA_Deck_SynaraStarter60`. Each generated asset carries the source-manifest fingerprint. At runtime, the manifest remains authoritative: the generated cache is accepted only when all 64 stable IDs, categories, names, authored-value masks/values, and the exact deck match that fingerprint; partial or stale caches fall back to the packaged UFS manifest.

The fingerprint hashes explicit UTF-8 after normalizing CRLF and bare-CR line endings to LF. `.gitattributes` also pins manifest/schema JSON to LF so cache identity is checkout-platform independent.

`FirstHourQuests.json` is the separate, single checked-in authority for Task 20's exact nine first-hour quests and canonical `citizen.synara.nia_vale`. Its declared fingerprint is canonical sorted compact UTF-8 JSON excluding only the fingerprint field. It contains complete Task 18 quest graphs (typed runtime payloads and explicit authored `Choice` branches), concrete rewards, Task 19 definition IDs, start-time and dynamic-objective WorldAsset bindings, and the exact Human Override story-plus-proven-Task18-crisis Champion gate. Runtime accepts a generated cache only with exact nine-quest/Nia fingerprint parity and otherwise falls back to this packaged manifest. Validate or generate its editor cache in UE5.8 with:

```text
UnrealEditor-Cmd DominionAscendant.uproject -run=DAFirstHourQuest -ValidateOnly
UnrealEditor-Cmd DominionAscendant.uproject -run=DAFirstHourQuest
```

Generation writes the nine expected packages beneath `/Game/DA/Quests` and Nia at `/Game/DA/Citizens/DA_Citizen_NiaVale`. Those `.uasset` files are generated caches; the JSON remains authoritative.

`ForgeweaveConquest.json` freezes quests 20–24 and their exact package paths for Broker of Ironheart plus the Force, Economic, Influence, and Alliance routes. It also records the hard alliance thresholds and names the canonical systemic evidence each route consumes. Generate or validate real quest DataAssets without checking in placeholder binaries with:

```text
UnrealEditor-Cmd DominionAscendant.uproject -run=DAForgeweaveConquestContent
UnrealEditor-Cmd DominionAscendant.uproject -run=DAForgeweaveConquestContent -ValidateOnly
```

`FirstAscension.json` freezes quest 25, the Replication doctrine, all 15 Forgeweave unlocks, the Autonomous Factory effects and real-source building/cinematic package paths. `PreCookFirstAscension.sh` fails closed before creating packages when any authored source is absent, then validates exact Asset Registry class/path/fingerprint coverage.

```text
UnrealEditor-Cmd DominionAscendant.uproject -run=DAFirstAscensionContent
UnrealEditor-Cmd DominionAscendant.uproject -run=DAFirstAscensionContent -ValidateOnly
```

`DaxtonEncounter.json` is the separate frozen authority for the three encounter phases, exact six Leader outcomes, `/Game/DA/Leaders/DA_Leader_DaxtonRhe`, and the exact packages expected beneath `/Game/Characters/Daxton`. The Leader package is generated from the manifest. Character packages are imported only from the declared artist-authored FBX sources under `ContentSource/Characters/Daxton`; missing sources fail generation and no binary placeholders are checked in.

```text
UnrealEditor-Cmd DominionAscendant.uproject -run=DADaxtonContent
UnrealEditor-Cmd DominionAscendant.uproject -run=DADaxtonContent -ValidateOnly
```

`VerticalSlicePresentation.json` fingerprints the Task 26 building, character,
VFX, music, ambient, SFX, and event-binding source documents as one frozen
coverage unit. Runtime fallback retains every canonical recipe but never claims
generated provenance. The editor cache is usable only when all 156 generated
definition packages independently pass exact Asset Registry validation:

```text
UnrealEditor-Cmd DominionAscendant.uproject -run=DAPresentationContent
UnrealEditor-Cmd DominionAscendant.uproject -run=DAPresentationContent -ValidateOnly
```
