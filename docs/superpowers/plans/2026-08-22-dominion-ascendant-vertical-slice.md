# DOMINION // ASCENDANT Vertical Slice Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the finite 6–10 hour Windows vertical slice defined in v1.1: a persistent Synara card-built city, living citizens, Founder/Command/City gameplay, Forgeweave/Eden regional simulation, four Forgeweave conquest routes, and the first Ascension.

**Architecture:** Use Unreal Engine 5.8 with C++ as the authoritative gameplay/simulation layer and Blueprints/Data Assets for content authoring. A single `UDA_CardDefinition` source of truth drives card UI, placement, construction, economy, simulation, combat, and asset representation; unique runtime `FCardInstance` records create persistent `ADAWorldAsset` actors when placed. City/citizen/world systems run at explicit simulation rates and persist independently from streamed 3D actors.

**Tech Stack:** Unreal Engine 5.8; C++; Blueprints; Primary Data Assets / Asset Manager; Gameplay Ability System; Enhanced Input; StateTree; Mass Entity for crowd representation; World Partition; Niagara; MetaSounds; CommonUI; Unreal Automation Tests; Git LFS for binary assets.

**Spec:** `DOMINION_ASCENDANT_Vertical_Slice_Production_Spec_v1.1.md`

## Global Constraints

- Target platform: Windows PC only for the vertical slice.
- Engine baseline: Unreal Engine 5.8.
- Exactly 3 major civilizations: Synara, Forgeweave, Eden Circuit.
- Player founding civilization: Synara only.
- Player build grid: 32×32 logical cells; each cell 8 m × 8 m.
- Development Cycle: 30 seconds of active simulation.
- World Tick: 5 Development Cycles.
- Starter deck: exactly 60 cards as frozen in v1.1.
- Content definitions: exactly 64 persistent/playable definitions in vertical-slice scope.
- Starting regional population: exactly 200 simulated residents, including 20 named citizens.
- Required authored quests: exactly 25.
- Required systemic world events: exactly 6.
- Required major Leaders: Mira Vey, Daxton Rhe, Amara Venn.
- Required Forgeweave resolutions: Force, Economic, Influence, Alliance.
- Maximum directly commanded force: 3 squads + 1 vehicle.
- Module-based destruction only for the 8 structures named in v1.1.
- Required UI surfaces: 27.
- Required core VFX definitions: 25.
- Required music cues: 9; ambient loops: 12; authored SFX events: minimum 60.
- Save state must survive exact city layout, Card Instances, citizens, quests/events, relationships, Forgeweave simulation, conquest, Leader state, and first Ascension.
- No multiplayer, PvP, co-op, live-service, marketplace, full 20-civilization production, Axiom Crown, or 128×128 player city in this milestone.
- Every system-changing content path must operate on the real persistent world state; no disposable mission-only duplicates of persistent city assets.
- TDD by default: failing automation/spec test first, minimal implementation second, regression suite before commit.
- Commit after every independently reviewable task.

---

# 0. Repository and Module Map

Create the project with the following source boundaries.

```text
DominionAscendant/
├─ DominionAscendant.uproject
├─ Config/
│  ├─ DefaultEngine.ini
│  ├─ DefaultGame.ini
│  └─ DefaultInput.ini
├─ Content/
│  ├─ DA/
│  │  ├─ Cards/
│  │  ├─ Civilizations/
│  │  ├─ Leaders/
│  │  ├─ Quests/
│  │  └─ Events/
│  ├─ Maps/
│  │  ├─ SynaraFrontier/
│  │  ├─ Ironheart/
│  │  ├─ EdenBasin/
│  │  └─ FrontierCorridor/
│  ├─ UI/
│  ├─ Characters/
│  ├─ Buildings/
│  ├─ VFX/
│  └─ Audio/
├─ Source/
│  ├─ DominionCore/
│  │  ├─ Public/
│  │  └─ Private/
│  ├─ DominionSimulation/
│  │  ├─ Public/
│  │  └─ Private/
│  ├─ DominionGameplay/
│  │  ├─ Public/
│  │  └─ Private/
│  ├─ DominionWorld/
│  │  ├─ Public/
│  │  └─ Private/
│  ├─ DominionUI/
│  │  ├─ Public/
│  │  └─ Private/
│  └─ DominionTests/
│     └─ Private/
└─ docs/
   └─ superpowers/
      ├─ specs/
      └─ plans/
```

## Module responsibilities

- `DominionCore`: stable IDs, card/civilization definitions, runtime records, registries, save DTOs.
- `DominionSimulation`: time, economy, utilities, adjacency, citizens, jobs, factions, migration, world-tick simulation.
- `DominionGameplay`: Founder, abilities, squads, combat, damage, construction actors, capture.
- `DominionWorld`: regions, travel, rival AI, diplomacy, trade, quests/events, conquest, Ascension orchestration.
- `DominionUI`: CommonUI screens, HUDs, overlays, inspect panels, deck/collection, accessibility presentation.
- `DominionTests`: automation, soak, serialization, quest graph, integration, and regression tests.

---

### Task 1: Bootstrap the UE 5.8 project and module boundaries

**Files:**
- Create: `DominionAscendant.uproject`
- Create: `Source/DominionCore/DominionCore.Build.cs`
- Create: `Source/DominionSimulation/DominionSimulation.Build.cs`
- Create: `Source/DominionGameplay/DominionGameplay.Build.cs`
- Create: `Source/DominionWorld/DominionWorld.Build.cs`
- Create: `Source/DominionUI/DominionUI.Build.cs`
- Create: `Source/DominionTests/DominionTests.Build.cs`
- Create: `Source/DominionAscendant.Target.cs`
- Create: `Source/DominionAscendantEditor.Target.cs`
- Create: `.gitignore`
- Create: `.gitattributes`
- Test: `Source/DominionTests/Private/Bootstrap/ModuleLoadSpec.cpp`

**Interfaces:**
- Consumes: none.
- Produces: loadable modules `DominionCore`, `DominionSimulation`, `DominionGameplay`, `DominionWorld`, `DominionUI`, `DominionTests`.

- [ ] **Step 1: Write the module-load automation test**

```cpp
BEGIN_DEFINE_SPEC(FDAModuleLoadSpec, "Dominion.Bootstrap.Modules",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FDAModuleLoadSpec)

void FDAModuleLoadSpec::Define()
{
    It("loads every runtime module", [this]()
    {
        FModuleManager& Modules = FModuleManager::Get();
        TestTrue("DominionCore", Modules.IsModuleLoaded("DominionCore"));
        TestTrue("DominionSimulation", Modules.IsModuleLoaded("DominionSimulation"));
        TestTrue("DominionGameplay", Modules.IsModuleLoaded("DominionGameplay"));
        TestTrue("DominionWorld", Modules.IsModuleLoaded("DominionWorld"));
        TestTrue("DominionUI", Modules.IsModuleLoaded("DominionUI"));
    });
}
```

- [ ] **Step 2: Build tests before runtime modules exist**

Run:

```powershell
Engine\Build\BatchFiles\Build.bat DominionAscendantEditor Win64 Development "<repo>\DominionAscendant.uproject" -WaitMutex
```

Expected: build/load failure because project modules do not yet exist.

- [ ] **Step 3: Create module rules with explicit dependencies**

Use:
- Core → `Core`, `CoreUObject`, `Engine`, `GameplayTags`
- Simulation → `DominionCore`, `MassEntity`, `MassCommon`, `NavigationSystem`
- Gameplay → `DominionCore`, `DominionSimulation`, `GameplayAbilities`, `GameplayTags`, `EnhancedInput`, `AIModule`, `StateTreeModule`
- World → `DominionCore`, `DominionSimulation`, `DominionGameplay`, `AIModule`
- UI → `DominionCore`, `DominionSimulation`, `DominionGameplay`, `DominionWorld`, `CommonUI`, `UMG`

Do not add cross-module dependencies in the opposite direction.

- [ ] **Step 4: Build and run the module test**

Run:

```powershell
UnrealEditor-Cmd.exe DominionAscendant.uproject -ExecCmds="Automation RunTests Dominion.Bootstrap.Modules;Quit" -unattended -nop4
```

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add DominionAscendant.uproject Source Config .gitignore .gitattributes
git commit -m "chore: bootstrap Dominion Ascendant UE5.8 modules"
```

---

### Task 2: Stable IDs, content definitions, and the authoritative card registry

**Files:**
- Create: `Source/DominionCore/Public/Content/DACardDefinition.h`
- Create: `Source/DominionCore/Private/Content/DACardDefinition.cpp`
- Create: `Source/DominionCore/Public/Content/DACivilizationDefinition.h`
- Create: `Source/DominionCore/Public/Content/DAContentRegistrySubsystem.h`
- Create: `Source/DominionCore/Private/Content/DAContentRegistrySubsystem.cpp`
- Create: `Source/DominionCore/Public/Content/DAContentTypes.h`
- Test: `Source/DominionTests/Private/Core/CardDefinitionSpec.cpp`
- Test: `Source/DominionTests/Private/Core/ContentRegistrySpec.cpp`

**Interfaces:**
- Consumes: UE Asset Manager.
- Produces:
  - `UDA_CardDefinition : UPrimaryDataAsset`
  - `UDA_CivilizationDefinition : UPrimaryDataAsset`
  - `UDAContentRegistrySubsystem::GetCardDefinition(const FPrimaryAssetId&) const`
  - `UDAContentRegistrySubsystem::ValidateRegistry(TArray<FText>& Errors) const`

- [ ] **Step 1: Write the failing card-definition validation test**

```cpp
It("rejects a placeable card without a world prefab", [this]()
{
    UDA_CardDefinition* Def = NewObject<UDA_CardDefinition>();
    Def->DefinitionId = FName("synara.adaptive_habitat");
    Def->CardType = EDACardType::Residential;
    Def->Footprint = FIntPoint(2, 2);
    Def->bPlaceable = true;

    TArray<FText> Errors;
    TestFalse("Invalid definition", Def->Validate(Errors));
    TestTrue("Missing prefab reported", Errors.Num() > 0);
});
```

- [ ] **Step 2: Run the test and verify failure**

Run `Dominion.Core.CardDefinition`.

Expected: compile/test failure because `UDA_CardDefinition` and `Validate` do not exist.

- [ ] **Step 3: Implement definition types**

Minimum `UDA_CardDefinition` properties:

```cpp
UPROPERTY(EditDefaultsOnly) FName DefinitionId;
UPROPERTY(EditDefaultsOnly) FPrimaryAssetId CivilizationId;
UPROPERTY(EditDefaultsOnly) EDACardType CardType;
UPROPERTY(EditDefaultsOnly) EDARarity Rarity;
UPROPERTY(EditDefaultsOnly) FIntPoint Footprint;
UPROPERTY(EditDefaultsOnly) bool bPlaceable = true;
UPROPERTY(EditDefaultsOnly) TSoftClassPtr<AActor> WorldPrefab;
UPROPERTY(EditDefaultsOnly) TSoftObjectPtr<UTexture2D> CardArt;
UPROPERTY(EditDefaultsOnly) int32 DeploymentCapital = 0;
UPROPERTY(EditDefaultsOnly) float MaintenanceCapitalPerCycle = 0.f;
UPROPERTY(EditDefaultsOnly) FDACardUtilityDemand UtilityDemand;
UPROPERTY(EditDefaultsOnly) FDACombatDefinition Combat;
UPROPERTY(EditDefaultsOnly) TArray<FGameplayTag> Tags;
```

Implement `Validate` to fail on:
- empty stable ID,
- non-positive footprint for placeable cards,
- missing prefab for placeable cards,
- negative costs,
- duplicate upgrade branch IDs,
- Dominion cards marked as random-cache eligible.

- [ ] **Step 4: Write registry uniqueness test**

Create two definitions with the same `DefinitionId`; assert `ValidateRegistry` returns a duplicate-ID error.

- [ ] **Step 5: Implement registry indexing**

At subsystem initialization:
- enumerate primary assets of card/civilization types;
- build `TMap<FName, TSoftObjectPtr<UDA_CardDefinition>>`;
- reject duplicate stable IDs in development/editor builds.

- [ ] **Step 6: Run core registry tests**

Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add Source/DominionCore Source/DominionTests
git commit -m "feat: add authoritative content definitions and registry"
```

---

### Task 3: Card Instances, collection ownership, and the 60-card deck

**Files:**
- Create: `Source/DominionCore/Public/Cards/DACardInstance.h`
- Create: `Source/DominionCore/Public/Cards/DACollectionState.h`
- Create: `Source/DominionCore/Public/Cards/DADeckState.h`
- Create: `Source/DominionCore/Private/Cards/DADeckRules.cpp`
- Create: `Source/DominionCore/Public/Cards/DADeckRules.h`
- Test: `Source/DominionTests/Private/Core/DeckRulesSpec.cpp`

**Interfaces:**
- Consumes: `UDA_CardDefinition`.
- Produces:
  - `FCardInstance { FGuid InstanceId; FName DefinitionId; ... }`
  - `FDACollectionState::AddInstance(FName DefinitionId, EDAAcquisitionSource Source)`
  - `FDADeckRules::Validate(const FDADeckState&, const UDAContentRegistrySubsystem&)`
  - `FDADeckState::Draw() -> TOptional<FGuid>`

- [ ] **Step 1: Write copy-limit and 60-card validation tests**

Test:
- Common/Specialized max 3,
- Elite max 2,
- Legendary/Mythic/Wonder/Leader max 1,
- exact 60-card deck accepted,
- 59 and 61 rejected.

- [ ] **Step 2: Run tests and verify they fail**

Expected: missing card/deck types.

- [ ] **Step 3: Implement `FCardInstance` and collection ownership**

Use `FGuid::NewGuid()` for instance identity. Store:
- acquisition World Tick,
- acquisition source,
- mastery XP,
- upgrade state,
- cosmetic variant,
- recovery state,
- optional `WorldAssetId`.

- [ ] **Step 4: Implement deck validation and deterministic shuffle**

Deck state stores **Instance IDs**, never repeated definition names without owned instances.

Use a campaign-seeded `FRandomStream` for shuffle.

- [ ] **Step 5: Add opening-hand and reserve-queue tests**

Verify:
- opening hand 7,
- hand cap 10,
- reserve cap 3,
- oldest reserve card bottoms if reserve is full.

- [ ] **Step 6: Run tests and commit**

```bash
git add Source/DominionCore Source/DominionTests
git commit -m "feat: add persistent card collection and deck rules"
```

---

### Task 4: Grid occupancy and placement validation

**Files:**
- Create: `Source/DominionSimulation/Public/City/DACityGridSubsystem.h`
- Create: `Source/DominionSimulation/Private/City/DACityGridSubsystem.cpp`
- Create: `Source/DominionSimulation/Public/City/DAPlacementTypes.h`
- Test: `Source/DominionTests/Private/Simulation/CityGridSpec.cpp`

**Interfaces:**
- Consumes: card footprint/placement requirements.
- Produces:
  - `FDAPlacementResult ValidatePlacement(const FDACardPlacementRequest&) const`
  - `bool ReserveFootprint(const FDAWorldAssetId&, const FIntPoint&, const FIntPoint&, EGridRotation)`
  - `void ReleaseFootprint(const FDAWorldAssetId&)`

- [ ] **Step 1: Write tests for bounds, overlap, rotation, and ownership**

Use a 32×32 test grid. Verify:
- 2×2 footprint at `(0,0)` succeeds;
- same footprint at `(31,31)` fails;
- overlapping reservation fails;
- rotated 1×2 occupies correct cells;
- unclaimed sector fails.

- [ ] **Step 2: Run tests and verify failure**

- [ ] **Step 3: Implement grid as a pure simulation structure**

Do not query meshes to determine occupancy.

Use compact cell records:

```cpp
struct FDAGridCell
{
    FDAWorldAssetId Occupant;
    uint8 LayerMask = 0;
    bool bRoad = false;
    bool bClaimed = false;
};
```

- [ ] **Step 4: Add placement-reason test**

`FDAPlacementResult` must return machine-readable codes:
- `OutOfBounds`
- `Occupied`
- `Unclaimed`
- `TerrainInvalid`
- `RoadMissing`
- `UtilityUnavailable`

- [ ] **Step 5: Run tests and commit**

```bash
git add Source/DominionSimulation Source/DominionTests
git commit -m "feat: add deterministic city grid placement service"
```

---

### Task 5: World Asset lifecycle and construction state machine

**Files:**
- Create: `Source/DominionGameplay/Public/World/DAWorldAsset.h`
- Create: `Source/DominionGameplay/Private/World/DAWorldAsset.cpp`
- Create: `Source/DominionSimulation/Public/City/DAWorldAssetRecord.h`
- Create: `Source/DominionGameplay/Public/Construction/DAConstructionComponent.h`
- Create: `Source/DominionGameplay/Private/Construction/DAConstructionComponent.cpp`
- Test: `Source/DominionTests/Private/Gameplay/WorldAssetLifecycleSpec.cpp`

