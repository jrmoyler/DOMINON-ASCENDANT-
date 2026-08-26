import type { GameState } from '@/game/types'
import { CYCLES_PER_WORLD_TICK } from '@/game/economy'
import { BUILD_LIMIT } from '@/game/grid'
import { fmt, signed } from './format'

interface Props {
  state: GameState
  income: { capital: number; insight: number; influence: number }
  onSpeed: (speed: 0 | 1 | 2 | 4) => void
  onSave: () => void
  onLoad: () => void
  onRestart: () => void
  hasSave: boolean
}

function Stat({
  label,
  value,
  delta,
  tone,
}: {
  label: string
  value: string
  delta?: string
  tone?: string
}) {
  return (
    <div className="stat">
      <span className="stat-label">{label}</span>
      <span className="stat-value" style={tone ? { color: tone } : undefined}>
        {value}
      </span>
      {delta && (
        <span className={`stat-delta ${delta.startsWith('-') ? 'neg' : 'pos'}`}>{delta}</span>
      )}
    </div>
  )
}

export function Hud({ state, income, onSpeed, onSave, onLoad, onRestart, hasSave }: Props) {
  const t = state.totals
  const powerShort = t.powerDemand > t.powerSupply
  const waterShort = t.waterDemand > t.waterSupply
  const housingShort = t.housingCapacity < state.population

  return (
    <header className="hud-top">
      <div className="hud-brand">
        <span className="brand-mark">DOMINION</span>
        <span className="brand-slash">//</span>
        <span className="brand-mark alt">ASCENDANT</span>
      </div>

      <div className="hud-stats">
        <Stat label="Capital" value={fmt(state.resources.capital)} delta={signed(income.capital)} />
        <Stat label="Insight" value={fmt(state.resources.insight)} delta={signed(income.insight, 2)} />
        <Stat
          label="Influence"
          value={fmt(state.resources.influence)}
          delta={signed(income.influence, 2)}
        />
        <div className="stat-divider" />
        <Stat
          label="Population"
          value={`${fmt(state.population)} / ${fmt(t.housingCapacity)}`}
          tone={housingShort ? '#f05252' : undefined}
        />
        <Stat
          label="Jobs"
          value={`${fmt(t.employed)} / ${fmt(t.jobCapacity)}`}
          tone={t.jobCapacity < state.population ? '#e8913a' : undefined}
        />
        <Stat
          label="Approval"
          value={`${fmt(t.happiness)}%`}
          tone={t.happiness < 40 ? '#f05252' : t.happiness > 65 ? '#4ade80' : '#e8913a'}
        />
        <div className="stat-divider" />
        <Stat
          label="Power"
          value={`${fmt(t.powerSupply)} / ${fmt(t.powerDemand)}`}
          tone={powerShort ? '#f05252' : undefined}
        />
        <Stat
          label="Water"
          value={`${fmt(t.waterSupply)} / ${fmt(t.waterDemand)}`}
          tone={waterShort ? '#f05252' : undefined}
        />
        <Stat label="Assets" value={`${state.assets.length} / ${BUILD_LIMIT}`} />
      </div>

      <div className="hud-time">
        <div className="cycle-readout">
          <span className="cycle-label">CYCLE</span>
          <span className="cycle-value">{state.cycle}</span>
          <span className="tick-label">
            TICK {state.worldTick} · {state.cycle % CYCLES_PER_WORLD_TICK}/{CYCLES_PER_WORLD_TICK}
          </span>
          <div className="cycle-bar">
            <div className="cycle-fill" style={{ width: `${state.cycleProgress * 100}%` }} />
          </div>
        </div>
        <div className="speed-group" role="group" aria-label="Simulation speed">
          {([0, 1, 2, 4] as const).map((s) => (
            <button
              key={s}
              className={`speed-btn ${state.speed === s ? 'active' : ''}`}
              onClick={() => onSpeed(s)}
              title={s === 0 ? 'Pause' : `${s}x speed`}
            >
              {s === 0 ? '❚❚' : `${s}×`}
            </button>
          ))}
        </div>
        <div className="hud-actions">
          <button onClick={onSave} title="Save to this browser">Save</button>
          <button onClick={onLoad} disabled={!hasSave} title="Load saved campaign">Load</button>
          <button onClick={onRestart} title="Start a new campaign">New</button>
        </div>
      </div>
    </header>
  )
}
