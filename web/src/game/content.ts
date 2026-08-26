import manifest from '@content/DA/Manifests/VerticalSliceContent.json'
import type {
  CardDefinition,
  CardType,
  Faction,
  Rarity,
  RawCardDefinition,
} from './types'

/**
 * Balance derivation for the browser slice.
 *
 * The content manifests authoritatively author only a handful of numbers per
 * card (`authoredValues`); the Vertical Slice Production Spec states that
 * "absent per-card values remain explicitly unauthored". A playable game needs
 * a number for every field, so the table below fills the gaps by card type.
 *
 * Authored values always win. Anything sourced here is web-slice tuning and is
 * surfaced in the UI as "derived" so it is never mistaken for balance canon.
 */
interface TypeProfile {
  deploymentCapital: number
  constructionCycles: number
  maintenance: number
  capital: number
  insight: number
  influence: number
  housing: number
  jobs: number
  power: number
  water: number
  data: number
  powerProduced: number
  waterProduced: number
  dataProduced: number
  happiness: number
  description: string
}

const TYPE_PROFILES: Record<CardType, TypeProfile> = {
  Residential: {
    deploymentCapital: 8, constructionCycles: 2, maintenance: 0.04,
    capital: 0.35, insight: 0, influence: 0,
    housing: 8, jobs: 0, power: 1.6, water: 1.6, data: 1,
    powerProduced: 0, waterProduced: 0, dataProduced: 0, happiness: 1,
    description: 'Housing. Raises the population ceiling and draws migrants when utilities and jobs hold.',
  },
  Retail: {
    deploymentCapital: 12, constructionCycles: 2, maintenance: 0.08,
    capital: 1.9, insight: 0, influence: 0.02,
    housing: 0, jobs: 5, power: 2.2, water: 1.1, data: 1.6,
    powerProduced: 0, waterProduced: 0, dataProduced: 0, happiness: 2,
    description: 'Commerce. The primary Capital engine — needs staff to earn.',
  },
  Office: {
    deploymentCapital: 16, constructionCycles: 3, maintenance: 0.10,
    capital: 0.8, insight: 0.30, influence: 0.02,
    housing: 0, jobs: 8, power: 2.6, water: 1.1, data: 4,
    powerProduced: 0, waterProduced: 0, dataProduced: 0, happiness: 0,
    description: 'Administration and research. Generates Insight and heavy Data demand.',
  },
  Industrial: {
    deploymentCapital: 18, constructionCycles: 3, maintenance: 0.12,
    capital: 2.4, insight: 0, influence: 0,
    housing: 0, jobs: 10, power: 5, water: 3, data: 1.6,
    powerProduced: 0, waterProduced: 0, dataProduced: 0, happiness: -2,
    description: 'Production. Strong Capital yield, high utility load, unpopular with neighbours.',
  },
  Infrastructure: {
    deploymentCapital: 14, constructionCycles: 2, maintenance: 0.15,
    capital: 0, insight: 0, influence: 0,
    housing: 0, jobs: 3, power: 1, water: 0.6, data: 0.6,
    powerProduced: 0, waterProduced: 0, dataProduced: 0, happiness: 0,
    description: 'Support infrastructure. See the per-card role below.',
  },
  Civic: {
    deploymentCapital: 10, constructionCycles: 2, maintenance: 0.10,
    capital: 0, insight: 0.05, influence: 0.10,
    housing: 0, jobs: 4, power: 1.6, water: 1.6, data: 1,
    powerProduced: 0, waterProduced: 0, dataProduced: 0, happiness: 4,
    description: 'Civic amenity. Buys happiness and Influence rather than Capital.',
  },
  Research: {
    deploymentCapital: 22, constructionCycles: 4, maintenance: 0.14,
    capital: 0, insight: 0.75, influence: 0.05,
    housing: 0, jobs: 6, power: 3.6, water: 1.6, data: 6,
    powerProduced: 0, waterProduced: 0, dataProduced: 0, happiness: 0,
    description: 'Dedicated research. The fastest route to Insight and Ascension.',
  },
  Defense: {
    deploymentCapital: 20, constructionCycles: 3, maintenance: 0.18,
    capital: 0, insight: 0, influence: 0.15,
    housing: 0, jobs: 6, power: 3, water: 1.1, data: 2,
    powerProduced: 0, waterProduced: 0, dataProduced: 0, happiness: 1,
    description: 'Defensive works. Projects Influence across the region.',
  },
  Wonder: {
    deploymentCapital: 60, constructionCycles: 6, maintenance: 0.30,
    capital: 1.0, insight: 0.5, influence: 0.6,
    housing: 0, jobs: 12, power: 6, water: 3, data: 5,
    powerProduced: 0, waterProduced: 0, dataProduced: 0, happiness: 8,
    description: 'Wonder. Expensive, slow, and transformative once it lights up.',
  },
  Unit: {
    deploymentCapital: 14, constructionCycles: 1, maintenance: 0.20,
    capital: 0, insight: 0, influence: 0.20,
    housing: 0, jobs: 0, power: 2, water: 1, data: 2,
    powerProduced: 0, waterProduced: 0, dataProduced: 0, happiness: 0,
    description: 'Deployable squad. Holds ground and asserts regional authority.',
  },
  Leader: {
    deploymentCapital: 0, constructionCycles: 1, maintenance: 0.10,
    capital: 0, insight: 0.2, influence: 0.4,
    housing: 0, jobs: 0, power: 1, water: 1, data: 2,
    powerProduced: 0, waterProduced: 0, dataProduced: 0, happiness: 3,
    description: 'Named leader. Anchors doctrine and diplomacy.',
  },
  Special: {
    deploymentCapital: 0, constructionCycles: 0, maintenance: 0,
    capital: 1, insight: 0.15, influence: 0.1,
    // Doubled against the 1x1 footprint scale so the Hall alone can carry the
    // opening 24 citizens: 14 jobs, and enough utility headroom to build on.
    housing: 24, jobs: 28, power: 0, water: 0, data: 0,
    powerProduced: 90, waterProduced: 80, dataProduced: 44, happiness: 8,
    description: 'The Founder Hall. Seat of governance, and the city’s first source of everything.',
  },
}

