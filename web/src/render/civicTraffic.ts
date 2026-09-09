import '@babylonjs/core/Meshes/instancedMesh'
import { BoxGeometry, CylinderGeometry, Matrix4, Quaternion, Vector3 as ThreeVector, type BufferGeometry } from 'three'
import { Mesh } from '@babylonjs/core/Meshes/mesh'
import { VertexData } from '@babylonjs/core/Meshes/mesh.vertexData'
import { StandardMaterial } from '@babylonjs/core/Materials/standardMaterial'
import { Color3 } from '@babylonjs/core/Maths/math.color'
import type { Scene } from '@babylonjs/core/scene'
import { CELL_METRES, GRID_SIZE } from '@/game/grid'

const ROAD = GRID_SIZE * CELL_METRES / 2 + 10
const SIDE = ROAD * 2

/** Clockwise route stays on the existing boulevard; never enters buildable cells. */
export function trafficPose(distance: number) {
  const travel = ((distance % (SIDE * 4)) + SIDE * 4) % (SIDE * 4)
  const side = Math.floor(travel / SIDE)
  const p = travel % SIDE
  if (side === 0) return { x: -ROAD + p, z: -ROAD, angle: Math.PI / 2 }
  if (side === 1) return { x: ROAD, z: -ROAD + p, angle: 0 }
  if (side === 2) return { x: ROAD - p, z: ROAD, angle: -Math.PI / 2 }
  return { x: -ROAD, z: ROAD - p, angle: Math.PI }
}

export function createCivicTraffic(scene: Scene) {
  const positions: number[] = [], normals: number[] = [], indices: number[] = [], colors: number[] = []
  function part(geometry: BufferGeometry, x: number, y: number, z: number, width: number, height: number, depth: number, paint: string, wheel = false) {
    const rotation = new Quaternion()
    if (wheel) rotation.setFromAxisAngle(new ThreeVector(0, 0, 1), Math.PI / 2)
    geometry.applyMatrix4(new Matrix4().compose(new ThreeVector(x, y, z), rotation, new ThreeVector(width, height, depth)))
    const offset = positions.length / 3
    const color = Color3.FromHexString(paint)
    positions.push(...Array.from(geometry.getAttribute('position').array))
    normals.push(...Array.from(geometry.getAttribute('normal').array))
    for (let index = 0; index < geometry.getAttribute('position').count; index += 1) colors.push(color.r, color.g, color.b, 1)
    const source = geometry.index!.array
    for (let index = 0; index < source.length; index += 3) indices.push(source[index] + offset, source[index + 2] + offset, source[index + 1] + offset)
    geometry.dispose()
  }
  part(new BoxGeometry(), 0, 0.65, 0, 1.55, 0.55, 3.2, '#c4b596')
  part(new BoxGeometry(), 0, 1.1, -0.22, 1.3, 0.55, 1.75, '#3c555b')
  part(new BoxGeometry(), 0, 1.43, -0.22, 1.4, 0.12, 1.85, '#d5c7a7')
  part(new BoxGeometry(), 0, 0.59, 1.65, 1.25, 0.16, 0.08, '#f5e1ab')
  part(new BoxGeometry(), 0, 0.59, -1.65, 1.25, 0.12, 0.08, '#944b35')
  for (const x of [-0.76, 0.76]) for (const z of [-1.05, 1.05]) {
    part(new CylinderGeometry(1, 1, 1, 10), x, 0.46, z, 0.35, 0.23, 0.35, '#2a3130', true)
  }
  const master = new Mesh('service-vehicle-template', scene)
  const data = new VertexData()
  data.positions = positions; data.normals = normals; data.indices = indices; data.colors = colors
  data.applyToMesh(master)
  const material = new StandardMaterial('service-vehicle-paint', scene)
  material.diffuseColor = Color3.White()
  material.specularColor = new Color3(0.12, 0.12, 0.12)
  master.material = material
  master.isPickable = false
  master.isVisible = false
  const vehicles = Array.from({ length: 8 }, (_, index) => {
    const vehicle = master.createInstance(`boulevard-service:${index}`)
    vehicle.isPickable = false
    return vehicle
  })
  const update = (seconds: number) => {
    vehicles.forEach((vehicle, index) => {
      const pose = trafficPose(index * SIDE * 0.5 + seconds * 5)
      vehicle.position.set(pose.x, 0.2, pose.z)
      vehicle.rotation.y = pose.angle
    })
  }
  update(0)
  return { update }
}
