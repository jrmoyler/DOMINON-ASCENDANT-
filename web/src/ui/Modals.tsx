import { useEffect, useRef, useState, type ReactNode } from 'react'
import { definition } from '@/game/content'
import {
  DRAFT_SKIP_CAPITAL, EVENT_BY_ID, TACTICS, canAfford, choiceCost, computeScore, eventChoices, eventText,
  type Speaker,
} from '@/game/campaign'
import { DIFFICULTY } from '@/game/economy'
import { ACHIEVEMENTS, loadProfile } from '@/game/meta'
import type { GameState } from '@/game/types'
import { audio, type Volumes } from '@/audio/audio'
import { FACTION_CSS, TYPE_CSS } from '@/render/palette'
import { FACTION_LABEL, benefit, playCost } from './cardInfo'
import { fmt } from './format'

function Modal({ className, labelledBy, children }: { className?: string; labelledBy: string; children: ReactNode }) {
  const ref = useRef<HTMLElement>(null)
  useEffect(() => {
    const first = ref.current?.querySelector<HTMLElement>('[data-autofocus], button:not(:disabled)')
    first?.focus({ preventScroll: true })
  }, [])
  return <div className="modal-backdrop">
    <section ref={ref} className={`modal ${className ?? ''}`} role="dialog" aria-modal="true" aria-labelledby={labelledBy}>
      {children}
    </section>
  </div>
}

/** Heraldic mark for each faction, drawn rather than loaded. */
export function Emblem({ faction, size = 56 }: { faction: string; size?: number }) {
  const color = FACTION_CSS[faction as keyof typeof FACTION_CSS] ?? '#d6bc80'
  return <svg className="emblem" width={size} height={size} viewBox="0 0 64 64" aria-hidden="true">
    <rect x="2" y="2" width="60" height="60" fill="#0e1314" stroke={color} strokeWidth="1.5" />
    {faction === 'Synara' && <g fill="none" stroke={color} strokeWidth="2.5">
      <polygon points="32,10 51,21 51,43 32,54 13,43 13,21" />
      <polygon points="32,20 42,26 42,38 32,44 22,38 22,26" fill={color} fillOpacity=".25" />
      <path d="M32 10v10M32 44v10M13 21l9 5M51 43l-9-5M51 21l-9 5M13 43l9-5" />
    </g>}
    {faction === 'Forgeweave' && <g fill={color}>
      <path d="M14 40h36v6H14zM20 30h24l4 10H16z" fillOpacity=".85" />
      <path d="M26 30V16h12v14" fill="none" stroke={color} strokeWidth="2.5" />
      <path d="M29 12c2-4 6-4 6-8 3 4 2 8-1 10" fillOpacity=".6" />
    </g>}
    {faction === 'EdenCircuit' && <g fill="none" stroke={color} strokeWidth="2.5">
      <path d="M32 54C16 44 14 24 32 10c18 14 16 34 0 44z" fill={color} fillOpacity=".2" />
      <path d="M32 54V18M32 30l-8-6M32 38l9-7M32 46l-7-5" />
    </g>}
    {(faction === 'Universal' || faction === 'Special' || faction === 'Fusion') && <g fill="none" stroke={color} strokeWidth="2.5">
      <circle cx="32" cy="32" r="18" />
      <path d="M32 14v36M14 32h36" />
      <circle cx="32" cy="32" r="6" fill={color} fillOpacity=".4" />
    </g>}
  </svg>
}

function SpeakerBlock({ speaker }: { speaker: Speaker }) {
  return <div className="speaker">
    <Emblem faction={speaker.faction} />
    <div><strong>{speaker.name}</strong><span>{speaker.role}</span></div>
  </div>
}

