# DOMINION // ASCENDANT
## Playable Systems Bible — v0.2

This document converts the worldbuilding foundation into a prototype-ready ruleset.

---

# 1. SYSTEM DESIGN PRINCIPLES

1. **Cards are persistent physical assets, not temporary spell effects.**
2. **The deck is a constrained civilization build plan.**
3. **The city grid is strategically meaningful without becoming zoning busywork.**
4. **Capital, Insight, and Influence must each answer a different strategic question.**
5. **Conquest expands possibility but does not allow an unrestricted “best cards from every faction” soup deck.**
6. **Every faction’s strength creates pressure from its corresponding systemic problem.**
7. **The vertical slice must prove the full loop before content scale increases.**

---

# 2. TIME MODEL

The core simulation unit is a **Development Cycle**.

- **1 Development Cycle = 30 seconds of active City/Command simulation.**
- Opening menus, deck management, dialogue choices, and tactical pause do not advance the cycle.
- Founder Mode can either run the city in real time or use Tactical Pause depending on difficulty settings.
- Resource generation, upkeep, population growth, construction, and systemic-problem checks resolve once per cycle.

This gives the game deterministic strategy underneath a real-time 3D presentation.

---

# 3. STARTING STATE

A standard new campaign begins with:

- **40 Capital**
- **12 Insight**
- **8 Influence**
- **24 citizens**
- **Founder Hall** already placed and not counted in the 60-card deck
- One basic road connection
- One basic microgrid connection
- A **7-card opening hand**
- One free mulligan of up to 3 cards

The Founder Hall provides per cycle:

- **+3 Capital**
- **+1 Insight**
- **+1 Influence**
- 8 administrative jobs
- 20 local power capacity
- 20 local water capacity

---

# 4. RESOURCE ECONOMY

## Capital

Capital is the velocity resource. It should move constantly.

Typical build costs:

| Tier | Typical Capital Cost |
|---|---:|
| Common | 4–8 |
| Specialized | 7–14 |
| Elite | 12–22 |
| Legendary | 22–36 |
| Mythic | 35–55 |
| Wonder | 45–70 |

Target total Capital generation:

| Stage | Capital / Cycle |
|---|---:|
| Opening | 8–14 |
| Early City | 15–24 |
| Midgame | 25–45 |
| Late Game | 50–90+ |

## Insight

Insight is intentionally slower.

Typical uses:

- Tier II building upgrade: **2–4 Insight**
- Tier III upgrade: **5–8 Insight**
- Advanced unit specialization: **3–8 Insight**
- Elite technology installation: **8–15 Insight**
- Fusion technology: **15–30 Insight**

Target total Insight generation:

| Stage | Insight / Cycle |
|---|---:|
| Opening | 1–3 |
| Early City | 3–6 |
| Midgame | 7–14 |
| Late Game | 15–30+ |

## Influence

Influence controls political scale rather than construction volume.

Typical uses:

- Claim a new district: **5 Influence**
- Enact a citywide policy: **5–12 Influence**
- Recruit a Champion: **8–15 Influence**
- Annex a defeated district: **10–25 Influence**
- Deploy a conquered Leader: **15–25 Influence**
- Construct a Wonder: **15–30 Influence**

Target total Influence generation:

| Stage | Influence / Cycle |
|---|---:|
| Opening | 1–2 |
| Early City | 2–5 |
| Midgame | 6–12 |
| Late Game | 13–25+ |

---

# 5. ECONOMY GUARDRAILS

To prevent exponential runaway:

### Maintenance
Elite and higher assets generally require **1–3 Capital upkeep per cycle**.

### Workforce
Most non-automated facilities require workers. A building at less than 50% staffing operates at 50% output.

### Utilities
Facilities without required power/water/network service enter **Degraded** state and operate at 25% output.

### District Administration
Each district supports 12 active non-residential facilities without penalty. Above that, Administration load begins reducing efficiency unless mitigated.

### Soft Storage Caps
Capital above 250 and Insight above 100 begin suffering small inefficiencies unless dedicated storage/research archive structures are built.

---

# 6. CONSTRUCTION SPEED

| Asset Tier | Base Construction Time |
|---|---:|
| Common | 1 cycle |
| Specialized | 2 cycles |
| Elite | 3 cycles |
| Legendary | 4 cycles |
| Mythic | 6 cycles |
| Wonder | 8 cycles |

A card appears immediately as a full-scale holographic construction footprint, then physically builds itself over the required cycles.

---

# 7. CARD PERSISTENCE

A card is a **deployable asset license**.

When played:

1. The card leaves the hand.
2. Its 3D asset is placed on the city grid.
3. It becomes a persistent world object.
4. It no longer exists in the active draw pile while that asset survives.

If the asset is damaged:

- It can be repaired normally.

If destroyed:

- It enters **Ruined** state for 3 cycles.
- Pay **60% of base Capital cost** to reconstruct it.

If the ruin is intentionally razed or captured:

- The card enters the **Recovery Pool**.
- It can return through a Recommission action, specific facilities, or between campaign chapters.

This preserves the “the card is the building” fantasy without making one unlucky attack permanently delete a rare card.

---

# 8. DECK RULES

Standard City Deck:

**60 cards**

Copy limits:

- Common: max **3**
- Specialized: max **3**
- Elite: max **2**
- Legendary: max **1**
- Mythic: max **1**
- Wonder: max **1**
- Leader: max **1**

Starting campaign deck composition rule:

- Minimum **36 cards** from your chosen civilization
- Maximum **24 Universal/Foundation cards**

After the first conquest:

- Minimum **30 cards** from your founding civilization
- Maximum **12 cards from any single conquered civilization**
- Maximum **18 total foreign-faction cards** until later tech expands the limit

This protects faction identity.

---

# 9. DOMINION LOADOUT

Conquering a civilization permanently unlocks its Dominion Ability, but stacking all 20 simultaneously would destroy balance.

The player therefore equips:

- **1 Founding Doctrine** — always active
- **3 Dominion Doctrines** — selected from conquered civilizations
- **2 Fusion Doctrines** — unlocked later through compatible civilization pairs

All conquered powers remain permanently owned and can be reconfigured at the capital between major operations.

---

