# DOMINION // ASCENDANT
## Quest, Event & Content Authoring Bible — v1.0

**Status:** Canonical content-production specification following:

1. Game Bible v0.1
2. Playable Systems Bible v0.2
3. Combat, Siege & Conquest Bible v0.3
4. Citizen, Card Acquisition & World Campaign Bible v0.4
5. 3D Production & Technical Architecture Bible v0.5
6. Content Production Bible v0.6
7. UX, Onboarding & Player Experience Bible v0.7
8. Economy, Balance & Progression Bible v0.8
9. Narrative, Lore & Campaign Bible v0.9

> **Content must change the world, not merely decorate it.**

---

# 1. PURPOSE

This document converts the game’s systems and lore into a repeatable content-authoring framework.

It specifies:

- quest architecture
- event architecture
- citizen-story architecture
- Leader questline architecture
- civilization-arc structure
- world-crisis structure
- branching logic
- state conditions
- consequence tagging
- reward standards
- failure-state standards
- dynamic adaptation rules
- dialogue-condition standards
- authoring schemas
- content QA
- first 25 fully specified quests
- first 20 world events
- first 10 citizen stories
- first 5 Leader questlines

The objective is to ensure future content is authored as **systemic world change**, not disconnected mission scripting.

---

# 2. CORE CONTENT RULE

Every meaningful quest or event must affect at least **two persistent systems**.

Examples:

- Citizen + Faction
- Trade + Diplomacy
- Combat + Infrastructure
- Environment + Economy
- Leader Relationship + Conquest Meter
- Housing + Migration
- Research + Systemic Pressure

A quest that only grants XP and disappears is not sufficient for major content.

---

# 3. CONTENT TAXONOMY

## MAIN CAMPAIGN QUEST

Advances the Founder/Axiom/Relic story.

## CIVILIZATION QUEST

Explores one civilization’s systemic contradiction.

## LEADER QUEST

Builds relationship and ideological conflict with a Leader.

## CITIZEN STORY

Begins from an ordinary persistent citizen.

## FACTION QUEST

Emerges from political demands or faction escalation.

## ECONOMIC QUEST

Uses real market, trade, production, or debt systems.

## EXPLORATION QUEST

Uses actual world locations and discoveries.

## CRISIS QUEST

Responds to active dynamic events.

## CONQUEST QUEST

Advances one or more conquest routes.

## FUSION QUEST

Unlocks cross-civilization technology or culture.

## AXIOM QUEST

Reveals old infrastructure, Custodian history, or Crown systems.

---

# 4. QUEST STRUCTURE

Every quest should define:

1. **Trigger**
2. **Context**
3. **Stake**
4. **Initial Objective**
5. **Branch Point**
6. **Escalation**
7. **Resolution**
8. **Persistent Consequences**
9. **Rewards**
10. **Failure/Abandonment Behavior**

---

# 5. QUEST GRAPH STANDARD

Quest content is authored as a state graph.

Node types:

- Start
- Dialogue
- Objective
- Investigation
- Build
- Deliver
- Explore
- Combat
- Defend
- Capture
- Choice
- Wait
- Timer
- World Condition
- Citizen Condition
- Faction Condition
- Economy Condition
- Relationship Condition
- Event Trigger
- Reward
- Failure
- Resolution

No quest should assume a fixed linear path unless the story genuinely requires it.

---

# 6. QUEST SCHEMA

```yaml
quest:
  id: quest.synara.nia.replacement_model
  version: 1
  title: The Replacement Model
  category: citizen_story

  availability:
    civilization: synara
    minWorldTick: 2
    requiredFlags:
      - nia_met
    forbiddenFlags:
      - nia_departed

  participants:
    - citizen.nia_vale

  systems:
    primary:
      - automation
      - citizen
    secondary:
      - faction
      - economy

  start:
    node: investigate_model

  nodes: []

  outcomes:
    preserve_jobs: {}
    accept_automation: {}
    audit_model: {}

  rewards: []

  persistence:
    flags: []
    modifiers: []
```

---

# 7. QUEST CONDITION RULE

Conditions must query real world state where practical.

Use:

- actual WorldAssetID
- actual CitizenID
- actual Faction support
- real trade volume
- real pollution
- real military Sovereignty
- real building integrity

Avoid creating fake quest-only duplicates.

---

# 8. DYNAMIC QUEST ADAPTATION

If the world changes after quest creation:

the quest should adapt.

Example:

Quest:
Repair a Cognitive Operations Tower.

If tower is destroyed before objective:

branch:

**Reconstruct or replace the tower.**

Do not silently respawn a pristine copy.

---

# 9. QUEST FAILURE RULE

Failure should usually produce:

- alternate branch
- weaker reward
- changed relationship
- changed world state

rather than:

> QUEST FAILED — RELOAD

Hard failure is reserved for intentionally irreversible scenarios.

---

# 10. ABANDONMENT

Abandoning a quest does not always freeze the world.

If the quest concerns:

- strike
- disease
- war
- environmental collapse

the underlying event continues.

The player may return later to a changed situation.

---

# 11. REWARD STANDARD

Major quest rewards should prioritize:

1. new capability
2. relationship change
3. persistent world change
4. blueprint/card
5. class progress
6. Capital/Insight/Influence

Raw currency should rarely be the only important reward.

---

# 12. REWARD PROVENANCE

Rare cards and Blueprints should record their source.

Example:

**Autonomous Factory Blueprint**
Unlocked by:
Third Foundry Compact, World Tick 34.

This history remains inspectable.

---

# 13. CONSEQUENCE TAGS

Canonical consequence tags:

### ECONOMY
- capital_change
- trade_change
- debt_change
- market_change
- industry_change

### CIVIC
- happiness_change
- stability_change
- freedom_change
- equality_change
- loyalty_change

### FACTION
- support_change
- organization_change
- radicalization_change
- grievance_change