/**
 * Per-card role overrides.
 *
 * The type profile alone cannot tell a Microgrid Station from a Warehouse -
 * both are Infrastructure. These entries give each support card a distinct job
 * so utility placement is a real layout decision. Web-slice tuning, not canon.
 */
const CARD_OVERRIDES: Record<string, Partial<TypeProfile>> = {
  // Power
  'universal.microgrid_station': { powerProduced: 240, description: 'Power plant. Supplies Power to every asset within six cells.' },
  'forgeweave.freight_furnace': { powerProduced: 320, happiness: -3, description: 'Heavy power generation. Filthy, but it lights a whole district.' },
  // Water
  'universal.water_reclaimer': { waterProduced: 240, description: 'Water reclamation. Supplies Water within six cells.' },
  'eden.living_waterway': { waterProduced: 260, happiness: 3, description: 'Ecological watercourse. Supplies Water and lifts approval.' },
  // Data
  'synara.neural_relay': { dataProduced: 180, description: 'Data relay. Supplies the Data that offices and research need.' },
  'synara.orchestration_hub': { dataProduced: 240, jobs: 6, description: 'Orchestration spine. Wide-area Data supply and coordination jobs.' },
  // Logistics and remediation
  'universal.transit_stop': { jobs: 4, happiness: 5, capital: 0.4, description: 'Transit. Moves people, lifts approval across the district.' },
  'universal.warehouse': { jobs: 6, capital: 0.8, description: 'Storage and distribution. Steady Capital, no utility output.' },
  'forgeweave.smog_reclaimer': { happiness: 6, jobs: 4, description: 'Scrubs industrial output. Buys back the approval that industry costs.' },
  'eden.pollinator_corridor': { happiness: 7, jobs: 2, description: 'Ecological corridor. Pure approval, very cheap to run.' },
}

