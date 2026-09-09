import { useState } from 'react'
import { definition } from '@/game/content'
import { UTILITY_RADIUS } from '@/game/economy'
import type { GameState, OverlayId } from '@/game/types'
import { fmt } from './format'

const OVERLAYS: { id: OverlayId; label: string }[] = [
  { id: 'none', label: 'None' },
  { id: 'power', label: 'Power' },
  { id: 'water', label: 'Water' },
  { id: 'data', label: 'Data' },
  { id: 'housing', label: 'Housing' },
  { id: 'employment', label: 'Jobs' },
  { id: 'happiness', label: 'Approval' },
]

export function QuestPanel({ state }: { state: GameState }) {
  const active = state.quests.filter((q) => q.status !== 'locked')
  const done = state.quests.filter((q) => q.status === 'complete').length
  const next = state.quests.find((q) => q.status === 'active')
  const objective = next?.objectives.find(o => !o.done)

  return (
    <details className="panel quest-panel">
      <summary><span>First Hour <b>{done}/{state.quests.length}</b><small>{state.ascended ? 'Forgeweave Ascension achieved' : next?.title ?? 'Awaiting next objective'}</small></span></summary>
      <div className="panel-content">
      {objective && <p className="next-objective">Next: {objective.label} <strong>{fmt(objective.progress)}/{fmt(objective.target)}</strong></p>}

      <div className="ascension-bar" title="Progress toward Forgeweave Ascension">
        <div className="ascension-fill" style={{ width: `${state.ascensionProgress * 100}%` }} />
      </div>
      {state.ascended && <div className="ascension-flag">CONVERGENCE AUTHORITY 1/20</div>}

      <ul className="quest-list">
        {active.map((quest) => (
          <li key={quest.id} className={`quest ${quest.status}`}>
            <div className="quest-title">
              {quest.status === 'complete' ? '✓' : '▸'} {quest.title}
            </div>
            {quest.status === 'active' && (
              <ul className="objectives">
                {quest.objectives.map((o) => (
                  <li key={o.id} className={o.done ? 'done' : ''}>
                    <span className="obj-label">{o.label}</span>
                    <span className="obj-progress">
                      {fmt(o.progress)}/{fmt(o.target)}
                    </span>
                  </li>
                ))}
              </ul>
            )}
          </li>
        ))}
      </ul>
      </div>
    </details>
  )
}

export function OverlayPanel({
  state,
  onOverlay,
}: {
  state: GameState
  onOverlay: (id: OverlayId) => void
}) {
  return (
    <details className="panel overlay-panel">
      <summary>Map layers <span>{state.overlay === 'none' ? 'Terrain' : OVERLAYS.find(o => o.id === state.overlay)?.label}</span></summary>
      <div className="panel-content">
      <div className="overlay-grid">
        {OVERLAYS.map((o) => (
          <button
            key={o.id}
            className={state.overlay === o.id ? 'active' : ''}
            aria-pressed={state.overlay === o.id}
            onClick={() => onOverlay(o.id)}
          >
            {o.label}
          </button>
        ))}
      </div>
      <p className="panel-help">Power, Water and Data show local utility reach. Buildings outside that reach cannot operate at full capacity.</p>
      </div>
    </details>
  )
}

export function InspectPanel({
  state,
  onDemolish,
  onClose,
}: {
  state: GameState
  onDemolish: (assetId: string) => void
  onClose: () => void
}) {
  const [confirmAsset, setConfirmAsset] = useState<string | null>(null)
  const asset = state.assets.find((a) => a.id === state.selectedAssetId)
  if (!asset) return null
  const def = definition(asset.definitionId)

  const rows: [string, string][] = [
    ['Faction', def.faction],
    ['Type', def.cardType],
    ['Footprint', `${def.footprint[0]}×${def.footprint[1]} cells`],
    ['Status', asset.operational ? (asset.brownout ? 'Brownout' : 'Operational') : `Building — ${asset.cyclesRemaining} cycles left`],
  ]
  if (def.housingCapacity > 0) rows.push(['Housing', fmt(def.housingCapacity)])
  if (def.jobCapacity > 0) rows.push(['Jobs', `${asset.staffed} / ${def.jobCapacity} staffed`])
  if (def.baseCapitalPerCycle > 0) rows.push(['Capital / cycle', def.baseCapitalPerCycle.toFixed(2)])
  if (def.baseInsightPerCycle > 0) rows.push(['Insight / cycle', def.baseInsightPerCycle.toFixed(2)])
  if (def.baseInfluencePerCycle > 0) rows.push(['Influence / cycle', def.baseInfluencePerCycle.toFixed(2)])
  if (def.maintenanceCapitalPerCycle > 0) rows.push(['Maintenance', `−${def.maintenanceCapitalPerCycle.toFixed(2)}`])
  if (def.powerProduced > 0) rows.push(['Power supplied', `+${Math.round(def.powerProduced)} (r${UTILITY_RADIUS})`])
  if (def.waterProduced > 0) rows.push(['Water supplied', `+${Math.round(def.waterProduced)} (r${UTILITY_RADIUS})`])
  if (def.utilityPower > 0) rows.push(['Power drawn', `−${Math.round(def.utilityPower)}`])

  if (def.utilityWater > 0) rows.push(['Water drawn', `−${def.utilityWater.toFixed(1)}`])
  if (def.utilityData > 0) rows.push(['Data drawn', `−${def.utilityData.toFixed(1)}`])
  if (def.dataProduced > 0) rows.push(['Data supplied', `+${Math.round(def.dataProduced)} (r${UTILITY_RADIUS})`])

  return (
    <section className="panel inspect-panel">
      <h2>
        Inspect
        <button className="panel-close" onClick={onClose} aria-label="Close inspector">
          ×
        </button>
      </h2>
      <div className="inspect-name">
        {def.displayName}
      </div>
      <div className="inspect-type">
        {def.cardType} · {def.rarity}
      </div>
      <p className="inspect-desc">{def.description}</p>
      <dl className="inspect-rows">
        {rows.map(([k, v]) => (
          <div key={k} className="inspect-row">
            <dt>{k}</dt>
            <dd>{v}</dd>
          </div>
        ))}
      </dl>
      {def.id !== 'special.founder_hall' && (
        <div className="decommission"><button className="demolish-btn" onClick={() => setConfirmAsset(asset.id)}>Decommission · {Math.floor(def.deploymentCapital / 2)} Capital refund</button>
        {confirmAsset === asset.id && <div className="confirm-action"><p>Remove {def.displayName}? Its output will stop immediately.</p><button className="danger-button" onClick={() => { onDemolish(asset.id); setConfirmAsset(null) }}>Confirm decommission</button><button onClick={() => setConfirmAsset(null)}>Keep building</button></div>}</div>
      )}
    </section>
  )
}

export function LogPanel({ state }: { state: GameState }) {
  return (
    <details className="panel log-panel">
      <summary>Dispatches <span>{state.log.length}</span></summary>
      <div className="panel-content">
      <ul className="log-list">
        {state.log.slice(0, 40).map((entry, i) => (
          <li key={`${entry.cycle}-${i}`} className={entry.kind}>
            <span className="log-cycle">{entry.cycle}</span>
            <span className="log-text">{entry.text}</span>
          </li>
        ))}
      </ul>
      </div>
    </details>
  )
}
