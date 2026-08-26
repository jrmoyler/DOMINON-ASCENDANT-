# DOMINION // ASCENDANT
## Vertical Slice Production Specification — v1.1

**Status:** SCOPE FROZEN FOR FIRST PLAYABLE PRODUCTION MILESTONE

**Canonical predecessors:**

1. Game Bible v0.1
2. Playable Systems Bible v0.2
3. Combat, Siege & Conquest Bible v0.3
4. Citizen, Card Acquisition & World Campaign Bible v0.4
5. 3D Production & Technical Architecture Bible v0.5
6. Content Production Bible v0.6
7. UX, Onboarding & Player Experience Bible v0.7
8. Economy, Balance & Progression Bible v0.8
9. Narrative, Lore & Campaign Bible v0.9
10. Quest, Event & Content Authoring Bible v1.0

> **This document is not an idea list. It is the finite definition of the first build that must prove DOMINION // ASCENDANT works as one coherent game.**

---

# 1. VERTICAL SLICE PRODUCT DEFINITION

The vertical slice is a **single-player premium PC gameplay build** proving the complete loop:

> **Own Card → Draw Card → Place Asset → Construct City → House Citizens → Create Jobs → Produce Resources → Explore in Third Person → Negotiate → Fight → Conquer or Unite → Ascend → Return to a Permanently Changed City**

The slice contains exactly three major civilizations:

1. **Synara** — player founding civilization
2. **Forgeweave** — primary rival / first Ascension target
3. **Eden Circuit** — neutral-to-allied third power

The slice ends after the first Forgeweave Ascension and the reveal:

# CONVERGENCE AUTHORITY: 1/20

---

# 2. TARGET PLAY EXPERIENCE

Target first-playthrough length:

**6–10 hours**

A highly experienced player may complete it faster.

A completionist may spend longer exploring citizen stories, alternative economic routes, and diplomacy.

The vertical slice must feel like a complete regional campaign with a meaningful ending, not a technology demo.

---

# 3. TARGET PLATFORM

## IN SCOPE

**Windows PC**

Primary input:
- Mouse + Keyboard
- Xbox-style controller

## NOT REQUIRED FOR VERTICAL SLICE

- PlayStation
- Xbox console certification
- Switch
- mobile
- browser
- Steam Deck certification

The architecture may remain portable, but production does not spend scope on certification outside Windows.

---

# 4. ENGINE TARGET

Recommended production target:

# UNREAL ENGINE 5-GENERATION RUNTIME

Required engine capabilities:

- streamed 3D worlds
- third-person character gameplay
- data-driven content definitions
- behavior/state trees
- modular destruction
- crowd representation
- world partition or equivalent streaming
- asynchronous loading
- controller support
- cinematic sequencing
- robust save serialization

The gameplay schemas remain engine-agnostic.

---

# 5. CORE SUCCESS THESIS

The slice succeeds only if players understand and enjoy this statement through play:

> **Your deck is your city, your city is your economy and army, and every civilization you absorb permanently changes what your city can become.**

---

# 6. SCOPE LOCK — FEATURES IN

The following systems are REQUIRED.

## CARD / TCG

- Card collection
- Blueprints
- Card crafting
- 60-card Synara City Deck
- 7-card opening hand
- draw flow
- hand limit
- copy limits
- deck builder
- persistent Card Instances
- card provenance/history
- card-to-world placement transition
- card recovery after destruction

## CITY BUILDING

- 32×32 logical player grid
- 8-meter logical cells
- 4×4 districts
- roads
- power
- water
- data
- logistics-lite
- placement preview
- positive/negative adjacency
- district specialization
- construction states
- Level II upgrades
- selected Level III upgrades
- one Wonder

## ECONOMY

- Capital
- Insight
- Influence
- maintenance
- staffing
- commercial demand
- Production Throughput
- research
- policy costs
- military upkeep
- repair/reconstruction

## CITIZENS

- cohort simulation
- 20 named persistent citizens
- households
- housing
- jobs
- job match
- commuting
- Happiness
- Loyalty
- Education
- factions
- migration
- one Champion Ascension path

## RPG

- third-person Founder
- exploration
- interaction
- dialogue
- traversal
- combat
- one primary weapon/tool family
- one secondary
- one utility
- one traversal device
- three active ability slots

## COMMAND / COMBAT

- allied squads
- enemy squads
- cover
- line of sight
- elevation
- suppression
- morale
- Command Points
- Control Zones
- one vehicle per major faction
- building damage
- module damage
- capture
- surrender
- repair
- persistent ruins

## WORLD

- regional World Map
- three civilizations
- three minor settlements
- resource sites
- travel
- World Ticks
- rival Forgeweave city simulation
- trade
- diplomacy
- dynamic regional events

## CONQUEST

- Military Sovereignty
- Economic Autonomy
- Civic Legitimacy
- Alliance Readiness
- Force route
- Economic route
- Influence route
- Alliance route
- mixed-route support
- Leader resolution
- post-conquest Loyalty
- first Ascension

## NARRATIVE

- Founder Hall mystery
- Nia Vale arc
- Mira Vey relationship
- Daxton Rhe arc
- Amara Venn arc
- Foundry Shortage
- Operation Iron Veil
- Forgeweave Relic
- 1/20 Convergence reveal

---

# 7. SCOPE LOCK — FEATURES OUT

The following are explicitly NOT part of v1.1 production.

- playable civilizations beyond Synara
- full 20-civilization world
- Axiom Crown region
- Relics 2–20
- multiplayer
- PvP
- co-op
- online card trading
- marketplace
- monetization store
- live-service seasons
- romance system
- full procedural planet
- 128×128 player city
- multiple player capitals
- multi-front warfare
- 20 Governors
- full espionage system
- global coalition warfare
- full debt/credit simulation beyond scripted regional contract use
- all 300 faction core cards
- Mythic cards beyond one non-ownable narrative preview
- full class trees for all 20 civilizations
- every building interior
- fully simulated thousands of individual citizens
- console certification
- mod tools
- endgame constitutional endings
- New Game+
- photo mode
- multiplayer-ready UI
- live economy

Any addition from this list requires an explicit scope revision.

---

# 8. WORLD FOOTPRINT

The vertical slice contains four streamed playable zones plus one strategic World Map.

## ZONE A — SYNARA FRONTIER CAPITAL

Purpose:
player city-building and home hub.

Logical build grid:
**32×32 cells**

Physical build area:
**256 m × 256 m**

Surrounding explorable buffer:
approximately **512 m × 512 m**

Contains:
- Founder Hall
- player districts
- nearby Axiom utility tunnel
- transit departure point
- small wilderness perimeter

---

# 9. ZONE B — IRONHEART

Purpose:
persistent Forgeweave rival city and conquest target.

Logical simulation grid:
**32×32 cells**

Physical core:
**256 m × 256 m**

