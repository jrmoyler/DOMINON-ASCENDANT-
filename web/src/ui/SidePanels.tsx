import { definition } from '@/game/content'
import { UTILITY_RADIUS } from '@/game/economy'
import type { GameState, OverlayId } from '@/game/types'
import { FACTION_CSS, TYPE_CSS } from '@/render/palette'
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

  return (
    <section className="panel quest-panel">
      <h2>
        First Hour
        <span className="panel-badge">
          {done}/{state.quests.length}
        </span>
      </h2>

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
    </section>
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
    <section className="panel overlay-panel">
      <h2>Overlays</h2>
      <div className="overlay-grid">
        {OVERLAYS.map((o) => (
          <button
            key={o.id}
            className={state.overlay === o.id ? 'active' : ''}
            onClick={() => onOverlay(o.id)}
          >
            {o.label}
          </button>
        ))}
      </div>
    </section>
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

  const derived = !def.authored.has('deploymentCapital')

  return (
    <section className="panel inspect-panel">
      <h2>
        Inspect
        <button className="panel-close" onClick={onClose} aria-label="Close inspector">
          ×
        </button>
      </h2>
      <div className="inspect-name" style={{ color: FACTION_CSS[def.faction] }}>
        {def.displayName}
      </div>
      <div className="inspect-type" style={{ color: TYPE_CSS[def.cardType] }}>
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
      {derived && (
        <p className="inspect-note">
          Values derived for the browser slice — the content manifests leave this card’s economy
          explicitly unauthored.
        </p>
      )}
      {def.id !== 'special.founder_hall' && (
        <button className="demolish-btn" onClick={() => onDemolish(asset.id)}>
          Decommission (refund {Math.floor(def.deploymentCapital / 2)} ⬢)
        </button>
      )}
    </section>
  )
}

export function LogPanel({ state }: { state: GameState }) {
  return (
    <section className="panel log-panel">
      <h2>Signal</h2>
      <ul className="log-list">
        {state.log.slice(0, 40).map((entry, i) => (
          <li key={`${entry.cycle}-${i}`} className={entry.kind}>
            <span className="log-cycle">{entry.cycle}</span>
            <span className="log-text">{entry.text}</span>
          </li>
        ))}
      </ul>
    </section>
  )
}
