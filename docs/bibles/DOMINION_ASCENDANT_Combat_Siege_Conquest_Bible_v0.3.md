# DOMINION // ASCENDANT
## Combat, Siege & Conquest Systems Bible — v0.3

**Status:** Canonical design pass following Game Bible v0.1 and Playable Systems Bible v0.2.

> **Combat does not replace the city-builder. Combat is what happens when two built civilizations collide.**

---

# 1. PURPOSE OF THIS DESIGN PASS

This document specifies the playable conflict layer for **DOMINION // ASCENDANT**:

- Founder combat
- Squad and unit combat
- Command Mode
- Cover, elevation, line of sight, and suppression
- Damage and resistance model
- Buildings as destructible/capturable assets
- Siege preparation and battlefield logistics
- Leader boss encounters
- The four conquest paths:
  - Force
  - Economic Dominion
  - Influence Dominion
  - Alliance
- Mixed conquest strategies
- Occupation, annexation, loyalty, and rebellion
- Ascension resolution
- Dominion Pressure adaptation
- The first complete Forgeweave conquest scenario

The governing rule remains:

> **Every combatant, fortification, vehicle, support system, and battlefield advantage should trace back to something the player physically owns, built, recruited, researched, captured, or allied with.**

The game should not feel as if the city-builder ends and a disconnected combat minigame begins.

---

# 2. CONFLICT DESIGN PILLARS

## 2.1 THE CITY IS THE LOADOUT

Your civilization determines what you can bring to war.

If your city contains:

- robotics factories, you can field synthetic units;
- transit systems, reinforcements arrive faster;
- hospitals, wounded units recover faster;
- logistics hubs, battlefield supply remains stable;
- intelligence facilities, enemy defenses are revealed;
- financial infrastructure, mercenary or emergency purchases become possible;
- cultural infrastructure, surrender and defection are easier;
- research assets, advanced equipment becomes available.

The battle begins before the player reaches the battlefield.

---

## 2.2 EVERY BATTLE SHOULD HAVE STRATEGIC OBJECTIVES

Most combat should not be “kill every enemy.”

Examples:

- Disable a shield network.
- Capture a power plant without destroying it.
- Escort a defector.
- Hold a transit station.
- Sabotage a logistics hub.
- Steal a prototype.
- Defend engineers while they breach a gate.
- Seize a broadcast tower.
- Rescue civilians.
- Force the enemy leader into negotiations.
- Destroy only the facility powering an oppressive system.
- Preserve enough infrastructure to make annexation viable.

---

## 2.3 DESTRUCTION HAS AN ECONOMIC COST

Destroying an enemy city should be easy to understand but strategically expensive.

A building you destroy cannot immediately become:

- your factory,
- your hospital,
- your research lab,
- your tax base,
- your housing,
- your captured card.

The most tactically aggressive route may therefore create the weakest conquered territory.

---

## 2.4 EVERY FACTION FIGHTS DIFFERENTLY

Faction identity must affect battlefield behavior.

Synara does not merely have blue units with different stats.

Synara fights through:

- automation,
- prediction,
- drones,
- coordinated networks,
- battlefield orchestration.

Forgeweave fights through:

- armor,
- production,
- repair,
- artillery,
- industrial overdrive.

Eden Circuit fights through:

- terrain,
- regeneration,
- concealment,
- living infrastructure,
- attrition.

The same philosophy that shapes the city must shape combat.

---

## 2.5 NON-MILITARY CONQUEST MUST BE EQUALLY COMPLETE

Economic, Influence, and Alliance victories are not menu shortcuts.

Each needs:

- missions,
- counterplay,
- strategic preparation,
- escalating responses,
- visible changes in the rival city,
- a climactic leader encounter,
- meaningful post-conquest consequences.

---

# 3. THE THREE PLAY MODES DURING CONFLICT

## FOUNDER MODE

Third-person action RPG control of the Founder.

Used for:

- exploration,
- direct combat,
- infiltration,
- dialogue,
- boss encounters,
- artifact recovery,
- sabotage,
- escort,
- investigation.

---

## COMMAND MODE

Tactical overhead mode.

Used for:

- selecting squads,
- assigning objectives,
- ordering formations,
- deploying reserves,
- marking targets,
- managing control zones,
- coordinating vehicles,
- calling faction abilities.

The game may slow to approximately **20% simulation speed** in standard difficulty while Command Mode is open.

Harder difficulties may reduce or remove this slowdown.

---

## CITY MODE

Strategic city and territory view.

During conflict it is used for:

- production,
- reinforcement,
- repair,
- emergency construction,
- supply routing,
- policy changes,
- civilian evacuation,
- defense allocation.

The player can transition between all three modes without loading a separate match.

---

# 4. FOUNDER COMBAT MODEL

The Founder is powerful but is not a one-person army.

The player should feel capable of changing a battle while still needing civilization infrastructure and allied units.

## Baseline Founder Resources

| Resource | Base | Function |
|---|---:|---|
| Health | 100 | Physical survivability |
| Guard | 50 | Equipment / shield protection |
| Stamina | 100 | Sprint, dodge, climb, melee actions |
| Tactical Charge | 0–100 | Powers Founder tactical abilities |
| Revive Tokens | 1 per operation | Emergency recovery; increased through specific progression |

**Tactical Charge is not an economy currency.** It is a temporary combat resource and cannot purchase cards or assets.

---

# 5. FOUNDER ATTRIBUTES

The five Founder attributes defined in v0.2 now have direct combat/conquest functions.

## COMMAND

Improves:

- squad effectiveness,
- tactical ability recharge,
- unit morale,
- reinforcement control,
- maximum active squads.

## VISION

Improves:

- weak-point identification,
- hacking/scanning,
- research interactions,
- anomaly control,
- technological boss solutions.

## STATECRAFT

Improves:

- occupation stability,
- surrender negotiations,
- political leverage,
- interrogation,
- faction management.

## ENTERPRISE

Improves:

- battlefield supply efficiency,
- emergency purchasing,
- economic warfare,
- salvage value,
- logistics.

## PRESENCE

Improves:

- morale,
- recruitment,
- defection,
- intimidation,
- diplomacy,
- Influence conquest.

---

# 6. FOUNDER LOADOUT

The Founder equips:

- **1 Primary Tool/Weapon**
- **1 Secondary Tool/Weapon**
- **1 Utility Device**
- **1 Traversal Device**
- **3 Active Abilities**
- **1 Founder Ultimate**
- **3 Passive Talents**
- **1 Civilization Relic**

