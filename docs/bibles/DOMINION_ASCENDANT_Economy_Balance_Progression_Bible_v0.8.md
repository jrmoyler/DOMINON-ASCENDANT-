# DOMINION // ASCENDANT
## Economy, Balance & Progression Bible — v0.8

**Status:** Canonical numerical tuning pass following:

1. Game Bible v0.1
2. Playable Systems Bible v0.2
3. Combat, Siege & Conquest Bible v0.3
4. Citizen, Card Acquisition & World Campaign Bible v0.4
5. 3D Production & Technical Architecture Bible v0.5
6. Content Production Bible v0.6
7. UX, Onboarding & Player Experience Bible v0.7

> **The economy should make the player choose what to build next, not whether they are allowed to play.**

---

# 1. PURPOSE

This document establishes the first mathematically tunable balance model for **DOMINION // ASCENDANT**.

It specifies:

- Capital income
- Insight income
- Influence income
- starting resources
- card cost bands
- construction costs
- crafting costs
- maintenance
- utilities
- population growth
- workforce demand
- commercial demand
- industrial throughput
- research pacing
- Influence sinks
- district and sector expansion
- unit and vehicle upkeep
- operation costs
- conquest rewards
- card-acquisition pacing
- upgrade costs
- Wonder pacing
- Dominion Pressure
- anti-snowball systems
- difficulty modifiers
- progression targets
- telemetry targets

---

# 2. NUMERICAL AUTHORITY

The numerical values in this document **supersede provisional economic values in earlier bibles where they conflict**.

The following earlier structural rules remain:

- 1 Development Cycle = 30 seconds of active simulation.
- 1 World Tick = 5 Development Cycles.
- Capital, Insight, and Influence remain the only three wallet currencies.
- Power, Water, Data, Materials, Supply, and similar values are operational resources, not wallet currencies.
- Cards remain persistent owned assets.
- Buildings remain persistent World Assets.

---

# 3. BALANCE PHILOSOPHY

The economy should create:

## SCARCITY OF PRIORITY
The player can afford something useful, but not everything useful.

## ABUNDANCE OF OPTIONS
The player usually has multiple valid actions.

## COST OF SPECIALIZATION
Extreme builds become powerful but create vulnerabilities.

## RECOVERABLE FAILURE
Bad choices create problems, not automatic campaign death.

## DIMINISHING SNOWBALL
Conquest expands capabilities faster than raw resource output.

---

# 4. THE THREE-CURRENCY MODEL

## CAPITAL

Answers:

> **What can I physically finance?**

Used for:

- crafting
- construction
- repairs
- upkeep
- trade
- military logistics
- rebuilding
- infrastructure

---

## INSIGHT

Answers:

> **What can I understand and improve?**

Used for:

- research
- upgrades
- advanced crafting
- class progression
- Fusion technology
- analysis
- high-tier blueprints

---

## INFLUENCE

Answers:

> **What political scale can I legitimately control?**

Used for:

- expansion
- policies
- Champions
- treaties
- civic initiatives
- annexation
- Leader integration
- Wonders
- governance

---

# 5. STARTING ECONOMY

Standard Synara campaign:

| Resource | Start |
|---|---:|
| Capital | **40** |
| Insight | **12** |
| Influence | **8** |
| Population | **24** |
| Core Housing Capacity | **24** |
| Workforce | **16** |
| Claimed Municipal Sector | **1** |
| Founder Hall | Placed |

The Founder Hall contains temporary founding residences for the initial 24 citizens.

Those residences are deliberately inefficient and encourage early housing construction.

---

# 6. STARTING FOUNDER HALL OUTPUT

Per Development Cycle:

- **+1.0 Capital**
- **+0.15 Insight**
- **+0.10 Influence**

The UI displays normalized flow approximately as:

- **+2.0 Capital/min**
- **+0.3 Insight/min**
- **+0.2 Influence/min**

Internal fractional values are allowed.

The HUD does not need to round the simulation itself.

---

# 7. ECONOMY DISPLAY RULE

Production resolves every Development Cycle.

However, the standard UI displays:

# NET PER MINUTE

rather than constantly flashing tiny 30-second gains.

Example:

**Capital +8.4/min**

Detailed view can show:

- gross production
- taxes
- trade
- maintenance
- military
- administration

---

# 8. CAPITAL BUILD-COST BANDS

| Rarity | Typical Deployment Cost |
|---|---:|
| Common | **5–10** |
| Specialized | **9–18** |
| Elite | **20–42** |
| Legendary | **50–100** |
| Mythic | **110–220** |
| Wonder | **180–400** |

Individual cards can deviate where justified.

Rarity alone does not determine power; footprint and system role matter.

---

# 9. COST CONSTRUCTION FORMULA

Initial tuning formula:

`Deployment Capital = Tier Base × Footprint Modifier × Complexity Modifier`

### Tier Base

| Tier | Base |
|---|---:|
| Common | 6 |
| Specialized | 11 |
| Elite | 24 |
| Legendary | 55 |
| Mythic | 120 |
| Wonder | 200 |

### Footprint Modifier

| Footprint | Modifier |
|---|---:|
| 1×1 | 0.80 |
| 1×2 | 0.95 |
| 2×2 | 1.00 |
| 2×3 | 1.15 |
| 3×3 | 1.35 |
| 4×4 | 1.65 |
| 5×5 | 2.00 |
| District-scale | bespoke |

### Complexity Modifier

Typical:
**0.85–1.30**

Examples:
- simple residence: 0.9
- specialized lab: 1.1
- advanced autonomous factory: 1.25

Final values are rounded for player display.

---

# 10. CAPITAL AFFORDABILITY TARGET

A healthy city should take approximately:

| Stage | Time to Afford Typical Same-Tier Asset |
|---|---|
| Opening | 2–4 minutes |
| Early | 2–3 minutes |
| Midgame | 2–4 minutes |
| Late | 3–5 minutes |
| Wonder | 6–15 minutes plus prerequisites |

This is from **net free cash flow**, not gross production.

Players may save longer for strategic spikes.

---

# 11. CAPITAL FLOW TARGETS

Target **net** Capital after normal maintenance:

| Stage | Capital / Development Cycle | Display / Min |
|---|---:|---:|
| Founding | 1.0–2.5 | 2–5 |
| Early City | 3–6 | 6–12 |
| Developed City | 7–12 | 14–24 |
| Regional Capital | 12–20 | 24–40 |
| Mature Empire | 20–35 | 40–70 |

These are healthy ranges, not hard caps.

---

# 12. GROSS VS NET CAPITAL

Target mature-city expense structure:

- **55–70%** available after fixed upkeep
- **15–25%** building maintenance
- **5–15%** administration
- **5–20%** military during peace
- war can temporarily push expenses much higher

If normal maintenance consistently consumes more than 55% of gross revenue, the city should feel financially strained.

---

# 13. CAPITAL PRODUCTION SOURCES

Capital comes from:

- Retail
- Household taxes
- Offices
- Industry sales
- Trade
- Contracts
- Conquered-territory transfers
- World-event opportunities

