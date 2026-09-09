import { describe, expect, it } from 'vitest'
import { NullEngine } from '@babylonjs/core/Engines/nullEngine'
import { Scene } from '@babylonjs/core/scene'
import { VertexData } from '@babylonjs/core/Meshes/mesh.vertexData'
import { definition, FOUNDER_HALL_ID } from '@/game/content'
import type { WorldAsset } from '@/game/types'
import { createBuildingVisual, createSelectionSignal, tagAsPickable } from './buildings'

describe('Babylon authored building visuals', () => {
  it('ships a multi-mesh landmark silhouette with selectable authored surfaces', () => {
    const engine = new NullEngine()
    const scene = new Scene(engine)
    const asset: WorldAsset = {
      id: 'founder-hall-test',
      definitionId: FOUNDER_HALL_ID,
      x: 15,
      y: 15,
      footprint: [2, 2],
      rotation: 1,
      cyclesRemaining: 0,
      operational: true,
      brownout: false,
      staffed: 12,
    }

    const visual = createBuildingVisual(scene, asset, definition(FOUNDER_HALL_ID))
    tagAsPickable(visual.root, asset.id)
    const surfaces = visual.root.getChildMeshes()
    const shell = surfaces.find((mesh) => mesh.name.startsWith('shell:'))

    expect(surfaces.length).toBe(6) // One draw per finish, independent of facade detail count.
    expect(shell?.getTotalVertices()).toBeGreaterThan(40)
    expect(visual.root.rotation.y).toBe(Math.PI / 2)
    expect(surfaces.every((mesh) => mesh.metadata?.assetId === asset.id && mesh.isPickable)).toBe(true)

    for (const mesh of surfaces) {
      const positions = mesh.getVerticesData('position')!
      const actual = mesh.getVerticesData('normal')!
      const computed: number[] = []
      VertexData.ComputeNormals(positions, mesh.getIndices()!, computed)
      // Compare the face with its three averaged shading normals. Individual
      // smooth cylinder vertex normals intentionally differ from the face normal.
      for (let i = 0; i < actual.length; i += 9) {
        const nx = actual[i] + actual[i + 3] + actual[i + 6]
        const ny = actual[i + 1] + actual[i + 4] + actual[i + 7]
        const nz = actual[i + 2] + actual[i + 5] + actual[i + 8]
        const length = Math.hypot(nx, ny, nz)
        const dot = (nx * computed[i] + ny * computed[i + 1] + nz * computed[i + 2]) / length
        expect(dot).toBeGreaterThan(0.98)
      }
    }
    const selection = createSelectionSignal(scene)
    const normals = selection.getVerticesData('normal')!
    for (let i = 1; i < normals.length; i += 3) expect(normals[i]).toBeGreaterThan(0.99)

    scene.dispose()
    engine.dispose()
  })
})
