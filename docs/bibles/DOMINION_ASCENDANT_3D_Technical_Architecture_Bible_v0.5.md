# DOMINION // ASCENDANT
## 3D Production & Technical Architecture Bible — v0.5

**Status:** Canonical technical design pass following:

1. Game Bible v0.1
2. Playable Systems Bible v0.2
3. Combat, Siege & Conquest Bible v0.3
4. Citizen, Card Acquisition & World Campaign Bible v0.4

> **One definition. Many representations. One persistent world.**

---

# 1. PURPOSE OF THIS DOCUMENT

This document converts the existing game design into a production architecture capable of supporting:

- Trading-card collection and deckbuilding
- Persistent 3D city construction
- Third-person Founder gameplay
- Tactical Command Mode
- Large-scale citizen simulation
- Rival civilization simulation
- Destructible and capturable structures
- Seamless city/Founder/Command transitions
- Dynamic campaign-state changes
- Procedural rival-city growth
- Save-state persistence
- Content scaling from a 3-faction prototype to 20 civilizations plus the Axiom Crown

The goal is to ensure the game does **not** become five separate games bolted together.

The same underlying entities and data must power every mode.

---

# 2. PRIMARY TECHNICAL PRINCIPLE

# SINGLE SOURCE OF TRUTH

A card is not:

- one UI object,
- plus one building definition,
- plus one combat object,
- plus one economy table,
- plus one separate upgrade record.

Instead:

# CARD DEFINITION
drives all representations.

A runtime Card Instance stores the player's unique ownership/history state.

A placed Card Instance creates a World Asset Instance.

This produces the canonical chain:

`Card Definition`
→ `Owned Card Instance`
→ `Deck Entry`
→ `Placement Preview`
→ `World Asset Instance`
→ `Simulation Entity`
→ `Combat Entity`
→ `Save-State Record`

Every system references stable IDs rather than copying data.

---

# 3. RECOMMENDED PRODUCTION TARGET

For a full premium 3D release, the recommended production architecture is:

# UNREAL ENGINE 5-GENERATION CLIENT

paired with data-oriented simulation services inside the game runtime.

Why this is the best conceptual fit:

- large streamed 3D environments,
- strong third-person tooling,
- destructible-world support,
- AI/state-machine tooling,
- data assets,
- procedural generation,
- scalable NPC representation,
- modern rendering,
- strong animation stack,
- mature console/PC production path.

However, the **simulation schema defined in this document is intentionally engine-agnostic**.

It could be implemented in another capable 3D engine without rewriting the game design.

---

# 4. PLATFORM PRIORITY

Production priority:

1. Windows PC
2. Current-generation consoles
3. Steam Deck / comparable performance profiles
4. High-end cloud streaming where appropriate

A browser build should be considered a separate reduced-scope product rather than forcing the full simulation into a web runtime.

---

# 5. GAME MODES ARE CAMERA + CONTROL STATES

Do not build Founder Mode, City Mode, and Command Mode as different levels.

They are different views over the **same world state**.

## FOUNDER MODE

- Third-person camera
- Direct Founder movement
- Interaction targeting
- Full combat controls

## COMMAND MODE

- Elevated tactical camera
- Squad selection
- Objective assignment
- Tactical slowdown
- Same units remain physically present

## CITY MODE

- High-altitude strategic camera
- Grid placement
- zoning overlays
- economy overlays
- construction management

Changing modes should never reload the city.

---

# 6. CAMERA STATE MACHINE

Canonical camera states:

- Founder Follow
- Founder Aim
- Founder Interior
- Tactical Orbit
- Tactical Focus
- City Orbit
- City Placement
- City Inspection
- World Map
- Cinematic
- Ascension

Each state defines:

- FOV
- pitch limits
- movement model
- input mapping
- UI layer
- selection rules
- simulation speed

---

# 7. SEAMLESS MODE TRANSITION

Example:

Founder presses City Mode.

Sequence:

1. Founder input locks.
2. Camera rises vertically.
3. Nearby geometry shifts LOD.
4. Citizen animation fidelity reduces.
5. City overlay fades in.
6. Selection system changes from interaction targeting to grid selection.
7. Simulation never stops.

Target transition:
**under 1.5 seconds** in normal circumstances.

The goal is to make the world feel continuous.

---

# 8. WORLD SPACE SCALE

Prototype:

- Grid: 32×32
- Cell size: 8 m
- City footprint: 256 m × 256 m
- 4×4 districts
- District: 8×8 cells

Full primary city:

- Grid: 128×128
- Cell size: 8 m
- City footprint: 1.024 km × 1.024 km
- 16×16 districts
- District: 8×8 cells

Visual structures may overhang logical cells.

---

# 9. WORLD HIERARCHY

Campaign world:

`Eidolon`
→ `Region`
→ `City`
→ `District`
→ `Cell`
→ `World Asset`
→ `Modules`

Example:

`Eidolon`
→ `Forgeweave Region`
→ `Ironheart`
→ `Foundry District 04`
→ `Cell 18,27`
→ `Infinite Foundry`
→ `Coolant Module`

Every layer has a stable persistent ID.

---

# 10. CORE PERSISTENT IDS

Required stable identifiers:

- CampaignID
- CivilizationID
- RegionID
- CityID
- DistrictID
- CellID
- CardDefinitionID
- CardInstanceID
- WorldAssetID
- CitizenID
- HouseholdID
- FactionID
- LeaderID
- UnitInstanceID
- VehicleInstanceID
- QuestID
- EventInstanceID
- TreatyID
- WarID

IDs should never be derived from display names.

Names can change.

IDs cannot.

---

# 11. CARD DEFINITION SCHEMA

Canonical content record:

```yaml
cardDefinition:
  id: synara.adaptive_habitat
  version: 1
  displayName: Adaptive Habitat
  civilization: synara
  cardType: residential
  rarity: specialized

  footprint:
    width: 2
    depth: 2
    rotationAllowed: true

  placement:
    validTerrain:
      - urban
      - plains
    requiresRoad: true
    roadDistance: 1
    powerDemand: 4
    waterDemand: 3
    dataDemand: 1

  economy:
    buildCapital: 8
    buildInsight: 0
    buildInfluence: 0
    maintenanceCapital: 1
    constructionCycles: 2

  population:
    housingCapacity: 8
    jobs: []

  production: []

  tags:
    - residential
    - synara
    - adaptive
    - networkable

  adjacency:
    positive: []
    negative: []

  simulation:
    factionPressure:
      dependency: 0

  combat:
    structuralIntegrity: 500
    armor: 10
    cyberIntegrity: 65
    capturable: true

  upgrades:
    level2:
      - density_branch
      - quality_branch

  representation:
    cardArt: cards/synara/adaptive_habitat
    worldPrefab: buildings/synara/adaptive_habitat
    constructionPrefab: construction/synara/adaptive_habitat
    ruinPrefab: ruins/synara/adaptive_habitat

  audio:
    placement: audio/build/place_habitat
    activeLoop: audio/build/habitat_active

  lore:
    key: lore.synara.adaptive_habitat
```

This data should be authorable through tools, not hand-maintained in giant source files.

---

# 12. CARD INSTANCE SCHEMA

A Card Definition is global content.

A Card Instance belongs to a player.

```yaml
cardInstance:
  id: card_7fb2...
  definitionId: synara.adaptive_habitat

  ownerCivilizationId: player
  acquiredAtWorldTick: 331
  acquiredBy: starter_deck

  masteryXP: 112
  masteryLevel: 2

  cosmeticVariant: ivory_glass_02

  upgradeState:
    level: 2
    branch: quality

  status:
    inDeck: true
    placed: true
    recoveryState: none

  worldAssetId: asset_a98c...
```

One definition may have many owned instances.

---

# 13. WORLD ASSET INSTANCE

When a Card Instance is placed:

```yaml
worldAsset:
  id: asset_a98c...
  cardInstanceId: card_7fb2...
  cityId: player_capital
  districtId: district_03_02

  transform:
    gridX: 14
    gridY: 22
    rotation: 90

  state:
    construction: operational
    structuralIntegrity: 472
    powerConnected: true
    waterConnected: true
    dataConnected: true
    staffedRatio: 0.875

  owner: player

  modules:
    - habitat_core
    - utility_spine

  runtime:
    currentOutputMultiplier: 1.12
```

This is the persistent bridge between card and world.

---

# 14. NO DUPLICATED ECONOMY VALUES

The world actor should never contain hardcoded:

`CapitalPerCycle = 4`

if that value exists in the Card Definition.

The actor asks the simulation:

> What does this definition currently produce given its instance state, upgrades, adjacency, staffing, policies, and world modifiers?

This prevents content drift.

---

# 15. SIMULATION ARCHITECTURE

Use a data-oriented simulation layer.

Core systems:

- Time System
- Resource System
- Population System
- Job System
- Utility System
- Adjacency System
- Construction System
- Faction System
- Migration System
- Trade System
- Diplomacy System
- Warfare System
- Event System
- Rival AI System
- Quest System
- Card Acquisition System
- Save System

Systems operate on persistent records rather than requiring every world actor to tick individually.

---

# 16. TICK RATE SEPARATION

Not every system needs 60 updates per second.

## RENDER / MOVEMENT

30–120 Hz depending platform and subsystem.

## COMBAT SIMULATION

30–60 Hz.

## NEARBY CITIZEN AI

5–15 Hz decision updates.

## CITY SIMULATION

1 Hz internal updates.

## DEVELOPMENT CYCLE

Every 30 seconds.

## WORLD TICK

Every 5 Development Cycles.

This separation is essential for scale.

---

# 17. DEVELOPMENT CYCLE

Existing canon:

**1 Development Cycle = 30 seconds of active simulation.**

At cycle boundary:

- building production resolves,
- upkeep resolves,
- population growth evaluates,
- class XP aggregates,
- faction pressure evaluates,
- card construction progresses,
- city events may trigger.

Real-time actors can animate continuously without requiring economic calculations every frame.

---

# 18. WORLD TICK

Existing canon:

**1 World Tick = 5 Development Cycles.**

At World Tick:

- rival construction resolves,
- diplomatic AI runs,
- trade markets update,
- migrations process,
- armies move,
- world events advance,
- remote cities progress.

---

# 19. SIMULATION AUTHORITY

The simulation state is authoritative.

3D actors are representations.

If a building streams out:

the building does not stop existing.

Its simulation record continues operating.

When streamed back in:

the world representation reconstructs from simulation state.

---

# 20. WORLD STREAMING

Each city uses spatial streaming.

Recommended unit:

# DISTRICT STREAMING CELL

One 8×8 logical district can be a primary streaming group.

Nearby:
- full geometry
- interiors
- citizens
- VFX

Mid distance:
- simplified buildings
- minimal citizens
- reduced animation

Far:
- merged district representation
- no individual citizens

---

# 21. SIMULATION LOD

Rendering LOD is not enough.

Use:

# SIMULATION LEVEL OF DETAIL

## SIM LOD 0 — FULL

Current Founder district.

- individual named citizens
- navigation
- conversations
- combat
- full building modules

## SIM LOD 1 — DETAILED

Nearby districts.

- citizen cohorts
- important named NPCs
- simplified schedules

## SIM LOD 2 — AGGREGATED

Current city, distant districts.

- population cohorts
- facility calculations
- no individual movement

## SIM LOD 3 — STRATEGIC

Remote cities.

- district-level economy
- strategic construction
- diplomacy
- armies

---

# 22. CITIZEN REPRESENTATION LOD

One citizen may exist as:

### Persistent record
Always.

### Cohort member
Most simulation.

### Lightweight crowd entity
When nearby.

### Full character actor
When interacting.

A named citizen promoted into a story quest is elevated from cohort simulation into a full persistent actor when required.

---

# 23. CROWD BUDGET

Initial performance target for active city district:

- 100–200 visible ambient citizens
- 20–40 high-fidelity nearby citizens
- 10–20 fully interactive named characters
- additional distant crowd impostors as platform allows

The city can represent thousands of residents without spawning thousands of full AI pawns.

---

# 24. CITIZEN SCHEDULES

Do not pathfind every citizen across the entire city every morning.

Use hierarchical travel.

Example:

Home
→ local exit node
→ district transit graph
→ destination entry node
→ workplace

Only nearby segments require physical pathfinding.

Remote commute happens through simulation.

---

# 25. NAVIGATION ARCHITECTURE

Navigation layers:

- Pedestrian Nav
- Vehicle Nav
- Tactical Unit Nav
- Service/Logistics Graph
- Transit Graph
- Air/Drone Graph

A single universal navmesh should not be forced to solve all movement.

---

# 26. ROAD GRAPH

Road placement generates logical graph edges.

Each segment stores:

- lane class
- capacity
- speed
- damage
- owner
- transit permissions

The same road graph powers:

- civilian commute,
- logistics,
- reinforcement travel,
- vehicle navigation,
- trade flow.

---

# 27. UTILITY NETWORKS

Utilities are graph simulations:

- Power
- Water
- Data
- Logistics
- Transit

Each has:

- nodes
- edges
- capacity
- demand
- damage state

A facility requests capacity.

The graph resolves whether demand is satisfied.

---

# 28. NETWORK FAILURE

Aethernet-style cascade failures become natural outcomes of the same graph architecture.

Destroy node:
`A`

Dependent edges recalculate.

Disconnected assets lose data coverage.

No custom scripted fake outage is required.

---

# 29. ADJACENCY SYSTEM

Adjacency should be computed when relevant state changes, not every frame.

Recalculate when:

- building placed,
- building removed,
- building upgraded,
- district modifier changes,
- relevant technology activates.

Cache results.

---

# 30. SPATIAL QUERY

Use spatial indexing for:

- nearby tags,
- cover,
- district membership,
- effect radius,
- utility nodes,
- capture zones.

Avoid brute-force querying all city assets.

---

# 31. CITY GRID SERVICE

The grid service owns:

- occupancy
- footprints
- terrain validity
- placement reservation
- district ownership
- underground slots
- elevation layer
- road compatibility

World meshes do not determine gameplay occupancy.

---

# 32. MULTI-LAYER GRID

Each cell may contain multiple logical layers:

- Surface
- Elevated
- Underground
- Utility
- Airspace

This allows:

- tunnels,
- transit,
- skybridges,
- utility conduits,
- elevated structures.

---

# 33. PLACEMENT PREVIEW

Placement sequence:

1. Player selects Card Instance.
2. Ghost prefab appears.
3. Grid cells highlight.
4. Utility requirements preview.
5. Adjacency bonuses preview.
6. Negative effects preview.
7. Cost preview.
8. Confirm.
9. Simulation reserves cells.
10. Construction actor spawns.

Never hide major consequences until after placement.

---

# 34. CONSTRUCTION STATE MACHINE

Building states:

- Preview
- Reserved
- Foundation
- Frame
- Exterior
- Systems
- Commissioning
- Operational
- Damaged
- Disabled
- Ruined
- Reconstructing

These states are content-driven.

---

# 35. CONSTRUCTION ANIMATION

Avoid one generic hologram.

Each civilization receives construction language.

## Synara
Precision drone assembly and luminous smart material.

## Forgeweave
Heavy cranes, molten fabrication, industrial panels.

## Eden Circuit
Growth, woven biomaterial, living structural formation.

## Aethernet
Network nodes activate first, then structure assembles around signal infrastructure.

Construction identity reinforces civilization identity.

---

# 36. CONSTRUCTION PREFAB KIT

Every placeable building requires:

- completed mesh
- construction-stage meshes or reveal masks
- collision
- nav blockers
- sockets
- modules
- damage zones
- interaction points
- signage anchors
- FX anchors
- audio anchors
- LODs
- proxy mesh
- ruin representation

---

# 37. MODULAR BUILDING KIT

To scale 20 factions, use:

# KIT + HERO STRUCTURE

Most city buildings use modular kits.

Each civilization requires:

- wall family
- facade family
- roof family
- window family
- utility family
- prop family
- signage family
- street furniture
- material library
- vegetation/environment kit where relevant

Hero/Wonder buildings receive bespoke assets.

---

# 38. ARCHITECTURAL GRAMMAR

Each civilization needs formal rules.

Example:

## Synara

- vertical proportions
- clean layered shells
- controlled emissive seams
- curved computational surfaces
- white ceramic/composite
- silver
- transparent smart glass

Forbidden:
- industrial rust
- exposed crude pipe networks
- random cyberpunk neon

This prevents procedural generation from eroding art direction.

---

# 39. PROCEDURAL VARIATION

A building definition points to:

- base footprint
- archetype
- faction kit
- height range
- module rules
- facade seed
- prop seed

Multiple placements of the same card can look related without being visually identical.

---

# 40. CARD ART AND WORLD ASSET

Card art should be generated/rendered from the canonical asset whenever practical.

Pipeline:

`3D Asset`
→ staged faction environment
→ hero camera
→ high-quality render
→ card art treatment

This maintains visual correspondence between the card and the object the player places.

---

# 41. CARD VISUAL IDENTITY

Card frame communicates:

- Civilization
- Card Type
- Rarity
- Footprint
- Costs
- Key tags
- Upgrade state

Avoid cramming full simulation data onto the face.

Detailed information lives in Inspect view.

---

# 42. CARD-TO-WORLD TRANSITION

When placed:

1. Card enlarges into world-space UI.
2. Frame separates.
3. 3D blueprint volume appears.
4. card artwork aligns with world footprint.
5. blueprint collapses into construction site.
6. physical construction begins.

The animation should make literal the idea:

> **The card becomes the city.**

---

# 43. DESTRUCTION ARCHITECTURE

Structures need modular damage.

Do not require every building to be a fully simulated physics object.

Damage hierarchy:

- Cosmetic damage
- Module damage
- Structural section damage
- Disabled state
- Ruin transition

Only important pieces require full physics.

---

# 44. STRUCTURAL MODULE SCHEMA

Example:

```yaml
modules:
  - id: power_core
    maxIntegrity: 100
    function:
      requiredFor:
        - production
    damageResponse:
      arcMultiplier: 1.4

  - id: production_line
    maxIntegrity: 160
    function:
      outputMultiplier: 1.0

  - id: control_center
    maxIntegrity: 80
    function:
      capturable: true
```

