import { describe, expect, it } from 'vitest'
import { audio, type Sfx } from './audio'

const ALL: Sfx[] = ['click', 'select', 'place', 'complete', 'cycle', 'quest', 'event', 'error', 'raid', 'draft', 'tactic', 'victory', 'defeat', 'coin']

describe('audio (no AudioContext)', () => {
  it('has sensible default volumes', () => {
    expect(audio.getVolumes()).toEqual({ master: 0.8, music: 0.55, sfx: 0.8, muted: false })
  })

  it('never throws without a browser', () => {
    expect(() => {
      audio.unlock()
      audio.unlock()
      for (const s of ALL) audio.play(s)
      audio.setMood({ act: 3, tension: 0.9, paused: true, night: 1 })
      audio.setMood({ act: 1, tension: 0, paused: false, night: 0 })
    }).not.toThrow()
  })

  it('clamps and merges partial volume updates', () => {
    audio.setVolumes({ music: 2, muted: true })
    expect(audio.getVolumes()).toEqual({ master: 0.8, music: 1, sfx: 0.8, muted: true })
    audio.setVolumes({ sfx: -1, master: Number.NaN })
    expect(audio.getVolumes()).toEqual({ master: 0.8, music: 1, sfx: 0, muted: true })
    const v = audio.getVolumes()
    v.master = 0
    expect(audio.getVolumes().master).toBe(0.8)
  })
})
