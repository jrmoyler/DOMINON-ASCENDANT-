import { useCallback, useEffect, useMemo, useRef, useState } from 'react'
import { CityScene } from '@/render/Scene'
import { createSceneSafely } from '@/render/createSceneSafely'
import { definition } from '@/game/content'
import { CYCLE_SECONDS, resolveCycle } from '@/game/economy'
import { canPlace } from '@/game/grid'
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
  const stateRef = useRef<GameState>(createInitialState())
  const [, forceRender] = useState(0)
  const bump = useCallback(() => forceRender((n) => n + 1), [])

  const [rotation, setRotation] = useState<0 | 1 | 2 | 3>(0)
  const [hover, setHover] = useState<{ x: number; y: number } | null>(null)
  const [toast, setToast] = useState<string | null>(null)
  const [started, setStarted] = useState(false)
  const [hasSave, setHasSave] = useState(false)
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
    window.setTimeout(() => setToast((t) => (t === message ? null : t)), 2600)
  }, [])

  // ---- scene lifecycle ---------------------------------------------------

  useEffect(() => {
    if (!canvasRef.current) return

    const canvas = canvasRef.current
    const scene = createSceneSafely(() => new CityScene(canvas, {
      onCellClick: (x, y) => {
        const state = stateRef.current
        if (!state.selectedInstanceId) {
          state.selectedAssetId = null
          bump()
          return
        }
        const result = placeCard(state, state.selectedInstanceId, x, y, rotationRef.current)
        if (!result.ok) flash(result.reason ?? 'Cannot build there')
        bump()
      },
      onAssetClick: (assetId) => {
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
    const frame = requestAnimationFrame(revealCommandInterface)
    return () => cancelAnimationFrame(frame)
  }, [started])

  useEffect(() => {
    if (!toast) return
    const frame = requestAnimationFrame(announceToast)
    return () => cancelAnimationFrame(frame)
  }, [toast])

  // ---- simulation clock --------------------------------------------------

  useEffect(() => {
    if (!started) return
    let raf = 0
    let last = performance.now()

    const tick = (now: number) => {
      raf = requestAnimationFrame(tick)
      const state = stateRef.current
      const dt = Math.min(0.25, (now - last) / 1000)
      last = now
      if (state.speed === 0) return

      state.cycleProgress += (dt * state.speed) / CYCLE_SECONDS
      let advanced = false
      while (state.cycleProgress >= 1) {
        state.cycleProgress -= 1
        advanceCycle(state)
        advanced = true
      }
      if (advanced) sceneRef.current?.sync(state)
      bump()
    }

    raf = requestAnimationFrame(tick)
    return () => cancelAnimationFrame(raf)
  }, [started, bump])

  // ---- keyboard ----------------------------------------------------------

  useEffect(() => {
    if (!started) return
    const onKey = (e: KeyboardEvent) => {
      const state = stateRef.current
      if (e.key === 'r' || e.key === 'R') {
        setRotation((r) => ((r + 1) % 4) as 0 | 1 | 2 | 3)
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
    const check = canPlace(stateRef.current.assets, hover.x, hover.y, selectedDef.footprint, rotation)
    const affordable = stateRef.current.resources.capital >= selectedDef.deploymentCapital
    scene.setGhost(selectedDef.footprint, rotation, true, check.ok && affordable)
  }, [selectedDef, hover, rotation])

  useEffect(() => {
    sceneRef.current?.sync(stateRef.current)
  })

  // ---- projected income for the HUD --------------------------------------

  const income = useMemo(() => {
    // resolveCycle mutates construction counters, so project on a clone.
    const clone: GameState = {
      ...state,
      assets: state.assets.map((a) => ({ ...a })),
      log: [],
    }
    const result = resolveCycle(clone)
    return {
      capital: result.capitalDelta,
      insight: result.insightDelta,
      influence: result.influenceDelta,
    }
    // Recomputed whenever the simulation advances or the city changes.
  }, [state, state.cycle, state.assets.length, state.population])

  // ---- actions -----------------------------------------------------------

  const handleRestart = () => {
    stateRef.current = createInitialState()
    setRotation(0)
    sceneRef.current?.sync(stateRef.current)
    bump()
  }

  const handleSave = () => {
    flash(saveToStorage(stateRef.current) ? 'Campaign saved' : 'Save unavailable in this browser')
    setHasSave(true)
  }

  const handleLoad = () => {
    const loaded = loadFromStorage()
    if (!loaded) {
      flash('No saved campaign found')
      return
    }
    stateRef.current = loaded
    sceneRef.current?.sync(loaded)
    flash('Campaign loaded')
    bump()
  }

  return (
    <div className="app">
      <canvas ref={canvasRef} className="viewport" />

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
        onLoad={handleLoad}
        onRestart={handleRestart}
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

      {selectedDef && (
        <div className="place-hint">
          Placing <strong>{selectedDef.displayName}</strong> — click a cell. R rotates, Esc cancels.
        </div>
      )}

      {toast && <div className="toast">{toast}</div>}
        </>
      )}
    </div>
  )
}
