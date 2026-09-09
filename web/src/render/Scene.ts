import { animate } from 'animejs'
import { ArcRotateCamera } from '@babylonjs/core/Cameras/arcRotateCamera'
import { Color3, Color4 } from '@babylonjs/core/Maths/math.color'
import { Vector3 } from '@babylonjs/core/Maths/math.vector'
import { Engine } from '@babylonjs/core/Engines/engine'
import { Scene } from '@babylonjs/core/scene'
import { DirectionalLight } from '@babylonjs/core/Lights/directionalLight'
import { HemisphericLight } from '@babylonjs/core/Lights/hemisphericLight'
import { ShadowGenerator } from '@babylonjs/core/Lights/Shadows/shadowGenerator'
import { Mesh } from '@babylonjs/core/Meshes/mesh'
import { TransformNode } from '@babylonjs/core/Meshes/transformNode'
import { VertexData } from '@babylonjs/core/Meshes/mesh.vertexData'
import { PBRMaterial } from '@babylonjs/core/Materials/PBR/pbrMaterial'
import { ImageProcessingConfiguration } from '@babylonjs/core/Materials/imageProcessingConfiguration'
import { GlowLayer } from '@babylonjs/core/Layers/glowLayer'
import { DefaultRenderingPipeline } from '@babylonjs/core/PostProcesses/RenderPipeline/Pipelines/defaultRenderingPipeline'
import { GradientMaterial } from '@babylonjs/materials/gradient/gradientMaterial'
import { CELL_METRES, GRID_SIZE, cellsOf, rotatedFootprint } from '@/game/grid'
import { UTILITY_RADIUS, computeServices } from '@/game/economy'
import { definition } from '@/game/content'
import type { GameState, WorldAsset } from '@/game/types'
import { PALETTE } from './palette'
import {
  createBuildingVisual,
  createPlacementGhost,
  createSelectionSignal,
  setSignalColor,
  tagAsPickable,
  type BuildingVisual,
} from './buildings'
import { createCivicEnvironment } from './civicEnvironment'
import { createCivicTraffic } from './civicTraffic'

const WORLD = GRID_SIZE * CELL_METRES

export interface SceneCallbacks {
  onCellClick: (x: number, y: number) => void
  onAssetClick: (assetId: string) => void
  onHover: (cell: { x: number; y: number } | null) => void
}

function color(value: number): Color3 {
  return Color3.FromHexString(`#${value.toString(16).padStart(6, '0')}`)
}

function cellCentre(x: number, y: number): Vector3 {
  return new Vector3(
    (x + 0.5) * CELL_METRES - WORLD / 2,
    0,
    (y + 0.5) * CELL_METRES - WORLD / 2,
  )
}

function applyMeshData(mesh: Mesh, positions: number[], indices: number[]) {
  const normals: number[] = []
  VertexData.ComputeNormals(positions, indices, normals)
  const data = new VertexData()
  data.positions = positions
  data.indices = indices
  data.normals = normals
  data.applyToMesh(mesh, true)
}

/** The developed plateau and north river share the environment's authored coordinates. */
export function terrainHeight(x: number, z: number): number {
  const northBank = -WORLD / 2 - 110
  const southBank = -WORLD / 2 - 40
  const riverDepth = Math.min(1, Math.max(0, (z - northBank) / 2), Math.max(0, (southBank - z) / 2))
  const riverWidth = Math.min(1, Math.max(0, (380 - Math.abs(x)) / 20))
  if (riverDepth > 0 && riverWidth > 0) return -5 * riverDepth * riverWidth
  const distance = Math.max(Math.abs(x), Math.abs(z))
  if (distance <= 380) return 0
  const edge = Math.min(1, (distance - 380) / 150)
  const ridges = Math.sin(x * 0.035) * 8 + Math.cos(z * 0.027) * 10 + Math.sin((x + z) * 0.014) * 14
  return edge * (ridges - 1.5)
}