export function EventModal({ state, onChoose }: { state: GameState; onChoose: (index: number) => void }) {
  const event = state.pendingEvent ? EVENT_BY_ID[state.pendingEvent] : null
  if (!event) return null
  const choices = eventChoices(state, event)
  return <Modal className={`event-modal ${event.story ? 'story' : ''}`} labelledBy="event-title">
    <div className="event-kicker">{event.story ? `Act ${state.act === 1 ? 'I' : state.act === 2 ? 'II' : 'III'} · Story` : 'Regional crisis · decision required'}</div>
    <h2 id="event-title">{event.title}</h2>
    <SpeakerBlock speaker={event.speaker} />
    <p className="event-text">{eventText(state, event)}</p>
    <div className="choice-list">
      {choices.map((choice, index) => {
        const cost = choiceCost(choice)
        const affordable = canAfford(state, cost)
        return <button key={choice.label} className="choice" disabled={!affordable} onClick={() => onChoose(index)}>
          <span className="choice-label"><b>{index + 1}</b>{choice.label}</span>
          {choice.detail && <span className="choice-detail">{choice.detail}</span>}
          <span className="choice-effects">
            {choice.effects.filter((e) => e.text).map((e) => <span key={e.text} className={`fx ${e.tone}`}>{e.text}</span>)}
            {!affordable && <span className="fx bad">Cannot afford</span>}
          </span>
        </button>
      })}
    </div>
    <p className="modal-note">The city waits while you decide. Keys 1–{choices.length} choose.</p>
  </Modal>
}

export function DraftCard({ id, onPick, index }: { id: string; onPick: () => void; index: number }) {
  const def = definition(id)
  const cost = playCost(def)
  const tactic = Boolean(TACTICS[id])
  return <button className="draft-card" onClick={onPick} style={{ ['--type' as string]: TYPE_CSS[def.cardType], ['--faction' as string]: FACTION_CSS[def.faction] }}>
    <span className="draft-top"><span>{tactic ? 'Tactic' : def.cardType}</span><span>{def.rarity}</span></span>
    <Emblem faction={def.faction} size={44} />
    <span className="draft-name">{def.displayName}</span>
    <span className="draft-faction">{FACTION_LABEL[def.faction]}</span>
    <span className="draft-benefit">{benefit(def)}</span>
    {!tactic && <span className="draft-desc">{def.description}</span>}
    <span className="draft-cost">
      {cost.capital > 0 && <span><b>{cost.capital}</b> CAP</span>}
      {cost.insight > 0 && <span><b>{cost.insight}</b> INS</span>}
      {cost.influence > 0 && <span><b>{cost.influence}</b> INF</span>}
      {!cost.capital && !cost.insight && !cost.influence && <span>Free</span>}
      {!tactic && <span className="draft-size">{def.footprint[0]}×{def.footprint[1]}</span>}
    </span>
    <span className="draft-key">{index + 1}</span>
  </button>
}

export function DraftModal({ state, onPick }: { state: GameState; onPick: (id: string | null) => void }) {
  if (!state.draft) return null
  return <Modal className="draft-modal" labelledBy="draft-title">
    <div className="event-kicker">Reward · {state.draft.reason}</div>
    <h2 id="draft-title">Draft a card</h2>
    <p className="modal-lede">Choose one card to add to your deck. It goes to the top of your draw pile.</p>
    <div className="draft-grid">
      {state.draft.options.map((id, index) => <DraftCard key={id} id={id} index={index} onPick={() => onPick(id)} />)}
    </div>
    <div className="dialog-actions"><button onClick={() => onPick(null)}>Skip · bank {DRAFT_SKIP_CAPITAL} Capital</button></div>
  </Modal>
}

