import { describe, expect, it } from 'vitest'
import { trafficPose, createCivicTraffic } from './civicTraffic'
import { NullEngine } from '@babylonjs/core/Engines/nullEngine'
import { Scene } from '@babylonjs/core/scene'

describe('ambient boulevard traffic', () => {
  it('constructs eight visible nonpickable instances and updates their actual transforms', () => {
    const engine = new NullEngine()
    const scene = new Scene(engine)
    try {
      const traffic = createCivicTraffic(scene)
      const vehicles = scene.meshes.filter((mesh) => mesh.name.startsWith('boulevard-service:'))
      expect(vehicles).toHaveLength(8)
      expect(vehicles.every((mesh) => mesh.isVisible && !mesh.isPickable)).toBe(true)
      const before = vehicles.map((mesh) => mesh.position.clone())
      traffic.update(5)
      expect(vehicles.every((mesh, index) => !mesh.position.equals(before[index]))).toBe(true)
      expect(vehicles.every((mesh) => Math.max(Math.abs(mesh.position.x), Math.abs(mesh.position.z)) === 138)).toBe(true)
    } finally {
      scene.dispose()
      engine.dispose()
    }
  })
  it('loops on connected perimeter roads without crossing buildable cells', () => {
    for (let distance = 0; distance <= 2000; distance += 3) {
      const pose = trafficPose(distance)
      expect(Math.max(Math.abs(pose.x), Math.abs(pose.z))).toBe(138)
      const next = trafficPose(distance + 0.1)
      expect(Math.hypot(next.x - pose.x, next.z - pose.z)).toBeLessThanOrEqual(0.101)
    }
    expect(trafficPose(0)).toEqual(trafficPose(1104))
  })
})
