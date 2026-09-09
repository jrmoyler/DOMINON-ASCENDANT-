import { useEffect } from 'react'
import { definition } from '@/game/content'
import type { CardDefinition, GameState } from '@/game/types'
import { settleCard } from './motion'

interface Props {
  state: GameState
  onSelect: (instanceId: string | null) => void
  onCycleCard: (instanceId: string) => void
  onRotate: () => void
  rotation: 0 | 1 | 2 | 3
}
function benefit(def: CardDefinition): string {
  if (def.powerProduced > 0) return `+${Math.round(def.powerProduced)} Power · 6-cell reach`
  if (def.waterProduced > 0) return `+${Math.round(def.waterProduced)} Water · 6-cell reach`
  if (def.dataProduced > 0) return `+${Math.round(def.dataProduced)} Data · 6-cell reach`
  if (def.housingCapacity > 0) return `${def.housingCapacity} new homes`
  if (def.baseInsightPerCycle >= .2) return `+${def.baseInsightPerCycle.toFixed(2)} Insight / cycle`
  if (def.baseCapitalPerCycle > 0) return `+${def.baseCapitalPerCycle.toFixed(1)} Capital / cycle`
  if (def.happiness > 0) return `+${def.happiness} local approval`
  return `${def.jobCapacity} jobs · ${def.constructionCycles} cycles to build`
}
export function CardHand({ state, onSelect, onCycleCard, onRotate, rotation }: Props) {
  useEffect(() => { const animation = settleCard(); return () => { animation?.revert() } }, [state.selectedInstanceId])
  const selected = state.selectedInstanceId ? definition(state.instances[state.selectedInstanceId].definitionId) : null
  return <section className="hand-bar" aria-label="Deployment hand">
    {selected && <div className="card-brief"><div><strong>{selected.displayName}</strong><span>{selected.description}</span></div><button onClick={() => onSelect(null)} aria-label="Cancel card placement">Cancel</button></div>}
    <div className="hand-meta"><div className="deck-counts"><strong>DEPLOYMENT HAND</strong><span>{state.draw.length} draw / {state.discard.length} discard</span></div><button className="rotate-btn" onClick={onRotate} title="Rotate footprint (R)" disabled={!selected}>Rotate {rotation * 90}° <span aria-hidden="true">↻</span></button></div>
    <div className="hand-cards">{state.hand.map((instanceId, index) => {
      const def = definition(state.instances[instanceId].definitionId)
      const isSelected = state.selectedInstanceId === instanceId
      const deficits = [state.resources.capital < def.deploymentCapital ? `${Math.ceil(def.deploymentCapital - state.resources.capital)} Capital` : '', state.resources.insight < def.deploymentInsight ? `${Math.ceil(def.deploymentInsight - state.resources.insight)} Insight` : '', state.resources.influence < def.deploymentInfluence ? `${Math.ceil(def.deploymentInfluence - state.resources.influence)} Influence` : ''].filter(Boolean)
      const affordable = deficits.length === 0
      return <article key={instanceId} className={`card ${isSelected ? 'selected' : ''} ${affordable ? '' : 'unaffordable'}`} data-type={def.cardType}>
        <button className="card-select" onClick={() => onSelect(isSelected ? null : instanceId)} aria-pressed={isSelected} aria-label={`${def.displayName}. ${benefit(def)}. Cost ${def.deploymentCapital} Capital${def.deploymentInsight ? `, ${def.deploymentInsight} Insight` : ''}${def.deploymentInfluence ? `, ${def.deploymentInfluence} Influence` : ''}.${affordable ? '' : ` Need ${deficits.join(' and ')} more.`}`}>
          <span className="card-head"><span className="card-type">{def.cardType}</span><span className="card-index">0{index + 1}</span></span>
          <span className="card-name">{def.displayName}</span>
          <span className="card-benefit">{benefit(def)}</span>
          <span className="card-spec"><span>{def.footprint[0]} × {def.footprint[1]} cells</span><span>{def.constructionCycles} cycles</span></span>
        </button>
        <div className="card-foot"><span className={`card-cost ${affordable ? '' : 'bad'}`} title={affordable ? 'Deployment cost' : `Need ${deficits.join(' and ')} more`}><b>{def.deploymentCapital}</b> CAP{def.deploymentInsight > 0 && <small>{def.deploymentInsight} INS</small>}{def.deploymentInfluence > 0 && <small>{def.deploymentInfluence} INF</small>}</span><button className="card-discard" disabled={state.draw.length + state.discard.length === 0} title="Replace this card from your deck" aria-label={`Replace ${def.displayName}`} onClick={() => onCycleCard(instanceId)}>↻</button></div>
        {!affordable && <span className="card-shortfall">Need {deficits.join(' + ')}</span>}
      </article>
    })}{state.hand.length === 0 && <div className="hand-empty"><strong>Your hand is deployed.</strong><span>Inspect a building to manage your city. Decommission an asset to return its card to the discard pile.</span></div>}</div>
  </section>
}