The exact visual form depends on faction and class.

Examples:

### Synara Founder
- Adaptive pulse weapon
- Command drone
- Predictive visor
- Tactical blink relay

### Forgeweave Founder
- Heavy industrial weapon
- Breach tool
- Deployable cover forge
- Powered traversal rig

### Eden Circuit Founder
- Bio-projectile weapon
- Regenerative field device
- Vine grapple
- Natural camouflage

---

# 7. DAMAGE CHANNELS

The game uses six broad damage channels.

| Channel | Strong Against | Often Weak Against |
|---|---|---|
| **Kinetic** | infantry, exposed machinery | heavy armor |
| **Thermal** | biological targets, light cover | insulated structures |
| **Arc** | shields, robotics, networks | grounded armor |
| **Bio** | living targets | synthetic units |
| **Cyber** | connected machinery and systems | disconnected/manual systems |
| **Structural** | buildings, fortifications, vehicles | personnel |

Damage channels are intentionally broad enough to remain readable.

A card or unit can have:

- Armor
- Shield
- Biological Resistance
- Cyber Integrity
- Structural Integrity

rather than dozens of hidden resistances.

---

# 8. FOUNDER DOWNED STATE

When Founder Health reaches zero:

1. The Founder enters **Downed** for 20 seconds.
2. A nearby allied squad can revive the Founder.
3. A Revive Token can self-recover the Founder.
4. If neither occurs, the Founder is extracted.

Extraction causes:

- the current tactical objective to fail or regress;
- loss of some operation momentum;
- surviving units to retreat if no commander remains;
- no permanent character death in the standard campaign.

Major Iron Dominion difficulty may optionally support harsher consequences.

---

# 9. UNIT COMBAT MODEL

Units are persistent civilization assets.

A Unit Card represents a real squad, crew, robot group, or specialist team.

If a squad survives, it gains experience.

If destroyed, it enters the same Recovery/Ruin ecosystem as other persistent cards unless a specific scenario creates permanent consequences.

---

# 10. SQUAD ROLES

## VANGUARD

Frontline pressure.

Strengths:
- direct assault,
- breach,
- close-range control.

## SENTINEL

Defense and holding territory.

Strengths:
- cover,
- overwatch,
- protection,
- zone control.

## SKIRMISHER

Mobility and flanking.

Strengths:
- repositioning,
- pursuit,
- hit-and-run.

## SUPPORT

Healing, shielding, morale, supply.

## ENGINEER

Repair, construction, breaching, capture.

## SPECIALIST

Hacking, investigation, biological control, stealth, faction-specific actions.

## ARTILLERY

Long-range structural and area pressure.

Rare in early play and expensive to support.

---

# 11. UNIT STAT BLOCK

Every combat Unit Card contains:

- Health
- Armor
- Movement Speed
- Effective Range
- Damage Channel
- Damage Output
- Accuracy
- Suppression
- Capture Rate
- Morale
- Supply Consumption
- Squad Size
- Tags
- Active Ability
- Passive Ability
- Upgrade Path
- Veteran Rank

---

# 12. SQUAD SIZE

Typical squads:

| Type | Typical Entities |
|---|---:|
| Specialist | 1–3 |
| Heavy Unit | 2–4 |
| Standard Infantry | 4–6 |
| Light Infantry | 6–8 |
| Drone Swarm | 6–16 |
| Robotic Swarm | 4–12 |

The card remains one tactical selection regardless of how many visible entities compose the squad.

---

# 13. MORALE

Squads have **0–100 Morale**.

Morale changes through:

- casualties,
- isolation,
- leader presence,
- nearby civilians,
- victories,
- suppression,
- supply,
- faction abilities,
- Founder Presence.

Thresholds:

| Morale | State |
|---|---|
| 75–100 | Inspired |
| 50–74 | Steady |
| 25–49 | Shaken |
| 1–24 | Breaking |
| 0 | Rout / surrender |

A routed squad attempts to retreat rather than fighting to the death.

This enables non-lethal and Influence-focused warfare.

---

# 14. COVER

Cover is evaluated dynamically from 3D geometry.

## Partial Cover
Approx. 25–50% of unit body obscured.

Benefits:
- reduced incoming accuracy,
- modest suppression resistance.

## Full Cover
Approx. 50%+ protection from an attack direction.

Benefits:
- strong accuracy reduction,
- large suppression resistance.

## Hardened Cover
Purpose-built defenses.

Benefits:
- high protection,
- structural durability,
- may resist splash damage.

Cover can be destroyed.

A Forgeweave barricade might begin as Hardened Cover and degrade into Partial Cover after structural damage.

---

# 15. ELEVATION

Elevation should matter without becoming a universal damage multiplier.

Higher ground provides:

- improved line of sight,
- longer detection,
- reduced obstruction,
- better firing angles,
- stronger artillery spotting.

Certain units receive explicit elevation bonuses.

Terravanta can reshape elevation.

Veyra can bypass it.

Eden Circuit can obscure it.

---

# 16. LINE OF SIGHT

Line of sight uses actual world geometry.

Units cannot shoot through:

- intact buildings,
- terrain,
- solid fortifications.

Certain technologies permit:

- indirect fire,
- sensor targeting,
- network-assisted targeting,
- wall penetration,
- biological sensing.

A damaged building can physically create new sightlines.

---

# 17. SUPPRESSION

Suppression represents battlefield pressure.

High suppression causes:

- lower accuracy,
- slower movement,
- inability to sprint,
- reduced capture rate,
- eventual Morale loss.

Suppression is especially useful for taking objectives without annihilating defenders.

---

# 18. COMMAND CAPACITY

The player cannot directly micromanage an unlimited army.

Baseline:

- 3 active squads
- 1 active vehicle
- Founder

Command attribute, headquarters, officers, and technologies increase this.

Units outside Command Capacity operate under AI doctrine.

Possible doctrines:

- Hold
- Patrol
- Defend civilians
- Protect infrastructure
- Advance cautiously
- Aggressive assault
- Avoid structural damage

---

# 19. COMMAND POINTS

Operations use temporary **Command Points (CP)**.

CP is not a purchase currency.

Baseline maximum:
**6 CP**

Typical costs:

| Tactical Action | CP |
|---|---:|
| Precision scan | 1 |
| Rapid redeploy | 1 |
| Emergency repair drone | 2 |
| Reinforcement squad | 2 |
| Faction tactical ability | 2–4 |
| Leader-scale battlefield effect | 4–6 |

