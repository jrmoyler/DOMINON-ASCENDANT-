import { describe, expect, it } from 'vitest'
import type { CardType } from '@/game/types'
import { createArchitectureGeometry } from './architectureGeometry'

const families: CardType[] = ['Residential', 'Retail', 'Office', 'Industrial', 'Infrastructure', 'Civic', 'Research', 'Defense', 'Wonder', 'Unit', 'Leader', 'Special']

describe('playable architectural geometry', () => {
  it.each(families)('%s stays inside one gameplay lot with finite normals and a bounded mesh budget', (type) => {
    const parts = createArchitectureGeometry(type, 8, 8)
    expect(parts.length).toBeLessThanOrEqual(24)
    let triangles = 0
    for (const { geometry } of parts) {
      const positions = geometry.getAttribute('position')
      const normals = geometry.getAttribute('normal')
      triangles += positions.count / 3
      for (let i = 0; i < positions.count; i++) {
        expect(Math.abs(positions.getX(i))).toBeLessThanOrEqual(4)
        expect(Math.abs(positions.getZ(i))).toBeLessThanOrEqual(4)
        expect(positions.getY(i)).toBeGreaterThanOrEqual(0)
        expect(Number.isFinite(normals.getX(i) + normals.getY(i) + normals.getZ(i))).toBe(true)
      }
      geometry.dispose()
    }
    expect(triangles).toBeLessThan(4000)
  })

  it('retains readable identity components rather than recoloring one shared silhouette', () => {
    for (const [type, detail] of [['Residential', 'courtyard-side-wings'], ['Industrial', 'sawtooth-factory-roof'], ['Infrastructure', 'water-tanks'], ['Civic', 'civic-colonnade'], ['Research', 'observatory'], ['Defense', 'watchtowers'], ['Retail', 'market-awning'], ['Wonder', 'crown']] as const) {
      const parts = createArchitectureGeometry(type, 16, 16)
      expect(parts.some((part) => part.name === detail)).toBe(true)
      parts.forEach((part) => part.geometry.dispose())
    }
  })
})