**Interfaces:**
- Consumes: `FCardInstance`, grid reservation, `UDA_CardDefinition`.
- Produces:
  - `FDAWorldAssetRecord`
  - `ADAWorldAsset::InitializeFromRecord(const FDAWorldAssetRecord&)`
  - `UDAConstructionComponent::AdvanceCycle()`
  - state enum: Preview/Foundation/Frame/Shell/Systems/Operational/Damaged/Disabled/Ruined/Reconstructing.

- [ ] **Step 1: Write lifecycle test for Adaptive Habitat**

Test progression from construction to Operational after the definition's construction-cycle count.

- [ ] **Step 2: Run and verify failure**

- [ ] **Step 3: Implement persistent record separate from actor**

The actor must be reconstructible from:

```cpp
struct FDAWorldAssetRecord
{
    FGuid WorldAssetId;
    FGuid CardInstanceId;
    FName CardDefinitionId;
    FName CityId;
    FIntPoint GridOrigin;
    uint8 Rotation;
    EDAConstructionState ConstructionState;
    float StructuralIntegrity;
    FName OwnerCivilizationId;
};
```

- [ ] **Step 4: Implement construction callbacks**

Construction component emits:
- `OnStageChanged`
- `OnConstructionCompleted`

Blueprint presentation binds to these; simulation does not depend on VFX.

- [ ] **Step 5: Run lifecycle tests and commit**

```bash
git add Source/DominionGameplay Source/DominionSimulation Source/DominionTests
git commit -m "feat: add persistent world asset construction lifecycle"
```

---

### Task 6: Save framework, schema versioning, and transactional campaign persistence

**Files:**
- Create: `Source/DominionCore/Public/Save/DACampaignSaveGame.h`
- Create: `Source/DominionCore/Public/Save/DASaveSchema.h`
- Create: `Source/DominionCore/Public/Save/DASaveMigration.h`
- Create: `Source/DominionCore/Private/Save/DASaveService.cpp`
- Create: `Source/DominionCore/Public/Save/DASaveService.h`
- Test: `Source/DominionTests/Private/Core/SaveRoundTripSpec.cpp`
- Test: `Source/DominionTests/Private/Core/SaveMigrationSpec.cpp`

**Interfaces:**
- Consumes: card, asset, city/world DTOs.
- Produces:
  - `FDASaveResult SaveCampaign(const FDACampaignSnapshot&, FString Slot)`
  - `TResult<FDACampaignSnapshot, FDASaveError> LoadCampaign(FString Slot)`
  - `int32 CurrentSchemaVersion`

- [ ] **Step 1: Write Adaptive Habitat round-trip test**

Serialize one card instance + world asset; load and assert:
- same IDs,
- placement,
- construction state,
- integrity,
- mastery,
- history.

- [ ] **Step 2: Write migration test from schema v1 to v2**

Fixture v1 lacks `HistoryTags`; migration must add an empty array without losing other state.

- [ ] **Step 3: Implement snapshot DTOs and versioned migration chain**

Never serialize full live Actors.

- [ ] **Step 4: Implement transactional write**

Write:
1. temporary slot,
2. checksum,
3. rotate prior backup,
4. atomically replace active slot.

- [ ] **Step 5: Run save tests and commit**

```bash
git add Source/DominionCore Source/DominionTests
git commit -m "feat: add versioned transactional campaign saves"
```

---

### Task 7: Simulation clock, Development Cycles, and World Ticks

**Files:**
- Create: `Source/DominionSimulation/Public/Time/DASimulationClockSubsystem.h`
- Create: `Source/DominionSimulation/Private/Time/DASimulationClockSubsystem.cpp`
- Test: `Source/DominionTests/Private/Simulation/SimulationClockSpec.cpp`

**Interfaces:**
- Produces:
  - `OnDevelopmentCycle(int64 CycleIndex)`
  - `OnWorldTick(int64 WorldTickIndex)`
  - `SetSimulationRate(float Rate)`
  - `SetPaused(bool)`

- [ ] **Step 1: Write deterministic clock test**

Simulate 150 active seconds.

Expected:
- 5 Development Cycles
- 1 World Tick

- [ ] **Step 2: Test pause and 4× fast-forward**

Paused time does not advance. Four-times mode advances simulation proportionally only when allowed.

- [ ] **Step 3: Implement clock using accumulated simulation time**

Do not bind economic calculations to frame tick.

- [ ] **Step 4: Run tests and commit**

```bash
git add Source/DominionSimulation Source/DominionTests
git commit -m "feat: add deterministic simulation clock"
```

---

### Task 8: Economy, staffing, construction cost, and maintenance formulas

**Files:**
- Create: `Source/DominionSimulation/Public/Economy/DAEconomySubsystem.h`
- Create: `Source/DominionSimulation/Private/Economy/DAEconomySubsystem.cpp`
- Create: `Source/DominionSimulation/Public/Economy/DAEconomyTypes.h`
- Test: `Source/DominionTests/Private/Simulation/EconomySpec.cpp`

**Interfaces:**
- Consumes: world-asset records, staffing, utilities, demand, condition.
- Produces:
  - `FDAFacilityOutput CalculateFacilityOutput(const FDAFacilityContext&) const`
  - `void ResolveDevelopmentCycle(FDACitySimulationState&)`
  - wallet values Capital/Insight/Influence.

- [ ] **Step 1: Encode v0.8 formula tests**

Verify:

```text
Facility Output = Base × Staffing × Utility × Demand × Condition × Modifier Stack
```

Tests:
- 100% staffing = 1.0
- 75–99% = 0.9
- 50–74% = 0.65
- critical utility = 0.25
- standard positive modifier cap = +60%
- standard negative floor = -60%

- [ ] **Step 2: Write maintenance test**

Industrial facility maintenance = approximately 1% of deployment cost per cycle before condition multiplier.

- [ ] **Step 3: Implement economy resolver with trace output**

`FDAFacilityOutput` stores a list of named contributions so UI can explain every number.

- [ ] **Step 4: Add 100-cycle boundedness test**

Balanced Synara starter block must remain solvent under expected first-hour tuning.

- [ ] **Step 5: Run and commit**

```bash
git add Source/DominionSimulation Source/DominionTests
git commit -m "feat: implement traceable city economy formulas"
```

---

### Task 9: Power, water, data, road, and adjacency graph services

**Files:**
- Create: `Source/DominionSimulation/Public/Networks/DAUtilityNetwork.h`
- Create: `Source/DominionSimulation/Private/Networks/DAUtilityNetwork.cpp`
- Create: `Source/DominionSimulation/Public/Networks/DARoadGraph.h`
- Create: `Source/DominionSimulation/Public/Adjacency/DAAdjacencySubsystem.h`
- Create: `Source/DominionSimulation/Private/Adjacency/DAAdjacencySubsystem.cpp`
- Test: `Source/DominionTests/Private/Simulation/UtilityGraphSpec.cpp`
- Test: `Source/DominionTests/Private/Simulation/AdjacencySpec.cpp`

**Interfaces:**
- Produces:
  - `FDAUtilityResolution ResolveUtility(EDAUtilityType, FDAWorldAssetId)`
  - `TArray<FDAAdjacencyModifier> RebuildForAsset(FDAWorldAssetId)`
  - `bool HasRoadAccess(FDAWorldAssetId) const`

- [ ] **Step 1: Write connectivity tests**

Microgrid capacity 25; two assets request 10 and 12 → fully supplied. Add third request 10 → deficit state.

- [ ] **Step 2: Write cached adjacency invalidation test**

Placement of a Research Annex next to Cognitive Operations Tower invalidates only affected spatial bucket and recalculates bonuses.

- [ ] **Step 3: Implement graph nodes/edges and spatial hash**

Do not iterate all buildings per frame.

- [ ] **Step 4: Add network-disruption test**

Disable Neural Relay and assert connected Synara assets lose Data state while unrelated districts remain unchanged.

- [ ] **Step 5: Commit**

```bash
git add Source/DominionSimulation Source/DominionTests
git commit -m "feat: add utility graphs roads and adjacency simulation"
```

---

### Task 10: Citizen cohorts, named citizens, homes, jobs, and migration