export function createTerrain(scene: Scene): Mesh {
  const mesh = new Mesh('authored-terrain', scene)
  const size = WORLD * 4.4
  const subdivisions = 84
  const positions: number[] = []
  const indices: number[] = []
  const uvs: number[] = []
  const axis = Array.from({ length: subdivisions + 1 }, (_, index) => (index / subdivisions - 0.5) * size)
  // Explicit bank rows prevent coarse-grid interpolation from burying the river margins.
  const xs = Array.from(new Set([...axis, -380, -360, 360, 380])).sort((a, b) => a - b)
  const zs = Array.from(new Set([...axis, -WORLD / 2 - 110, -WORLD / 2 - 108, -WORLD / 2 - 42, -WORLD / 2 - 40])).sort((a, b) => a - b)
  for (const z of zs) for (const x of xs) {
    positions.push(x, terrainHeight(x, z), z)
    uvs.push(x / size + 0.5, z / size + 0.5)
  }
  for (let zIndex = 0; zIndex < zs.length - 1; zIndex += 1) {
    for (let xIndex = 0; xIndex < xs.length - 1; xIndex += 1) {
      const row = xs.length
      const a = zIndex * row + xIndex
      indices.push(a, a + 1, a + row, a + 1, a + row + 1, a + row)
    }
  }
  const normals: number[] = []
  VertexData.ComputeNormals(positions, indices, normals)
  const data = new VertexData()
  data.positions = positions
  data.indices = indices
  data.normals = normals
  data.uvs = uvs
  data.applyToMesh(mesh, true)

  const material = new PBRMaterial('terrain-strata', scene)
  material.albedoColor = color(PALETTE.ground)
  material.metallic = 0.08
  material.roughness = 0.91
  material.environmentIntensity = 0.42
  mesh.material = material
  mesh.receiveShadows = true
  mesh.isPickable = true
  mesh.metadata = { terrain: true }
  return mesh
}

function appendRoadQuad(
  positions: number[],
  indices: number[],
  x1: number,
  z1: number,
  x2: number,
  z2: number,
  width: number,
) {
  const dx = x2 - x1
  const dz = z2 - z1
  const length = Math.hypot(dx, dz) || 1
  const px = (-dz / length) * width
  const pz = (dx / length) * width
  const start = positions.length / 3
  positions.push(x1 + px, 0.09, z1 + pz, x1 - px, 0.09, z1 - pz)
  positions.push(x2 + px, 0.09, z2 + pz, x2 - px, 0.09, z2 - pz)
  indices.push(start, start + 1, start + 2, start + 2, start + 1, start + 3)
}

export function createCivicLattice(scene: Scene): Mesh {
  const mesh = new Mesh('civic-lattice', scene)
  const positions: number[] = []
  const indices: number[] = []
  const half = WORLD / 2
  for (let index = 0; index <= GRID_SIZE; index += 1) {
    const p = index * CELL_METRES - half
    const width = index % 8 === 0 ? 0.1 : 0.035
    appendRoadQuad(positions, indices, p, -half, p, half, width)
    appendRoadQuad(positions, indices, -half, p, half, p, width)
  }
  applyMeshData(mesh, positions, indices)
  const material = new PBRMaterial('lattice-signal', scene)
  material.albedoColor = color(PALETTE.gridLine)
  material.emissiveColor = color(PALETTE.gridMajor)
  material.emissiveIntensity = 0.03
  material.metallic = 0.08
  material.roughness = 0.3
  mesh.material = material
  mesh.isPickable = false
  return mesh
}