---

# 45. DESTRUCTION PERFORMANCE

Use staged destruction:

## Stage 1
Decals/material damage.

## Stage 2
Pre-authored breakable modules.

## Stage 3
Local physics debris.

## Stage 4
Ruin state.

Debris beyond gameplay relevance is pooled or removed.

---

# 46. RUIN AS GAMEPLAY

Ruin representation stores:

- original WorldAssetID
- salvage value
- reconstruction cost
- cover geometry
- contamination/fire hazards

Ruins remain strategic objects.

---

# 47. INTERIORS

Do not make every building fully explorable.

Three interior tiers:

## TIER A — HERO INTERIOR
Leaders, Wonders, story buildings.

## TIER B — MODULAR INTERIOR
Offices, markets, civic centers.

## TIER C — FACADE + ENTRY
Normal simulation buildings.

This keeps production feasible.

---

# 48. INTERIOR STREAMING

Interior cells stream when:

- Founder approaches entrance,
- mission requires building,
- combat moves indoors.

Exterior remains visible through proxy systems as necessary.

---

# 49. FOUNDER CHARACTER ARCHITECTURE

Founder consists of:

- Character Motor
- Camera Controller
- Interaction Component
- Combat Component
- Ability Component
- Equipment Component
- Traversal Component
- Dialogue Interface
- Tactical Command Interface

Systems should remain modular.

---

# 50. ABILITY SYSTEM

Abilities use data definitions.

Each ability defines:

- activation
- cost
- cooldown
- targeting
- effect tags
- damage
- status effects
- animation
- VFX
- audio
- AI hints

Founder, Leaders, Champions, and units should use the same core ability framework.

---

# 51. STATUS EFFECTS

Examples:

- Suppressed
- Shielded
- Burning
- Disrupted
- Hacked
- Regenerating
- Inspired
- Routed
- Immobilized
- Marked

Effects are tag-driven.

---

# 52. UNIT ARCHITECTURE

Unit card:

`UnitDefinition`

Runtime:

`UnitInstance`

World representation:

`SquadEntity`

A squad may contain multiple visual actors but one tactical entity.

---

# 53. SQUAD CONTROLLER

Squad controller owns:

- formation
- destination
- target
- cover assignment
- morale
- suppression
- supply
- active ability
- casualty state

Individual members handle local locomotion and animation.

---

# 54. COVER SYSTEM

Cover data can be generated from:

- building sockets
- road barriers
- destroyed modules
- terrain
- deployables

Each cover point stores:

- direction
- height
- occupancy
- protection rating
- structural source

---

# 55. LINE OF SIGHT

Combat LOS:

1. broad-phase spatial visibility,
2. narrow ray/shape tests,
3. cover resolution,
4. sensor modifiers.

Do not raycast every unit against every enemy every frame.

---

# 56. TACTICAL COMMAND SERVICE

Command Mode does not possess separate units.

It queries existing World Unit Instances.

Service supports:

- select
- group
- move
- attack
- defend
- capture
- escort
- use ability
- doctrine assignment

---

# 57. AI ARCHITECTURE

Separate:

# MICRO AI

Immediate unit behavior.

# TACTICAL AI

Squad/objective behavior.

# STRATEGIC AI

City, diplomacy, war, economy.

Do not put all behavior in one giant AI controller.

---

# 58. MICRO AI

Examples:

- seek cover,
- reload,
- heal,
- flee grenade,
- maintain formation,
- select target.

Update frequently.

---

# 59. TACTICAL AI

Examples:

- defend Control Zone,
- flank,
- retreat,
- capture facility,
- sabotage utilities,
- protect Leader.

Update less frequently.

---

# 60. STRATEGIC AI

Examples:

- build factory,
- form alliance,
- declare war,
- increase housing,
- solve faction crisis.

Runs on World Ticks.

---

# 61. LEADER AI

Leader personality provides weights.

```yaml
leader:
  id: forgeweave.daxton_rhe

  strategy:
    industry: 1.0
    defense: 0.8
    expansion: 0.7
    diplomacy: 0.4
    ecology: 0.15

  personality:
    aggression: 0.82
    honor: 0.71
    riskTolerance: 0.78
    adaptability: 0.62
```

Strategic AI reads these weights.

---

# 62. STATE TREE / BEHAVIOR LAYERS

Leader runtime behaviors should be separate from strategic personality.

A Leader Boss can have:

- Combat Phase State
- Objective State
- Dialogue State
- Crisis State
- Surrender Evaluation

The campaign Leader record persists outside combat.

---

# 63. RIVAL CITY GENERATION

Rival cities are not static maps.

Generation inputs:

- Civilization
- region terrain
- campaign seed
- Leader priorities
- available Card Pool
- historical World Tick
- economic conditions

Output:

- road skeleton
- district plan
- initial facilities
- defensive plan
- population distribution

Then the rival AI develops it during play.

---

# 64. PROCEDURAL CITY PIPELINE

1. Terrain constraints
2. Major road graph
3. District allocation
4. Utility backbone
5. Mandatory civilization structures
6. Residential capacity
7. Economy facilities
8. Civic facilities
9. Defense
10. Decoration/props
11. Navigation bake/update
12. Simulation validation

---

# 65. CITY VALIDATION

A generated rival city must satisfy:

- minimum housing
- utility supply
- road access
- workforce viability
- Leader location
- Wonder/capital location
- viable siege paths
- at least two alternate approaches
- no inaccessible critical objective

Generation fails validation → reroll or repair.

---

# 66. RIVAL CITY GROWTH

Strategic AI uses a build planner.

Example priorities:

`need housing`
→ search owned Card Pool for valid housing
→ evaluate candidate districts
→ score placements
→ reserve Capital
→ place asset
→ save WorldAsset record

The same rules apply to player and AI.

---

# 67. AI FAIRNESS

Standard difficulty rivals should obey:

- resources,
- construction times,
- card access,
- utility rules.

Higher difficulty can receive:

- better planning,
- better tactical coordination,
- modest production efficiency,
- faster adaptation.

Avoid spawning impossible resources.

---

# 68. PROCEDURAL DECORATION

Roadside props, trees, crowds, signage, clutter, and parked vehicles should use procedural dressing.