**Files:**
- Create: `Source/DominionSimulation/Public/Citizens/DACitizenRecord.h`
- Create: `Source/DominionSimulation/Public/Citizens/DAHouseholdRecord.h`
- Create: `Source/DominionSimulation/Public/Citizens/DAJobSystem.h`
- Create: `Source/DominionSimulation/Private/Citizens/DAJobSystem.cpp`
- Create: `Source/DominionSimulation/Public/Citizens/DAMigrationSystem.h`
- Create: `Source/DominionSimulation/Private/Citizens/DAMigrationSystem.cpp`
- Test: `Source/DominionTests/Private/Simulation/CitizenJobsSpec.cpp`
- Test: `Source/DominionTests/Private/Simulation/MigrationSpec.cpp`

**Interfaces:**
- Produces:
  - `FDACitizenRecord`
  - `FDAHouseholdRecord`
  - `FDAJobAssignment`
  - `UDAJobSystem::ResolveAssignments(FDACitySimulationState&)`
  - `UDAMigrationSystem::ResolveWorldTick(...)`

- [ ] **Step 1: Write workforce and job-match tests**

24 population → default workforce ~16.

Excellent match = 1.15 output; Good = 1.0; Acceptable = 0.85; Poor = 0.65.

- [ ] **Step 2: Write migration formula tests from v0.8**

For vacancy 20 and Attractiveness 70, expected incoming accumulation = 1.5 citizens/World Tick before rounding/accumulation.

- [ ] **Step 3: Implement citizen records and deterministic matching buckets**

Partition candidates by district, class/skill, and commute zone.

- [ ] **Step 4: Seed exactly 20 named citizen definitions**

Use stable IDs:
- `citizen.synara.nia_vale`
- `citizen.forgeweave.mara_kest`
- `citizen.eden.ori_sen`
- `citizen.neutral.tal_arden`
and the remaining names frozen in v1.1.

- [ ] **Step 5: Add streaming-identity test**

Promote a cohort member to named/full representation, unload region representation, reload, and assert same CitizenID/home/job/history.

- [ ] **Step 6: Commit**

```bash
git add Source/DominionSimulation Source/DominionTests Content/DA
git commit -m "feat: add persistent citizens jobs households and migration"
```

---

### Task 11: Synara factions, Dependency, and the Nia systemic hook

**Files:**
- Create: `Source/DominionSimulation/Public/Factions/DAFactionSystem.h`
- Create: `Source/DominionSimulation/Private/Factions/DAFactionSystem.cpp`
- Create: `Source/DominionSimulation/Public/Factions/DASystemicPressureSystem.h`
- Create: `Source/DominionSimulation/Private/Factions/DASystemicPressureSystem.cpp`
- Test: `Source/DominionTests/Private/Simulation/SynaraDependencySpec.cpp`

**Interfaces:**
- Produces:
  - faction support/organization/radicalization/grievance
  - `float CalculateDependency(const FDACitySimulationState&)`
  - event hooks when thresholds cross 25/50/70/85/100.

- [ ] **Step 1: Write Dependency source/mitigation tests**

Verify v0.8 values for:
- AI Worker,
- Autonomous Exchange,
- automated office/industry,
- Agency Forum,
- Education >70,
- human staffing ratio.

- [ ] **Step 2: Implement four Synara factions**

Stable IDs:
- `faction.synara.ascendants`
- `faction.synara.human_agency`
- `faction.synara.synthetic_rights`
- `faction.synara.moderates`

- [ ] **Step 3: Implement threshold events without directly launching quests**

Emit `FDASystemicPressureChanged`; narrative layer subscribes later.

- [ ] **Step 4: Run tests and commit**

```bash
git add Source/DominionSimulation Source/DominionTests Content/DA/Factions
git commit -m "feat: add Synara factions and dependency pressure"
```

---

### Task 12: Founder character, Enhanced Input, and seamless Founder/City/Command camera states

**Files:**
- Create: `Source/DominionGameplay/Public/Founder/DAFounderCharacter.h`
- Create: `Source/DominionGameplay/Private/Founder/DAFounderCharacter.cpp`
- Create: `Source/DominionGameplay/Public/Camera/DACameraModeController.h`
- Create: `Source/DominionGameplay/Private/Camera/DACameraModeController.cpp`
- Create: `Content/Input/IMC_Founder.uasset`
- Create: `Content/Input/IMC_City.uasset`
- Create: `Content/Input/IMC_Command.uasset`
- Test: `Source/DominionTests/Private/Gameplay/CameraModeSpec.cpp`

**Interfaces:**
- Produces:
  - `EDAPlayMode { Founder, City, Command }`
  - `RequestMode(EDAPlayMode)`
  - `OnModeChanged`
  - Founder movement/interact inputs.

- [ ] **Step 1: Write state-transition test**

Founder → City → Command → Founder preserves same pawn/world object references and does not load a different level.

- [ ] **Step 2: Implement mode controller**

Each state sets:
- camera parameters,
- input mapping context,
- selection rules,
- requested simulation slowdown.

- [ ] **Step 3: Implement Founder baseline movement**

Required:
- move
- sprint
- dodge
- interact
- camera
- traversal hook

- [ ] **Step 4: Add transition-duration instrumentation**

Record transition duration; automated functional test fails if transition exceeds configured 1.5-second budget in benchmark map under test conditions.

- [ ] **Step 5: Commit**

```bash
git add Source/DominionGameplay Content/Input Source/DominionTests
git commit -m "feat: add founder controls and seamless play modes"
```

---

### Task 13: Gameplay Ability System combat foundation

**Files:**
- Create: `Source/DominionGameplay/Public/Combat/DAAbilitySystemComponent.h`
- Create: `Source/DominionGameplay/Public/Combat/DACombatAttributeSet.h`
- Create: `Source/DominionGameplay/Private/Combat/DACombatAttributeSet.cpp`
- Create: `Source/DominionGameplay/Public/Combat/DADamageTypes.h`
- Create: `Source/DominionGameplay/Public/Combat/DAStatusTags.h`
- Test: `Source/DominionTests/Private/Gameplay/CombatAttributesSpec.cpp`

**Interfaces:**
- Produces:
  - Health, Guard, Stamina, TacticalCharge attributes.
  - damage channels: Kinetic, Thermal, Arc, Bio, Cyber, Structural.
  - status tags: Suppressed, Shielded, Burning, Disrupted, Hacked, Regenerating, Inspired, Routed, Immobilized, Marked.

- [ ] **Step 1: Write damage-channel test**

Apply Arc damage to a shielded synthetic target; assert Guard resolves before Health and configured resistance modifies final damage.

- [ ] **Step 2: Implement attribute set and GameplayEffect helpers**

Keep damage calculation authoritative in C++ execution calculation.

- [ ] **Step 3: Create Founder baseline abilities**

Data-driven definitions:
- Precision Scan
- Drone Barrier
- Orchestration Mark
- Coordinated Override

- [ ] **Step 4: Add Downed-state test**

At 0 Health:
- Downed tag applied,
- 20-second recovery window begins,
- no immediate permanent death.

- [ ] **Step 5: Commit**

```bash
git add Source/DominionGameplay Source/DominionTests Content/DA/Abilities
git commit -m "feat: add founder combat attributes and abilities"
```

---

### Task 14: Squads, morale, cover, Command Points, and tactical orders

**Files:**
- Create: `Source/DominionGameplay/Public/Units/DASquadEntity.h`
- Create: `Source/DominionGameplay/Private/Units/DASquadEntity.cpp`
- Create: `Source/DominionGameplay/Public/Command/DACommandSubsystem.h`
- Create: `Source/DominionGameplay/Private/Command/DACommandSubsystem.cpp`
- Create: `Source/DominionGameplay/Public/Combat/DACoverSubsystem.h`
- Create: `Source/DominionGameplay/Private/Combat/DACoverSubsystem.cpp`
- Test: `Source/DominionTests/Private/Gameplay/CommandModeSpec.cpp`

**Interfaces:**
- Produces:
  - `EDACommandOrder { Move, Attack, Hold, Defend, Capture, Escort, Retreat, Ability }`
  - max 3 directly controlled squads + 1 vehicle.
  - morale 0–100 and required morale states.
  - CP max 6 by baseline.

- [ ] **Step 1: Write command-cap test**

Fourth squad cannot enter direct selection while three are assigned; it remains AI-doctrine controlled.

- [ ] **Step 2: Write morale transition test**

Test 100→74→49→24→0 produces Inspired/Steady/Shaken/Breaking/Rout.