# 10. DRAW AND BUILD FLOW

- Opening hand: **7 cards**
- Draw: **1 card per Development Cycle**
- Hand limit: **10**
- At 10 cards, additional draws enter a 3-slot **Reserve Queue**
- If the Reserve Queue is full, the oldest card is sent to the bottom of the deck
- One free mulligan of up to 3 cards at campaign start
- Certain Office/Leader assets manipulate draw order, search the deck, or recover cards

There are no arbitrary “mana” points. A card is playable whenever the city can afford and support it.

---

# 11. THE GRID

## Prototype

**32 × 32 cells**

Each cell represents approximately **8 meters × 8 meters**.

Physical footprint:

**256 m × 256 m**

The prototype is divided into:

**4 × 4 districts**, each district containing **8 × 8 cells**.

## Full Game

**128 × 128 cells**

Approximately:

**1.024 km × 1.024 km** of primary build area per major city.

---

# 12. FOOTPRINT CLASSES

| Footprint | Typical Asset |
|---|---|
| 1×1 | Utility, kiosk, pylon, small unit pad |
| 1×2 | Small retail, townhouse, office |
| 2×2 | Standard facility |
| 2×3 | Large office, factory, civic facility |
| 3×3 | Major industrial/research building |
| 4×4 | Landmark |
| 5×5 | Legendary megastructure |
| District-scale | Wonder / special terrain system |

The visible architecture can overhang cells. The grid is logical, not visually boxy.

---

# 13. ROADS AND INFRASTRUCTURE

Roads, power lines, water lines, and data links are not individual deck cards.

Instead, **Infrastructure cards create networks**.

Example:

**Transit Nexus**
- occupies 2×2 cells
- includes 12 Road Charges
- each Road Charge lays one cell of road

**Neural Relay**
- physically occupies 1×1
- creates data coverage within a 5-cell radius

This prevents the deck from becoming 30 road cards while preserving the rule that important infrastructure comes from placed cards.

---

# 14. PLACEMENT REQUIREMENTS

Unless a card explicitly overrides the rule, a facility requires:

- Road access within 1 cell
- Sufficient power
- Sufficient water
- Valid terrain
- Free footprint
- Required district claim
- Required worker capacity

Specialized assets may additionally require:

- Network coverage
- Elevation
- Natural terrain
- Nearby research
- Nearby industry
- Low pollution
- High Security
- Specific faction tags

---

# 15. ADJACENCY MODEL

Each asset has tags such as:

`Residential`, `Commercial`, `Research`, `Network`, `Natural`, `Industrial`, `Civic`, `Security`, `Transit`, `Education`.

Adjacency checks use a default **2-cell radius**.

Suggested formula:

`Final Output = Base Output × (1 + Positive Adjacency - Negative Adjacency + District Modifier)`

Guardrails:

- Positive adjacency capped at **+40%** before faction abilities
- Negative adjacency capped at **-50%**
- Wonders and Dominion effects can exceed those caps

Example:

University adjacent to Research Institute: **+10% Insight**  
Aethernet Data Hub connected: additional **+8%**  
Synara Cognitive Tower connected: additional **+7%**

Total: **+25%**

---

# 16. DISTRICT SPECIALIZATION

When a district contains at least **6 assets sharing a primary tag**, it may specialize.

Examples:

- Research District: +10% Insight
- Industrial District: +10% production
- Cultural District: +10% Influence
- Commercial District: +10% Capital
- Residential District: +10% capacity
- Defense District: +10% fortification
- Eco District: +10% regeneration
- Transit District: +10% Mobility

Mixed-use districts remain viable and gain better Stability but no maximum-output specialization.

---

# 17. CITY METRICS

All cities track:

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

Most values run from **0–100**.

Critical thresholds:

- 80–100: Exceptional
- 60–79: Healthy
- 40–59: Strained
- 20–39: Crisis
- 0–19: Collapse risk

Faction systemic problems read these metrics plus faction-specific pressure meters.

---

# 18. UNIVERSAL UPGRADE SYSTEM

Most assets have **three levels**.

## Level I — Established
Base card state.

## Level II — Specialize
Choose one of two branches.

Typical cost:
- 60% of original Capital cost
- 2–4 Insight

## Level III — Ascendant
Upgrade the chosen specialization.

Typical cost:
- 100% of original Capital cost
- 5–8 Insight
- 2–5 Influence for civic/legendary assets

---

# 19. UPGRADE BRANCHES BY CARD TYPE

| Type | Branch A | Branch B |
|---|---|---|
| Residential | Density | Quality |
| Office | Specialization | Administration |
| Retail | Volume | Prestige |
| Industrial | Throughput | Precision |
| Research | Discovery | Applied Science |
| Civic | Unity | Authority |
| Infrastructure | Capacity | Resilience |
| Defense | Firepower | Fortification |
| Unit | Assault | Support |
| Vehicle | Speed | Armor |
| Champion | Signature Talent | Leadership Talent |

A player therefore upgrades not merely “more,” but **toward a city philosophy**.

---

# 20. FUSION UPGRADES

When two conquered civilizations have a compatible synergy, a Level III asset may gain a **Fusion Socket**.

Examples:

- Virelia + Eden Circuit → Living Architecture
- Synara + Automara → Synthetic Society
- Nocturne + Aethernet → Aegis Network
- Auric Ledger + Signal Republic → Attention Economy
- Forgeweave + Vector Forge → Autonomous Manufacturing
- Hearthspire + Terravanta → Planetary Urbanism
- Mnemosyne + Epoch Vault → Predictive Education

Fusion upgrades require significant Insight and a physical installable Fusion Module.

---

# 21. 20 CIVILIZATION CORE CARD SETS

Each faction receives a 15-card **Core Identity Set**. These are not the whole faction collection; they establish the minimum gameplay vocabulary for the first full production pass.


## 01 — Synara