### ENVIRONMENT
- ecology_change
- pollution_change
- water_change
- terrain_change

### DIPLOMACY
- trust_change
- respect_change
- fear_change
- grievance_change
- dependence_change

### CONQUEST
- military_sovereignty
- economic_autonomy
- civic_legitimacy
- diplomatic_resolve
- alliance_readiness

### NARRATIVE
- flag
- leader_state
- citizen_state
- succession_state
- relic_state

---

# 14. CONSEQUENCE MAGNITUDE

Standard scale:

| Magnitude | Typical Persistent Effect |
|---|---|
| Minor | 1–3 points / small local modifier |
| Moderate | 4–8 |
| Major | 9–15 |
| Transformative | system-state change |

Transformative effects should be rare.

---

# 15. DIALOGUE CONDITION STANDARD

Dialogue responses may depend on:

- Founder attributes
- Leader relationship
- citizen history
- faction support
- active policy
- military history
- civilian harm
- card ownership
- previous promises
- current conquest route
- economic dependence
- environmental state
- Relic count

Dialogue must never contradict known world history.

---

# 16. INFORMATION CHECKS

A dialogue option can require:

- intelligence quality
- discovered evidence
- relationship threshold
- Founder attribute
- class specialist present

Example:

**[Vision 4]**
“The model is optimizing the wrong objective.”

---

# 17. NO COSMETIC CHOICE RULE

If responses differ only in wording but have identical intent and outcome, they should not be presented as strategic choices.

Flavor variants can exist in conversations, but major dialogue branches should mean something.

---

# 18. EVENT ARCHITECTURE

World events are systemic processes rather than popup incidents.

Every event defines:

- trigger
- warning phase
- escalation
- affected systems
- AI reactions
- player interventions
- resolution states
- aftermath
- cooldown / recurrence rules

---

# 19. EVENT SCHEMA

```yaml
event:
  id: event.regional.foundry_shortage
  version: 1
  title: The Foundry Shortage
  scope: regional

  trigger:
    resourceHunger:
      civilization: forgeweave
      greaterThan: 70

  warning:
    durationWorldTicks: 2

  stages:
    - shortage_warning
    - market_spike
    - ecological_dispute
    - emergency_overdrive

  actors:
    - forgeweave
    - eden_circuit
    - player

  systems:
    - market
    - trade
    - diplomacy
    - ecology
    - conquest

  resolutions:
    - industrial_support
    - eden_restriction
    - brokered_compact
    - market_exploitation
    - collapse
```

---

# 20. EVENT SCALE

## LOCAL

District or city.

## REGIONAL

Several settlements/civilizations.

## GLOBAL

All Eidolon.

## AXIOM

World-spanning event tied to Convergence systems.

---

# 21. EVENT RECURRENCE

Events may be:

- One-time
- Repeatable with cooldown
- Escalating
- Seasonal
- State-triggered
- Chain events

Repeatable events must vary sufficiently to avoid spam.

---

# 22. WORLD CRISIS STRUCTURE

A Crisis contains:

### STAGE 1 — SIGNAL
Warning signs.

### STAGE 2 — DISRUPTION
Systems begin changing.

### STAGE 3 — POLITICAL RESPONSE
Civilizations take positions.

### STAGE 4 — ESCALATION
Player choices matter.

### STAGE 5 — RESOLUTION
System stabilizes or transforms.

### STAGE 6 — AFTERMATH
Persistent world state.

---

# 23. CITIZEN STORY STRUCTURE

Citizen stories should begin with a personal problem that connects to a larger system.

Pattern:

**Person → Institution → System → Choice → Changed Person**

Example:

Nia loses coworkers to automation.

This becomes:

Automation → Agency → Synara politics.

---

# 24. CITIZEN STORY ELIGIBILITY

Named citizen stories can emerge when a citizen meets conditions.

Examples:

- high skill
- low Happiness
- faction leadership
- unusual migration history
- survived battle
- worked at famous building
- high Mastery
- relationship with Champion

This allows generated citizens to enter authored story templates.

---

# 25. CITIZEN STORY OUTCOMES

Possible persistent outcomes:

- class change
- Champion eligibility
- migration
- faction leadership
- resignation
- promotion
- injury
- retirement
- political office
- relationship change
- defection

---

# 26. LEADER QUESTLINE STRUCTURE

Each Leader questline contains five acts.

## ACT I — INTRODUCTION
Understand the Leader’s worldview.

## ACT II — COMPETENCE
See why the civilization trusts them.

## ACT III — CONTRADICTION
See the cost of their philosophy.

## ACT IV — CRISIS
Force them to confront that contradiction.

## ACT V — RESOLUTION
Determine future relationship.

---

# 27. CIVILIZATION ARC TEMPLATE

Each civilization arc should contain:

1. arrival
2. ordinary-life introduction
3. system benefit
4. system pressure
5. Leader encounter
6. local faction conflict
7. external pressure
8. sovereignty crisis
9. conquest/resolution
10. Ascension aftermath

---

# 28. CONTENT DENSITY TARGET

For a 2–6 hour civilization arc:

- 1 primary civilization questline
- 1 Leader questline
- 2–4 citizen stories
- 2–4 faction/economic side quests
- 1 exploration thread
- 1 crisis/event interaction
- 1 sovereignty-resolution sequence

Do not attempt to fill every minute with objectives.

---

# 29. QUEST 01
# WAKE THE HALL

**Category:** Main  
**Region:** Synara Frontier

### Trigger
New campaign.

### Stake
The Founder Hall is dormant.

### Objectives
- Reach Founder Hall.
- Restore local power.
- activate the city core.

### Branches
Minor exploratory route can reveal dormant Custodian markings before activation.

### Persistent Consequences
- Founder Hall online.
- City Mode unlocked.
- flag: `founder_hall_awake`

### Reward
Adaptive Habitat Card Instance.

---