No single source should dominate every civilization.

---

# 14. HOUSEHOLD TAX MODEL

Residential buildings do not directly print Capital.

Households contribute according to Wealth and policy.

Base household contribution per cycle:

| Wealth Tier | Base Taxable Activity |
|---|---:|
| Struggling | 0.01 |
| Working | 0.03 |
| Comfortable | 0.05 |
| Affluent | 0.08 |
| Elite | 0.12 |

Tax policy modifies the realized amount.

Heavy taxation can affect:

- Happiness
- Equality
- migration
- faction support

---

# 15. COMMERCIAL DEMAND

Retail cannot scale infinitely by spam.

Every district has:

# CONSUMER DEMAND

Simplified:

`Demand = Population × Wealth Demand Factor × Mobility Access`

Retail facilities share that demand.

If Retail Capacity exceeds local/regional Demand:

commercial output is reduced.

---

# 16. RETAIL SATURATION

Example:

District Demand:
**10 commerce units**

Retail Capacity:
**15**

Saturation:

`10 / 15 = 0.67`

Retail output operates at approximately 67% demand efficiency before other modifiers.

This makes population and transportation economically relevant.

---

# 17. INDUSTRIAL OUTPUT

Industry creates:

# PRODUCTION THROUGHPUT

Production Throughput is **not a wallet currency**.

It represents local manufacturing capacity.

It is consumed by:

- card crafting
- repairs
- unit production
- vehicle production
- export contracts
- major projects

Unused throughput may generate Capital through exports if market demand exists.

---

# 18. INDUSTRIAL OVERSUPPLY

Industrial spam can create:

- low export prices
- Resource Hunger
- pollution
- worker shortage
- supply-chain stress

Forgeweave can overcome some limits but suffers its unique systemic problem faster.

---

# 19. NET CAPITAL FORMULA

Conceptual:

`Net Capital = Commercial + Taxes + Trade + Contracts + Export Revenue - Building Maintenance - Administration - Military Upkeep - Debt/Obligations`

For each facility:

`Facility Output = Base Output × Staffing × Utility × Demand × Condition × Modifier Stack`

---

# 20. MODIFIER STACKING

To prevent runaway multiplicative bonuses:

Most standard bonuses are summed into a bounded modifier pool.

Example:

- Adjacency +20%
- District +10%
- Policy +8%
- Staff specialization +7%

Standard bonus:
**+45%**

Standard positive modifier cap:
**+60%**

Standard negative modifier floor:
**-60%**

Dominion and Fusion effects can exceed those boundaries where explicitly designed.

---

# 21. CONDITION MULTIPLIER

| Condition | Output |
|---|---:|
| Operational 76–100% integrity | 100% |
| Worn / minor damage | 90–99% |
| Damaged | 50–80% |
| Disabled | 0% |
| Ruined | 0% |

Specific modules can cause more targeted losses.

---

# 22. UTILITY MULTIPLIER

If required utilities are missing:

| Utility State | Output |
|---|---:|
| Fully supplied | 100% |
| Minor deficit | 85% |
| Significant deficit | 60% |
| Critical | 25% |
| Offline | 0% |

---

# 23. STAFFING MULTIPLIER

| Staffing | Output |
|---|---:|
| 100% | 100% |
| 75–99% | 90% |
| 50–74% | 65% |
| 25–49% | 40% |
| 1–24% | 15% |
| 0% | 0% unless automated |

Elite specialists can push selected facilities above 100%.

---

# 24. BUILDING MAINTENANCE

Base maintenance as percent of deployment cost per cycle:

| Type | Approx. Rate |
|---|---:|
| Residential | 0.5% |
| Retail | 0.7% |
| Office | 0.6% |
| Research | 0.8% |
| Industrial | 1.0% |
| Infrastructure | 0.5% |
| Defense | 0.8% |
| Wonder | bespoke 0.4–0.8% |

Example:

40-Capital Elite factory at 1%:
**0.4 Capital/cycle**

---

# 25. MAINTENANCE CONDITION PENALTY

Poorly maintained assets cost more.

| Condition | Maintenance |
|---|---:|
| Healthy | 100% |
| Worn | 115% |
| Damaged | 140% |
| Critically damaged | 180% |

Repairing infrastructure can be cheaper than tolerating permanent degradation.

---

# 26. REPAIR COST

Repair cost is proportional to missing Structural Integrity.

Base:

`Repair Capital = Deployment Cost × Missing Integrity % × 0.50`

Example:

40-Capital building at 50% integrity:

`40 × 0.5 × 0.5 = 10 Capital`

Full reconstruction from Ruined remains approximately **60% of deployment cost**, as established previously.

---

# 27. INSIGHT FLOW

Target net Insight:

| Stage | Insight / Cycle | Display / Min |
|---|---:|---:|
| Founding | 0.15–0.4 | 0.3–0.8 |
| Early | 0.5–1.2 | 1–2.4 |
| Developed | 1.5–3 | 3–6 |
| Regional | 3–5 | 6–10 |
| Late | 5–9 | 10–18 |

Insight should remain substantially scarcer than Capital.

---

# 28. RESEARCH PROJECT COSTS

| Research Tier | Insight | Time |
|---|---:|---:|
| Foundation | 4–8 | 3–6 cycles |
| Tier I | 8–14 | 5–10 cycles |
| Tier II | 16–28 | 10–18 cycles |
| Tier III | 32–55 | 18–30 cycles |
| Fusion | 50–90 | 25–45 cycles |
| Mythic | 100–180 | 40–80 cycles |

Research time and Insight are both requirements.

A rich player cannot instantly buy technological understanding with Capital.

---

# 29. RESEARCH SLOT MODEL

Base:
**1 active major research project**

Research facilities can provide:

- additional parallel project slot,
- faster project time,
- local specialization.

Maximum standard parallel major projects:
**3**

Certain civilizations may exceed this.

---

# 30. INSIGHT GENERATION FORMULA

`Research Output = Base Insight × Staffing × Utilities × Education Factor × Research Adjacency × Condition`

Education Factor:

| City Education | Factor |
|---|---:|
| 0–24 | 0.70 |
| 25–49 | 0.85 |
| 50–69 | 1.00 |
| 70–84 | 1.10 |
| 85–100 | 1.20 |

This makes citizens relevant to technology.

---

# 31. INFLUENCE FLOW

Target net Influence:

| Stage | Influence / Cycle | Display / Min |
|---|---:|---:|
| Founding | 0.10–0.25 | 0.2–0.5 |
| Early | 0.25–0.6 | 0.5–1.2 |
| Developed | 0.7–1.5 | 1.4–3 |
| Regional | 1.5–3 | 3–6 |
| Late | 3–6 | 6–12 |

Influence should usually feel like the slowest wallet currency.

---

# 32. POPULATION MANDATE

Happy, stable populations generate small passive Influence.

Conceptual:

`Population Mandate = (Population / 100) × HappinessFactor × StabilityFactor`

Where:

`HappinessFactor = max(0, (Happiness - 50) / 50)`

