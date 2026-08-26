import { definition } from './content'
import { assetDistance } from './grid'
import type { CityTotals, GameState, WorldAsset } from './types'

/**
 * Spec §14 fixes 1 Development Cycle at 30 s of active simulation. That pacing
 * assumes the PC slice, where the player is doing something real-time between
 * cycles. The browser slice is pure city management, so a 30 s cycle reads as
 * dead air: this build runs a 10 s cycle and keeps every other ratio intact.
 */
export const CYCLE_SECONDS = 10
/** Spec §14: 1 World Tick = 5 Development Cycles. */
export const CYCLES_PER_WORLD_TICK = 5

/** Utilities only reach buildings within this many cells of a supplier. */
export const UTILITY_RADIUS = 6

/** Spec §45 Standard difficulty baseline. */
export const STARTING_RESOURCES = { capital: 40, insight: 12, influence: 8 }
export const STARTING_POPULATION = 24

export interface AssetServices {
  power: boolean
  water: boolean
  data: boolean
}

/**
 * Which operational assets can actually reach each consumer.
 *
 * Supply is local: an Infrastructure asset feeds consumers within
 * UTILITY_RADIUS cells. This is what makes layout matter.
 */
export function computeServices(assets: WorldAsset[]): Map<string, AssetServices> {
  const operational = assets.filter((a) => a.operational)
  const powerSources = operational.filter((a) => definition(a.definitionId).powerProduced > 0)
  const waterSources = operational.filter((a) => definition(a.definitionId).waterProduced > 0)
  const dataSources = operational.filter((a) => definition(a.definitionId).dataProduced > 0)

  const services = new Map<string, AssetServices>()
  for (const asset of assets) {
    const near = (sources: WorldAsset[]) =>
      sources.some((s) => s.id === asset.id || assetDistance(asset, s) <= UTILITY_RADIUS)
    services.set(asset.id, {
      power: near(powerSources),
      water: near(waterSources),
      data: near(dataSources),
    })
  }
  return services
}

/** Aggregate city state derived from the current assets and population. */
export function computeTotals(assets: WorldAsset[], population: number): CityTotals {
  const services = computeServices(assets)

  let housingCapacity = 0
  let jobCapacity = 0
  let powerSupply = 0
  let powerDemand = 0
  let waterSupply = 0
  let waterDemand = 0
  let dataSupply = 0
  let dataDemand = 0
  let happiness = 50
  let dependency = 0
  let resourceHunger = 0

  for (const asset of assets) {
    if (!asset.operational) continue
    const def = definition(asset.definitionId)
    const svc = services.get(asset.id)!
    const served = svc.power && svc.water

    powerSupply += def.powerProduced
    waterSupply += def.waterProduced
    dataSupply += def.dataProduced
    powerDemand += def.utilityPower
    waterDemand += def.utilityWater
    dataDemand += def.utilityData

    if (served) {
      housingCapacity += def.housingCapacity
      jobCapacity += Math.round(def.jobCapacity * (1 + def.workforceRequirementModifier))
      happiness += def.happiness
      dependency += def.synaraDependencyPerCycle
      resourceHunger += def.forgeweaveResourceHungerPerCycle
    } else {
      // An unserved building is a blight rather than an asset.
      happiness -= 2
    }
  }

  const employed = Math.min(population, jobCapacity)
  const unemployment = population > 0 ? 1 - employed / population : 0
  // Allow a natural unemployment rate before approval starts to suffer.
  const excessUnemployment = Math.max(0, unemployment - 0.15)
  happiness -= Math.round(excessUnemployment * 55)

  // Urban strain — a sprawling city costs goodwill regardless of amenities.
  happiness -= Math.floor(assets.filter((a) => a.operational).length / 8)

  if (housingCapacity < population) happiness -= Math.round((population - housingCapacity) * 1.5)
  if (powerDemand > powerSupply) happiness -= 12
  if (waterDemand > waterSupply) happiness -= 12
  if (dataDemand > dataSupply) happiness -= 4

  return {
    population,
    housingCapacity,
    jobCapacity,
    employed,
    powerSupply,
    powerDemand,
    waterSupply,
    waterDemand,
    dataSupply,
    dataDemand,
    happiness: Math.max(0, Math.min(100, happiness)),
    dependency,
    resourceHunger,
  }
}

