import { describe, expect, it } from 'vitest'
import { NullEngine } from '@babylonjs/core/Engines/nullEngine'
import { Scene } from '@babylonjs/core/scene'
import { createCivicEnvironment } from './civicEnvironment'

describe('civic environment budget and interaction boundaries', () => {
  it('batches the full environment into a bounded draw budget without intercepting city picks', () => {
    const engine = new NullEngine()
    const scene = new Scene(engine)
    const meshes = createCivicEnvironment(scene)
    expect(meshes.length).toBeLessThanOrEqual(10)
    expect(meshes.every((mesh) => !mesh.isPickable)).toBe(true)
    expect(meshes.reduce((count, mesh) => count + mesh.getTotalVertices(), 0)).toBeLessThan(130_000)
    for (const mesh of meshes) {
      const positions = mesh.getVerticesData('position')!
      expect(Array.from(positions).every(Number.isFinite)).toBe(true)
      expect(mesh.getIndices()!.every((index) => index >= 0 && index < mesh.getTotalVertices())).toBe(true)
    }
    expect(meshes.find((mesh) => mesh.name === 'civic-context:water')).toBeDefined()
    scene.dispose()
    engine.dispose()
  })
})