Gameplay assets remain deterministic.

Decoration can vary without affecting save compatibility.

---

# 69. WORLD REGION STREAMING

The full planet should not be one physically loaded continuous map.

Campaign uses region instances connected through strategic travel.

Each major civilization region contains:

- city
- outskirts
- resource zones
- minor settlements
- exploration areas

World Map represents larger geopolitical movement.

---

# 70. REGION PERSISTENCE

Even unloaded regions maintain:

- city simulation,
- owner,
- resources,
- army positions,
- trade,
- quests,
- major damage,
- construction queue.

Physical world reconstructs when loaded.

---

# 71. TRAVEL TRANSITION

When traveling:

1. Save current region delta.
2. Advance strategic time as appropriate.
3. Stream destination region.
4. Instantiate persistent local world state.
5. Spawn Founder at valid transit point.
6. Resume high-fidelity simulation.

---

# 72. WORLD MAP DATA MODEL

```yaml
region:
  id: forgeweave_heartland

  owner: forgeweave
  neighbors:
    - eden_basin
    - synara_frontier

  resources:
    metals: 85
    water: 32
    biomass: 21

  settlements:
    - ironheart
    - smelter_quarter
    - ore_station_7

  armies:
    - forge_army_01

  activeEvents:
    - foundry_shortage
```

---

# 73. SAVE PHILOSOPHY

Do not serialize entire world actors.

Save:

- canonical definitions by ID
- runtime persistent state
- changes from defaults

Reconstruct visual actors from saved state.

---

# 74. SAVE LAYERS

## PROFILE SAVE

- accessibility
- settings
- cosmetics
- meta unlocks

## CAMPAIGN SAVE

- world
- diplomacy
- collection
- Founder
- Relics
- campaign seed

## CITY SAVE

- World Assets
- citizens
- districts
- utilities
- resources

## OPERATION SAVE

- active siege
- units
- damage
- objectives

---

# 75. SAVE VERSIONING

Every save contains:

- schema version
- content version
- build version

Migration functions upgrade older records.

Never assume saves match current content schema exactly.

---

# 76. CONTENT VERSIONING

Card Definitions include a content version.

Balance updates should distinguish:

- stat changes
- schema changes
- asset changes
- removed content

Existing Card Instances reference Definition ID and inherit safe balance changes.

---

# 77. DEFINITION REGISTRY

At game boot:

1. Load Card Registry.
2. Validate unique IDs.
3. Validate references.
4. Validate upgrade trees.
5. Validate prefab paths.
6. Validate faction tags.
7. Validate localization keys.
8. Report broken content.

Broken content should fail during development builds, not appear silently at runtime.

---

# 78. CONTENT VALIDATION RULES

Examples:

- Placeable card must have footprint.
- World asset card must have prefab.
- Residential must define capacity.
- Job-producing asset must reference valid classes.
- Capturable building requires control module.
- Wonder must enforce copy limit 1.
- Dominion card must not appear in random acquisition pools.

Automate these checks.

---

# 79. DATA AUTHORING TOOLS

Designers need editors for:

- Card Definitions
- Upgrade Trees
- Civilization Kits
- Leader Personality
- Events
- Quests
- Factions
- Trade Goods
- Dialogue Conditions
- Spawn Tables

Avoid requiring programmers for routine content authoring.

---

# 80. CARD DEFINITION EDITOR

Editor should preview:

- card frame
- 3D asset
- footprint
- cost
- economy
- utilities
- adjacency
- combat modules
- upgrade branches
- acquisition methods

One tool should expose the complete lifecycle.

---

# 81. CITY SANDBOX TOOL

Internal tool:

- instant resource controls
- spawn any card
- toggle grid
- inspect utilities
- inspect adjacency
- fast-forward cycles
- force faction events
- damage buildings
- swap civilization art kits

This will be critical for balancing.

---

# 82. SIMULATION INSPECTOR

Developer overlay shows:

- WorldAssetID
- DefinitionID
- current production
- modifiers
- staffing
- utility state
- adjacency
- faction pressure
- save state

Every unexpected number should be explainable.

---

# 83. ECONOMY TRACE

For each output:

**Adaptive Exchange: 5.6 Capital**

Trace:

- Base: 4.0
- District bonus: +10%
- Network bonus: +20%
- Staff: 100%
- Policy: +5%
- Dependency penalty: -5%

Final:
5.2 or calculated result according to actual stacking rules.

The system should expose math clearly to designers and players.

---

# 84. EVENT SYSTEM

Event definitions contain:

- eligibility
- trigger
- warning state
- stages
- choices
- AI responses
- consequences
- persistent flags

Events modify underlying systems rather than faking outcomes where possible.

---

# 85. EVENT SCHEMA

```yaml
event:
  id: world.foundry_shortage

  trigger:
    forgeweaveResourceHunger:
      greaterThan: 70

  stages:
    - warning
    - market_pressure
    - eden_response
    - regional_shortage

  choices:
    - support_forgeweave
    - support_eden
    - broker
    - exploit
```

---

# 86. QUEST SYSTEM

Quests are state graphs.

Nodes:

- objective
- dialogue
- wait condition
- choice
- event
- combat
- reward
- fail state

Quest actions should reference persistent IDs.

---

# 87. QUEST WORLD INTEGRATION

If a quest says:

“Repair Nia's Cognitive Tower”

it references the real WorldAssetID.

Destroy that tower before the quest?

The quest adapts.

Do not spawn a duplicate fake tower.

---

# 88. DIALOGUE CONDITIONS

Dialogue can query:

- Leader relationship
- faction support
- city metrics
- prior quest choices
- conquest history
- card ownership
- Founder attributes
- civilian damage history
- active treaties

This creates reactive dialogue.

---

# 89. DYNAMIC BARKS

Citizens react to:

- nearby construction
- war
- shortages
- Founder presence
- new policies
- conquered architecture
- high pollution
- machine rights
- migration

Barks are condition-tagged.

---

# 90. AUDIO SYSTEM

Audio layers:

- civilization ambience
- district ambience
- building loops
- citizen ambience
- combat
- strategic tension
- event music
- Leader themes
- Ascension themes

