import { describe, expect, it } from 'vitest'
import { NullEngine } from '@babylonjs/core/Engines/nullEngine'
import { Scene } from '@babylonjs/core/scene'
import { definition, FOUNDER_HALL_ID } from '@/game/content'
import type { WorldAsset } from '@/game/types'
import { createBuildingVisual, tagAsPickable } from './buildings'

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
      rotation: 0,
      cyclesRemaining: 0,
      operational: true,
      brownout: false,
      staffed: 12,
    }

    const visual = createBuildingVisual(scene, asset, definition(FOUNDER_HALL_ID))
    tagAsPickable(visual.root, asset.id)
    const surfaces = visual.root.getChildMeshes()
    const shell = surfaces.find((mesh) => mesh.name.startsWith('shell:'))

    expect(surfaces.length).toBeGreaterThanOrEqual(12)
    expect(shell?.getTotalVertices()).toBeGreaterThan(40)
    expect(surfaces.every((mesh) => mesh.metadata?.assetId === asset.id && mesh.isPickable)).toBe(true)

    scene.dispose()
    engine.dispose()
  })
})