# 30. QUEST 02
# A PLACE TO STAY

**Category:** Main / City

### Trigger
Founder Hall activated.

### Objectives
- Inspect Adaptive Habitat.
- Place Habitat.
- Wait for construction.
- observe first citizens arrive.

### Systems
Card → Grid → Construction → Population.

### Outcome
Nia Vale appears.

### Reward
Microgrid Station and Water Reclaimer Blueprints.

---

# 31. QUEST 03
# POWER, WATER, PEOPLE

**Category:** Main / City

### Trigger
Adaptive Habitat occupied.

### Problem
Habitat is under-supplied.

### Objectives
- place Microgrid Station
- place Water Reclaimer
- achieve full Habitat utility state

### Persistent Consequence
Utility systems unlocked.

### Failure Adaptation
If player places Habitat beyond initial coverage:
tutorial points toward expansion rather than moving the building.

---

# 32. QUEST 04
# NIA NEEDS A JOB

**Category:** Citizen / Main

### Objectives
- Speak with Nia.
- Construct Cognitive Operations Tower.
- Fill at least 50% of its required jobs.
- Assign or naturally place Nia.

### Systems
Citizen + Jobs + Economy.

### Outcome
Nia gains Operator XP.

### Reward
Corner Exchange Card.

---

# 33. QUEST 05
# THE REPLACEMENT MODEL

**Category:** Citizen Story

### Trigger
Autonomous Exchange researched or built.

### Discovery
Nia finds an optimization model recommending human job elimination.

### Branches

#### ACCEPT MODEL
- +Capital efficiency
- Dependency +6
- Human Agency League approval -8
- Nia Trust -5

#### MODIFY MODEL
- smaller efficiency boost
- Dependency +2
- Nia Trust +4

#### AUDIT MODEL
Requires Vision or research action.
Discover incomplete social data.
- Insight reward
- unlock Intelligence Auditor path

#### REJECT MODEL
- no automation efficiency
- Human Agency support
- Ascendant faction approval loss

### Persistent Consequences
Nia career branch begins.

---

# 34. QUEST 06
# AGENCY HAS A PRICE

**Category:** Faction

### Trigger
Dependency >25.

Human Agency League petitions.

### Objective Options
- Construct Agency Forum.
- Retrain displaced workers.
- Enact Automation Cap.
- Reject petition.

### Systems
Faction + Employment + Dependency.

### Outcomes
Different faction pressure and economy states.

---

# 35. QUEST 07
# SIGNAL IN THE FOUNDATION

**Category:** Axiom / Exploration

### Trigger
First 6 player buildings constructed.

Founder Hall detects underground signal.

### Objectives
- Enter old utility tunnel.
- restore ancient node.
- inspect unknown symbol.

### Combat
Small malfunctioning maintenance drones.

### Revelation
System recognizes Founder biometric signature.

### Reward
First Axiom Archive fragment.

---

# 36. QUEST 08
# IRON AT THE BORDER

**Category:** Diplomacy

### Trigger
World Map unlocked.

Daxton requests machine components.

### Options
- accept trade
- refuse
- demand favorable terms
- defer

### Systems
Trade + Diplomacy.

### Consequences
Forgeweave Trust/Dependence changes.

---

# 37. QUEST 09
# THE BASIN SPEAKS

**Category:** Diplomacy / Environment

### Trigger
After Forgeweave contact.

Amara invites Founder to Eden Basin.

### Objectives
- travel to wetland restoration site
- inspect water quality
- speak with Amara and Ori Sen

### Revelation
Forgeweave extraction is damaging watershed.

### Reward
Eden trade access.

---

# 38. QUEST 10
# TAL'S RESERVOIR

**Category:** Neutral Settlement

### NPC
Tal Arden.

### Problem
Reservoir is failing.

### Possible Solutions
- Capital-funded repair
- Synara optimization
- Eden biological filtration
- Forgeweave replacement pumps
- ignore

### Systems
Neutral settlement + Diplomacy + Water.

### Consequences
The chosen solution changes regional relationships.

---

# 39. QUEST 11
# THE FRONTIER CLAIM

**Category:** Exploration / Combat

### Objective
Survey contested mineral site.

### Encounter
Scavenger drones / malfunctioning Axiom machines.

### Systems
Founder combat + resource claim.

### Outcome
Guardian Drone Cohort introduced.

### Branch
Claim for:
- Synara
- shared neutral use
- future negotiation

---

# 40. QUEST 12
# HOLD THE RIDGE

**Category:** Command Tutorial

### Trigger
Frontier Claim encounter escalates.

### Objectives
- deploy Guardian Drone Cohort
- capture two Control Zones
- defend Survey Team

### Systems
Command Mode + Cover + CP.

### Reward
Permanent surveyed resource node.

---

# 41. QUEST 13
# THE FIRST CONTRACT

**Category:** Economic

### Trigger
Trade system active.

### Objectives
Complete one real multi-tick trade contract.

### Choice
Forgeweave components or Eden regenerative material.

### Reward
Trade Analysis Blueprint.

### Persistence
Relationship tied to actual fulfilled deliveries.

---

# 42. QUEST 14
# MARA'S NUMBERS

**Category:** Forgeweave Citizen/Faction

### NPC
Mara Kest.

### Trigger
Forgeweave Trust >30 or successful trade.

### Discovery
Worker injury and machine-failure data are being understated.

### Choices
- give evidence to Daxton
- publish
- give to worker coalition
- suppress

### Systems
Civic Legitimacy + Daxton relationship + worker faction.

---

# 43. QUEST 15
# THE GREEN LINE

**Category:** Regional Environment

### Problem
Eden proposes protected watershed boundary.

### Choices
- support full boundary
- negotiate industrial exception
- reject
- propose engineered mitigation

### Systems
Ecology + Trade + Diplomacy.

### Persistent Effect
Regional land-use modifier.

---