`StabilityFactor = Stability / 100`

Example:

Population 100  
Happiness 70  
Stability 75

`1 × 0.4 × 0.75 = 0.30 Influence/cycle`

This prevents passive Influence from becoming enormous.

---

# 33. INFLUENCE SINKS

| Action | Typical Influence Cost |
|---|---:|
| Minor civic initiative | 2–5 |
| District policy | 3–6 |
| Citywide policy | 6–12 |
| Formal treaty initiative | 2–8 |
| Champion recruitment | 8–18 |
| Leader integration | 15–30 |
| Municipal Sector claim | 5–20 |
| Annex defeated territory | 12–35 |
| Wonder authorization | 15–40 |
| High-level Federation action | 25–50 |

---

# 34. MUNICIPAL SECTOR MODEL

The 128×128 full city contains:

- 16×16 logical 8×8-cell districts
- grouped into **4×4 Municipal Sectors**
- each Sector contains 4×4 districts
- total **16 Municipal Sectors**

Expansion costs Influence by Sector rather than every tiny district.

Prototype 32×32 city is effectively one full Municipal Sector.

---

# 35. MUNICIPAL SECTOR CLAIM COST

Suggested:

| Owned Sector Number | Influence |
|---|---:|
| 1 | free founding sector |
| 2 | 5 |
| 3 | 6 |
| 4 | 8 |
| 5–6 | 10 |
| 7–8 | 12 |
| 9–12 | 15 |
| 13–16 | 20 |

Remote or difficult terrain:
**×1.25–1.75**

This keeps expansion meaningful without turning it into 256 tiny purchases.

---

# 36. POPULATION CAPACITY

Population cannot exceed functional housing capacity for long.

Short-term emergency occupancy can reach:

**115% of Housing Capacity**

Above 100%:

- Happiness declines
- Health declines
- housing faction pressure rises

Above 110%:
severe overcrowding effects begin.

---

# 37. WORKFORCE

Default workforce availability:

`Workforce = Population × 0.65`

This represents citizens currently available for standard labor.

Faction, age structure, automation, policy, and crisis can modify this.

Starting:
24 population → approximately **16 workers**

---

# 38. EMPLOYMENT TARGET

Healthy range:

**90–97% employment of available workforce**

Below 85%:
unemployment pressure rises.

Above 98%:
labor shortage pressure rises.

Both extremes create gameplay.

---

# 39. JOB VACANCY

If job openings exceed workforce by more than 10%:

- wages/Wealth pressure rises,
- production suffers,
- immigration attractiveness rises,
- automation becomes more attractive.

This creates the intended Synara temptation.

---

# 40. ATTRACTIVENESS SCORE

City Attractiveness 0–100 uses:

| Category | Weight |
|---|---:|
| Employment Opportunity | 20% |
| Housing Availability/Quality | 15% |
| Security | 15% |
| Health | 15% |
| Freedom/Agency | 10% |
| Education | 10% |
| Ecology | 5% |
| Mobility | 5% |
| Culture/Belonging | 5% |

Faction modifiers can alter priorities by citizen cohort.

---

# 41. MIGRATION FORMULA

Migration resolves each World Tick.

For positive migration:

`Incoming = Vacancy × 0.15 × max(0, (Attractiveness - 40) / 60)`

Fractional values accumulate.

Example:

Vacancy = 20  
Attractiveness = 70

`20 × 0.15 × 0.5 = 1.5 citizens/World Tick`

Approximately 1–2 new citizens every 2.5 active minutes.

---

# 42. EMIGRATION FORMULA

If Attractiveness falls below 40:

`Outgoing = Population × 0.02 × ((40 - Attractiveness) / 40)`

Additional grievance multipliers may increase this.

Attractiveness 20 and population 100:

`100 × 0.02 × 0.5 = 1 citizen/World Tick`

A bad city declines progressively rather than instantly emptying.

---

# 43. NATURAL POPULATION GROWTH

Natural growth is intentionally slower than migration.

Household formation is evaluated every **12 World Ticks**.

Base growth:
**0.25–0.75% population**

Requirements:

- available housing
- Health > 50
- Stability > 45

This is abstracted demographic growth rather than literal real-time childbirth.

---

# 44. HOUSING VACANCY TARGET

Healthy:
**5–15% vacancy**

Below 3%:
housing prices and overcrowding pressure rise.

Above 25%:
district prosperity and maintenance efficiency may decline.

Hearthspire can operate efficiently at lower vacancy.

---

# 45. WEALTH AND INEQUALITY

Wealth is primarily a household state.

City Wealth:
average household prosperity.

Equality:
distribution balance.

Auric Ledger can produce very high Wealth while losing Equality.

The player should not be able to maximize both automatically.

---

# 46. SYSTEMIC PRESSURE GROWTH

Faction-specific pressure meters generally use:

- 0–24 Healthy
- 25–49 Emerging
- 50–69 Significant
- 70–84 Crisis Risk
- 85–99 Critical
- 100 Crisis Event

Typical healthy play should see its faction pressure fluctuate in:
**20–55**

A player pushing the faction's strength aggressively should often enter:
**50–80**

100 should require sustained neglect or intentional extremism.

---

# 47. SYNARA DEPENDENCY FORMULA

Baseline pressure sources:

| Source | Dependency |
|---|---:|
| AI Worker assigned | +0.15/cycle |
| Autonomous Exchange | +0.10/cycle |
| Fully automated Office | +0.20/cycle |
| Fully automated Industrial | +0.25/cycle |
| Thinking Spire district automation | +0.35/cycle |

Mitigation:

| Source | Reduction |
|---|---:|
| Agency Forum | -0.20/cycle district |
| Education >70 | -10% growth |
| Human staff ratio >60% | -0.10/cycle district |
| Human Override policy | -20% growth |

Pressure is recalculated, not simply accumulated forever.

---

# 48. FORGEWEAVE RESOURCE HUNGER

Growth drivers:

- active industrial throughput
- scarce material input
- Overdrive
- replication
- imports under shortage

Mitigation:

- efficient logistics
- recycling
- Eden regenerative materials
- reduced production
- alternative research

Resource Hunger should make high output powerful but costly.

---

# 49. EDEN ECOLOGICAL BALANCE

Pressure increases when:

- natural tiles removed
- pollution rises
- water is overused
- industrial adjacency grows

Pressure decreases through:

- restored habitat
- wetlands
- Pollinator Corridors
- regenerative industry.

---

# 50. CARD CRAFTING ECONOMY

Blueprint ownership allows crafting of Card Instances.

A card's full lifecycle cost is divided:

## CRAFT COST
approximately **35%** of base economic cost.

## DEPLOYMENT COST
approximately **65%**

Starter-deck cards have already paid Craft Cost.

This prevents players from paying twice for starting cards.

---

# 51. CRAFT COST EXAMPLE

Adaptive Habitat target lifecycle Capital:
**12**

Craft:
**4 Capital**

Deploy:
**8 Capital**

If the card is later recovered and redeployed:
standard redeployment uses the normal deployment rules unless a scenario specifically waives relocation cost.