| Card | Type | Core Effect |
|---|---|---|
| **Adaptive Habitat** | Residential | Houses 8 citizens; +1 Agency if adjacent to a Civic asset. |
| **Civic Autonomy Pods** | Residential | Lower density housing that protects Agency from automation pressure. |
| **Cognitive Operations Tower** | Office | Creates Orchestrators; +2 Insight per Development Cycle. |
| **Predictive Bureau** | Office | Reveals one upcoming city event and boosts adjacent offices. |
| **Autonomous Exchange** | Retail | Generates +4 Capital without workers; adds 1 Dependency. |
| **Algorithmic Market** | Retail | Capital output rises when linked to three or more networked assets. |
| **Synthetic Fabrication Node** | Industrial | Produces machine components and reduces construct time of robotic assets. |
| **Swarm Foundry** | Industrial | Creates drone units; gains throughput from adjacent automation assets. |
| **Neural Relay** | Infrastructure | Provides network coverage to a 5-cell radius. |
| **Orchestration Hub** | Infrastructure | Assigns one AI Worker to an adjacent facility. |
| **Guardian Drone Cohort** | Unit | Fast defensive drone squad; does not consume population. |
| **Audit Sentinel** | Unit | Detects sabotage, corruption, and runaway automation. |
| **Agency Forum** | Civic | Restores civilian Agency and reduces Dependency in its district. |
| **Archon Mira Vey** | Leader | Once per 5 cycles, automate one facility for free for 3 cycles. |
| **The Thinking Spire** | Wonder | Once built, one entire district can operate autonomously. |

## 02 — Concord Vale

| Card | Type | Core Effect |
|---|---|---|
| **Commons Residence** | Residential | Houses 10 citizens; +Happiness when adjacent to another residential asset. |
| **Kinship Block** | Residential | Generates +1 Influence when district Stability is high. |
| **Cooperative Guild Hall** | Office | Produces Stewards and Coalition Builders. |
| **Mediation House** | Office | Reduces faction tension in its district. |
| **Citizen Marketplace** | Retail | Generates Capital and small Influence based on Happiness. |
| **Mutual Exchange** | Retail | Shares 20% of its income with the lowest-wealth adjacent district. |
| **Community Works** | Industrial | Low-output factory with no Equality penalty. |
| **Artisan Cooperative** | Industrial | Creates high-value crafted goods; scales with Education. |
| **Assembly Square** | Civic | Generates +3 Influence; hosts faction negotiations. |
| **Commons Transit Hub** | Infrastructure | Adds district Mobility and citizen interaction bonuses. |
| **Community Marshal Cohort** | Unit | Defensive peacekeeping unit with low unrest generation. |
| **Coalition Envoys** | Unit | Can peacefully reduce resistance in rival districts. |
| **Consensus Charter** | Civic | District policies gain +10% effectiveness while no faction is hostile. |
| **Mayor Solenne Hart** | Leader | Convert one hostile faction to neutral after completing its demand. |
| **The Great Assembly** | Wonder | Conquered districts receive a major Loyalty bonus. |

## 03 — Hearthspire

| Card | Type | Core Effect |
|---|---|---|
| **Vertical Village** | Residential | Houses 14 citizens on a 2x2 footprint. |
| **Stacked Microhomes** | Residential | Cheap 1x2 housing that can be upgraded vertically twice. |
| **Urban Planning Atelier** | Office | Reduces placement and road costs inside its district. |
| **Housing Authority** | Office | Raises Stability when housing demand is satisfied. |
| **Neighborhood Arcade** | Retail | Gains Capital from nearby residential density. |
| **Skybridge Market** | Retail | Connects two elevated residential structures. |
| **Modular Construction Yard** | Industrial | Reduces build times for Residential cards. |
| **Prefab Megafactory** | Industrial | Produces modular structure components. |
| **Utility Spine** | Infrastructure | Carries power, water, and transit vertically. |
| **District Transit Core** | Infrastructure | Raises capacity of dense districts. |
| **Infrastructure Wardens** | Unit | Repair damaged buildings and utilities during sieges. |
| **Urban Response Crew** | Unit | Rapidly restores services after disasters. |
| **Public Courtyard** | Civic | Offsets Happiness penalties from high density. |
| **Governor Cassian Rowe** | Leader | Once per 4 cycles, add one temporary housing tier to a residence. |
| **The Endless Habitat** | Wonder | Massively increases city population cap without consuming new land. |

## 04 — Nova Crucible

| Card | Type | Core Effect |
|---|---|---|
| **Researcher Enclave** | Residential | Houses 8; residents generate small passive Insight. |
| **Prototype Dormitory** | Residential | Cheap housing; adjacent experiments gain faster staffing. |
| **Innovation Campus** | Office | Generates +3 Insight and Experimentalist specialists. |
| **Anomaly Analysis Bureau** | Office | Reduces severity of failed experiments. |
| **Prototype Bazaar** | Retail | Sells discovered prototypes for Capital. |
| **Patent Exchange** | Retail | Converts a portion of Insight generation into Capital. |
| **Experimental Foundry** | Industrial | Can craft assets one tech tier early with failure risk. |
| **Quantum Materials Lab** | Industrial | Produces rare materials for Elite and Legendary cards. |
| **Field Test Range** | Research | Boosts unit and vehicle research. |
| **Containment Grid** | Infrastructure | Prevents experiment failures spreading outside its radius. |
| **Anomaly Hunters** | Unit | Specialists against mutated, unstable, or exotic enemies. |
| **Prototype Vanguard** | Unit | Uses one random advanced weapon each deployment. |
| **Risk Review Council** | Civic | Spend Influence to reroll a failed experiment outcome. |
| **Director Kael Orin** | Leader | Guarantees the next Breakthrough attempt but doubles its Insight cost. |
| **The Crucible** | Wonder | Permits permanent access to research one era ahead. |

## 05 — Terravanta

| Card | Type | Core Effect |
|---|---|---|
| **Terraced Habitat** | Residential | Houses 10; gains Happiness from elevation and natural tiles. |
| **Cliffside Commune** | Residential | Can be placed on slopes normally unsuitable for construction. |
| **Geoengineering Bureau** | Office | Produces Geomancers and Surveyors. |
| **Watershed Office** | Office | Improves water security across connected districts. |
| **Earthworks Market** | Retail | Generates Capital from terraforming projects. |
| **Stone & Soil Exchange** | Retail | Turns excess terrain resources into Capital. |
| **Terraforming Plant** | Industrial | Creates terrain-edit charges. |
| **Geothermal Works** | Industrial | Generates power from suitable terrain. |
| **Watershed Engine** | Infrastructure | Creates or redirects a water channel. |
| **Seismic Stabilizer** | Infrastructure | Protects nearby buildings from terrain instability. |
| **Geo Warden Crew** | Unit | Repairs terrain damage and erects combat earthworks. |
| **Survey Expedition** | Unit | Reveals hidden resources and terrain hazards. |
| **Land Steward Forum** | Civic | Reduces environmental instability from nearby development. |
| **Warden Ilyra Stone** | Leader | Terraform one 2x2 area at no Capital cost once per 5 cycles. |
| **The World Engine** | Wonder | Unlocks large-scale permanent terrain editing. |

