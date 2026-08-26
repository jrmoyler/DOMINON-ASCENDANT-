# Final integration fix report

Date: 2026-08-26

Base: `db85b7763e137d260eb5b1669859a7efd95fa930`

Release/performance disposition: **NOT_READY**

## Result

This pass closes the seven final-review finding groups at source and portable-contract level while preserving a single campaign authority. The canonical campaign snapshot now owns the complete persistent city state, the World subsystem owns production mutation, gameplay combat facades commit through a Core compare-and-swap interface, and UI/travel integrations are registered production subsystems. No plan or bible was changed, no binary Unreal asset was added, and no unavailable Unreal result is claimed.

The checked performance and release evidence intentionally remains `NOT_READY`. This pass is not a release sign-off: Unreal Engine 5.8 compilation, Automation execution, cook, reference-hardware capture, and the required human evidence remain unavailable in this environment.

## Strict RED/GREEN record

The focused portable contract `Tests/Release/test_final_integration_contract.py` was authored before the production changes. Its initial run exposed the reviewed seams: no registry-to-world starter bootstrap, no Core-owned persisted city aggregate, no shipped UI/travel service implementations, no Founder ASC/four granted abilities, gameplay combat bound to a save-object copy instead of a transaction authority, missing final save-envelope/invariant enforcement, and the `ADFounderCharacter`/private-hand compile blockers. The already-authored exact 60-copy manifest assertion stayed green and served as the frozen input rather than creating another starter-deck truth.

After implementation, the focused contract is 8/8 green as part of the 25-test Release suite. Production-facing native Automation tests were also updated to use canonical bootstrap or fully valid campaign aggregates instead of one-card/manual state injection. Migration, round-trip, tamper, damage/capture persistence, reconstruction, first-hour, UI navigation, performance soak, and release-route fixtures now exercise the stricter aggregate contract.

## Fixes by finding group

1. **Compile blockers and deck encapsulation**
   - Replaced the nonexistent `ADFounderCharacter` spelling with `ADAFounderCharacter` in gameplay, camera, test, and performance sources.
   - Removed external access to `FDADeckState::Hand`; callers use `GetHand()` and controlled setup/edit methods.
   - Added `FDADeckState::TrySwapInstance`, so UI deck edits atomically exchange one owned member for one owned available non-member without publishing an intermediate 59/61-card state.

2. **Canonical production bootstrap**
   - `UDAContentRegistrySubsystem` retains the deterministic collection/deck produced by the canonical manifest pipeline and exposes it by copy through `BuildStarterCampaignContent`.
   - `UDAWorldStateSubsystem::InitializeVerticalSliceState`, including `command.campaign.create`, consumes that output and creates the frozen start: 60 unique starter instances in the exact deck, Founder Hall with a distinct bidirectionally linked discovery card, 40 Capital, 12 Materials, 8 Insight, population 24, and the complete named roster with households.
   - The canonical state validates before publication and supports immediate placement from the seeded wallet/deck/collection.

3. **Persistent city simulation**
   - Moved the full reflected `FDACitySimulationState` and its persistent DTOs to `DominionCore` and embedded it in `FDACampaignSnapshot`.
   - Development cycles resolve construction, facility synchronization, real economy output/maintenance, and jobs; world ticks resolve migration. `LiveSignals` is projected from that state and committed in the same transaction.
   - Schema 19 serializes, migrates, validates, and round-trips facilities, wallet, citizens, households, jobs, utilities, migration accumulators, counters, and histories represented by the aggregate.

4. **Shipped integrations**
   - Added and registered `UDAUIAuthoritativeFeatureSubsystem` for research, conquest, Daxton leader resolution, Ascension, history/metrics, and Founder interactions, all through canonical owners.
   - Added and registered `UDARegionTravelRuntimeSubsystem`; reconstructed actors are disposable projections bound to the World campaign authority and retry reconstruction cannot duplicate owners.
   - Routed first-hour placement, travel, research, and Founder interaction ingress through the existing command/coordinator boundaries.
   - `ADAFounderCharacter` now implements `IAbilitySystemInterface`, owns a real ability-system component and combat attributes, initializes actor info, and grants four native scoped abilities: Precision Scan, Drone Barrier, Orchestration Mark, and Coordinated Override.
   - Authoritative feature snapshots now include late-game conquest, leader, Ascension, history, metrics, research, and interaction state used by viewmodels.

5. **Canonical combat state**
   - Added the Core-owned `IDACampaignAuthority` interface and implemented it in `UDAWorldStateSubsystem` and the persistence-only `UDACampaignSaveGame` test/compatibility owner.
   - Structural damage and capture resolve a fresh authoritative snapshot, mutate a candidate, synchronize owned projections, validate it, and publish through compare-and-swap. They no longer own mutable save snapshots.
   - Region reconstruction accepts the production authority directly; its snapshot overload is explicitly read-only and rejects commits.

