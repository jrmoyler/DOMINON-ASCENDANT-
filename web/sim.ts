/**
 * Headless balance harness.
 *
 * Plays the campaign with a simple greedy strategy and reports whether the
 * first-hour quest chain is actually completable. Run: npx tsx sim.ts
 */
import { definition } from './src/game/content'
import { GRID_SIZE, canPlace } from './src/game/grid'
import { advanceCycle, createInitialState, cycleCard, placeCard } from './src/game/state'
import type { CardType, GameState } from './src/game/types'

/** What the city needs most right now, in priority order. */
function wanted(state: GameState): CardType[] {
  const t = state.totals
  const needs: CardType[] = []
  if (t.powerSupply < t.powerDemand * 1.15) needs.push('Infrastructure')
  if (t.waterSupply < t.waterDemand * 1.15) needs.push('Infrastructure')
  if (t.housingCapacity < state.population + 8) needs.push('Residential')
  if (t.jobCapacity < state.population + 6) needs.push('Retail', 'Office', 'Industrial')
  if (t.happiness < 55) needs.push('Civic')
  needs.push('Retail', 'Residential', 'Office', 'Industrial', 'Civic', 'Infrastructure', 'Research')
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

function step(state: GameState) {
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
    const worst = [...state.hand].sort(
      (a, b) =>
        definition(state.instances[b].definitionId).deploymentCapital -
        definition(state.instances[a].definitionId).deploymentCapital,
    )[0]
    cycleCard(state, worst)
  }
  return false
}

const state = createInitialState()
const MAX_CYCLES = 400
const milestones: string[] = []
let lastComplete = 0

for (let i = 0; i < MAX_CYCLES; i++) {
  step(state)
  advanceCycle(state)
  const done = state.quests.filter((q) => q.status === 'complete').length
  if (done > lastComplete) {
    lastComplete = done
    milestones.push(`cycle ${state.cycle}: ${state.quests[done - 1].title}`)
  }
  if (i % 50 === 0 || i === MAX_CYCLES - 1) {
    const t = state.totals
    console.log(
      `c${String(state.cycle).padStart(3)} ` +
        `cap ${state.resources.capital.toFixed(0).padStart(4)} ` +
        `ins ${state.resources.insight.toFixed(0).padStart(3)} ` +
        `inf ${state.resources.influence.toFixed(0).padStart(3)} ` +
        `pop ${String(state.population).padStart(3)}/${String(t.housingCapacity).padStart(3)} ` +
        `job ${String(t.employed).padStart(3)}/${String(t.jobCapacity).padStart(3)} ` +
        `hap ${String(t.happiness).padStart(3)} ` +
        `pwr ${t.powerSupply.toFixed(0)}/${t.powerDemand.toFixed(0)} ` +
        `assets ${state.assets.length} ` +
        `quests ${done}/9`,
    )
  }
}

console.log('\nMilestones:')
for (const m of milestones) console.log('  ' + m)
console.log(`\nQuests complete: ${state.quests.filter((q) => q.status === 'complete').length}/9`)
console.log(`Ascended: ${state.ascended}`)
for (const q of state.quests) {
  if (q.status === 'complete') continue
  console.log(`  [${q.status}] ${q.title}`)
  for (const o of q.objectives) console.log(`      ${o.done ? '✓' : ' '} ${o.label} ${o.progress}/${o.target}`)
}
