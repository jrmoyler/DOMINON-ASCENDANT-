import { TACTICS } from '@/game/campaign'
import type { CardDefinition, Resources } from '@/game/types'

/** One-line headline of what a card does for the city. */
export function benefit(def: CardDefinition): string {
  const tactic = TACTICS[def.id]
  if (tactic) return tactic.text
  if (def.powerProduced > 0) return `+${Math.round(def.powerProduced)} Power · 6-cell reach`
  if (def.waterProduced > 0) return `+${Math.round(def.waterProduced)} Water · 6-cell reach`
  if (def.dataProduced > 0) return `+${Math.round(def.dataProduced)} Data · 6-cell reach`
  if (def.housingCapacity > 0) return `${def.housingCapacity} new homes`
  if (def.cardType === 'Defense') return `Defense against raids · ${def.jobCapacity} jobs`
  if (def.cardType === 'Wonder') return `+${def.happiness} approval · all three currencies · defense`
  if (def.baseInsightPerCycle >= .2) return `+${def.baseInsightPerCycle.toFixed(2)} Insight / cycle`
  if (def.baseCapitalPerCycle > 0) return `+${def.baseCapitalPerCycle.toFixed(1)} Capital / cycle · ${def.jobCapacity} jobs`
  if (def.happiness > 0) return `+${def.happiness} local approval`
  return `${def.jobCapacity} jobs · ${def.constructionCycles} cycles to build`
}

/** What it costs to play: a tactic's cost, or a building's deployment cost. */
export function playCost(def: CardDefinition): Resources {
  const tactic = TACTICS[def.id]
  if (tactic) return { capital: tactic.cost.capital ?? 0, insight: tactic.cost.insight ?? 0, influence: tactic.cost.influence ?? 0 }
  return { capital: def.deploymentCapital, insight: def.deploymentInsight, influence: def.deploymentInfluence }
}

export const FACTION_LABEL: Record<string, string> = {
  Synara: 'Synara', Forgeweave: 'Forgeweave', EdenCircuit: 'Eden Circuit', Universal: 'Universal', Fusion: 'Fusion', Special: 'Founder',
}
