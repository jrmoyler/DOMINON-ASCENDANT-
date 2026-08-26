# DOMINION // ASCENDANT
## Citizen, Card Acquisition & World Campaign Systems Bible — v0.4

**Status:** Canonical design pass following Game Bible v0.1, Playable Systems Bible v0.2, and Combat/Siege/Conquest Bible v0.3.

> **A city is not buildings. A civilization is not cards. The people living between them are the game state that gives both meaning.**

---

# 1. PURPOSE OF THIS DESIGN PASS

This document specifies the persistent simulation layer for **DOMINION // ASCENDANT**:

- Citizen population model
- Jobs and workforce
- Class progression
- Education and specialization
- Citizen factions
- Loyalty, ideology, and migration
- Social mobility
- Happiness, freedom, inequality, and unrest
- Card acquisition
- Crafting and discovery
- Duplicate handling
- Non-predatory pack-like rewards
- Collection progression
- World map structure
- Rival civilization AI
- Diplomacy
- Alliances
- Trade
- Espionage
- Dynamic world events
- Rival expansion and conquest
- Campaign pacing
- Endgame progression toward the Axiom Crown

The governing rule:

> **Every major system should produce stories visible in the world.**

If a city has low housing, the result should not merely be `-12 Stability`.

Citizens should:

- leave,
- protest,
- form movements,
- overcrowd districts,
- vote differently,
- change jobs,
- join rivals,
- create quests,
- become leaders,
- or radicalize.

---

# 2. CITIZEN SIMULATION PHILOSOPHY

Citizens exist at three layers simultaneously.

## INDIVIDUAL LAYER

Important citizens have:

- name,
- age band,
- occupation,
- class,
- home,
- workplace,
- skills,
- faction,
- loyalty,
- relationships,
- traits,
- personal needs,
- experience,
- history.

These are citizens the player can meet in Founder Mode.

---

## COHORT LAYER

Most population simulation runs through cohorts.

Example:

**Adaptive Habitat 14B**
- 8 residents
- median Education 62
- Happiness 71
- Loyalty 68
- 3 machine-positive
- 2 autonomy-focused
- 2 politically neutral
- 1 anti-automation

This prevents the game from simulating thousands of fully autonomous RPG agents at maximum fidelity.

---

## CIVILIZATION LAYER

Aggregated metrics:

- Population
- Happiness
- Health
- Education
- Security
- Wealth
- Equality
- Ecology
- Mobility
- Freedom
- Stability
- Innovation
- Loyalty
- War Exhaustion
- Factionalism
- Employment

All three layers feed one another.

---

# 3. CITIZEN PERSISTENCE

Named citizens persist across the campaign.

They can:

- level up,
- change jobs,
- move homes,
- marry or form households,
- mentor others,
- become champions,
- become governors,
- defect,
- migrate,
- become activists,
- be injured,
- retire,
- die through major story events or high-difficulty rules.

The city develops a remembered population history.

---

# 4. CITIZEN DATA MODEL

Each persistent citizen stores:

- Citizen ID
- Name
- Origin Civilization
- Current Civilization
- Home District
- Residence Asset
- Workplace Asset
- Occupation
- Class
- Class Rank
- Education
- Health
- Wealth Tier
- Happiness
- Personal Loyalty
- Political Faction
- Cultural Identity
- Traits
- Relationships
- Grievances
- Aspirations
- Service History
- Combat History if applicable
- Migration History
- Notable Events

Not every field must be shown to the player at all times.

---

# 5. POPULATION TIERS

Population is grouped into four gameplay tiers.

## RESIDENT

Normal citizen.

Provides:
- workforce,
- taxes,
- local demand,
- population metrics.

## SPECIALIST

Skilled citizen.

Examples:
- engineer,
- researcher,
- mediator,
- financier,
- surgeon,
- analyst.

Provides stronger facility output.

## ELITE SPECIALIST

High-level professional.

Can:
- train others,
- unlock advanced facility functions,
- run special projects.

## CHAMPION

Named exceptional character represented by a Champion Card.

Champions can:
- accompany the Founder,
- command squads,
- govern districts,
- perform special missions,
- become political actors.

---

# 6. HOUSEHOLDS

Residential buildings contain households rather than anonymous capacity alone.

Households have:

- size,
- income,
- education,
- needs,
- culture,
- faction tendencies.

Household behavior affects:

- consumption,
- migration,
- schooling,
- political support,
- retail demand.

This gives Residential cards actual strategic identity.

---

# 7. JOB SYSTEM

Every staffed facility creates Jobs.

Example:

### Cognitive Operations Tower — Level I

Requires:
- 4 Operators
- 2 Analysts
- 1 Administrator

Optional:
- 1 Elite Orchestrator

At full staffing:
100% output.

At 75% staffing:
90% output.

At 50% staffing:
65% output.

Below 25%:
facility enters Staff Crisis.

---

# 8. JOB MATCH QUALITY

Citizens are not interchangeable.

Each job has:

- minimum Education
- preferred class
- skill tags
- faction compatibility where relevant
- commute tolerance

Job Match Quality:

| Match | Output |
|---|---:|
| Excellent | 115% |
| Good | 100% |
| Acceptable | 85% |
| Poor | 65% |
| Unqualified | cannot fill specialized role |

This creates demand for education, housing, transit, and migration.

---

# 9. COMMUTE

Citizens evaluate:

- travel time,
- transit quality,
- safety,
- congestion.

Long commute effects:

- lower Happiness,
- lower facility efficiency,
- higher migration risk.

Veyra and Vector Forge can build extremely efficient commuting systems.

Hearthspire reduces commute through vertical mixed-use planning.

---

# 10. UNEMPLOYMENT

Unemployment is not automatically catastrophic.

Short-term unemployment:
- increases available labor,
- lowers wage pressure.

Long-term unemployment:
- reduces Happiness,
- increases faction recruitment,
- reduces Stability,
- can increase crime or protest.

Policies and civic assets influence outcomes.

---

# 11. CLASS SYSTEM

The six universal archetypes remain:

- Architect
- Vanguard
- Scholar
- Operator
- Envoy
- Warden

Each civilization transforms these into faction classes.

Example:

## SYNARA

Architect → Systems Architect  
Vanguard → Swarm Marshal  
Scholar → Intelligence Auditor  
Operator → Orchestrator  
Envoy → Agency Mediator  
Warden → Protocol Guardian

---

# 12. CLASS PROGRESSION

Citizens progress through:

## INITIATE

Entry-level.

## PRACTITIONER

Competent professional.

## SPECIALIST

Advanced worker.

## MASTER

Elite specialist.

## ASCENDANT

Rare civilization-defining rank.

Class progression requires:

- Experience
- Education
- facility access
- specific achievements
- sometimes Insight expenditure

---

# 13. EXPERIENCE SOURCES

Citizens gain class experience from:

- working,
- completing projects,
- emergencies,
- combat,
- training,
- mentorship,
- discoveries,
- major civic events.

Different civilizations accelerate different paths.

Mnemosyne excels at class development.

---

# 14. MULTICLASSING

After conquering another civilization, qualified citizens can combine class lineages.

Example:

**Synara Systems Architect**
+
**Forgeweave Mechanist**

becomes:

# SYNTHETIC INDUSTRIAL ARCHITECT

Requirements:

- Rank 3 in primary class
- Rank 2 in secondary lineage
- Fusion technology discovered
- training facility available

Multiclasses should be powerful but scarce.

---

# 15. EDUCATION SYSTEM

Education is generated by:

- schools,
- academies,
- libraries,
- specialist mentorship,
- research institutions.

Education affects:

- job qualification,
- Innovation,
- class growth,
- political behavior,
- migration attractiveness.

High Education without good opportunity increases Brain Drain.

---

# 16. SOCIAL MOBILITY

Citizens track opportunity.

High social mobility means:
- lower inequality pressure,
- greater loyalty,
- faster class advancement.

Low mobility means:
- elite classes remain concentrated,
- faction resentment grows,
- talented citizens leave.

Auric Ledger must manage this carefully.

---

# 17. WEALTH TIERS

Households exist across:

- Struggling
- Working
- Comfortable
- Affluent
- Elite

Wealth affects:

- housing demand,
- retail behavior,
- political priorities,
- migration,
- tax contribution.

The game should not imply wealthy automatically means happier.

---

# 18. CITIZEN NEEDS

Citizens evaluate six core need families:

## SHELTER
Housing quality and availability.

## SECURITY
Crime, conflict, safety.

## HEALTH
Medical and environmental conditions.

## OPPORTUNITY
Employment and advancement.

## BELONGING
Community and cultural identity.

## AGENCY
Freedom, political voice, control over life.

Different factions prioritize them differently.

---

# 19. PERSONAL HAPPINESS

Simplified model:

`Happiness = Needs + Relationships + Opportunity + Environment - Grievances`

Displayed 0–100.

The player sees causes rather than only the number.

Example:

**Citizen: Talia Ro**
- Happiness: 48
- +12 stable employment
- +8 nearby public park
- -15 long commute
- -11 automation anxiety
- -6 rent pressure

---

# 20. LOYALTY

Loyalty is distinct from Happiness.

A citizen can be unhappy but loyal.

Loyalty factors:

- cultural identity,
- Founder reputation,
- government fairness,
- faction affiliation,
- war history,
- economic opportunity,
- treatment of their home civilization.

---

# 21. FACTIONS

Each civilization begins with 3–5 native political/social factions.

Universal faction archetypes include:

- Traditionalists
- Reformers
- Industrialists
- Labor
- Security Bloc
- Ecologists
- Technologists
- Autonomists
- Integrationists
- Spiritual/Cultural Preservationists
- Merchant Coalition
- Expansionists

Civilizations express these differently.

---

# 22. FACTION MODEL

Each faction tracks:

- Support %
- Approval
- Organization
- Radicalization
- Resources
- Key Demands
- Leader
- Foreign Relationships

Support is population share.

Organization determines ability to act.

Radicalization determines how aggressively it acts.

---

# 23. FACTION ACTIONS

Factions may:

- petition,
- endorse,
- organize events,
- strike,
- protest,
- fund campaigns,
- demand policy,
- sabotage,
- form militias,
- negotiate with rivals,
- support secession.

The game creates escalation stages rather than immediate rebellion.

---

# 24. FACTION DEMANDS

Demands should be systemic.

Examples:

- Reduce automation.
- Expand housing.
- Stop industrial pollution.
- Recognize synthetic citizens.
- Lower taxation.
- Increase security.
- Reduce surveillance.
- Protect local culture.
- Expand education.
- End a war.

Accepting every demand is impossible.

---

# 25. POLITICAL CAPITAL

Influence is the currency used to act politically.

The player may spend Influence to:

- negotiate,
- enact reforms,
- organize referenda,
- appoint mediators,
- fund civic initiatives.

But Influence cannot simply erase grievances.

A policy must address the actual cause.

---

# 26. MIGRATION

Citizens can move:

- between districts,
- between player cities,
- between civilizations.

Migration considers:

- jobs,
- housing,
- safety,
- freedom,
- culture,
- education,
- war,
- taxes,
- family,
- faction persecution,
- environmental conditions.

---

# 27. IMMIGRATION

Rival citizens may voluntarily migrate to the player's civilization.

Benefits:
- workforce,
- specialists,
- cultural knowledge,
- foreign class access.

Challenges:
- housing demand,
- integration,
- faction tension,
- cultural identity.

Concord Vale handles integration especially well.

---

# 28. BRAIN DRAIN

Highly skilled citizens leave when:

- opportunities are low,
- wages are poor,
- Freedom is low,
- Education is high but class advancement is blocked,
- rival cities offer better research or careers.

Mnemosyne's unique problem heavily interacts with this system.

---

# 29. REFUGEES

War, ecological collapse, mutation events, or economic failure can produce refugees.

The player may:

- accept,
- partially accept,
- fund external aid,
- refuse.

This affects:

- Population
- Influence
- Stability
- foreign relations
- housing
- faction politics

---

# 30. CITIZEN STORY EVENTS

Named citizens can trigger personal story chains.

Examples:

- Researcher discovers forbidden technology.
- Worker becomes labor organizer.
- Synthetic citizen demands legal recognition.
- Child prodigy needs education access.
- Soldier returns traumatized from war.
- Merchant builds a private economic empire.
- Citizen falls in love with a rival civilization resident.
- Engineer exposes corruption.
- Governor's family becomes politically powerful.

These chains create emergent RPG content.

---

# 31. CHAMPION ASCENSION

A normal citizen can become a Champion.

Requirements may include:

- Master class rank
- major achievement
- high personal reputation
- unique story trigger

When elevated:

1. Citizen receives a Champion Card.
2. Their physical character remains in the world.
3. They can enter the player's deck.
4. They may join Founder operations.
5. They retain their personal history.

This turns city simulation into collectible-card progression.

---

# 32. CARD ACQUISITION PHILOSOPHY

Cards should be earned through play first.

The player should feel:

> **I obtained this because my civilization accomplished something.**

not:

> **I opened random boxes until I got lucky.**

---

# 33. CARD ACQUISITION SOURCES

Cards come from:

## CONSTRUCTION RESEARCH
Unlock standard faction assets.

## DISCOVERY
Find unique blueprints in the world.

## QUEST REWARDS
Earn specialist cards and Champions.

## CONQUEST
Unlock civilization card pools.

## CRAFTING
Build known blueprints.

## FACTION REPUTATION
Earn civic and political assets.

## EXPLORATION
Discover relics, vehicles, rare structures.

## TRADE
Purchase blueprints or assets from other civilizations.

## EVENTS
Limited world-event cards.

---

# 34. BLUEPRINTS VS CARDS

Important distinction:

## BLUEPRINT
Permanent knowledge allowing a card to be manufactured.

## CARD
A deployable asset license/object in the player's collection.

Unlocking a blueprint does not give infinite free cards.

Cards must still be:

- crafted,
- earned,
- traded,
- rewarded,
- duplicated through faction abilities.

---

# 35. CARD CRAFTING

Crafting requires:

- Capital
- sometimes Insight
- appropriate production facility
- blueprint
- craft time

Example:

**Adaptive Habitat**
- 8 Capital
- 1 cycle
- construction facility required

**Guardian Drone Cohort**
- 10 Capital
- 2 Insight
- Synthetic Fabrication Node
- 2 cycles

---

# 36. RARITY AND CRAFTING

## COMMON
Easy to craft.

## SPECIALIZED
Requires faction facility.

## ELITE
Requires advanced facility + Insight.

## LEGENDARY
Requires unique project, named creator, or story condition.

## MYTHIC
Requires world-scale achievement.

## DOMINION
Cannot be normally crafted.

---

# 37. ACQUISITION CACHE

The game may use visually satisfying reward “packs” called:

# DISCOVERY CACHES

But these are not purchased with premium currency.

Caches are earned by:

- conquest milestones,
- exploration,
- quests,
- seasonal world events,
- challenge completion.

A Cache reveals a small set of blueprints/resources/cards.

---

# 38. CACHE PROTECTION

Caches use deterministic protection.

Rules:

- no card can drop if the player lacks the infrastructure to ever use it unless it is a future-facing blueprint reward;
- duplicate protection prioritizes unowned cards;
- after 5 Caches without an Elite, the next contains Elite+;
- after 15 without Legendary, the next contains Legendary;
- Dominion cards never drop randomly.

No paid loot-box economy is required.

---

# 39. DUPLICATE HANDLING

Duplicate cards are useful.

Duplicates can be:

## KEPT
Needed for deck copy limits.

## FUSED
Combine duplicate copies into reduced upgrade cost.

## RECYCLED
Convert into materials.

## TRADED
Exchange with NPC markets or other systems.

## ARCHIVED
Permanent collection milestone.

---

# 40. CARD MASTERY

Cards gain **Mastery XP** when used.

Mastery rewards:

- cosmetic construction variants,
- alternate materials,
- animation variants,
- lore,
- small sidegrade modules.

Mastery should avoid permanent raw-stat power creep beyond controlled limits.

---

# 41. CARD OWNERSHIP VS DECK ACCESS

The player's Collection contains all owned cards.

A City Deck is the 60-card subset selected for a campaign city or operation.

A player can own 400+ cards without bringing them all into one build.

---

# 42. STARTER COLLECTION

A new player receives:

- one complete 60-card starter deck for chosen civilization,
- 20 Universal Foundation cards,
- 5 faction blueprints not yet crafted,
- Founder Hall,
- one basic Founder loadout,
- one faction Leader,
- zero foreign civilization cards.

The game should be fully playable without random acquisition.

---

# 43. WORLD MAP STRUCTURE

Eidolon uses a layered strategic world map.

## HOME REGION
Player capital and nearby frontier.

## OUTER RING
8 civilizations.

## MID RING
8 civilizations.

## INNER RING
4 civilizations.

## AXIOM CORE
Final region.

Connections are semi-fixed but campaign-specific.

---

# 44. CAMPAIGN RANDOMIZATION

At campaign start:

- civilization locations shuffle within ring constraints;
- diplomatic relationships randomize;
- some resource regions move;
- rival Leaders receive personality modifiers;
- 2–4 world crises are seeded;
- neutral minor settlements appear dynamically.

This creates replayability without fully procedural lore.

---

# 45. TRAVEL

The Founder travels through:

- roads,
- rail,
- air,
- frontier paths,
- faction-specific transport,
- fast travel once established.

Travel time exists on the strategic layer.

While traveling, rival civilizations continue acting.

---

# 46. WORLD TIME

World simulation advances in:

# WORLD TICKS

One World Tick occurs every **5 Development Cycles** during normal play.

Major strategic actions resolve on World Ticks.

Examples:

- rival construction,
- trade changes,
- diplomatic shifts,
- army movement,
- faction events,
- migration.

---

# 47. RIVAL CIVILIZATION AI

Every rival civilization operates through five strategic planners.

## DEVELOPMENT AI
Builds the city.

## ECONOMY AI
Manages trade and production.

## DIPLOMACY AI
Handles relations.

## SECURITY AI
Plans defense and war.

## CIVIC AI
Responds to population problems.

A higher-level **Leader Personality** weights all five.

---

# 48. RIVAL CITY DEVELOPMENT

Rivals physically develop their cities using the same card/asset rules.

They:

- construct buildings,
- upgrade districts,
- create units,
- respond to shortages,
- rebuild after attacks.

The rival city the player visits should visibly change over time.

---

# 49. RIVAL DEVELOPMENT BUDGET

Rivals receive simulated Capital, Insight, and Influence.

They do not cheat with infinite resources under standard difficulty.

Higher difficulties may receive modest efficiency bonuses rather than arbitrary free armies.

---

# 50. LEADER PERSONALITY WEIGHTS

Each Leader has strategic tendencies.

Example:

## Daxton Rhe
- Industry: 100
- Defense: 80
- Diplomacy: 40
- Ecology: 15
- Expansion: 70

## Amara Venn
- Ecology: 100
- Diplomacy: 75
- Defense: 55
- Expansion: 35
- Industry: 30

These weights influence choices.

---

# 51. RIVAL MEMORY

Rivals remember:

- promises,
- betrayals,
- aid,
- invasions,
- trade behavior,
- civilian harm,
- treatment of Leaders,
- broken treaties,
- shared wars.

Relationship changes should cite reasons.

Example:

**Aethernet Trust: 42**
- +12 shared research treaty
- +8 defended network hub
- -18 espionage discovered
- -10 alliance with Nocturne

---

# 52. DIPLOMATIC RELATIONSHIP

Every civilization tracks:

- Trust
- Respect
- Fear
- Dependence
- Grievance
- Ideological Compatibility

These combine into diplomatic stance.

---

# 53. DIPLOMATIC STANCES

- Allied
- Friendly
- Cooperative
- Neutral
- Wary
- Hostile
- Rival
- At War
- Existential Enemy

Stance is not a single reputation number.

---

# 54. TREATIES

Possible agreements:

- Trade Agreement
- Research Exchange
- Defensive Pact
- Non-Aggression Pact
- Transit Agreement
- Migration Accord
- Intelligence Sharing
- Resource Compact
- Environmental Treaty
- Technology Licensing
- Mutual Defense Alliance
- Federation Proposal

Each treaty creates actual mechanical effects.

---

# 55. TRADE

Trade deals specify real goods/capabilities.

Examples:

- Forgeweave exports machine components.
- Eden Circuit exports regenerative materials.
- Auric Ledger provides credit.
- Veyra provides transit capacity.
- Aethernet provides network services.

Trade routes can be disrupted.

---

# 56. MARKET PRICES

Regional supply and demand affect trade.

If Forgeweave's production collapses:
machine component prices rise.

If Eden Circuit has a bumper harvest:
food prices fall.

Players can profit from world changes.

Auric Ledger is especially powerful here.

---

# 57. ECONOMIC DEPENDENCE

Civilizations track how much critical supply comes from each partner.

High dependence creates:

- leverage,
- vulnerability,
- alliance pressure.

Economic conquest derives from this actual system.

---

# 58. ESPIONAGE

Espionage is performed by assets and specialists.

Operations include:

- gather intelligence,
- steal blueprint,
- reveal faction support,
- sabotage,
- manipulate trade,
- counterintelligence,
- plant false data,
- rescue asset.

Failure can damage Trust severely.

---

# 59. ESPIONAGE VISIBILITY

Every operation has:

- Detection Risk
- Attribution Risk

An operation may be detected without proving who caused it.

This creates geopolitical uncertainty.

---

# 60. ALLIANCES

Alliance is stronger than Friendly stance.

Allies can:

- join wars,
- share logistics,
- provide units,
- share research,
- accept migration,
- coordinate projects.

But allies can disagree.

---

# 61. ALLIANCE COHESION

Alliances track:

- Shared Threat
- Shared Prosperity
- Ideological Fit
- Trust
- Burden Balance

Low cohesion can fracture alliances.

---

# 62. MULTI-CIVILIZATION COALITIONS

Late-game civilizations may form coalitions.

Example:

**The Free Systems Pact**
- Aethernet
- Concord Vale
- Automara

Potential motivation:
fear of centralized Founder control.

Coalitions have:
- shared policy,
- war goals,
- economic cooperation,
- internal disagreements.

---

# 63. MINOR SETTLEMENTS

The world contains non-major settlements.

Examples:

- mining towns,
- research outposts,
- agricultural communes,
- freeports,
- abandoned ruins,
- refugee cities,
- nomadic camps.

They are not full conquest civilizations.

They can:
- trade,
- join,
- provide quests,
- become contested.

---

# 64. WORLD EVENTS

Dynamic events occur at local, regional, and global scales.

## LOCAL
District-level.

Example:
- factory accident,
- housing protest,
- disease cluster.

## REGIONAL
Multiple cities.

Example:
- drought,
- trade disruption,
- refugee wave.

## GLOBAL
Entire world.

Example:
- solar storm,
- Axiom signal,
- market crash,
- synthetic rights movement.

---

# 65. EVENT STRUCTURE

Every meaningful event should contain:

- Trigger
- Warning
- Escalation
- Player Choices
- Rival Responses
- Resolution
- Persistent Consequence

Avoid random “+5/-5” popups.

---

# 66. EVENT EXAMPLE — THE SILICON FEVER

Trigger:
Automara synthetic production rises rapidly.

Warning:
rare processor shortages.

Escalation:
Forgeweave buys global supply.

Possible player choices:
- stockpile,
- sell,
- develop substitute,
- broker treaty,
- sabotage production.

Persistent effect:
world robotics costs change for several chapters.

---

# 67. EVENT EXAMPLE — THE GREEN LINE

Trigger:
regional Ecology falls below threshold.

Eden Circuit creates a transnational ecological restoration proposal.

Civilizations choose:
- participate,
- refuse,
- exploit,
- undermine.

Outcome can create a permanent ecological alliance.

---

# 68. WORLD CRISES

Major crises are multi-stage event chains.

Examples:

- Continental resource collapse
- Machine personhood crisis
- Global information war
- Mutagen outbreak
- Infrastructure cascade failure
- Financial panic
- Axiom interference

A campaign seeds several crises but not all.

---

# 69. AXIOM INTERFERENCE

As Dominion Pressure rises, the Axiom Crown begins acting indirectly.

Early signs:

- strange signals,
- impossible forecasts,
- dormant infrastructure activating,
- unknown units,
- relic resonance.

Later:
- Axiom agents intervene,
- world events accelerate,
- rival technology mutates.

---

# 70. RIVAL EXPANSION

Rivals can claim:

- minor settlements,
- resource zones,
- frontier territory.

Expansion occurs physically on the world map.

Nomad Reach expands especially aggressively.

---

# 71. RIVAL CONQUEST

Rival civilizations can attack one another.

Possible outcomes:

- territory capture,
- protectorate,
- alliance,
- partial annexation,
- full conquest.

However:

> Only the Founder can perform full Relic Ascension.

Rivals may dominate one another but cannot complete the Axiom sequence.

---

# 72. WHY ONLY THE FOUNDER CAN ASCEND