---

# 52. ADVANCED CRAFTING

Elite+ cards may require:

- Insight
- Production Throughput
- specialized facility
- project time

Example:

Guardian Drone Cohort:

- 8 Capital Craft
- 2 Insight
- 5 Production Throughput
- Synthetic Fabrication Node
- 2 cycles

---

# 53. DUPLICATE FUSION

Combining duplicate Card Instances does **not** create an infinitely stronger card.

Instead, a duplicate may reduce one upgrade cost.

Suggested:

- Common duplicate: -20% Capital on next upgrade
- Specialized: -20% Capital
- Elite: -15% Capital and -1 Insight
- Legendary duplicates generally do not randomly occur

Cap duplicate discount:
**40%**

---

# 54. RECYCLING

Recycle returns approximately:

- Common: 40% Craft Cost
- Specialized: 45%
- Elite: 50%
- Legendary: bespoke/manual confirmation

Recycling should not be a profitable crafting loop.

---

# 55. CARD ACQUISITION PACE

Target unique new cards/blueprints:

| Campaign Stage | Unique Acquisition Rate |
|---|---|
| First 2 hours | 5–8/hour |
| Hours 2–10 | 3–6/hour |
| Hours 10–30 | 2–4/hour |
| Hours 30+ | 1–3/hour plus conquest unlocks |

Conquest causes deliberate acquisition spikes.

---

# 56. DISCOVERY CACHE PACE

Typical:

- **1 meaningful Cache every 45–90 minutes**
- additional Caches from major quests/challenges

Caches are supplementary.

They are not the primary progression engine.

---

# 57. CACHE CONTENT VALUE

Typical Cache contains:

- 1 guaranteed card or blueprint
- 1–2 supporting resources
- chance of cosmetic/mastery item
- milestone rarity protection

Dominion cards never appear.

---

# 58. RESEARCH BLUEPRINT PACE

Faction research trees should yield:

- Foundation blueprint every 15–30 minutes early
- higher-tier blueprint every 45–120 minutes
- major technologies as quest/era milestones

This avoids unlocking the entire faction collection through passive research.

---

# 59. UPGRADE COSTS

Level II:

`Capital = 55% of Deployment Cost`  
`Insight = 2–5`

Level III:

`Capital = 90% of Deployment Cost`  
`Insight = 5–10`

Fusion:

`Capital = 50–100% of Deployment Cost`  
`Insight = 15–40`
plus specific civilization requirements.

---

# 60. UPGRADE TIME

| Upgrade | Time |
|---|---:|
| Level II | 2–4 cycles |
| Level III | 4–8 cycles |
| Fusion | 8–16 cycles |
| Wonder enhancement | bespoke |

Buildings operate at reduced efficiency during major upgrades.

---

# 61. WONDER ECONOMY

A Wonder should feel like a project.

Typical requirement:

- 180–400 Capital
- 15–40 Influence
- 20–60 Insight
- high Production Throughput
- 8–20 construction cycles
- faction condition

Wonder construction should materially affect the city's economy while underway.

---

# 62. WONDER LIMIT

Base:

**1 active faction Wonder per city**

Conquests can allow imported/hybrid Wonders later through special planning.

Do not allow every Wonder to be spammed into one city.

---

# 63. POLICY UPKEEP

Powerful policies may require recurring Influence maintenance.

Typical:

- minor: none
- moderate: 0.05–0.15 Influence/cycle
- major: 0.20–0.40 Influence/cycle

If Influence reserve reaches zero, optional policies suspend rather than creating negative currency.

---

# 64. MILITARY UNIT UPKEEP

Garrisoned:

| Unit | Capital/Cycle |
|---|---:|
| Light Squad | 0.15 |
| Standard Squad | 0.25 |
| Specialist | 0.30 |
| Heavy Squad | 0.45 |
| Elite Squad | 0.60 |

Deployed operation:
approximately **×1.75**

This represents transport, supply, repair, and operational tempo.

---

# 65. VEHICLE UPKEEP

| Vehicle | Garrison | Deployed |
|---|---:|---:|
| Light Recon | 0.30 | 0.55 |
| Transport | 0.40 | 0.70 |
| Assault | 0.65 | 1.15 |
| Heavy/Siege | 1.00 | 1.75 |

Autonomous civilizations may reduce crew requirements but not eliminate material upkeep.

---

# 66. MILITARY AFFORDABILITY TARGET

Peace-time military upkeep:

**8–18% of gross Capital**

War-time:

**20–40%**

Sustained total war above 40% should create economic strain.

---

# 67. REINFORCEMENT COST

Deploying reserves into an active operation costs:

- normal upkeep
- one-time logistics Capital

Typical:

| Reinforcement | Capital |
|---|---:|
| Light squad | 2 |
| Standard squad | 3 |
| Heavy squad | 5 |
| Vehicle | 4–8 |

Veyra and Vector Forge can reduce this.

---

# 68. WAR DAMAGE ECONOMY

A major military victory can still be economically disastrous.

Post-war expenses include:

- repairs
- reconstruction
- garrison
- emergency services
- refugee support
- integration

This is a major anti-snowball mechanism.

---

# 69. OPERATIONAL SUPPLY

Supply is not purchased as a fourth wallet currency.

Instead:

Supply Capacity is generated by infrastructure.

Operations consume capacity based on:

- squads
- vehicles
- distance
- terrain
- battle duration

Insufficient capacity increases Capital cost and combat penalties.

---

# 70. CONQUEST REWARD PHILOSOPHY

Conquest should primarily reward:

- capability
- card access
- classes
- architecture
- strategic options

not huge raw resource dumps.

The strongest reward is:

> **What you can now do.**

---

# 71. ASCENSION BASE REWARD

Every full Ascension grants:

- Civilization Relic
- Dominion Doctrine
- Core card pool unlock
- class lineage unlock
- Leader resolution
- Fusion eligibility
- architecture kit
- **+10 Influence**
- **+10 Insight**

Raw resource grant is intentionally modest.

---

# 72. CONQUEST METHOD BONUS

## FORCE

- salvage opportunities: +10–30 Capital depending preserved assets
- lower starting Loyalty

## ECONOMIC

- +5–15 Capital reserve
- favorable initial trade integration
- moderate Loyalty

## INFLUENCE

- +8–15 Influence
- high civic transition support

## ALLIANCE

- no large raw-resource payout
- highest Loyalty
- strongest Leader relationship
- lowest integration overhead

---

# 73. POST-CONQUEST INTEGRATION COST

A new civilization creates temporary:

# INTEGRATION OVERHEAD

For approximately **10 World Ticks**.

Effects can include:

- +5–15% administration cost
- reduced foreign building output
- increased faction activity

High Loyalty shortens this.

Direct Rule can increase resource extraction but extends integration.

---

# 74. EMPIRE ADMINISTRATION

Each controlled civilization beyond the first adds baseline administrative complexity.

Conceptual multiplier:

`Empire Overhead = 1 + 0.025 × (Integrated Civilizations - 1)^1.15`

