import { afterEach, describe, expect, it } from 'vitest'
import { NullEngine } from '@babylonjs/core/Engines/nullEngine'
import { Scene } from '@babylonjs/core/scene'
import { TransformNode } from '@babylonjs/core/Meshes/transformNode'
import type { Mesh } from '@babylonjs/core/Meshes/mesh'
import { Vector3 } from '@babylonjs/core/Maths/math.vector'
import { Ray } from '@babylonjs/core/Culling/ray'
import { createCellSignals, createCivicLattice, createSkyVault, createTerrain, terrainHeight } from './Scene'

const engines: NullEngine[] = []
function makeScene() { const engine = new NullEngine(); engines.push(engine); return new Scene(engine) }
afterEach(() => { engines.splice(0).forEach((engine) => engine.dispose()) })
function yNormals(mesh: Mesh) { return Array.from(mesh.getVerticesData('normal')!).filter((_, index) => index % 3 === 1) }

describe('world geography and Babylon surface orientation', () => {
  it('faces terrain, lattice, and coverage toward the gameplay camera above the district', () => {
    const scene = makeScene()
    expect(Math.min(...yNormals(createTerrain(scene)))).toBeGreaterThan(0)
    expect(Math.min(...yNormals(createCivicLattice(scene)))).toBeGreaterThan(0.99)
    const root = new TransformNode('coverage', scene)
    createCellSignals(scene, root, [{ cell: 16 * 32 + 16, value: 0x44ccaa, alpha: 0.2, height: 0.14 }])
    expect(Math.min(...yNormals(root.getChildMeshes()[0] as Mesh))).toBeGreaterThan(0.99)
  })

  it('keeps the actual triangulated river bed below water and skyline foundations on level ground', () => {
    const scene = makeScene()
    const terrain = createTerrain(scene)
    terrain.computeWorldMatrix(true)
    for (const x of [-350, -86, 0, 86, 350]) for (const z of [-233, -203, -173]) {
      expect(terrainHeight(x, z)).toBeLessThan(-1.3)
      const pick = new Ray(new Vector3(x, 10, z), new Vector3(0, -1, 0)).intersectsMesh(terrain)
      expect(pick.hit).toBe(true)
      expect(pick.pickedPoint!.y).toBeLessThan(-1.3)
    }
    for (const [x, z] of [[0, 0], [-160, -270], [160, -342], [212, 100], [0, -168], [0, -238]]) {
      expect(terrainHeight(x, z)).toBe(0)
    }
  })

  it('orients the sky vault inward around the camera', () => {
    const sky = createSkyVault(makeScene())
    const positions = sky.getVerticesData('position')!
    const normals = sky.getVerticesData('normal')!
    const i = (9 * 56 + 14) * 3
    const dot = positions[i] * normals[i] + (positions[i + 1] + 1024 * 0.08) * normals[i + 1] + positions[i + 2] * normals[i + 2]
    expect(dot).toBeLessThan(0)
  })
})
