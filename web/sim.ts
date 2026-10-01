/**
 * Headless balance harness.
 *
 * Plays the campaign with a simple greedy strategy and reports whether the
 * first-hour quest chain is actually completable. Run: npx tsx sim.ts
 */
import { definition } from './src/game/content'
import { GRID_SIZE, canPlace } from './src/game/grid'
import { advanceCycle, chooseDraft, garrison, marketDraft, createInitialState, cycleCard, decideEvent, influencePressure, placeCard, playTacticCard } from './src/game/state'
import { EVENT_BY_ID, TACTICS, canAfford, choiceCost, eventChoices } from './src/game/campaign'
import type { CardType, Difficulty, GameState } from './src/game/types'

/** What the city needs most right now, in priority order. */
function wanted(state: GameState): CardType[] {
  const t = state.totals
  const needs: CardType[] = []
  if (t.powerSupply < t.powerDemand * 1.15) needs.push('Infrastructure')
  if (t.waterSupply < t.waterDemand * 1.15) needs.push('Infrastructure')
  if (t.housingCapacity < state.population + 8) needs.push('Residential')
  const jobsFull = t.jobCapacity > state.population * 1.4 + 10
  if (t.jobCapacity < state.population + 6) needs.push('Retail', 'Office', 'Industrial')
  if (t.happiness < 55) needs.push('Civic')
  if (state.act === 3 && t.defense < 40) needs.push('Defense')
  if (state.path === 'force') needs.push('Defense')
  if (state.path === 'economic') needs.push('Industrial')
  if (state.path === 'alliance') needs.push('Wonder')
  needs.push('Residential')
  if (!jobsFull) needs.push('Retail', 'Office', 'Industrial', 'Research')
  needs.push('Civic', 'Infrastructure')
  return needs
}

/** First free anchor that fits, searched outward from the grid centre. */
function findSpot(state: GameState, fp: [number, number]): { x: number; y: number } | null {
  const c = Math.floor(GRID_SIZE / 2)
  for (let ring = 1; ring < 14; ring++) {
    for (let dx = -ring; dx <= ring; dx++) {
      for (let dy = -ring; dy <= ring; dy++) {
        if (Math.max(Math.abs(dx), Math.abs(dy)) !== ring) continue
        const x = c + dx
        const y = c + dy
        if (canPlace(state.assets, x, y, fp, 0).ok) return { x, y }
      }
    }
  }
  return null
}

function decide(state: GameState) {
  for (let guard = 0; guard < 6 && (state.pendingEvent || state.draft); guard++) {
    if (state.pendingEvent) {
      const event = EVENT_BY_ID[state.pendingEvent]
      const choices = eventChoices(state, event)
      let index = event.id === 'story.forge_lord' ? PATH_INDEX : choices.findIndex((c) => canAfford(state, choiceCost(c)))
      if (index < 0) index = choices.length - 1
      if (!decideEvent(state, index).ok) decideEvent(state, choices.length - 1)
      if (state.pendingEvent === event.id) state.pendingEvent = null
    } else if (state.draft) {
      chooseDraft(state, state.draft.options.find((id) => definition(id).placeable || TACTICS[id]) ?? null)
    }
  }
}

function step(state: GameState) {
  decide(state)
  for (const id of [...state.hand]) {
    const tactic = TACTICS[state.instances[id].definitionId]
    const savingInfluence = state.path === 'influence' && (tactic?.cost.influence ?? 0) > 0
    if (tactic && !savingInfluence && canAfford(state, tactic.cost)) playTacticCard(state, id)
  }
  if (state.threat > 60 || (state.threat > 40 && state.path !== 'influence')) influencePressure(state)
  if (state.act === 3 && state.raidTimer <= 2 && state.totals.defense < 60) garrison(state)
  if (state.resources.capital > 250) marketDraft(state)
  const priorities = wanted(state)
  for (const type of priorities) {
    for (const instanceId of [...state.hand]) {
      const def = definition(state.instances[instanceId].definitionId)
      if (def.cardType !== type) continue
      if (!def.placeable) continue
      if (state.resources.capital < def.deploymentCapital) continue
      if (state.resources.insight < def.deploymentInsight) continue
      const spot = findSpot(state, def.footprint)
      if (!spot) continue
      const result = placeCard(state, instanceId, spot.x, spot.y, 0)
      if (result.ok) return true
    }
  }
  // Nothing useful and affordable — cycle the most expensive dead card.
  if (state.resources.capital > 30 && state.hand.length > 0) {
    const worst = [...state.hand].filter((id) => definition(state.instances[id].definitionId).cardType !== 'Wonder').sort(
      (a, b) =>
        definition(state.instances[b].definitionId).deploymentCapital -
        definition(state.instances[a].definitionId).deploymentCapital,
    )[0]
    if (worst) cycleCard(state, worst)
  }
  return false
}

const DIFF = (process.argv[2] ?? 'governor') as Difficulty
const PATH_INDEX = Number(process.argv[3] ?? 0)
const RUNS = Number(process.argv[4] ?? 1)
const MAX_CYCLES = 500
const summary: string[] = []

for (let run = 0; run < RUNS; run++) {
  const state = createInitialState(DIFF)
  const milestones: string[] = []
  let lastComplete = 0
  for (let i = 0; i < MAX_CYCLES && !state.outcome; i++) {
    step(state)
    advanceCycle(state)
    decide(state)
    const done = state.quests.filter((q) => q.status === 'complete').length
    if (done > lastComplete) {
      for (let q = lastComplete; q < done; q++) milestones.push(`cycle ${state.cycle}: ${state.quests[q].title}`)
      lastComplete = done
    }
    if (RUNS === 1 && (i % 25 === 0)) {
      const t = state.totals
      console.log(
        `c${String(state.cycle).padStart(3)} act${state.act} ` +
          `cap ${state.resources.capital.toFixed(0).padStart(4)} ` +
          `ins ${state.resources.insight.toFixed(0).padStart(3)} ` +
          `inf ${state.resources.influence.toFixed(0).padStart(3)} ` +
          `pop ${String(state.population).padStart(3)}/${String(t.housingCapacity).padStart(3)} ` +
          `job ${String(t.employed).padStart(3)}/${String(t.jobCapacity).padStart(3)} ` +
          `hap ${String(t.happiness).padStart(3)} def ${t.defense} dom ${state.threat.toFixed(0)} ` +
          `assets ${state.assets.length} deck ${Object.keys(state.instances).length} quests ${done}`,
      )
    }
  }
  if (RUNS === 1) {
    console.log('\nMilestones:')
    for (const m of milestones) console.log('  ' + m)
    for (const q of state.quests) {
      if (q.status !== 'active') continue
      console.log(`  [${q.status}] ${q.title}`)
      for (const o of q.objectives) console.log(`      ${o.done ? '✓' : ' '} ${o.label} ${o.progress}/${o.target}`)
    }
  }
  summary.push(`${state.outcome ? state.outcome.kind + ': ' + state.outcome.title : 'unfinished'} @ cycle ${state.cycle} · quests ${lastComplete}/${state.quests.length} · act1 done ${milestones[8]?.split(':')[0] ?? '-'} · raids ${state.stats.raidsRepelled}/${state.stats.raidsSuffered}`)
}
console.log(summary.join('\n'))