## 06 — Virelia

| Card | Type | Core Effect |
|---|---|---|
| **Symbiotic Habitat** | Residential | Residents regenerate Health and resist disease. |
| **Adaptive Nest** | Residential | Changes its bonuses to match nearby district type. |
| **Genome Institute** | Office | Produces Gene Smiths; +3 Insight. |
| **Evolution Clinic** | Office | Unlocks biological unit modifications. |
| **Augmentation Clinic** | Retail | Sells enhancements for Capital but increases Mutation pressure. |
| **Bio Apothecary** | Retail | Improves city Health and generates modest Capital. |
| **Tissue Fabricator** | Industrial | Produces organic construction material. |
| **Protein Forge** | Industrial | Creates biological equipment and unit upgrades. |
| **Spore Utility Network** | Infrastructure | Distributes organic resources between nearby buildings. |
| **Purity Station** | Infrastructure | Reduces Mutation pressure in a 4-cell radius. |
| **Adaptive Guard** | Unit | Gains resistance to the last damage type received. |
| **Field Surgeons** | Unit | Heal biological units and civilians. |
| **Ethics Conclave** | Civic | Limits Mutation growth from augmentation and restores Influence. |
| **Dr. Seraphine Voss** | Leader | Apply one controlled mutation to a unit without risk. |
| **The Helix Cathedral** | Wonder | All biological units gain an augmentation slot. |

## 07 — Forgeweave

| Card | Type | Core Effect |
|---|---|---|
| **Worker Arcology** | Residential | Dense worker housing; +1 Industrial staffing per adjacent factory. |
| **Forge Quarters** | Residential | Residents gain Stability from high industrial employment. |
| **Production Directorate** | Office | Boosts factory throughput in its district. |
| **Industrial Design Bureau** | Office | Reduces Capital cost of upgrading Industrial assets. |
| **Industrial Exchange** | Retail | Generates Capital from factory output. |
| **Machine Parts Market** | Retail | Nearby Industrial assets pay less maintenance. |
| **Infinite Foundry** | Industrial | High-output production facility with heavy resource demand. |
| **Replication Forge** | Industrial | Can duplicate one Common or Specialized asset card after charging. |
| **Freight Furnace** | Infrastructure | Moves industrial inputs rapidly between adjacent factories. |
| **Smog Reclaimer** | Infrastructure | Reduces Health/Ecology penalties from industry. |
| **Forge Guard** | Unit | Heavy infantry that gains armor near Industrial assets. |
| **Mechanist Crew** | Unit | Repairs factories and vehicles during combat. |
| **Workers' Canteen** | Civic | Offsets worker Happiness loss from industrial density. |
| **Forge Lord Daxton Rhe** | Leader | Overdrive all factories in one district for 2 cycles. |
| **The Grand Forge** | Wonder | Periodically creates a duplicate non-Legendary asset card. |

## 08 — Eden Circuit

| Card | Type | Core Effect |
|---|---|---|
| **Garden Commune** | Residential | Houses 8; adjacent natural tiles generate Happiness. |
| **Canopy Habitat** | Residential | Can be built without clearing forest tiles. |
| **Ecological Design House** | Office | Produces Ecosystem Engineers and Cultivators. |
| **Restoration Bureau** | Office | Repairs degraded natural tiles. |
| **Harvest Market** | Retail | Generates Capital from nearby regenerative land. |
| **Seed Exchange** | Retail | Converts surplus ecology output into Capital or Influence. |
| **Regenerative Bioworks** | Industrial | Produces materials without permanent Ecology loss. |
| **Mycelium Works** | Industrial | Shares resources underground with nearby assets. |
| **Living Waterway** | Infrastructure | Provides water and Ecology to connected tiles. |
| **Pollinator Corridor** | Infrastructure | Links natural zones and improves regeneration. |
| **Ranger Circle** | Unit | Excels in natural terrain and detects ecological threats. |
| **Symbiosis Keepers** | Unit | Restore Ecology and calm wildlife events. |
| **Balance Grove** | Civic | Reduces development penalties in its district. |
| **Caretaker Amara Venn** | Leader | Restore one degraded 3x3 natural area instantly. |
| **The Worldgarden** | Wonder | All undeveloped natural tiles produce resources each cycle. |

## 09 — Automara

| Card | Type | Core Effect |
|---|---|---|
| **Synthetic Habitation Stack** | Residential | Provides maintenance bays and housing for synthetic citizens. |
| **Human-Machine Quarter** | Residential | Mixed housing that reduces machine-personhood tension. |
| **Robotics Directorate** | Office | Creates Mechwrights and Drone Commanders. |
| **Personhood Bureau** | Office | Improves Loyalty of advanced synthetic citizens. |
| **Machine Exchange** | Retail | Generates Capital from robotic services. |
| **Upgrade Market** | Retail | Sells modular parts and reduces unit upgrade costs. |
| **Automaton Foundry** | Industrial | Produces robotic workers and combat units. |
| **Servo Works** | Industrial | Creates vehicle and robot components. |
| **Charging Grid** | Infrastructure | Supplies robotic units within a wide radius. |
| **Command Mesh** | Infrastructure | Increases control capacity for autonomous units. |
| **Synthetic Vanguard** | Unit | Heavy robotic combat squad; no population cost. |
| **Repair Drones** | Unit | Automatically repair nearby machines. |
| **Rights Forum** | Civic | Lets the player choose and manage machine legal status. |
| **Marshal AX-9 Aya** | Leader | Deploy a temporary elite synthetic squad once per battle. |
| **The First Mind** | Wonder | Unlocks synthetic citizens as a full population type. |

