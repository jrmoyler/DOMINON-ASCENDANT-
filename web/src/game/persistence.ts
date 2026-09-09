import { DEFINITIONS, FOUNDER_HALL_ID, STARTER_DECK } from './content'
import { CYCLES_PER_WORLD_TICK, computeTotals } from './economy'
import { BUILD_LIMIT, cellsOf, inBounds, rotatedFootprint } from './grid'
import { ascensionProgress, initialQuests } from './quests'
import type { GameState, WorldAsset } from './types'

const record = (value: unknown): value is Record<string, unknown> =>
  value !== null && typeof value === 'object' && !Array.isArray(value)
const nonnegative = (value: unknown): value is number =>
  typeof value === 'number' && Number.isFinite(value) && value >= 0
const whole = (value: unknown): value is number => nonnegative(value) && Number.isSafeInteger(value)

/**
 * Validate the entire ownership graph before a save enters the simulation.
 * UI selections and derived values are rebuilt; authored quest wording is never
 * loaded from an old save. Invalid checkpoints can safely fall back to backup.
 */
export function restoreCampaign(value: unknown, version: number): GameState | null {
  if (!record(value) || value.version !== version) return null
  const state = value as unknown as GameState
  if (!whole(state.cycle) || !whole(state.population) || !whole(state.placedCount)) return null
  if (!nonnegative(state.cycleProgress) || state.cycleProgress >= 1) return null
  if (!record(state.resources) || ![state.resources.capital, state.resources.insight,
    state.resources.influence].every(nonnegative)) return null

  if (!Array.isArray(state.assets) || state.assets.length < 1 || state.assets.length > BUILD_LIMIT) return null
  const occupied = new Set<number>()
  const assets = new Map<string, WorldAsset>()
  let halls = 0
  for (const asset of state.assets) {
    if (!record(asset) || typeof asset.id !== 'string' || assets.has(asset.id)) return null
    const def = DEFINITIONS[asset.definitionId]
    if (!def?.placeable || !Array.isArray(asset.footprint) || asset.footprint.length !== 2) return null
    if (asset.footprint.some((size, index) => size !== def.footprint[index])) return null
    if (![0, 1, 2, 3].includes(asset.rotation)) return null
    const [width, depth] = rotatedFootprint(asset.footprint, asset.rotation)
    if (!inBounds(asset.x, asset.y, width, depth)) return null
    if (!whole(asset.cyclesRemaining) || asset.cyclesRemaining > def.constructionCycles) return null
    if (typeof asset.operational !== 'boolean' || asset.operational !== (asset.cyclesRemaining === 0)) return null
    if (!whole(asset.staffed) || typeof asset.brownout !== 'boolean') return null
    for (const cell of cellsOf(asset)) {
      if (occupied.has(cell)) return null
      occupied.add(cell)
    }
    if (asset.definitionId === FOUNDER_HALL_ID) {
      halls++
      if (!asset.operational) return null
    }
    assets.set(asset.id, asset)
  }
  if (halls !== 1) return null

  if (!record(state.instances)) return null
  const expected = new Map(STARTER_DECK.map((entry) => [entry.definitionId, entry.quantity]))
  const counts = new Map<string, number>()
  const ownership = new Set<string>()
  const deployed = new Set<string>()
  for (const [id, instance] of Object.entries(state.instances)) {
    if (!record(instance) || instance.id !== id || !expected.has(instance.definitionId)) return null
    counts.set(instance.definitionId, (counts.get(instance.definitionId) ?? 0) + 1)
    if (instance.worldAssetId !== null) {
      if (typeof instance.worldAssetId !== 'string') return null
      const asset = assets.get(instance.worldAssetId)
      if (!asset || asset.definitionId !== instance.definitionId || deployed.has(asset.id)) return null
      deployed.add(asset.id)
      ownership.add(id)
    }
  }
  if ([...expected].some(([id, quantity]) => counts.get(id) !== quantity)) return null
  if (state.assets.some((asset) => asset.definitionId !== FOUNDER_HALL_ID && !deployed.has(asset.id))) return null
  for (const zone of [state.hand, state.draw, state.discard]) {
    if (!Array.isArray(zone)) return null
    for (const id of zone) {
      if (typeof id !== 'string' || !Object.prototype.hasOwnProperty.call(state.instances, id) || ownership.has(id)) return null
      ownership.add(id)
    }
  }
  if (state.hand.length > 6 || ownership.size !== Object.keys(state.instances).length) return null

  const canonicalQuests = initialQuests()
  if (!Array.isArray(state.quests) || state.quests.length !== canonicalQuests.length) return null
  let incomplete = false
  for (let index = 0; index < canonicalQuests.length; index++) {
    const saved = state.quests[index]
    const canonical = canonicalQuests[index]
    if (!record(saved) || saved.id !== canonical.id || !Array.isArray(saved.objectives)) return null
    if (saved.objectives.length !== canonical.objectives.length) return null
    if (incomplete ? saved.status !== 'locked' : !['complete', 'active'].includes(saved.status)) return null
    if (saved.status !== 'complete') incomplete = true
    for (let objectiveIndex = 0; objectiveIndex < canonical.objectives.length; objectiveIndex++) {
      const objective = saved.objectives[objectiveIndex]
      const authored = canonical.objectives[objectiveIndex]
      if (!record(objective) || objective.id !== authored.id || !nonnegative(objective.progress)) return null
      if (objective.progress > authored.target || objective.done !== (objective.progress >= authored.target)) return null
      if (saved.status === 'complete' && !objective.done) return null
      if (saved.status === 'locked' && objective.progress !== 0) return null
      authored.progress = objective.progress
      authored.done = objective.done
    }
    canonical.status = saved.status
  }
  if (!Array.isArray(state.log) || state.log.length > 120) return null
  for (const entry of state.log) {
    if (!record(entry) || !whole(entry.cycle) || entry.cycle > state.cycle || typeof entry.text !== 'string') return null
    if (!['info', 'good', 'warn', 'quest'].includes(entry.kind)) return null
  }

  state.quests = canonicalQuests
  state.totals = computeTotals(state.assets, state.population)
  state.worldTick = Math.floor(state.cycle / CYCLES_PER_WORLD_TICK)
  state.ascensionProgress = ascensionProgress(state)
  state.ascended = state.ascensionProgress >= 1
  state.speed = 0
  state.selectedAssetId = null
  state.selectedInstanceId = null
  if (!['none', 'power', 'water', 'data', 'employment', 'housing', 'happiness'].includes(state.overlay)) {
    state.overlay = 'none'
  }
  return state
}
