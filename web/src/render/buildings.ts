import { Color3 } from '@babylonjs/core/Maths/math.color'
import { Vector3 } from '@babylonjs/core/Maths/math.vector'
import { Mesh } from '@babylonjs/core/Meshes/mesh'
import { TransformNode } from '@babylonjs/core/Meshes/transformNode'
import { VertexData } from '@babylonjs/core/Meshes/mesh.vertexData'
import { VertexBuffer } from '@babylonjs/core/Buffers/buffer'
import { PBRMaterial } from '@babylonjs/core/Materials/PBR/pbrMaterial'
import type { Scene } from '@babylonjs/core/scene'
import type { CardDefinition, WorldAsset } from '@/game/types'
import { rotatedFootprint } from '@/game/grid'
import { createArchitecturePlan, type PlanPoint } from './architecture'
import { FACTION_COLOR, TYPE_STYLE } from './palette'

export interface BuildingVisual {
  root: TransformNode
  accents: PBRMaterial[]
  signalColor: number
}

function asColor(value: number): Color3 {
  return Color3.FromHexString(`#${value.toString(16).padStart(6, '0')}`)
}

function applyVertexData(mesh: Mesh, positions: number[], indices: number[]) {
  const normals: number[] = []
  VertexData.ComputeNormals(positions, indices, normals)
  const data = new VertexData()
  data.positions = positions
  data.indices = indices
  data.normals = normals
  data.applyToMesh(mesh, true)
}

/** A bespoke multi-contour architectural shell; no primitive mesh builders. */
function createLoft(
  name: string,
  scene: Scene,
  contours: PlanPoint[][],
  heights: number[],
  material: PBRMaterial,
): Mesh {
  const positions: number[] = []
  const indices: number[] = []
  const sides = contours[0].length

  for (let ring = 0; ring < contours.length; ring += 1) {
    for (const [x, z] of contours[ring]) positions.push(x, heights[ring], z)
  }
  for (let ring = 0; ring < contours.length - 1; ring += 1) {
    const next = (ring + 1) * sides
    const current = ring * sides
    for (let side = 0; side < sides; side += 1) {
      const following = (side + 1) % sides
      indices.push(current + side, current + following, next + side)
      indices.push(current + following, next + following, next + side)
    }
  }

  const topCentre = positions.length / 3
  positions.push(0, heights.at(-1) ?? 1, 0)
  const topStart = (contours.length - 1) * sides
  for (let side = 0; side < sides; side += 1) {
    indices.push(topCentre, topStart + side, topStart + ((side + 1) % sides))
  }

  const mesh = new Mesh(name, scene)
  applyVertexData(mesh, positions, indices)
  mesh.material = material
  mesh.receiveShadows = true
  return mesh
}

function interpolateContour(plan: ReturnType<typeof createArchitecturePlan>, t: number): PlanPoint[] {
  const fromIndex = t < 0.5 ? 0 : 1
  const toIndex = fromIndex + 1
  const local = t < 0.5 ? t * 2 : (t - 0.5) * 2
  return plan.contours[fromIndex].map(([x, z], index) => {
    const [tx, tz] = plan.contours[toIndex][index]
    return [x + (tx - x) * local, z + (tz - z) * local] as const
  })
}

function createFacadeBand(
  name: string,
  scene: Scene,
  contour: PlanPoint[],
  height: number,
  thickness: number,
  material: PBRMaterial,
): Mesh {
  const positions: number[] = []
  const indices: number[] = []
  for (const [x, z] of contour) positions.push(x, height - thickness, z, x, height + thickness, z)
  for (let side = 0; side < contour.length; side += 1) {
    const next = (side + 1) % contour.length
    const low = side * 2
    const high = low + 1
    const nextLow = next * 2
    const nextHigh = nextLow + 1
    indices.push(low, nextLow, high, nextLow, nextHigh, high)
  }
  const mesh = new Mesh(name, scene)
  applyVertexData(mesh, positions, indices)
  mesh.material = material
  return mesh
}

function createSpireBlade(
  name: string,
  scene: Scene,
  path: PlanPoint[],
  baseHeight: number,
  material: PBRMaterial,
): Mesh {
  const positions: number[] = []
  const indices: number[] = []
  const width = 0.42
  path.forEach(([x, z], index) => {
    const y = baseHeight * (0.32 + index * 0.44)
    const tangent = index === path.length - 1
      ? new Vector3(x - path[index - 1][0], 0, z - path[index - 1][1])
      : new Vector3(path[index + 1][0] - x, 0, path[index + 1][1] - z)
    const side = Vector3.Cross(tangent.normalize(), Vector3.Up()).normalize().scale(width * (1 - index * 0.22))
    positions.push(x + side.x, y, z + side.z, x - side.x, y, z - side.z)
  })
  for (let index = 0; index < path.length - 1; index += 1) {
    const a = index * 2
    indices.push(a, a + 2, a + 1, a + 2, a + 3, a + 1)
  }
  const mesh = new Mesh(name, scene)
  applyVertexData(mesh, positions, indices)
  mesh.material = material
  return mesh
}

