import { afterEach, describe, expect, it, vi } from 'vitest'
import { ALL_DEFINITIONS, definition } from './content'
import { projectNextCycle } from './economy'
import { assetAt, canPlace } from './grid'
import {
  SAVE_BACKUP_KEY, SAVE_KEY, advanceCycle, clearStorage, createInitialState, deserialize,
  loadFromStorage, placeCard, previewPlacement, saveToStorage, serialize,
} from './state'
import type { GameState } from './types'

function residentialInHand(state: GameState): string {
  const id = Object.values(state.instances).find((instance) =>
    definition(instance.definitionId).cardType === 'Residential')!.id
  if (!state.hand.includes(id)) {
    const index = state.draw.indexOf(id)
    state.draw[index] = state.hand[0]
    state.hand[0] = id
  }
  return id
}

afterEach(() => vi.unstubAllGlobals())

describe('placement feedback and simulation', () => {
  it('opens with housing, income and infrastructure while preserving the exact deck', () => {
    const state = createInitialState()
    const types = state.hand.map((id) => definition(state.instances[id].definitionId).cardType)
    expect(types).toEqual(expect.arrayContaining(['Residential', 'Retail', 'Infrastructure']))
    expect(state.hand).toHaveLength(6)
    expect(new Set([...state.hand, ...state.draw]).size).toBe(60)
    expect(deserialize(serialize(state))).not.toBeNull()
  })

  it('rejects fractional cells, invalid rotations, and row-wrap selection', () => {
    const state = createInitialState()
    expect(canPlace(state.assets, 0.5, 0, [1, 1], 0).ok).toBe(false)
    expect(canPlace(state.assets, 0, 0, [1, 1], 4 as 0).ok).toBe(false)
    expect(assetAt([{ ...state.assets[0], x: 0, y: 1 }], 32, 0)).toBeUndefined()
  })

  it('uses the same affordability decision for preview and placement without mutation', () => {
    const state = createInitialState()
    const id = residentialInHand(state)
    state.resources.capital = 0
    const before = serialize(state)
    const preview = previewPlacement(state, id, 2, 2, 0)
    expect(preview.ok).toBe(false)
    expect(placeCard(state, id, 2, 2, 0)).toMatchObject({ ok: false, reason: preview.reason })
    expect(serialize(state)).toBe(before)
  })

  it('warns about unserved sites before a player pays for them', () => {
    const state = createInitialState()
    state.resources.capital = 1000
    const id = residentialInHand(state)
    const before = serialize(state)
    const preview = previewPlacement(state, id, 0, 0, 0)
    expect(preview.ok).toBe(true)
    expect(preview.warnings.length).toBeGreaterThan(0)
    expect(preview.services?.water).toBe(false)
    expect(serialize(state)).toBe(before)
  })

  it('checks Insight and Influence independently before confirming a build', () => {
    const state = createInitialState()
    const def = ALL_DEFINITIONS.find((card) => card.deploymentInsight > 0 && card.deploymentInfluence > 0)!
    const id = state.hand[0]
    state.instances[id].definitionId = def.id
    state.resources = { capital: def.deploymentCapital, insight: 0, influence: 0 }
    expect(previewPlacement(state, id, 0, 0, 0).reason).toContain('Insight')
    expect(placeCard(state, id, 0, 0, 0).reason).toContain('Insight')
    state.resources.insight = def.deploymentInsight
    expect(previewPlacement(state, id, 0, 0, 0).reason).toContain('Influence')
    expect(placeCard(state, id, 0, 0, 0).reason).toContain('Influence')
    state.resources.influence = def.deploymentInfluence
    expect(placeCard(state, id, 0, 0, 0).ok).toBe(true)
    expect(state.resources).toEqual({ capital: 0, insight: 0, influence: 0 })
  })

  it('projects construction completion and exact next-cycle income without advancing the city', () => {
    const state = createInitialState()
    const id = residentialInHand(state)
    state.resources.capital = 1000
    expect(placeCard(state, id, 12, 15, 0).ok).toBe(true)
    state.assets[1].cyclesRemaining = 1
    const before = serialize(state)
    const projected = projectNextCycle(state)
    expect(serialize(state)).toBe(before)
    const previous = { ...state.resources }
    advanceCycle(state)
    expect(state.assets[1].operational).toBe(true)
    expect(state.resources.capital).toBeCloseTo(previous.capital + projected.capitalDelta)
    expect(state.resources.insight).toBeCloseTo(previous.insight + projected.insightDelta)
  })
})