Conquered architecture can introduce musical motifs into the player's capital.

---

# 91. MUSIC HYBRIDIZATION

As civilizations integrate, the capital score layers motifs.

Example:

Synara base
+ Forgeweave percussion
+ Eden organic textures

The soundtrack becomes another representation of campaign history.

---

# 92. VFX SYSTEM

Faction VFX style guide should define:

- construction
- damage
- healing
- shields
- network
- capture
- ability telegraph
- Relic
- Ascension

Avoid arbitrary effects disconnected from faction identity.

---

# 93. PERFORMANCE BUDGETS

Performance targets should be platform-scaled.

Initial PC target:

- 60 FPS standard gameplay target
- 30 FPS fallback high-density mode
- no simulation stalls at Development Cycle boundaries
- mode transition under 1.5 seconds
- district streaming without visible major hitch

---

# 94. FRAME BUDGET PRINCIPLE

Never run:

- economy recalculation
- population reassignment
- world AI
- massive pathfinding
- shader compilation

all on one frame.

Spread simulation work across frames and worker tasks.

---

# 95. ASYNC SIMULATION

Systems suited for asynchronous work:

- rival planning
- migration scoring
- job matching
- trade recalculation
- procedural city scoring
- path precomputation
- adjacency rebuild

Results apply at safe synchronization points.

---

# 96. JOB MATCHING PERFORMANCE

Do not compare every citizen to every job.

Partition by:

- city
- district
- skill bucket
- commute zone
- class qualification

Then solve local matching.

---

# 97. EVENT-DRIVEN RECALCULATION

Example:

Power network recalculates when:

- generator changes
- line changes
- demand changes
- damage occurs

Not every frame.

Same principle applies to most city simulation.

---

# 98. DETERMINISM

Strategic simulation should be deterministic for a given:

- campaign seed
- state
- actions

Use seeded randomness.

Benefits:

- reproducible bugs
- stable saves
- replay/debugging
- future multiplayer options

---

# 99. RANDOM STREAMS

Separate random streams:

- world generation
- AI decisions
- event rolls
- cosmetic variation
- loot/cache rewards
- combat spread

Cosmetic randomness must not accidentally alter strategic outcomes.

---

# 100. REPLAY LOG

Development builds should optionally record:

- player commands
- random seeds
- cycle boundaries
- strategic AI decisions

This enables deterministic bug reproduction.

---

# 101. SAVE SAFETY

Use:

- autosave rotation
- transactional writes
- checksum
- backup slot
- recovery metadata

A large persistent city game cannot tolerate fragile saves.

---

# 102. AUTOSAVE MOMENTS

Autosave:

- every configurable time interval
- before travel
- after Ascension
- after major conquest resolution
- before irreversible Leader decision
- after major construction batch if needed

Avoid autosaving mid-destructive transaction.

---

# 103. CONTENT STREAMING

Content bundles should be grouped by:

- civilization
- region
- common systems
- active quest

Do not keep all 20 civilizations' high-resolution hero assets resident.

---

# 104. MATERIAL STRATEGY

Use shared faction master materials.

Parameters:

- faction palette
- wear
- emissive
- damage
- wetness
- construction state
- capture ownership

This reduces shader/content explosion.

---

# 105. CAPTURE VISUAL TRANSITION

Captured building should not instantly reskin.

Stages:

1. Original faction building
2. New banners/signage
3. utility integration
4. optional hybrid retrofit
5. full conversion only if player chooses Convert

This visually preserves history.

---

# 106. HYBRID ARCHITECTURE

Hybrid buildings should be composed from compatible sockets.

Example:

Synara + Forgeweave Autonomous Factory:

- Forgeweave heavy industrial chassis
- Synara orchestration crown
- mixed material set
- autonomous drone gantries
- shared construction language

Avoid simply recoloring one faction.

---

# 107. UPGRADE VISUALIZATION

Level upgrades should visibly modify the structure.

Level II:
- add module
- alter facade
- add signage/utility hardware

Level III:
- major silhouette change

Fusion:
- unmistakable cross-civilization module

Players should read progression from the skyline.

---

# 108. DAMAGE PERSISTENCE

Damage remains until repaired.

A battle-scarred district should stay visibly damaged after the war.

Repair work becomes:

- economic action
- visual storytelling
- post-war gameplay.

---

# 109. CITY HISTORY LAYER

Each district stores notable history:

- founded date
- major battles
- previous owner
- disasters
- Ascension transformation
- famous citizens

Inspectable plaques or UI can surface this.

---

# 110. CARD HISTORY

A Card Instance tracks notable events:

- first placement
- cities served
- battles survived
- captures
- destruction/rebuild count
- Mastery
- previous owners if traded/captured

Rare cards become artifacts with history.

---

# 111. CAPTURED ENEMY CARDS

If a building is preserved after conquest:

the player can eventually receive its Card Instance.

The card remembers:

**Captured at Ironheart during Operation Iron Veil.**

This creates collectible provenance.

---

# 112. 3D CARD INSPECTION

Collection view should allow:

- rotate card
- flip for lore/stats
- transition into 3D asset
- inspect construction stages
- inspect upgrades
- inspect owned instances and histories

The collection becomes a museum of the campaign.

---

# 113. UI ARCHITECTURE

Major UI layers:

- HUD
- Founder Interaction
- Combat
- Tactical Command
- City Build
- Economy
- Collection
- Deck Builder
- Diplomacy
- World Map
- Citizens
- Factions
- Quest
- Inspection
- Pause/Settings

Shared data selectors should prevent duplicate state logic.

---

# 114. INFORMATION HIERARCHY

The player should never need a spreadsheet to understand immediate problems.

City Mode surfaces:

## FIRST LEVEL
“What is wrong?”

## SECOND LEVEL
“Why?”

## THIRD LEVEL
“Exact numbers.”

Example:

**Power Deficit**

→ Industrial District 3 is undersupplied.

→ inspect graph for exact demand/capacity.

---

# 115. OVERLAYS

City overlays:

- Power
- Water
- Data
- Logistics
- Transit
- Pollution
- Happiness
- Wealth
- Equality
- Security
- Faction support
- Loyalty
- Housing
- Employment
- Adjacency

