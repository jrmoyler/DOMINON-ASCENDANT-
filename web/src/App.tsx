import { useCallback, useEffect, useMemo, useRef, useState } from 'react'
import { CityScene } from '@/render/Scene'
import { createSceneSafely } from '@/render/createSceneSafely'
import { definition } from '@/game/content'
import { CYCLE_SECONDS } from '@/game/economy'
import { previewPlacement, projectNextCycle } from '@/game/planning'
import {
  advanceCycle,
  createInitialState,
  cycleCard,
  demolish,
  loadFromStorage,
  placeCard,
  saveToStorage,
} from '@/game/state'
import type { GameState, OverlayId } from '@/game/types'
import { Hud } from '@/ui/Hud'
import { CardHand } from '@/ui/CardHand'
import { InspectPanel, LogPanel, OverlayPanel, QuestPanel } from '@/ui/SidePanels'
import { Intro } from '@/ui/Intro'
import { announceToast, revealCommandInterface } from '@/ui/motion'

export default function App() {
  const canvasRef = useRef<HTMLCanvasElement>(null)
  const sceneRef = useRef<CityScene | null>(null)

  // The authoritative game state is a mutable ref; React re-renders off a
  // version counter. This keeps the 60fps render loop away from React's
  // reconciler while the HUD still updates every frame that matters.
  const [initialState] = useState(createInitialState)
  const stateRef = useRef<GameState>(initialState)
  const [revision, forceRender] = useState(0)
  const [, pulse] = useState(0)
  const bump = useCallback(() => forceRender((n) => n + 1), [])

  const [rotation, setRotation] = useState<0 | 1 | 2 | 3>(0)
  const [hover, setHover] = useState<{ x: number; y: number } | null>(null)
  const [toast, setToast] = useState<string | null>(null)
  const [started, setStarted] = useState(false)
  const [hasSave, setHasSave] = useState(false)
  const [confirmAction, setConfirmAction] = useState<'restart' | 'load' | null>(null)
  const [helpOpen, setHelpOpen] = useState(false)
  const [victoryOpen, setVictoryOpen] = useState(false)
  const previouslyAscended = useRef(false)
  const startedRef = useRef(false)
  const blockedRef = useRef(false)
  const toastTimer = useRef<number>()
  const dialogRef = useRef<HTMLDialogElement>(null)
  startedRef.current = started
  blockedRef.current = confirmAction !== null || helpOpen || victoryOpen
  const [rendererStatus, setRendererStatus] = useState<
    { kind: 'loading' | 'ready' | 'unavailable'; message?: string }
  >({ kind: 'loading' })

  const rotationRef = useRef(rotation)
  rotationRef.current = rotation

  useEffect(() => {
    setHasSave(loadFromStorage() !== null)
  }, [])

  const flash = useCallback((message: string) => {
    setToast(message)
    window.clearTimeout(toastTimer.current)
    toastTimer.current = window.setTimeout(() => setToast(null), 3200)
  }, [])

  // ---- scene lifecycle ---------------------------------------------------

  useEffect(() => {
    if (!canvasRef.current) return

    const canvas = canvasRef.current
    const scene = createSceneSafely(() => new CityScene(canvas, {
      onCellClick: (x, y) => {
        if (!startedRef.current || blockedRef.current) return
        const state = stateRef.current
        if (!state.selectedInstanceId) {
          state.selectedAssetId = null
          bump()
          return
        }
        const result = placeCard(state, state.selectedInstanceId, x, y, rotationRef.current)
        if (!result.ok) flash(result.reason ?? 'Cannot build there')
        else flash('Construction commissioned. Your next card is ready.')
        bump()
      },
      onAssetClick: (assetId) => {
        if (!startedRef.current || blockedRef.current) return
        const state = stateRef.current
        if (state.selectedInstanceId) {
          flash('That ground is occupied')
          return
        }
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
  }, [bump, flash])

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
      if (state.speed === 0 || document.hidden || blockedRef.current) return

      state.cycleProgress += (dt * state.speed) / CYCLE_SECONDS
      let advanced = false
      while (state.cycleProgress >= 1) {
        state.cycleProgress -= 1
        advanceCycle(state)
        advanced = true
      }
      if (advanced) {
        if (saveToStorage(state)) setHasSave(true)
        bump()
      } else if (now - lastPulse >= 100) {
        pulse((n) => n + 1)
        lastPulse = now
      }
    }

    raf = requestAnimationFrame(tick)
    return () => cancelAnimationFrame(raf)
  }, [started, bump])

  // ---- keyboard ----------------------------------------------------------

  useEffect(() => {
    if (!started) return
    const onKey = (e: KeyboardEvent) => {
      if (e.defaultPrevented || e.repeat || e.ctrlKey || e.metaKey || e.altKey || blockedRef.current) return
      const target = e.target as HTMLElement | null
      if (target?.closest('input, select, textarea, dialog, [contenteditable="true"]')) return
      if (e.key === ' ' && target?.closest('button, summary')) return
      const state = stateRef.current
      if (e.key === 'r' || e.key === 'R') {
        setRotation((r) => ((r + 1) % 4) as 0 | 1 | 2 | 3)
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
        bump()
      } else if (e.key >= '1' && e.key <= '6') {
        const index = Number(e.key) - 1
        const id = state.hand[index]
        if (id) {
          state.selectedInstanceId = state.selectedInstanceId === id ? null : id
          state.selectedAssetId = null
          bump()
        }
      }
    }
    window.addEventListener('keydown', onKey)
    return () => window.removeEventListener('keydown', onKey)
  }, [started, bump])

  // ---- ghost preview -----------------------------------------------------

  const state = stateRef.current
  const selectedDef = state.selectedInstanceId
    ? definition(state.instances[state.selectedInstanceId].definitionId)
    : null

  useEffect(() => {
    const scene = sceneRef.current
    if (!scene) return
    if (!selectedDef || !hover) {
      scene.setGhost([1, 1], 0, false, true)
      return
    }
    const check = previewPlacement(stateRef.current, stateRef.current.selectedInstanceId!, hover.x, hover.y, rotation)
    scene.setGhost(selectedDef.footprint, rotation, true, check.ok)
  }, [selectedDef, hover, rotation, revision])

  useEffect(() => {
    sceneRef.current?.sync(stateRef.current)
  }, [revision])

  // ---- projected income for the HUD --------------------------------------

  const income = useMemo(() => projectNextCycle(stateRef.current), [revision])
  const placement = useMemo(() => selectedDef && hover && stateRef.current.selectedInstanceId
    ? previewPlacement(stateRef.current, stateRef.current.selectedInstanceId, hover.x, hover.y, rotation)
    : null, [selectedDef, hover, rotation, revision])

  useEffect(() => {
    if (confirmAction || helpOpen || victoryOpen) dialogRef.current?.showModal()
    else dialogRef.current?.close()
  }, [confirmAction, helpOpen, victoryOpen])

  useEffect(() => {
    const ascended = stateRef.current.ascended
    if (started && ascended && !previouslyAscended.current) setVictoryOpen(true)
    previouslyAscended.current = ascended
  }, [revision, started])

  useEffect(() => () => window.clearTimeout(toastTimer.current), [])

  // ---- actions -----------------------------------------------------------

  const handleRestart = () => {
    stateRef.current = createInitialState()
    setRotation(0)
    setHover(null)
    sceneRef.current?.resetView()
    sceneRef.current?.sync(stateRef.current)
    bump()
  }

  const handleSave = () => {
    const saved = saveToStorage(stateRef.current)
    flash(saved ? 'Campaign saved' : 'Save unavailable in this browser')
    if (saved) setHasSave(true)
  }

  const handleLoad = () => {
    const loaded = loadFromStorage()
    if (!loaded) {
      flash('No saved campaign found')
      return
    }
    stateRef.current = loaded
    sceneRef.current?.sync(loaded)
    setRotation(0)
    setHover(null)
    sceneRef.current?.resetView()
    flash('Campaign loaded and paused. Choose a speed to resume.')
    bump()
  }

  return (
    <div className="app">
      <canvas ref={canvasRef} className="viewport" aria-label="Ashcroft Basin city. Drag to orbit, scroll to zoom. Select a card then click a vacant grid cell to build." />

      {!started ? (
        <Intro
          onStart={() => setStarted(true)}
          onContinue={hasSave ? () => { handleLoad(); setStarted(true) } : undefined}
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
          bump()
        }}
        onSave={handleSave}
        onLoad={() => setConfirmAction('load')}
        onRestart={() => setConfirmAction('restart')}
      />

      <aside className="rail rail-left">
        <QuestPanel state={state} />
        <OverlayPanel
          state={state}
          onOverlay={(id: OverlayId) => {
            state.overlay = id
            sceneRef.current?.sync(state)
            bump()
          }}
        />
      </aside>

      <aside className="rail rail-right">
        {state.selectedAssetId ? (
          <InspectPanel
            state={state}
            onDemolish={(assetId) => {
              const result = demolish(state, assetId)
              if (!result.ok) flash(result.reason ?? 'Cannot decommission')
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
        onRotate={() => setRotation((r) => ((r + 1) % 4) as 0 | 1 | 2 | 3)}
        onSelect={(id) => {
          state.selectedInstanceId = id
          if (id) state.selectedAssetId = null
          bump()
        }}
        onCycleCard={(id) => {
          cycleCard(state, id)
          bump()
        }}
      />

      <div className="view-tools" role="group" aria-label="View controls">
        <button onClick={() => sceneRef.current?.resetView()} title="Reset camera (Home)">Recenter</button>
        <button onClick={() => setHelpOpen(true)}>Field guide</button>
      </div>

      {state.speed === 0 && <div className="pause-status" role="status">Paused · plan your next move</div>}

      {selectedDef && (
        <div className={`place-hint ${placement && (!placement.ok || placement.warnings.length) ? 'placement-warning' : ''}`} title={placement?.warnings.join(' · ')}>
          <strong>{selectedDef.displayName}</strong>
          <span>{placement ? (placement.ok ? (placement.warnings.length ? placement.warnings[0] : `Site ready · ${hover!.x + 1}, ${hover!.y + 1}`) : placement.reason) : 'Choose a vacant site near your utilities'}</span>
          <button onClick={() => { state.selectedInstanceId = null; bump() }}>Cancel placement</button>
        </div>
      )}

      {toast && <div className="toast" role="status" aria-live="polite">{toast}</div>}
      <dialog ref={dialogRef} className="command-dialog" onCancel={() => { setConfirmAction(null); setHelpOpen(false); setVictoryOpen(false) }} aria-labelledby="dialog-title">
        <h2 id="dialog-title">{victoryOpen ? 'Forgeweave Ascension' : helpOpen ? 'Your field guide' : confirmAction === 'restart' ? 'Begin a new campaign?' : 'Restore your checkpoint?'}</h2>
        {victoryOpen ? <>
          <p><strong>CONVERGENCE AUTHORITY: 1/20</strong></p>
          <p>You completed every First Hour objective. Your city has earned its first Ascension.</p>
          <p>{state.population} citizens · {state.assets.length} city assets · {state.cycle} Development Cycles</p>
          <p>Continue shaping the basin, or begin a new campaign from Menu.</p>
        </> : helpOpen ? <>
          <p>Build a self-sufficient capital, complete your active objectives, and earn Forgeweave Ascension.</p>
          <ol>
            <li>Start with power and water. Coverage extends six cells from an operational utility.</li>
            <li>Select a card, then choose an empty site. Read all three costs. Rotate before placement.</li>
            <li>Balance housing with jobs. Commercial income must cover maintenance.</li>
            <li>Use the service overlays to diagnose shortfalls, and the objective panel to guide expansion.</li>
          </ol>
          <p>Drag to orbit · right-drag to pan · scroll or pinch to zoom · R rotates · 1–6 selects a card · Space pauses · Home recenters.</p>
          <p>The campaign saves after each cycle. You can also save from Campaign controls. Restored games begin paused.</p>
        </> : <p>{confirmAction === 'restart' ? 'Your current city will be replaced. Save it first from Campaign controls if you want to keep it. The new campaign will autosave after its first cycle.' : 'This replaces the current city with your last saved checkpoint.'}</p>}
        <div className="dialog-actions">
          <button autoFocus onClick={() => { setConfirmAction(null); setHelpOpen(false); setVictoryOpen(false) }}>{helpOpen || victoryOpen ? 'Return to city' : 'Keep playing'}</button>
          {!helpOpen && !victoryOpen && <button onClick={() => { if (confirmAction === 'restart') handleRestart(); else handleLoad(); setConfirmAction(null) }}>{confirmAction === 'restart' ? 'Start new campaign' : 'Restore checkpoint'}</button>}
        </div>
      </dialog>
        </>
      )}
    </div>
  )
}
