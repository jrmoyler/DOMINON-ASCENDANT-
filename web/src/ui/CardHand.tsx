import { definition } from '@/game/content'
import type { GameState } from '@/game/types'
import { FACTION_CSS, TYPE_CSS } from '@/render/palette'

interface Props {
  state: GameState
  onSelect: (instanceId: string | null) => void
  onCycleCard: (instanceId: string) => void
  onRotate: () => void
  rotation: 0 | 1 | 2 | 3
}

export function CardHand({ state, onSelect, onCycleCard, onRotate, rotation }: Props) {
  return (
    <div className="hand-bar">
      <div className="hand-meta">
        <div className="deck-counts">
          <span title="Cards remaining in the draw pile">DRAW {state.draw.length}</span>
          <span title="Cards in the discard pile">DISCARD {state.discard.length}</span>
        </div>
        <button className="rotate-btn" onClick={onRotate} title="Rotate footprint (R)">
          ⟳ {rotation * 90}°
        </button>
      </div>

      <div className="hand-cards">
        {state.hand.map((instanceId) => {
          const instance = state.instances[instanceId]
          const def = definition(instance.definitionId)
          const selected = state.selectedInstanceId === instanceId
          const affordable =
            state.resources.capital >= def.deploymentCapital &&
            state.resources.insight >= def.deploymentInsight &&
            state.resources.influence >= def.deploymentInfluence

          return (
            <div
              key={instanceId}
              className={`card ${selected ? 'selected' : ''} ${affordable ? '' : 'unaffordable'}`}
              onClick={() => onSelect(selected ? null : instanceId)}
              style={{ borderTopColor: FACTION_CSS[def.faction] }}
            >
              <div className="card-head">
                <span className="card-type" style={{ color: TYPE_CSS[def.cardType] }}>
                  {def.cardType}
                </span>
                <span className="card-fp">
                  {def.footprint[0]}×{def.footprint[1]}
                </span>
              </div>

              <div className="card-name">{def.displayName}</div>

              <div className="card-stats">
                {def.housingCapacity > 0 && <span>HSG {def.housingCapacity}</span>}
                {def.jobCapacity > 0 && <span>JOBS {def.jobCapacity}</span>}
                {def.baseCapitalPerCycle > 0 && (
                  <span>CAP +{def.baseCapitalPerCycle.toFixed(1)}</span>
                )}
                {def.baseInsightPerCycle > 0 && <span>INS +{def.baseInsightPerCycle.toFixed(2)}</span>}
                {def.powerProduced > 0 && <span>PWR +{Math.round(def.powerProduced)}</span>}
                {def.waterProduced > 0 && <span>H2O +{Math.round(def.waterProduced)}</span>}
                {def.utilityPower > 0 && <span className="dim">PWR −{Math.round(def.utilityPower)}</span>}
              </div>

              <div className="card-foot">
                <span className={`card-cost ${affordable ? '' : 'bad'}`}>
                  {def.deploymentCapital} CAP
                  {def.deploymentInsight > 0 && ` · ${def.deploymentInsight} INS`}
                </span>
                <button
                  className="card-discard"
                  title="Discard and draw a replacement"
                  onClick={(e) => {
                    e.stopPropagation()
                    onCycleCard(instanceId)
                  }}
                >
                  ↻
                </button>
              </div>
            </div>
          )
        })}
        {state.hand.length === 0 && (
          <div className="hand-empty">No cards in hand — the deck is exhausted.</div>
        )}
      </div>
    </div>
  )
}