## 10 — Aethernet

| Card | Type | Core Effect |
|---|---|---|
| **Connected Habitat** | Residential | Houses 9; gains bonuses from network coverage. |
| **Mesh Apartments** | Residential | Each adjacent networked residence adds +1 capacity. |
| **Network Tower** | Office | Produces Network Weavers; extends data coverage. |
| **Cyber Operations Center** | Office | Detects and counters network intrusion. |
| **Digital Exchange** | Retail | Capital output scales with number of connected facilities. |
| **Remote Market** | Retail | Can trade with districts without road adjacency. |
| **Data Fabrication Center** | Industrial | Turns Insight into advanced components. |
| **Server Foundry** | Industrial | Provides compute capacity to research and defense assets. |
| **Neural Relay** | Infrastructure | Connects all assets within 5 cells. |
| **Zero-Trust Gateway** | Infrastructure | Stops cyber infections from crossing its network boundary. |
| **Protocol Knights** | Unit | Cyber-defense unit capable of disabling hostile systems. |
| **Signal Runners** | Unit | Fast infiltration and sabotage specialists. |
| **Open Protocol Forum** | Civic | Raises Influence from highly connected districts. |
| **Nexus Regent Lyra Quinn** | Leader | Instantly reconnect one disabled network segment. |
| **The Infinite Mesh** | Wonder | All connected assets receive a global adjacency bonus. |

## 11 — Nocturne Bastion

| Card | Type | Core Effect |
|---|---|---|
| **Secured Arcology** | Residential | High Security; slightly reduces Freedom. |
| **Watch Quarter** | Residential | Residents provide passive local detection. |
| **Intelligence Citadel** | Office | Produces operatives and reveals enemy infiltration. |
| **Threat Analysis Bureau** | Office | Forecasts likely attack vectors. |
| **Protected Exchange** | Retail | Hard to raid; earns bonus Capital during wartime. |
| **Licensed Armory** | Retail | Reduces equipment cost for defensive units. |
| **Defense Forge** | Industrial | Produces fortification components and weapons. |
| **Armor Works** | Industrial | Upgrades defensive units and vehicles. |
| **Aegis Pylon** | Defense | Projects a local shield over adjacent structures. |
| **Bastion Wall Hub** | Infrastructure | Grants wall segments and hardened roadblocks. |
| **Sentinel Cohort** | Unit | Elite defensive infantry. |
| **Shadow Wardens** | Unit | Stealth counter-infiltration specialists. |
| **Civil Liberties Tribunal** | Civic | Reduces Dissent caused by surveillance. |
| **Commander Orin Black** | Leader | Lock down one district, greatly increasing defense for 2 cycles. |
| **The Black Citadel** | Wonder | Projects a permanent citywide defensive shield. |

## 12 — Veyra

| Card | Type | Core Effect |
|---|---|---|
| **Transit Habitat** | Residential | Houses 9; residents travel instantly within its district transit network. |
| **Waypoint Lofts** | Residential | Cheap housing near transit nodes. |
| **Mobility Command** | Office | Produces Transit Engineers and Pathbreakers. |
| **Flow Analytics Bureau** | Office | Reduces congestion and maintenance costs. |
| **Passage Terminal** | Retail | Generates Capital from travelers. |
| **Velocity Market** | Retail | Income rises with district Mobility. |
| **Vehicle Works** | Industrial | Produces high-speed ground vehicles. |
| **Mag-Rail Foundry** | Industrial | Creates advanced transit components. |
| **Transit Nexus** | Infrastructure | Links distant districts through rapid transit. |
| **Momentum Lane Hub** | Infrastructure | Grants high-speed road segments. |
| **Velocity Riders** | Unit | Fast strike unit with hit-and-run bonuses. |
| **Courier Corps** | Unit | Moves resources or mission items instantly between linked nodes. |
| **Maintenance Depot** | Civic | Reduces Wear accumulation nearby. |
| **Rider-King Talon Vey** | Leader | All units gain maximum movement speed for one battle phase. |
| **The Horizon Rail** | Wonder | Creates a permanent high-speed network between all owned districts. |

## 13 — Civic Meridian

| Card | Type | Core Effect |
|---|---|---|
| **Civic Quarter** | Residential | Houses 10; Stability rises when city services are healthy. |
| **Public Housing Forum** | Residential | Low-cost housing with Equality bonus. |
| **Administrative Forum** | Office | Produces Stewards and Administrators. |
| **Services Directorate** | Office | Improves efficiency of nearby Civic assets. |
| **Public Exchange** | Retail | Generates steady Capital unaffected by market volatility. |
| **Permit Market** | Retail | Reduces Capital cost of new district development. |
| **Municipal Works** | Industrial | Produces civic and infrastructure materials. |
| **Public Utility Works** | Industrial | Improves basic service reliability. |
| **Service Center** | Civic | Raises Health, Stability, and Happiness modestly. |
| **Policy Archive** | Civic | Stores one additional district-level policy. |
| **Civic Marshals** | Unit | Defensive unit with low occupation penalties. |
| **Public Architects** | Unit | Rapidly repair civic and infrastructure assets. |
| **Bureaucracy Office** | Infrastructure | Reduces Administration load in adjacent assets. |
| **First Steward Adira Sol** | Leader | Temporarily activate one extra civilization policy. |
| **The People's Forum** | Wonder | Permanently grants an additional global policy slot. |

## 14 — Auric Ledger

| Card | Type | Core Effect |
|---|---|---|
| **Merchant Heights** | Residential | High-Wealth housing that boosts nearby Retail. |
| **Dividend Row** | Residential | Residents generate small passive Capital. |
| **Capital Exchange** | Office | Produces Financiers and Market Strategists. |
| **Risk Bureau** | Office | Reduces losses from market events. |
| **Grand Bazaar** | Retail | High Capital output based on nearby population. |
| **Futures Market** | Retail | Can bank production for a larger delayed payout. |
| **Asset Mint** | Industrial | Converts industrial goods into tradeable Capital instruments. |
| **Bond Works** | Industrial | Finances nearby construction at reduced immediate cost. |
| **Clearing House** | Infrastructure | Links commercial districts for shared income bonuses. |
| **Reserve Vault** | Infrastructure | Stores protected Capital during raids and crashes. |
| **Economic Wardens** | Unit | Protect commercial districts and suppress looting. |
| **Trade Envoys** | Unit | Improve trade agreements and economic conquest. |
| **Equity Forum** | Civic | Reduces Inequality generated by wealthy districts. |
| **Chancellor Nyra Vale** | Leader | Apply Compound Dominion to stored Capital for one boosted cycle. |
| **The Infinite Vault** | Wonder | Stored Capital earns permanent compounding interest. |