export function OutcomeModal({ state, isBest, onNew, onTitle, onLoad }: {
  state: GameState
  isBest: boolean
  onNew: () => void
  onTitle: () => void
  onLoad?: () => void
}) {
  const [shared, setShared] = useState<string | null>(null)
  const outcome = state.outcome
  if (!outcome) return null
  const score = computeScore(state)
  const victory = outcome.kind === 'victory'
  const share = async () => {
    const url = window.location.protocol.startsWith('http') ? window.location.origin : ''
    const text = `${victory ? 'I won' : 'I fell'} in DOMINION // ASCENDANT — “${outcome.title}”. Score ${fmt(score.total)} (${score.grade}) on ${DIFFICULTY[state.difficulty].label}, ${state.stats.peakPopulation} citizens, ${state.cycle} cycles.${url ? ` Can you do better? ${url}` : ''}`
    try {
      if (navigator.share) { await navigator.share({ title: 'DOMINION // ASCENDANT', text }); setShared('Shared'); return }
    } catch { /* user cancelled */ return }
    try { await navigator.clipboard.writeText(text); setShared('Copied to clipboard') } catch { setShared(text) }
  }
  return <Modal className={`outcome-modal ${outcome.kind}`} labelledBy="outcome-title">
    <div className="event-kicker">{victory ? 'Campaign complete · Victory' : 'Campaign lost'}</div>
    <h2 id="outcome-title">{outcome.title}</h2>
    <p className="event-text">{outcome.text}</p>
    <div className="score-block">
      <div className="grade" aria-label={`Grade ${score.grade}`}>{score.grade}</div>
      <div>
        <div className="score-total">{fmt(score.total)}<small>score{isBest ? ' · new best' : ''}</small></div>
        <dl className="score-lines">
          {score.lines.filter(([, v]) => v > 0).map(([label, value]) => <div key={label}><dt>{label}</dt><dd>{fmt(value)}</dd></div>)}
        </dl>
      </div>
    </div>
    <div className="outcome-stats">
      <span><b>{state.cycle}</b> cycles</span>
      <span><b>{state.stats.peakPopulation}</b> peak citizens</span>
      <span><b>{state.stats.built}</b> buildings raised</span>
      <span><b>{state.stats.raidsRepelled}</b>/<b>{state.stats.raidsRepelled + state.stats.raidsSuffered}</b> raids repelled</span>
      <span><b>{state.stats.cardsDrafted}</b> cards drafted</span>
      <span><b>{DIFFICULTY[state.difficulty].label}</b></span>
    </div>
    {shared && <p className="modal-note" role="status">{shared}</p>}
    <div className="dialog-actions">
      <button onClick={share}>Share result</button>
      {onLoad && <button onClick={onLoad}>Load last checkpoint</button>}
      <button onClick={onTitle}>Title screen</button>
      <button className="primary" onClick={onNew}>New campaign</button>
    </div>
  </Modal>
}

export function SettingsModal({ onClose, onReplayGuide }: { onClose: () => void; onReplayGuide?: () => void }) {
  const [volumes, setVolumes] = useState<Volumes>(audio.getVolumes())
  const update = (patch: Partial<Volumes>) => {
    audio.setVolumes(patch)
    setVolumes(audio.getVolumes())
  }
  const slider = (key: 'master' | 'music' | 'sfx', label: string) => <label className="slider">
    <span>{label}<b>{Math.round(volumes[key] * 100)}</b></span>
    <input type="range" min={0} max={1} step={0.05} value={volumes[key]}
      onChange={(e) => update({ [key]: Number(e.target.value) })}
      onPointerUp={() => audio.play('click')} />
  </label>
  return <Modal className="settings-modal" labelledBy="settings-title">
    <h2 id="settings-title">Settings</h2>
    <div className="settings-group">
      <h3>Audio</h3>
      <label className="toggle"><input type="checkbox" checked={volumes.muted} onChange={(e) => update({ muted: e.target.checked })} /> Mute all sound</label>
      {slider('master', 'Master')}
      {slider('music', 'Music')}
      {slider('sfx', 'Effects')}
    </div>
    <div className="dialog-actions">
      {onReplayGuide && <button onClick={onReplayGuide}>Field guide</button>}
      <button className="primary" onClick={onClose}>Done</button>
    </div>
  </Modal>
}

export function HonoursModal({ onClose }: { onClose: () => void }) {
  const profile = loadProfile()
  return <Modal className="honours-modal" labelledBy="honours-title">
    <h2 id="honours-title">Honours</h2>
    <p className="modal-lede">{profile.achievements.length} of {ACHIEVEMENTS.length} earned · {profile.victories} victories in {profile.campaigns} campaigns · {profile.endings.length}/5 endings</p>
    <ul className="honours">
      {ACHIEVEMENTS.map((a) => {
        const earned = profile.achievements.includes(a.id)
        return <li key={a.id} className={earned ? 'earned' : ''}>
          <span className="honour-mark" aria-hidden="true">{earned ? '◆' : '◇'}</span>
          <div><strong>{a.title}</strong><span>{a.text}</span></div>
        </li>
      })}
    </ul>
    <div className="best-scores">
      {(['settler', 'governor', 'ascendant'] as const).map((d) => <span key={d}>{DIFFICULTY[d].label}<b>{profile.best[d] ? fmt(profile.best[d]!) : '—'}</b></span>)}
    </div>
    <div className="dialog-actions"><button className="primary" onClick={onClose}>Close</button></div>
  </Modal>
}
