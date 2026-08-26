import { FOUNDER_HALL_ID, STARTER_DECK, definition } from './content'
import {
  CYCLES_PER_WORLD_TICK,
  STARTING_POPULATION,
  STARTING_RESOURCES,
  computeTotals,
  resolveCycle,
} from './economy'
import { GRID_SIZE, canPlace } from './grid'
import { ascensionProgress, evaluateQuests, initialQuests } from './quests'
import type { CardInstance, GameState, LogEntry, OverlayId, WorldAsset } from './types'

export const SAVE_VERSION = 1
export const HAND_SIZE = 6

let idCounter = 0
const nextId = (prefix: string) => `${prefix}_${(idCounter++).toString(36)}_${Math.random().toString(36).slice(2, 7)}`

/** Deterministic-enough shuffle for a single-player slice. */
function shuffle<T>(items: T[]): T[] {
  const out = [...items]
  for (let i = out.length - 1; i > 0; i--) {
    const j = Math.floor(Math.random() * (i + 1))
    ;[out[i], out[j]] = [out[j], out[i]]
  }
  return out
}

function log(state: GameState, text: string, kind: LogEntry['kind'] = 'info') {
  state.log.unshift({ cycle: state.cycle, text, kind })
  if (state.log.length > 120) state.log.length = 120
}

/**
 * Build the frozen campaign start: the exact 60-card Synara starter deck
 * (Spec v1.1 §22) plus a Founder Hall placed at the centre of the grid.
 */
export function createInitialState(): GameState {
  const instances: Record<string, CardInstance> = {}
  const deck: string[] = []

  for (const entry of STARTER_DECK) {
    for (let i = 0; i < entry.quantity; i++) {
      const instance: CardInstance = {
        id: nextId('ci'),
        definitionId: entry.definitionId,
        worldAssetId: null,
      }
      instances[instance.id] = instance
      deck.push(instance.id)
    }
  }

  const hallDef = definition(FOUNDER_HALL_ID)
  const centre = Math.floor(GRID_SIZE / 2) - 1
  const hall: WorldAsset = {
    id: nextId('wa'),
    definitionId: FOUNDER_HALL_ID,
    x: centre,
    y: centre,
    footprint: hallDef.footprint,
    rotation: 0,
    cyclesRemaining: 0,
    operational: true,
    brownout: false,
    staffed: 0,
  }

  const draw = shuffle(deck)
  const hand = draw.splice(0, HAND_SIZE)

  const state: GameState = {
    version: SAVE_VERSION,
    cycle: 0,
    worldTick: 0,
    cycleProgress: 0,
    speed: 1,
    resources: { ...STARTING_RESOURCES },
    population: STARTING_POPULATION,
    assets: [hall],
    instances,
    hand,
    draw,
    discard: [],
    quests: initialQuests(),
    log: [],
    totals: computeTotals([hall], STARTING_POPULATION),
    selectedInstanceId: null,
    selectedAssetId: null,
    overlay: 'none',
    ascensionProgress: 0,
    ascended: false,
    placedCount: 0,
  }

  log(state, 'The Founder Hall wakes. Ashcroft Basin is yours to build.', 'quest')
  log(state, 'Objective: bring an Infrastructure asset online.', 'quest')
  return state
}

export function refreshTotals(state: GameState) {
  state.totals = computeTotals(state.assets, state.population)
  const completed = evaluateQuests(state)
  for (const id of completed) {
    const quest = state.quests.find((q) => q.id === id)
    if (quest) log(state, `Quest complete — ${quest.title}`, 'good')
  }
  state.ascensionProgress = ascensionProgress(state)
  if (state.ascensionProgress >= 1 && !state.ascended) {
    state.ascended = true
    log(state, 'FORGEWEAVE ASCENSION — CONVERGENCE AUTHORITY: 1/20', 'good')
  }
}

/** Draw back up to the hand limit from the draw pile, reshuffling discards. */
export function refillHand(state: GameState) {
  while (state.hand.length < HAND_SIZE) {
    if (state.draw.length === 0) {
      if (state.discard.length === 0) break
      state.draw = shuffle(state.discard)
      state.discard = []
      log(state, 'Deck reshuffled.')
    }
    const next = state.draw.shift()
    if (!next) break
    state.hand.push(next)
  }
}

export interface PlaceResult {
  ok: boolean
  reason?: string
}