export function createSkyVault(scene: Scene): Mesh {
  const mesh = new Mesh('sky-vault', scene)
  const radius = WORLD * 4
  const rings = 18
  const segments = 56
  const positions: number[] = []
  const indices: number[] = []
  for (let ring = 0; ring <= rings; ring += 1) {
    const polar = (ring / rings) * Math.PI * 0.54
    const y = Math.cos(polar) * radius - radius * 0.08
    const spread = Math.sin(polar) * radius
    for (let segment = 0; segment < segments; segment += 1) {
      const angle = (segment / segments) * Math.PI * 2
      positions.push(Math.cos(angle) * spread, y, Math.sin(angle) * spread)
    }
  }
  for (let ring = 0; ring < rings; ring += 1) {
    for (let segment = 0; segment < segments; segment += 1) {
      const next = (segment + 1) % segments
      const a = ring * segments + segment
      const b = ring * segments + next
      const c = (ring + 1) * segments + segment
      const d = (ring + 1) * segments + next
      indices.push(a, b, c, b, d, c)
    }
  }
  applyMeshData(mesh, positions, indices)
  const material = new GradientMaterial('sky-gradient', scene)
  material.topColor = Color3.FromHexString('#8da5ad')
  material.bottomColor = Color3.FromHexString('#d3c9ae')
  material.offset = 0.15
  material.scale = 0.92
  material.backFaceCulling = true
  material.disableLighting = true
  mesh.material = material
  mesh.infiniteDistance = true
  mesh.isPickable = false
  return mesh
}

interface OverlayCell { cell: number; value: number; alpha: number; height: number }

/** One draw call per overlay color, independent of utility coverage size. */
export function createCellSignals(scene: Scene, root: TransformNode, cells: OverlayCell[]) {
  const groups = new Map<string, OverlayCell[]>()
  for (const cell of cells) {
    const key = `${cell.value}:${cell.alpha}:${cell.height}`
    const group = groups.get(key) ?? []
    group.push(cell)
    groups.set(key, group)
  }
  for (const [key, group] of groups) {
    const positions: number[] = []
    const indices: number[] = []
    const half = CELL_METRES * 0.47
    for (const item of group) {
      const centre = cellCentre(item.cell % GRID_SIZE, Math.floor(item.cell / GRID_SIZE))
      const index = positions.length / 3
      positions.push(centre.x - half, item.height, centre.z - half, centre.x - half, item.height, centre.z + half,
        centre.x + half, item.height, centre.z + half, centre.x + half, item.height, centre.z - half)
      indices.push(index, index + 2, index + 1, index, index + 3, index + 2)
    }
    const mesh = new Mesh(`coverage:${key}`, scene)
    applyMeshData(mesh, positions, indices)
    const material = new PBRMaterial(`coverage:${key}`, scene)
    material.albedoColor = color(group[0].value)
    material.emissiveColor = color(group[0].value)
    material.emissiveIntensity = 0.5
    material.alpha = group[0].alpha
    material.transparencyMode = PBRMaterial.PBRMATERIAL_ALPHABLEND
    material.disableLighting = true
    mesh.material = material
    mesh.isPickable = false
    mesh.parent = root
  }
}

