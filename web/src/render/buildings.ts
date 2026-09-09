import { Color3 } from '@babylonjs/core/Maths/math.color'
import { Mesh } from '@babylonjs/core/Meshes/mesh'
import { TransformNode } from '@babylonjs/core/Meshes/transformNode'
import { VertexData } from '@babylonjs/core/Meshes/mesh.vertexData'
import { VertexBuffer } from '@babylonjs/core/Buffers/buffer'
import { PBRMaterial } from '@babylonjs/core/Materials/PBR/pbrMaterial'
import type { Scene } from '@babylonjs/core/scene'
import type { CardDefinition, WorldAsset } from '@/game/types'
import { createArchitecturePlan, type PlanPoint } from './architecture'
import { FACTION_COLOR, TYPE_STYLE } from './palette'
import { createArchitectureGeometry, type ArchitectureFinish } from './architectureGeometry'
import { mergeGeometries } from 'three/examples/jsm/utils/BufferGeometryUtils.js'
import type { BufferGeometry } from 'three'

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
    indices.push(index * 2, index * 2 + 1, next * 2, next * 2, index * 2 + 1, next * 2 + 1)
  }
  const mesh = new Mesh(name, scene)
  applyVertexData(mesh, positions, indices)
  mesh.material = material
  return mesh
}

// Cache immutable Three-authored geometry; instances own their Babylon materials.
const geometryCache = new Map<string, { name: string; finish: ArchitectureFinish; positions: number[]; normals: number[]; indices: number[] }[]>()

export function createBuildingVisual(scene: Scene, asset: WorldAsset, def: CardDefinition): BuildingVisual {
  const root = new TransformNode(`asset:${asset.id}`, scene)
  const [fw, fd] = asset.footprint
  root.rotation.y = asset.rotation * Math.PI / 2
  const key = `${def.cardType}:${fw}:${fd}`
  let parts = geometryCache.get(key)
  if (!parts) {
    const authoredParts = createArchitectureGeometry(def.cardType, fw * 8, fd * 8)
    const batches = new Map<ArchitectureFinish, BufferGeometry[]>()
    for (const part of authoredParts) {
      const batch = batches.get(part.finish) ?? []
      batch.push(part.geometry); batches.set(part.finish, batch)
    }
    parts = [...batches].map(([finish, geometries]) => {
      const name = finish
      const geometry = mergeGeometries(geometries, false)!
      geometries.forEach((part) => part.dispose())
      const positions = Array.from(geometry.getAttribute('position').array)
      const normals = Array.from(geometry.getAttribute('normal').array)
      const indices = Array.from({ length: positions.length / 3 }, (_, index) => index)
      // Three uses counterclockwise triangles; Babylon's default is clockwise.
      for (let i = 0; i < indices.length; i += 3) [indices[i + 1], indices[i + 2]] = [indices[i + 2], indices[i + 1]]
      geometry.dispose()
      return { name, finish, positions, normals, indices }
    })
    geometryCache.set(key, parts)
  }
  const finishes: Record<ArchitectureFinish, [number, number, number]> = {
    shell: [TYPE_STYLE[def.cardType].color, 0.12, 0.72], stone: [0xc7bfaa, 0.04, 0.85],
    metal: [0x4d565b, 0.65, 0.4], glass: [0x29424c, 0.45, 0.22],
    signal: [FACTION_COLOR[def.faction], 0.2, 0.4], garden: [0x526841, 0, 0.95],
  }
  const materials = Object.fromEntries(Object.entries(finishes).map(([finish, [color, metallic, roughness]]) => {
    const material = new PBRMaterial(`${finish}:${asset.id}`, scene)
    material.albedoColor = asColor(color)
    material.metallic = metallic
    material.roughness = roughness
    material.environmentIntensity = 0.65
    if (finish === 'signal') { material.emissiveColor = asColor(color); material.emissiveIntensity = 0.3 }
    return [finish, material]
  })) as Record<ArchitectureFinish, PBRMaterial>
  for (const part of parts) {
    const mesh = new Mesh(`${part.name}:${asset.id}`, scene)
    const data = new VertexData()
    data.positions = part.positions; data.normals = part.normals; data.indices = part.indices
    data.applyToMesh(mesh)
    mesh.material = materials[part.finish]
    mesh.receiveShadows = true
    mesh.parent = root
    mesh.metadata = { assetId: asset.id, architecturalPart: part.name }
  }
  root.metadata = { assetId: asset.id }
  return { root, accents: [materials.signal], signalColor: FACTION_COLOR[def.faction] }
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