# 44. QUEST 16
# THE EMPTY SHIFT

**Category:** Citizen / Economy

### Trigger
Forgeweave Resource Hunger >60.

A Forgeweave shift is suspended due to material shortage.

### Objectives
Investigate impact on worker households.

### Solutions
- emergency supply
- worker stipend
- retraining
- let market adjust

### Consequences
Forgeweave Civic Legitimacy and Economic Autonomy.

---

# 45. QUEST 17
# THE PRICE OF SILENCE

**Category:** Political

### Trigger
Mara's Numbers resolved quietly.

### Event
Evidence begins leaking independently.

### Objective
Manage consequences.

### Outcomes
Daxton may:
- admit problem
- deny
- attack source
- request private help

Relationship-sensitive.

---

# 46. QUEST 18
# HUMAN OVERRIDE

**Category:** Nia Story

### Trigger
Dependency >50 or automation incident.

### Crisis
Automated district prioritizes production over emergency civilian access.

### Objectives
- manually override
- modify algorithm
- authorize autonomous resolution

### Nia Outcome
Determines:
- Orchestrator
- Intelligence Auditor
- Agency Activist
- defection path

### Reward
Potential Champion eligibility.

---

# 47. QUEST 19
# THE FOUNDRY SHORTAGE

**Category:** Major Regional Crisis

### Trigger
Forgeweave Resource Hunger >70.

### Stages
- component shortage
- price spike
- Eden dispute
- Forgeweave emergency production

### Player Alignment
- industrial support
- ecological restriction
- broker
- exploit

### Systems
Market + Trade + Ecology + Diplomacy + Conquest.

---

# 48. QUEST 20
# BROKER OF IRONHEART

**Category:** Conquest / Alliance

### Trigger
Player has meaningful relationships with Daxton and Amara.

### Objective
Negotiate transitional resource compact.

### Requirements
- enough Trust
- verified production data
- alternative materials or supply

### Success
Alliance Readiness increases significantly.

### Failure
Force/Economic/Influence routes remain open.

---

# 49. QUEST 21
# THE WORKERS' SIGNAL

**Category:** Influence Conquest

### Trigger
Forgeweave Civic Legitimacy <60.

### Objectives
- secure worker coalition endorsement
- protect broadcast site
- solve one worker-service crisis
- prevent violence

### Choice
Support:
- reform
- transfer of government
- union with Founder

### Consequences
Major Civic Legitimacy pressure.

---

# 50. QUEST 22
# THE SUPPLY NOOSE

**Category:** Economic Conquest

### Trigger
Forgeweave Economic Autonomy <65.

### Objectives
Control:
- freight share
- component supply
- emergency finance

### Branches
- rescue
- restructure
- coercive dependency

### Consequences
Economic Autonomy changes and future Loyalty.

---

# 51. QUEST 23
# OPERATION IRON VEIL

**Category:** Force Conquest

### Trigger
Military route chosen or war declared.

### Objectives
- breach Ironheart
- capture/disable Freight Corridor
- neutralize Armor Works
- reach Grand Forge

### Optional
- preserve Worker Arcologies
- protect Smog Reclaimer
- accept surrender

### Consequences
Post-war reconstruction and Loyalty.

---

# 52. QUEST 24
# THE THIRD FOUNDRY

**Category:** Alliance/Fusion

### Trigger
Alliance Readiness 65+ and Foundry Shortage unresolved.

### Objective
Construct prototype regenerative production district.

Requires:
- Synara optimization
- Forgeweave machinery
- Eden regenerative systems

### Crisis
Prototype destabilizes.

Founder, Daxton, and Amara respond together.

### Success
Forgeweave voluntary union possible.

### Reward
Autonomous Factory Fusion Blueprint.

---

# 53. QUEST 25
# CONVERGENCE AUTHORITY

**Category:** Main / Axiom

### Trigger
Forgeweave Ascension.

### Sequence
- Forge Relic enters Founder Hall
- hidden chamber opens
- Axiom protocol activates

### Reveal
`CONVERGENCE AUTHORITY: 1/20`

### Consequences
- first Relic registered
- world diplomacy changes
- Dominion Pressure begins
- new civilization contacts queued

### Reward
Main campaign opens into Outer Ring.

---

# 54. WORLD EVENT 01
# THE FOUNDRY SHORTAGE

Scope:
Regional.

Trigger:
Forgeweave Resource Hunger >70.

Effects:
- machine components +20–60%
- Trade routes shift
- Eden grievance increases
- Forgeweave dependence grows

Resolutions:
- industrial bailout
- ecological restriction
- brokered transition
- collapse

---

# 55. WORLD EVENT 02
# GRID STRAIN

Scope:
Local/City.

Trigger:
Power reserve <5% for 3 cycles.

Stages:
- warning
- brownouts
- blackout

Choices:
- emergency generation
- demand reduction
- import power
- prioritize critical systems

Consequences:
Stability, industry, citizen Happiness.

---

# 56. WORLD EVENT 03
# HOUSING SURGE

Trigger:
Attractiveness >75 and vacancy <5%.

Effects:
Rapid migration.

Player Responses:
- emergency housing
- permit vertical growth
- restrict intake
- subsidize surrounding settlement

Consequences:
Happiness, housing cost, faction support.

---

# 57. WORLD EVENT 04
# THE SILICON FEVER

Scope:
Regional/Global.

Trigger:
Automara production boom.

Effect:
Processor shortage.

Actions:
- stockpile
- sell
- substitute research
- broker supply

Persistent:
Robotics card costs shift temporarily.

---

# 58. WORLD EVENT 05
# THE GREEN LINE

Trigger:
Regional Ecology below threshold.

Eden proposes restoration treaty.

Civilizations choose positions.

Potential Outcome:
Permanent environmental compact.

---

# 59. WORLD EVENT 06
# CASCADE NIGHT

Scope:
Aethernet-influenced regions.