CP regenerates through:

- holding Command Zones,
- Leader abilities,
- communications infrastructure,
- completing tactical objectives.

This gives Command Mode a decision economy without adding a fourth persistent currency.

---

# 20. VEHICLES

Vehicle Cards correspond to persistent vehicles produced by a facility.

Roles:

- Transport
- Recon
- Assault
- Siege
- Engineering
- Logistics
- Air support

Vehicles require:

- fuel/power,
- maintenance,
- crew unless autonomous,
- supply connection.

Destroyed vehicles can become battlefield wreckage and physical cover.

---

# 21. BUILDING COMBAT STATES

Buildings use four states.

## OPERATIONAL
100–51% Structural Integrity.

Normal output.

## DAMAGED
50–26%.

Reduced output and visible physical damage.

## DISABLED
25–1%.

No production.

Can be captured or repaired.

## RUINED
0%.

Major structural failure.

The card enters Ruined state.

The site remains physical wreckage until:
- reconstructed,
- salvaged,
- cleared.

---

# 22. BUILDING MODULES

Large buildings have targetable modules.

Examples:

### Factory
- Power Module
- Production Line
- Storage
- Control Center

### Defense Tower
- Weapon
- Sensor
- Shield
- Power Core

### Research Center
- Compute Core
- Laboratory
- Data Archive
- Containment System

Destroying modules changes function without necessarily destroying the entire building.

This creates surgical warfare.

---

# 23. CAPTURE SYSTEM

A Disabled enemy facility can be captured if:

- an allied Engineer, Founder, or capture-capable squad reaches the control point;
- enemy contest is removed;
- the facility retains at least 10% Structural Integrity.

Base capture time:
**20 seconds**

Modifiers:

- Engineer: -35%
- Founder Statecraft: up to -20%
- Local civilian support: up to -25%
- Enemy Loyalty: up to +40%
- Active security systems: capture blocked

After capture choose:

### PRESERVE
Operate the original card with reduced efficiency until integrated.

### CONVERT
Transform it into your faction equivalent.

### STUDY
Disable permanently and extract Insight.

### SALVAGE
Destroy for Capital/materials.

### GIFT
Transfer to a local allied faction for Influence and Loyalty.

---

# 24. CIVILIAN DAMAGE

Civilian populations exist physically and strategically.

Large-scale indiscriminate destruction causes:

- Influence penalties,
- lower post-conquest Loyalty,
- higher resistance,
- diplomatic consequences,
- Leader relationship changes,
- possible coalition escalation.

The game does not require zero collateral damage, but it remembers the player's methods.

---

# 25. RULES OF ENGAGEMENT

Before a major operation, the player chooses a doctrine.

## SURGICAL
Prioritize infrastructure preservation and civilian safety.

Benefits:
- Loyalty
- Influence
- capture value

Cost:
- slower combat
- restricted heavy weapons

## STANDARD
Balanced.

## TOTAL BREACH
Maximum structural force.

Benefits:
- speed
- military pressure

Cost:
- severe infrastructure and Loyalty damage
- higher Dominion Pressure response

---

# 26. SUPPLY

Every deployed army has a Supply score from **0–100**.

Supply derives from:

- roads,
- transit,
- logistics facilities,
- captured depots,
- local allies,
- vehicles,
- faction abilities.

At low supply:

| Supply | Effect |
|---|---|
| 75–100 | Fully supplied |
| 50–74 | Minor ability cooldown penalty |
| 25–49 | Reduced repair and reinforcement |
| 1–24 | Morale and combat penalties |
| 0 | No reinforcement; units begin forced withdrawal |

Vector Forge is exceptionally strong here.

Nomad Reach is exceptionally independent of fixed supply.

---

# 27. SIEGE PREPARATION

A full siege begins on the campaign map before the battle.

The player selects:

- Founder loadout
- up to 6 Reserve Squads
- up to 2 Reserve Vehicles
- one Rules of Engagement doctrine
- one primary conquest objective
- one support objective
- active Dominion Doctrines
- available allied faction support

The player's city then calculates:

- reinforcement speed,
- repair speed,
- supply capacity,
- intelligence quality.

---

# 28. THE FIVE SIEGE PHASES

# PHASE I — RECON

Objectives:

- reveal defenses,
- identify utilities,
- locate civilian centers,
- find alternate routes,
- discover political factions,
- identify weak modules.

Recon can happen through:

- Founder infiltration,
- scouts,
- hackers,
- allied citizens,
- economic intelligence,
- diplomacy.

Good Recon allows surgical operations.

---

# PHASE II — ISOLATION

The player tries to weaken the target without taking the capital yet.

Possible targets:

- logistics,
- power,
- communications,
- economic supply,
- propaganda,
- reinforcements,
- political allies.

The enemy reacts.

Overusing Isolation can also hurt the population and reduce future Loyalty.

---

# PHASE III — BREACH

The player gains physical access to defended territory.

Methods:

- destroy fortification,
- hack a gate,
- bribe access,
- capture transit,
- use terrain,
- infiltrate,
- trigger local uprising,
- receive diplomatic invitation.

Every conquest style can create a different breach.

---

# PHASE IV — SOVEREIGNTY

The attacker contests the systems that make the rival government function.

Key targets:

- Government
- Leader command
- Broadcast
- Treasury
- Defense
- Critical infrastructure
- Civic legitimacy

This phase is where the four conquest routes converge.

---

# PHASE V — LEADER RESOLUTION

A named Leader encounter concludes the conflict.

The encounter may be:

- combat,
- duel,
- negotiation,
- hacking confrontation,
- economic showdown,
- political debate,
- survival challenge,
- hybrid multi-stage sequence.

The final Leader does not always have to die or even be physically defeated.

---

# 29. THE DOMINION CONFLICT MODEL

Every rival civilization tracks four conquest meters.

## MILITARY SOVEREIGNTY
0–100

Represents ability to defend territory.

## ECONOMIC AUTONOMY
0–100

Represents independence of the economy.

## CIVIC LEGITIMACY
0–100

Represents public support for the government.

## DIPLOMATIC RESOLVE
0–100

Represents willingness to remain independent rather than join the Founder.

The state begins near:

**100 / 100 / 100 / 100**

Different player actions pressure different meters.

The objective is not necessarily to reduce all four.

---

# 30. FORCE CONQUEST

Primary meter:

# MILITARY SOVEREIGNTY

Military Sovereignty falls through:

- capturing command points,
- disabling defense networks,
- defeating elite units,
- controlling strategic districts,
- destroying or capturing military facilities,
- defeating Leader combat phases.

Example values:

| Action | Sovereignty Damage |
|---|---:|
| Capture minor defense node | -4 |
| Capture barracks/foundry | -7 |
| Disable city shield | -10 |
| Defeat elite champion | -8 |
| Capture military district | -12 |
| Capture capital command | -20 |

At **25 or lower**:
the defender enters Critical Defense.

At **0**:
the Leader must surrender, flee, or enter a final stand.

Military victory still requires Leader Resolution.

---

# 31. ECONOMIC CONQUEST

Primary meter:

# ECONOMIC AUTONOMY

Economic Autonomy falls when the rival economy becomes dependent on the player.

Tools include:

- trade dominance,
- ownership of critical production,
- supply contracts,
- debt,
- resource monopoly,
- captured markets,
- superior logistics,
- controlled exports,
- strategic investment,
- economic rescue.

Important rule:

> Economic conquest should reward creating dependency, not simply destroying the rival economy.

Destroying their economy can actually make Economic conquest harder.

Example pressure:

| Condition | Autonomy Impact |
|---|---:|
| 20% critical imports controlled | -5 |
| 40% industrial trade controlled | -10 |
| Rival accepts emergency credit | -8 |
| Player controls key regional resource | -10 |
| Rival treasury crisis unresolved | -12 |
| 60% regional trade under player | -15 |

At **25 or lower**:
the rival receives **Economic Ultimatum** missions.

At **0**:
the player can demand annexation, protectorate status, or economic union.

The Leader may still reject it and trigger a final political or tactical encounter.

---

# 32. INFLUENCE CONQUEST

Primary meter:

# CIVIC LEGITIMACY

The player is not simply generating generic propaganda.

The player gains support by:

- solving local problems,
- exposing corruption,
- defending civilians,
- supporting factions,
- improving services,
- broadcasting achievements,
- winning debates,
- completing cultural quests,
- demonstrating higher quality of life.

The incumbent may counter with:

- censorship,
- reform,
- propaganda,
- concessions,
- repression,
- misinformation.

Example pressure:

| Action | Legitimacy Impact |
|---|---:|
| Solve district crisis | -4 to -8 incumbent legitimacy |
| Gain major faction endorsement | -10 |
| Expose major scandal | -8 |
| Protect city during disaster | -12 |
| Control broadcast network | multiplier, not automatic victory |
| Harm civilians | restores incumbent legitimacy / hurts player |

At **25 or lower**:
mass defection events begin.

At **0**:
the incumbent government loses mandate.

The Leader Resolution becomes an election, abdication, public debate, coup prevention, or last-stand encounter depending on personality.

---

# 33. ALLIANCE CONQUEST

Primary meter:

# DIPLOMATIC RESOLVE

Unlike the other routes, the player reduces Resolve by proving that joining is better than remaining separate.

The alliance route requires four parallel components:

## TRUST
0–100

## SHARED INTEREST
0–100

## CRISIS RESOLUTION
0–100

## RESPECT
0–100

Final Alliance Readiness:

`(Trust + Shared Interest + Crisis Resolution + Respect) / 4`

Requirements for voluntary union:

- Alliance Readiness **80+**
- No component below **65**
- No active Major Grievance
- Civilization-specific Leader Quest complete

This is intentionally the hardest peaceful route.

Its reward is exceptionally high post-conquest Loyalty.

---

# 34. HYBRID CONQUEST

The player can mix methods.

Example:

- Economic pressure reduces Forgeweave's ability to supply its military.
- Influence campaigns create worker strikes.
- A surgical assault captures the final Command Foundry.
- Negotiation convinces Daxton Rhe to join.

The game records the final conquest profile:

**Force 35% / Economic 30% / Influence 20% / Alliance 15%**

This profile affects:

- post-conquest Loyalty,
- Leader relationship,
- available Ascension choices,
- future world reputation.

---

# 35. CONQUEST SCORE

Every conflict tracks hidden strategy weights based on successful actions.

At resolution the game assigns one of several conquest identities:

- Liberator
- Unifier
- Investor
- Strategist
- Conqueror
- Protector
- Manipulator
- Destroyer
- Coalition Builder
- Technocrat

These are reputational descriptors, not morality scores.

NPCs react to the player's actual historical behavior.

---

# 36. LEADER ENCOUNTER RULE

Every Leader encounter has three conceptual phases.

## PHASE A — THEIR STRENGTH

The Leader demonstrates the civilization's greatest advantage.

## PHASE B — THEIR PROBLEM

Their civilization's systemic weakness destabilizes the encounter.

## PHASE C — YOUR ANSWER

The player uses combat, prior choices, city capabilities, or dialogue to resolve the contradiction.

A Leader battle should therefore teach the thesis of that civilization.

---

# 37. THE 20 LEADER ENCOUNTER CONCEPTS

| Civilization | Leader | Encounter Thesis |
|---|---|---|
| **Synara** | Archon Mira Vey | Predictive AI anticipates player actions until human unpredictability breaks the model. |
| **Concord Vale** | Mayor Solenne Hart | Several factions fight over the same arena; victory requires preventing social collapse while confronting Hart. |
| **Hearthspire** | Governor Cassian Rowe | A vertical-city evacuation becomes a battle against cascading infrastructure overload. |
| **Nova Crucible** | Director Kael Orin | Experimental weapons continuously mutate arena rules; the player can stabilize or exploit them. |
| **Terravanta** | Warden Ilyra Stone | Battlefield terrain physically reforms around the player and becomes unstable from overuse. |
| **Virelia** | Dr. Seraphine Voss | Voss evolves defenses in response to damage types until accumulated Mutation becomes dangerous to everyone. |
| **Forgeweave** | Forge Lord Daxton Rhe | An industrial fortress continuously manufactures reinforcements until its resource network is broken or redirected. |
| **Eden Circuit** | Caretaker Amara Venn | Living terrain heals defenders; excessive destruction causes the ecosystem itself to become hostile. |
| **Automara** | Marshal AX-9 Aya | The player must decide whether to destroy, liberate, or persuade the synthetic army whose loyalty is shifting. |
| **Aethernet** | Nexus Regent Lyra Quinn | Every system is interconnected; disabling one node changes the entire arena and may cause cascade failure. |
| **Nocturne Bastion** | Commander Orin Black | Surveillance predicts routes and locks sectors; the player succeeds by manipulating what the system believes is threatening. |
| **Veyra** | Rider-King Talon Vey | High-speed mobile battle through transit corridors whose infrastructure progressively wears down. |
| **Civic Meridian** | First Steward Adira Sol | The battle is governed by emergency laws and civic systems; the player can exploit bureaucracy or preserve legitimacy. |
| **Auric Ledger** | Chancellor Nyra Vale | Financial assets fund defenses in real time; controlling exchanges changes the boss's available abilities. |
| **Signal Republic** | Luminary Zara Vox | Perception changes battlefield support; civilian belief and broadcast control alter which “truth” gains resources. |
| **Lexara** | High Arbiter Cassia Rune | The encounter runs under changing legal restrictions that both sides can invoke, challenge, or overturn. |
| **Mnemosyne** | Provost Elian Sage | The enemy learns the player's patterns; changing tactics is more important than raw damage. |
| **Vector Forge** | Route Marshal Juno Strake | The boss is almost impossible while supplied; the fight is about routing and severing logistics. |
| **Nomad Reach** | Pathfinder Keira Ash | No fixed arena; the Leader relocates command through mobile settlements and frontier terrain. |
| **Epoch Vault** | Chronarch Elara Prime | The game previews likely player actions and future arena states, but reacting to those forecasts changes them. |