Applied primarily to administration costs, not all production.

Example approximate overhead:

| Integrated Civilizations | Added Administration Burden |
|---|---:|
| 1 | 0% |
| 5 | ~12% |
| 10 | ~34% |
| 15 | ~58% |
| 20 | ~83% |

Governors, Civic Meridian, automation, federation, and policies mitigate this.

This makes empire management matter without deleting conquest rewards.

---

# 75. ANTI-SNOWBALL SYSTEMS

The game uses multiple soft brakes rather than one blunt scaling penalty.

1. Dominion Doctrine slot limit
2. foreign-card deck limits
3. Integration Overhead
4. Loyalty
5. rebellion
6. administrative burden
7. military upkeep
8. rival adaptation
9. Dominion Pressure
10. infrastructure reconstruction
11. faction-system conflicts
12. systemic weakness inheritance through Fusion

---

# 76. DOMINION LOADOUT CAP

Existing rule remains:

- 1 Founding Doctrine
- 3 Dominion Doctrines
- 2 Fusion Doctrines

Conquering more civilizations increases **choice**, not unlimited simultaneous power.

---

# 77. FOREIGN CARD ACCESS

Initial post-conquest deck restriction:

- minimum 30 founding-civilization cards
- maximum 12 from one conquered civilization
- maximum 18 total foreign cards

Late research can relax these gradually.

Maximum endgame foreign allowance should still preserve meaningful deck identity.

---

# 78. DOMINION PRESSURE FORMULA

Base full Ascension:

**+5 Pressure**

Modifiers:

| Action | Pressure |
|---|---:|
| Destructive Force conquest | +2 |
| Major civilian catastrophe | +3 |
| Leader execution | +2 |
| Broken major treaty | +4 |
| Peaceful Alliance | -1 |
| Federated autonomy | -1 |
| World-saving crisis intervention | -1 situational |

Pressure cannot go below 0.

---

# 79. DOMINION PRESSURE THRESHOLDS

| Pressure | World Response |
|---|---|
| 0–14 | localized concern |
| 15 | fortification |
| 25 | defensive treaties |
| 40 | Champion hunters |
| 50 | technology sharing |
| 65 | anti-Founder coalition |
| 80 | total-war economies |
| 95 | open Axiom interference |
| 100 | accelerated Crown awakening |

---

# 80. RIVAL ADAPTATION BUDGET

Each major conquest grants surviving rivals:

# ADAPTATION POINTS

These are **AI-only planning weights**, not a player currency.

They invest into counters based on player behavior.

Example adaptation categories:

- anti-drone
- cyber defense
- economic diversification
- propaganda resilience
- fortification
- ecological independence

This improves AI response without simply raising health.

---

# 81. ECONOMIC CATCH-UP

The game avoids obvious rubber-banding.

A struggling player may receive opportunities, not invisible bonuses:

- emergency contracts
- ally assistance
- low-interest credit
- crisis quests
- salvage opportunities
- migration incentives

The player can understand why help exists.

---

# 82. DEBT

Optional economic mechanic:

Auric Ledger and certain treaties can provide loans.

Debt records:

- Principal
- Interest
- Duration
- Creditor

Debt is denominated in Capital, not a new currency.

Default can cause:

- Dependence
- diplomatic leverage
- market penalties

---

# 83. TRADE PRICE MODEL

Each strategic good has:

- Base Price
- Regional Supply
- Regional Demand
- Crisis Modifier

Conceptual:

`Price = Base × Demand/Supply`

Clamp normal market movement:
**0.5×–2.0×**

Crisis events can temporarily exceed that.

---

# 84. MARKET VOLATILITY

Normal World Tick price movement target:

**<5%**

Major event:
**10–30%**

Global crisis:
**30–100%+**

The market should feel responsive, not randomly chaotic.

---

# 85. TRADE ROUTE CAPACITY

Trade is limited by:

- infrastructure
- distance
- treaties
- logistics
- security

Signing ten contracts without freight capacity should not magically move infinite goods.

---

# 86. COMMERCIAL PROFITABILITY

Retail target payback period:

**6–12 Development Cycles**

Industrial:
**10–20 Cycles**

Research facilities should generally not directly pay themselves back in Capital.

Civic buildings may never directly pay back Capital.

Their value is systemic.

---

# 87. BUILDING PAYBACK IS NOT EVERYTHING

Buildings can provide:

- Stability
- Loyalty
- Insight
- Influence
- Housing
- defense
- class training
- systemic-pressure mitigation

Economic ROI is only one dimension.

---

# 88. FIRST-HOUR BALANCE TARGET

By minute ~60 standard play:

Expected:

- Population: **30–40**
- Capital reserve: **15–40**
- Insight reserve: **4–12**
- Influence reserve: **4–10**
- 8–14 player-placed assets
- 1 named citizen relationship
- Dependency: **10–35**
- 1–2 researched Blueprints
- 1 field Unit
- 0 Ascensions

Player should not feel either bankrupt or fully solved.

---

# 89. FIRST-3-HOUR TARGET

Expected:

- Population: **45–75**
- 14–25 placed assets
- 2–4 district specializations emerging
- 6–12 acquired Blueprints/cards beyond starter
- first serious faction tension
- first meaningful Forgeweave/Eden treaty
- first combat squad progression
- Resource Hunger crisis developing regionally

---

# 90. 10-HOUR PROGRESSION TARGET

Typical mainline player:

- **1 Ascension**
- 70–110 Population in capital
- 30–50 placed assets
- 70–100 unique owned cards/blueprints
- Level II common across core assets
- first Level III assets
- 1 Fusion technology
- 1–3 Champions
- 2–4 active treaties
- 2–3 explored minor settlements
- Dominion Pressure roughly **4–12**
- first conquered/hybrid district

The player should feel transformed but nowhere near finished.

---

# 91. 25-HOUR PROGRESSION TARGET

Typical:

- **3–5 Relics**
- 160–300 capital Population
- 70–120 placed assets across holdings
- 130–190 unique cards/blueprints
- 3–6 Champions
- 2–4 Fusion technologies
- 1–2 Governors
- multiple treaties/alliances
- first coalition politics
- outer-ring conflict largely resolved
- Dominion Pressure **15–35**

---

# 92. 50-HOUR PROGRESSION TARGET

Typical:

- **8–12 Relics**
- 350–700 population across directly simulated major holdings
- substantial additional abstract population
- 200–280 unique cards/blueprints
- multiple hybrid districts
- 6–10 Champions
- 4–8 Leaders integrated/resolved
- Mid Ring heavily active
- 5–8 Fusion technologies
- multi-front politics
- Dominion Pressure **35–65**

---

# 93. 100-HOUR PROGRESSION TARGET

For a deep campaign player:

- **18–20 Relics**
- mature multi-region empire/federation
- 300+ unique blueprints/cards
- high Mastery on favorite assets
- most major class lineages encountered
- 10+ meaningful Fusion technologies
- multiple fully hybrid districts
- late-game Governor network
- Axiom Crown campaign unlocked or underway
- Dominion Pressure usually **70–100**
- world map visibly transformed by campaign history

