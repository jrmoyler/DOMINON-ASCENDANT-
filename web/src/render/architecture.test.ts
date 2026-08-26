import { describe, expect, it } from 'vitest'
import type { CardType } from '@/game/types'
import { createArchitecturePlan } from './architecture'

const CARD_TYPES: CardType[] = [
  'Residential',
  'Retail',
  'Office',
  'Industrial',
  'Infrastructure',
  'Civic',
  'Research',
  'Defense',
  'Wonder',
  'Unit',
  'Leader',
  'Special',
]

describe('authored architecture plans', () => {
  it('reconstructs the same authored silhouette for a stable asset seed', () => {
    const first = createArchitecturePlan('Research', 'asset-144', [2, 3])
    const second = createArchitecturePlan('Research', 'asset-144', [2, 3])

    expect(second).toEqual(first)
  })

  it('keeps every facade vertex inside the occupied gameplay footprint', () => {
    const plan = createArchitecturePlan('Industrial', 'forge-7', [3, 2])

    expect(plan.contours.flat().every(([x, z]) => Math.abs(x) <= 11.76 && Math.abs(z) <= 7.84)).toBe(true)
  })

  it('gives every card family a distinct non-primitive silhouette and layered detail', () => {
    const plans = CARD_TYPES.map((type) => createArchitecturePlan(type, 'shared-seed', [2, 2]))
    const signatures = plans.map((plan) => plan.signature)

    expect(new Set(signatures).size).toBe(CARD_TYPES.length)
    for (const plan of plans) {
      expect(plan.contours[0].length).toBeGreaterThanOrEqual(8)
      expect(plan.contours.length).toBeGreaterThanOrEqual(3)
      expect(plan.facadeBands).toBeGreaterThanOrEqual(3)
      expect(plan.spirePaths.length).toBeGreaterThanOrEqual(1)
    }
  })
})