## 15 — Signal Republic

| Card | Type | Core Effect |
|---|---|---|
| **Creator District** | Residential | Residents generate small Influence from high Happiness. |
| **Studio Lofts** | Residential | Boost nearby media and cultural facilities. |
| **Signal Studio** | Office | Produces Broadcasters and Narrative Strategists. |
| **Verification Desk** | Office | Reduces Misinformation spread. |
| **Experience Market** | Retail | Generates Capital and Influence from visitors. |
| **Creator Exchange** | Retail | Scales with nearby cultural output. |
| **Broadcast Complex** | Industrial | Produces media assets and transmission capacity. |
| **Immersive Works** | Industrial | Creates city-scale experiences and propaganda defenses. |
| **Transmission Spire** | Infrastructure | Projects Influence across adjacent borders. |
| **Public Network Node** | Infrastructure | Raises communication coverage and citizen awareness. |
| **Signal Corps** | Unit | Disrupts enemy propaganda and communications. |
| **Cultural Envoys** | Unit | Accelerates Influence conquest. |
| **Independent Press Hall** | Civic | Reduces Misinformation while preserving Influence output. |
| **Luminary Zara Vox** | Leader | Launch a citywide broadcast that doubles Influence for one cycle. |
| **The Voice of Eidolon** | Wonder | Projects cultural Influence across the world map. |

## 16 — Lexara

| Card | Type | Core Effect |
|---|---|---|
| **Judicial Quarter** | Residential | High Stability when Law load is balanced. |
| **Advocate Row** | Residential | Produces citizens suited to legal and civic roles. |
| **Tribunal Tower** | Office | Produces Arbiters and Advocates. |
| **Investigation Bureau** | Office | Reveals corruption, crime, and covert hostile actions. |
| **Licensed Exchange** | Retail | Generates reliable Capital under strong legal order. |
| **Contract Market** | Retail | Improves trade agreements and reduces breach risk. |
| **Compliance Works** | Industrial | Produces certified high-value components. |
| **Evidence Fabrication Lab** | Industrial | Supports investigations and forensic technology. |
| **Court Network** | Civic | Extends legal effects across its district. |
| **Appeals Forum** | Civic | Reduces Happiness penalties from strict laws. |
| **Lawkeepers** | Unit | Strong against crime, sabotage, and insurgency. |
| **Investigators** | Unit | Reveal hidden units and illegal operations. |
| **Regulatory Archive** | Infrastructure | Stores one extra district Law. |
| **High Arbiter Cassia Rune** | Leader | Suspend or enact one Law without cooldown. |
| **The Eternal Tribunal** | Wonder | Permanently grants an additional civilization Law slot. |

## 17 — Mnemosyne

| Card | Type | Core Effect |
|---|---|---|
| **Scholar Residence** | Residential | Residents gain experience faster. |
| **Mentor Commons** | Residential | New citizens begin with bonus Education. |
| **Academy Tower** | Office | Produces Scholars and Mentors. |
| **Cognition Lab** | Office | Accelerates class progression. |
| **Knowledge Exchange** | Retail | Converts surplus Insight into Capital. |
| **Archive Market** | Retail | Generates revenue from discovered lore and artifacts. |
| **Cognitive Fabrication Lab** | Industrial | Produces advanced educational and simulation hardware. |
| **Memory Works** | Industrial | Creates training modules for units. |
| **Learning Grid** | Infrastructure | Shares experience bonuses among connected facilities. |
| **Archive Node** | Infrastructure | Protects discovered technologies from sabotage. |
| **Lorekeepers** | Unit | Support unit that boosts allied experience gain. |
| **Mentor Cadre** | Unit | Can instantly train a basic unit into a specialized class. |
| **Public Academy** | Civic | Raises citywide Education. |
| **Provost Elian Sage** | Leader | Grant one unit or specialist an immediate level. |
| **The Infinite Library** | Wonder | All citizens and units gain permanent accelerated mastery. |

## 18 — Vector Forge

| Card | Type | Core Effect |
|---|---|---|
| **Logistics Quarter** | Residential | Workers travel efficiently to supply facilities. |
| **Depot Housing** | Residential | Cheap housing near industrial/logistics districts. |
| **Routing Command** | Office | Produces Route Masters and Quartermasters. |
| **Flow Control Bureau** | Office | Identifies and reroutes around supply bottlenecks. |
| **Distribution Exchange** | Retail | Capital scales with goods moved through the district. |
| **Freight Market** | Retail | Sells excess logistics capacity. |
| **Megahub** | Industrial | Stores and redistributes huge volumes of resources. |
| **Container Works** | Industrial | Reduces transport cost of industrial goods. |
| **Supply Spine** | Infrastructure | Automatically moves materials between linked assets. |
| **Redundant Junction** | Infrastructure | Keeps supply chains alive after one node is destroyed. |
| **Logistics Commandos** | Unit | Can establish temporary battlefield supply points. |
| **Quartermaster Corps** | Unit | Reduces upkeep of nearby units. |
| **Resilience Office** | Civic | Reduces penalties from disrupted supply chains. |
| **Route Marshal Juno Strake** | Leader | Reroute all disconnected assets through one temporary network. |
| **The Grand Junction** | Wonder | Automates resource distribution across all owned districts. |

## 19 — Nomad Reach