Explorable industrial envelope:
approximately **512 m × 512 m**

Required districts:

1. Worker Arcology District
2. Smelter Quarter
3. Freight Corridor
4. Production Directorate
5. Foundry Citadel
6. Grand Forge precinct

Operation Iron Veil occurs inside this same persistent city.

There is no separate disposable combat arena.

---

# 10. ZONE C — EDEN BASIN

Purpose:
diplomacy, environment, third-party systemic pressure.

Playable footprint:
approximately **384 m × 384 m**

Not player-buildable.

Contains:

- Garden Commune
- Ecological Design House
- Harvest Market
- Regenerative Bioworks
- Living Waterway
- restoration wetlands
- Amara's council site
- Ori Sen research station

---

# 11. ZONE D — NEUTRAL FRONTIER CORRIDOR

Purpose:
exploration, first combat, contested resource site.

Playable footprint:
approximately **512 m × 256 m**

Contains:

- Tal Arden settlement
- contested mineral ridge
- abandoned Axiom node
- broken transport infrastructure
- neutral roadside market
- first Command Mode encounter

---

# 12. MINOR SETTLEMENTS

Exactly three:

## 1. ARDEN RESERVOIR
Leader:
Tal Arden.

Dependency:
water.

## 2. ORE STATION SEVEN
Dependency:
Forgeweave industrial employment.

## 3. RIVER CROSSING
Dependency:
Eden water and regional trade.

These settlements are not conquest civilizations.

---

# 13. WORLD MAP

The strategic World Map includes only:

- Synara Frontier
- Ironheart
- Eden Basin
- Arden Reservoir
- Ore Station Seven
- River Crossing
- contested mineral ridge
- primary trade corridors

Required overlays:

- Political
- Economic
- Military
- Civic

Axiom overlay is limited to the first discovered node and becomes meaningful only at the ending.

---

# 14. VERTICAL SLICE TIME MODEL

Canonical:

**1 Development Cycle = 30 seconds active simulation**

**1 World Tick = 5 Development Cycles**

The slice must support:

- tactical pause
- menu pause
- fast-forward up to 4× while in safe City Mode
- no fast-forward during active combat or critical scripted sequences

---

# 15. EXACT CARD CONTENT COUNT

The vertical slice contains:

- **15 Synara faction definitions**
- **15 Forgeweave faction definitions**
- **15 Eden Circuit faction definitions**
- **17 Universal definitions**
- **1 Fusion definition**
- **1 Founder Hall special definition**

Total:

# 64 PLAYABLE/PERSISTENT CONTENT DEFINITIONS

Not every definition is immediately owned.

---

# 16. SYNARA DEFINITIONS — 15

1. Adaptive Habitat
2. Civic Autonomy Pods
3. Cognitive Operations Tower
4. Predictive Bureau
5. Autonomous Exchange
6. Algorithmic Market
7. Synthetic Fabrication Node
8. Swarm Foundry
9. Neural Relay
10. Orchestration Hub
11. Guardian Drone Cohort
12. Audit Sentinel
13. Agency Forum
14. Archon Mira Vey
15. The Thinking Spire

---

# 17. FORGEWEAVE DEFINITIONS — 15

1. Worker Arcology
2. Forge Quarters
3. Production Directorate
4. Industrial Design Bureau
5. Industrial Exchange
6. Machine Parts Market
7. Infinite Foundry
8. Replication Forge
9. Freight Furnace
10. Smog Reclaimer
11. Forge Guard
12. Mechanist Crew
13. Workers' Canteen
14. Forge Lord Daxton Rhe
15. The Grand Forge

---

# 18. EDEN CIRCUIT DEFINITIONS — 15

1. Garden Commune
2. Canopy Habitat
3. Ecological Design House
4. Restoration Bureau
5. Harvest Market
6. Seed Exchange
7. Regenerative Bioworks
8. Mycelium Works
9. Living Waterway
10. Pollinator Corridor
11. Ranger Circle
12. Symbiosis Keepers
13. Balance Grove
14. Caretaker Amara Venn
15. The Worldgarden

---

# 19. UNIVERSAL DEFINITIONS — 17

1. Compact Residence
2. Civic Apartments
3. Corner Exchange
4. District Market
5. Field Workshop
6. Utility Fabricator
7. Administrative Office
8. Research Annex
9. Public Clinic
10. Community Plaza
11. Microgrid Station
12. Water Reclaimer
13. Transit Stop
14. Warehouse
15. Founder Monument
16. Watch Post
17. Barrier Hub

---

# 20. FUSION DEFINITION — 1

# AUTONOMOUS FACTORY

Sources:
Synara + Forgeweave.

Required to prove the hybrid civilization thesis.

---

# 21. SPECIAL DEFINITION — 1

# FOUNDER HALL

Not part of the 60-card deck.

Always placed at campaign start.

Functions:

- deck management
- governance
- Relic chamber
- Doctrine management
- Leader meetings
- campaign planning
- city overview
- Ascension transformation

---

# 22. EXACT 60-CARD SYNARA STARTER DECK

## Residential — 12

- 3× Adaptive Habitat
- 3× Civic Autonomy Pods
- 3× Compact Residence
- 3× Civic Apartments

## Economy / Retail — 9

- 3× Autonomous Exchange
- 2× Algorithmic Market
- 2× Corner Exchange
- 2× District Market

## Office / Research — 8

- 3× Cognitive Operations Tower
- 2× Predictive Bureau
- 2× Research Annex
- 1× Administrative Office

## Industrial — 7

- 3× Synthetic Fabrication Node
- 2× Swarm Foundry
- 1× Field Workshop
- 1× Utility Fabricator

## Infrastructure — 10

- 3× Neural Relay
- 2× Orchestration Hub
- 2× Microgrid Station
- 1× Water Reclaimer
- 1× Transit Stop
- 1× Warehouse

## Civic — 6

- 3× Agency Forum
- 1× Community Plaza
- 1× Public Clinic
- 1× Founder Monument

## Defense / Units — 6

- 2× Guardian Drone Cohort
- 2× Audit Sentinel
- 1× Watch Post
- 1× Barrier Hub

## Leader — 1

- 1× Archon Mira Vey

## Wonder — 1

- 1× The Thinking Spire

**TOTAL: 60**

---

# 23. CARD ACQUISITION DURING SLICE

The player begins with the full 60-card starter deck.

During the campaign the player may additionally unlock:

- 6 Synara Blueprint upgrades
- 3 Eden support cards
- 4 Universal discovery rewards
- 15 Forgeweave definitions after Ascension
- 1 Champion Card
- 1 Dominion Doctrine
- 1 Fusion Blueprint

The Forgeweave card pool unlock occurs at the end; the slice only needs to prove collection/deck access after Ascension, not a second full conquest using them.

---

# 24. EXACT EDEN SUPPORT REWARDS

The three Eden cards the player can acquire before Ascension are:

1. Pollinator Corridor
2. Symbiosis Keepers
3. Balance Grove

They are obtained through diplomacy/quests, not random caches.

---

# 25. EXACT DISCOVERY REWARDS

Four Universal rewards can be earned through exploration:

1. Public Clinic
2. Warehouse
3. Founder Monument
4. Barrier Hub

---

# 26. CHAMPION CARD

Exactly one citizen-to-Champion path must be fully implemented:

# NIA VALE — HUMAN OVERRIDE

The player does not begin with this card.

It is earned only through the Nia storyline.

---

# 27. DOMINION REWARD

Forgeweave Ascension unlocks:

# REPLICATION

Vertical-slice implementation:

Once every **12 Development Cycles**, the player may duplicate one eligible non-Legendary owned Asset Card.

Eligibility and cost must use the Card Instance system.

---

# 28. FUSION REWARD

# AUTONOMOUS FACTORY

Initial tuning:

- -80% workforce requirement
- +25% Industrial Throughput
- +15% adjacent Industrial construction speed
- high Power and Data demand
- Dependency +0.20/cycle
- Resource Hunger +0.15/cycle

The slice ends after the player can inspect and optionally place one.

---

# 29. CITIZEN SIMULATION SCOPE

At campaign start:

# 200 TOTAL SIMULATED RESIDENTS ACROSS REGION

Distribution:

- Synara Frontier: 24
- Ironheart: 90
- Eden Basin: 50
- Arden Reservoir: 12
- Ore Station Seven: 12
- River Crossing: 12

Population can change through migration/events.

Named citizens are included within these populations, not added on top.

---

# 30. NAMED CITIZENS — EXACTLY 20

## Synara Frontier — 7

1. Nia Vale — junior systems technician
2. Jalen Orr — utility engineer
3. Tomas Rell — Corner Exchange operator
4. Maelin Qu — Agency Forum organizer
5. Suri Kade — research analyst
6. Dev Arlen — transit technician
7. Ivo Renn — Guardian Drone controller

## Ironheart — 6

8. Mara Kest — maintenance foreman / worker organizer
9. Darek Vol — foundry chief engineer
10. Sora Pell — industrial medic
11. Bren Tal — freight dispatcher
12. Yara Vennik — young Forge Guard veteran
13. Olan Grest — machine-parts merchant

## Eden Basin — 4

14. Ori Sen — hydrologist
15. Luma Rei — restoration technician
16. Kiran Moss — regenerative farmer
17. Tessa Vahl — ecological council clerk

## Neutral Settlements — 3

18. Tal Arden — reservoir settlement elder
19. Jori Pell — Ore Station Seven coordinator
20. Sera Noll — River Crossing trader

---

# 31. FULL CITIZEN STORYLINES REQUIRED

Exactly four must receive full multi-stage authored story content:

1. Nia Vale
2. Mara Kest
3. Ori Sen
4. Tal Arden

The remaining sixteen named citizens require:

- persistence
- schedules
- contextual dialogue
- relationship/state reactions
- at least one optional micro-event each

They do not require full branching story arcs.

---

# 32. FACTIONS REQUIRED

## Synara

1. Ascendants
2. Human Agency League
3. Synthetic Rights Front
4. Practical Moderates

## Forgeweave

5. Forge Workers Coalition
6. Production Houses
7. Expansion Bloc
8. Sustainable Industry Caucus

## Eden Circuit

9. Restoration Council
10. Deep Preservationists
11. Regenerative Pragmatists

Total:
# 11 FACTIONS

Only Synara's four require full player-facing management UI in the slice.

Forgeweave/Eden factions primarily affect diplomacy and conquest state.

---

# 33. CLASS CONTENT REQUIRED

Fully implemented class lineages:

## Synara

- Systems Architect
- Swarm Marshal
- Intelligence Auditor
- Orchestrator
- Agency Mediator
- Protocol Guardian

## Forgeweave

- Fabricator
- Industrialist
- Mechanist
- Forge Commander

## Eden

- Cultivator
- Ecosystem Engineer
- Ranger
- Symbiosis Keeper

Only Synara needs complete citizen progression from Initiate through Master during the slice.

Ascendant rank is represented by Nia's Champion path.

---

# 34. EXACT QUEST SET — 25

All twenty-five v1.0 quests are in scope.

1. Wake the Hall
2. A Place to Stay
3. Power, Water, People
4. Nia Needs a Job
5. The Replacement Model
6. Agency Has a Price
7. Signal in the Foundation
8. Iron at the Border
9. The Basin Speaks
10. Tal's Reservoir
11. The Frontier Claim
12. Hold the Ridge
13. The First Contract
14. Mara's Numbers
15. The Green Line
16. The Empty Shift
17. The Price of Silence
18. Human Override
19. The Foundry Shortage
20. Broker of Ironheart
21. The Workers' Signal
22. The Supply Noose
23. Operation Iron Veil
24. The Third Foundry
25. Convergence Authority

Not all conquest-route quests appear in one playthrough.

---

# 35. REQUIRED QUEST BRANCHING

The campaign must support at least four valid Forgeweave resolutions:

## FORCE
Operation Iron Veil.

## ECONOMIC
The Supply Noose.

## INFLUENCE
The Workers' Signal.

## ALLIANCE
The Third Foundry.

A mixed route may transition between them before final resolution.

---

# 36. WORLD EVENTS REQUIRED

The vertical slice implements exactly six systemic events from the v1.0 library:

1. The Foundry Shortage
2. Grid Strain
3. Housing Surge
4. The Green Line
5. Corridor Failure
6. Migration Wave

Other v1.0 events remain design-canon but out of production scope.

---

# 37. LEADER CONTENT REQUIRED

Exactly three major Leaders are fully represented:

## ARCHON MIRA VEY

Roles:
- starting political authority
- tutorial/city evaluator
- Axiom mystery witness

Required content:
- Founder introduction
- city evaluation
- Nia/automation reactions
- first Ascension reaction

No full combat boss.

---

## FORGE LORD DAXTON RHE

Roles:
- primary rival
- economic partner
- conquest target
- boss/negotiation endpoint

Required:
- production-floor introduction
- Foundry Shortage scenes
- Mara confrontation
- three-phase final encounter
- six possible post-conflict states

---

## CARETAKER AMARA VENN

Roles:
- third-party diplomatic power
- ecological counterweight
- Alliance-route partner

Required:
- Eden introduction
- Green Line debate
- Foundry Shortage response
- Third Foundry crisis
- Ascension reaction

---

# 38. DAXTON FINAL ENCOUNTER — EXACT PHASES

## PHASE I — THE FORGE LORD

Required mechanics:
- powered industrial armor
- Forge Guard reinforcement
- deployable hardened cover
- Grand Forge production

Completion:
reduce encounter state to 60% or achieve equivalent non-damage objective.

---

## PHASE II — OVERDRIVE