Only one primary diagnostic overlay should dominate at a time.

---

# 116. ACCESSIBILITY

Support:

- full pause option
- scalable text
- UI contrast modes
- non-color faction indicators
- remapping
- camera sensitivity
- motion reduction
- subtitle controls
- combat assist
- city automation assist

Strategic complexity should not require high motor speed.

---

# 117. INPUT CONTEXT

Input maps change by mode.

Founder:
- move
- aim
- interact
- combat

Command:
- select
- pan
- order
- group

City:
- pan
- rotate
- place
- inspect

Transitions clear incompatible held inputs.

---

# 118. CONTROLLER-FIRST TEST

All core actions must be possible on controller.

City building cannot depend on mouse-only precision.

---

# 119. MODDABILITY

Long-term architecture should keep game definitions data-driven enough to support:

- new cards
- new factions
- custom scenarios
- balance mods

Do not promise full mod support in the first release, but avoid architecture that makes it impossible.

---

# 120. NETWORK/MULTIPLAYER POSITION

Primary campaign should be built as:

# SINGLE-PLAYER FIRST

Do not architect the vertical slice around live multiplayer.

However:

- deterministic strategic systems,
- stable IDs,
- authoritative state boundaries

should keep future asynchronous or co-op possibilities open.

---

# 121. POSSIBLE FUTURE MULTIPLAYER

Later possibilities:

- city showcase
- card trading
- asynchronous challenge cities
- co-op siege
- limited PvP arena
- campaign federation

None should block single-player production.

---

# 122. TELEMETRY

Internal playtest telemetry:

- cards drawn
- cards placed
- placement cancel rate
- resource starvation
- upgrade choices
- citizen migration
- faction crises
- combat deaths
- buildings destroyed/captured
- conquest methods
- Leader outcomes
- mode-switch frequency
- frame times
- streaming stalls

Use telemetry to identify design friction.

---

# 123. QA TEST MATRIX

Critical system intersections:

### Card × Grid
Can every footprint place correctly?

### Card × Utilities
Do requirements update?

### Card × Save
Does placement persist?

### Building × Combat
Does damage persist?

### Building × Capture
Does ownership change?

### Citizen × Building
Does job/housing state survive streaming?

### Citizen × Quest
Does named NPC persist?

### Rival AI × World Tick
Does remote growth stay valid?

### Conquest × Diplomacy
Do relationships update?

### Ascension × Card Pool
Are new cards unlocked correctly?

---

# 124. AUTOMATED DATA TESTS

Run validation for every content build:

- duplicate IDs
- missing prefabs
- impossible footprints
- cyclic upgrade graph
- invalid acquisition pool
- missing localization
- missing ruin asset
- invalid class references
- missing civilization
- impossible utility requirements

---

# 125. SIMULATION TESTS

Headless tests should support:

- 1,000 Development Cycles
- rival growth
- economy stability
- migration
- faction escalation
- market simulation
- conquest aftermath

The design must survive long simulations without requiring rendering.

---

# 126. SOAK TESTS

Run campaigns for simulated months.

Detect:

- infinite wealth
- dead economies
- irreversible population collapse
- AI construction deadlock
- utility loops
- faction spam
- uncontrolled save growth

---

# 127. SAVE SIZE BUDGET

Avoid storing transient visual state.

Persistent data should stay compact enough that:

- multiple campaign saves are practical,
- autosaves remain fast,
- cloud sync remains reasonable.

Citizen cohort aggregation is important here.

---

# 128. FIRST PRODUCTION DATASET

Vertical slice minimum:

## Civilizations
- Synara
- Forgeweave
- Eden Circuit

## Cards
- 60-card Synara deck
- Forgeweave core set
- Eden support set
- Universal set

## Citizens
- 200 simulated
- 20 named

## Cities
- 1 player city
- 1 rival city
- 1 neutral city

## World
- 3 minor settlements
- resource zones
- Iron Veil combat area

---

# 129. VERTICAL SLICE SCENE ARCHITECTURE

Recommended region set:

## SYNARA CAPITAL
Player construction sandbox.

## FORGEWEAVE IRONHEART
Rival persistent city.

## EDEN BASIN
Diplomatic/environmental city.

## FRONTIER CORRIDOR
Travel/exploration.

## IRON VEIL SIEGE DISTRICT
Part of Ironheart, not a disconnected arena.

---

# 130. VERTICAL SLICE TECH MILESTONE 1
# ONE CARD BECOMES ONE BUILDING

Implement:

- CardDefinition
- CardInstance
- Deck
- placement
- grid
- construction
- WorldAsset
- save/reload

Card:
**Adaptive Habitat**

Success criterion:

The same Card Instance can be:

1. viewed in collection,
2. added to deck,
3. drawn,
4. placed,
5. constructed,
6. damaged,
7. saved,
8. reloaded,
9. inspected with history intact.

This proves the core architecture.

---

# 131. TECH MILESTONE 2
# A LIVING BLOCK

Implement:

- 8×8 district
- 6 building types
- road graph
- power
- water
- 20 citizens
- jobs
- commute
- one faction pressure

Success:

The district operates for 100 Development Cycles without invalid state.

---

# 132. TECH MILESTONE 3
# MODE CONTINUITY

Implement:

- Founder Mode
- City Mode
- Command Mode
- same world
- seamless cameras
- one squad

Success:

No level reload or state duplication occurs during transitions.

---

# 133. TECH MILESTONE 4
# BUILDING UNDER ATTACK

Implement:

- structural modules
- cover
- damage
- disabled state
- capture
- repair
- ruin
- persistence

Building:
**Synthetic Fabrication Node**

---

# 134. TECH MILESTONE 5
# SIMULATION LOD

Implement:

- full citizen
- crowd citizen
- cohort
- remote city aggregate

Success:

A named citizen remains logically consistent through all LOD transitions.

---

# 135. TECH MILESTONE 6
# RIVAL BUILDS

Forgeweave AI:

- earns resources,
- chooses one of six cards,
- places valid structures,
- reacts to Resource Hunger.

Success:

Ironheart changes while player is elsewhere.

---

# 136. TECH MILESTONE 7
# WORLD EVENT