- [ ] **Step 3: Implement squad tactical controller**

Controller owns:
- formation,
- destination,
- target,
- morale,
- suppression,
- supply,
- active order.

- [ ] **Step 4: Implement authored cover sockets plus ruin/deployable registration**

Cover types:
- Partial
- Full
- Hardened
- Destructible.

- [ ] **Step 5: Implement CP earning/spending**

Baseline max 6. Control Zones can award CP; tactical abilities spend it.

- [ ] **Step 6: Commit**

```bash
git add Source/DominionGameplay Source/DominionTests
git commit -m "feat: add squads command points morale and cover"
```

---

### Task 15: Structural damage, eight modular structures, capture, surrender, and ruins

**Files:**
- Create: `Source/DominionGameplay/Public/Damage/DAStructuralDamageComponent.h`
- Create: `Source/DominionGameplay/Private/Damage/DAStructuralDamageComponent.cpp`
- Create: `Source/DominionGameplay/Public/Capture/DACaptureComponent.h`
- Create: `Source/DominionGameplay/Private/Capture/DACaptureComponent.cpp`
- Test: `Source/DominionTests/Private/Gameplay/StructuralDamageSpec.cpp`
- Test: `Source/DominionTests/Private/Gameplay/CaptureSpec.cpp`

**Interfaces:**
- Produces:
  - module health records,
  - Operational/Damaged/Disabled/Ruined transitions,
  - `CanCapture()`,
  - Preserve/Convert/Study/Salvage/Gift outcome.

- [ ] **Step 1: Write module-state tests**

For Synthetic Fabrication Node:
- destroy Control Center → production disabled but structure not Ruined;
- reduce total Structural Integrity to 0 → Ruined.

- [ ] **Step 2: Implement module schema**

Full modular destruction only for:
1. Synthetic Fabrication Node
2. Swarm Foundry
3. Infinite Foundry
4. Replication Forge
5. Smog Reclaimer
6. Freight Furnace
7. Grand Forge
8. Autonomous Factory

- [ ] **Step 3: Write capture test**

Disabled Infinite Foundry at >=10% integrity + uncontested Engineer/Founder capture interaction → capture completes after base 20 seconds.

- [ ] **Step 4: Implement capture outcome mutation**

Preserve keeps original definition and changes owner. Convert creates a sanctioned conversion operation; Study/Salvage remove operational use and generate correct rewards.

- [ ] **Step 5: Add surrender test**

Breaking/Routed Forge Guard with low Military Sovereignty can surrender; accepting surrender writes history and changes post-conflict values.

- [ ] **Step 6: Commit**

```bash
git add Source/DominionGameplay Source/DominionTests
git commit -m "feat: add persistent structural damage capture and surrender"
```

---

### Task 16: Regional state, World Map, travel, trade, and diplomacy

**Files:**
- Create: `Source/DominionWorld/Public/Regions/DARegionState.h`
- Create: `Source/DominionWorld/Public/Regions/DAWorldStateSubsystem.h`
- Create: `Source/DominionWorld/Private/Regions/DAWorldStateSubsystem.cpp`
- Create: `Source/DominionWorld/Public/Trade/DATradeSystem.h`
- Create: `Source/DominionWorld/Private/Trade/DATradeSystem.cpp`
- Create: `Source/DominionWorld/Public/Diplomacy/DADiplomacySystem.h`
- Create: `Source/DominionWorld/Private/Diplomacy/DADiplomacySystem.cpp`
- Test: `Source/DominionTests/Private/World/TradeDiplomacySpec.cpp`

**Interfaces:**
- Produces:
  - region persistent state,
  - travel API,
  - `FDADiplomaticRelationship { Trust, Respect, Fear, Dependence, Grievance, Compatibility }`,
  - real multi-tick trade contracts.

- [ ] **Step 1: Seed exact vertical-slice regions**

Stable IDs:
- `region.synara_frontier`
- `region.ironheart`
- `region.eden_basin`
- `settlement.arden_reservoir`
- `settlement.ore_station_7`
- `settlement.river_crossing`

- [ ] **Step 2: Write trade fulfillment test**

A 5-World-Tick Forgeweave machine-component contract updates actual deliveries and relationship state only when route capacity and supply succeed.

- [ ] **Step 3: Implement relationship reason ledger**

Every relationship mutation stores:
- source tag,
- magnitude,
- World Tick.

UI can explain the aggregate score.

- [ ] **Step 4: Implement travel state handoff**

Before region unload:
- snapshot persistent delta;
- advance travel time;
- load destination;
- reconstruct local actors from region state.

- [ ] **Step 5: Commit**

```bash
git add Source/DominionWorld Source/DominionTests Content/DA/Regions
git commit -m "feat: add regional travel trade and diplomacy"
```

---

### Task 17: Forgeweave rival AI and Resource Hunger

**Files:**
- Create: `Source/DominionWorld/Public/AI/DARivalCityPlanner.h`
- Create: `Source/DominionWorld/Private/AI/DARivalCityPlanner.cpp`
- Create: `Source/DominionWorld/Public/AI/DAForgeweaveStrategy.h`
- Create: `Source/DominionWorld/Private/AI/DAForgeweaveStrategy.cpp`
- Test: `Source/DominionTests/Private/World/ForgeweaveAISpec.cpp`
- Test: `Source/DominionTests/Private/World/ForgeweaveSoakSpec.cpp`

**Interfaces:**
- Consumes: World Tick, economy state, six-card Forgeweave build pool.
- Produces: valid Ironheart construction/repair/trade/defense decisions.

- [ ] **Step 1: Write six-card build-pool test**

Planner may choose only:
- Worker Arcology
- Production Directorate
- Industrial Exchange
- Infinite Foundry
- Freight Furnace
- Smog Reclaimer

during vertical slice.

- [ ] **Step 2: Implement scoring priorities**

Score candidates by:
- housing shortage,
- output shortage,
- Resource Hunger mitigation,
- defense pressure,
- affordability,
- placement validity.

- [ ] **Step 3: Implement Resource Hunger**

Growth based on:
- active industrial throughput,
- material scarcity,
- Overdrive;
mitigation based on:
- logistics,
- recycling,
- Eden inputs,
- production reduction.

- [ ] **Step 4: Run 100-World-Tick soak**

Fail if:
- invalid placement,
- impossible utility state,
- negative impossible economy,
- planner deadlock for >10 ticks without crisis explanation.

- [ ] **Step 5: Commit**

```bash
git add Source/DominionWorld Source/DominionTests
git commit -m "feat: add Forgeweave rival city planning and resource hunger"
```

---

### Task 18: Quest/event state-graph runtime, history tags, and promises

**Files:**
- Create: `Source/DominionWorld/Public/Narrative/DAQuestDefinition.h`
- Create: `Source/DominionWorld/Public/Narrative/DAQuestRuntime.h`
- Create: `Source/DominionWorld/Private/Narrative/DAQuestRuntime.cpp`
- Create: `Source/DominionWorld/Public/Narrative/DAWorldEventDefinition.h`
- Create: `Source/DominionWorld/Public/Narrative/DAHistoryLedger.h`
- Create: `Source/DominionWorld/Public/Narrative/DAPromiseLedger.h`
- Test: `Source/DominionTests/Private/World/QuestGraphSpec.cpp`
- Test: `Source/DominionTests/Private/World/PromiseSpec.cpp`

**Interfaces:**
- Produces:
  - graph nodes from v1.0 taxonomy,
  - quest save state,
  - event save state,
  - history tags,
  - promise conflicts.

- [ ] **Step 1: Write graph reachability validator test**

A definition containing an unreachable resolution node must fail validation.

- [ ] **Step 2: Implement quest nodes as data-driven tagged variants**

Required nodes:
Dialogue, Objective, Investigation, Build, Deliver, Explore, Combat, Defend, Capture, Choice, Wait, Timer, WorldCondition, CitizenCondition, FactionCondition, EconomyCondition, RelationshipCondition, EventTrigger, Reward, Failure, Resolution.

- [ ] **Step 3: Write dynamic asset-loss test**

Quest references an actual tower WorldAssetID; destroy it mid-quest; runtime selects `ReconstructOrReplace` branch rather than spawning duplicate.

- [ ] **Step 4: Implement promise ledger**

Before an action carrying a conflicting history tag commits, expose:
`GetBrokenPromises(ActionTags) -> TArray<FDAPromiseRecord>`.

- [ ] **Step 5: Commit**

