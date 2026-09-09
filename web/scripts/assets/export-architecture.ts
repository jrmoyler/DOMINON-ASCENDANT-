import { mkdir, writeFile } from 'node:fs/promises'
import { resolve } from 'node:path'
import { Group, Mesh, MeshStandardMaterial } from 'three'
import { GLTFExporter } from 'three/examples/jsm/exporters/GLTFExporter.js'
import { createArchitectureGeometry } from '../../src/render/architectureGeometry'
import type { CardType } from '../../src/game/types'

class NodeFileReader {
  result: ArrayBuffer | string | null = null
  onloadend?: () => void
  readAsArrayBuffer(blob: Blob) { void blob.arrayBuffer().then((buffer) => { this.result = buffer; this.onloadend?.() }) }
  readAsDataURL(blob: Blob) { void blob.arrayBuffer().then((buffer) => { this.result = `data:${blob.type};base64,${Buffer.from(buffer).toString('base64')}`; this.onloadend?.() }) }
}
Object.assign(globalThis, { FileReader: NodeFileReader })
const output = resolve(process.argv[2] ?? '../docs/art-assets/models')
await mkdir(output, { recursive: true })
const families: CardType[] = ['Residential', 'Retail', 'Office', 'Industrial', 'Infrastructure', 'Civic', 'Research', 'Defense', 'Wonder', 'Unit', 'Leader', 'Special']
const colors = { shell: 0x9ca7a5, stone: 0xc7bfaa, metal: 0x4d565b, glass: 0x29424c, signal: 0x6eaba8, garden: 0x526841 }
const manifest = []
for (const type of families) {
  const group = new Group(); group.name = `dominion-${type.toLowerCase()}`
  const parts = createArchitectureGeometry(type, 16, 16)
  const materials = new Map()
  let triangles = 0
  for (const part of parts) {
    let material = materials.get(part.finish)
    if (!material) { material = new MeshStandardMaterial({ color: colors[part.finish], roughness: part.finish === 'glass' ? 0.22 : 0.75, metalness: part.finish === 'metal' ? 0.65 : 0.1 }); material.name = part.finish; materials.set(part.finish, material) }
    const mesh = new Mesh(part.geometry, material); mesh.name = part.name; group.add(mesh)
    triangles += part.geometry.getAttribute('position').count / 3
  }
  const binary = await new GLTFExporter().parseAsync(group, { binary: true, onlyVisible: true }) as ArrayBuffer
  const file = `${type.toLowerCase()}.glb`
  await writeFile(resolve(output, file), Buffer.from(binary))
  manifest.push({ family: type, file, bytes: binary.byteLength, triangles, meshes: parts.length, materials: materials.size, units: 'metres', footprint: [16, 16], up: '+Y', pivot: 'ground-centre', source: 'web/src/render/architectureGeometry.ts' })
  parts.forEach(({ geometry }) => geometry.dispose()); materials.forEach((material) => material.dispose())
}
await writeFile(resolve(output, 'manifest.json'), JSON.stringify(manifest, null, 2) + '\n')
console.log(JSON.stringify(manifest, null, 2))
