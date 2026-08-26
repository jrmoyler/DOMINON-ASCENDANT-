import * as THREE from 'three'
import { CELL_METRES, GRID_SIZE, cellsOf, rotatedFootprint } from '@/game/grid'
import { UTILITY_RADIUS, computeServices } from '@/game/economy'
import { definition } from '@/game/content'
import type { GameState, WorldAsset } from '@/game/types'
import { PALETTE } from './palette'
import { createBuildingMesh, createGhostMesh } from './buildings'

const WORLD = GRID_SIZE * CELL_METRES

export interface SceneCallbacks {
  onCellClick: (x: number, y: number) => void
  onAssetClick: (assetId: string) => void
  onHover: (cell: { x: number; y: number } | null) => void
}

/** Grid cell -> world-space centre. */
function cellCentre(x: number, y: number): THREE.Vector3 {
  return new THREE.Vector3(
    (x + 0.5) * CELL_METRES - WORLD / 2,
    0,
    (y + 0.5) * CELL_METRES - WORLD / 2,
  )
}

export class CityScene {
  private renderer: THREE.WebGLRenderer
  private scene = new THREE.Scene()
  private camera: THREE.PerspectiveCamera
  private raycaster = new THREE.Raycaster()
  private pointer = new THREE.Vector2()
  private ground: THREE.Mesh
  private buildingRoot = new THREE.Group()
  private overlayRoot = new THREE.Group()
  private ghost: THREE.Mesh
  private selectionRing: THREE.Mesh
  private meshes = new Map<string, THREE.Group>()
  private frame = 0
  private disposed = false

  // Camera rig: orbit around a target on the ground plane.
  private target = new THREE.Vector3(0, 0, 0)
  private orbit = { azimuth: Math.PI * 0.25, polar: Math.PI * 0.34, distance: 140 }
  private dragging: 'none' | 'orbit' | 'pan' = 'none'
  private lastPointer = { x: 0, y: 0 }
  private movedWhileDown = false

  private hoverCell: { x: number; y: number } | null = null
  private ghostFootprint: [number, number] = [1, 1]
  private ghostValid = true
  private ghostVisible = false