---

# 38. NONLETHAL LEADER RESOLUTION

Every Leader should support multiple outcomes where narratively appropriate.

Possible outcomes:

- Recruited
- Appointed Governor
- Allied
- Retired
- Exiled
- Imprisoned
- Released
- Missing
- Defiant opposition
- Deceased

Not every outcome is available every campaign.

The history of the conflict determines the available choices.

---

# 39. LEADER RELATIONSHIP

Conquered Leaders track:

- Respect
- Trust
- Fear
- Grievance
- Ideological Alignment
- Personal Loyalty

These values affect:

- whether the Leader joins you,
- whether they accept governorship,
- unique quests,
- rebellion risk,
- secret fusion abilities,
- final Axiom Crown events.

---

# 40. POST-CONQUEST TERRITORY STATE

A conquered civilization is not immediately “done.”

Each territory tracks:

## LOYALTY
Willingness to remain under your dominion.

## STABILITY
Ability to function without crisis.

## PROSPERITY
Economic health.

## AUTONOMY
Local political freedom.

## IDENTITY
Strength of local cultural identity.

Identity is not something the player should simply erase.

Attempts to suppress Identity may increase short-term control and long-term rebellion.

---

# 41. INITIAL LOYALTY BY CONQUEST STYLE

Suggested baseline:

| Resolution | Starting Loyalty |
|---|---:|
| Voluntary Alliance | 80 |
| Influence transfer | 65 |
| Economic union | 55 |
| Surgical military surrender | 45 |
| Destructive military conquest | 25 |

Modifiers include:

- civilian harm,
- infrastructure damage,
- treatment of Leader,
- local faction support,
- cultural compatibility,
- pre-war relationship.

---

# 42. GOVERNANCE MODE

After conquest choose one governance model.

## DIRECT RULE

High resource extraction.

- lower Autonomy
- higher rebellion risk.

## GOVERNORSHIP

Appoint a Leader.

Balanced control.

## PROTECTORATE

High local Autonomy.

Lower direct income.

## FEDERATED MEMBER

High Loyalty and Identity retention.

Lower centralized control.

## SPECIAL ADMINISTRATIVE ZONE

Economic integration without full political absorption.

Different choices are better for different civilizations.

---

# 43. REBELLION

Rebellion risk is evaluated from:

- Loyalty
- Stability
- Identity suppression
- Leader grievances
- economic hardship
- occupation force
- Dominion Pressure
- neighboring rivals

A revolt does not instantly flip the whole civilization.

It begins through:

- strikes,
- sabotage,
- demonstrations,
- tax refusal,
- militia formation,
- district secession,
- full rebellion.

The player has time to respond.

---

# 44. REBELLION RESPONSES

The Founder can answer through:

- concessions,
- investment,
- policy reform,
- local elections,
- cultural protection,
- negotiation,
- security,
- replacement governor,
- economic aid,
- force.

Every response changes the long-term history of the empire.

---

# 45. ASCENSION SEQUENCE

A conquest concludes with a major world event.

## STEP 1 — SILENCE

Combat simulation pauses.

The conquered city's civilization theme fades into ambient sound.

## STEP 2 — RELIC AWAKENING

The civilization Relic emerges from its Wonder, capital, or hidden vault.

## STEP 3 — WORLD TRANSIT

The Relic crosses Eidolon toward the player's capital.

The world map visibly reacts.

## STEP 4 — CAPITAL TRANSFORMATION

A new architectural district seed appears in the player's capital.

## STEP 5 — CARD LIBERATION

The conquered faction's eligible card pool unlocks.

## STEP 6 — DOCTRINE ACQUISITION

The Dominion Doctrine becomes available.

## STEP 7 — LEADER DECISION

The player resolves the Leader's status.

## STEP 8 — DOMINION PRESSURE

Remaining civilizations update diplomacy, military posture, and alliances.

## STEP 9 — FUSION CHECK

New technology combinations are discovered.

---

# 46. DOMINION PRESSURE

Dominion Pressure is a global adaptation score.

Base gain per conquest:

**+5**

Additional pressure:

| Behavior | Pressure |
|---|---:|
| Destructive conquest | +2 |
| Leader execution | +2 |
| Civilian catastrophe | +3 |
| Peaceful alliance | -1 |
| High-autonomy federation | -1 |
| Broken alliance | +4 |

Major thresholds:

- **15**: Rival fortification
- **25**: Defensive alliances
- **40**: Champion hunters
- **50**: Technology sharing
- **65**: Anti-Founder coalition
- **80**: Total-war economies
- **95**: Axiom interference
- **100**: Axiom Crown awakening conditions accelerate

This is more flexible than conquest count alone.

---

# 47. AI ADAPTATION

Remaining civilizations should learn from the player.

The world tracks:

- most-used damage channel,
- preferred conquest path,
- most-used unit class,
- most-used Dominion Doctrine,
- reliance on automation,
- reliance on heavy industry,
- civilian-damage history,
- supply dependence.

Enemies begin countering repeated strategies.

Examples:

- Heavy Arc usage → grounded armor.
- Constant Influence conquest → independent media institutions.
- Repeated network attacks → manual fallback systems.
- Reliance on drones → anti-swarm defenses.
- Repeated economic coercion → trade diversification.