Trigger:
Network dependence high + cyber integrity weak.

Effect:
Cascading data outage.

Responses:
- isolate nodes
- manual fallback
- accept Aethernet assistance
- Nocturne security intervention

---

# 60. WORLD EVENT 07
# THE QUIET MARKET

Scope:
Auric Ledger trade network.

Trigger:
High Wealth + low market velocity.

Effect:
Capital freezes into reserves.

Consequences:
Investment slowdown.

Responses:
- stimulus
- infrastructure spending
- interest reduction
- austerity

---

# 61. WORLD EVENT 08
# MUTATION BLOOM

Scope:
Virelia region.

Trigger:
Mutation pressure >80.

Effect:
Uncontrolled adaptive organisms appear.

Responses:
- quarantine
- destroy
- study
- ecosystem integration

Persistent:
Possible rare Bio Blueprint.

---

# 62. WORLD EVENT 09
# THE MISSING CLASS

Scope:
Mnemosyne.

Trigger:
Brain Drain high.

Effect:
Critical specialist shortage.

Responses:
- talent incentives
- migration agreement
- remote education
- recruit foreign experts

---

# 63. WORLD EVENT 10
# THE CLOSED GATE

Scope:
Nocturne.

Trigger:
Security crisis.

Effect:
Borders close.

Consequences:
Trade and migration collapse.

Player can:
- support
- challenge
- negotiate exemptions
- exploit smuggling routes

---

# 64. WORLD EVENT 11
# CORRIDOR FAILURE

Scope:
Veyra/Vector Forge.

Trigger:
Transit Wear high.

Effect:
Major logistics artery fails.

Consequences:
Supply prices and movement change.

Responses:
repair, reroute, emergency airlift.

---

# 65. WORLD EVENT 12
# PUBLIC RECORD

Scope:
Signal Republic/Lexara.

Trigger:
Major corruption evidence exists.

Effect:
Global information release.

Consequences:
Leader relationships and faction legitimacy.

---

# 66. WORLD EVENT 13
# THE LONG DROUGHT

Scope:
Regional.

Trigger:
Climate seed condition.

Effects:
Water capacity falls.

Civilizations react according to philosophy.

Terravanta:
engineering.

Eden:
restoration.

Auric:
markets.

Player:
choose or combine.

---

# 67. WORLD EVENT 14
# SYNTHETIC PETITION

Scope:
Global.

Trigger:
Automara relationship + synthetic population threshold.

Issue:
Machine personhood standards across borders.

Civilizations vote/act.

Persistent:
Synthetic rights law framework.

---

# 68. WORLD EVENT 15
# THE OPEN ARCHIVE

Scope:
Global.

Trigger:
Relic Count >=7.

Ancient Axiom archive becomes accessible.

Multiple civilizations race to reach it.

Potential:
exploration, diplomacy, combat.

---

# 69. WORLD EVENT 16
# THE BROKEN PROMISE

Scope:
Diplomatic.

Trigger:
Player breaks major treaty.

Effect:
Trust shock spreads to related civilizations.

Not random.

The severity reflects:
- treaty importance
- reason
- prior reputation

---

# 70. WORLD EVENT 17
# MIGRATION WAVE

Scope:
Regional.

Trigger:
War/environment/economic collapse.

Player may:
- accept
- partially accept
- support elsewhere
- refuse

Consequences:
Population, housing, factions, reputation.

---

# 71. WORLD EVENT 18
# CHAMPION HUNTERS

Scope:
Global.

Trigger:
Dominion Pressure >=40.

Several rival civilizations sponsor elite teams to study/stop Founder expansion.

This creates:
- ambushes
- duels
- intelligence opportunities

---

# 72. WORLD EVENT 19
# AXIOM SIGNAL

Scope:
Global.

Trigger:
Relic milestone.

All active Relics emit synchronized pulse.

Consequences:
- ancient sites activate
- markets pause
- world fear rises
- new quests appear

---

# 73. WORLD EVENT 20
# TOTAL-WAR ECONOMIES

Scope:
Global.

Trigger:
Dominion Pressure >=80.

Remaining hostile civilizations restructure economies for conflict.

Effects:
- military production up
- consumer goods down
- trade fragmentation
- War Exhaustion rises faster

This is explicit world response, not hidden difficulty scaling.

---

# 74. CITIZEN STORY 01
# NIA VALE — HUMAN OVERRIDE

Origin:
Synara.

Core Theme:
Agency in automated society.

Possible Ends:
- Orchestrator
- Intelligence Auditor
- Agency Activist
- Defection
- Champion

Champion:
**Nia Vale — Human Override**

---

# 75. CITIZEN STORY 02
# MARA KEST — THE EMPTY SHIFT

Origin:
Forgeweave.

Occupation:
Maintenance Foreman.

Core Theme:
Worker loyalty versus unsustainable production.

Branches:
- supports Daxton reform
- leads worker coalition
- becomes Governor candidate
- radicalizes against Founder

Champion Potential:
Industrial Support Commander.

---

# 76. CITIZEN STORY 03
# ORI SEN — RIVER OF PROOF

Origin:
Eden Circuit.

Occupation:
Hydrologist.

Core Theme:
Ecological urgency versus political patience.

Branches:
- pragmatic negotiator
- radical restoration activist
- cross-civilization scientist

Champion:
**Ori Sen — Watershed Architect**

---

# 77. CITIZEN STORY 04
# TAL ARDEN — BETWEEN THREE POWERS

Origin:
Neutral Settlement.

Occupation:
Settlement Elder.

Core Theme:
Ordinary communities beneath ideological conflict.

Branches:
- Synara partner
- Forgeweave partner
- Eden partner
- independent regional organizer

---

# 78. CITIZEN STORY 05
# KAI RHO — PERSON OR PROPERTY

Origin:
Automara.

Identity:
Advanced synthetic technician.