| Card | Type | Core Effect |
|---|---|---|
| **Frontier Habitat** | Residential | Can be placed outside contiguous territory at reduced cost. |
| **Mobile Homestead** | Residential | May relocate once per chapter. |
| **Expedition Hall** | Office | Produces Pathfinders and Expeditioneers. |
| **Cartography Bureau** | Office | Reveals nearby terrain and hidden sites. |
| **Caravan Exchange** | Retail | Generates Capital from distance traveled and remote trade. |
| **Frontier Market** | Retail | Operates without full infrastructure for limited periods. |
| **Mobile Fabricator** | Industrial | Can relocate and build remote assets. |
| **Field Works** | Industrial | Produces basic resources from undeveloped territory. |
| **Beacon Tower** | Infrastructure | Claims remote territory in a radius. |
| **Trail Hub** | Infrastructure | Creates low-cost frontier routes. |
| **Pathfinder Rangers** | Unit | Excellent scouts with high wilderness mobility. |
| **Expedition Crew** | Unit | Can establish temporary outposts. |
| **Frontier Council** | Civic | Raises Loyalty of distant settlements. |
| **Pathfinder Keira Ash** | Leader | Establish one free temporary forward settlement. |
| **The Gateway** | Wonder | Allows permanent non-contiguous colonies anywhere discovered. |

## 20 — Epoch Vault

| Card | Type | Core Effect |
|---|---|---|
| **Continuity Habitat** | Residential | Protects residents from disaster and economic shocks. |
| **Legacy Enclave** | Residential | Stores cultural continuity and Stability. |
| **Futures Observatory** | Office | Reveals probable upcoming events. |
| **Scenario Bureau** | Office | Improves accuracy of predictions. |
| **Probability Exchange** | Retail | Lets the player wager Capital on forecast outcomes. |
| **Continuity Market** | Retail | Generates stable income during crises. |
| **Preservation Complex** | Industrial | Stores strategic materials against future shortages. |
| **Resilience Foundry** | Industrial | Produces disaster-resistant components. |
| **Forecast Array** | Infrastructure | Extends prediction horizon by one event. |
| **Continuity Grid** | Infrastructure | Keeps essential systems running during emergencies. |
| **Epoch Guardians** | Unit | Gain bonuses when fighting a predicted threat. |
| **Contingency Corps** | Unit | Can deploy instantly to a forecast disaster. |
| **Free Will Forum** | Civic | Reduces Future Lock created by excessive prediction. |
| **Chronarch Elara Prime** | Leader | Reveal two alternative outcomes for the next major event. |
| **The Thousand-Year Engine** | Wonder | Permanently previews major world events before they occur. |

---

# 22. UNIVERSAL FOUNDATION SET

These cards can appear in any civilization deck.

| Card | Type | Effect |
|---|---|---|
| **Compact Residence** | Residential | Houses 6 citizens at low cost. |
| **Civic Apartments** | Residential | Houses 8; +Stability if basic services are healthy. |
| **Corner Exchange** | Retail | +2 Capital per cycle. |
| **District Market** | Retail | +3 Capital per cycle; scales with nearby population. |
| **Field Workshop** | Industrial | Produces basic construction materials. |
| **Utility Fabricator** | Industrial | Reduces infrastructure repair cost. |
| **Administrative Office** | Office | Adds district Administration capacity. |
| **Research Annex** | Research | +2 Insight per cycle. |
| **Public Clinic** | Civic | Raises Health in a 3-cell radius. |
| **Community Plaza** | Civic | +2 Influence and +Happiness nearby. |
| **Microgrid Station** | Infrastructure | Provides 25 power capacity in a 4-cell radius. |
| **Water Reclaimer** | Infrastructure | Provides 25 water capacity in a 4-cell radius. |
| **Transit Stop** | Infrastructure | Grants 6 Road Charges and improves Mobility. |
| **Warehouse** | Infrastructure | Adds material storage and reduces local supply disruption. |
| **Watch Post** | Defense | Basic detection and district defense. |
| **Barrier Hub** | Defense | Grants 6 defensive barrier segments. |
| **Civic Guard Squad** | Unit | Basic defensive squad. |
| **Survey Team** | Unit | Reveals terrain and hidden resources. |
| **Founder Monument** | Civic | +3 Influence; boosts Founder reputation locally. |
| **Recovery Depot** | Infrastructure | Returns one destroyed Common/Specialized card from Recovery Pool every 6 cycles. |

---

# 23. FIRST CONCRETE 60-CARD STARTER DECK
## SYNARA — “THE ORCHESTRATED CITY”

This is the recommended first playable starter deck because Synara naturally teaches automation, networks, adjacency, civilian needs, and the danger of over-optimization.

The **Founder Hall** is placed before play and is not counted in the 60.

### Residential — 12

- 3× Adaptive Habitat
- 3× Civic Autonomy Pods
- 3× Compact Residence
- 3× Civic Apartments

### Economy / Retail — 9

- 3× Autonomous Exchange
- 2× Algorithmic Market
- 2× Corner Exchange
- 2× District Market

### Office / Research — 8

- 3× Cognitive Operations Tower
- 2× Predictive Bureau
- 2× Research Annex
- 1× Administrative Office

### Industrial — 7

- 3× Synthetic Fabrication Node
- 2× Swarm Foundry
- 1× Field Workshop
- 1× Utility Fabricator

### Infrastructure — 10

- 3× Neural Relay
- 2× Orchestration Hub
- 2× Microgrid Station
- 1× Water Reclaimer
- 1× Transit Stop
- 1× Warehouse

### Civic / Stability — 6

- 3× Agency Forum
- 1× Community Plaza
- 1× Public Clinic
- 1× Founder Monument

### Defense / Units — 6

- 2× Guardian Drone Cohort
- 2× Audit Sentinel
- 1× Watch Post
- 1× Barrier Hub

### Leader — 1

- 1× Archon Mira Vey

### Wonder — 1

- 1× The Thinking Spire

**TOTAL: 60 cards**

---

# 24. SYNARA STARTER CURVE

The deck should be tuned so a normal opening can produce this sequence:

## Cycle 0

Start:
- 40 Capital
- 12 Insight
- 8 Influence
- Founder Hall active

Ideal first placements:
- Adaptive Habitat
- Microgrid Station
- Corner Exchange

## Cycle 1–2

Add:
- Neural Relay
- Cognitive Operations Tower
- Water Reclaimer

The player now understands:
- utility coverage
- card footprints
- workforce
- network coverage
- passive Insight

## Cycle 3–5