Implement:
**The Foundry Shortage**

It must update:

- market,
- Forgeweave AI,
- Eden diplomacy,
- factions,
- quest state.

---

# 137. TECH MILESTONE 8
# OPERATION IRON VEIL

Implement:

- streaming siege district
- Founder
- squads
- building capture
- sovereignty
- Daxton encounter
- aftermath persistence.

---

# 138. TECH MILESTONE 9
# ASCENSION

Implement:

- Relic sequence
- unlock Forgeweave cards
- new district architecture
- Daxton persistent state
- Replication Doctrine
- Autonomous Factory fusion.

Success:

The player's original city data model survives becoming a hybrid civilization.

---

# 139. TECH MILESTONE 10
# SAVE EVERYTHING

Save campaign.

Close application.

Reload.

Verify:

- exact city layout
- damage
- citizens
- jobs
- deck
- card histories
- diplomacy
- rival city changes
- world event
- Leader relationships
- conquest state
- hybrid architecture.

If this fails, the vertical slice is not complete.

---

# 140. PRODUCTION TEAM DISCIPLINES

Full production eventually needs:

- Game Design
- Systems Design
- Narrative Design
- Technical Design
- Gameplay Engineering
- Simulation Engineering
- AI Engineering
- Tools Engineering
- UI/UX
- Environment Art
- Character Art
- Technical Art
- Animation
- VFX
- Audio
- QA
- Production

The architecture is deliberately designed to let content creators work through validated data tools.

---

# 141. CONTENT SCALING RULE

Never scale to all 20 civilizations merely because the schema supports them.

Scale only after:

1. One Card → World loop works.
2. City simulation is stable.
3. Mode transitions work.
4. Combat affects persistent buildings.
5. Rival city simulation works.
6. One conquest persists.
7. One Ascension creates a hybrid city.
8. Save/reload is reliable.

Then civilization production becomes content scaling rather than systems invention.

---

# 142. ANTI-PATTERNS

Do not:

- hardcode card effects inside world actors,
- simulate every citizen as a full AI pawn,
- run city economy every frame,
- duplicate player and AI city rules,
- create separate battle-only fake buildings,
- use disconnected combat arenas for city sieges,
- reskin one architecture set twenty times,
- serialize entire scene actors,
- let procedural generation bypass gameplay validation,
- build multiplayer before the single-player loop works.

---

# 143. TECHNICAL DEFINITION OF THE GAME

From an engineering perspective:

> **DOMINION // ASCENDANT is a persistent data-driven civilization simulation in which collectible card instances instantiate durable 3D world entities that participate simultaneously in economic, social, tactical, political, and narrative systems across multiple simulation levels of detail.**

---

# 144. CORE RUNTIME FLOW

```text
PLAYER EARNS BLUEPRINT
        ↓
CRAFTS CARD INSTANCE
        ↓
ADDS CARD TO CITY DECK
        ↓
DRAWS CARD
        ↓
GRID SERVICE VALIDATES PLACEMENT
        ↓
WORLD ASSET INSTANCE CREATED
        ↓
CONSTRUCTION STATE MACHINE
        ↓
SIMULATION SYSTEMS REGISTER ASSET
        ↓
CITIZENS STAFF / USE IT
        ↓
ECONOMY + FACTIONS + UTILITIES UPDATE
        ↓
FOUNDER CAN VISIT IT
        ↓
COMBAT CAN DAMAGE / CAPTURE IT
        ↓
SAVE SYSTEM PERSISTS STATE
        ↓
CARD HISTORY RECORDS WHAT HAPPENED
```

That is the technical heart of the entire game.

---

# 145. DATA FLOW FOR CONQUEST

```text
RIVAL CITY SIMULATION
        ↓
DIPLOMACY / ECONOMY / CIVIC / MILITARY PRESSURE
        ↓
CONQUEST METER CHANGES
        ↓
SIEGE OR POLITICAL RESOLUTION
        ↓
LEADER RESOLUTION
        ↓
OWNERSHIP TRANSFER
        ↓
CARD POOL UNLOCK
        ↓
RELIC ACQUISITION
        ↓
DOMINION DOCTRINE
        ↓
FUSION DISCOVERY
        ↓
PLAYER CITY GAINS NEW DATA + NEW VISUAL KIT
```

No separate conquest game state is required.

---

# 146. FIRST COMPLETE TECHNICAL PROOF

The first genuine proof of the game is not:

“Can we render a city?”

It is:

> **Can one Adaptive Habitat card be owned, drawn, placed, constructed, staffed by persistent citizens, connected to utilities, generate city effects, become damaged during combat, be captured or repaired, survive streaming, survive save/reload, accumulate history, and remain the same card throughout?**

If yes, the foundational architecture works.

---

# 147. SECOND COMPLETE TECHNICAL PROOF

The second proof is:

> **Can Forgeweave build and operate a rival city using the same rules while the player is somewhere else, then physically present the evolved city when visited?**

If yes, the world simulation works.

---

# 148. THIRD COMPLETE TECHNICAL PROOF

The third proof is:

> **Can the player conquer Forgeweave and have that conquest permanently alter their cards, architecture, citizens, diplomacy, combat options, Leader roster, economy, and skyline without creating separate special-case systems?**

If yes, DOMINION // ASCENDANT exists as one coherent game.

---

# 149. TECHNICAL DESIGN MANTRA

# ONE CARD.
# ONE INSTANCE.
# ONE WORLD ENTITY.
# MANY SYSTEMS.
# PERSISTENT HISTORY.
# NO FAKE SEPARATION BETWEEN MODES.

---

# 150. NEXT DESIGN PASS

With the foundational game architecture defined through v0.5, the next canonical pass should become the **Content Production Bible**:

- all 20 civilization visual languages,
- architectural grammars,
- district archetypes,
- unit silhouettes,
- Leader visual direction,
- Founder equipment language,
- VFX language,
- construction animation language,
- material palettes,
- card-frame system,
- Wonder design briefs,
- world-region biomes,
- 3D asset tiering,
- reusable modular kit requirements,
- first 50 vertical-slice asset briefs,
- audio identity,
- art QA rules.

That pass converts systems into a concrete art/content production pipeline.
