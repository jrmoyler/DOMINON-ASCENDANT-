import { CatmullRomCurve3, Color, Vector3 } from 'three'

export interface AtmospherePoint {
  x: number
  y: number
  z: number
}

export interface AtmosphereBand {
  points: AtmospherePoint[]
  color: string
  alpha: number
}

function hashUnit(seed: string): number {
  let hash = 0x811c9dc5
  for (let index = 0; index < seed.length; index += 1) {
    hash ^= seed.charCodeAt(index)
    hash = Math.imul(hash, 0x01000193)
  }
  return (hash >>> 0) / 0xffffffff
}

/**
 * Three.js owns this isolated curve-authoring boundary. Babylon consumes the
 * resulting neutral points, so the two renderers never compete for a canvas.
 */
export function createStrategicAtmosphere(seed: string): AtmosphereBand[] {
  return Array.from({ length: 5 }, (_, band) => {
    const phase = hashUnit(`${seed}:${band}`) * Math.PI * 2
    const altitude = 64 + band * 8
    const curve = new CatmullRomCurve3([
      new Vector3(-168, altitude + Math.sin(phase) * 5, -102 + band * 14),
      new Vector3(-76, altitude + 12 + Math.cos(phase) * 8, -66 + band * 8),
      new Vector3(12, altitude - 4 + Math.sin(phase * 1.7) * 7, -48 + band * 3),
      new Vector3(92, altitude + 10 + Math.cos(phase * 1.3) * 6, -72 - band * 5),
      new Vector3(168, altitude + Math.sin(phase + 0.8) * 5, -112 - band * 8),
    ])
    const base = new Color(band % 2 === 0 ? '#4dd8e6' : '#b98cff')
    base.offsetHSL(band * 0.018, -0.04, band * 0.025)
    return {
      points: curve.getPoints(47).map((point) => ({ x: point.x, y: point.y, z: point.z })),
      color: `#${base.getHexString()}`,
      alpha: 0.09 + band * 0.018,
    }
  })
}
