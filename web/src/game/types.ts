/**
 * Core types for the DOMINION // ASCENDANT browser vertical slice.
 *
 * Naming follows the Unreal slice (Development Cycle, World Tick, Card
 * Instance, World Asset) so the two implementations stay legible to each
 * other. See docs/bibles/DOMINION_ASCENDANT_Vertical_Slice_Production_Spec_v1.1.md
 */

export type Faction = 'Synara' | 'Forgeweave' | 'EdenCircuit' | 'Universal' | 'Fusion' | 'Special'

export type CardType =
  | 'Residential'
  | 'Office'
  | 'Retail'
  | 'Industrial'
  | 'Infrastructure'
  | 'Civic'
  | 'Research'
  | 'Defense'
  | 'Wonder'
  | 'Unit'
  | 'Leader'
  | 'Special'

export type Rarity = 'Common' | 'Elite' | 'Leader' | 'Wonder'

/** Raw shape as authored in Content/DA/Manifests/VerticalSliceContent.json. */
export interface RawCardDefinition {
  id: string
  displayName: string
  faction: string
  cardType: string
  rarity: string
  placeable: boolean
  footprint: number[]
  worldPrefab?: string
  authoredValues?: string[]
  tags?: string[]
  upgradeBranchIds?: string[]
  [key: string]: unknown
}

/**
 * A resolved, playable card definition.
 *
 * `authored` lists the fields the content manifests actually specify. Every
 * other number here comes from the web-slice derivation table in
 * `deriveStats()` and is NOT balance canon.
 */
export interface CardDefinition {
  id: string
  displayName: string
  faction: Faction
  cardType: CardType
  rarity: Rarity
  placeable: boolean
  /** [width, depth] in 8-metre logical grid cells. */
  footprint: [number, number]
  authored: Set<string>

  /** Cost in Capital to place from hand. */
  deploymentCapital: number
  /** Cost in Insight to place from hand (mostly Elite/Fusion). */
  deploymentInsight: number
  /** Cost in Influence to place from hand. */
  deploymentInfluence: number

  /** Development Cycles the site spends under construction. */
  constructionCycles: number

  baseCapitalPerCycle: number
  baseInsightPerCycle: number
  baseInfluencePerCycle: number
  maintenanceCapitalPerCycle: number

  housingCapacity: number
  /** Jobs offered once operational. */
  jobCapacity: number

  utilityPower: number
  utilityWater: number
  utilityData: number

  /** Positive = produces the utility, negative handled via demand fields. */
  powerProduced: number
  waterProduced: number
  dataProduced: number

  happiness: number

  synaraDependencyPerCycle: number
  forgeweaveResourceHungerPerCycle: number
  workforceRequirementModifier: number
  industrialThroughputModifier: number

  description: string
}

/** One physical building standing on the city grid. */
export interface WorldAsset {
  /** Stable id, also the Card Instance link (bidirectional, as in the slice). */
  id: string
  definitionId: string
  /** Anchor cell (min x, min y). */
  x: number
  y: number
  footprint: [number, number]
  rotation: 0 | 1 | 2 | 3
  /** Cycles of construction still remaining; 0 means operational. */
  cyclesRemaining: number
  operational: boolean
  /** Set when utilities cannot meet demand. */
  brownout: boolean
  staffed: number
}

export interface CardInstance {
  id: string
  definitionId: string
  /** Set while this instance is deployed as a World Asset. */
  worldAssetId: string | null
}

export type Zone = 'hand' | 'draw' | 'discard' | 'deployed'

export interface Resources {
  capital: number
  insight: number
  influence: number
}

export interface CityTotals {
  population: number
  housingCapacity: number
  jobCapacity: number
  employed: number
  powerSupply: number
  powerDemand: number
  waterSupply: number
  waterDemand: number
  dataSupply: number
  dataDemand: number
  happiness: number
  dependency: number
  resourceHunger: number
}

export type OverlayId =
  | 'none'
  | 'power'
  | 'water'
  | 'data'
  | 'employment'
  | 'housing'
  | 'happiness'

export interface QuestObjectiveState {
  id: string
  label: string
  done: boolean
  progress: number
  target: number
}

export interface QuestState {
  id: string
  title: string
  status: 'locked' | 'active' | 'complete'
  objectives: QuestObjectiveState[]
}

export interface LogEntry {
  cycle: number
  text: string
  kind: 'info' | 'good' | 'warn' | 'quest'
}

export interface GameState {
  version: number
  cycle: number
  worldTick: number
  /** Fractional progress through the current Development Cycle, 0..1. */
  cycleProgress: number
  speed: 0 | 1 | 2 | 4
  resources: Resources
  population: number
  assets: WorldAsset[]
  instances: Record<string, CardInstance>
  hand: string[]
  draw: string[]
  discard: string[]
  quests: QuestState[]
  log: LogEntry[]
  totals: CityTotals
  selectedInstanceId: string | null
  selectedAssetId: string | null
  overlay: OverlayId
  ascensionProgress: number
  ascended: boolean
  placedCount: number
}
