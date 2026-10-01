import { useEffect } from 'react'
import { definition } from '@/game/content'
import type { GameState } from '@/game/types'
import { settleCard } from './motion'
import { TACTICS } from '@/game/campaign'
import { benefit, playCost } from './cardInfo'

interface Props {
  state: GameState
  onSelect: (instanceId: string | null) => void
  onCycleCard: (instanceId: string) => void
  onRotate: () => void
  onPlayTactic: (instanceId: string) => void
  rotation: 0 | 1 | 2 | 3
}
export function CardHand({ state, onSelect, onCycleCard, onRotate, onPlayTactic, rotation }: Props) {
  useEffect(() => { const animation = settleCard(); return () => { animation?.revert() } }, [state.selectedInstanceId])
  const selected = state.selectedInstanceId ? definition(state.instances[state.selectedInstanceId].definitionId) : null
  const selectedTactic = selected ? TACTICS[selected.id] : undefined
  const tacticCost = selected ? playCost(selected) : null
  const tacticAffordable = tacticCost ? state.resources.capital >= tacticCost.capital && state.resources.insight >= tacticCost.insight && state.resources.influence >= tacticCost.influence : false
  return <section className="hand-bar" aria-label="Deployment hand">
    {selected && <div className={`card-brief ${selectedTactic ? 'tactic' : ''}`}>
      <div><strong>{selected.displayName}{selectedTactic && <em> · Tactic</em>}</strong><span>{selectedTactic ? selectedTactic.text : selected.description}</span></div>
      {selectedTactic && <button className="primary" disabled={!tacticAffordable} onClick={() => onPlayTactic(state.selectedInstanceId!)}>{selectedTactic.label}</button>}
      <button onClick={() => onSelect(null)} aria-label="Cancel card selection">Cancel</button>
    </div>}
    <div className="hand-meta"><div className="deck-counts"><strong>DEPLOYMENT HAND</strong><span>{state.draw.length} draw / {state.discard.length} discard</span></div><button className="rotate-btn" onClick={onRotate} title="Rotate footprint (R)" disabled={!selected || Boolean(selectedTactic)}>Rotate {rotation * 90}° <span aria-hidden="true">↻</span></button></div>
    <div className="hand-cards">{state.hand.map((instanceId, index) => {
      const def = definition(state.instances[instanceId].definitionId)
      const isSelected = state.selectedInstanceId === instanceId
      const tactic = Boolean(TACTICS[def.id])
      const cost = playCost(def)
      const deficits = [state.resources.capital < cost.capital ? `${Math.ceil(cost.capital - state.resources.capital)} Capital` : '', state.resources.insight < cost.insight ? `${Math.ceil(cost.insight - state.resources.insight)} Insight` : '', state.resources.influence < cost.influence ? `${Math.ceil(cost.influence - state.resources.influence)} Influence` : ''].filter(Boolean)
      const affordable = deficits.length === 0
      return <article key={instanceId} className={`card ${isSelected ? 'selected' : ''} ${affordable ? '' : 'unaffordable'} ${tactic ? 'is-tactic' : ''}`} data-type={tactic ? 'Tactic' : def.cardType} data-faction={def.faction}>
        <button className="card-select" onClick={() => onSelect(isSelected ? null : instanceId)} aria-pressed={isSelected} aria-label={`${def.displayName}. ${benefit(def)}. Cost ${cost.capital} Capital${cost.insight ? `, ${cost.insight} Insight` : ''}${cost.influence ? `, ${cost.influence} Influence` : ''}.${affordable ? '' : ` Need ${deficits.join(' and ')} more.`}`}>
          <span className="card-head"><span className="card-type">{tactic ? `Tactic · ${def.cardType}` : def.cardType}</span><span className="card-index">0{index + 1}</span></span>
          <span className="card-name">{def.displayName}</span>
          <span className="card-benefit">{benefit(def)}</span>
          <span className="card-spec">{tactic ? <span>Play from hand · returns to deck</span> : <><span>{def.footprint[0]} × {def.footprint[1]} cells</span><span>{def.constructionCycles} cycles</span></>}</span>
        </button>
        <div className="card-foot"><span className={`card-cost ${affordable ? '' : 'bad'}`} title={affordable ? 'Cost to play' : `Need ${deficits.join(' and ')} more`}>{cost.capital > 0 || (!cost.insight && !cost.influence) ? <><b>{cost.capital}</b> CAP</> : null}{cost.insight > 0 && <small>{cost.insight} INS</small>}{cost.influence > 0 && <small>{cost.influence} INF</small>}</span><button className="card-discard" disabled={state.draw.length + state.discard.length === 0} title="Replace this card from your deck" aria-label={`Replace ${def.displayName}`} onClick={() => onCycleCard(instanceId)}>↻</button></div>
        {!affordable && <span className="card-shortfall">Need {deficits.join(' + ')}</span>}
      </article>
    })}{state.hand.length === 0 && <div className="hand-empty"><strong>Your hand is deployed.</strong><span>Inspect a building to manage your city. Decommission an asset to return its card to the discard pile.</span></div>}</div>
  </section>
}