```bash
git add Source/DominionWorld Source/DominionTests
git commit -m "feat: add persistent quest event history and promise runtime"
```

---

### Task 19: Author and validate the 64 content definitions and exact starter deck

**Files:**
- Create: `Content/DA/Cards/Synara/*`
- Create: `Content/DA/Cards/Forgeweave/*`
- Create: `Content/DA/Cards/Eden/*`
- Create: `Content/DA/Cards/Universal/*`
- Create: `Content/DA/Cards/Fusion/DA_Card_AutonomousFactory.uasset`
- Create: `Content/DA/Buildings/DA_FounderHall.uasset`
- Create: `Content/DA/Decks/DA_Deck_SynaraStarter60.uasset`
- Test: `Source/DominionTests/Private/Content/VerticalSliceContentSpec.cpp`

**Interfaces:**
- Consumes: definition schemas and registry.
- Produces: exactly 64 definitions and one valid 60-instance starter collection/deck factory.

- [ ] **Step 1: Write exact-count content test**

Assert:
- 15 Synara
- 15 Forgeweave
- 15 Eden
- 17 Universal
- 1 Fusion
- 1 Founder Hall
= 64.

- [ ] **Step 2: Write exact starter-deck composition test**

Assert every quantity from v1.1 Section 22.

- [ ] **Step 3: Author Data Assets with gameplay values from v0.8**

Use temporary graybox prefab references where final art is not yet complete, but every placeable definition must resolve to a valid prefab class.

- [ ] **Step 4: Run registry validation**

Zero:
- duplicate IDs,
- missing prefabs,
- invalid costs,
- copy-limit violations,
- Dominion-cache violations.

- [ ] **Step 5: Commit**

```bash
git add Content/DA Source/DominionTests
git commit -m "content: author frozen vertical slice card definitions"
```

---

### Task 20: First-hour onboarding quests and Nia Vale arc

**Files:**
- Create: `Content/DA/Quests/Q_WakeTheHall.uasset`
- Create: `Content/DA/Quests/Q_APlaceToStay.uasset`
- Create: `Content/DA/Quests/Q_PowerWaterPeople.uasset`
- Create: `Content/DA/Quests/Q_NiaNeedsAJob.uasset`
- Create: `Content/DA/Quests/Q_ReplacementModel.uasset`
- Create: `Content/DA/Quests/Q_AgencyHasAPrice.uasset`
- Create: `Content/DA/Quests/Q_SignalInFoundation.uasset`
- Create: `Content/DA/Quests/Q_IronAtBorder.uasset`
- Create: `Content/DA/Quests/Q_BasinSpeaks.uasset`
- Create: `Content/DA/Citizens/DA_Citizen_NiaVale.uasset`
- Test: `Source/DominionTests/Private/Content/FirstHourQuestSpec.cpp`

**Interfaces:**
- Consumes: quest runtime, real city assets, citizens, factions, diplomacy.
- Produces: first-hour sequence and Nia state machine.

- [ ] **Step 1: Author Wake the Hall through Nia Needs a Job**

Every objective must bind to actual:
- Founder Hall,
- Adaptive Habitat,
- utilities,
- Nia CitizenID,
- Cognitive Operations Tower.

- [ ] **Step 2: Write first-hour state progression test**

Programmatically advance each objective and assert:
- no quest starts before prerequisite,
- required unlocks occur exactly once.

- [ ] **Step 3: Author Replacement Model branch states**

Outcomes:
- accept,
- modify,
- audit,
- reject.

Each writes persistent relationship/faction/Dependency effects.

- [ ] **Step 4: Author Human Override eligibility hook**

Champion eligibility requires the correct Nia story state and later crisis completion.

- [ ] **Step 5: Commit**

```bash
git add Content/DA/Quests Content/DA/Citizens Source/DominionTests
git commit -m "content: implement first-hour onboarding and Nia arc"
```

---

### Task 21: World Map UI, 27 UI surfaces, controller parity, and accessibility baseline

**Files:**
- Create: `Source/DominionUI/Public/Navigation/DAUIScreenRouter.h`
- Create: `Source/DominionUI/Private/Navigation/DAUIScreenRouter.cpp`
- Create: `Content/UI/Screens/*`
- Create: `Content/UI/HUD/*`
- Create: `Content/UI/Overlays/*`
- Create: `Content/UI/Input/*`
- Test: `Source/DominionTests/Private/UI/UIScreenCoverageSpec.cpp`
- Test: `Source/DominionTests/Private/UI/ControllerNavigationSpec.cpp`

**Interfaces:**
- Consumes: runtime view-model data from all modules.
- Produces: 27 frozen surfaces; 8 required overlays; keyboard/controller navigation.

- [ ] **Step 1: Write required-surface registry test**

Assert all 27 screen IDs from v1.1 are registered and loadable.

- [ ] **Step 2: Implement CommonUI router**

Define stable screen IDs and input contexts; no screen directly manipulates simulation state without a command/service call.

- [ ] **Step 3: Implement City/Founder/Command HUDs first**

Bind:
- wallets,
- population,
- Dependency,
- hand,
- squads,
- CP,
- Supply,
- objective.

- [ ] **Step 4: Implement collection/deck/building/citizen/faction/research screens**

Use advanced tooltip trace data from simulation rather than duplicating formulas.

- [ ] **Step 5: Implement World Map/Diplomacy/Treaty/Conquest screens**

Relationship and conquest values must expose reason ledgers.

- [ ] **Step 6: Implement accessibility requirements**

At minimum all frozen v1.1 options:
text scale, subtitles, speaker labels, color-independent markers, color-vision presets, camera-shake slider, FOV, motion blur, reduced flash, Tactical Pause, hold/toggle, aim assist, build-snap, tutorial recall, tooltip modes.

- [ ] **Step 7: Run controller-navigation automated smoke test**

Every required screen must be enterable/exitable and every campaign-critical action reachable without hover.

- [ ] **Step 8: Commit**

```bash
git add Source/DominionUI Content/UI Source/DominionTests
git commit -m "feat: implement vertical slice UI navigation and accessibility"
```

---

### Task 22: Six systemic events and the regional Foundry Shortage campaign

**Files:**
- Create: `Content/DA/Events/E_FoundryShortage.uasset`
- Create: `Content/DA/Events/E_GridStrain.uasset`
- Create: `Content/DA/Events/E_HousingSurge.uasset`
- Create: `Content/DA/Events/E_GreenLine.uasset`
- Create: `Content/DA/Events/E_CorridorFailure.uasset`
- Create: `Content/DA/Events/E_MigrationWave.uasset`
- Create: `Content/DA/Quests/Q_TalsReservoir.uasset`
- Create: `Content/DA/Quests/Q_FrontierClaim.uasset`
- Create: `Content/DA/Quests/Q_HoldTheRidge.uasset`
- Create: `Content/DA/Quests/Q_FirstContract.uasset`
- Create: `Content/DA/Quests/Q_MarasNumbers.uasset`
- Create: `Content/DA/Quests/Q_GreenLine.uasset`
- Create: `Content/DA/Quests/Q_EmptyShift.uasset`
- Create: `Content/DA/Quests/Q_PriceOfSilence.uasset`
- Create: `Content/DA/Quests/Q_HumanOverride.uasset`
- Create: `Content/DA/Quests/Q_FoundryShortage.uasset`
- Test: `Source/DominionTests/Private/Content/FoundryShortageSpec.cpp`

**Interfaces:**
- Consumes: market, trade, Resource Hunger, Ecology, relationships, quest runtime.
- Produces: regional crisis with at least four resolution states.

- [ ] **Step 1: Write Foundry Shortage trigger test**

Resource Hunger >70 triggers warning once; price starts at +20%, can escalate to +35% and +60%.

- [ ] **Step 2: Implement event stages using real market modifiers**

Do not directly credit/debit fake quest currency.

- [ ] **Step 3: Author Tal/Mara/Ori regional quests against persistent citizens**

Their outcome tags must change later Daxton/Amara dialogue.

- [ ] **Step 4: Test ignored-event resolution**

Advance World Ticks without player intervention; ensure crisis can escalate/resolution state persists without broken quest assumptions.

- [ ] **Step 5: Commit**

```bash
git add Content/DA/Events Content/DA/Quests Source/DominionTests
git commit -m "content: implement systemic regional crisis campaign"
```

---

### Task 23: Conquest meters and the four Forgeweave routes