export function placeCard(
  state: GameState,
  instanceId: string,
  x: number,
  y: number,
  rotation: 0 | 1 | 2 | 3,
): PlaceResult {
  const instance = state.instances[instanceId]
  if (!instance) return { ok: false, reason: 'Unknown card' }
  if (!state.hand.includes(instanceId)) return { ok: false, reason: 'Card is not in hand' }

  const def = definition(instance.definitionId)
  if (!def.placeable) return { ok: false, reason: `${def.displayName} cannot be placed` }

  const check = canPlace(state.assets, x, y, def.footprint, rotation)
  if (!check.ok) return check

  if (state.resources.capital < def.deploymentCapital) {
    return { ok: false, reason: `Needs ${def.deploymentCapital} Capital` }
  }
  if (state.resources.insight < def.deploymentInsight) {
    return { ok: false, reason: `Needs ${def.deploymentInsight} Insight` }
  }
  if (state.resources.influence < def.deploymentInfluence) {
    return { ok: false, reason: `Needs ${def.deploymentInfluence} Influence` }
  }

  state.resources.capital -= def.deploymentCapital
  state.resources.insight -= def.deploymentInsight
  state.resources.influence -= def.deploymentInfluence

  const asset: WorldAsset = {
    id: nextId('wa'),
    definitionId: def.id,
    x,
    y,
    footprint: def.footprint,
    rotation,
    cyclesRemaining: def.constructionCycles,
    operational: def.constructionCycles <= 0,
    brownout: false,
    staffed: 0,
  }

  // Bidirectional Card Instance <-> World Asset link, as in the Unreal slice.
  instance.worldAssetId = asset.id
  state.assets.push(asset)
  state.hand = state.hand.filter((id) => id !== instanceId)
  state.selectedInstanceId = null
  state.placedCount += 1

  log(
    state,
    asset.operational
      ? `${def.displayName} placed and operational.`
      : `${def.displayName} under construction (${def.constructionCycles} cycles).`,
  )

  refillHand(state)
  refreshTotals(state)
  return { ok: true }
}

/** Decommission a placed asset, refunding half its deployment Capital. */
export function demolish(state: GameState, assetId: string): PlaceResult {
  const asset = state.assets.find((a) => a.id === assetId)
  if (!asset) return { ok: false, reason: 'No such asset' }
  if (asset.definitionId === FOUNDER_HALL_ID) {
    return { ok: false, reason: 'The Founder Hall cannot be decommissioned' }
  }
  const def = definition(asset.definitionId)

  state.assets = state.assets.filter((a) => a.id !== assetId)
  state.resources.capital += Math.floor(def.deploymentCapital / 2)

  // Return the card instance to the discard pile.
  const instance = Object.values(state.instances).find((ci) => ci.worldAssetId === assetId)
  if (instance) {
    instance.worldAssetId = null
    state.discard.push(instance.id)
  }

  if (state.selectedAssetId === assetId) state.selectedAssetId = null
  log(state, `${def.displayName} decommissioned.`, 'warn')
  refreshTotals(state)
  return { ok: true }
}

/** Discard one card from hand and immediately draw a replacement. */
export function cycleCard(state: GameState, instanceId: string) {
  if (!state.hand.includes(instanceId)) return
  state.hand = state.hand.filter((id) => id !== instanceId)
  state.discard.push(instanceId)
  if (state.selectedInstanceId === instanceId) state.selectedInstanceId = null
  refillHand(state)
}

/** Advance one full Development Cycle. */
export function advanceCycle(state: GameState) {
  const result = resolveCycle(state)

  state.cycle += 1
  if (state.cycle % CYCLES_PER_WORLD_TICK === 0) state.worldTick += 1

  state.resources.capital = Math.max(0, state.resources.capital + result.capitalDelta)
  state.resources.insight = Math.max(0, state.resources.insight + result.insightDelta)
  state.resources.influence = Math.max(0, state.resources.influence + result.influenceDelta)
  state.population = Math.max(0, state.population + result.populationDelta)

  for (const event of result.events) log(state, event, 'info')

  if (result.capitalDelta < 0 && state.resources.capital <= 0) {
    log(state, 'Treasury empty — maintenance is outrunning income.', 'warn')
  }

  refreshTotals(state)
}

export function setOverlay(state: GameState, overlay: OverlayId) {
  state.overlay = overlay
}

export function serialize(state: GameState): string {
  return JSON.stringify({ ...state, version: SAVE_VERSION })
}

export function deserialize(raw: string): GameState | null {
  try {
    const parsed = JSON.parse(raw) as GameState
    if (parsed.version !== SAVE_VERSION) return null
    if (!Array.isArray(parsed.assets) || !parsed.instances) return null
    // Rehydrate derived state rather than trusting the saved copy.
    parsed.totals = computeTotals(parsed.assets, parsed.population)
    parsed.speed = 0
    return parsed
  } catch {
    return null
  }
}

export const SAVE_KEY = 'dominion-ascendant.save.v1'

export function saveToStorage(state: GameState): boolean {
  try {
    localStorage.setItem(SAVE_KEY, serialize(state))
    return true
  } catch {
    return false
  }
}

export function loadFromStorage(): GameState | null {
  try {
    const raw = localStorage.getItem(SAVE_KEY)
    return raw ? deserialize(raw) : null
  } catch {
    return null
  }
}

export function clearStorage() {
  try {
    localStorage.removeItem(SAVE_KEY)
  } catch {
    /* storage unavailable; nothing to clear */
  }
}