Add:
- Autonomous Exchange
- Orchestration Hub
- Agency Forum

This introduces Synara’s central decision:

> Automation increases output, but excessive automation creates Dependency and lowers Agency.

## Cycle 6–10

The player can:
- specialize a district
- build Swarm Foundry
- deploy Guardian Drones
- upgrade key assets to Tier II
- begin planning for the Thinking Spire

A good first ten-cycle city should produce approximately:

- **18–25 Capital / cycle**
- **5–8 Insight / cycle**
- **3–6 Influence / cycle**

without entering severe Dependency.

---

# 25. SYNARA PRESSURE METER

Synara’s unique systemic meter is:

# DEPENDENCY

Dependency ranges from 0–100.

Typical increases:

- Autonomous Exchange: +1 per 3 cycles
- AI Worker assignment: +1 per 4 cycles
- Fully automated district: +2 per cycle
- Thinking Spire: +5 immediately when activated

Typical reductions:

- Agency Forum: -2 per cycle in its district
- High Education: slows Dependency growth
- Human-staffed facilities: -1 periodically
- Certain policies: reduce automation pressure

Thresholds:

| Dependency | Effect |
|---|---|
| 0–24 | Healthy augmentation |
| 25–49 | -5% Agency |
| 50–69 | -10% civilian productivity without automation |
| 70–84 | Anti-machine factions can spawn |
| 85–99 | Random automated assets may ignore orders |
| 100 | **Agency Crisis** world event begins |

The optimal Synara player does **not** maximize automation. They optimize the relationship between human and machine labor.

---

# 26. PROTOTYPE WIN CONDITION

The vertical slice contains:

- Player faction: Synara
- Rival faction: Forgeweave
- Neutral/support faction: Eden Circuit
- One 32×32 player city
- One 32×32 rival city
- One explorable frontier zone between them

The player wins the slice by completing any one conquest route:

### Force
Destroy Forgeweave’s Command Foundry and defeat Daxton Rhe.

### Economic
Control 60% of the region’s industrial trade and force Forgeweave into dependency.

### Influence
Raise pro-Synara sentiment inside Forgeweave above 70%.

### Alliance
Solve Forgeweave’s Resource Hunger crisis and negotiate voluntary union.

All routes culminate in the same major **Ascension** sequence but produce different Loyalty, Leader, and district outcomes.

---

# 27. FIRST FUSION TECHNOLOGY

Conquering Forgeweave while playing Synara unlocks:

# AUTONOMOUS FACTORY

**Fusion:** Synara + Forgeweave

Install into one Industrial facility.

Effect:

- Removes 80% of worker requirement
- +25% throughput
- +15% construction speed to adjacent industrial assets
- Adds +2 Dependency every 3 cycles
- Adds +1 Resource Hunger pressure per cycle

This is deliberately powerful and dangerous.

It demonstrates the core endgame design principle:

> **Fusion technologies combine strengths and also combine consequences.**

---

# 28. FIRST VERTICAL-SLICE CARD TARGET

Do not model 300 cards immediately.

Prototype in this order:

### Milestone A — 18 cards
Six Synara, six Forgeweave, six Universal.

Prove:
- drawing
- cost
- placement
- construction
- resource output
- adjacency
- destruction/recovery

### Milestone B — 36 cards
Add:
- utilities
- civic metrics
- units
- upgrades
- faction pressure

### Milestone C — 50 cards
Add Eden Circuit and first cross-faction interactions.

### Milestone D — 60-card Synara deck
Validate a full starter deck and conquest route.

Only after this loop is enjoyable should production expand to all 300 Core Identity cards.

---

# 29. BALANCE TARGETS FOR FIRST PLAYTESTS

A first city should feel constrained but never starved.

Target pacing:

- First meaningful building: immediate
- First positive adjacency: within 2 cycles
- First Tier II upgrade: within 5–7 cycles
- First unit deployment: within 6–8 cycles
- First systemic-problem warning: within 8–12 cycles
- First district specialization: within 10–15 cycles
- First rival confrontation: within 15–20 cycles
- First meaningful conquest decision: within 25–40 cycles

The player should always have at least two viable strategic decisions available.

---

# 30. PROTOTYPE SUCCESS CRITERIA

The vertical slice is successful only if playtesters can answer “yes” to all of these:

1. Does drawing a card create excitement because it changes the physical city?
2. Does placement location meaningfully matter?
3. Do the three currencies create different decisions rather than acting as three colors of money?
4. Does the faction’s systemic problem force adaptation?
5. Is switching between City, Founder, and Command modes additive rather than distracting?
6. Does conquering Forgeweave materially change how the Synara city plays?
7. Does the Ascension reward feel powerful enough to motivate another conquest?
8. Does Dominion Pressure prevent conquest from becoming a trivial snowball?
9. Can two players using the same faction produce visibly and mechanically different cities?
10. Does the city itself tell the story of the player’s deck and conquest history?

If those ten conditions hold, the game has a defensible core loop.

---

# 31. NEXT SYSTEMS TO SPECIFY

The next production-facing design pass should define:

1. **Combat model**
   - Founder combat
   - Units
   - cover
   - damage
   - siege
   - destructible buildings
   - Leader boss encounters

2. **Conquest simulation**
   - Force
   - Influence
   - Economic
   - Alliance progress meters

3. **Citizen simulation**
   - jobs
   - class progression
   - factions
   - loyalty
   - migration
   - systemic-problem reactions

4. **Card acquisition**
   - earning packs without exploitative monetization
   - crafting
   - discovery
   - conquest rewards
   - rarity protection
   - duplicate handling

5. **World campaign layer**
   - 20 rival cities
   - travel
   - diplomacy
   - alliances
   - Dominion Pressure
   - procedural events

6. **3D production rules**
   - modular architecture kits
   - card-to-prefab schema
   - LODs
   - interiors
   - destruction states
   - construction animations

7. **Save-state architecture**
   - city state
   - deck ownership
   - placed cards
   - citizens
   - conquest state
   - leader state
   - world simulation

---

# DESIGN MANTRA

> **Your deck is not a menu of bonuses. It is a box of real things waiting to exist.**

**Draw it. Place it. Build it. Live in it. Defend it. Lose it. Rebuild it. Conquer with it.**