**Files:**
- Create: `Source/DominionWorld/Public/Conquest/DAConquestState.h`
- Create: `Source/DominionWorld/Public/Conquest/DAConquestSystem.h`
- Create: `Source/DominionWorld/Private/Conquest/DAConquestSystem.cpp`
- Create: `Content/DA/Quests/Q_BrokerOfIronheart.uasset`
- Create: `Content/DA/Quests/Q_WorkersSignal.uasset`
- Create: `Content/DA/Quests/Q_SupplyNoose.uasset`
- Create: `Content/DA/Quests/Q_OperationIronVeil.uasset`
- Create: `Content/DA/Quests/Q_ThirdFoundry.uasset`
- Test: `Source/DominionTests/Private/World/ConquestRoutesSpec.cpp`

**Interfaces:**
- Produces:
  - Military Sovereignty
  - Economic Autonomy
  - Civic Legitimacy
  - Alliance Readiness
  - route-weight history.

- [ ] **Step 1: Write meter mutation tests**

No action may remove >15 Economic Autonomy by default. Stored Influence alone cannot reduce Civic Legitimacy to zero.

- [ ] **Step 2: Implement Force route hooks**

Real:
- Control Zones,
- captured military assets,
- elite unit defeat,
- city shield/command effects.

- [ ] **Step 3: Implement Economic route hooks**

Require:
- actual fulfilled trade,
- freight share,
- component dependence,
- emergency finance/contract state.

- [ ] **Step 4: Implement Influence route hooks**

Require:
- worker endorsement,
- real service-crisis solution,
- credibility history,
- faction support.

- [ ] **Step 5: Implement Alliance Readiness**

Components:
- Trust
- Shared Interest
- Crisis Resolution
- Respect.

Union eligibility:
- average >=80,
- no component <65,
- no Major Grievance,
- Third Foundry complete.

- [ ] **Step 6: Run four deterministic completion tests**

Create four independent campaign fixtures and complete Forgeweave via each route.

- [ ] **Step 7: Commit**

```bash
git add Source/DominionWorld Content/DA/Quests Source/DominionTests
git commit -m "feat: implement four systemic Forgeweave conquest routes"
```

---

### Task 24: Daxton Rhe three-phase final encounter and Leader outcomes

**Files:**
- Create: `Source/DominionGameplay/Public/Boss/Daxton/DADaxtonEncounter.h`
- Create: `Source/DominionGameplay/Private/Boss/Daxton/DADaxtonEncounter.cpp`
- Create: `Content/DA/Leaders/DA_Leader_DaxtonRhe.uasset`
- Create: `Content/Characters/Daxton/*`
- Test: `Source/DominionTests/Private/Gameplay/DaxtonEncounterSpec.cpp`

**Interfaces:**
- Consumes: relationship, conquest route, Grand Forge modules, worker state.
- Produces: Governor/Advisor/Allied/Exile/Prisoner/Dead persistent Leader state.

- [ ] **Step 1: Write phase-transition tests**

Phase I completes at encounter 60% equivalent state.
Phase II activates Overdrive, Heat, Coolant, Resource Hunger.
Phase III exposes choice objectives.

- [ ] **Step 2: Implement Phase I mechanics**

- Powered armor
- cover deployment
- Forge Guard reinforcement
- Grand Forge production loop.

- [ ] **Step 3: Implement Phase II systemic interactions**

Player can:
- damage,
- disable coolant,
- redirect supply,
- hack production,
- trigger worker shutdown.

Every method modifies the same encounter state.

- [ ] **Step 4: Implement Phase III outcomes**

At least:
- defeat Daxton,
- save Grand Forge,
- evacuate workers,
- stabilize/offer union.

- [ ] **Step 5: Write Leader-resolution matrix tests**

Given Trust/Respect/Grievance and route history, assert allowed outcomes.

- [ ] **Step 6: Commit**

```bash
git add Source/DominionGameplay Content/DA/Leaders Content/Characters/Daxton Source/DominionTests
git commit -m "feat: implement Daxton systemic leader encounter"
```

---

### Task 25: First Ascension, Replication, Forgeweave unlocks, and Autonomous Factory

**Files:**
- Create: `Source/DominionWorld/Public/Ascension/DAAscensionSystem.h`
- Create: `Source/DominionWorld/Private/Ascension/DAAscensionSystem.cpp`
- Create: `Content/DA/Quests/Q_ConvergenceAuthority.uasset`
- Create: `Content/DA/Doctrines/DA_Doctrine_Replication.uasset`
- Create: `Content/DA/Cards/Fusion/DA_Card_AutonomousFactory.uasset`
- Create: `Content/Buildings/Fusion/AutonomousFactory/*`
- Create: `Content/Cinematics/CS_ForgeweaveAscension.uasset`
- Test: `Source/DominionTests/Private/World/AscensionSpec.cpp`

**Interfaces:**
- Consumes: completed conquest state.
- Produces:
  - Forge Relic state,
  - Replication doctrine,
  - Forgeweave card-pool visibility,
  - Leader resolution,
  - Fusion eligibility,
  - Founder Hall visual state 3,
  - `first_relic_acquired`,
  - `CONVERGENCE AUTHORITY: 1/20`.

- [ ] **Step 1: Write atomic Ascension-state test**

One transaction must:
- mark Forgeweave resolved,
- add Relic,
- unlock card pool,
- unlock Replication,
- persist Leader state,
- unlock Fusion,
- increase Influence/Insight by v0.8 base reward,
- write history.

If any step fails, transaction rolls back.

- [ ] **Step 2: Implement Replication eligibility**

Once every 12 Development Cycles:
- eligible non-Legendary Asset Card;
- create a new Card Instance with `AcquiredBy=Replication`;
- provenance references source instance.

- [ ] **Step 3: Implement Autonomous Factory systemic effects**

Assert:
- -80% workforce,
- +25% throughput,
- +15% adjacent industrial construction speed,
- Dependency +0.20/cycle,
- Resource Hunger +0.15/cycle.

- [ ] **Step 4: Implement Founder Hall state transition and cinematic hooks**

Presentation reads state; gameplay unlock transaction is authoritative before cinematic plays.

- [ ] **Step 5: Write post-Ascension save/reload test**

Reload and assert:
- Relic present,
- Forgeweave cards accessible,
- doctrine unlocked,
- Daxton state preserved,
- Autonomous Factory blueprint available,
- Founder Hall state 3,
- 1/20 history flag.

- [ ] **Step 6: Commit**

```bash
git add Source/DominionWorld Content/DA Content/Buildings/Fusion Content/Cinematics Source/DominionTests
git commit -m "feat: implement first Forgeweave Ascension"
```

---

### Task 26: Art/VFX/audio integration gates for the frozen vertical slice

**Files:**
- Modify: `Content/Buildings/*`
- Modify: `Content/Characters/*`
- Modify: `Content/VFX/*`
- Modify: `Content/Audio/*`
- Create: `Source/DominionTests/Private/Content/PresentationCoverageSpec.cpp`

**Interfaces:**
- Consumes: gameplay event hooks.
- Produces: presentation coverage for required assets without moving gameplay logic into presentation Blueprints.

- [ ] **Step 1: Write coverage manifest test**

Assert:
- 50 primary v0.6 assets resolve,
- 25 core VFX IDs resolve,
- 9 music cues resolve,
- 12 ambient loops resolve,
- >=60 SFX events resolve.

- [ ] **Step 2: Bind construction stages to faction presentation**

Synara, Forgeweave, Eden must each have visibly distinct construction hooks.

- [ ] **Step 3: Bind damage and capture state presentation**

Ensure ownership changes use signage/integration stages rather than instant recolor.

- [ ] **Step 4: Integrate Daxton and Ascension presentation**

Gameplay outcome must remain valid with cinematics skipped.

- [ ] **Step 5: Run content coverage tests and commit**

```bash
git add Content Source/DominionTests
git commit -m "content: integrate vertical slice presentation coverage"
```

---

### Task 27: Performance, simulation LOD, and 1,000-cycle soak

**Files:**
- Create: `Source/DominionSimulation/Public/LOD/DASimulationLODSubsystem.h`
- Create: `Source/DominionSimulation/Private/LOD/DASimulationLODSubsystem.cpp`
- Create: `Source/DominionTests/Private/Performance/SimulationSoakSpec.cpp`
- Create: `Source/DominionTests/Private/Performance/BenchmarkFunctionalTest.cpp`
- Create: `Config/DefaultScalability.ini`

**Interfaces:**
- Produces: Sim LOD 0/1/2/3 and benchmark instrumentation.