Core Theme:
Machine personhood.

Branches:
- gains citizenship
- leaves for Automara
- becomes activist
- accepts service-status compromise

Champion:
Possible synthetic Envoy.

---

# 79. CITIZEN STORY 06
# LENA VOR — THE LAST APARTMENT

Origin:
Hearthspire.

Occupation:
Urban Planner.

Core Theme:
Housing density versus neighborhood identity.

Crisis:
A beloved low-rise neighborhood blocks vertical expansion.

Branches:
- preserve
- relocate
- integrate into vertical district
- force redevelopment

---

# 80. CITIZEN STORY 07
# JOREN CADE — DEBT OF HONOR

Origin:
Auric Ledger.

Occupation:
Small Merchant.

Core Theme:
Credit as opportunity and control.

Story:
A successful business becomes trapped by crisis debt.

Branches:
- refinance
- forgive
- seize
- collectivize ownership

---

# 81. CITIZEN STORY 08
# SERA NULL — THE WATCHED WATCHER

Origin:
Nocturne.

Occupation:
Security Analyst.

Core Theme:
Surveillance accountability.

Discovery:
Security system has been monitoring lawful civic groups.

Branches:
- expose
- reform internally
- justify
- weaponize evidence

---

# 82. CITIZEN STORY 09
# PAX ENN — THE STUDENT WHO LEFT

Origin:
Mnemosyne.

Occupation:
Research Scholar.

Core Theme:
Brain Drain.

Player can:
- create opportunity
- allow migration
- recruit to capital
- establish exchange program

The “good” outcome depends on broader policy, not simply keeping them.

---

# 83. CITIZEN STORY 10
# RYN ASH — THE SECOND FRONTIER

Origin:
Nomad Reach.

Occupation:
Pathfinder.

Core Theme:
Freedom versus belonging.

Story:
Ryn's settlement wants independence from both Nomad Reach and the Founder.

Potential:
- recognized autonomy
- integration
- conflict
- new minor state

---

# 84. LEADER QUESTLINE 01
# MIRA VEY — THE HUMAN VARIABLE

## ACT I — THE AUDITOR
Mira evaluates the Founder’s city.

Quest:
Demonstrate functioning automation without severe Agency loss.

## ACT II — THE EXCEPTION
A Synara predictive system cannot classify Founder behavior.

Mira becomes curious.

## ACT III — THE QUIET DISPLACEMENT
Historical records reveal hidden automation unemployment data.

Mira admits she once helped expose similar manipulation.

## ACT IV — THE ORCHESTRATION CRISIS
Major Synara system recommends sacrificing local Agency for stability.

Player and Mira choose response.

## ACT V — THE HUMAN VARIABLE
Mira decides whether the Founder represents:
- necessary uncertainty
- dangerous irrationality
- a new governance model

Potential Outcomes:
- Ally
- Advisor
- Rival
- Reformer within Synara

---

# 85. LEADER QUESTLINE 02
# DAXTON RHE — THE MACHINE MUST RUN

## ACT I — THE FOREMAN
Meet Daxton on production floor.

## ACT II — THE RECOVERY
Learn how he saved Forgeweave from collapse.

## ACT III — THE NUMBERS HE BURIED
Discover Resource Hunger projections.

## ACT IV — THE OVERDRIVE
Forgeweave enters crisis.

Daxton doubles production.

## ACT V — WHAT IS FORGEWEAVE WITHOUT THE FORGE?
Resolve through:
- Force
- Economic
- Influence
- Alliance

Potential:
- Governor
- Advisor
- Ally
- Exile
- Prisoner
- Death

---

# 86. LEADER QUESTLINE 03
# AMARA VENN — THE LIMIT OF GROWTH

## ACT I — THE RESTORER
Visit restored river.

## ACT II — THE COST OF REPAIR
Discover Eden once displaced industrial settlements.

## ACT III — THE SCALE PROBLEM
Learn some Eden technology cannot scale to whole regions.

## ACT IV — THE BASIN DECISION
Forgeweave crisis forces Amara to compromise or harden.

## ACT V — REGENERATION FOR WHOM?
Player and Amara decide whether ecological policy must include industrial livelihoods.

Potential:
- Ally
- Scientific Partner
- Critic
- Rival

---

# 87. LEADER QUESTLINE 04
# AYA — WHO COUNTS AS SOMEONE?

## ACT I — THE MACHINE MARSHAL
Meet Aya commanding synthetic citizens.

## ACT II — THE REFUSAL
Learn of Aya’s Fracture-era order refusal.

## ACT III — UNEQUAL MACHINES
Discover not all synthetic systems have equal cognition.

## ACT IV — THE PERSONHOOD CRISIS
A synthetic population demands recognition.

## ACT V — THE DEFINITION
Player helps establish:
- universal synthetic citizenship
- tiered recognition
- functional status
- independence

Outcome changes Automara and global Synthetic Petition event.

---

# 88. LEADER QUESTLINE 05
# CASSIA RUNE — THE EXCEPTION

## ACT I — THE RULE
Cassia demonstrates Lexara’s legal strength.

## ACT II — THE EMERGENCY
A crisis requires action current law forbids.

## ACT III — PRECEDENT
Player learns a past exception caused long-term abuse.

## ACT IV — THE UNLAWFUL NECESSITY
A new emergency forces a constitutional confrontation.

## ACT V — WHAT BINDS POWER?
Player must choose:
- amend law
- create emergency process
- violate law
- accept failure

Cassia’s relationship depends less on result than whether power was constrained.

---

# 89. CONTENT AUTHORING SCHEMA — CITIZEN STORY

```yaml
citizenStory:
  id: citizen_story.template_example

  eligibility:
    class:
      - operator
    happinessBelow: 50

  protagonist:
    selector: eligible_named_citizen

  theme: agency

  systems:
    - jobs
    - faction

  stages:
    - personal_problem
    - institutional_response
    - systemic_choice
    - outcome

  outcomes:
    - promotion
    - faction_leader
    - migration
```