function createSignalLoop(name: string, scene: Scene, radius: number, material: PBRMaterial): Mesh {
  const positions: number[] = []
  const indices: number[] = []
  const segments = 48
  for (let index = 0; index < segments; index += 1) {
    const angle = (index / segments) * Math.PI * 2
    const rhythm = 1 + Math.sin(index * 2.13) * 0.045
    const inner = radius * rhythm - 0.12
    const outer = radius * rhythm + 0.12
    positions.push(Math.cos(angle) * inner, 0, Math.sin(angle) * inner)
    positions.push(Math.cos(angle) * outer, 0, Math.sin(angle) * outer)
  }
  for (let index = 0; index < segments; index += 1) {
    const next = (index + 1) % segments
    indices.push(index * 2, next * 2, index * 2 + 1, next * 2, next * 2 + 1, index * 2 + 1)
  }
  const mesh = new Mesh(name, scene)
  applyVertexData(mesh, positions, indices)
  mesh.material = material
  return mesh
}

export function createBuildingVisual(
  scene: Scene,
  asset: WorldAsset,
  def: CardDefinition,
): BuildingVisual {
  const root = new TransformNode(`asset:${asset.id}`, scene)
  const [fw, fd] = rotatedFootprint(asset.footprint, asset.rotation)
  const plan = createArchitecturePlan(def.cardType, `${asset.id}:${def.id}`, [fw, fd])
  const style = TYPE_STYLE[def.cardType]
  const faction = asColor(FACTION_COLOR[def.faction])

  const shellMaterial = new PBRMaterial(`shell:${asset.id}`, scene)
  shellMaterial.albedoColor = asColor(style.color)
  shellMaterial.metallic = plan.materialFamily === 'alloy' ? 0.76 : plan.materialFamily === 'glass' ? 0.42 : 0.16
  shellMaterial.roughness = plan.materialFamily === 'stone' ? 0.76 : 0.34
  shellMaterial.environmentIntensity = 0.72

  const accentMaterial = new PBRMaterial(`signal:${asset.id}`, scene)
  accentMaterial.albedoColor = faction.scale(0.4)
  accentMaterial.emissiveColor = faction
  accentMaterial.emissiveIntensity = 1.15
  accentMaterial.metallic = 0.35
  accentMaterial.roughness = 0.22

  const shell = createLoft(`shell:${asset.id}`, scene, plan.contours, plan.heights, shellMaterial)
  shell.parent = root
  shell.metadata = { assetId: asset.id }

  for (let band = 1; band <= plan.facadeBands; band += 1) {
    const t = band / (plan.facadeBands + 1)
    const mesh = createFacadeBand(
      `band:${asset.id}:${band}`,
      scene,
      interpolateContour(plan, t),
      plan.heights.at(-1)! * t,
      0.08,
      accentMaterial,
    )
    mesh.parent = root
    mesh.metadata = { assetId: asset.id }
  }

  for (let index = 0; index < plan.spirePaths.length; index += 1) {
    const blade = createSpireBlade(
      `spire:${asset.id}:${index}`,
      scene,
      plan.spirePaths[index],
      plan.heights.at(-1)! * (1 + plan.canopyBias * 0.28),
      index % 2 === 0 ? accentMaterial : shellMaterial,
    )
    blade.parent = root
    blade.metadata = { assetId: asset.id }
  }

  const baseSignal = createSignalLoop(
    `foundation-signal:${asset.id}`,
    scene,
    Math.max(fw, fd) * 4 * 0.88,
    accentMaterial,
  )
  baseSignal.position.y = 0.18
  baseSignal.parent = root
  baseSignal.metadata = { assetId: asset.id }

  root.metadata = { assetId: asset.id }
  return { root, accents: [accentMaterial], signalColor: FACTION_COLOR[def.faction] }
}

export function createPlacementGhost(scene: Scene): BuildingVisual {
  const root = new TransformNode('placement-ghost', scene)
  const plan = createArchitecturePlan('Special', 'placement-ghost', [1, 1])
  const material = new PBRMaterial('placement-ghost-signal', scene)
  material.albedoColor = Color3.FromHexString('#4ade80')
  material.emissiveColor = Color3.FromHexString('#4ade80')
  material.emissiveIntensity = 1.4
  material.alpha = 0.24
  material.transparencyMode = PBRMaterial.PBRMATERIAL_ALPHABLEND
  material.backFaceCulling = false
  const mesh = createLoft('placement-hologram', scene, plan.contours, [0, 1.6, 4.8], material)
  mesh.parent = root
  root.setEnabled(false)
  return { root, accents: [material], signalColor: 0x4ade80 }
}

export function createSelectionSignal(scene: Scene): Mesh {
  const material = new PBRMaterial('selection-signal-material', scene)
  material.albedoColor = Color3.FromHexString('#f2c14e')
  material.emissiveColor = Color3.FromHexString('#f2c14e')
  material.emissiveIntensity = 1.6
  material.disableLighting = true
  const mesh = createSignalLoop('selection-signal', scene, 4.65, material)
  mesh.position.y = 0.32
  mesh.setEnabled(false)
  return mesh
}

export function setSignalColor(materials: PBRMaterial[], value: number, intensity: number) {
  const color = asColor(value)
  for (const material of materials) {
    material.albedoColor = color.scale(0.4)
    material.emissiveColor = color
    material.emissiveIntensity = intensity
  }
}

export function tagAsPickable(root: TransformNode, assetId: string) {
  root.getChildMeshes().forEach((mesh) => {
    mesh.isPickable = true
    mesh.metadata = { assetId }
    if (!mesh.isVerticesDataPresent(VertexBuffer.UVKind)) {
      mesh.setVerticesData(VertexBuffer.UVKind, new Array((mesh.getTotalVertices() || 0) * 2).fill(0), true)
    }
  })
}
