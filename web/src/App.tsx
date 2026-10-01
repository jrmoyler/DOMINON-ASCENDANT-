import { useCallback, useEffect, useMemo, useRef, useState } from 'react'
import { CityScene } from '@/render/Scene'
import { createSceneSafely } from '@/render/createSceneSafely'
import { definition } from '@/game/content'
import { CYCLE_SECONDS } from '@/game/economy'
import { previewPlacement, projectNextCycle } from '@/game/planning'
import {
  advanceCycle,
  chooseDraft,
  createInitialState,
  cycleCard,
  decideEvent,
  demolish,
  garrison,
  influencePressure,
  loadFromStorage,
  marketDraft,
  placeCard,
  saveToStorage,
  playTacticCard,
} from '@/game/state'
import { EVENT_BY_ID, TACTICS, eventChoices } from '@/game/campaign'
import { advisorTip } from '@/game/advisor'
import { checkAchievements, recordCampaignStart, recordOutcome } from '@/game/meta'
import type { Difficulty, GameState, OverlayId } from '@/game/types'
import { audio } from '@/audio/audio'
import { Hud } from '@/ui/Hud'
import { CardHand } from '@/ui/CardHand'
import { AdvisorStrip, CommandPanel, InspectPanel, LogPanel, OverlayPanel, QuestPanel } from '@/ui/SidePanels'
import { DraftModal, EventModal, HonoursModal, OutcomeModal, SettingsModal } from '@/ui/Modals'
import { Intro } from '@/ui/Intro'
import { announceToast, revealCommandInterface } from '@/ui/motion'
import { fmt } from '@/ui/format'

/** Day length in Development Cycles. */
const DAY_CYCLES = 8

function nightOf(state: GameState): number {
  const phase = ((state.cycle + state.cycleProgress) / DAY_CYCLES) % 1
  // Long day, short dusk-to-dawn: cube the cosine curve so night stays brief.
  return Math.pow((1 - Math.cos(phase * Math.PI * 2)) / 2, 2.2) * 0.9
}