export class CityScene {
  private engine: Engine
  private scene: Scene
  private camera: ArcRotateCamera
  private ground: Mesh
  private traffic: ReturnType<typeof createCivicTraffic>
  private shadow: ShadowGenerator
  private buildingRoot: TransformNode
  private overlayRoot: TransformNode
  private ghost: BuildingVisual
  private selectionSignal: Mesh
  private meshes = new Map<string, BuildingVisual>()
  private disposed = false
  private pointerDown = { x: 0, y: 0 }
  private movedWhileDown = false
  private hoverCell: { x: number; y: number } | null = null
  private ghostFootprint: [number, number] = [1, 1]
  private ghostValid = true
  private ghostVisible = false
  private activePointers = new Set<number>()
  private overlaySignature = ''
  private hasSynced = false
  private heightTargets = new Map<string, number>()
  private constructionMotion = new Map<string, ReturnType<typeof animate>>()
  private reducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)').matches

  constructor(
    private canvas: HTMLCanvasElement,
    private callbacks: SceneCallbacks,
  ) {
    this.engine = new Engine(canvas, true, {
      adaptToDeviceRatio: true,
      antialias: true,
      preserveDrawingBuffer: false,
      stencil: true,
      powerPreference: 'high-performance',
    })
    this.scene = new Scene(this.engine)
    this.engine.setHardwareScalingLevel(1 / Math.min(window.devicePixelRatio || 1, 1.5))
    this.scene.clearColor = new Color4(0.56, 0.63, 0.65, 1)
    this.scene.fogMode = Scene.FOGMODE_EXP2
    this.scene.fogDensity = 0.0011
    this.scene.fogColor = color(PALETTE.fog)
    this.scene.ambientColor = new Color3(0.22, 0.24, 0.22)

    this.camera = new ArcRotateCamera(
      'command-camera',
      Math.PI * 0.68,
      Math.PI * 0.29,
      175,
      Vector3.Zero(),
      this.scene,
    )
    this.camera.lowerRadiusLimit = 32
    this.camera.upperRadiusLimit = 460
    this.camera.lowerBetaLimit = 0.3
    this.camera.upperBetaLimit = Math.PI * 0.44
    this.camera.wheelDeltaPercentage = 0.012
    this.camera.panningSensibility = 115
    this.camera.panningAxis = new Vector3(1, 0, 1)
    this.camera.inertia = 0.78
    this.camera.attachControl(canvas, true)

    const hemi = new HemisphericLight('dawn-fill', new Vector3(0.18, 1, 0.32), this.scene)
    hemi.intensity = 1.05
    hemi.diffuse = color(PALETTE.keyLight)
    hemi.groundColor = color(PALETTE.fillLight)

    const key = new DirectionalLight('synara-sun', new Vector3(-0.48, -0.82, -0.34), this.scene)
    key.position = new Vector3(WORLD * 0.6, WORLD, WORLD * 0.5)
    key.intensity = 2.7
    key.diffuse = Color3.FromHexString('#fff0d2')
    this.shadow = new ShadowGenerator(window.innerWidth < 768 ? 1024 : 2048, key)
    this.shadow.useBlurExponentialShadowMap = true
    this.shadow.blurKernel = 12
    this.shadow.bias = 0.0007

    this.ground = createTerrain(this.scene)
    createCivicLattice(this.scene)
    createSkyVault(this.scene)
    createCivicEnvironment(this.scene).forEach((mesh) => {
      if (mesh.name.endsWith(':stone') || mesh.name.endsWith(':foliage')) this.shadow.addShadowCaster(mesh)
    })

    this.traffic = createCivicTraffic(this.scene)
    this.buildingRoot = new TransformNode('city-assets', this.scene)
    this.overlayRoot = new TransformNode('analysis-signals', this.scene)
    this.ghost = createPlacementGhost(this.scene)
    this.selectionSignal = createSelectionSignal(this.scene)

    const glow = new GlowLayer('signal-bloom', this.scene, { blurKernelSize: 42 })
    glow.intensity = 0.18
    const pipeline = new DefaultRenderingPipeline('cinematic-pipeline', true, this.scene, [this.camera])
    pipeline.fxaaEnabled = true
    pipeline.bloomEnabled = true
    pipeline.bloomThreshold = 0.76
    pipeline.bloomWeight = 0.08
    pipeline.bloomKernel = 48
    pipeline.samples = window.innerWidth < 768 ? 1 : 2

    const processing = this.scene.imageProcessingConfiguration
    processing.toneMappingEnabled = true
    processing.toneMappingType = ImageProcessingConfiguration.TONEMAPPING_ACES
    processing.exposure = 1.1
    processing.contrast = 1.08

    this.attachEvents()
    this.resize()
  }

  private eventCoordinates(event: PointerEvent) {
    const rect = this.canvas.getBoundingClientRect()
    return {
      // Babylon converts CSS pixels through hardwareScalingLevel internally.
      x: event.clientX - rect.left,
      y: event.clientY - rect.top,
    }
  }

  private attachEvents() {
    this.canvas.addEventListener('pointerdown', this.onPointerDown)
    this.canvas.addEventListener('pointermove', this.onPointerMove)
    window.addEventListener('pointerup', this.onPointerUp)
    window.addEventListener('pointercancel', this.onPointerCancel)
    this.canvas.addEventListener('contextmenu', this.preventContextMenu)
    this.canvas.addEventListener('pointerleave', this.onPointerLeave)
  }

  private preventContextMenu = (event: Event) => event.preventDefault()

  private onPointerLeave = () => {
    this.hoverCell = null
    this.ghost.root.setEnabled(false)
    this.callbacks.onHover(null)
  }

  private onPointerDown = (event: PointerEvent) => {
    this.activePointers.add(event.pointerId)
    this.pointerDown = { x: event.clientX, y: event.clientY }
    this.movedWhileDown = this.activePointers.size > 1
    this.updateHover(event)
  }

  private onPointerMove = (event: PointerEvent) => {
    if (Math.abs(event.clientX - this.pointerDown.x) + Math.abs(event.clientY - this.pointerDown.y) > 4) {
      this.movedWhileDown = true
    }
    this.updateHover(event)
  }

  private onPointerCancel = (event: PointerEvent) => {
    this.activePointers.delete(event.pointerId)
    this.movedWhileDown = true
    this.onPointerLeave()
  }

  private onPointerUp = (event: PointerEvent) => {
    const startedHere = this.activePointers.delete(event.pointerId)
    if (!startedHere || this.movedWhileDown || event.button !== 0 || this.activePointers.size) return
    const rect = this.canvas.getBoundingClientRect()
    if (event.clientX < rect.left || event.clientX > rect.right || event.clientY < rect.top || event.clientY > rect.bottom) return
    this.updateHover(event)
    if (this.ghostVisible) {
      if (this.hoverCell) this.callbacks.onCellClick(this.hoverCell.x, this.hoverCell.y)
      return
    }
    const { x, y } = this.eventCoordinates(event)
    const hit = this.scene.pick(x, y, (mesh) => Boolean(mesh.metadata?.assetId), false, this.camera)
    const assetId = hit?.pickedMesh?.metadata?.assetId as string | undefined
    if (assetId) this.callbacks.onAssetClick(assetId)
    else if (this.hoverCell) this.callbacks.onCellClick(this.hoverCell.x, this.hoverCell.y)
  }

  private updateHover(event: PointerEvent) {
    const { x, y } = this.eventCoordinates(event)
    const hit = this.scene.pick(x, y, (mesh) => mesh === this.ground, false, this.camera)
    const point = hit?.pickedPoint
    if (!point) {
      this.hoverCell = null
      this.ghost.root.setEnabled(false)
      this.callbacks.onHover(null)
      return
    }
    const cellX = Math.floor((point.x + WORLD / 2) / CELL_METRES)
    const cellY = Math.floor((point.z + WORLD / 2) / CELL_METRES)
    if (cellX < 0 || cellY < 0 || cellX >= GRID_SIZE || cellY >= GRID_SIZE) {
      this.hoverCell = null
      this.ghost.root.setEnabled(false)
      this.callbacks.onHover(null)
      return
    }
    const changed = !this.hoverCell || this.hoverCell.x !== cellX || this.hoverCell.y !== cellY
    this.hoverCell = { x: cellX, y: cellY }
    if (changed) this.callbacks.onHover(this.hoverCell)
    this.refreshGhost()
  }

  setGhost(footprint: [number, number], rotation: 0 | 1 | 2 | 3, visible: boolean, valid: boolean) {
    this.ghostFootprint = rotatedFootprint(footprint, rotation)
    this.ghostVisible = visible
    this.ghostValid = valid
    this.refreshGhost()
  }

  private refreshGhost() {
    if (!this.ghostVisible || !this.hoverCell) {
      this.ghost.root.setEnabled(false)
      return
    }
    const [w, d] = this.ghostFootprint
    const centre = cellCentre(this.hoverCell.x, this.hoverCell.y)
    this.ghost.root.position.set(
      centre.x + ((w - 1) * CELL_METRES) / 2,
      0.18,
      centre.z + ((d - 1) * CELL_METRES) / 2,
    )
    this.ghost.root.scaling.set(w, 1, d)
    setSignalColor(
      this.ghost.accents,
      this.ghostValid ? PALETTE.hoverValid : PALETTE.hoverInvalid,
      this.ghostValid ? 1.45 : 1.8,
    )
    this.ghost.root.setEnabled(true)
  }

  sync(state: GameState) {
    const seen = new Set<string>()
    for (const asset of state.assets) {
      seen.add(asset.id)
      let visual = this.meshes.get(asset.id)
      if (!visual) {
        visual = createBuildingVisual(this.scene, asset, definition(asset.definitionId))
        const [w, d] = rotatedFootprint(asset.footprint, asset.rotation)
        const centre = cellCentre(asset.x, asset.y)
        visual.root.position.set(
          centre.x + ((w - 1) * CELL_METRES) / 2,
          0,
          centre.z + ((d - 1) * CELL_METRES) / 2,
        )
        visual.root.parent = this.buildingRoot
        tagAsPickable(visual.root, asset.id)
        visual.root.getChildMeshes().forEach((mesh) => this.shadow.addShadowCaster(mesh))
        this.meshes.set(asset.id, visual)
      }

      const def = definition(asset.definitionId)
      const progress = def.constructionCycles
        ? 1 - asset.cyclesRemaining / def.constructionCycles
        : 1
      const targetHeight = asset.operational ? 1 : Math.max(0.14, progress)
      if (this.heightTargets.get(asset.id) !== targetHeight) {
        this.constructionMotion.get(asset.id)?.cancel()
        if (!this.hasSynced || this.reducedMotion) {
          visual.root.scaling.y = targetHeight
        } else {
          if (!this.heightTargets.has(asset.id)) visual.root.scaling.y = 0.04
          this.constructionMotion.set(asset.id, animate(visual.root.scaling, {
            y: targetHeight,
            duration: asset.operational ? 850 : 440,
            ease: asset.operational ? 'outBack(1.2)' : 'outCubic',
          }))
        }
        this.heightTargets.set(asset.id, targetHeight)
      }
      setSignalColor(
        visual.accents,
        visual.signalColor,
        asset.operational ? (asset.brownout ? 0.04 : 0.45) : 0.02,
      )
    }

    for (const [id, visual] of this.meshes) {
      if (seen.has(id)) continue
      this.constructionMotion.get(id)?.cancel()
      this.constructionMotion.delete(id)
      this.heightTargets.delete(id)
      visual.root.dispose(false, true)
      this.meshes.delete(id)
    }

    const selected = state.selectedAssetId
      ? state.assets.find((asset) => asset.id === state.selectedAssetId)
      : undefined
    if (selected) {
      const [w, d] = rotatedFootprint(selected.footprint, selected.rotation)
      const centre = cellCentre(selected.x, selected.y)
      this.selectionSignal.position.set(
        centre.x + ((w - 1) * CELL_METRES) / 2,
        0.32,
        centre.z + ((d - 1) * CELL_METRES) / 2,
      )
      this.selectionSignal.scaling.set(w, 1, d)
      this.selectionSignal.setEnabled(true)
    } else {
      this.selectionSignal.setEnabled(false)
    }

    this.syncOverlay(state)
    this.refreshGhost()
    this.hasSynced = true
  }

  private syncOverlay(state: GameState) {
    const signature = JSON.stringify([state.overlay, state.assets.map((asset) => [asset.id, asset.x, asset.y, asset.rotation, asset.operational, asset.staffed, asset.brownout])])
    if (signature === this.overlaySignature) return
    this.overlaySignature = signature
    this.overlayRoot.getChildren().forEach((node) => node.dispose(false, true))
    if (state.overlay === 'none') return
    const overlayCells: OverlayCell[] = []

    const services = computeServices(state.assets)
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
          const originX = cell % GRID_SIZE
          const originY = Math.floor(cell / GRID_SIZE)
          for (let dx = -UTILITY_RADIUS; dx <= UTILITY_RADIUS; dx += 1) {
            for (let dy = -UTILITY_RADIUS; dy <= UTILITY_RADIUS; dy += 1) {
              const x = originX + dx
              const y = originY + dy
              if (x >= 0 && y >= 0 && x < GRID_SIZE && y < GRID_SIZE) covered.add(y * GRID_SIZE + x)
            }
          }
        }
      }
      for (const cell of covered) {
        overlayCells.push({ cell, value: PALETTE.rim, alpha: 0.16, height: 0.14 })
      }
    }

    for (const asset of state.assets) {
      const def = definition(asset.definitionId)
      const service = services.get(asset.id)
      let value: number | null = null
      switch (state.overlay) {
        case 'power': value = def.powerProduced > 0 ? PALETTE.rim : service?.power ? PALETTE.hoverValid : PALETTE.hoverInvalid; break
        case 'water': value = def.waterProduced > 0 ? PALETTE.rim : service?.water ? PALETTE.hoverValid : PALETTE.hoverInvalid; break
        case 'data': value = def.dataProduced > 0 ? PALETTE.rim : service?.data ? PALETTE.hoverValid : 0xe8913a; break
        case 'housing': value = def.housingCapacity > 0 ? PALETTE.hoverValid : null; break
        case 'employment': value = def.jobCapacity > 0 ? (asset.staffed > 0 ? PALETTE.hoverValid : 0xe8913a) : null; break
        case 'happiness': value = def.happiness > 0 ? PALETTE.hoverValid : def.happiness < 0 ? PALETTE.hoverInvalid : null; break
      }
      if (value === null) continue
      for (const cell of cellsOf(asset)) {
        overlayCells.push({ cell, value, alpha: 0.42, height: 0.24 })
      }
    }
    createCellSignals(this.scene, this.overlayRoot, overlayCells)
  }

  focusOn(asset: WorldAsset) {
    const [w, d] = rotatedFootprint(asset.footprint, asset.rotation)
    const centre = cellCentre(asset.x + (w - 1) / 2, asset.y + (d - 1) / 2)
    this.camera.setTarget(new Vector3(centre.x, 0, centre.z))
  }

  resetView() {
    this.camera.setTarget(Vector3.Zero())
    this.camera.alpha = Math.PI * 0.68
    this.camera.beta = Math.PI * 0.29
    this.camera.radius = 175
    this.camera.inertialAlphaOffset = 0
    this.camera.inertialBetaOffset = 0
    this.camera.inertialRadiusOffset = 0
    this.camera.inertialPanningX = 0
    this.camera.inertialPanningY = 0
  }

  resize() {
    this.engine.resize()
  }

  start() {
    this.engine.runRenderLoop(() => {
      if (!this.disposed && !document.hidden) {
        const target = this.camera.target
        target.x = Math.max(-WORLD / 2, Math.min(WORLD / 2, target.x))
        target.z = Math.max(-WORLD / 2, Math.min(WORLD / 2, target.z))
        const pulse = this.reducedMotion ? 1 : 1 + Math.sin(performance.now() * 0.0022) * 0.04
        this.selectionSignal.scaling.y = pulse
        if (!this.reducedMotion) this.traffic.update(performance.now() / 1000)
        this.scene.render()
      }
    })
  }

  dispose() {
    this.disposed = true
    this.constructionMotion.forEach((animation) => animation.cancel())
    this.constructionMotion.clear()
    this.canvas.removeEventListener('pointerdown', this.onPointerDown)
    this.canvas.removeEventListener('pointermove', this.onPointerMove)
    window.removeEventListener('pointerup', this.onPointerUp)
    window.removeEventListener('pointercancel', this.onPointerCancel)
    this.canvas.removeEventListener('contextmenu', this.preventContextMenu)
    this.canvas.removeEventListener('pointerleave', this.onPointerLeave)
    this.camera.detachControl()
    this.scene.dispose()
    this.engine.dispose()
  }
}
