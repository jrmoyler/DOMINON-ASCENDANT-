import { describe, expect, it, vi } from 'vitest'
import { createSceneSafely } from './createSceneSafely'

describe('renderer startup boundary', () => {
  it('keeps the application alive and reports an actionable message when WebGL is unavailable', () => {
    const report = vi.fn()

    const scene = createSceneSafely(
      () => { throw new Error('WebGL not supported') },
      report,
    )

    expect(scene).toBeNull()
    expect(report).toHaveBeenCalledWith(
      '3D acceleration is unavailable. Enable WebGL and hardware acceleration, then reload.',
    )
  })

  it('returns the scene without reporting when renderer startup succeeds', () => {
    const report = vi.fn()
    const expected = { engine: 'babylon' }

    expect(createSceneSafely(() => expected, report)).toBe(expected)
    expect(report).not.toHaveBeenCalled()
  })
})