describe('campaign persistence integrity', () => {
  it('round-trips a deployed city with exact card ownership and canonical derived data', () => {
    const state = createInitialState()
    state.resources.capital = 1000
    expect(placeCard(state, residentialInHand(state), 12, 15, 0).ok).toBe(true)
    advanceCycle(state)
    state.selectedInstanceId = state.hand[0]
    state.totals.happiness = -999
    const loaded = deserialize(serialize(state))!
    expect(loaded).not.toBeNull()
    expect(loaded.assets).toEqual(state.assets)
    expect(loaded.instances).toEqual(state.instances)
    expect(loaded.speed).toBe(0)
    expect(loaded.selectedInstanceId).toBeNull()
    expect(loaded.totals.happiness).toBeGreaterThanOrEqual(0)
  })

  it.each([
    ['missing hand', (s: any) => { delete s.hand }],
    ['negative treasury', (s: any) => { s.resources.capital = -1 }],
    ['non-finite treasury', (s: any) => { s.resources.capital = 'Infinity' }],
    ['missing instance', (s: any) => { delete s.instances[s.hand[0]] }],
    ['duplicate ownership', (s: any) => { s.draw.push(s.hand[0]) }],
    ['orphan card', (s: any) => { s.draw.pop() }],
    ['unknown asset', (s: any) => { s.assets[0].definitionId = 'unknown' }],
    ['invalid geometry', (s: any) => { s.assets[0].x = -2 }],
    ['invalid footprint', (s: any) => { s.assets[0].footprint = [99, 99] }],
    ['invalid clock', (s: any) => { s.cycleProgress = 20 }],
    ['bad quest shape', (s: any) => { s.quests[0].objectives = [] }],
    ['broken quest sequence', (s: any) => { s.quests[1].status = 'complete' }],
  ])('rejects corrupt saves: %s', (_, corrupt) => {
    const state = createInitialState()
    corrupt(state)
    expect(deserialize(JSON.stringify(state))).toBeNull()
  })

  it('rejects a deployed card whose link no longer points to its asset', () => {
    const state = createInitialState()
    const id = residentialInHand(state)
    state.resources.capital = 1000
    placeCard(state, id, 12, 15, 0)
    state.instances[id].worldAssetId = 'missing'
    expect(deserialize(serialize(state))).toBeNull()
  })

  it('restores authored quest text while retaining earned progress', () => {
    const state = createInitialState()
    const original = state.quests[0].title
    state.quests[0].title = 'Stale old title'
    state.quests[0].objectives[0].label = 'Stale label'
    const loaded = deserialize(serialize(state))!
    expect(loaded.quests[0].title).toBe(original)
    expect(loaded.quests[0].objectives[0].label).not.toBe('Stale label')
  })

  it('recovers the previous valid checkpoint if the current save is damaged', () => {
    const items = new Map<string, string>()
    vi.stubGlobal('localStorage', {
      getItem: (key: string) => items.get(key) ?? null,
      setItem: (key: string, value: string) => { items.set(key, value) },
      removeItem: (key: string) => { items.delete(key) },
    })
    const state = createInitialState()
    expect(saveToStorage(state)).toBe(true)
    advanceCycle(state)
    expect(saveToStorage(state)).toBe(true)
    items.set(SAVE_KEY, '{broken')
    expect(loadFromStorage()?.cycle).toBe(0)
    // Saving after recovery must retain the good backup, not copy the damage.
    expect(saveToStorage(state)).toBe(true)
    expect(deserialize(items.get(SAVE_BACKUP_KEY)!)?.cycle).toBe(0)
    clearStorage()
    expect(loadFromStorage()).toBeNull()
  })

  it('reports storage failure and leaves the existing checkpoint readable', () => {
    const original = serialize(createInitialState())
    const items = new Map([[SAVE_KEY, original]])
    vi.stubGlobal('localStorage', {
      getItem: (key: string) => items.get(key) ?? null,
      setItem: () => { throw new Error('Quota exceeded') },
    })
    const state = createInitialState()
    advanceCycle(state)
    expect(saveToStorage(state)).toBe(false)
    expect(items.get(SAVE_KEY)).toBe(original)
    expect(loadFromStorage()?.cycle).toBe(0)
  })

  it('preserves 60 unique starter cards through placement and repeated saves', () => {
    const state = createInitialState()
    expect(Object.keys(state.instances)).toHaveLength(60)
    for (const instance of Object.values(state.instances)) expect(definition(instance.definitionId)).toBeTruthy()
    let restored = state
    for (let i = 0; i < 100; i++) {
      advanceCycle(restored)
      restored = deserialize(serialize(restored))!
      expect(restored).not.toBeNull()
      expect(new Set([...restored.hand, ...restored.draw, ...restored.discard]).size).toBe(60)
    }
  })
})
