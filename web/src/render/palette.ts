import type { CardType, Faction } from '@/game/types'

/**
 * Visual direction per Content/UI/Manifests/VerticalSliceUI.json:
 * tone "industrial_civic_synara", low-chrome HUD, centre kept clear.
 */
export const PALETTE = {
  background: 0x080d16,
  fog: 0x0a1020,
  ground: 0x121a29,
  groundEdge: 0x0d1420,
  gridLine: 0x1e2b40,
  gridMajor: 0x2b3d59,
  hoverValid: 0x4ade80,
  hoverInvalid: 0xf05252,
  selection: 0xf2c14e,
  keyLight: 0xdce6f5,
  fillLight: 0x2a4a7a,
  rim: 0x4dd8e6,
} as const

export const FACTION_COLOR: Record<Faction, number> = {
  Synara: 0x4dd8e6,
  Forgeweave: 0xe8913a,
  EdenCircuit: 0x6fcf7f,
  Universal: 0x8ea3c0,
  Fusion: 0xb98cff,
  Special: 0xf2c14e,
}

export const FACTION_CSS: Record<Faction, string> = {
  Synara: '#4dd8e6',
  Forgeweave: '#e8913a',
  EdenCircuit: '#6fcf7f',
  Universal: '#8ea3c0',
  Fusion: '#b98cff',
  Special: '#f2c14e',
}

/** Base body colour and silhouette height (in metres) per card type. */
export const TYPE_STYLE: Record<CardType, { color: number; height: number }> = {
  Residential: { color: 0x3f4d66, height: 11 },
  Retail: { color: 0x4a4f6b, height: 8 },
  Office: { color: 0x394a68, height: 20 },
  Industrial: { color: 0x53483f, height: 12 },
  Infrastructure: { color: 0x2f4152, height: 9 },
  Civic: { color: 0x45526b, height: 10 },
  Research: { color: 0x36486b, height: 16 },
  Defense: { color: 0x4b4640, height: 9 },
  Wonder: { color: 0x5c4f6e, height: 28 },
  Unit: { color: 0x4d4a44, height: 5 },
  Leader: { color: 0x55506b, height: 7 },
  Special: { color: 0x6b5a33, height: 18 },
}

export const TYPE_CSS: Record<CardType, string> = {
  Residential: '#7f93b8',
  Retail: '#c2a8f0',
  Office: '#6fa8e6',
  Industrial: '#e8913a',
  Infrastructure: '#4dd8e6',
  Civic: '#9fd4a8',
  Research: '#8ab6ff',
  Defense: '#e0c07a',
  Wonder: '#d6a6ff',
  Unit: '#c9b79a',
  Leader: '#f0b8d0',
  Special: '#f2c14e',
}
