import { useEffect } from 'react'
import { EXPECTED_COUNTS } from '@/game/content'
import { STARTING_POPULATION, STARTING_RESOURCES } from '@/game/economy'
import { revealIntro } from './motion'

export function Intro({ onStart, onContinue, rendererStatus }: {
  onStart: () => void
  onContinue?: () => void
  rendererStatus: { kind: 'loading' | 'ready' | 'unavailable'; message?: string }
}) {
  useEffect(() => { const animation = revealIntro(); return () => { animation?.revert() } }, [])
  const ready = rendererStatus.kind === 'ready'
  return <main className="intro">
    <div className="intro-edition">A CITY IS A PROMISE. <span>KEEP YOURS.</span></div>
    <div className="intro-inner">
      <div className="intro-kicker">SYNARA AUTHORITY <span>01 / ASHCROFT BASIN</span></div>
      <h1 className="intro-title">DOMINION<span className="intro-title-second"><span className="slash">//</span> ASCENDANT</span></h1>
      <p className="intro-sub">Build a civilization worth inheriting.</p>
      <p className="intro-body">The Founder Hall has awakened. Beyond its walls, an entire basin waits for power, shelter and purpose. Your first city begins with a hand of cards.</p>
      <div className="intro-supplies" aria-label="Starting resources">
        <span><strong>{EXPECTED_COUNTS.starterInstances}</strong> cards in your deck</span>
        <span><strong>{STARTING_RESOURCES.capital}</strong> starting Capital</span>
        <span><strong>{STARTING_POPULATION}</strong> citizens to lead</span>
      </div>
      <div className="intro-actions">
        {onContinue && <button className="primary" onClick={onContinue} disabled={!ready}>Continue campaign <span aria-hidden="true">→</span></button>}
        <button className={onContinue ? 'secondary' : 'primary'} onClick={onStart} disabled={!ready}>
          {rendererStatus.kind === 'loading' ? 'Preparing the basin…' : ready ? onContinue ? 'Begin a new campaign' : 'Assume command' : 'World unavailable'}
          {ready && !onContinue && <span aria-hidden="true">→</span>}
        </button>
      </div>
      <div className={`intro-system ${rendererStatus.kind}`} role="status">
        <span className="system-indicator" aria-hidden="true" />
        {rendererStatus.kind === 'loading' ? 'Surveying terrain and preparing your city' : ready ? 'Ashcroft Basin is ready for your arrival' : rendererStatus.message}
      </div>
      <details className="field-guide">
        <summary>Field guide <span>How to play + controls</span></summary>
        <div className="intro-rules">
          <div className="intro-rule"><h3>01 / Establish</h3><p>Select a card, then tap open ground. Build near the Founder Hall: utilities reach six cells.</p></div>
          <div className="intro-rule"><h3>02 / Sustain</h3><p>Add housing and jobs. Keep Power, Water and Data supplied. Commerce funds your expansion.</p></div>
          <div className="intro-rule"><h3>03 / Ascend</h3><p>Follow First Hour objectives to achieve Forgeweave Ascension and your first Convergence Authority.</p></div>
        </div>
        <div className="intro-controls">
          <span><kbd>Tap / click</kbd> place or inspect</span><span><kbd>Drag</kbd> orbit</span>
          <span><kbd>Right-drag</kbd> pan</span><span><kbd>Scroll / pinch</kbd> zoom</span>
          <span><kbd>R</kbd> rotate</span><span><kbd>1–6</kbd> select card</span><span><kbd>Space</kbd> pause</span><span><kbd>Esc</kbd> cancel</span>
        </div>
      </details>
    </div>
    <div className="intro-footer"><span>THE FIRST HOUR</span><span>Build. Sustain. Ascend.</span></div>
  </main>
}