6. **Snapshot/save invariants**
   - Validation enforces the exact authored 60-card starter acquisition partition, 60-card deck size and rarity copy limits, owned/unique membership, an exact draw/hand/reserve partition, and exact bidirectional `CardInstance.WorldAssetId` / `WorldAsset.CardInstanceId` / definition links with recovery state.
   - Validation also enforces canonical facility ownership and exact citizen/job/utility/wallet/population/cycle projections from city state to `LiveSignals`.
   - Historical migration repairs partial starter fixtures from the authored pipeline without retaining orphan starter identities; deployed historical cards are preserved as non-starter discoveries with their links intact.
   - Native save fixtures were upgraded to valid canonical aggregates, with forged/tampered graph and envelope regressions.

7. **Envelope versions**
   - Schema 19 writes `contentVersion` and `buildVersion`, includes both in checksum material, requires exact integral fields, and rejects missing, incompatible, future, or injected historical authority.
   - Explicit v18-to-v19 migration authors the city aggregate and compatibility fields; historical migration and current-version tests cover acceptance and rejection paths without silently laundering fields.

## Ownership and module-direction review

- `DominionCore` owns persistent DTOs, validation, save schema, and the mutation interface. It does not depend on Simulation, Gameplay, World, or UI.
- `DominionSimulation` operates on the Core city DTO through its existing Core dependency.
- `DominionGameplay` consumes the Core authority and Simulation types; it does not import World or UI.
- `DominionWorld` remains the production aggregate owner and may compose lower-level Simulation/Gameplay services.
- `DominionUI` remains the top-level adapter and depends on World; no lower module imports UI.
- Existing `Build.cs` dependencies already cover the new includes. No dependency was added in the reverse direction and no module cycle was introduced.
- New reflected structs/classes keep `.generated.h` last, persistent fields are `UPROPERTY`, transient UObject references are weak/object properties, and raw interface pointers are non-owning runtime bindings.

## Principal files

Core and persistence:

- `Source/DominionCore/Public/Campaign/DACampaignAuthority.h`
- `Source/DominionCore/Public/Simulation/DACitySimulationState.h`
- `Source/DominionCore/Public/Cards/DADeckState.h`
- `Source/DominionCore/Public/Content/DAContentRegistrySubsystem.h`
- `Source/DominionCore/Private/Content/DAContentRegistrySubsystem.cpp`
- `Source/DominionCore/Public/Save/DACampaignSaveGame.h`
- `Source/DominionCore/Private/Save/DACampaignSaveGame.cpp`
- `Source/DominionCore/Public/Save/DASaveJsonFields.h`
- `Source/DominionCore/Public/Save/DASaveSchema.h`
- `Source/DominionCore/Private/Save/DASaveService.cpp`

Simulation, gameplay, World, and UI:

- `Source/DominionSimulation/Public/{Citizens,Economy}/...` compatibility headers and economy implementation
- `Source/DominionGameplay/Public/Founder/DAFounderCharacter.h`
- `Source/DominionGameplay/{Public,Private}/Founder/DAFounderGameplayAbilities.*`
- `Source/DominionGameplay/{Public,Private}/{Damage,Capture}/...`
- `Source/DominionGameplay/{Public,Private}/World/DAWorldAsset.*`
- `Source/DominionWorld/{Public,Private}/Regions/DAWorldStateSubsystem.*`
- `Source/DominionWorld/{Public,Private}/Regions/DARegionAuthorityResolver.*`
- `Source/DominionWorld/{Public,Private}/Regions/DARegionTravelRuntimeSubsystem.*`
- `Source/DominionWorld/Private/AI/DAForgeweaveStrategy.cpp`
- `Source/DominionWorld/Private/Narrative/DAFirstHourCampaignCoordinatorSubsystem.cpp`
- `Source/DominionUI/{Public,Private}/Commands/DAUIAuthoritativeFeatureSubsystem.*`
- `Source/DominionUI/{Public,Private}/Commands/DAUICommandEndpoint.*`
- `Source/DominionUI/Private/ViewModels/DAUIViewModels.cpp`

Tests:

- `Tests/Release/test_final_integration_contract.py`
- `Source/DominionTests/Private/Core/{SaveRoundTripSpec,SaveMigrationSpec}.cpp`
- Production-path and invariant fixture updates under `Source/DominionTests/Private/{Content,Gameplay,Performance,Release,UI,World}`
- Portable contract updates in `Tests/Content/test_first_hour_quest_manifest.py` and `Tests/UI/test_task21_authority_contract.py`

## Fresh verification

Portable Python suites:

```text
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s Tests/Content -p 'test_*.py' -v
Ran 69 tests in 0.952s — OK

PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s Tests/UI -p 'test_*.py' -v
Ran 18 tests in 0.006s — OK

PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s Tests/Performance -p 'test_*.py' -v
Ran 8 tests in 0.233s — OK

PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s Tests/Release -p 'test_*.py' -v
Ran 25 tests in 0.390s — OK (includes final integration contract 8/8)
```

Portable native contracts, all compiled with `g++ -std=c++17 -Wall -Wextra -pedantic` and executed successfully:

```text
Tests/Performance/SimulationLODPolicyTest.cpp                 exit 0
Tests/Performance/SoakProgressGuardTest.cpp                   exit 0
Tests/Content/Native/PresentationNiagaraEmitterValidationTest.cpp exit 0
```

Repository integrity:

```text
34 JSON files under Config/Content/Tests parsed with python3 -m json.tool — PASS
git diff --check — PASS
find . -type f -name '*.uasset' — no results
binding plan/spec diff — no results
known blocker scan — no ADFounderCharacter in Source and no private DeckState.Hand access
Build/Performance/Task27PerformanceEvidence.json — NOT_READY
Build/Release/Task28ReleaseEvidence.json — NOT_READY
```

## Unreal Engine limitation

The required Unreal attempts were made honestly from the repository root:

```text
UnrealBuildTool DominionAscendantEditor Linux Development -Project=.../DominionAscendant.uproject
exit 127: /bin/bash: UnrealBuildTool: command not found

UnrealEditor-Cmd .../DominionAscendant.uproject -unattended -nop4 -nullrhi \
  -ExecCmds="Automation RunTests Dominion;Quit" \
  -TestExit="Automation Test Queue Empty"
exit 127: /bin/bash: UnrealEditor-Cmd: command not found
```

Therefore no Unreal C++ compilation, UHT pass, Automation result, cook, or runtime behavior is claimed. No sidecar, `.uasset`, screenshot, video, benchmark, or human attestation was fabricated to fill that gap.

## Self-review and residual risk

- Reviewed every changed C++/header integration seam for generated-header placement, reflected persistence, include availability, interface lifetime, subsystem registration/deinitialization, and dependency direction.
- Rechecked copy/rebind behavior for `FDADeckState` so its runtime collection pointer never becomes serialized authority.
- Rechecked city-to-live projection and combat-owned facility/card synchronization before snapshot validation and compare-and-swap publication.
- Replaced strict-invariant-breaking one-card/manual native fixtures with canonical bootstrap or fully linked aggregates; static suites cannot prove those UE test translation units compile.
- The remaining material risk is exactly the unavailable UE5.8/UHT/Automation/cook/runtime environment. Performance and release evidence correctly remains `NOT_READY` until those checks and the required hardware/human protocols are actually run.

## Residual corrective pass

This bounded follow-up closes the seven Important residual findings from the final whole-branch re-review. It supersedes the earlier bootstrap resource wording above: the frozen production wallet is exactly **40 Capital, 12 Insight, and 8 Influence**; no Materials balance is substituted for either authored currency.

### Residual RED/GREEN evidence

Strict RED was captured in `Tests/Release/test_final_integration_contract.py` before changing production code: 7 newly added semantic contracts failed and 8 existing contracts passed. The failures corresponded one-for-one to bootstrap/opening hand, real travel loading/reconstruction, authored Founder ability behavior, universal campaign revision CAS, rival collection isolation, a persisted deployed deck zone, and lossless v18-to-v19 facility/utility synthesis.

After the production changes, the focused contract is 15/15 green. Native Automation regressions were added at the production entry points even though the unavailable Unreal executable prevents running them here:

- `SaveRoundTripSpec.cpp` covers exact `command.campaign.create` state, immediate placement, overlapping damage/capture CAS rejection, and revision save/load preservation.
- `TradeDiplomacySpec.cpp` injects only the platform loader boundary and proves `EnsureRegionLoaded` invokes it with the authored map, persisted local actors reconstruct, canonical WorldAssets reconstruct, and missing maps fail closed.
- `CombatAttributesSpec.cpp` checks all four authored cooldown/scope/duration/effect definitions and activates Drone Barrier through the Founder's real ASC, including charge, timed status, cooldown, cover creation, and cleanup.
- `ForgeweaveAISpec.cpp`, `DeckRulesSpec.cpp`, and `SaveMigrationSpec.cpp` cover rival provenance isolation, deployed-zone transitions/no replay, and exact nonempty utility plus facility migration respectively.

### Residual fixes