/** Larger footprints cost and yield proportionally more. */
function areaScale(footprint: [number, number]): number {
  const area = Math.max(1, footprint[0] * footprint[1])
  return Math.sqrt(area) / Math.sqrt(4)
}

const num = (raw: RawCardDefinition, key: string): number | undefined => {
  const v = raw[key]
  return typeof v === 'number' ? v : undefined
}

function resolve(raw: RawCardDefinition): CardDefinition {
  const cardType = raw.cardType as CardType
  const base = TYPE_PROFILES[cardType] ?? TYPE_PROFILES.Civic
  const profile: TypeProfile = { ...base, ...(CARD_OVERRIDES[raw.id] ?? {}) }
  const fp = raw.footprint ?? [1, 1]
  const footprint: [number, number] = [
    Math.max(1, fp[0] ?? 1),
    Math.max(1, fp[1] ?? 1),
  ]
  const scale = areaScale(footprint)
  const authored = new Set(raw.authoredValues ?? [])

  // Authored value if the manifest specifies it, otherwise the derived default.
  const pick = (key: string, derived: number): number => num(raw, key) ?? derived

  const rarity = (raw.rarity as Rarity) ?? 'Common'
  const rarityMult = rarity === 'Elite' ? 1.6 : rarity === 'Wonder' ? 2.2 : 1

  return {
    id: raw.id,
    displayName: raw.displayName,
    faction: raw.faction as Faction,
    cardType,
    rarity,
    placeable: raw.placeable !== false,
    footprint,
    authored,

    deploymentCapital: Math.round(pick('deploymentCapital', profile.deploymentCapital * scale)),
    deploymentInsight: Math.round(pick('deploymentInsight', 0)),
    deploymentInfluence: Math.round(pick('deploymentInfluence', 0)),

    constructionCycles: Math.max(0, Math.round(pick('constructionCycles', profile.constructionCycles))),

    baseCapitalPerCycle: pick('baseCapitalPerCycle', profile.capital * scale * rarityMult),
    baseInsightPerCycle: pick('baseInsightPerCycle', profile.insight * scale * rarityMult),
    baseInfluencePerCycle: pick('baseInfluencePerCycle', profile.influence * scale * rarityMult),
    maintenanceCapitalPerCycle: pick('maintenanceCapitalPerCycle', profile.maintenance * scale),

    housingCapacity: Math.round(pick('housingCapacity', profile.housing * scale)),
    jobCapacity: Math.round(pick('jobCapacity', profile.jobs * scale)),

    utilityPower: pick('utilityPower', profile.power * scale),
    utilityWater: pick('utilityWater', profile.water * scale),
    utilityData: pick('utilityData', profile.data * scale),

    powerProduced: pick('powerProduced', profile.powerProduced * scale),
    waterProduced: pick('waterProduced', profile.waterProduced * scale),
    dataProduced: pick('dataProduced', profile.dataProduced * scale),

    happiness: pick('happiness', profile.happiness),

    synaraDependencyPerCycle: pick('synaraDependencyPerCycle', 0),
    forgeweaveResourceHungerPerCycle: pick('forgeweaveResourceHungerPerCycle', 0),
    workforceRequirementModifier: pick('workforceRequirementModifier', 0),
    industrialThroughputModifier: pick('industrialThroughputModifier', 0),

    description: profile.description,
  }
}

const rawDefs = manifest.definitions as unknown as RawCardDefinition[]

export const DEFINITIONS: Record<string, CardDefinition> = Object.fromEntries(
  rawDefs.map((raw) => [raw.id, resolve(raw)]),
)

export const ALL_DEFINITIONS: CardDefinition[] = Object.values(DEFINITIONS)

export const STARTER_DECK = manifest.starterDeck as { definitionId: string; quantity: number }[]

export const EXPECTED_COUNTS = manifest.expectedCounts as Record<string, number>

export const FOUNDER_HALL_ID = 'special.founder_hall'

export function definition(id: string): CardDefinition {
  const def = DEFINITIONS[id]
  if (!def) throw new Error(`Unknown card definition: ${id}`)
  return def
}
