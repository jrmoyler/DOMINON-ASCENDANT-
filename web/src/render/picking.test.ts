import { describe, expect, it } from 'vitest'
import { NullEngine } from '@babylonjs/core/Engines/nullEngine'
import { Scene } from '@babylonjs/core/scene'
import { ArcRotateCamera } from '@babylonjs/core/Cameras/arcRotateCamera'
import { Vector3 } from '@babylonjs/core/Maths/math.vector'
// Deliberately import only the game's scene module: it must register Babylon's
// ray side effect itself, or every click on the city misses.
import { createTerrain } from './Scene'

describe('city picking', () => {
  it('a screen-space pick lands on the terrain using only the scene module imports', () => {
    const engine = new NullEngine({ renderWidth: 800, renderHeight: 600, textureSize: 512, deterministicLockstep: false, lockstepMaxSteps: 1 })
    const scene = new Scene(engine)
    const camera = new ArcRotateCamera('cam', Math.PI * 0.68, Math.PI * 0.29, 175, Vector3.Zero(), scene)
    const ground = createTerrain(scene)
    scene.render()
    const hit = scene.pick(400, 300, (mesh) => mesh === ground, false, camera)
    expect(hit?.hit).toBe(true)
    expect(hit?.pickedPoint).toBeTruthy()
    engine.dispose()
  })
})