100 hours is a deep-content target, not a mandatory minimum campaign duration.

---

# 94. MAINLINE CAMPAIGN LENGTH TARGET

Initial design target:

**45–70 hours** for a focused first completion.

Deep side content:

**90–140+ hours**

Replayability comes from:

- starting civilization
- conquest order
- world seed
- Leader relationships
- campaign crises
- deck choices
- endings

not mandatory grind.

---

# 95. ASCENSION PACING

Target:

### First Ascension
6–10 hours.

### Second
3–6 additional hours.

### Later Ascensions
2–5 hours depending method and world state.

Some Alliance or Economic integrations may span long background arcs while the player pursues another civilization.

---

# 96. WHY ASCENSIONS ACCELERATE

The first Ascension teaches the entire loop.

Later:

- more transportation
- more economy
- more intelligence
- more Leaders
- more cards

The player becomes operationally capable.

However, Dominion Pressure and coalition behavior increase strategic difficulty.

---

# 97. CARD COLLECTION COMPLETION

Do not expect one campaign to naturally obtain every cosmetic or every card variant.

Primary gameplay collection should approach:

**70–85% of core gameplay blueprints** in a thorough campaign.

Remaining items reward:

- alternate routes
- replay
- secret quests
- rare Leader outcomes

---

# 98. CHAMPION PACE

Target:

- first Champion: 2–5 hours
- 2–4 Champions by 10h
- 5–8 by 25h
- 8–15 by 50h

Not every high-level citizen becomes a Champion.

---

# 99. CLASS PROGRESSION PACE

Approximate active-service time:

| Rank | Typical Time |
|---|---|
| Initiate → Practitioner | 30–60 min |
| Practitioner → Specialist | 1–3 h |
| Specialist → Master | 3–8 h |
| Master → Ascendant | major story/achievement |

Training facilities and Mnemosyne can accelerate this.

---

# 100. FOUNDER ATTRIBUTE PROGRESSION

Founder receives meaningful progression from:

- major quests
- exploration milestones
- Leader encounters
- Ascensions

Avoid XP farming from trivial enemies.

Suggested campaign progression:

- 1 meaningful Founder point every 45–90 minutes early
- slower later
- major Ascension grants bonus progression opportunity

---

# 101. FOUNDER POWER CAP

Founder progression should improve:

- options
- efficiency
- synergy

more than raw damage.

A late Founder should not personally invalidate armies.

---

# 102. ENEMY SCALING

Enemies scale through:

- improved card pools
- upgraded cities
- veteran units
- better AI
- better logistics
- counter-technologies
- alliances

Not through universal level scaling.

An early-region basic unit should remain physically comprehensible late game.

---

# 103. DIFFICULTY — STORY FOCUS

Suggested modifiers:

| System | Modifier |
|---|---:|
| Player Capital output | 1.15× |
| Player Insight output | 1.10× |
| Maintenance | 0.85× |
| Repair cost | 0.80× |
| Rival economic efficiency | 0.90× |
| Dominion Pressure gain | 0.80× |
| Faction escalation | 0.80× |
| Command Mode | full pause available |

---

# 104. DIFFICULTY — STANDARD

All baseline values:

**1.00×**

This is the canonical tuning target.

---

# 105. DIFFICULTY — STRATEGIST

| System | Modifier |
|---|---:|
| Player output | 1.00× |
| Maintenance | 1.05× |
| Rival efficiency | 1.10× |
| Rival adaptation | 1.20× |
| Dominion Pressure | 1.10× |
| Faction escalation | 1.10× |
| Command slowdown | 20–50% configurable |

Difficulty should come primarily from smarter opposition.

---

# 106. DIFFICULTY — IRON DOMINION

| System | Modifier |
|---|---:|
| Player Capital output | 0.95× |
| Maintenance | 1.15× |
| Repair cost | 1.20× |
| Rival efficiency | 1.20× |
| Rival adaptation | 1.35× |
| Dominion Pressure | 1.25× |
| Faction escalation | 1.20× |
| Command Mode | real time |
| Strategic losses | harsher persistence |

Avoid extreme enemy-health multipliers.

---

# 107. DYNAMIC DIFFICULTY POLICY

Do **not** secretly modify enemy stats because the player is winning.

The world may respond through explicit systems:

- coalitions
- counter-tech
- diplomacy
- market behavior
- fortification

The player should see why difficulty increased.

---

# 108. ECONOMIC FAILURE STATES

## LIQUIDITY STRAIN

Capital < 10 and net flow negative.

Responses:
- warning
- halt optional construction
- contracts suggested

## DEFICIT

Capital reaches 0.

Optional policies suspend.
New discretionary construction blocks.

Essential city systems continue briefly.

## FISCAL CRISIS

Sustained deficit for 5+ cycles.

Effects:
- maintenance delays
- Stability loss
- credit/emergency options
- political consequences

No instant bankruptcy screen.

---

# 109. INSIGHT FAILURE

Insight reaching zero is not a crisis.

It means:

- no advanced research spending
- some upgrades delayed

Existing research facilities continue generating Insight.

---

# 110. INFLUENCE FAILURE

Influence reaching zero means:

- no new political commitments
- optional Influence-upkeep policies suspend
- annexation/treaty initiatives delay

The government does not collapse merely because the wallet reaches zero.

---

# 111. ECONOMY RECOVERY TOOLS

Every civilization should have access to some recovery routes:

- sell unused card
- recycle duplicate
- export throughput
- accept contract
- reduce military deployment
- suspend policy
- defer upgrade
- trade
- request ally aid
- take loan if available
- salvage ruin

No single bad purchase should permanently ruin a standard campaign.

---

# 112. DECOMMISSIONING

A building can be decommissioned.

Return:

- approximately 35% of Deployment Capital if healthy
- lower if damaged

Card Instance returns to collection after process completes.

This lets players redesign cities.

---

# 113. RELOCATION

Certain small structures can relocate.

Standard building relocation:

- decommission
- card returns
- redeploy

Nomad Reach has stronger direct relocation abilities.

---

# 114. CITY ADMINISTRATION CAP

Each Municipal Sector has baseline operational capacity.

If excessive specialized facilities are packed into one Sector:

# ADMINISTRATION LOAD

rises.

Above capacity:

- maintenance increases
- response times slow
- Stability suffers

Civic/administrative buildings expand capacity.

---

# 115. ADMINISTRATION COST

Target:

- healthy city: 5–10% gross Capital
- sprawling unmanaged city: 15–30%

This makes compact planning viable.

---

# 116. POWER ECONOMY

Power is capacity, not currency.

Each source provides:

**Power Capacity**

Each facility consumes:

**Power Demand**

Target reserve:
**10–20%**

Below 5%:
grid strain.

Negative:
brownouts/outages.

---

# 117. WATER ECONOMY

Same capacity model.

Water scarcity affects:

- population
- Health
- industry
- Ecology

Terravanta and Eden can solve water differently.

---

# 118. DATA CAPACITY

Data is required by:

- Synara
- Aethernet
- advanced offices
- defense
- automated systems

Data shortage reduces advanced function rather than turning off basic housing.

---

# 119. MATERIAL THROUGHPUT

Industrial facilities produce/consume local material categories.

To avoid a fourth wallet:

materials remain:

- facility inventories
- logistics flows
- project requirements

The player does not see “12,443 Steel Coins” in a global wallet.

---

# 120. LOGISTICS COST

Longer supply chains increase:

- delivery time
- disruption risk

not arbitrary Capital tax alone.

Vector Forge is powerful because it reduces this friction.

---

# 121. TRADE CONTRACT BALANCE

Contracts should last:

**5–20 World Ticks**

Long contracts offer better stability.

Short contracts offer flexibility.

Breaking contracts creates Grievance.

---

# 122. ECONOMIC CONQUEST THRESHOLDS

Existing Autonomy meter remains 0–100.

To prevent cheese:

No single action may normally remove more than:

**15 Economic Autonomy**

Repeated identical leverage suffers diminishing returns.

The rival must become genuinely dependent across multiple systems.

---

# 123. INFLUENCE CONQUEST THRESHOLDS

Civic Legitimacy pressure cannot be generated purely by spending stored Influence.

At least **60% of total legitimacy pressure** must originate from:

- quests
- faction endorsement
- city performance comparison
- exposed failures
- real services
- persistent cultural presence

This prevents “buying” ideological conquest.

---

# 124. ALLIANCE BALANCE

Voluntary union requirements remain:

- Alliance Readiness 80+
- all components 65+
- no Major Grievance
- Leader Quest complete

Alliance should usually take longer than a reckless military conquest, but produce far lower integration costs.

---

# 125. FORCE BALANCE

Military conquest is fastest when:

- player has strong army
- target is isolated

But direct Force produces:

- repairs
- occupation
- lower Loyalty
- higher Pressure

The speed advantage is paid later.

---

# 126. REBELLION ECONOMICS

Rebellion causes:

- tax loss
- trade disruption
- security cost
- possible asset damage

Resolving the political cause is usually cheaper long-term than permanent garrison.

---

# 127. GOVERNOR ECONOMICS

Governor quality affects:

- Administration
- Stability
- corruption risk
- policy efficiency

A strong local Leader can save more resources than direct-rule extraction generates.

---

# 128. FEDERATION ECONOMICS

Federated territory:

- sends lower direct Capital
- costs much less Administration
- has higher Loyalty
- contributes military/diplomatic support

This should be a genuinely competitive empire strategy.

---

# 129. DIRECT RULE ECONOMICS

Direct Rule:

- +20–30% extractable local revenue
- higher Administration
- lower Autonomy
- higher rebellion risk

Not an automatic best option.

---

# 130. PROTECTORATE ECONOMICS

Protectorate:

- low direct income
- low administrative cost
- strategic access
- diplomatic value

Useful for distant territories.

---

# 131. CITY SPECIALIZATION BONUS

A specialized district gains approximately:

**+10% primary output**

but pays opportunity cost in mixed-use Stability/flexibility.

Further specialization should require buildings/policies rather than stacking automatic bonuses.

---

# 132. ADJACENCY BALANCE

Standard adjacency:

- small: +5%
- medium: +10%
- major: +15%

Normal asset should rarely create more than +15% to one neighbor.

Total standard positive adjacency cap:
**+40%**

as established.

---

# 133. NEGATIVE ADJACENCY

Typical:

- minor nuisance: -5%
- major conflict: -10%
- severe hazard: -15–25%

Negative effects should often target city metrics rather than only production.

---

# 134. FUSION BALANCE

Fusion technology should be roughly:

**20–40% stronger** than a comparable standard upgrade

but introduces:

- dual systemic pressure
- higher cost
- stricter infrastructure
- doctrine slot requirements

Fusion is power with complexity.

---

# 135. AUTONOMOUS FACTORY FINAL INITIAL TUNING

Synara + Forgeweave.

Benefits:

- **-80% workforce requirement**
- **+25% Industrial Throughput**
- **+15% adjacent Industrial construction speed**

Costs:

- deployment equivalent: Elite/Fusion tier
- **24 Insight**
- high Data and Power demand
- **Dependency +0.20/cycle**
- **Resource Hunger +0.15/cycle**

If both faction pressures exceed 80:

**Runaway Production Crisis** can trigger.

---

# 136. THINKING SPIRE INITIAL TUNING

Wonder.

Suggested:

- 220 Capital
- 30 Insight
- 25 Influence
- 12 cycles

Benefit:

One district may run AI administration with sharply reduced worker requirements.

Cost:

- significant Data demand
- persistent Dependency pressure
- high maintenance

---

# 137. GRAND FORGE INITIAL TUNING

Suggested:

- 240 Capital
- 20 Insight
- 20 Influence
- 14 cycles

Benefit:

- large Production Throughput
- Replication acceleration

Cost:

- major Power
- major material input
- Resource Hunger pressure

---

# 138. WORLDGARDEN INITIAL TUNING

Suggested:

- 200 Capital
- 28 Insight
- 30 Influence
- 14 cycles

Benefit:

- strong Ecology
- regenerative resources
- city Health/Happiness support

Cost:

- large land footprint
- water
- ecological maintenance conditions

---

# 139. VERTICAL-SLICE ECONOMY BENCHMARK

Operation Iron Veil should generally become feasible when the Synara player has:

- Population **65+**
- Net Capital **6+/cycle**
- Insight **20+ reserve or strong research**
- Influence **12+**
- 2–4 combat-capable Unit cards
- stable utilities
- Dependency below catastrophic level
- at least one functioning diplomatic/trade relationship

A skilled player may attempt earlier.

---

# 140. FOUNDRY SHORTAGE ECONOMIC VALUES

When event begins:

Machine-component price:
**+20%**

Escalation:
**+35%**

Regional crisis:
**+60%**

If resolved:
price trends toward baseline over 4–8 World Ticks.

Players can profit, but hoarding may create diplomatic consequences.

---

# 141. ECONOMY TELEMETRY TARGETS

Track:

- Capital earned/spent per hour
- Net Capital flow
- idle resource reserve
- building payback time
- maintenance share
- military share
- Insight earned/spent
- Influence earned/spent
- time resource capped
- failed placement due to cost
- decommission rate
- repair burden
- bankruptcy events

---

# 142. HEALTHY CAPITAL RESERVE

Target normal reserve behavior:

Early:
**10–40**

Mid:
**30–120**

Late:
**80–300**

If players constantly sit at zero:
economy is too punishing.

If most players sit on thousands:
sinks are insufficient.

---

# 143. HEALTHY INSIGHT RESERVE

Players should frequently choose between:

- saving for big research
- taking smaller upgrades

Target reserve:

Early:
**4–20**

Mid:
**15–60**

Late:
**40–150**

---

# 144. HEALTHY INFLUENCE RESERVE

Target:

Early:
**3–15**

Mid:
**10–40**

Late:
**25–100**