The Founder's origin is connected to the ancient Axiom architecture.

This becomes a central mystery.

Rival Leaders eventually realize:
the player is not simply another ruler.

They are a dormant **Convergence Key**.

This gives narrative justification to the player's unique campaign role.

---

# 73. CIVILIZATION ELIMINATION

A rival civilization should rarely disappear permanently before the player reaches it.

If defeated by another rival:

- its Leader may flee,
- government may go underground,
- Relic becomes inaccessible,
- liberation quest appears.

The player can restore or absorb it later.

---

# 74. PROXY WARS

Civilizations may support third parties without declaring direct war.

They can send:

- units,
- money,
- technology,
- intelligence.

This creates geopolitical complexity.

---

# 75. DIPLOMATIC ULTIMATUMS

At high tension, rivals can issue demands.

Examples:

- withdraw troops,
- stop espionage,
- end trade embargo,
- release Leader,
- reduce border defenses,
- cancel alliance.

Refusal may escalate.

---

# 76. WAR DECLARATION

War requires a strategic cause.

Causes include:

- direct invasion,
- treaty violation,
- border conflict,
- ideological conflict,
- resource crisis,
- coalition war.

Surprise attacks are possible but heavily affect Trust and world reputation.

---

# 77. CASUS BELLI

The game tracks recognized justifications.

Examples:

- Self-defense
- Liberation
- Treaty enforcement
- Border claim
- Resource dispute
- Regime overthrow
- Dominion conquest

Other civilizations respond differently depending on justification.

---

# 78. WORLD REPUTATION

The Founder gains global reputational descriptors.

Examples:

- Reliable
- Expansionist
- Merciful
- Ruthless
- Innovative
- Manipulative
- Protective
- Wealthy
- Unpredictable
- Diplomatic

These emerge from actions.

---

# 79. CITY ATTRACTIVENESS

Every city has an **Attractiveness Score** based on:

- jobs,
- housing,
- safety,
- health,
- freedom,
- culture,
- education,
- environment,
- wages.

High Attractiveness drives migration.

---

# 80. CULTURAL IDENTITY

Each civilization has a Culture value representing preservation of its distinct identity.

After conquest, the player can:

- preserve,
- integrate,
- hybridize,
- suppress.

Hybridization unlocks unique architecture and events.

Suppression may reduce short-term resistance but creates long-term grievance.

---

# 81. HYBRID DISTRICTS

Once two civilizations integrate deeply, the player may create Hybrid Districts.

Example:

## SYNARA + FORGEWEAVE
**Autonomous Industrial Quarter**

Visual language:
- computational white surfaces
- exposed heavy machinery
- autonomous production rails

Mechanical identity:
- high throughput
- high automation
- combined systemic pressure.

---

# 82. HYBRID CULTURE

Hybridization can produce:

- new citizen identities,
- fusion classes,
- fusion architecture,
- unique festivals,
- mixed factions,
- new Leaders.

This ensures conquest changes culture, not only stats.

---

# 83. WORLD MAP RESOURCES

Strategic resource regions include:

- Metals
- Rare Minerals
- Biomass
- Water
- Energy
- Data Infrastructure
- Agricultural Land
- Ancient Axiom Sites

These influence city strengths.

---

# 84. RESOURCE CLAIMS

Territory around a resource must be:

- claimed,
- settled,
- protected,
- traded for,
- or shared.

Resource disputes create diplomacy.

---

# 85. ANCIENT AXIOM SITES

Hidden ruins contain:

- lore,
- technologies,
- Relic clues,
- Founder upgrades,
- Mythic blueprints.

Some activate only after specific Ascensions.

---

# 86. EXPLORATION LOOP

Exploration uses Founder Mode.

Loop:

1. Survey region.
2. Discover point of interest.
3. Resolve encounter.
4. Recover Blueprint/Artifact/Intel.
5. Establish claim or leave.
6. World state updates.

Nomad Reach excels here.

---

# 87. CARD DISCOVERY THROUGH EXPLORATION

Rare cards should often have stories.

Example:

**Abandoned Weather Engine**

The player discovers a ruined Terravanta installation.

Quest:
restore it.

Reward:
Weather Engine Blueprint.

Now the card has provenance.

---

# 88. QUEST TYPES

- Citizen Story
- Faction
- Civilization
- Leader
- Exploration
- Research
- Economic
- Crisis
- Conquest
- Fusion
- Axiom Mystery

---

# 89. QUEST CONSEQUENCES

Quests should modify systems.

Completing a labor dispute may:
- raise Loyalty,
- unlock a Champion,
- change wages,
- alter faction support.

Not just grant XP.

---

# 90. CAMPAIGN CHAPTERS

Suggested campaign pacing:

## CHAPTER I — FOUNDATION
Build first city.
Meet neighboring powers.

## CHAPTER II — FIRST CONTACT
Trade, diplomacy, exploration.

## CHAPTER III — FIRST ASCENSION
Conquer or ally with first civilization.

## CHAPTER IV — THE REACTION
World responds to the Founder.

## CHAPTER V — COALITIONS
Mid-ring politics and multi-party war.

## CHAPTER VI — CRISIS AGE
Global crises escalate.