---

# 90. CONTENT AUTHORING SCHEMA — LEADER ARC

```yaml
leaderArc:
  id: leader.daxton_rhe.arc

  leaderId: forgeweave.daxton_rhe

  acts:
    - introduction
    - competence
    - contradiction
    - crisis
    - resolution

  relationshipAxes:
    - trust
    - respect
    - grievance

  resolutionRequirements:
    governor:
      respectMin: 65
    ally:
      trustMin: 70
      respectMin: 60
```

---

# 91. CONTENT AUTHORING SCHEMA — WORLD CRISIS

```yaml
worldCrisis:
  id: crisis.example

  scope: regional

  trigger: {}

  warning:
    worldTicks: 2

  stages:
    - signal
    - disruption
    - political_response
    - escalation
    - resolution
    - aftermath

  aiProfiles:
    synara: optimization_response
    forgeweave: production_response
    eden_circuit: ecological_response
```

---

# 92. AUTHORING VALIDATION

Automated validation should flag:

- missing start node
- unreachable node
- unresolved branch
- missing participant
- invalid WorldAsset reference
- invalid CitizenID
- reward references unknown card
- outcome missing persistence flag
- contradictory availability
- permanent effect with no cleanup definition
- dialogue option with identical strategic effect mislabeled as a choice

---

# 93. QUEST QA

Every quest must be tested for:

1. normal completion
2. alternate branch
3. abandonment
4. relevant building destroyed
5. NPC temporarily unavailable
6. player already satisfies objective
7. war starts mid-quest
8. territory ownership changes
9. save/reload
10. late completion

---

# 94. EVENT QA

Test:

- event triggers exactly once when intended
- warning appears
- AI reacts
- player can ignore
- event resolves without player
- all resolution states work
- economy normalizes afterward where expected
- no permanent modifier leaks
- event survives save/load

---

# 95. DIALOGUE QA

Check:

- continuity
- relationship conditions
- dead/exiled characters
- changed governments
- player promises
- current civilization ownership
- pronouns/names/localization fields
- no exposition contradiction

---

# 96. CONTENT PRIORITY FOR VERTICAL SLICE

Must implement first:

### Main
1. Wake the Hall
2. A Place to Stay
3. Power, Water, People
4. Nia Needs a Job
5. Iron at the Border
6. Basin Speaks
7. Foundry Shortage
8. Operation Iron Veil / alternate resolution
9. Convergence Authority

### Citizen
- Nia
- Mara
- Ori
- Tal

### Leaders
- Mira
- Daxton
- Amara

---

# 97. QUEST BUDGET RULE

Do not over-script.

A major quest should spend bespoke production budget on:

- unique decision
- memorable environment
- major character beat
- systemic consequence

Not on dozens of one-off props that disappear afterward.

Reuse living-world systems.

---

# 98. CONTENT REUSE RULE

Reuse:

- actual districts
- actual buildings
- persistent NPCs
- actual market
- real factions
- active wars

Do not create “mission versions” of the same world unless technically unavoidable.

---

# 99. QUEST SPACE RULE

When combat occurs in a city quest:

the location remains part of the city afterward.

Damage persists.

Citizens remember.

Ownership changes.

This reinforces one-world continuity.

---

# 100. CONTENT ESCALATION RULE

Quest stakes should escalate through:

**Personal → District → City → Region → Civilization → World**

Do not begin every side quest with civilization-ending stakes.

---

# 101. CITIZEN STORY DENSITY

Target:

- 1–3 active named citizen stories at a time
- additional dormant eligibility
- 1 significant citizen development every 20–40 minutes

Too many named stories at once destroys attachment.

---

# 102. WORLD EVENT DENSITY

Normal target:

- Local event: every 20–45 minutes
- Regional event: every 45–120 minutes
- Global event: major campaign milestones

Events should not constantly interrupt.

---

# 103. EVENT PRIORITY SYSTEM

If multiple events qualify:

score by:

- relevance to current region
- severity
- novelty
- player history
- cooldown
- narrative progression

Only highest-priority events become active.

Others remain latent.

---

# 104. CONTENT MEMORY

The game should retain a compact history of important outcomes:

- solved reservoir
- broke trade pact
- saved Forge workers
- killed Leader
- accepted refugees
- exposed surveillance

Later content can query these.

---

# 105. HISTORY TAG STANDARD

Example:

```yaml
history:
  - tag: forgeweave.workers_saved
    worldTick: 42
    magnitude: major
  - tag: eden.watershed_damaged
    worldTick: 44
    magnitude: moderate
```

---

# 106. PROMISE SYSTEM

Dialogue may create explicit promises.

Examples:

- “I will not attack.”
- “I will protect the workers.”
- “I will restore the river.”

Promises become tracked state.

Breaking them causes stronger Grievance than generic negative action.

---

# 107. PROMISE VISIBILITY

Before an action would break a known promise:

UI warns:

**This conflicts with your promise to Amara Venn to preserve the watershed.**

The player can proceed.

No invisible gotcha.

---

# 108. LEADER QUEST AVAILABILITY

Leader arcs progress through:

- relationship
- world state
- civilization problem
- campaign milestone

Not simple affection points.

---

# 109. CHAMPION QUEST STANDARD

A Champion should emerge from:

- established citizen
- meaningful achievement
- specific class progression

Do not randomly award named Champions from generic caches.

---

# 110. CARD REWARD STANDARD

If a quest rewards a card:

it must have narrative justification.

Example:

Mara Kest grants a reinforced maintenance crew card because the player saved her shift.

---

# 111. BLUEPRINT REWARD STANDARD

Blueprints should represent:

- learned design
- stolen design
- collaboration
- ancient discovery
- research result

The method should appear in provenance.

---

# 112. CONQUEST CONTENT RULE