Influence spikes should often be spent around expansion or conquest.

---

# 145. BUILD-PACE TELEMETRY

Target player-created assets:

First hour:
**8–14**

First 3 hours:
**14–25 total**

10h:
**30–50 capital assets**

If substantially higher:
city growth may be visually and strategically too fast.

---

# 146. DECISION DENSITY

Target meaningful strategic decision:

approximately every **1–3 minutes** during active city play.

Not every decision needs a new building.

Decisions include:

- placement
- upgrade
- research
- policy
- citizen/faction response
- trade
- expedition
- combat

---

# 147. ECONOMIC DEAD-TIME RULE

A player should rarely need to wait with nothing productive to do solely because resources are accumulating.

While saving for a major project, they can:

- explore
- negotiate
- inspect citizens
- redesign
- fight
- research
- manage trade

The hybrid game structure is part of economic pacing.

---

# 148. PLAYER CHOICE STANDARD

A healthy resource situation should regularly create choices such as:

> I can afford the better factory, the new housing block, or the diplomatic initiative—but not all three right now.

That is desirable tension.

---

# 149. BAD SCARCITY STANDARD

Avoid:

> I cannot perform any meaningful action for five minutes because every system requires the same resource I do not have.

When this occurs, costs or recovery routes need tuning.

---

# 150. FIRST ECONOMY BALANCE TEST

Build a headless Synara simulation with:

- Founder Hall
- Adaptive Habitat
- Microgrid
- Water Reclaimer
- Corner Exchange
- Cognitive Tower
- Autonomous Exchange
- Neural Relay
- Agency Forum

Run:
**100 Development Cycles**

Test three strategies:

### Balanced
Moderate automation.

### Maximum Automation
Highest short-term output.

### Human-First
Low automation.

Expected:

- Automation has highest short-term efficiency.
- Balanced has strongest long-term combined economy/stability.
- Human-First remains viable but slower.
- None should collapse automatically.

---

# 151. SECOND BALANCE TEST

Forgeweave regional economy.

Run:
**500 Development Cycles**

Scenarios:

- unrestricted industrial growth
- sustainable growth
- Eden trade support
- logistics disruption

Expected:

Unrestricted growth should eventually create serious Resource Hunger.

Sustainable play should remain competitive.

---

# 152. THIRD BALANCE TEST

Conquest snowball.

Simulate:

- 1
- 5
- 10
- 15
- 20

integrated civilizations.

Measure:

- gross income
- administrative burden
- military cost
- Loyalty crises
- doctrine power
- rival strength

Desired result:

The player gains more strategic options at every stage, while the ratio of **usable surplus power per conquest** decreases.

---

# 153. LATE-GAME ECONOMY

Late game shifts away from:

> Can I afford this house?

toward:

> Which region receives investment?

Major Capital sinks:

- reconstruction
- armies
- Wonders
- hybrid infrastructure
- megaprojects
- diplomacy
- regional aid
- Axiom preparation

---

# 154. LATE-GAME INSIGHT

Insight sinks:

- Mythic technologies
- Fusion research
- Leader projects
- Axiom analysis
- citywide system upgrades

Research never becomes irrelevant.

---

# 155. LATE-GAME INFLUENCE

Influence sinks:

- Federation
- regional governance
- Leader integration
- coalition building
- major treaties
- peaceful Ascensions
- Axiom legitimacy decisions

Influence becomes increasingly valuable as empire size grows.

---

# 156. AXIOM PREPARATION

The final campaign should test all three currencies.

Preparation may require:

- Capital for infrastructure and forces
- Insight to understand Axiom systems
- Influence to maintain empire unity

A player who optimized only one dimension should face strategic weaknesses.

---

# 157. BALANCE PATCH PHILOSOPHY

Balance updates should prioritize:

1. removing dominant no-brainer strategies
2. strengthening underused viable strategies
3. preserving faction identity
4. avoiding arbitrary nerfs that invalidate player history
5. respecting persistent Card Instances

Where possible, adjust systems rather than deleting a player's investment.

---

# 158. PVE BALANCE STANDARD

This is primarily a single-player strategy/RPG.

Perfect symmetry is not required.

Civilizations can be intentionally asymmetric.

Balance target:

> Every civilization should have multiple viable paths to success and recognizable weaknesses.

Not:

> Every card has identical expected value.

---

# 159. POWERFUL CARDS ARE ALLOWED

A Legendary or Dominion Card can feel exceptional.

Its balance can come from:

- rarity/acquisition
- footprint
- maintenance
- systemic pressure
- doctrine slot
- utility demand
- strategic vulnerability

Do not flatten all memorable cards into small percentage differences.

---

# 160. ECONOMY SUCCESS QUESTIONS

A playtest economy is healthy if players answer yes:

1. Do Capital, Insight, and Influence feel meaningfully different?
2. Do you usually have more than one worthwhile thing to spend on?
3. Does saving for a large project feel intentional rather than boring?
4. Does population matter to the economy?
5. Does automation feel tempting?
6. Can industry become too aggressive?
7. Does war noticeably cost something?
8. Does a conquest make you more versatile without instantly making you unstoppable?
9. Can you recover from a financial mistake?
10. Do late-game resources still have meaningful uses?

---

# 161. V0.8 PROTOTYPE BALANCE GATE

Before expanding beyond the 3-civilization vertical slice:

- first-hour economy stays within target bands
- no infinite Capital loop
- no zero-worker automation exploit
- no retail-spam economy
- no industrial-spam economy
- no Influence-buyout exploit
- no passive research runaway
- war has real opportunity cost
- all four Forgeweave conquest routes remain economically viable
- first Ascension increases options more than raw output
- 500-cycle simulations remain stable

---

# 162. CANONICAL ECONOMIC LOOP

> **House people → create work → produce value → choose what to finance → research better systems → earn legitimacy → expand → absorb new capabilities → pay the cost of complexity → specialize again.**

---

# 163. BALANCE MANTRA

# CAPITAL BUILDS.
# INSIGHT ADVANCES.
# INFLUENCE LEGITIMIZES.
# POPULATION POWERS.
# WAR CONSUMES.
# CONQUEST COMPLICATES.
# ASCENSION EXPANDS POSSIBILITY.

---

# 164. NEXT CANONICAL PASS

The next design document should be:

# NARRATIVE, LORE & CAMPAIGN BIBLE — v0.9

It should define:

- the history of Eidolon
- the Axiom Crown
- origin of the 20 civilizations
- the Founder
- Convergence Key mythology
- all 20 Leaders' personalities and histories
- civilization political structures
- inter-civilization history
- rivalries
- alliances
- religions/philosophies where applicable
- the Relics
- Axiom mystery progression
- main campaign chapters
- first campaign story
- Operation Iron Veil narrative beats
- Nia Vale's full arc
- Daxton Rhe's arc
- Amara Venn's arc
- branching Leader outcomes
- endgame revelations
- ending conditions
- narrative rules for future quests

The goal of v0.9 is to make the game's systems feel like the inevitable product of a world with a coherent history, rather than mechanics waiting for lore.