Required mechanics:
- production speed increase
- Heat meter
- Coolant stability
- Resource Hunger escalation
- multiple valid system interactions

Player may:
- attack
- disable coolant
- redirect supply
- hack production
- trigger worker shutdown

---

## PHASE III — THE CHOICE

At least four outcomes must be playable:

1. Defeat Daxton
2. Save the Grand Forge
3. Evacuate workers
4. Stabilize production / offer union

The encounter cannot end solely because a health bar reaches zero.

---

# 39. DAXTON POST-CONFLICT STATES

Supported:

1. Governor
2. Industrial Advisor
3. Allied Forge Lord
4. Exile
5. Prisoner
6. Dead

All six require persistent save states.

At least four must be reachable in a normal first campaign.

---

# 40. MIRA RELATIONSHIP AXES

Track:

- Trust
- Respect
- Concern

Mira is intentionally not recruitable in the vertical slice.

---

# 41. DAXTON RELATIONSHIP AXES

Track:

- Trust
- Respect
- Grievance

These directly control available final outcomes.

---

# 42. AMARA RELATIONSHIP AXES

Track:

- Trust
- Respect
- Ecological Grievance

---

# 43. NARRATIVE HISTORY TAGS

At minimum, implement history tags for:

- founder_hall_awake
- nia_jobs_preserved
- nia_automation_accepted
- nia_model_audited
- agency_petition_supported
- agency_petition_rejected
- tal_reservoir_saved
- forge_trade_supported
- eden_watershed_supported
- mara_evidence_exposed
- mara_evidence_suppressed
- workers_protected
- ironheart_civilian_damage
- grand_forge_preserved
- daxton_governor
- daxton_exiled
- daxton_dead
- forgeweave_allied
- forgeweave_forced
- forgeweave_economic_union
- forgeweave_influence_transfer
- first_relic_acquired

---

# 44. PROMISES REQUIRED

The vertical slice must support explicit persistent promises.

Required promise examples:

- Protect Forgeweave workers.
- Preserve the Eden watershed.
- Avoid military escalation.
- Deliver industrial components.

Before breaking a promise, the UI warns the player.

---

# 45. ECONOMY TARGETS

Use v0.8 Standard difficulty baseline.

Starting:

- Capital 40
- Insight 12
- Influence 8
- Population 24

First-hour target:

- 30–40 population
- 15–40 Capital reserve
- 4–12 Insight reserve
- 4–10 Influence reserve
- 8–14 player-placed assets

---

# 46. PLAYER CITY BUILD LIMIT

Vertical slice hard support target:

# 50 PERSISTENT PLAYER-PLACED WORLD ASSETS

This is not a deck limit.

The player may decommission/rebuild.

The grid can physically support more, but production performance acceptance is based on 50 major placed assets plus roads/utilities/props.

---

# 47. IRONHEART ASSET TARGET

Ironheart must contain:

- 35–45 persistent gameplay-relevant building assets
- 25–40 decorative/support structures
- road/freight network
- 90 simulated residents
- 6 named citizens
- 3–5 active combat squads during siege phases

---

# 48. EDEN ASSET TARGET

Eden Basin:

- 15–20 gameplay-relevant structures
- ecological infrastructure
- 50 simulated residents
- 4 named citizens
- no full player siege

---

# 49. ROAD / UTILITY SYSTEMS

Required:

## Roads
- straight
- corner
- T-junction
- four-way intersection

## Power
- generation source
- local capacity
- deficit
- outage

## Water
- source
- capacity
- deficit

## Data
- Synara network coverage
- disruption

## Logistics
Vertical slice uses simplified facility-to-facility connection graph.

No full global freight simulation.

---

# 50. DISTRICT SPECIALIZATION

Exactly five specialization types need UI and gameplay support:

1. Residential
2. Commercial
3. Research
4. Industrial
5. Civic

Defense/Eco/Transit specializations remain future scope.

Eden ecological behavior uses civilization scripting rather than player district specialization.

---

# 51. BUILDING UPGRADES

Required:

- Level I
- Level II two-branch choice
- selected Level III proof on five cards

Five Level III-capable cards:

1. Adaptive Habitat
2. Cognitive Operations Tower
3. Autonomous Exchange
4. Synthetic Fabrication Node
5. Agency Forum

The slice does not require Level III art for every definition.

---

# 52. CONSTRUCTION STATES

Every placeable building must support:

1. Preview
2. Foundation
3. Frame
4. Shell
5. Systems
6. Operational
7. Damaged
8. Disabled
9. Ruined
10. Reconstructing

Art complexity may vary by tier.

---

# 53. DESTRUCTION SCOPE

Full module-based destruction is required for exactly eight structures:

1. Synthetic Fabrication Node
2. Swarm Foundry
3. Infinite Foundry
4. Replication Forge
5. Smog Reclaimer
6. Freight Furnace
7. Grand Forge
8. Autonomous Factory

All other buildings require:
- generic damage material
- Disabled state
- Ruin state

---

# 54. BUILDING CAPTURE

Capture interactions required for:

- Infinite Foundry
- Replication Forge
- Freight Furnace
- Smog Reclaimer
- one Worker Arcology
- Production Directorate

Post-capture choices:

- Preserve
- Convert
- Study
- Salvage
- Gift where scenario permits

---

# 55. FOUNDER COMBAT SCOPE

Required Founder combat:

- light attack/fire
- heavy/alternate attack
- dodge
- sprint
- interact
- one ranged primary
- one secondary utility weapon/tool
- one deployable device
- one traversal tool
- three active abilities
- one Ultimate
- Health
- Guard
- Stamina
- Tactical Charge

No giant weapon catalog.

---

# 56. SYNARA FOUNDER LOADOUT

Initial:

### Primary
Adaptive Pulse Carbine

### Secondary
Arc Disruptor

### Utility
Command Drone

### Traversal
Tactical Relay Grapple

### Abilities
1. Precision Scan
2. Drone Barrier
3. Orchestration Mark

### Ultimate
Coordinated Override

---

# 57. ENEMY COMBAT ARCHETYPES

Exactly six Forgeweave combat archetypes:

1. Forge Guard Rifle
2. Forge Guard Heavy
3. Breach Worker
4. Mechanist Support
5. Industrial Drone
6. Daxton Command Guard

Variants can share rigs/equipment.

---

# 58. ALLIED UNIT ARCHETYPES

Exactly four initially controllable allied archetypes:

1. Guardian Drone Cohort
2. Audit Sentinel
3. Survey Team
4. Symbiosis Keepers after Eden reward

---

# 59. VEHICLES

Three required drivable/commandable vehicle definitions:

1. Synara Command Vehicle
2. Forgeweave Heavy Carrier
3. Eden Terrain Utility Rover

Only Synara Command Vehicle must support direct Founder driving in the slice.

Others may be AI/Command controlled.

---

