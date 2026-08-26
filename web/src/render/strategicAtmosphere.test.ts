import { describe, expect, it } from 'vitest'
import { createStrategicAtmosphere } from './strategicAtmosphere'

describe('strategic atmosphere field', () => {
  it('creates deterministic authored ribbons that span the city horizon', () => {
    const field = createStrategicAtmosphere('synara-dawn')

    expect(field).toEqual(createStrategicAtmosphere('synara-dawn'))
    expect(field).toHaveLength(5)
    expect(field.every((band) => band.points.length === 48)).toBe(true)
    expect(Math.max(...field.flatMap((band) => band.points.map((point) => point.x)))).toBeGreaterThan(120)
    expect(Math.min(...field.flatMap((band) => band.points.map((point) => point.x)))).toBeLessThan(-120)
  })
})