1. **Exact bootstrap and opening hand.** The World production bootstrap now seeds 40 Capital, 12 Insight, and 8 Influence, leaves unrelated Materials untouched, draws through `FDADeckState::DrawOpeningHand`, and places an authored untimed starter card immediately through the canonical placement transaction. The Founder Hall origin was corrected to a valid in-bounds capital-grid location.
2. **Real travel handoff.** Each canonical region persists an authored soft map path. The production runtime checks the package, requests a blocking dynamic level-stream load, verifies a loaded level, and fails closed otherwise. Reconstruction materializes both persisted `LocalActors` and canonical regional `WorldAssets`; runtime actors remain disposable projections of `UDAWorldStateSubsystem`.
3. **Functional Founder GAS abilities.** The JSON definition source now authors target scope, duration, range, and concrete effect identity. The four native abilities resolve that source at activation, enforce the exact 8/18/12/60-second cooldowns and Tactical Charge, apply duration-backed GameplayEffects to scoped targets, and remove them automatically. Drone Barrier additionally registers and removes real deployable cover through the existing cover subsystem; no permanent loose status tags are used.
4. **Universal mutation revision.** `FDACampaignSnapshot::CampaignMutationRevision` is reflected and persisted. Every accepted campaign CAS requires the candidate's base revision to equal the current authority and advances the revision once; prepared world-tick publication and durable travel-stage mutations also advance it. Competing damage/capture candidates from one base cannot both commit, and round-trip retains the token.
5. **Rival ownership isolation.** Forgeweave construction creates a rival WorldAsset plus deterministic rival planner provenance in regional AI state, with no player `FCardInstance`, no player collection insertion, and no collection-viewmodel exposure. Snapshot validation and migration enforce that separation; only the existing conquest/Ascension unlock flow can mint player-owned Forgeweave cards.
6. **Persisted deployed zone.** `FDADeckState` owns a reflected `Deployed` zone. Placement atomically transfers Hand to Deployed, recovery/cancellation transfers it back to one legal playable zone, and migration restores legacy deployed links without duplicating membership. Validation partitions all 60 deck members exactly across Draw/Hand/Reserve/Deployed and cross-checks every deployed member's recovery and WorldAsset link.
7. **Lossless v18-to-v19 synthesis.** Migration carries persisted utility signals and reconstructs every facility field from the canonical definition plus persisted live/asset evidence: identity/linkage, type, deployment cost, authored maintenance, outputs, staffing, automation/modifiers, demand, utility, condition, and wonder rate. Missing definitions, ambiguous rival provenance/maps, or absent required utility evidence fail migration rather than inventing state.

### Fresh residual verification

```text
Focused final-integration contract   15/15  PASS
Tests/Content                        69/69  PASS
Tests/UI                             18/18  PASS
Tests/Performance                     8/8   PASS
Tests/Release                        32/32  PASS

SimulationLODPolicyTest.cpp                 compile/run exit 0
SoakProgressGuardTest.cpp                   compile/run exit 0
PresentationNiagaraEmitterValidationTest.cpp compile/run exit 0

JSON parse                             34/34 PASS
git diff --check                             PASS
binding plan/spec diff                       clean
*.uasset search                              none
ADFounderCharacter/private DeckState.Hand    none
Task27PerformanceEvidence.json               NOT_READY
Task28ReleaseEvidence.json                   NOT_READY
```

The Unreal attempts were repeated after the final source edit:

```text
UnrealBuildTool DominionAscendantEditor Linux Development -Project=.../DominionAscendant.uproject
/bin/bash: line 1: UnrealBuildTool: command not found
exit 127

UnrealEditor-Cmd .../DominionAscendant.uproject -unattended -nop4 -nullrhi \
  '-ExecCmds=Automation RunTests Dominion;Quit' \
  '-TestExit=Automation Test Queue Empty'
/bin/bash: line 1: UnrealEditor-Cmd: command not found
exit 127
```

No UE compile, UHT, Automation, cook, runtime streaming, GAS runtime, performance, or release-readiness claim is inferred from the portable results. Performance and release evidence remains intentionally `NOT_READY`.

### Residual self-review

- Re-read the changed Core, Gameplay, World, UI, and test seams for obvious C++/UE API hazards. New persisted fields are reflected with `UPROPERTY`; every changed reflected header keeps its generated header last; transient loader/streaming projections are not serialized.
- Checked dynamic-level-streaming arguments, weak runtime ownership, teardown behavior, timed GameplayEffect tag removal, Founder actor-info/ability grants, deck private access, move/copy handling for the revision, and integer-revision overflow guards.
- Rechecked module direction: Core owns DTO/save invariants, Simulation consumes Core, Gameplay consumes Core/Simulation, World composes lower layers, and UI adapts World. Existing `Build.cs` dependencies cover the new APIs; no reverse dependency or cycle was added.
- Rechecked canonical ownership: one persisted campaign owns deck/city/world/rival state; runtime travel actors and viewmodels are projections; damage and capture publish only through the World CAS owner.
- The only material unresolved verification risk is the absent Unreal 5.8 toolchain. The native Automation sources cannot be proven to compile or execute until UBT/UHT/Automation is available.