# 60. COMMAND MODE CAP

Player can directly command:

- maximum 3 squads
- maximum 1 vehicle

Additional forces use AI doctrine.

Required orders:

- Move
- Attack
- Hold
- Defend
- Capture
- Escort
- Retreat
- Ability

---

# 61. COVER

Required cover types:

- Partial
- Full
- Hardened
- Destructible

Cover must be generated from:

- authored cover sockets
- deployable barriers
- selected ruins

No requirement for fully dynamic cover analysis across every decorative object.

---

# 62. MORALE

Required states:

- Inspired
- Steady
- Shaken
- Breaking
- Rout/Surrender

Forgeweave squads must be capable of surrendering.

---

# 63. SUPPLY

Operation Iron Veil tracks Supply 0–100.

Sources:

- captured Freight Corridor
- Synara vehicle
- forward control zone
- strategic logistics state

Low Supply must reduce:
- reinforcement
- repair
- Morale

---

# 64. CONQUEST METERS

Required exact meters:

- Military Sovereignty
- Economic Autonomy
- Civic Legitimacy
- Alliance Readiness

Diplomatic Resolve exists internally but Alliance Readiness is the player-facing peaceful-union metric for this slice.

---

# 65. FORCE ROUTE ACCEPTANCE

Force route can finish only if:

- Military Sovereignty reaches 0 or conditional surrender threshold
- Daxton encounter resolves
- Ironheart ownership state changes
- post-war Loyalty calculated
- damaged buildings persist after return

---

# 66. ECONOMIC ROUTE ACCEPTANCE

Economic route can finish only if:

- player controls required freight/component leverage
- Economic Autonomy reaches threshold
- actual trade contracts contributed
- Daxton restructuring encounter occurs
- no generic “purchase Forgeweave” button exists

---

# 67. INFLUENCE ROUTE ACCEPTANCE

Influence route can finish only if:

- worker faction endorsement exists
- at least one service crisis was resolved
- Civic Legitimacy reaches threshold
- civilian support comes from real persistent actions
- stored Influence alone cannot complete route

---

# 68. ALLIANCE ROUTE ACCEPTANCE

Alliance route requires:

- Trust threshold
- Respect threshold
- no unresolved Major Grievance
- Third Foundry complete
- joint crisis successful
- voluntary Relic transfer

---

# 69. FIRST ASCENSION

Required sequence:

1. Forgeweave systems halt/react.
2. Forge Relic emerges.
3. world transit sequence.
4. Founder Hall receives Relic.
5. Forgeweave card pool unlocks.
6. Replication unlocks.
7. Daxton state resolves.
8. Autonomous Factory unlocks.
9. capital architecture changes.
10. hidden chamber opens.
11. `CONVERGENCE AUTHORITY: 1/20`
12. end-of-slice campaign state saves successfully.

---

# 70. UI SCREENS — EXACT SET

Required full screens/panels:

1. Main Menu
2. New Campaign
3. Settings
4. Accessibility
5. Pause
6. City HUD
7. Founder HUD
8. Command HUD
9. Card Collection
10. Deck Builder
11. Card Inspect / 3D Inspect
12. Blueprint / Crafting
13. Building Inspect
14. Citizen Inspect
15. Faction Panel
16. Diplomacy
17. Treaty Builder
18. World Map
19. Quest Journal
20. History / Timeline
21. Research
22. City Metrics
23. Conquest Dashboard
24. Leader Resolution
25. Ascension Reward Sequence
26. Save / Load
27. Returning Player Recap

Total:
# 27 PLAYER-FACING UI SURFACES

---

# 71. OVERLAYS REQUIRED

City overlays:

1. Power
2. Water
3. Data
4. Employment
5. Housing
6. Happiness
7. Dependency
8. Adjacency

No pollution/equality/wealth overlay required as dedicated map layers in the vertical slice; those values remain inspectable through panels.

---

# 72. ACCESSIBILITY REQUIRED

- full key rebinding
- controller remapping where platform permits
- scalable text
- subtitles
- speaker labels
- subtitle background
- color-independent faction markers
- color-vision presets
- camera shake slider
- FOV slider
- motion blur toggle
- reduced flash
- full Tactical Pause option
- hold/toggle options
- aim assist
- build snap strength
- tutorial recall
- simple/advanced tooltips

---

# 73. AUDIO SCOPE

Required minimum:

## Music — 9 cues

1. Opening / Founder Arrival
2. Synara City Day
3. Synara Tension
4. Forgeweave Region
5. Eden Basin
6. Frontier Combat
7. Iron Veil
8. Daxton / Grand Forge
9. Ascension / Convergence

## Ambient Loops — 12

- Synara civic
- Synara industrial
- Synara night
- Forgeweave industrial
- Forgeweave worker district
- Forgeweave Grand Forge
- Eden wetland
- Eden settlement
- Frontier wilderness
- neutral settlement
- Axiom tunnel
- post-Ascension capital

## Core SFX Families

- UI
- card
- construction
- utilities
- citizen
- Founder movement
- weapons
- units
- vehicles
- damage
- capture
- Relic

Minimum acceptance:
**60 distinct authored SFX events** before variation layers.

---

# 74. VFX SCOPE

Required authored VFX families:

1. Synara placement
2. Synara construction
3. Synara scan
4. Synara shield/barrier
5. Synara drone deployment
6. Synara network disruption
7. Forgeweave construction
8. Forgeweave furnace
9. Forgeweave welding
10. Forgeweave Overdrive
11. Forgeweave structural breach
12. Eden construction/growth
13. Eden regeneration
14. Eden water restoration
15. damage sparks
16. fire
17. smoke
18. arc damage
19. cyber disruption
20. capture
21. building disabled
22. ruin transition
23. Autonomous Factory fusion
24. Forge Relic awakening
25. Ascension transfer

Minimum:
# 25 CORE VFX DEFINITIONS

---

# 75. ART ASSET SCOPE

The fifty primary production assets defined in v0.6 remain required.

Additionally required environment kit:

- 12 road modules
- 8 sidewalk modules
- 6 intersection pieces
- 8 street lights
- 12 industrial props
- 12 civic props
- 12 ecological props
- 8 utility props
- 10 clutter sets
- 8 foliage families
- 6 rock families
- 6 water-edge modules
- 8 ruin modules
- 8 barrier/fence families
- 6 freight props

---

# 76. CHARACTER ASSETS REQUIRED

High-fidelity:

1. Founder base
2. Mira Vey
3. Daxton Rhe
4. Amara Venn
5. Nia Vale
6. Mara Kest
7. Ori Sen
8. Tal Arden

Mid-detail persistent named NPC kit:

- supports remaining 12 named citizens through modular faces/hair/clothing

Crowd kit:

- 24 base body/head combinations
- faction clothing variations
- age/role variations

---

# 77. ANIMATION SCOPE

Founder:

- locomotion
- sprint
- dodge
- aim/fire
- utility
- traversal
- interaction
- basic melee
- downed/revive

Squads:

- locomotion
- cover
- fire
- suppression
- surrender
- repair
- capture

Characters:

- conversation idles
- gesture library
- profession-specific ambient actions

Daxton:
- bespoke boss set

Minimum animation reuse must prioritize rigs over one-off bespoke sequences.

---

# 78. INTERIOR SCOPE

Tier A full interiors:

1. Founder Hall
2. Grand Forge command/boss area
3. Eden Council/Restoration Hall

Tier B modular interiors:

4. Cognitive Operations Tower
5. Agency Forum
6. Production Directorate
7. Industrial Exchange
8. Ecological Design House

All other structures:
facade + entry/limited lobby only.

---

# 79. SAVE SYSTEM ACCEPTANCE

The campaign save must preserve:

- exact player city layout
- Card Instances
- card histories
- deck
- resource balances
- construction state
- upgrades
- damage
- ruins
- citizens
- named citizen history
- jobs
- factions
- migration state
- relationships
- promises
- quests
- world events
- trade contracts
- Forgeweave city growth
- conquest meters
- Daxton state
- Relic state
- post-Ascension architecture

---

# 80. SAVE VERSIONING

Every save contains:

- schema version
- content version
- build version

At least one automated migration test from an older internal schema to current must exist before vertical-slice signoff.

---

# 81. SIMULATION LOD SCOPE

Required levels:

## LOD 0 — Full
Founder district / combat.

## LOD 1 — Detailed
nearby districts.

## LOD 2 — Aggregated
distant active city.

## LOD 3 — Strategic
unloaded major regions.

A named citizen must remain logically consistent across all transitions.

---

# 82. RIVAL AI SCOPE

Forgeweave AI must independently:

- earn Capital
- consume Production Throughput
- construct from a six-card build pool
- repair damage
- respond to Resource Hunger
- change trade posture
- strengthen defense if threatened

Required autonomous build pool:

1. Worker Arcology
2. Production Directorate
3. Industrial Exchange
4. Infinite Foundry
5. Freight Furnace
6. Smog Reclaimer

---

# 83. EDEN AI SCOPE

Eden does not need full autonomous city-building parity.

Required simulation:

- Ecology
- water state
- trade
- diplomacy
- event responses
- selected facility state changes

This is a deliberate scope reduction.

---

# 84. FOUNDER HALL EVOLUTION

Three visual states:

## STATE 1 — DORMANT
campaign opening.

## STATE 2 — SYNARA ACTIVE
after activation.

## STATE 3 — SYNARA + FORGEWEAVE
after first Ascension.

The third state must physically show Forgeweave integration.

---

# 85. RELIC ROOM

The Founder Hall requires:

- one empty 20-position architectural system
- only first slot active in slice
- Forge Relic physical asset
- hidden chamber
- Axiom display

The other nineteen slots can remain abstract/inactive silhouettes without final art.

---

# 86. CARD-TO-WORLD TECH PROOF

Adaptive Habitat must support the complete lifecycle:

1. owned
2. visible in Collection
3. added to Deck
4. drawn
5. selected
6. placement validated
7. constructed
8. staffed
9. utility-connected
10. upgraded
11. damaged
12. repaired
13. streamed out
14. streamed in
15. saved
16. reloaded
17. decommissioned
18. returned to collection
19. redeployed
20. history preserved

If any step requires a duplicate special-case asset, architecture is not accepted.

---

# 87. FIRST 60-MINUTE UX ACCEPTANCE

A new tester without developer coaching must be able to:

- move Founder
- activate Founder Hall
- place Adaptive Habitat
- understand utilities
- house residents
- employ Nia
- generate Capital
- encounter Dependency
- research with Insight
- understand Influence
- meet Forgeweave/Eden
- complete first combat
- command Guardian Drones

without external explanation.

---

# 88. PERFORMANCE TARGET

Reference performance target:

**60 FPS at 1920×1080, High preset**

Reference-class PC:

- Ryzen 5 5600 or equivalent
- RTX 3060 / RX 6700 XT-class GPU
- 16 GB system RAM
- SSD

Fallback quality target:

**30 FPS** under worst-case high-density city/combat load.

---

# 89. PERFORMANCE SCENE

Benchmark scene:

Synara Capital with:

- 50 placed gameplay assets
- 120 visible/represented citizens across LODs
- weather active
- 3 player squads
- 3 enemy squads
- 1 vehicle
- construction effect
- one damaged building
- City → Command → Founder transition

No Development Cycle boundary may create a frame hitch exceeding the production-defined severe-hitch threshold.

---

# 90. STREAMING ACCEPTANCE

Required:

- City ↔ Founder camera transitions without level reload
- district streaming without missing collision
- named NPC persistence
- no duplicate World Assets
- no visible empty city block for more than transient streaming tolerance
- travel between regions preserves prior state

---

# 91. INPUT ACCEPTANCE

All campaign-critical actions must be completable with:

## Mouse + Keyboard

and

## Controller

No required function may depend on hover-only UI.

---

# 92. TECHNICAL MILESTONE A
# ONE CARD, ONE BUILDING

Deliver:

- CardDefinition
- CardInstance
- Deck
- Grid
- Placement
- Construction
- WorldAsset
- Save/Reload

Asset:
Adaptive Habitat.

Exit criterion:
full lifecycle test passes.

---

# 93. TECHNICAL MILESTONE B
# LIVING BLOCK

Deliver:

- 8×8 district
- utilities
- roads
- housing
- jobs
- 20 citizens
- Capital
- Insight
- Dependency

Exit:
100 Development Cycles with no invalid state.

---

# 94. TECHNICAL MILESTONE C
# THREE MODES

Deliver:

- Founder Mode
- City Mode
- Command Mode
- seamless transitions
- one squad

Exit:
same objects persist through all views.

---

# 95. TECHNICAL MILESTONE D
# PERSISTENT DAMAGE

Deliver:

- building modules
- damage
- Disabled
- Ruin
- repair
- capture

Exit:
Synthetic Fabrication Node survives save/reload in every state.

---

# 96. TECHNICAL MILESTONE E
# CITIZEN SYSTEM

Deliver:

- 20 named
- cohorts
- homes
- jobs
- commute
- factions
- Nia story state

Exit:
named citizen remains coherent through simulation LOD and save/load.

---

# 97. TECHNICAL MILESTONE F
# REGIONAL WORLD

Deliver:

- World Map
- Ironheart
- Eden Basin
- settlements
- travel
- trade
- diplomacy

Exit:
regions change while unloaded.

---

# 98. TECHNICAL MILESTONE G
# RIVAL BUILDS

Deliver Forgeweave strategic AI.

Exit:
Ironheart evolves for 100 World Ticks without:
- invalid placement
- impossible utility state
- economic deadlock

---

