import { useEffect } from 'react'
import { EXPECTED_COUNTS } from '@/game/content'
import { STARTING_POPULATION, STARTING_RESOURCES } from '@/game/economy'
import { revealIntro } from './motion'

export function Intro({
  onStart,
  onContinue,
  rendererStatus,
}: {
  onStart: () => void
  onContinue?: () => void
  rendererStatus: { kind: 'loading' | 'ready' | 'unavailable'; message?: string }
}) {
  useEffect(() => {
    revealIntro()
  }, [])

  return (
    <div className="intro">
      <div className="intro-inner">
        <div className="intro-kicker">SYNARA COMMAND AUTHORITY · ASHCROFT BASIN</div>
        <h1 className="intro-title">
          DOMINION <span className="slash">//</span> ASCENDANT
        </h1>
        <p className="intro-sub">Frontier Capital Command Simulation</p>

        <p className="intro-body">
          The Founder Hall has woken after a long dark. You hold a {EXPECTED_COUNTS.starterInstances}
          -card Synara starter deck, {STARTING_RESOURCES.capital} Capital, and a 32×32 grid of
          frozen ground. Build a city that can feed {STARTING_POPULATION} citizens, keep the lights
          on, and carry the basin through its first hour toward Forgeweave Ascension.
        </p>

        <div className="intro-rules">
          <div className="intro-rule">
            <h3>Build</h3>
            <p>
              Pick a card, click a cell. Utilities only reach six cells — put power and water where
              the city actually is.
            </p>
          </div>
          <div className="intro-rule">
            <h3>Balance</h3>
            <p>
              Housing draws migrants, jobs employ them, commerce and industry pay for all of it.
              Approval falls when any of the three is missing.
            </p>
          </div>
          <div className="intro-rule">
            <h3>Ascend</h3>
            <p>
              Nine first-hour quests gate the run. Close them all to reach CONVERGENCE AUTHORITY
              1/20.
            </p>
          </div>
        </div>

        <div className="intro-actions">
          <button
            className="primary"
            onClick={onStart}
            disabled={rendererStatus.kind !== 'ready'}
          >
            {rendererStatus.kind === 'loading' ? 'Initializing world' : rendererStatus.kind === 'ready' ? 'Assume command' : 'Renderer unavailable'}
          </button>
          {onContinue && (
            <button className="secondary" onClick={onContinue}>
              Continue saved campaign
            </button>
          )}
        </div>

        <div className={`intro-system ${rendererStatus.kind}`} role="status">
          <span className="system-indicator" />
          {rendererStatus.kind === 'loading' && 'Babylon command world initializing'}
          {rendererStatus.kind === 'ready' && 'Command world online · Babylon.js 9'}
          {rendererStatus.kind === 'unavailable' && rendererStatus.message}
        </div>

        <div className="intro-controls">
          <span><kbd>Click</kbd> place / select</span>
          <span><kbd>Drag</kbd> orbit</span>
          <span><kbd>Shift</kbd>+drag or right-drag pan</span>
          <span><kbd>Scroll</kbd> zoom</span>
          <span><kbd>R</kbd> rotate</span>
          <span><kbd>1–6</kbd> pick card</span>
          <span><kbd>Space</kbd> pause</span>
        </div>
      </div>
    </div>
  )
}