  constructor(
    private canvas: HTMLCanvasElement,
    private callbacks: SceneCallbacks,
  ) {
    this.renderer = new THREE.WebGLRenderer({
      canvas,
      antialias: true,
      powerPreference: 'high-performance',
    })
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2))
    this.renderer.shadowMap.enabled = true
    this.renderer.shadowMap.type = THREE.PCFSoftShadowMap

    this.scene.background = new THREE.Color(PALETTE.background)
    this.scene.fog = new THREE.Fog(PALETTE.fog, WORLD * 0.9, WORLD * 2.4)

    this.camera = new THREE.PerspectiveCamera(45, 1, 1, 3000)

    // Ground plate.
    this.ground = new THREE.Mesh(
      new THREE.PlaneGeometry(WORLD, WORLD),
      new THREE.MeshStandardMaterial({ color: PALETTE.ground, roughness: 0.96, metalness: 0.02 }),
    )
    this.ground.rotation.x = -Math.PI / 2
    this.ground.receiveShadow = true
    this.scene.add(this.ground)

    // Surrounding terrain so the city does not float in the void.
    const apron = new THREE.Mesh(
      new THREE.PlaneGeometry(WORLD * 4, WORLD * 4),
      new THREE.MeshStandardMaterial({ color: PALETTE.groundEdge, roughness: 1 }),
    )
    apron.rotation.x = -Math.PI / 2
    apron.position.y = -0.4
    this.scene.add(apron)

    this.scene.add(this.buildGrid())
    this.scene.add(this.buildingRoot)
    this.scene.add(this.overlayRoot)

    this.ghost = createGhostMesh()
    this.ghost.visible = false
    this.scene.add(this.ghost)

    this.selectionRing = new THREE.Mesh(
      new THREE.RingGeometry(CELL_METRES * 0.55, CELL_METRES * 0.72, 32),
      new THREE.MeshBasicMaterial({
        color: PALETTE.selection,
        side: THREE.DoubleSide,
        transparent: true,
        opacity: 0.9,
      }),
    )
    this.selectionRing.rotation.x = -Math.PI / 2
    this.selectionRing.position.y = 0.3
    this.selectionRing.visible = false
    this.scene.add(this.selectionRing)

    this.addLights()
    this.attachEvents()
    this.resize()
    this.updateCamera()
  }

  private addLights() {
    this.scene.add(new THREE.HemisphereLight(PALETTE.keyLight, PALETTE.fillLight, 0.55))

    const key = new THREE.DirectionalLight(PALETTE.keyLight, 1.35)
    key.position.set(WORLD * 0.5, WORLD * 0.85, WORLD * 0.35)
    key.castShadow = true
    key.shadow.mapSize.set(2048, 2048)
    const extent = WORLD * 0.75
    key.shadow.camera.left = -extent
    key.shadow.camera.right = extent
    key.shadow.camera.top = extent
    key.shadow.camera.bottom = -extent
    key.shadow.camera.near = 10
    key.shadow.camera.far = WORLD * 3
    key.shadow.bias = -0.0009
    this.scene.add(key)

    const rim = new THREE.DirectionalLight(PALETTE.rim, 0.35)
    rim.position.set(-WORLD * 0.4, WORLD * 0.3, -WORLD * 0.5)
    this.scene.add(rim)
  }

  private buildGrid(): THREE.Group {
    const group = new THREE.Group()
    const minor: number[] = []
    const major: number[] = []
    const half = WORLD / 2

    for (let i = 0; i <= GRID_SIZE; i++) {
      const p = i * CELL_METRES - half
      const target = i % 8 === 0 ? major : minor
      target.push(p, 0, -half, p, 0, half)
      target.push(-half, 0, p, half, 0, p)
    }

    const make = (points: number[], color: number, opacity: number) => {
      const geometry = new THREE.BufferGeometry()
      geometry.setAttribute('position', new THREE.Float32BufferAttribute(points, 3))
      const line = new THREE.LineSegments(
        geometry,
        new THREE.LineBasicMaterial({ color, transparent: true, opacity }),
      )
      line.position.y = 0.06
      return line
    }

    group.add(make(minor, PALETTE.gridLine, 0.5))
    group.add(make(major, PALETTE.gridMajor, 0.85))
    return group
  }

  // ---- interaction -------------------------------------------------------

  private attachEvents() {
    const c = this.canvas
    c.addEventListener('pointerdown', this.onPointerDown)
    c.addEventListener('pointermove', this.onPointerMove)
    window.addEventListener('pointerup', this.onPointerUp)
    c.addEventListener('wheel', this.onWheel, { passive: false })
    c.addEventListener('contextmenu', (e) => e.preventDefault())
    c.addEventListener('pointerleave', () => {
      this.hoverCell = null
      this.ghost.visible = false
      this.callbacks.onHover(null)
    })
  }

  private onPointerDown = (event: PointerEvent) => {
    this.canvas.setPointerCapture?.(event.pointerId)
    this.movedWhileDown = false
    this.lastPointer = { x: event.clientX, y: event.clientY }
    // Right button or Shift pans; anything else orbits while dragging.
    // A left press that never moves is treated as a click on pointer-up.
    this.dragging = event.button === 2 || event.shiftKey ? 'pan' : 'orbit'
  }

  private onPointerMove = (event: PointerEvent) => {
    const dx = event.clientX - this.lastPointer.x
    const dy = event.clientY - this.lastPointer.y
    if (Math.abs(dx) + Math.abs(dy) > 3) this.movedWhileDown = true

    if (this.dragging === 'orbit') {
      this.orbit.azimuth -= dx * 0.005
      this.orbit.polar = THREE.MathUtils.clamp(this.orbit.polar - dy * 0.005, 0.12, Math.PI / 2.15)
      this.updateCamera()
    } else if (this.dragging === 'pan') {
      const scale = this.orbit.distance * 0.0016
      const forward = new THREE.Vector3(-Math.sin(this.orbit.azimuth), 0, -Math.cos(this.orbit.azimuth))
      const right = new THREE.Vector3(forward.z, 0, -forward.x)
      this.target.addScaledVector(right, -dx * scale)
      this.target.addScaledVector(forward, -dy * scale)
      const limit = WORLD * 0.75
      this.target.x = THREE.MathUtils.clamp(this.target.x, -limit, limit)
      this.target.z = THREE.MathUtils.clamp(this.target.z, -limit, limit)
      this.updateCamera()
    }

    this.lastPointer = { x: event.clientX, y: event.clientY }
    this.updateHover(event)
  }

  private onPointerUp = (event: PointerEvent) => {
    this.dragging = 'none'
    // A drag was a camera move, not a click.
    if (this.movedWhileDown) return
    if (event.button !== 0) return

    // While a card is held, the ghost on the ground is what the player is
    // aiming at — a tall building's silhouette must not steal the click.
    if (this.ghostVisible) {
      if (this.hoverCell) this.callbacks.onCellClick(this.hoverCell.x, this.hoverCell.y)
      return
    }

    const hit = this.pickAsset(event)
    if (hit) {
      this.callbacks.onAssetClick(hit)
      return
    }
    if (this.hoverCell) this.callbacks.onCellClick(this.hoverCell.x, this.hoverCell.y)
  }

  private onWheel = (event: WheelEvent) => {
    event.preventDefault()
    const factor = Math.exp(event.deltaY * 0.0011)
    this.orbit.distance = THREE.MathUtils.clamp(this.orbit.distance * factor, 30, 620)
    this.updateCamera()
  }

  private setPointerFrom(event: PointerEvent | WheelEvent) {
    const rect = this.canvas.getBoundingClientRect()
    this.pointer.x = ((event.clientX - rect.left) / rect.width) * 2 - 1
    this.pointer.y = -((event.clientY - rect.top) / rect.height) * 2 + 1
    this.raycaster.setFromCamera(this.pointer, this.camera)
  }

  private pickAsset(event: PointerEvent): string | null {
    this.setPointerFrom(event)
    const hits = this.raycaster.intersectObjects(this.buildingRoot.children, true)
    for (const hit of hits) {
      let object: THREE.Object3D | null = hit.object
      while (object && !object.userData.assetId) object = object.parent
      if (object?.userData.assetId) return object.userData.assetId as string
    }
    return null
  }

  private updateHover(event: PointerEvent) {
    this.setPointerFrom(event)
    const hit = this.raycaster.intersectObject(this.ground, false)[0]
    if (!hit) {
      this.hoverCell = null
      this.ghost.visible = false
      this.callbacks.onHover(null)
      return
    }
    const x = Math.floor((hit.point.x + WORLD / 2) / CELL_METRES)
    const y = Math.floor((hit.point.z + WORLD / 2) / CELL_METRES)
    if (x < 0 || y < 0 || x >= GRID_SIZE || y >= GRID_SIZE) {
      this.hoverCell = null
      this.ghost.visible = false
      this.callbacks.onHover(null)
      return
    }
    const changed = !this.hoverCell || this.hoverCell.x !== x || this.hoverCell.y !== y
    this.hoverCell = { x, y }
    if (changed) this.callbacks.onHover({ x, y })
    this.refreshGhost()
  }

  // ---- public API --------------------------------------------------------

  setGhost(footprint: [number, number], rotation: 0 | 1 | 2 | 3, visible: boolean, valid: boolean) {
    this.ghostFootprint = rotatedFootprint(footprint, rotation)
    this.ghostVisible = visible
    this.ghostValid = valid
    this.refreshGhost()
  }

  private refreshGhost() {
    if (!this.ghostVisible || !this.hoverCell) {
      this.ghost.visible = false
      return
    }
    const [w, d] = this.ghostFootprint
    const centre = cellCentre(this.hoverCell.x, this.hoverCell.y)
    this.ghost.position.set(
      centre.x + ((w - 1) * CELL_METRES) / 2,
      0.2,
      centre.z + ((d - 1) * CELL_METRES) / 2,
    )
    this.ghost.scale.set(w * CELL_METRES * 0.94, 6, d * CELL_METRES * 0.94)
    ;(this.ghost.material as THREE.MeshBasicMaterial).color.setHex(
      this.ghostValid ? PALETTE.hoverValid : PALETTE.hoverInvalid,
    )
    this.ghost.visible = true
  }

  /** Reconcile scene meshes with game state. */
  sync(state: GameState) {
    const seen = new Set<string>()

    for (const asset of state.assets) {
      seen.add(asset.id)
      let mesh = this.meshes.get(asset.id)
      if (!mesh) {
        mesh = createBuildingMesh(asset, definition(asset.definitionId))
        const [w, d] = rotatedFootprint(asset.footprint, asset.rotation)
        const centre = cellCentre(asset.x, asset.y)
        mesh.position.set(
          centre.x + ((w - 1) * CELL_METRES) / 2,
          0,
          centre.z + ((d - 1) * CELL_METRES) / 2,
        )
        this.buildingRoot.add(mesh)
        this.meshes.set(asset.id, mesh)
      }

      // Under-construction assets sit low and dim.
      const accent = mesh.userData.accent as THREE.Mesh | undefined
      if (accent) {
        const material = accent.material as THREE.MeshStandardMaterial
        material.emissiveIntensity = asset.operational ? (asset.brownout ? 0.15 : 1.0) : 0.05
      }
      const def = definition(asset.definitionId)
      const progress = def.constructionCycles
        ? 1 - asset.cyclesRemaining / def.constructionCycles
        : 1
      mesh.scale.y = asset.operational ? 1 : Math.max(0.12, progress)
    }

    for (const [id, mesh] of this.meshes) {
      if (seen.has(id)) continue
      this.buildingRoot.remove(mesh)
      mesh.traverse((child) => {
        if (child instanceof THREE.Mesh) child.geometry.dispose()
      })
      this.meshes.delete(id)
    }

    const selected = state.selectedAssetId
      ? state.assets.find((a) => a.id === state.selectedAssetId)
      : undefined
    if (selected) {
      const [w, d] = rotatedFootprint(selected.footprint, selected.rotation)
      const centre = cellCentre(selected.x, selected.y)
      this.selectionRing.position.set(
        centre.x + ((w - 1) * CELL_METRES) / 2,
        0.35,
        centre.z + ((d - 1) * CELL_METRES) / 2,
      )
      const radius = Math.max(w, d)
      this.selectionRing.scale.setScalar(radius)
      this.selectionRing.visible = true
    } else {
      this.selectionRing.visible = false
    }

    this.syncOverlay(state)
    this.refreshGhost()
  }

  /** Paint per-cell overlay tiles for the active analysis mode. */
  private syncOverlay(state: GameState) {
    this.overlayRoot.clear()
    if (state.overlay === 'none') return

    const services = computeServices(state.assets)
    const geometry = new THREE.PlaneGeometry(CELL_METRES * 0.9, CELL_METRES * 0.9)

    // For the utility overlays, first wash in the ground actually covered by a
    // supplier, so the player can see reach before committing to a placement.
    const utility = state.overlay === 'power' || state.overlay === 'water' || state.overlay === 'data'
    if (utility) {
      const supplies = (id: string) => {
        const def = definition(id)
        if (state.overlay === 'power') return def.powerProduced > 0
        if (state.overlay === 'water') return def.waterProduced > 0
        return def.dataProduced > 0
      }
      const covered = new Set<number>()
      for (const asset of state.assets) {
        if (!asset.operational || !supplies(asset.definitionId)) continue
        for (const cell of cellsOf(asset)) {
          const ox = cell % GRID_SIZE
          const oy = Math.floor(cell / GRID_SIZE)
          for (let dx = -UTILITY_RADIUS; dx <= UTILITY_RADIUS; dx++) {
            for (let dy = -UTILITY_RADIUS; dy <= UTILITY_RADIUS; dy++) {
              const nx = ox + dx
              const ny = oy + dy
              if (nx < 0 || ny < 0 || nx >= GRID_SIZE || ny >= GRID_SIZE) continue
              covered.add(ny * GRID_SIZE + nx)
            }
          }
        }
      }
      const washMaterial = new THREE.MeshBasicMaterial({
        color: 0x4dd8e6,
        transparent: true,
        opacity: 0.12,
        depthWrite: false,
      })
      for (const cell of covered) {
        const tile = new THREE.Mesh(geometry, washMaterial)
        tile.rotation.x = -Math.PI / 2
        const centre = cellCentre(cell % GRID_SIZE, Math.floor(cell / GRID_SIZE))
        tile.position.set(centre.x, 0.14, centre.z)
        this.overlayRoot.add(tile)
      }
    }

    for (const asset of state.assets) {
      const def = definition(asset.definitionId)
      const svc = services.get(asset.id)
      let color: number | null = null

      switch (state.overlay) {
        case 'power':
          color = def.powerProduced > 0 ? 0x4dd8e6 : svc?.power ? 0x4ade80 : 0xf05252
          break
        case 'water':
          color = def.waterProduced > 0 ? 0x4dd8e6 : svc?.water ? 0x4ade80 : 0xf05252
          break
        case 'data':
          color = def.dataProduced > 0 ? 0x4dd8e6 : svc?.data ? 0x4ade80 : 0xe8913a
          break
        case 'housing':
          color = def.housingCapacity > 0 ? 0x4ade80 : null
          break
        case 'employment':
          color = def.jobCapacity > 0 ? (asset.staffed > 0 ? 0x4ade80 : 0xe8913a) : null
          break
        case 'happiness':
          color = def.happiness > 0 ? 0x4ade80 : def.happiness < 0 ? 0xf05252 : null
          break
        default:
          color = null
      }
      if (color === null) continue

      const material = new THREE.MeshBasicMaterial({
        color,
        transparent: true,
        opacity: 0.4,
        depthWrite: false,
      })
      for (const cell of cellsOf(asset)) {
        const cx = cell % GRID_SIZE
        const cy = Math.floor(cell / GRID_SIZE)
        const tile = new THREE.Mesh(geometry, material)
        tile.rotation.x = -Math.PI / 2
        const centre = cellCentre(cx, cy)
        tile.position.set(centre.x, 0.25, centre.z)
        this.overlayRoot.add(tile)
      }
    }
  }

  focusOn(asset: WorldAsset) {
    const centre = cellCentre(asset.x, asset.y)
    this.target.set(centre.x, 0, centre.z)
    this.updateCamera()
  }

  private updateCamera() {
    const { azimuth, polar, distance } = this.orbit
    this.camera.position.set(
      this.target.x + distance * Math.sin(polar) * Math.sin(azimuth),
      this.target.y + distance * Math.cos(polar),
      this.target.z + distance * Math.sin(polar) * Math.cos(azimuth),
    )
    this.camera.lookAt(this.target)
  }

  resize() {
    const width = this.canvas.clientWidth || window.innerWidth
    const height = this.canvas.clientHeight || window.innerHeight
    this.renderer.setSize(width, height, false)
    this.camera.aspect = width / Math.max(1, height)
    this.camera.updateProjectionMatrix()
  }

  start() {
    const loop = () => {
      if (this.disposed) return
      this.frame = requestAnimationFrame(loop)
      this.renderer.render(this.scene, this.camera)
    }
    loop()
  }

  dispose() {
    this.disposed = true
    cancelAnimationFrame(this.frame)
    this.canvas.removeEventListener('pointerdown', this.onPointerDown)
    this.canvas.removeEventListener('pointermove', this.onPointerMove)
    window.removeEventListener('pointerup', this.onPointerUp)
    this.canvas.removeEventListener('wheel', this.onWheel)
    this.renderer.dispose()
  }
}