# 99. TECHNICAL MILESTONE H
# SYSTEMIC CRISIS

Deliver:
The Foundry Shortage.

Exit:
event changes:
- price
- diplomacy
- Resource Hunger
- Eden response
- quest availability

without hardcoded fake values detached from systems.

---

# 100. TECHNICAL MILESTONE I
# CONQUEST

Deliver all four routes.

Exit:
four separate saved campaign runs can resolve Forgeweave via each method.

---

# 101. TECHNICAL MILESTONE J
# ASCENSION

Deliver:

- Relic
- Founder Hall transformation
- Replication
- Forgeweave cards
- Leader state
- Autonomous Factory
- 1/20 reveal

Exit:
save/reload after Ascension reconstructs exact new world state.

---

# 102. PRODUCTION SEQUENCE

Production order is locked:

1. Data model
2. Card lifecycle
3. Grid and building
4. Utilities/economy
5. Founder movement
6. Citizen simulation
7. three-mode continuity
8. combat
9. destruction/capture
10. World Map/travel
11. rival AI
12. diplomacy/trade
13. quests/events
14. conquest
15. Daxton encounter
16. Ascension
17. content polish
18. accessibility
19. performance optimization
20. full regression/signoff

Do not start broad civilization content production before the foundation passes its gates.

---

# 103. CONTENT PRODUCTION SEQUENCE

## ART WAVE 1
- Adaptive Habitat
- Synthetic Fabrication Node
- Founder
- Nia
- basic Synara kit

## ART WAVE 2
- Forgeweave kit
- Infinite Foundry
- Daxton
- Forge Guard

## ART WAVE 3
- Eden kit
- Garden Commune
- Amara
- Ori

## ART WAVE 4
- Wonders
- Grand Forge
- Thinking Spire
- Worldgarden

## ART WAVE 5
- Autonomous Factory
- Ascension transformation
- final damage states
- polish variants

---

# 104. NARRATIVE PRODUCTION SEQUENCE

1. First-hour dialogue
2. Nia arc
3. Forgeweave introduction
4. Eden introduction
5. Foundry Shortage
6. Mara/Ori/Tal stories
7. four conquest routes
8. Daxton ending states
9. Ascension
10. post-Ascension reactions

---

# 105. UI PRODUCTION SEQUENCE

1. Founder HUD
2. City HUD
3. Card hand
4. placement
5. building inspect
6. citizens/jobs
7. resource/tooltips
8. deck builder
9. collection
10. research
11. World Map
12. diplomacy
13. factions
14. Command HUD
15. conquest dashboard
16. quest/history
17. Leader resolution
18. Ascension
19. accessibility/settings
20. returning-player recap

---

# 106. RISK REGISTER

| Risk | Severity | Mitigation | Exit Criterion |
|---|---|---|---|
| Hybrid genre feels fragmented | Critical | One-world entity model; seamless modes | Testers describe systems as connected |
| Card/world state diverges | Critical | Single source of truth | Adaptive Habitat lifecycle passes |
| Save corruption | Critical | Transactional saves + backups + versioning | 100 save/load cycles without loss |
| Citizen simulation too expensive | High | Cohorts + simulation LOD | Performance target met with population load |
| Rival AI produces invalid city | High | validator + limited build pool | 100 World Tick soak passes |
| Combat feels detached | High | use real city assets/objectives | Iron Veil damage persists afterward |
| Conquest routes are not equally viable | High | dedicated route acceptance tests | Four route completion runs pass |
| Alliance becomes “nice dialogue” | High | hard systemic requirements | Cannot complete without Third Foundry conditions |
| Economy snowballs | High | v0.8 overhead/sinks | 500-cycle simulations remain bounded |
| Automation dominates all builds | High | Dependency + workforce/civic tradeoff | human-first and balanced builds viable |
| UI overwhelms new players | High | staged onboarding | first-hour test passes without coaching |
| Art factions look like recolors | High | visual grammars + grayscale test | faction recognition test passes |
| Destruction scope explodes | High | only 8 module-destruction structures | other buildings use standardized states |
| Interiors consume production | Medium | 3 interior tiers | only 8 authored interior targets |
| Quest state breaks due dynamic world | High | state graph + adaptation rules | destruction/ownership QA passes |
| Performance hitches at cycle tick | High | distributed async simulation | benchmark passes |
| Controller city building feels poor | High | controller-first placement UX | all core build actions pass controller test |
| Scope creep to 20 factions | Critical | v1.1 explicit OUT list | no extra faction production before signoff |

---

# 107. DESIGN RISKS THAT ARE ACCEPTED

The vertical slice intentionally accepts:

- incomplete global world
- incomplete endgame
- only one fully playable starting civilization
- reduced Eden AI
- limited vehicle selection
- limited weapon catalog
- limited Level III assets
- 3 major Leaders only

These are not defects.

They are scope decisions.

---

# 108. UNIT TEST REQUIREMENTS

Automated tests required for:

- card ID uniqueness
- card definition validation
- deck copy limits
- placement footprints
- utility capacity
- economy formulas
- staffing multiplier
- research costs
- Dependency calculations
- capture eligibility
- reward provenance
- save schema serialization
- history tags
- quest graph reachability
- event trigger conditions

---

# 109. SYSTEM TESTS

Required headless/system tests:

## Economy
100 / 500 Development Cycle runs.

## Population
migration, jobs, housing.

## Forgeweave AI
100 World Ticks.

## Foundry Shortage
all resolution paths.

## Conquest
all four routes.

## Post-Ascension
resource/card/Leader state validation.

---

# 110. INTEGRATION TESTS

Required:

1. Draw → place → construct → save → reload.
2. Citizen moves home/job → stream out → stream in.
3. Building damaged → captured → saved → reloaded.
4. Trade contract alters Economic Autonomy.
5. Faction quest alters Civic Legitimacy.
6. Combat objective alters Military Sovereignty.
7. Third Foundry alters Alliance Readiness.
8. Daxton state alters post-Ascension dialogue.
9. Forgeweave unlock alters Collection.
10. Autonomous Factory adds both systemic pressures.

---

# 111. QUEST REGRESSION MATRIX

Every required quest must test:

- start
- success
- alternative branch
- abandonment where allowed
- pre-satisfied objective
- building destroyed
- citizen unavailable
- region ownership changed
- save/reload mid-quest
- completion after delayed world time

---

# 112. PERFORMANCE TESTS

Measure:

- Founder city traversal
- City Mode high view
- Command Mode combat
- Ironheart siege
- Grand Forge boss
- Ascension
- region travel
- Development Cycle boundary
- World Tick processing
- save operation
- load operation

---

# 113. LONG SOAK TEST

Simulate:

# 1,000 DEVELOPMENT CYCLES

with:

- player city economy
- population
- Forgeweave AI
- trade
- events enabled

Must not produce:

- infinite money
- negative impossible population
- invalid World Assets
- utility deadlock
- unbounded event spam
- save growth runaway
- permanent stuck quests

---

# 114. PLAYER TEST GROUPS

At minimum test with:

## Group A — City Builder Players
Do they understand cards as construction inventory?

## Group B — TCG Players
Do they value placement and persistent assets?

## Group C — RPG Players
Do they care about citizens/Leaders?

## Group D — Strategy Players
Do conquest and economy feel systemic?

## Group E — Genre-Unfamiliar Players
Can onboarding teach the game without developer help?

---

# 115. VERTICAL SLICE PLAYER ACCEPTANCE

The slice is not accepted until a meaningful majority of first-time testers can answer yes to:

1. I understood that cards become real city assets.
2. My placement decisions mattered.
3. Capital, Insight, and Influence felt different.
4. Citizens felt connected to my city decisions.
5. Synara's automation felt powerful and dangerous.
6. Forgeweave felt like a different civilization, not a reskin.
7. Eden changed how I approached Forgeweave.
8. Combat affected the persistent city.
9. I understood more than one conquest route.
10. My Forgeweave resolution felt like my choice.
11. Ascension materially changed my city.
12. I wanted to continue toward a second civilization.

---

# 116. FAILURE CONDITIONS FOR THE SLICE

The vertical slice fails if:

- cards feel like menu tokens rather than world objects
- city building can be ignored
- citizens feel decorative
- Founder gameplay can be removed without affecting strategy
- Command Mode feels like an unrelated RTS
- Force is obviously superior to every other conquest route
- Alliance is only dialogue
- Forgeweave resets after siege
- Ascension is only a reward screen
- post-Ascension city looks unchanged
- save/load loses history
- first-hour onboarding requires developer coaching
- the game only works because systems are heavily scripted for one sequence

---

# 117. DEFINITION OF CONTENT COMPLETE

Content Complete means:

- all 25 quests implemented
- six world events functional
- 20 named citizens persistent
- three Leaders complete
- 64 definitions valid
- all core UI surfaces available
- all required dialogue recorded or final-text ready
- all required gameplay areas accessible
- all four conquest routes completable
- Ascension complete

It does not mean polished.

---

# 118. DEFINITION OF FEATURE COMPLETE

Feature Complete means:

- no required system is represented by a stub
- save/load works
- controller works
- all content paths are playable
- all acceptance criteria have executable tests

---

# 119. DEFINITION OF VERTICAL SLICE COMPLETE

Vertical Slice Complete means:

- feature complete
- content complete
- art direction consistent
- audio/VFX integrated
- performance targets met
- critical/high bugs resolved
- save regression passes
- onboarding passes
- all four conquest route runs pass
- first Ascension is shippable-quality
- no scoped-out system is required to make the experience coherent

---

# 120. BUG SEVERITY GATE

## BLOCKER
Data loss, crash, campaign impossible.

Must be zero.

## CRITICAL
Main system broken, conquest impossible, save corruption risk.

Must be zero.

## HIGH
Major feature degraded or one route broken.

Must be zero for signoff.

## MEDIUM
Noticeable defect with workaround.

May remain only if explicitly accepted.

## LOW
Cosmetic/minor.

May remain within agreed polish tolerance.

---

# 121. SCOPE CHANGE RULE

After v1.1 freeze:

Any proposed new feature must answer:

1. Which acceptance criterion cannot be met without it?
2. What existing scope is removed to pay for it?
3. Does it increase art, engineering, narrative, UI, QA, or performance risk?

If there is no concrete answer, it is deferred.

---

# 122. POST-v1.1 DECISION

After the vertical slice is proven, the project chooses among:

## PATH A — FULL PRODUCTION
Scale toward all 20 civilizations.

## PATH B — ITERATE CORE
Fix weak systems before scaling.

## PATH C — REDUCE SCOPE
If one genre pillar is not carrying its weight.

The slice exists to answer this honestly.

---

# 123. WHAT NOT TO DO NEXT

Do not immediately:

- create 20 full city kits
- create 300 final cards
- write all Leader dialogue
- build the Crown
- add multiplayer
- produce every biome

The next development work should execute the finite vertical slice.

---

# 124. FIRST IMPLEMENTATION TARGET

The first end-to-end playable proof remains:

# ADAPTIVE HABITAT

It must be:

- a Card Definition
- an owned Card Instance
- in a 60-card Deck
- drawn
- placed
- constructed
- staffed
- utility-connected
- simulated
- visually explorable
- damaged
- repaired
- saved
- reloaded
- historically persistent

Everything else builds outward from that proof.

---

# 125. SECOND IMPLEMENTATION TARGET

# ONE LIVING SYNARA DISTRICT

Required:

- Adaptive Habitat
- Corner Exchange
- Cognitive Operations Tower
- Microgrid Station
- Water Reclaimer
- Neural Relay
- Agency Forum
- 20 residents
- jobs
- Capital
- Insight
- Dependency
- Nia

This is the first mini-gameplay loop.

---

# 126. THIRD IMPLEMENTATION TARGET

# THE FRONTIER CLAIM

Required:

- Founder traversal
- basic combat
- Guardian Drone Cohort
- Command Mode
- cover
- Control Zones
- resource claim

This proves city assets can enter conflict.

---

# 127. FOURTH IMPLEMENTATION TARGET

# THE FOUNDRY SHORTAGE

Required:

- Forgeweave remote economy
- Eden response
- market movement
- trade
- diplomacy
- event staging

This proves the world is systemic.

---

# 128. FIFTH IMPLEMENTATION TARGET

# OPERATION IRON VEIL / ALTERNATE RESOLUTION

Required:

- conquest meters
- persistent Ironheart
- Daxton
- multiple routes
- infrastructure consequences

This proves conquest.

---

# 129. FINAL IMPLEMENTATION TARGET

# FIRST ASCENSION

Required:

- Forge Relic
- Replication
- Forgeweave cards
- Leader outcome
- city transformation
- Autonomous Factory
- 1/20 reveal

This proves the game's long-term promise.

---

# 130. VERTICAL SLICE NORTH STAR

The build is successful if a player finishes and can tell another person:

> “I built my city out of cards, met people who actually lived in what I built, got pulled into a conflict between an industrial civilization and an ecological one, fought and negotiated inside their real cities, took over Forgeweave in my own way, and when I got back home their technology had literally become part of my city and my deck.”

If the player can truthfully describe that experience, the vertical slice has proven DOMINION // ASCENDANT.

---

# 131. PRODUCTION MANTRA

# DO NOT BUILD THE WHOLE WORLD YET.
# BUILD ONE CARD CORRECTLY.
# BUILD ONE DISTRICT CORRECTLY.
# BUILD ONE RIVAL CORRECTLY.
# CONQUER ONE CIVILIZATION CORRECTLY.
# MAKE THE WORLD REMEMBER IT.
# THEN SCALE.