export interface CycleResult {
  capitalDelta: number
  insightDelta: number
  influenceDelta: number
  populationDelta: number
  events: string[]
}

/**
 * Resolve one Development Cycle: construction, production, maintenance and
 * migration, in that order.
 */
export function resolveCycle(state: GameState): CycleResult {
  const events: string[] = []

  // 1. Construction advances first, so a site finished this cycle produces next.
  for (const asset of state.assets) {
    if (asset.operational) continue
    asset.cyclesRemaining -= 1
    if (asset.cyclesRemaining <= 0) {
      asset.cyclesRemaining = 0
      asset.operational = true
      events.push(`${definition(asset.definitionId).displayName} is now operational.`)
    }
  }

  const totals = computeTotals(state.assets, state.population)
  const services = computeServices(state.assets)

  // 2. Production. Output scales with staffing and requires utilities.
  const staffRatio = totals.jobCapacity > 0 ? Math.min(1, totals.employed / totals.jobCapacity) : 0

  let capital = 0
  let insight = 0
  let influence = 0
  let maintenance = 0

  const powerRatio = totals.powerDemand > 0 ? Math.min(1, totals.powerSupply / totals.powerDemand) : 1
  const waterRatio = totals.waterDemand > 0 ? Math.min(1, totals.waterSupply / totals.waterDemand) : 1
  const dataRatio = totals.dataDemand > 0 ? Math.min(1, totals.dataSupply / totals.dataDemand) : 1
  const gridFactor = Math.min(powerRatio, waterRatio)

  for (const asset of state.assets) {
    const def = definition(asset.definitionId)
    if (!asset.operational) {
      maintenance += def.maintenanceCapitalPerCycle * 0.5
      continue
    }
    const svc = services.get(asset.id)!
    maintenance += def.maintenanceCapitalPerCycle

    const served = svc.power && svc.water
    asset.brownout = !served || gridFactor < 0.999
    if (!served) {
      asset.staffed = 0
      continue
    }

    // Jobs are filled from the shared labour pool.
    const jobs = Math.round(def.jobCapacity * (1 + def.workforceRequirementModifier))
    asset.staffed = Math.round(jobs * staffRatio)

    const labour = jobs > 0 ? staffRatio : 1
    const throughput = 1 + def.industrialThroughputModifier

    capital += def.baseCapitalPerCycle * labour * gridFactor * throughput
    insight += def.baseInsightPerCycle * labour * Math.min(gridFactor, svc.data ? dataRatio : 0.25)
    influence += def.baseInfluencePerCycle * labour * gridFactor
  }

  // 3. Population upkeep, administrative overhead, and happiness scaling.
  // Both sinks scale superlinearly so a large city has to keep earning.
  const upkeep = state.population * 0.035 * (1 + state.population / 120)
  const assetCount = state.assets.length
  const overhead = assetCount * 0.05 * (1 + assetCount / 40)
  const happinessFactor = 0.6 + (totals.happiness / 100) * 0.8
  capital *= happinessFactor
  const capitalDelta = capital - maintenance - upkeep - overhead

  // 4. Migration. People arrive when there is room, work and goodwill.
  let populationDelta = 0
  const room = totals.housingCapacity - state.population
  if (room > 0 && totals.happiness >= 45) {
    // Unfilled jobs are the strong pull; housing alone still draws a trickle,
    // so a city can never dead-lock at exactly full employment.
    const jobPull = totals.jobCapacity > totals.employed ? 1 : 0.3
    populationDelta = Math.min(room, Math.max(1, Math.round(room * 0.18 * jobPull)))
  } else if (room < 0) {
    populationDelta = Math.max(room, -Math.max(1, Math.round(-room * 0.25)))
    events.push('Citizens are leaving — not enough housing.')
  } else if (totals.happiness < 30 && state.population > 0) {
    populationDelta = -Math.max(1, Math.round(state.population * 0.05))
    events.push('Unrest is driving citizens out of the city.')
  }

  if (totals.powerDemand > totals.powerSupply) {
    events.push('Power deficit — output is throttled. Build a utility asset.')
  }
  if (totals.waterDemand > totals.waterSupply) {
    events.push('Water deficit — output is throttled.')
  }

  return {
    capitalDelta,
    insightDelta: insight,
    influenceDelta: influence,
    populationDelta,
    events,
  }
}