export default function App() {
  const canvasRef = useRef<HTMLCanvasElement>(null)
  const sceneRef = useRef<CityScene | null>(null)

  // The authoritative game state is a mutable ref; React re-renders off a
  // version counter. This keeps the 60fps render loop away from React's
  // reconciler while the HUD still updates every frame that matters.
  const [initialState] = useState(() => createInitialState())
  const stateRef = useRef<GameState>(initialState)
  const [revision, forceRender] = useState(0)
  const [, pulse] = useState(0)
  const bump = useCallback(() => forceRender((n) => n + 1), [])

  const [rotation, setRotation] = useState<0 | 1 | 2 | 3>(0)
  const [hover, setHover] = useState<{ x: number; y: number } | null>(null)
  const [toast, setToast] = useState<{ text: string; tone: 'good' | 'warn' | 'honour' } | null>(null)
  const [started, setStarted] = useState(false)
  const [hasSave, setHasSave] = useState(false)
  const [confirmAction, setConfirmAction] = useState<'restart' | 'load' | 'title' | null>(null)
  const [helpOpen, setHelpOpen] = useState(false)
  const [settingsOpen, setSettingsOpen] = useState(false)
  const [honoursOpen, setHonoursOpen] = useState(false)
  const [newBest, setNewBest] = useState(false)
  const startedRef = useRef(false)
  const blockedRef = useRef(false)
  const toastTimer = useRef<number>()
  const dialogRef = useRef<HTMLDialogElement>(null)
  const recordedOutcome = useRef(false)
  const state = stateRef.current
  const modalOpen = Boolean(state.pendingEvent || state.draft || state.outcome)
  startedRef.current = started
  blockedRef.current = confirmAction !== null || helpOpen || settingsOpen || modalOpen
  const [rendererStatus, setRendererStatus] = useState<
    { kind: 'loading' | 'ready' | 'unavailable'; message?: string }
  >({ kind: 'loading' })

  const rotationRef = useRef(rotation)
  rotationRef.current = rotation

  useEffect(() => {
    setHasSave(loadFromStorage() !== null)
  }, [])

  // Browsers only allow audio after a gesture: unlock on the first one.
  useEffect(() => {
    const unlock = () => audio.unlock()
    window.addEventListener('pointerdown', unlock)
    window.addEventListener('keydown', unlock)
    return () => {
      window.removeEventListener('pointerdown', unlock)
      window.removeEventListener('keydown', unlock)
    }
  }, [])

  const flash = useCallback((text: string, tone: 'good' | 'warn' | 'honour' = 'good') => {
    setToast({ text, tone })
    window.clearTimeout(toastTimer.current)
    toastTimer.current = window.setTimeout(() => setToast(null), tone === 'honour' ? 4200 : 3200)
  }, [])

  const honours = useCallback(() => {
    const fresh = checkAchievements(stateRef.current)
    if (fresh.length) {
      audio.play('quest')
      flash(`Honour earned — ${fresh.map((a) => a.title).join(', ')}`, 'honour')
    }
  }, [flash])

  // ---- scene lifecycle ---------------------------------------------------

  useEffect(() => {
    if (!canvasRef.current) return

    const canvas = canvasRef.current
    const scene = createSceneSafely(() => new CityScene(canvas, {
      onCellClick: (x, y) => {
        if (!startedRef.current || blockedRef.current) return
        const state = stateRef.current
        if (!state.selectedInstanceId) {
          if (state.selectedAssetId) audio.play('click')
          state.selectedAssetId = null
          bump()
          return
        }
        if (TACTICS[state.instances[state.selectedInstanceId].definitionId]) {
          flash('Tactics are played from the card panel, not placed', 'warn')
          return
        }
        const def = definition(state.instances[state.selectedInstanceId].definitionId)
        const result = placeCard(state, state.selectedInstanceId, x, y, rotationRef.current)
        if (!result.ok) {
          audio.play('error')
          flash(result.reason ?? 'Cannot build there', 'warn')
        } else {
          audio.play('place')
          const placed = state.assets[state.assets.length - 1]
          sceneRef.current?.sync(state)
          sceneRef.current?.burst(placed.id, 0xd6bc80, 0.7)
          sceneRef.current?.floatText(placed.id, `−${def.deploymentCapital} Capital`, 'info')
          honours()
        }
        bump()
      },
      onAssetClick: (assetId) => {
        if (!startedRef.current || blockedRef.current) return
        const state = stateRef.current
        if (state.selectedInstanceId) {
          audio.play('error')
          flash('That ground is occupied', 'warn')
          return
        }
        audio.play('click')
        state.selectedAssetId = assetId
        bump()
      },
      onHover: (cell) => setHover(cell),
    }), (message) => setRendererStatus({ kind: 'unavailable', message }))

    if (!scene) return
    setRendererStatus({ kind: 'ready' })

    sceneRef.current = scene
    scene.sync(stateRef.current)
    scene.start()

    const onResize = () => scene.resize()
    window.addEventListener('resize', onResize)

    return () => {
      window.removeEventListener('resize', onResize)
      scene.dispose()
      sceneRef.current = null
    }
  }, [bump, flash, honours])

  useEffect(() => {
    if (!started) return
    let animation: ReturnType<typeof revealCommandInterface>
    const frame = requestAnimationFrame(() => { animation = revealCommandInterface() })
    return () => { cancelAnimationFrame(frame); animation?.revert() }
  }, [started])

  useEffect(() => {
    if (!toast) return
    let animation: ReturnType<typeof announceToast>
    const frame = requestAnimationFrame(() => { animation = announceToast() })
    return () => { cancelAnimationFrame(frame); animation?.revert() }
  }, [toast])

  // ---- simulation clock --------------------------------------------------

  const onCycle = useCallback(() => {
    const state = stateRef.current
    const before = {
      quests: state.quests.filter((q) => q.status === 'complete').length,
      event: state.pendingEvent,
      draft: state.draft,
      capital: state.resources.capital,
    }
    const report = advanceCycle(state)
    const scene = sceneRef.current
    const hall = state.assets[0]?.id ?? null
    audio.play('cycle')

    const delta = state.resources.capital - before.capital + (report.raid?.stolen ?? 0)
    if (Math.abs(delta) >= 0.5) {
      scene?.floatText(hall, `${delta >= 0 ? '+' : '−'}${fmt(Math.abs(delta))} Capital`, delta >= 0 ? 'gold' : 'bad')
      if (delta >= 1) audio.play('coin')
    }
    for (const id of report.completedAssets) {
      scene?.burst(id, 0xa8c8a2)
      scene?.floatText(id, 'Online', 'good')
    }
    if (report.completedAssets.length) audio.play('complete')
    if (report.raid) {
      if (report.raid.repelled) {
        audio.play('tactic')
        scene?.burst(hall, 0x4dd8e6, 2.6)
        scene?.floatText(hall, `Raid repelled · ${report.raid.defense} vs ${report.raid.strength}`, 'good')
        flash(`Ironheart raid repelled — ${report.raid.defense} Defense against ${report.raid.strength}.`)
      } else {
        audio.play('raid')
        scene?.shake(1.4)
        scene?.burst(hall, 0xf05252, 3.2)
        for (const asset of state.assets) if (state.sabotaged[asset.id] === 3) scene?.floatText(asset.id, 'Sabotaged', 'bad')
        flash(`Raid breached the district — ${report.raid.hit.join(', ') || 'no buildings'} sabotaged, ${report.raid.stolen} Capital seized.`, 'warn')
      }
    }
    const questsNow = state.quests.filter((q) => q.status === 'complete').length
    if (questsNow > before.quests) audio.play('quest')
    if (state.pendingEvent && state.pendingEvent !== before.event) audio.play('event')
    else if (state.draft && state.draft !== before.draft) audio.play('draft')
    if (state.outcome) audio.play(state.outcome.kind === 'victory' ? 'victory' : 'defeat')

    if (!state.outcome && saveToStorage(state)) setHasSave(true)
    honours()
    bump()
  }, [bump, flash, honours])

  useEffect(() => {
    if (!started) return
    let raf = 0
    let last = performance.now()
    let lastPulse = last

    const tick = (now: number) => {
      raf = requestAnimationFrame(tick)
      const state = stateRef.current
      const dt = Math.min(0.25, (now - last) / 1000)
      last = now
      const night = nightOf(state)
      sceneRef.current?.setNight(night)
      if (now - lastPulse >= 250) {
        audio.setMood({
          act: state.act,
          tension: state.act === 3 ? Math.min(1, state.threat / 100 + (state.flags.includes('overdrive') ? 0.2 : 0)) : Math.min(0.6, (state.strikes.insolvency + state.strikes.unrest) / 8),
          paused: state.speed === 0 || blockedRef.current,
          night,
        })
      }
      if (state.speed === 0 || document.hidden || blockedRef.current || state.outcome) return

      state.cycleProgress += (dt * state.speed) / CYCLE_SECONDS
      if (state.cycleProgress >= 1) {
        state.cycleProgress -= 1
        onCycle()
        state.cycleProgress = Math.min(state.cycleProgress, 0.99)
      } else if (now - lastPulse >= 100) {
        pulse((n) => n + 1)
        lastPulse = now
      }
    }

    raf = requestAnimationFrame(tick)
    return () => cancelAnimationFrame(raf)
  }, [started, onCycle])

  // ---- campaign actions --------------------------------------------------

  const decide = useCallback((index: number) => {
    const state = stateRef.current
    const event = state.pendingEvent ? EVENT_BY_ID[state.pendingEvent] : null
    const result = decideEvent(state, index)
    if (!result.ok) {
      audio.play('error')
      flash(result.reason ?? 'That choice is unavailable', 'warn')
      return
    }
    audio.play(event?.story ? 'quest' : 'select')
    if (state.outcome) audio.play(state.outcome.kind === 'victory' ? 'victory' : 'defeat')
    else if (state.draft) audio.play('draft')
    sceneRef.current?.sync(state)
    honours()
    bump()
  }, [bump, flash, honours])

  const pickDraft = useCallback((id: string | null) => {
    chooseDraft(stateRef.current, id)
    audio.play(id ? 'draft' : 'coin')
    if (id) flash(`${definition(id).displayName} added to the top of your deck.`)
    honours()
    bump()
  }, [bump, flash, honours])

  const playTactic = useCallback((instanceId: string) => {
    const state = stateRef.current
    const def = definition(state.instances[instanceId].definitionId)
    const result = playTacticCard(state, instanceId)
    if (!result.ok) {
      audio.play('error')
      flash(result.reason ?? 'Cannot play that tactic', 'warn')
      return
    }
    audio.play('tactic')
    sceneRef.current?.sync(state)
    sceneRef.current?.burst(state.assets[0]?.id ?? null, 0xb98cff, 2.2)
    sceneRef.current?.floatText(state.assets[0]?.id ?? null, def.displayName, 'good')
    flash(`${def.displayName}: ${TACTICS[def.id].text}`)
    bump()
  }, [bump, flash])

  // ---- keyboard ----------------------------------------------------------

  useEffect(() => {
    if (!started) return
    const onKey = (e: KeyboardEvent) => {
      if (e.defaultPrevented || e.repeat || e.ctrlKey || e.metaKey || e.altKey) return
      const target = e.target as HTMLElement | null
      if (target?.closest('input, select, textarea, [contenteditable="true"]')) return
      const state = stateRef.current
      // Decision shortcuts work inside the decision dialogs.
      if (state.pendingEvent && e.key >= '1' && e.key <= '9') {
        const event = EVENT_BY_ID[state.pendingEvent]
        const index = Number(e.key) - 1
        if (event && index < eventChoices(state, event).length) { e.preventDefault(); decide(index) }
        return
      }
      if (!state.pendingEvent && state.draft && e.key >= '1' && e.key <= '3') {
        const id = state.draft.options[Number(e.key) - 1]
        if (id) { e.preventDefault(); pickDraft(id) }
        return
      }
      if (blockedRef.current || target?.closest('dialog')) return
      if (e.key === ' ' && target?.closest('button, summary')) return
      if (e.key === 'r' || e.key === 'R') {
        setRotation((r) => ((r + 1) % 4) as 0 | 1 | 2 | 3)
        audio.play('click')
      } else if (e.key === 'Home') {
        e.preventDefault()
        sceneRef.current?.resetView()
      } else if (e.key === 'Escape') {
        state.selectedInstanceId = null
        state.selectedAssetId = null
        bump()
      } else if (e.key === ' ') {
        e.preventDefault()
        state.speed = state.speed === 0 ? 1 : 0
        audio.play('click')
        bump()
      } else if ((e.key === 't' || e.key === 'T' || e.key === 'Enter') && state.selectedInstanceId &&
        TACTICS[state.instances[state.selectedInstanceId].definitionId]) {
        e.preventDefault()
        playTactic(state.selectedInstanceId)
      } else if (e.key >= '1' && e.key <= '6') {
        const index = Number(e.key) - 1
        const id = state.hand[index]
        if (id) {
          state.selectedInstanceId = state.selectedInstanceId === id ? null : id
          state.selectedAssetId = null
          audio.play('select')
          bump()
        }
      }
    }
    window.addEventListener('keydown', onKey)
    return () => window.removeEventListener('keydown', onKey)
  }, [started, bump, decide, pickDraft, playTactic])

  // ---- ghost preview -----------------------------------------------------

  const selectedDef = state.selectedInstanceId
    ? definition(state.instances[state.selectedInstanceId].definitionId)
    : null
  const selectedIsBuilding = Boolean(selectedDef && !TACTICS[selectedDef.id])

  useEffect(() => {
    const scene = sceneRef.current
    if (!scene) return
    if (!selectedDef || !selectedIsBuilding || !hover) {
      scene.setGhost([1, 1], 0, false, true)
      return
    }
    const check = previewPlacement(stateRef.current, stateRef.current.selectedInstanceId!, hover.x, hover.y, rotation)
    scene.setGhost(selectedDef.footprint, rotation, true, check.ok)
  }, [selectedDef, selectedIsBuilding, hover, rotation, revision])

  useEffect(() => {
    sceneRef.current?.sync(stateRef.current)
  }, [revision])

  // ---- projected income for the HUD --------------------------------------

  const income = useMemo(() => projectNextCycle(stateRef.current), [revision])
  const tip = useMemo(() => advisorTip(stateRef.current, income.capital), [revision, income])
  const placement = useMemo(() => selectedIsBuilding && hover && stateRef.current.selectedInstanceId
    ? previewPlacement(stateRef.current, stateRef.current.selectedInstanceId, hover.x, hover.y, rotation)
    : null, [selectedIsBuilding, hover, rotation, revision])

  useEffect(() => {
    if (confirmAction || helpOpen) dialogRef.current?.showModal()
    else dialogRef.current?.close()
  }, [confirmAction, helpOpen])

  // Record the campaign result once, when it ends.
  useEffect(() => {
    const state = stateRef.current
    if (!state.outcome || recordedOutcome.current) return
    recordedOutcome.current = true
    const result = recordOutcome(state)
    setNewBest(result.best)
    if (result.unlocked.length) flash(`Honour earned — ${result.unlocked.map((a) => a.title).join(', ')}`, 'honour')
  }, [revision, flash])

  useEffect(() => () => window.clearTimeout(toastTimer.current), [])

  // ---- campaign management -----------------------------------------------

  const resetView = () => {
    setRotation(0)
    setHover(null)
    sceneRef.current?.resetView()
    sceneRef.current?.sync(stateRef.current)
  }

  const beginCampaign = (difficulty: Difficulty) => {
    stateRef.current = createInitialState(difficulty)
    recordedOutcome.current = false
    setNewBest(false)
    recordCampaignStart()
    resetView()
    audio.play('quest')
    setStarted(true)
    bump()
  }

  const handleSave = () => {
    const saved = saveToStorage(stateRef.current)
    flash(saved ? 'Campaign saved' : 'Save unavailable in this browser', saved ? 'good' : 'warn')
    if (saved) setHasSave(true)
  }

  const handleLoad = () => {
    const loaded = loadFromStorage()
    if (!loaded) {
      flash('No saved campaign found', 'warn')
      return false
    }
    stateRef.current = loaded
    recordedOutcome.current = Boolean(loaded.outcome)
    resetView()
    flash('Campaign loaded and paused. Choose a speed to resume.')
    bump()
    return true
  }

  const toTitle = () => {
    setStarted(false)
    setHasSave(loadFromStorage() !== null)
    bump()
  }

  return (
    <div className="app">
      <canvas ref={canvasRef} className="viewport" aria-label="Ashcroft Basin city. Drag to orbit, scroll to zoom. Select a card then click a vacant grid cell to build." />

      {!started ? (
        <Intro
          onStart={beginCampaign}
          onContinue={hasSave ? () => { if (handleLoad()) { audio.play('quest'); setStarted(true) } } : undefined}
          onSettings={() => setSettingsOpen(true)}
          onHonours={() => setHonoursOpen(true)}
          rendererStatus={rendererStatus}
        />
      ) : (
        <>

      <Hud
        state={state}
        income={income}
        hasSave={hasSave}
        onSpeed={(speed) => {
          state.speed = speed
          audio.play('click')
          bump()
        }}
        onSave={handleSave}
        onLoad={() => setConfirmAction('load')}
        onRestart={() => setConfirmAction('restart')}
        onSettings={() => setSettingsOpen(true)}
        onTitle={() => setConfirmAction('title')}
      />

      <aside className="rail rail-left">
        <QuestPanel state={state} />
        <OverlayPanel
          state={state}
          onOverlay={(id: OverlayId) => {
            state.overlay = id
            audio.play('click')
            sceneRef.current?.sync(state)
            bump()
          }}
        />
        <AdvisorStrip tip={tip} />
      </aside>

      <aside className="rail rail-right">
        <CommandPanel
          state={state}
          onPressure={() => {
            const result = influencePressure(state)
            audio.play(result.ok ? 'tactic' : 'error')
            if (!result.ok) flash(result.reason ?? 'Unavailable', 'warn')
            else sceneRef.current?.floatText(state.assets[0]?.id ?? null, '−8 Dominance', 'good')
            bump()
          }}
          onGarrison={() => {
            const result = garrison(state)
            audio.play(result.ok ? 'tactic' : 'error')
            if (!result.ok) flash(result.reason ?? 'Unavailable', 'warn')
            else sceneRef.current?.floatText(state.assets[0]?.id ?? null, 'Garrison hired', 'good')
            bump()
          }}
          onMarket={() => {
            const result = marketDraft(state)
            audio.play(result.ok ? 'draft' : 'error')
            if (!result.ok) flash(result.reason ?? 'Unavailable', 'warn')
            bump()
          }}
        />
        {state.selectedAssetId ? (
          <InspectPanel
            state={state}
            onDemolish={(assetId) => {
              const result = demolish(state, assetId)
              if (!result.ok) {
                audio.play('error')
                flash(result.reason ?? 'Cannot decommission', 'warn')
              } else audio.play('place')
              sceneRef.current?.sync(state)
              bump()
            }}
            onClose={() => {
              state.selectedAssetId = null
              bump()
            }}
          />
        ) : (
          <LogPanel state={state} />
        )}
      </aside>

      <CardHand
        state={state}
        rotation={rotation}
        onRotate={() => { audio.play('click'); setRotation((r) => ((r + 1) % 4) as 0 | 1 | 2 | 3) }}
        onSelect={(id) => {
          state.selectedInstanceId = id
          if (id) state.selectedAssetId = null
          audio.play(id ? 'select' : 'click')
          bump()
        }}
        onCycleCard={(id) => {
          cycleCard(state, id)
          audio.play('draft')
          bump()
        }}
        onPlayTactic={playTactic}
      />

      <div className="view-tools" role="group" aria-label="View controls">
        <button onClick={() => sceneRef.current?.resetView()} title="Reset camera (Home)">Recenter</button>
        <button onClick={() => setHelpOpen(true)}>Field guide</button>
      </div>

      {state.speed === 0 && !modalOpen && <div className="pause-status" role="status">Paused · plan your next move</div>}

      {selectedDef && selectedIsBuilding && (
        <div className={`place-hint ${placement && (!placement.ok || placement.warnings.length) ? 'placement-warning' : ''}`} title={placement?.warnings.join(' · ')}>
          <strong>{selectedDef.displayName}</strong>
          <span>{placement ? (placement.ok ? (placement.warnings.length ? placement.warnings[0] : `Site ready · ${hover!.x + 1}, ${hover!.y + 1}`) : placement.reason) : 'Choose a vacant site near your utilities'}</span>
          <button onClick={() => { state.selectedInstanceId = null; bump() }}>Cancel placement</button>
        </div>
      )}

      {toast && <div className={`toast ${toast.tone}`} role="status" aria-live="polite">{toast.text}</div>}

      {state.pendingEvent ? <EventModal key={state.pendingEvent} state={state} onChoose={decide} />
        : state.draft ? <DraftModal key={state.draft.options.join()} state={state} onPick={pickDraft} />
          : state.outcome ? <OutcomeModal
            state={state}
            isBest={newBest}
            onNew={() => beginCampaign(state.difficulty)}
            onTitle={toTitle}
            onLoad={state.outcome.kind === 'defeat' && hasSave ? () => { handleLoad() } : undefined}
          /> : null}

      <dialog ref={dialogRef} className="command-dialog" onCancel={() => { setConfirmAction(null); setHelpOpen(false) }} aria-labelledby="dialog-title">
        <h2 id="dialog-title">{helpOpen ? 'Your field guide' : confirmAction === 'restart' ? 'Begin a new campaign?' : confirmAction === 'title' ? 'Return to the title screen?' : 'Restore your checkpoint?'}</h2>
        {helpOpen ? <>
          <p>Build a self-sufficient capital through three acts: the First Hour, the Regional Crisis, and Iron at the Border — where Forge Lord Daxton Rhe will try to take Ashcroft.</p>
          <ol>
            <li>Start with power and water. Coverage extends six cells from an operational utility.</li>
            <li>Select a card, then choose an empty site. Rotate before placement. Units and leaders are <b>tactics</b>: select them and press Play.</li>
            <li>Balance housing with jobs. Commerce must cover maintenance — an empty treasury for too long ends the campaign.</li>
            <li>Every objective earns a card draft. The Basin Market sells more. Crises ask you to choose; every choice has a cost.</li>
            <li>In Act III, Ironheart Dominance rises each cycle. Defense buildings, garrisons, tactics and Influence push it back. At 100%, Ashcroft falls.</li>
          </ol>
          <p>Drag to orbit · right-drag to pan · scroll or pinch to zoom · R rotates · 1–6 selects a card · T plays a tactic · Space pauses · Home recenters · 1–3 answer decisions.</p>
          <p>The campaign autosaves after each cycle. Restored games begin paused.</p>
        </> : <p>{confirmAction === 'restart' ? 'Your current city will be replaced with a new campaign at the same difficulty. Save it first if you want to keep it.' : confirmAction === 'title' ? 'Your campaign autosaved at the end of the last cycle. You can continue it from the title screen.' : 'This replaces the current city with your last saved checkpoint.'}</p>}
        <div className="dialog-actions">
          <button autoFocus onClick={() => { setConfirmAction(null); setHelpOpen(false) }}>{helpOpen ? 'Return to city' : 'Keep playing'}</button>
          {!helpOpen && <button className="primary" onClick={() => {
            if (confirmAction === 'restart') beginCampaign(state.difficulty)
            else if (confirmAction === 'title') { saveToStorage(stateRef.current); toTitle() }
            else handleLoad()
            setConfirmAction(null)
          }}>{confirmAction === 'restart' ? 'Start new campaign' : confirmAction === 'title' ? 'Return to title' : 'Restore checkpoint'}</button>}
        </div>
      </dialog>
        </>
      )}

      {settingsOpen && <SettingsModal onClose={() => setSettingsOpen(false)} onReplayGuide={started ? () => { setSettingsOpen(false); setHelpOpen(true) } : undefined} />}
      {honoursOpen && <HonoursModal onClose={() => setHonoursOpen(false)} />}
    </div>
  )
}