## CHAPTER VII — INNER RING
Advanced civilizations become central.

## CHAPTER VIII — CONVERGENCE WAR
Final rivals and Axiom interference.

## CHAPTER IX — THE AXIOM CROWN
Final campaign.

---

# 91. ASCENSION PACING

The game should not demand 20 identical conquests.

Expected path:

- 6–8 major full conquest arcs
- 4–6 diplomatic unions
- 2–4 economic absorptions
- 2–4 Influence transfers
- several hybrid outcomes

The player still obtains all 20 Relics, but the method varies.

---

# 92. RIVAL SCALING

Remaining rivals gain:

- experience,
- technology,
- alliances,
- upgraded cities,
- countermeasures.

They do not simply receive health bonuses.

---

# 93. DOMINION PRESSURE WORLD EFFECTS

At pressure thresholds:

## 15
Rivals prioritize fortification.

## 25
Defensive treaties increase.

## 40
Champion hunters appear.

## 50
Technology sharing begins.

## 65
Anti-Founder coalition possible.

## 80
Total-war economies.

## 95
Axiom agents appear openly.

## 100
Axiom Crown awakening accelerates.

---

# 94. PLAYER EMPIRE MANAGEMENT

As the player controls more civilizations, direct micromanagement must decrease.

The player appoints:

- Governors
- Regional Councils
- Automated Administration
- Local Leaders

This shifts gameplay from city manager toward empire architect.

---

# 95. GOVERNOR AUTONOMY

Governors can receive priorities:

- Growth
- Stability
- Research
- Defense
- Wealth
- Ecology
- Cultural Preservation

They execute within policy constraints.

---

# 96. GOVERNOR FAILURE

Governors can:

- mismanage,
- become corrupt,
- become popular,
- challenge the Founder,
- create new policies,
- form personal alliances.

Strong Leaders remain characters after conquest.

---

# 97. SUCCESSION

If a conquered Leader is removed, local political systems may generate new Leaders.

Named citizens can rise.

This prevents territories from becoming static.

---

# 98. WORLD SIMULATION BUDGET

For implementation efficiency:

## FULL SIMULATION
Current city and nearby active region.

## MEDIUM SIMULATION
Major rival cities.

## ABSTRACT SIMULATION
Distant districts and minor settlements.

When the Founder arrives, abstract results instantiate into physical world state.

---

# 99. SAVE STATE

Persistent campaign save must record:

## Player
- Founder progression
- Collection
- Blueprints
- Decks
- Relics
- Reputation

## City
- buildings
- upgrades
- damage
- citizens
- jobs
- factions
- resources
- policies

## World
- diplomacy
- wars
- trade
- events
- resource claims
- rival city states
- Leader relationships
- Dominion Pressure

---

# 100. CARD ACQUISITION VERTICAL SLICE

For the Synara / Forgeweave / Eden Circuit prototype:

The player can earn cards through:

### Starting Deck
60 Synara cards.

### Research
Unlock 6 Synara blueprint cards.

### Exploration
Discover 4 Universal cards.

### Eden Relationship
Earn 3 Eden Circuit cards.

### Forgeweave Conquest
Unlock 15 Forgeweave Core cards.

### Leader Quest
Earn one Champion.

### Ascension
Gain one Dominion card.

The prototype therefore demonstrates multiple acquisition channels without loot boxes.

---

# 101. FIRST CITIZEN STORY — NIA VALE

**Origin:** Synara  
**Occupation:** Junior Systems Technician  
**Starting Class:** Operator Initiate

Nia works in a Cognitive Operations Tower.

Initial traits:
- Curious
- Automation Skeptic
- High Aptitude

Story arc:

1. She discovers an optimization system quietly replacing human jobs.
2. The player may suppress, investigate, or publicize it.
3. Nia can become:
   - Orchestrator,
   - Agency Activist,
   - Intelligence Auditor,
   - or leave for Concord Vale.
4. If supported through the full arc, she can become a Champion.

Champion Card:

# NIA VALE — HUMAN OVERRIDE

Ability:
Temporarily restores manual control to automated systems and prevents runaway automation.

This is the ideal kind of citizen-to-card transformation.

---

# 102. FIRST FACTION SET — SYNARA

Synara begins with:

## ASCENDANTS
Pro-automation technologists.

Demand:
accelerate orchestration.

## HUMAN AGENCY LEAGUE
Protect human decision-making.

Demand:
limits on automation.

## SYNTHETIC RIGHTS FRONT
Supports personhood for advanced systems.

Demand:
legal recognition.

## PRACTICAL MODERATES
Care primarily about stability and prosperity.

Demand:
balance.

These factions directly embody Synara's systemic problem.

---

# 103. FIRST WORLD EVENT CHAIN
# THE FOUNDRY SHORTAGE

Trigger:
Forgeweave Resource Hunger > 70.

Stage I:
Machine component prices increase.

Stage II:
Forgeweave imports aggressively.

Stage III:
Eden Circuit protests destructive extraction.

Stage IV:
Regional shortage.

Possible player responses:

- Profit from exports
- Support Forgeweave
- Develop substitute materials
- Support Eden restrictions
- Broker three-way deal
- Sabotage competitor
- Prepare for Forgeweave collapse

Outcome changes:
- trade,
- diplomacy,
- Resource Hunger,
- Eden relationship,
- conquest options.

---

# 104. FIRST THREE-WAY DIPLOMATIC ARC

Participants:
- Synara
- Forgeweave
- Eden Circuit

Conflict:
industrial expansion vs ecological regeneration.

The player can become:

- industrial partner,
- ecological ally,
- neutral broker,
- economic opportunist,
- conqueror.

The region should feel politically alive before the first siege.

---

# 105. COLLECTION PROGRESSION

Long-term collection goals:

## FACTION COMPLETION
Collect all cards from a civilization.

## ARCHITECTURE COMPLETION
Unlock all visual variants.

## CLASS COMPLETION
Train all class lineages.

## CHAMPION COMPLETION
Discover all named Champion paths.

## FUSION COMPLETION
Unlock all cross-civilization synergies.

## AXIOM ARCHIVE
Recover ancient technologies.

---

# 106. NO PAY-TO-WIN RULE

If commercialized:

- paid content may include cosmetics,
- soundtrack,
- expansion campaigns,
- optional architecture skins.

Core competitive or campaign power should not require paid random acquisition.

If cards are sold in any form, gameplay acquisition must remain deterministic and complete.

---

# 107. CAMPAIGN VICTORY CONDITIONS

Primary victory:

Collect all 20 Relics and resolve the Axiom Crown.

Optional meta-victories:

- Maximum Federation
- Zero destructive conquests
- Total military domination
- Global economic hegemony
- Universal ecological recovery
- Synthetic liberation
- Knowledge ascension
- Perfect cultural hybridization

These become campaign achievements and ending modifiers.

---

# 108. THE PLAYER'S CIVILIZATION SHOULD LOOK LIKE HISTORY

By late game, the player's capital should visibly tell the campaign story.

A player might have:

- Synara white computational towers,
- Forgeweave industrial rail,
- Eden living rooftops,
- Auric markets,
- Aethernet data spires,
- Nocturne defenses.

The skyline itself becomes a save-file biography.

---

# 109. CAMPAIGN SUCCESS QUESTIONS

The world layer is ready to scale only if testers answer yes:

1. Do citizens feel like inhabitants rather than resource numbers?
2. Do jobs, housing, and transit create meaningful city decisions?
3. Can a normal citizen become memorable through simulation?
4. Do factions create pressure without becoming constant nuisance popups?
5. Does migration make rival cities feel connected?
6. Are cards acquired through accomplishments rather than arbitrary randomness?
7. Does every rare card feel like it has provenance?
8. Do rival cities visibly change while the player is away?
9. Does diplomacy remember what the player actually did?
10. Can a world event meaningfully change conquest strategy?
11. Does controlling more territory increase both capability and management complexity?
12. Does the late-game capital visibly reflect the player's conquest history?

---

# 110. IMPLEMENTATION ORDER

## MILESTONE A — POPULATION FOUNDATION

Implement:
- households,
- jobs,
- commute,
- Happiness,
- Loyalty.

Target:
200 simulated citizens through cohorts, 20 named citizens.

---

## MILESTONE B — CLASSES

Implement:
- six universal archetypes,
- Synara class lineages,
- experience,
- training.

---

## MILESTONE C — FACTIONS

Implement:
- four Synara factions,
- support,
- demands,
- one protest escalation.

---

## MILESTONE D — MIGRATION

Implement:
- city Attractiveness,
- inbound/outbound migration,
- one foreign Eden citizen cohort.

---

## MILESTONE E — CARD ACQUISITION

Implement:
- Blueprints,
- crafting,
- Discovery Cache,
- duplicate recycle,
- one Champion Ascension.

---

## MILESTONE F — WORLD MAP

Implement:
- Synara home,
- Forgeweave city,
- Eden Circuit city,
- three minor settlements,
- resource regions.

---

## MILESTONE G — RIVAL AI

Implement:
- Forgeweave development,
- trade,
- Resource Hunger response,
- diplomacy.

---

## MILESTONE H — WORLD EVENT

Implement:
**The Foundry Shortage**

It must affect:
- prices,
- relationships,
- citizen factions,
- conquest options.

---

## MILESTONE I — THREE-WAY CAMPAIGN

Combine:
- city simulation,
- citizen stories,
- card acquisition,
- diplomacy,
- world event,
- Forgeweave conquest,
- Ascension.

This becomes the first real proof of DOMINION // ASCENDANT as a living world rather than a set of disconnected systems.

---

# 111. CANONICAL PERSISTENT LOOP

> **Build homes → attract people → create jobs → develop classes → satisfy or provoke factions → discover cards → expand the city → trade with rivals → react to world events → build relationships or pressure → conquer or unite → absorb new people and systems → manage the consequences → evolve again.**

---

# 112. DESIGN MANTRA

# PEOPLE MAKE THE CITY.
# THE CITY MAKES THE DECK.
# THE DECK SHAPES THE WORLD.
# THE WORLD REMEMBERS WHAT YOU DID.