This keeps conquest from becoming a solved routine.

---

# 48. FORCE DOES NOT MEAN EVIL

Military conquest can be liberating or oppressive depending on conduct.

A surgical military campaign that:

- protects civilians,
- captures infrastructure,
- defeats an abusive regime,
- accepts surrender,
- restores services

can produce better Loyalty than a manipulative economic campaign.

The system judges consequences, not labels.

---

# 49. ECONOMIC WARFARE RULES

Economic conquest uses actual simulated systems.

The player may:

- subsidize rival industries,
- buy supply contracts,
- establish trade corridors,
- purchase stakes in facilities,
- undercut markets,
- offer emergency credit,
- control critical exports,
- invest in local infrastructure.

Hard rule:

> The player cannot press a generic “buy civilization” button.

Every percentage of leverage must have an actual economic source.

---

# 50. INFLUENCE WARFARE RULES

Influence campaigns require truth, service, culture, narrative, or ideology.

Influence is generated through actual assets and actions.

Examples:

- Signal towers distribute broadcasts.
- Civic centers host events.
- Founder missions solve problems.
- Factions endorse the player.
- Citizens compare living standards.
- Rival scandals become public.

Repeated misinformation can generate short-term gains but creates a hidden **Credibility Debt** that may collapse later.

---

# 51. DIPLOMACY RULES

Diplomatic union should require the player to understand the rival's unique problem.

Examples:

### Forgeweave
Solve Resource Hunger without destroying industrial identity.

### Eden Circuit
Prove expansion can occur without ecological collapse.

### Automara
Resolve machine citizenship.

### Auric Ledger
Prevent both economic collapse and authoritarian financial control.

### Epoch Vault
Demonstrate that uncertainty can be survived rather than perfectly predicted.

The Alliance route is essentially the “understand this civilization deeply” route.

---

# 52. THE FIRST FULL CONQUEST SCENARIO
# OPERATION IRON VEIL

**Player:** Synara  
**Target:** Forgeweave  
**Neutral power:** Eden Circuit

This is the recommended vertical-slice climax.

---

# 53. REGIONAL SETUP

Forgeweave controls:

- Ironheart Industrial District
- Smelter Quarter
- Grand Freight Corridor
- Foundry Citadel
- Three worker residential zones
- One regional mineral source

Eden Circuit controls:

- Verdant Basin
- River restoration network
- food supply corridor

Synara controls:

- player-built capital sector
- network relay outposts
- research corridor

The central conflict:

Forgeweave's industrial economy is consuming resources faster than the region can replenish them.

Daxton Rhe refuses to slow production because doing so would collapse employment, defense, and political legitimacy.

---

# 54. FORGEWEAVE WAR STATE

Starting conquest meters:

- Military Sovereignty: **100**
- Economic Autonomy: **100**
- Civic Legitimacy: **82**
- Diplomatic Resolve: **95**

Unique systemic meter:

# RESOURCE HUNGER — 62/100

At 75:
factory maintenance rises.

At 85:
civilian shortages begin.

At 100:
industrial collapse event begins.

---

# 55. FORCE ROUTE — IRON VEIL

Major objectives:

1. Capture Grand Freight Corridor.
2. Disable two Armor Works.
3. Breach Foundry Citadel.
4. Disable the Replication Forge.
5. Defeat Daxton Rhe.

Optional preservation objectives:

- Keep Worker Arcologies above 75% integrity.
- Preserve the Smog Reclaimer.
- Do not destroy the mineral refinery.
- Accept surrender of Forge Guard squads.

Success:
Military Sovereignty reaches 0.

Best-case Force outcome:
Daxton respects the player's efficiency and may accept Governorship.

Worst case:
Forgeweave is physically devastated and begins at extremely low Loyalty.

---

# 56. ECONOMIC ROUTE — THE SUPPLY NOOSE

Objectives:

1. Become primary supplier of coolant and machine components.
2. Control at least 40% of regional freight.
3. Build a Synara-Eden alternative materials network.
4. Refinance Forgeweave's failing industrial reserve.
5. Force or negotiate conversion of the Infinite Foundry supply contract.

Economic Autonomy falls as Forgeweave becomes dependent.

Climax:
Daxton attempts an emergency production surge.

The player must decide whether to:

- let it fail,
- rescue it,
- seize it,
- restructure it.

Different choices shape post-conquest Loyalty.

---

# 57. INFLUENCE ROUTE — THE WORKERS' SIGNAL

Objectives:

1. Investigate unsafe industrial conditions.
2. Build an Agency Forum near Forgeweave worker districts.
3. Support or oppose worker coalitions.
4. Publicly solve a regional pollution crisis.
5. Win endorsements from three Forgeweave civic factions.

Climax:
A mass assembly occurs outside the Grand Forge.

Daxton confronts the Founder publicly.

This can become:

- debate,
- riot prevention,
- assassination prevention,
- duel,
- peaceful transfer.

---

# 58. ALLIANCE ROUTE — THE THIRD FOUNDRY

The player must prove Forgeweave can survive without endless extraction.

Requirements:

- Trust 80+
- Shared Interest 80+
- Crisis Resolution 80+
- Respect 80+

Core quest:

Create the first **Regenerative Industrial Loop** using:

- Synara optimization,
- Forgeweave production,
- Eden Circuit regeneration.

The result is a prototype manufacturing district that maintains output without increasing Resource Hunger.

Daxton's final encounter is not a boss fight.

It is a live industrial crisis.

The player, Daxton, and Amara Venn must keep the new system alive during cascading failures.

If successful, Forgeweave voluntarily joins.

This unlocks a superior relationship with Daxton and a unique early Fusion variant.

---

# 59. DAXTON RHE BOSS ENCOUNTER

If conflict reaches combat:

# PHASE I — THE FORGE LORD

Daxton uses:

- powered industrial armor,
- heavy structural attacks,
- deployable cover,
- Forge Guard reinforcements.

The Grand Forge manufactures additional defenses.

Primary lesson:
**production is his power.**

---

# 60. DAXTON PHASE II — OVERDRIVE

At 60% encounter health / equivalent progress:

Daxton activates Industrial Overdrive.

Effects:

- faster reinforcement production,
- factory heat increases,
- coolant demand doubles,
- Resource Hunger rises rapidly.

The player can:

- keep fighting,
- disable coolant,
- hack production,
- redirect supply,
- convince workers to shut lines down.

