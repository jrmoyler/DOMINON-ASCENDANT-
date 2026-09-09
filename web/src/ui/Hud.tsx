import { useRef } from 'react'
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
function Stat({ label, value, delta, tone }: { label: string; value: string; delta?: string; tone?: string }) {
  return <div className="stat"><span className="stat-label">{label}</span><span className="stat-value" style={tone ? { color: tone } : undefined}>{value}</span>{delta && <span className={`stat-delta ${delta.startsWith('-') ? 'neg' : 'pos'}`} title="Projected change per cycle">{delta}<small> / cycle</small></span>}</div>
}
export function Hud({ state, income, onSpeed, onSave, onLoad, onRestart, hasSave }: Props) {
  const t = state.totals
  const menuRef = useRef<HTMLDetailsElement>(null)
  const closeMenu = () => { if (menuRef.current) menuRef.current.open = false }
  const warning = t.powerDemand > t.powerSupply || t.waterDemand > t.waterSupply || t.dataDemand > t.dataSupply
  return <header className="hud-top" aria-label="Command controls">
    <div className="command-ledger">
      <div className="hud-brand"><span>DA <b>//</b></span><small>ASHCROFT BASIN</small></div>
      <div className="hud-stats">
        <Stat label="Capital" value={fmt(state.resources.capital)} delta={signed(income.capital)} />
        <Stat label="Insight" value={fmt(state.resources.insight, 1)} delta={signed(income.insight, 2)} />
        <Stat label="Influence" value={fmt(state.resources.influence, 1)} delta={signed(income.influence, 2)} />
      </div>
      <details className="city-ledger">
        <summary className={warning ? 'has-warning' : ''}>City <span>{warning ? 'Needs supply' : `${fmt(t.happiness)}% approval`}</span></summary>
        <div className="ledger-content">
          <h2>City ledger <span>Supply / demand</span></h2>
          <div className="city-metrics">
            <Stat label="Citizens / housing" value={`${fmt(state.population)} / ${fmt(t.housingCapacity)}`} tone={t.housingCapacity < state.population ? 'var(--warn)' : undefined} />
            <Stat label="Employed / jobs" value={`${fmt(t.employed)} / ${fmt(t.jobCapacity)}`} />
            <Stat label="Approval" value={`${fmt(t.happiness)}%`} tone={t.happiness < 40 ? 'var(--bad)' : 'var(--good)'} />
            <Stat label="Power" value={`${fmt(t.powerSupply)} / ${fmt(t.powerDemand)}`} tone={t.powerDemand > t.powerSupply ? 'var(--bad)' : undefined} />
            <Stat label="Water" value={`${fmt(t.waterSupply)} / ${fmt(t.waterDemand)}`} tone={t.waterDemand > t.waterSupply ? 'var(--bad)' : undefined} />
            <Stat label="Data" value={`${fmt(t.dataSupply)} / ${fmt(t.dataDemand)}`} tone={t.dataDemand > t.dataSupply ? 'var(--bad)' : undefined} />
            <Stat label="Placed assets" value={`${state.assets.length} / ${BUILD_LIMIT}`} />
          </div><p className="panel-help">Supply is local. Use map layers to check whether each building is in range.</p>
        </div>
      </details>
    </div>
    <div className="hud-time">
      <div className="cycle-readout"><span className="cycle-label">{state.speed === 0 ? 'PAUSED' : 'CYCLE'} <b>{state.cycle}</b></span><span className="tick-label">WORLD TICK {state.worldTick} · {state.cycle % CYCLES_PER_WORLD_TICK}/{CYCLES_PER_WORLD_TICK}</span><div className="cycle-bar"><div className="cycle-fill" style={{ width: `${state.cycleProgress * 100}%` }} /></div></div>
      <div className="speed-group" role="group" aria-label="Simulation speed">{([0, 1, 2, 4] as const).map(s => <button key={s} className={`speed-btn ${state.speed === s ? 'active' : ''}`} onClick={() => onSpeed(s)} aria-pressed={state.speed === s} aria-label={s === 0 ? 'Pause simulation' : `Play at ${s} times speed`}>{s === 0 ? 'Ⅱ' : `${s}×`}</button>)}</div>
      <details className="campaign-menu" ref={menuRef}>
        <summary aria-label="Campaign menu">Menu</summary><div className="campaign-content">
          <h2>Campaign</h2>
          <button onClick={() => { onSave(); closeMenu() }}>Save campaign</button>
          <button disabled={!hasSave} onClick={() => { closeMenu(); onLoad() }}>Load saved campaign</button>
          <button onClick={() => { closeMenu(); onRestart() }}>Start new campaign</button>
          <p className="panel-help">Saves are stored on this browser.</p>
          <details className="menu-controls"><summary>Controls</summary><p>Tap a card, then open ground to place. Drag to orbit; pinch or scroll to zoom. R rotates, 1–6 select cards, Space pauses, Esc cancels selection.</p></details>
        </div>
      </details>
    </div>
  </header>
}