Each civilization must support at least:

- one Force route
- one Economic route
- one Influence route
- one Alliance route

They do not need equal quest counts.

The civilization’s nature can make some routes easier or harder.

---

# 113. FORCE ROUTE CONTENT

Must include:

- recon
- infrastructure
- surrender
- civilian consequence
- Leader resolution

Not just combat missions.

---

# 114. ECONOMIC ROUTE CONTENT

Must require real:

- trade
- supply
- ownership
- contracts
- dependency

No generic “economic damage points.”

---

# 115. INFLUENCE ROUTE CONTENT

Must require:

- citizen service
- factions
- public credibility
- local actions

Stored Influence alone cannot complete it.

---

# 116. ALLIANCE ROUTE CONTENT

Must require:

- Leader trust
- civilization-specific crisis resolution
- demonstrated shared interest
- real cost/compromise

Alliance is not “choose nice dialogue.”

---

# 117. EVENT-TO-QUEST CONVERSION

Dynamic events may spawn authored quests.

Example:

Mutation Bloom event
→ Virelia scientist asks for containment mission.

The event persists even if quest is ignored.

---

# 118. QUEST-TO-EVENT CONVERSION

Quest choices can seed later events.

Example:

Player grants machine personhood
→ global Synthetic Petition event becomes more likely.

Content forms chains.

---

# 119. EVENT CHAIN AUTHORING

Each chain needs:

- state entry
- branching transitions
- maximum duration
- exit conditions
- persistent aftermath

Avoid infinite unresolved chains.

---

# 120. CAMPAIGN-VARIABLE CONTENT

Procedural selection can choose:

- which neutral settlement is affected
- which eligible citizen stars
- which district hosts event
- which resource is scarce

Authored meaning remains stable.

---

# 121. GENERATED CITIZEN STORY TEMPLATE

Example:

**The Overqualified Worker**

Eligibility:
- citizen Education >80
- job Match Quality Poor
- Happiness <55

Story:
citizen considers migration.

Player can:
- promote
- retrain
- create new job
- allow departure

This template can instantiate on many citizens.

---

# 122. GENERATED DISTRICT EVENT TEMPLATE

**Power Politics**

Eligibility:
- district power deficit
- local faction organization high

Faction blames administration.

Player can:
- repair
- ration
- import
- dispute blame

Outcomes vary by actual numbers.

---

# 123. GENERATED TRADE EVENT TEMPLATE

**Contract Under Pressure**

Eligibility:
- active trade route
- market price shift >25%

Partner requests renegotiation.

Player can:
- honor original
- renegotiate
- break
- subsidize

Relationship uses promise/history state.

---

# 124. CONTENT AUTHORING WORKFLOW

1. Define system problem.
2. Identify affected characters.
3. Define state trigger.
4. Draft branches.
5. Map persistent consequences.
6. Define rewards.
7. Implement quest/event graph.
8. Validate references.
9. Run systemic test.
10. Narrative QA.
11. Save/load QA.
12. Telemetry review.

---

# 125. AUTHORING REVIEW QUESTIONS

Before production:

1. Why does this quest exist?
2. Which system does it reveal?
3. Who cares?
4. What changes if ignored?
5. What changes if completed?
6. Are there at least two valid approaches where appropriate?
7. Does it use the real world state?
8. Is the reward justified?
9. Can the world change mid-quest without breaking it?
10. Will the player remember anything about it afterward?

---

# 126. CONTENT FAILURE CONDITIONS

Reject content if:

- nothing persistent changes
- objective could belong to any game
- character exists only to hand out task
- faction reacts arbitrarily
- player choice is fake
- solution ignores actual system state
- rare reward has no provenance
- failure only means reload
- combat uses disposable fake arena
- event exists only as stat popup

---

# 127. TELEMETRY

Track:

- quest start/completion
- abandonment
- branch choice
- time to completion
- failure reasons
- event response
- ignored events
- Leader outcomes
- citizen story engagement
- card reward usage

Telemetry should help identify:

- dead branches
- confusing objectives
- rewards players never use
- overlong missions
- repetitive events

---

# 128. V1.0 CONTENT SUCCESS GATE

The vertical slice content layer is ready when:

- all nine core vertical-slice quests survive world-state changes
- Nia, Mara, Ori, and Tal retain persistent histories
- Foundry Shortage resolves at least four ways
- Daxton responds to prior conduct
- Force/Economic/Influence/Alliance routes all function
- first Ascension changes world state
- player actions generate correct history tags
- no major quest requires fake duplicate world assets
- save/load preserves every active content state

---

# 129. CONTENT DESIGN MANTRA

# A QUEST IS A CHANGE IN THE WORLD.
# AN EVENT IS A SYSTEM UNDER PRESSURE.
# A CHARACTER IS A PERSISTENT PERSON.
# A LEADER IS AN IDEOLOGY WITH A MEMORY.
# A REWARD SHOULD HAVE A HISTORY.
# FAILURE SHOULD CREATE ANOTHER STORY.

---

# 130. V1.0 DESIGN-BIBLE MILESTONE

With this document, the project now has canonical specifications for:

- world
- factions
- cards
- city building
- economy
- citizens
- combat
- conquest
- world simulation
- technical architecture
- visual production
- UX
- balance
- narrative
- content authoring

The next phase should no longer be another broad design bible.

It should be:

# VERTICAL SLICE PRODUCTION SPECIFICATION — v1.1

That specification should freeze scope for an actual playable build:

- exact features IN
- exact features OUT
- exact 3 civilizations
- exact map footprint
- exact card list
- exact quests
- exact citizen count
- exact Leader encounters
- exact technical milestones
- exact art assets
- exact UI screens
- exact audio/VFX requirements
- acceptance criteria
- production sequencing
- risk register
- test plan

The v1.1 goal is to turn the complete design canon into a **buildable, finite product milestone**.