Primary lesson:
**his greatest strength is consuming him.**

---

# 61. DAXTON PHASE III — THE CHOICE

The Grand Forge enters catastrophic overload.

The Founder chooses between objectives.

### DEFEAT DAXTON
Fastest military ending.

### SAVE THE GRAND FORGE
Preserves the Wonder and gains Respect.

### EVACUATE THE WORKERS
Gains Loyalty and Influence.

### STABILIZE PRODUCTION
Requires sufficient Vision/Enterprise and prior research.

### OFFER UNION
Available only if relationship conditions are met.

The final gameplay objective changes based on the choice.

The boss therefore ends in a civilization decision rather than a health bar alone.

---

# 62. FIRST ASCENSION REWARD

Conquering or allying with Forgeweave unlocks:

# REPLICATION

Periodically duplicate one non-Legendary Asset Card.

Also unlock:

- Forgeweave Core Card Pool
- Forgeweave class lineage
- Daxton relationship
- Forgeweave architecture
- Forge Relic
- industrial district skin
- first Synara + Forgeweave Fusion research

---

# 63. AUTONOMOUS FACTORY FUSION

**Synara + Forgeweave**

Installable Fusion Module.

Effects:

- -80% workforce requirement
- +25% throughput
- +15% construction speed to adjacent Industrial assets

Consequences:

- +2 Synara Dependency every 3 cycles
- +1 Forgeweave Resource Hunger each cycle
- if both systemic meters are in Crisis, the factory can enter runaway production

Runaway production creates a special emergency where the facility continuously manufactures goods while consuming city resources until shut down.

This establishes the fusion rule:

> **You inherit weaknesses with strengths.**

---

# 64. EDEN CIRCUIT'S ROLE IN THE VERTICAL SLICE

Eden Circuit is not merely scenery.

Amara Venn evaluates both civilizations.

Her support changes based on the player's behavior.

Eden may:

- provide regenerative materials,
- deny food supply,
- mediate negotiations,
- condemn environmental destruction,
- provide Rangers,
- open hidden terrain routes.

The player's treatment of Eden creates a relationship that persists after the Forgeweave conquest.

This demonstrates the campaign's multi-party diplomacy.

---

# 65. COMBAT CARD INTERACTION

Cards in the player's City Deck can influence operations in three ways.

## DEPLOYED ASSETS

Already-built units and vehicles can be assigned to the operation.

## FIELD CARDS

Certain cards have **Expedition** capability and may be constructed in captured or controlled zones.

Examples:

- Field Relay
- Temporary Barrier
- Mobile Fabricator
- Forward Clinic

They still require Capital, support, and a valid card.

## STRATEGIC SUPPORT

Certain city assets project effects into battle.

Examples:

- Intelligence Citadel → improved Recon
- Horizon Rail → faster reinforcement
- Infinite Vault → larger emergency reserve
- Academy Tower → higher veteran starting rank

The city and combat remain mechanically linked.

---

# 66. FORWARD CONSTRUCTION

During sieges, construction is limited.

The player may build only:

- Infrastructure tagged **Field**
- Defense tagged **Deployable**
- facilities in fully captured districts
- faction-specific mobile structures

This prevents players from dropping a skyscraper in the middle of a firefight.

---

# 67. CONTROL ZONES

Important battlefield locations become Control Zones.

Examples:

- intersection,
- station,
- rooftop,
- plaza,
- command room,
- bridge,
- relay tower.

Holding Control Zones provides:

- CP generation,
- supply,
- respawn/reinforcement access,
- local intel,
- Sovereignty pressure.

This gives squads territory to fight over.

---

# 68. TACTICAL PAUSE

Standard mode:

Command Mode slows simulation to **20% speed**.

Accessibility option:
full pause.

High difficulty:
50% speed.

Iron Dominion:
real time.

This lets the game remain tactical without forcing high actions-per-minute play.

---

# 69. COMBAT DIFFICULTY

Difficulty should change AI and strategic tolerance more than raw enemy health.

## STORY
- forgiving Founder damage
- full Tactical Pause
- strong objective hints.

## STRATEGIST
- intended baseline.

## ASCENDANT
- faster enemy adaptation
- limited Command slow
- more meaningful supply.

## IRON DOMINION
- real-time Command Mode
- harsher Recovery
- persistent strategic losses
- high enemy adaptation.

Avoid turning harder difficulties into massive health multipliers.

---

# 70. BOSS DESIGN RULES

Every boss must:

1. Use the civilization's core mechanic.
2. Expose the civilization's systemic problem.
3. Respond to at least two non-damage systems.
4. Have destructible/interactable environment.
5. Recognize prior campaign choices.
6. Support at least one alternate resolution when narratively valid.
7. Unlock a conquest reward that changes future gameplay.

---

# 71. COMBAT READABILITY

The visual language must remain readable despite 3D city complexity.

Each faction gets:

- recognizable silhouettes,
- consistent material language,
- clear ability telegraphs,
- distinct unit role markers,
- faction-specific construction animation,
- faction-specific damage effects.

The UI should never rely solely on color.

---

# 72. DESTRUCTION VISUAL STANDARD

Buildings require at least:

- pristine state,
- damaged material state,
- module destruction state,
- disabled state,
- ruin state.

Large structures should use modular destruction so damage reflects where the building was hit.

Avoid instantly swapping a pristine tower for generic rubble.

---

# 73. CIVILIAN EVACUATION

Before high-risk sieges, the player can spend:

- Influence,
- time,
- transport capacity

to evacuate civilians.

Benefits:

- reduced collateral risk,
- better Loyalty.

Cost:

- gives enemy more preparation time,
- consumes transport,
- may reveal attack intentions.

This creates an actual strategic tradeoff.

---

# 74. SURRENDER

Enemy squads and facilities can surrender.

Surrender chance is influenced by:

- Morale,
- Military Sovereignty,
- Founder Presence,
- Leader status,
- nearby allies,
- civilian support,
- player's prior treatment of prisoners.

Accepted surrender improves:

- Influence,
- Loyalty,
- future surrender likelihood.

Mass mistreatment makes later enemies fight harder.

---

# 75. PRISONERS AND DEFEATED UNITS

The game should abstract prisoner logistics rather than becoming a detention simulator.

Captured combatants create strategic choices:

- Release
- Exchange
- Reintegrate
- Transfer to local authority

Treatment affects world reputation.

---

# 76. VICTORY WITHOUT ANNEXATION

