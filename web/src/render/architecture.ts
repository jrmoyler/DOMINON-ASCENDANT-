import type { CardType } from '@/game/types'

export type PlanPoint = readonly [x: number, z: number]

export interface ArchitecturePlan {
  signature: string
  contours: PlanPoint[][]
  heights: number[]
  facadeBands: number
  spirePaths: PlanPoint[][]
  canopyBias: number
  materialFamily: 'ceramic' | 'alloy' | 'stone' | 'bio-composite' | 'glass'
}

interface FamilyProfile {
  sides: number
  height: number
  bands: number
  taper: number
  twist: number
  spires: number
  canopyBias: number
  material: ArchitecturePlan['materialFamily']
}

const FAMILY: Record<CardType, FamilyProfile> = {
  Residential: { sides: 10, height: 13, bands: 6, taper: 0.76, twist: 0.08, spires: 2, canopyBias: 0.64, material: 'ceramic' },
  Retail: { sides: 12, height: 9, bands: 4, taper: 1.08, twist: -0.05, spires: 1, canopyBias: 0.82, material: 'glass' },
  Office: { sides: 9, height: 24, bands: 9, taper: 0.58, twist: 0.16, spires: 3, canopyBias: 0.38, material: 'glass' },
  Industrial: { sides: 14, height: 14, bands: 5, taper: 0.88, twist: -0.11, spires: 4, canopyBias: 0.28, material: 'alloy' },
  Infrastructure: { sides: 11, height: 12, bands: 7, taper: 0.46, twist: 0.22, spires: 3, canopyBias: 0.18, material: 'alloy' },
  Civic: { sides: 16, height: 15, bands: 5, taper: 0.72, twist: 0.03, spires: 2, canopyBias: 0.75, material: 'stone' },
  Research: { sides: 13, height: 21, bands: 8, taper: 0.42, twist: 0.3, spires: 4, canopyBias: 0.34, material: 'ceramic' },
  Defense: { sides: 8, height: 12, bands: 4, taper: 0.9, twist: 0.12, spires: 5, canopyBias: 0.12, material: 'alloy' },
  Wonder: { sides: 18, height: 36, bands: 12, taper: 0.28, twist: 0.44, spires: 6, canopyBias: 0.9, material: 'bio-composite' },
  Unit: { sides: 9, height: 7, bands: 3, taper: 0.52, twist: -0.18, spires: 2, canopyBias: 0.16, material: 'alloy' },
  Leader: { sides: 15, height: 18, bands: 7, taper: 0.36, twist: 0.25, spires: 5, canopyBias: 0.86, material: 'stone' },
  Special: { sides: 17, height: 29, bands: 10, taper: 0.24, twist: -0.38, spires: 7, canopyBias: 1, material: 'bio-composite' },
}

function hashUnit(seed: string): number {
  let hash = 2166136261
  for (let i = 0; i < seed.length; i += 1) {
    hash ^= seed.charCodeAt(i)
    hash = Math.imul(hash, 16777619)
  }
  return (hash >>> 0) / 0xffffffff
}

function contour(
  sides: number,
  width: number,
  depth: number,
  scale: number,
  rotation: number,
  pulse: number,
): PlanPoint[] {
  return Array.from({ length: sides }, (_, index) => {
    const angle = rotation + (index / sides) * Math.PI * 2
    const authoredRhythm = 0.82 + 0.12 * Math.sin(index * 2.399 + pulse * Math.PI * 2)
    return [
      Math.cos(angle) * width * scale * authoredRhythm,
      Math.sin(angle) * depth * scale * authoredRhythm,
    ] as const
  })
}

export function createArchitecturePlan(
  cardType: CardType,
  assetSeed: string,
  footprint: readonly [number, number],
): ArchitecturePlan {
  const profile = FAMILY[cardType]
  const pulse = hashUnit(`${cardType}:${assetSeed}`)
  const width = footprint[0] * 4 * 0.98
  const depth = footprint[1] * 4 * 0.98
  const contours = [
    contour(profile.sides, width, depth, 1, pulse * 0.12, pulse),
    contour(profile.sides, width, depth, 0.84, profile.twist, pulse + 0.17),
    contour(profile.sides, width, depth, profile.taper, profile.twist * 2.1, pulse + 0.41),
  ]

  const spirePaths = Array.from({ length: profile.spires }, (_, index) => {
    const angle = (index / profile.spires) * Math.PI * 2 + pulse
    const reach = 0.2 + (index % 3) * 0.09
    return [
      [Math.cos(angle) * width * reach, Math.sin(angle) * depth * reach] as const,
      [Math.cos(angle + profile.twist) * width * reach * 0.55, Math.sin(angle + profile.twist) * depth * reach * 0.55] as const,
      [Math.cos(angle + profile.twist * 2) * width * 0.04, Math.sin(angle + profile.twist * 2) * depth * 0.04] as const,
    ]
  })

  return {
    signature: `${cardType}:${profile.sides}:${profile.bands}:${profile.spires}:${profile.material}`,
    contours,
    heights: [0, profile.height * (0.42 + pulse * 0.08), profile.height],
    facadeBands: profile.bands,
    spirePaths,
    canopyBias: profile.canopyBias,
    materialFamily: profile.material,
  }
}