- [ ] **Step 1: Implement the four simulation LODs**

- LOD0 full,
- LOD1 detailed,
- LOD2 aggregated,
- LOD3 strategic.

Named CitizenID remains stable across all.

- [ ] **Step 2: Write 1,000 Development Cycle headless soak**

Enabled:
- player economy,
- population,
- Forgeweave AI,
- trade,
- six events.

Assert no:
- infinite money,
- negative impossible population,
- duplicate WorldAsset IDs,
- unresolved invalid utility graph,
- event spam runaway,
- stuck required quest.

- [ ] **Step 3: Build benchmark scene**

50 player gameplay assets, 120 visible/represented citizens, weather, 3 allied squads, 3 enemy squads, 1 vehicle, construction VFX, one damaged building.

- [ ] **Step 4: Capture performance metrics**

Target:
- 60 FPS at 1080p High on reference-class hardware,
- 30 FPS fallback worst-case,
- no severe Development Cycle hitch,
- mode transition <1.5 sec.

- [ ] **Step 5: Optimize only measured bottlenecks**

Prefer:
- cached graph calculations,
- Mass/crowd LOD,
- HLOD,
- async planning,
- event-driven recompute.

Do not remove core systemic behavior to mask avoidable implementation costs.

- [ ] **Step 6: Commit**

```bash
git add Source/DominionSimulation Source/DominionTests Config
git commit -m "perf: add simulation LOD and vertical slice soak benchmark"
```

---

### Task 28: Full regression, four-route campaign fixtures, and release candidate gate

**Files:**
- Create: `Source/DominionTests/Private/Release/VerticalSliceReleaseSpec.cpp`
- Create: `Content/Test/Fixtures/ForceRoute/*`
- Create: `Content/Test/Fixtures/EconomicRoute/*`
- Create: `Content/Test/Fixtures/InfluenceRoute/*`
- Create: `Content/Test/Fixtures/AllianceRoute/*`
- Create: `docs/qa/vertical-slice-release-checklist.md`

**Interfaces:**
- Consumes: every previous task.
- Produces: release-candidate evidence against v1.1 acceptance criteria.

- [ ] **Step 1: Create deterministic fixtures immediately before final route commitment**

Each fixture must contain the minimum legitimate systemic state needed to complete one route, not debug cheats that bypass requirements.

- [ ] **Step 2: Run all four route completions**

Verify each reaches first Ascension and produces different:
- Loyalty,
- Daxton state,
- history,
- damage,
- relationship aftermath.

- [ ] **Step 3: Run save/load at critical checkpoints**

Required:
- first city built,
- mid-Nia quest,
- active Foundry Shortage,
- mid-Iron Veil,
- Daxton phase transition,
- immediately after Ascension.

- [ ] **Step 4: Run controller-only campaign-critical smoke**

No mouse required for:
- build,
- card inspect,
- combat,
- Command,
- diplomacy,
- conquest,
- save/load.

- [ ] **Step 5: Run onboarding observation protocol**

Use first-time testers with no developer coaching. Record whether each v1.1 first-hour acceptance action succeeds.

- [ ] **Step 6: Verify bug gate**

Required for signoff:
- Blocker = 0
- Critical = 0
- High = 0
- Medium = explicitly accepted only
- Low = within polish tolerance

- [ ] **Step 7: Produce release evidence**

`docs/qa/vertical-slice-release-checklist.md` must link:
- automation results,
- soak results,
- performance capture,
- four-route videos/logs,
- save regression results,
- accessibility/controller checklist,
- known accepted Medium/Low issues.

- [ ] **Step 8: Commit release-candidate evidence**

```bash
git add Source/DominionTests Content/Test docs/qa
git commit -m "test: complete vertical slice release candidate gate"
```

---

# Execution Milestones

## Milestone 1 — Card Becomes City
Tasks 1–9.

**Demonstration:** Adaptive Habitat can be owned, drawn, placed, constructed, powered, staffed, simulated, saved, and restored.

## Milestone 2 — City Becomes Society
Tasks 10–11 + first-hour portion of Task 20.

**Demonstration:** A living Synara block with Nia, jobs, utilities, Capital, Insight, factions, and Dependency.

## Milestone 3 — Founder Lives in the City
Tasks 12–15.

**Demonstration:** Founder and squads fight inside the same persistent built environment; buildings can be damaged/captured and remain changed.

## Milestone 4 — World Exists Beyond the Player
Tasks 16–18 + Task 22.

**Demonstration:** Forgeweave evolves while unloaded, Eden reacts, trade matters, Foundry Shortage changes systems.

## Milestone 5 — Conquest Is Systemic
Tasks 23–24.

**Demonstration:** the same Ironheart can be resolved through Force, Economic, Influence, or Alliance play.

## Milestone 6 — Ascension Proves the Franchise Loop
Task 25.

**Demonstration:** Forgeweave becomes part of the player's collection, city, doctrine set, Leader history, and architecture.

## Milestone 7 — Shippable Vertical Slice
Tasks 21, 26–28 finalized.

**Demonstration:** performance, UI, art, audio, accessibility, save integrity, and all route regressions meet v1.1 gates.

---

# Continuous Integration Commands

Create CI jobs that run in this order.

```powershell
# Compile editor + tests
Engine\Build\BatchFiles\Build.bat DominionAscendantEditor Win64 Development DominionAscendant.uproject -WaitMutex

# Core/Simulation unit tests
UnrealEditor-Cmd.exe DominionAscendant.uproject -ExecCmds="Automation RunTests Dominion.Core;Automation RunTests Dominion.Simulation;Quit" -unattended -nop4

# Gameplay/World integration tests
UnrealEditor-Cmd.exe DominionAscendant.uproject -ExecCmds="Automation RunTests Dominion.Gameplay;Automation RunTests Dominion.World;Quit" -unattended -nop4

# Content validation
UnrealEditor-Cmd.exe DominionAscendant.uproject -ExecCmds="Automation RunTests Dominion.Content;Quit" -unattended -nop4

# UI/controller smoke
UnrealEditor-Cmd.exe DominionAscendant.uproject -ExecCmds="Automation RunTests Dominion.UI;Quit" -unattended -nop4

# Release suite on candidate branches
UnrealEditor-Cmd.exe DominionAscendant.uproject -ExecCmds="Automation RunTests Dominion.Release;Quit" -unattended -nop4
```

A failed content-validation or save-regression test blocks merge.

---

# Branch and Review Discipline

Use one branch/worktree per independently reviewable task or tightly coupled task group.

Naming examples:

```text
feat/core-content-registry
feat/card-lifecycle
feat/city-grid
feat/citizen-sim
feat/foundry-shortage
feat/forgeweave-conquest
feat/first-ascension
```

Each review must answer:

1. Does this task meet its interfaces?
2. Do its tests fail before implementation and pass after?
3. Does it introduce an opposite-direction module dependency?
4. Does it duplicate authoritative state?
5. Does it add content outside v1.1 scope?
6. Does save/load preserve the new state?
7. Can presentation be skipped without breaking gameplay?

---

# Scope-Control Checklist

Before accepting any new work item:

- [ ] It is required by a v1.1 acceptance criterion.
- [ ] If it is not required, an existing scoped item is explicitly removed to pay for it.
- [ ] It does not introduce a fourth major civilization.
- [ ] It does not require multiplayer architecture beyond stable deterministic boundaries already planned.
- [ ] It does not increase the 64-definition frozen content count without a spec revision.
- [ ] It does not expand full modular destruction beyond the eight frozen structures.
- [ ] It does not expand the 27 UI-surface set without removing/replacing another surface.
- [ ] It does not turn a systemic quest into a disposable mission world.

---

# Definition of Engineering Done

Engineering is complete for v1.1 only when:

- all 28 tasks are implemented;
- all automated validation suites pass;
- all four conquest routes complete;
- first Ascension survives save/load;
- 1,000-cycle soak passes;
- reference performance gate passes;
- controller-only critical path passes;
- first-hour onboarding passes without coaching;
- no Blocker/Critical/High defects remain;
- the release checklist contains evidence for every v1.1 acceptance criterion.

---

# Implementation North Star

> **Never implement a “card system,” “city system,” “RPG system,” or “combat system” that owns a second copy of the truth.**

The same persistent entities must flow through every mode.

**Adaptive Habitat is the first proof. Forgeweave Ascension is the final proof. Everything between them exists to demonstrate that they are part of one game.**