A player may win a conflict and choose not to absorb the civilization.

Possible outcomes:

- Treaty
- Protectorate
- Reparations
- Trade union
- Military withdrawal
- Alliance
- Regime guarantee

However, only full **Ascension-compatible union** yields the civilization's Relic.

This allows role-playing without undermining the main campaign objective.

---

# 77. RELIC CONSENT

For peaceful unions, the civilization voluntarily activates its Relic.

For hostile conquest, the Relic activates after Sovereignty is broken.

The Relic is not merely stolen loot.

It is a civilization-scale key responding to a legitimate transfer of power under the world's ancient Axiom architecture.

This prevents the central quest from feeling like collecting twenty random gems.

---

# 78. CONQUEST FAILURE

A failed operation does not necessarily end the campaign.

Possible consequences:

- retreat,
- captured unit cards,
- lost forward district,
- diplomatic humiliation,
- enemy counterattack,
- reduced local support,
- higher rival readiness.

The player can recover.

Failure should create new stories rather than constant reload pressure.

---

# 79. COUNTER-SIEGES

Rivals can attack the player's city using the same rules.

Your own placement decisions matter defensively.

Poorly designed cities may have:

- exposed power,
- single-point network failure,
- vulnerable logistics,
- undefended residential districts.

This is where the city-building strategy becomes combat strategy.

---

# 80. CITY DEFENSE DOCTRINE

Before leaving the capital, the player sets defensive AI priorities.

Examples:

- Protect civilians
- Protect Wonder
- Protect utilities
- Hold perimeter
- Preserve infrastructure
- Counterattack
- Evacuate if breached

Your civilization continues behaving while the Founder is elsewhere.

---

# 81. MULTI-FRONT WAR

Late game allows multiple simultaneous conflicts.

The player cannot personally attend all fronts.

Governors and Leaders become important.

A recruited Leader can command a theater using their own strategic AI tendencies.

This gives conquered Leaders real mechanical value.

---

# 82. GOVERNOR WAR PERSONALITY

Leaders have strategic traits.

Example:

### Daxton Rhe
- excellent industrial defense
- high willingness to escalate
- low willingness to retreat.

### Amara Venn
- excellent terrain defense
- high civilian preservation
- avoids destructive counterattack.

### Lyra Quinn
- excellent cyber defense
- aggressively attacks enemy networks.

Appointing Governors becomes a strategic choice.

---

# 83. WAR EXHAUSTION

Civilizations track **War Exhaustion 0–100**.

Increases from:

- prolonged fighting,
- casualties,
- civilian disruption,
- shortages,
- repeated invasions.

High War Exhaustion causes:

- lower Happiness,
- lower production,
- stronger peace factions,
- reduced recruitment.

Successful defense, victories, rest, propaganda, and peace reduce it.

This prevents endless war from being free.

---

# 84. OCCUPATION FORCE

Directly ruled hostile territory may require units stationed there.

Garrisoning units:

- raises Security,
- reduces military availability elsewhere,
- may lower Freedom,
- can increase resentment if excessive.

Political solutions should eventually outperform permanent occupation.

---

# 85. VERTICAL-SLICE IMPLEMENTATION ORDER

The combat prototype should be built in this order.

## MILESTONE 1 — FOUNDER COMBAT SANDBOX

Implement:

- movement,
- camera,
- Health/Guard/Stamina,
- one primary tool,
- one utility,
- cover interaction,
- one enemy squad.

Success:
Founder combat is readable and satisfying.

---

## MILESTONE 2 — ONE SQUAD + COMMAND MODE

Implement:

- one allied squad,
- selection,
- move,
- attack,
- hold,
- cover,
- Morale,
- Tactical Pause.

Success:
Founder and squad complement each other.

---

## MILESTONE 3 — BUILDING COMBAT

Implement:

- one factory,
- modules,
- Structural Integrity,
- Disabled state,
- capture,
- repair,
- ruin.

Success:
destroying and capturing feel meaningfully different.

---

## MILESTONE 4 — SIEGE BLOCK

Implement:

- 8×8 combat district,
- three Control Zones,
- supply,
- reinforcement,
- CP,
- Force Sovereignty meter.

Success:
battle has tactical territory rather than arena combat only.

---

## MILESTONE 5 — DAXTON BOSS

Implement:

- three phases,
- Grand Forge production loop,
- Overdrive,
- environmental crisis,
- at least two endings.

Success:
boss communicates Forgeweave's philosophy.

---

## MILESTONE 6 — FOUR CONQUEST ROUTES

Implement:

- Force
- Economic
- Influence
- Alliance

with the same target city.

Success:
the same city supports four genuinely different strategies.

---

## MILESTONE 7 — ASCENSION

Implement:

- Forge Relic
- world-map transit
- capital transformation
- card unlock
- Replication Doctrine
- Daxton outcome
- Autonomous Factory fusion.

Success:
the player immediately wants to conquer another civilization.

---

# 86. PLAYTEST METRICS

Track:

- average operation duration,
- Founder damage received,
- squad casualties,
- structures destroyed vs captured,
- civilian impact,
- Command Mode frequency,
- CP spending,
- supply failures,
- conquest route chosen,
- route switching,
- Leader outcome,
- post-conquest Loyalty.

---

# 87. COMBAT SUCCESS QUESTIONS

The combat layer is ready to scale only if testers answer yes:

1. Does your city composition noticeably affect your military options?
2. Does the Founder feel important without making squads irrelevant?
3. Does Command Mode add strategy without excessive micromanagement?
4. Is taking a building alive more interesting than simply destroying it?
5. Do supply and infrastructure matter without becoming tedious?
6. Does a Forgeweave battle feel mechanically different from Synara?
7. Can a player understand why they are winning or losing a conquest meter?
8. Are the four conquest paths all engaging enough to be legitimate primary playstyles?
9. Does the Leader encounter express the faction's philosophical conflict?
10. Does Ascension materially change the next chapter of the game?

---

# 88. CANONICAL COMBAT LOOP

> **Prepare in your city → gather intelligence → deploy real assets → establish supply → take objectives → preserve or destroy infrastructure → break sovereignty → confront the Leader → decide the civilization's future → Ascend → return home changed.**

---

# 89. DESIGN MANTRA

# BUILDING IS STRATEGY.
# INFRASTRUCTURE IS POWER.
# WAR HAS A PRICE.
# CONQUEST HAS CONSEQUENCES.
# EVERY VICTORY CHANGES THE WORLD.
