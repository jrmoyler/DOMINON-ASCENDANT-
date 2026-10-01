import { DIFFICULTY } from './economy'
import type { GameState } from './types'

export interface Tip {
  tone: 'urgent' | 'warn' | 'info'
  text: string
}

/** Tal Arden's running advice: the single most important thing to fix right now. */
export function advisorTip(state: GameState, capitalDelta: number): Tip {
  const t = state.totals
  const limit = DIFFICULTY[state.difficulty].strikes
  if (state.act === 3 && state.threat >= 75) {
    return { tone: 'urgent', text: `Dominance is at ${Math.round(state.threat)}. At 100 Ashcroft falls. Spend Influence on pressure, hire a garrison, or play defensive tactics.` }
  }
  if (state.strikes.insolvency >= 2) {
    return { tone: 'urgent', text: `Insolvency ${state.strikes.insolvency}/${limit}. Build Retail or Industry, or decommission idle buildings to cut maintenance.` }
  }
  if (state.strikes.unrest >= 2) {
    return { tone: 'urgent', text: `Unrest ${state.strikes.unrest}/${limit}. Approval is collapsing — build Civic assets or play an approval tactic.` }
  }
  if (Object.keys(state.sabotaged).length) {
    return { tone: 'warn', text: 'Sabotaged buildings are offline. Crews restore them in a few cycles — an Audit Sentinel restores them at once.' }
  }
  if (t.powerDemand > t.powerSupply) return { tone: 'warn', text: 'Power demand exceeds supply. Build a Microgrid Station or Freight Furnace.' }
  if (t.waterDemand > t.waterSupply) return { tone: 'warn', text: 'Water demand exceeds supply. Build a Water Reclaimer or Living Waterway.' }
  if (state.population > t.housingCapacity) return { tone: 'warn', text: 'Citizens are sleeping rough and leaving. Build Residential within utility reach.' }
  if (capitalDelta < -0.5 && state.resources.capital < 30) {
    return { tone: 'warn', text: 'The treasury is shrinking. Commerce pays for everything — prioritise Retail and Industry.' }
  }
  if (t.jobCapacity < state.population * 0.8) return { tone: 'info', text: 'Too many citizens without work. Retail, Office and Industry add jobs and income.' }
  if (t.housingCapacity - state.population <= 2) return { tone: 'info', text: 'Housing is nearly full. New homes will draw more citizens — and more workers.' }
  if (t.happiness < 50) return { tone: 'info', text: 'Approval is low. Civic buildings, transit and green corridors lift it.' }
  if (t.dataDemand > t.dataSupply) return { tone: 'info', text: 'Offices and research need Data. A Neural Relay or Orchestration Hub will restore full output.' }
  if (state.act === 3 && state.raidTimer <= 2) {
    return { tone: 'warn', text: `An Ironheart raid is ${state.raidTimer <= 1 ? 'imminent' : 'two cycles out'}. Check your Defense against its strength.` }
  }
  if (state.act === 1 && state.cycle < 3) return { tone: 'info', text: 'Pick a card from your hand, then click open ground near the Founder Hall to build it.' }
  if (state.resources.capital > 120 && !state.draft) return { tone: 'info', text: 'The treasury is healthy. Visit the Basin Market to draft a new card for your deck.' }
  return { tone: 'info', text: 'The city holds. Follow your objective and keep supply ahead of demand.' }
}
