import { useEffect, useState } from 'react'
import { EXPECTED_COUNTS } from '@/game/content'
import { DIFFICULTY, STARTING_POPULATION } from '@/game/economy'
import { QUEST_COUNT } from '@/game/quests'
import { loadProfile } from '@/game/meta'
import type { Difficulty } from '@/game/types'
import { isStandalone, onInstallAvailable } from '@/pwa/install'
import { audio } from '@/audio/audio'
import { revealIntro } from './motion'
import { fmt } from './format'

export function Intro({ onStart, onContinue, onSettings, onHonours, rendererStatus }: {
  onStart: (difficulty: Difficulty) => void
  onContinue?: () => void
  onSettings: () => void
  onHonours: () => void
  rendererStatus: { kind: 'loading' | 'ready' | 'unavailable'; message?: string }
}) {
  useEffect(() => { const animation = revealIntro(); return () => { animation?.revert() } }, [])
  const [choosing, setChoosing] = useState(false)
  const [difficulty, setDifficulty] = useState<Difficulty>('governor')
  const [install, setInstall] = useState<null | (() => Promise<boolean>)>(null)
  const [profile] = useState(loadProfile)
  useEffect(() => (isStandalone() ? undefined : onInstallAvailable((prompt) => setInstall(() => prompt))), [])
  const ready = rendererStatus.kind === 'ready'
  const best = Math.max(0, ...Object.values(profile.best).map(Number))

  return <main className={`intro ${choosing ? 'choosing' : ''}`}>
    <div className="intro-edition">A CITY IS A PROMISE. <span>KEEP YOURS.</span></div>
    <div className="intro-inner">
      <div className="intro-kicker">SYNARA AUTHORITY <span>ASHCROFT BASIN · A CAMPAIGN IN THREE ACTS</span></div>
      <h1 className="intro-title">DOMINION<span className="intro-title-second"><span className="slash">//</span> ASCENDANT</span></h1>
      <p className="intro-sub">Build a civilization worth inheriting.</p>
      <p className="intro-body">The Founder Hall has awakened. Raise a city from an empty basin, weather the crises that test it, and hold it against the Forge Lord of Ironheart. Your city begins with a hand of cards.</p>
      <div className="intro-supplies" aria-label="Campaign">
        <span><strong>{EXPECTED_COUNTS.starterInstances}</strong> cards in your deck</span>
        <span><strong>{QUEST_COUNT}</strong> objectives · 3 acts</span>
        <span><strong>5</strong> endings</span>
        <span><strong>{STARTING_POPULATION}</strong> citizens to lead</span>
      </div>

      {choosing ? <div className="difficulty-picker" role="radiogroup" aria-label="Difficulty">
        {(Object.keys(DIFFICULTY) as Difficulty[]).map((key) => <button
          key={key}
          role="radio"
          aria-checked={difficulty === key}
          className={`difficulty ${difficulty === key ? 'active' : ''}`}
          onClick={() => { setDifficulty(key); audio.play('click') }}
        >
          <strong>{DIFFICULTY[key].label}</strong>
          <span>{DIFFICULTY[key].blurb}</span>
          <small>{DIFFICULTY[key].capital} starting Capital{profile.best[key] ? ` · best ${fmt(profile.best[key]!)}` : ''}</small>
        </button>)}
        <div className="intro-actions">
          <button className="primary" onClick={() => onStart(difficulty)} disabled={!ready}>Begin campaign <span aria-hidden="true">→</span></button>
          <button className="secondary" onClick={() => setChoosing(false)}>Back</button>
        </div>
      </div> : <div className="intro-actions">
        {onContinue && <button className="primary" onClick={onContinue} disabled={!ready}>Continue campaign <span aria-hidden="true">→</span></button>}
        <button className={onContinue ? 'secondary' : 'primary'} onClick={() => { setChoosing(true); audio.play('select') }} disabled={!ready}>
          {rendererStatus.kind === 'loading' ? 'Preparing the basin…' : ready ? 'New campaign' : 'World unavailable'}
          {ready && !onContinue && <span aria-hidden="true">→</span>}
        </button>
      </div>}

      <div className="intro-links">
        <button onClick={onHonours}>Honours <span>{profile.achievements.length}</span></button>
        <button onClick={onSettings}>Settings</button>
        {install && <button onClick={async () => { if (await install()) setInstall(null) }}>Install game</button>}
      </div>

      <div className={`intro-system ${rendererStatus.kind}`} role="status">
        <span className="system-indicator" aria-hidden="true" />
        {rendererStatus.kind === 'loading' ? 'Surveying terrain and preparing your city' : ready ? (best ? `Ashcroft is ready · your best score ${fmt(best)}` : 'Ashcroft Basin is ready for your arrival') : rendererStatus.message}
      </div>
      <details className="field-guide">
        <summary>Field guide <span>How to play + controls</span></summary>
        <div className="intro-rules">
          <div className="intro-rule"><h3>01 / Establish</h3><p>Select a card, then tap open ground. Build near the Founder Hall: utilities reach six cells.</p></div>
          <div className="intro-rule"><h3>02 / Sustain</h3><p>Balance homes, jobs and approval. Answer crises. Draft new cards as you complete objectives.</p></div>
          <div className="intro-rule"><h3>03 / Hold</h3><p>In Act III, Ironheart raids your city. Raise defenses, spend Influence, and choose how the war ends.</p></div>
        </div>
        <div className="intro-controls">
          <span><kbd>Tap / click</kbd> place or inspect</span><span><kbd>Drag</kbd> orbit</span>
          <span><kbd>Right-drag</kbd> pan</span><span><kbd>Scroll / pinch</kbd> zoom</span>
          <span><kbd>R</kbd> rotate</span><span><kbd>1–6</kbd> select card</span><span><kbd>T</kbd> play tactic</span><span><kbd>Space</kbd> pause</span><span><kbd>Esc</kbd> cancel</span>
        </div>
      </details>
    </div>
    <div className="intro-footer"><span>THREE ACTS · FIVE ENDINGS</span><span>Build. Endure. Ascend.</span></div>
  </main>
}
