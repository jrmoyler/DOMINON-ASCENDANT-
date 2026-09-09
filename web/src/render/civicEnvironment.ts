import { BoxGeometry, PlaneGeometry, CylinderGeometry, IcosahedronGeometry, Matrix4, Quaternion, Vector3 as ThreeVector, type BufferGeometry } from 'three'
import { Color3 } from '@babylonjs/core/Maths/math.color'
import { Mesh } from '@babylonjs/core/Meshes/mesh'
import { VertexData } from '@babylonjs/core/Meshes/mesh.vertexData'
import { StandardMaterial } from '@babylonjs/core/Materials/standardMaterial'
import type { Scene } from '@babylonjs/core/scene'
import { CELL_METRES, GRID_SIZE } from '@/game/grid'

/** Authored city context is outside the playable district; it never claims a cell. */
const HALF = GRID_SIZE * CELL_METRES / 2
const COLORS = { paving: '#827d6c', asphalt: '#343e40', marking: '#c7bea3', stone: '#b4ac92', roof: '#4c5856', glazing: '#486b70', foliage: '#52634b', bark: '#645344', water: '#527a80', light: '#efc984' }
type Surface = keyof typeof COLORS
interface Batch { positions: number[]; normals: number[]; indices: number[] }

export function createCivicEnvironment(scene: Scene): Mesh[] {
  const batches = new Map<Surface, Batch>()
  const transform = new Matrix4()
  const rotation = new Quaternion()
  const boxGeometry = new BoxGeometry(1, 1, 1)
  const canopyGeometry = new IcosahedronGeometry(1, 1)
  const glazingGeometry = new PlaneGeometry(1, 1)
  const trunkGeometry = new CylinderGeometry(0.7, 1, 1, 6)
  function append(surface: Surface, geometry: BufferGeometry, x: number, y: number, z: number, sx: number, sy: number, sz: number, angle = 0) {
    const batch = batches.get(surface) ?? { positions: [], normals: [], indices: [] }
    batches.set(surface, batch)
    rotation.setFromAxisAngle(new ThreeVector(0, 1, 0), angle)
    transform.compose(new ThreeVector(x, y, z), rotation, new ThreeVector(sx, sy, sz))
    const transformed = geometry.clone().applyMatrix4(transform)
    const offset = batch.positions.length / 3
    batch.positions.push(...Array.from(transformed.getAttribute('position').array))
    batch.normals.push(...Array.from(transformed.getAttribute('normal').array))
    const indices = transformed.index?.array ?? Array.from({ length: transformed.getAttribute('position').count }, (_, index) => index)
    // Three uses counterclockwise front faces; Babylon defaults to clockwise.
    for (let index = 0; index < indices.length; index += 3) {
      batch.indices.push(indices[index] + offset, indices[index + 2] + offset, indices[index + 1] + offset)
    }
    transformed.dispose()
  }
  function box(surface: Surface, x: number, y: number, z: number, w: number, h: number, d: number, angle = 0) {
    append(surface, boxGeometry, x, y, z, w, h, d, angle)
  }
  function tree(x: number, z: number, seed: number) {
    const height = 4.5 + (seed % 4) * 0.65
    append('bark', trunkGeometry, x, height * 0.36, z, 0.24, height * 0.72, 0.24)
    append('foliage', canopyGeometry, x, height, z, 2.1, 2.7, 1.8, seed)
    append('foliage', canopyGeometry, x + 0.9, height - 0.6, z - 0.6, 1.5, 1.9, 1.8, seed + 0.4)
  }

  // Inset plot surfaces make the buildable district readable without a luminous wireframe.
  for (let x = 0; x < GRID_SIZE; x += 1) for (let z = 0; z < GRID_SIZE; z += 1) {
    const cx = (x + 0.5) * CELL_METRES - HALF
    const cz = (z + 0.5) * CELL_METRES - HALF
    box('paving', cx, 0, cz, CELL_METRES - 0.2, 0.08, CELL_METRES - 0.2)
  }
  // Four connected boulevards frame the district, with kerbs and lane markings.
  for (const side of [-1, 1]) {
    box('paving', side * (HALF + 9), -0.04, 0, 18, 0.3, HALF * 2 + 36)
    box('paving', 0, -0.04, side * (HALF + 9), HALF * 2, 0.3, 18)
    box('asphalt', side * (HALF + 8), 0.14, 0, 9, 0.08, HALF * 2 + 18)
    box('asphalt', 0, 0.14, side * (HALF + 8), HALF * 2 + 18, 0.08, 9)
    box('stone', side * (HALF + 0.7), 0.3, 0, 0.5, 0.6, HALF * 2)
    box('stone', 0, 0.3, side * (HALF + 0.7), HALF * 2, 0.6, 0.5)
    for (let p = -HALF + 6; p < HALF; p += 12) {
      box('marking', side * (HALF + 8), 0.2, p, 0.16, 0.03, 4)
      box('marking', p, 0.2, side * (HALF + 8), 4, 0.03, 0.16)
      tree(side * (HALF + 16), p, p + HALF)
      tree(p, side * (HALF + 16), p + HALF + 2)
    }
    for (let p = -HALF + 10; p < HALF; p += 32) {
      box('roof', side * (HALF + 2.4), 2.5, p, 0.18, 5, 0.18)
      box('roof', side * (HALF + 3.1), 5, p, 1.6, 0.15, 0.28)
      box('light', side * (HALF + 3.4), 4.9, p, 0.8, 0.1, 0.2)
    }
  }
  // River, masonry quay, and the two approach bridges establish geography.
  box('water', 0, -1.3, -HALF - 75, 720, 0.12, 62)
  box('stone', 0, -0.25, -HALF - 40, 520, 2, 4)
  box('stone', 0, -0.25, -HALF - 110, 520, 2, 4)
  for (const x of [-86, 86]) {
    box('paving', x, 0.2, -HALF - 73, 15, 2.2, 78)
    box('asphalt', x, 1.32, -HALF - 73, 10, 0.06, 78)
    for (const edge of [-6.5, 6.5]) box('stone', x + edge, 2.1, -HALF - 73, 0.5, 1.4, 78)
    for (const z of [-HALF - 92, -HALF - 56]) box('stone', x, -3, z, 12, 7, 3)
  }
  // Older city blocks beyond the district: repeated bays, stepped roofs, service cores.
  for (let index = 0; index < 30; index += 1) {
    const side = index < 12 ? -1 : 1
    const x = index < 12 ? (index - 5.5) * 29 : side * (HALF + 36 + (index % 3) * 24)
    const z = index < 12 ? -HALF - 138 - (index % 3) * 23 : ((index - 12) / 3 | 0) * 39 - 100
    const width = 15 + (index % 3) * 3
    const depth = 14 + (index % 4) * 2
    const height = 13 + (index * 7 % 23)
    box('stone', x, height / 2, z, width, height, depth)
    box('paving', x, 0.3, z, width + 4, 0.6, depth + 4)
    box('roof', x, height + 0.35, z, width + 0.5, 0.7, depth + 0.5)
    box('roof', x - width * 0.16, height + 1.8, z, width * 0.4, 3, depth * 0.3)
    for (let level = 3; level < height - 1; level += 3.5) for (let bay = -width / 2 + 2; bay < width / 2 - 1; bay += 3) {
      append('glazing', glazingGeometry, x + bay, level, z + depth / 2 + 0.015, 1.6, 1.7, 1)
      append('glazing', glazingGeometry, x + bay, level, z - depth / 2 - 0.015, 1.6, 1.7, 1)
    }
    tree(x + width / 2 + 3, z + depth / 2, index)
  }
  boxGeometry.dispose(); canopyGeometry.dispose(); trunkGeometry.dispose(); glazingGeometry.dispose()
  return Array.from(batches, ([surface, batch]) => {
    const mesh = new Mesh(`civic-context:${surface}`, scene)
    const data = new VertexData()
    data.positions = batch.positions; data.indices = batch.indices; data.normals = batch.normals
    data.applyToMesh(mesh)
    const material = new StandardMaterial(`civic-context:${surface}`, scene)
    material.diffuseColor = Color3.FromHexString(COLORS[surface])
    material.specularColor = Color3.FromHexString(surface === 'water' ? '#809da0' : '#131715')
    if (surface === 'light') material.emissiveColor = Color3.FromHexString(COLORS.light).scale(0.3)
    material.backFaceCulling = surface !== 'glazing'
    mesh.material = material
    mesh.receiveShadows = surface !== 'water'
    mesh.isPickable = false
    mesh.freezeWorldMatrix()
    return mesh
  })
}
