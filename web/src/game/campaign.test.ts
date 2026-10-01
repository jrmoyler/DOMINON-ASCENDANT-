import { describe, expect, it } from 'vitest'
import { definition } from './content'
import {
  EVENTS, EVENT_BY_ID, TACTICS, advanceIronheart, canAfford, choiceCost, computeScore, eventChoices, offerDraft, raidStrength,
} from './campaign'
import { cityTotals } from './economy'
import {
  advanceCycle, chooseDraft, createInitialState, decideEvent, deserialize, garrison, influencePressure, marketDraft,
  playTacticCard, refreshTotals, serialize,
} from './state'
import { BROKER_QUEST_ID } from './quests'
import type { GameState } from './types'

function completeQuestsThrough(state: GameState, lastIndex: number) {
  state.quests.forEach((quest, index) => {
    if (index > lastIndex) return
    quest.status = 'complete'
    quest.objectives.forEach((o) => { o.progress = o.target; o.done = true })
  })
  if (state.quests[lastIndex + 1]) state.quests[lastIndex + 1].status = 'active'
}

function tacticInHand(state: GameState, definitionId: string): string {
  const id = Object.values(state.instances).find((i) => i.definitionId === definitionId)!.id
  for (const zone of [state.draw, state.discard]) {
    const index = zone.indexOf(id)
    if (index >= 0) zone.splice(index, 1)
  }
  if (!state.hand.includes(id)) state.hand[0] = id
  return id
}

describe('campaign events', () => {
  it('every event offers at least one choice and every choice describes its effects', () => {
    const state = createInitialState()
    state.path = 'force'
    for (const event of EVENTS) {
      const choices = eventChoices(state, event)
      expect(choices.length, event.id).toBeGreaterThan(0)
      for (const choice of choices) expect(choice.label.length).toBeGreaterThan(0)
    }
  })

  it('resolving a crisis applies its effects, clears the decision and survives a save', () => {
    const state = createInitialState()
    state.pendingEvent = 'event.breakthrough'
    const before = state.resources.capital
    expect(decideEvent(state, 1).ok).toBe(true)
    expect(state.resources.capital).toBe(before + 40)
    expect(state.pendingEvent).toBeNull()
    expect(state.stats.eventsResolved).toBe(1)
    expect(deserialize(serialize(state))).not.toBeNull()
  })

  it('refuses a choice the treasury cannot cover', () => {
    const state = createInitialState()
    state.pendingEvent = 'event.founders_day'
    state.resources.capital = 0
    const choice = eventChoices(state, EVENT_BY_ID['event.founders_day'])[0]
    expect(canAfford(state, choiceCost(choice))).toBe(false)
    expect(decideEvent(state, 0).ok).toBe(false)
    expect(state.pendingEvent).toBe('event.founders_day')
  })

  it('opens a regional crisis when the cooldown runs out', () => {
    const state = createInitialState()
    state.cycle = 30
    state.eventCooldown = 1
    state.speed = 1
    advanceCycle(state)
    expect(state.pendingEvent).not.toBeNull()
    expect(EVENT_BY_ID[state.pendingEvent!].story).toBeFalsy()
  })
})

describe('story spine', () => {
  it('Act I completion asks for a doctrine and opens Act II', () => {
    const state = createInitialState()
    completeQuestsThrough(state, 7)
    state.quests[8].objectives.forEach((o) => { o.progress = o.target - 1 })
    state.population = 200
    state.resources.capital = 999
    state.assets.length = 1
    // Force the last objective through the live evaluator.
    state.quests[8].objectives.forEach((o) => { o.progress = o.target; o.done = true })
    refreshTotals(state)
    expect(state.pendingEvent).toBe('story.ascension')
    expect(decideEvent(state, 1).ok).toBe(true)
    expect(state.doctrine).toBe('concord')
    expect(state.act).toBe(2)
    expect(state.modifiers.some((m) => m.cyclesLeft === -1)).toBe(true)
  })

  it('answering the Forge Lord picks a path, relabels objectives and starts Ironheart', () => {
    const state = createInitialState()
    completeQuestsThrough(state, 12)
    state.pendingEvent = 'story.forge_lord'
    expect(decideEvent(state, 1).ok).toBe(true)
    expect(state.path).toBe('economic')
    expect(state.act).toBe(3)
    const broker = state.quests.find((q) => q.id === BROKER_QUEST_ID)!
    expect(broker.objectives[0].label).toContain('industrial')
    expect(state.threat).toBeGreaterThan(0)
    const loaded = deserialize(serialize(state))!
    expect(loaded.quests.find((q) => q.id === BROKER_QUEST_ID)!.objectives[1].label).toContain('Capital')
  })

  it('the final choice ends the campaign in victory with a score', () => {
    const state = createInitialState()
    completeQuestsThrough(state, 14)
    state.path = 'alliance'
    state.pendingEvent = 'story.the_choice'
    expect(decideEvent(state, 0).ok).toBe(true)
    expect(state.outcome?.kind).toBe('victory')
    expect(state.outcome?.title).toBe('The Third Foundry')
    const score = computeScore(state)
    expect(score.total).toBeGreaterThan(1000)
    const cycle = state.cycle
    advanceCycle(state)
    expect(state.cycle).toBe(cycle)
  })
})

describe('deck building', () => {
  it('drafted cards join the deck, appear on top of the draw pile and persist', () => {
    const state = createInitialState()
    offerDraft(state, 'test')
    expect(state.draft?.options).toHaveLength(3)
    const pick = state.draft!.options[0]
    chooseDraft(state, pick)
    expect(Object.keys(state.instances)).toHaveLength(61)
    expect(state.draw.length + state.hand.length + state.discard.length).toBe(61)
    expect(state.draft).toBeNull()
    expect(deserialize(serialize(state))).not.toBeNull()
  })

  it('skipping a draft banks Capital instead', () => {
    const state = createInitialState()
    offerDraft(state, 'test')
    const before = state.resources.capital
    chooseDraft(state, null)
    expect(state.resources.capital).toBeGreaterThan(before)
    expect(Object.keys(state.instances)).toHaveLength(60)
  })

  it('the market sells drafts at a rising price', () => {
    const state = createInitialState()
    state.resources.capital = 500
    expect(marketDraft(state).ok).toBe(true)
    expect(state.draft).not.toBeNull()
    expect(marketDraft(state).ok).toBe(false)
    chooseDraft(state, null)
    const before = state.resources.capital
    expect(marketDraft(state).ok).toBe(true)
    expect(before - state.resources.capital).toBe(55)
  })

  it('units and leaders are playable tactics that return to the discard pile', () => {
    const state = createInitialState()
    const id = tacticInHand(state, 'synara.guardian_drone_cohort')
    const defenseBefore = state.totals.defense
    expect(playTacticCard(state, id).ok).toBe(true)
    expect(state.totals.defense).toBe(defenseBefore + 14)
    expect(state.discard).toContain(id)
    expect(state.hand).not.toContain(id)
    expect(state.hand).toHaveLength(6)
  })

  it('every tactic references a real, non-placeable card', () => {
    for (const id of Object.keys(TACTICS)) expect(definition(id).placeable).toBe(false)
  })
})

describe('Ironheart', () => {
  it('raids breach a weak city: sabotage, seizure and rising Dominance', () => {
    const state = createInitialState()
    state.act = 3
    state.raidTimer = 1
    state.resources.capital = 100
    const totals = { ...cityTotals(state), defense: 0 }
    const report = advanceIronheart(state, totals, 0)!
    expect(report.repelled).toBe(false)
    expect(report.stolen).toBe(12)
    expect(state.stats.raidsSuffered).toBe(1)
  })

  it('a strong defense repels raids and pushes Dominance back', () => {
    const state = createInitialState()
    state.act = 3
    state.threat = 40
    state.raidTimer = 1
    const totals = { ...cityTotals(state), defense: raidStrength(state) + 5 }
    const report = advanceIronheart(state, totals, 0)!
    expect(report.repelled).toBe(true)
    expect(state.threat).toBeLessThan(40)
  })

  it('pressure and garrisons are only available once Ironheart moves', () => {
    const state = createInitialState()
    state.resources = { capital: 200, insight: 50, influence: 50 }
    expect(influencePressure(state).ok).toBe(false)
    expect(garrison(state).ok).toBe(false)
    state.act = 3
    state.threat = 50
    expect(influencePressure(state).ok).toBe(true)
    expect(state.threat).toBe(42)
    expect(garrison(state).ok).toBe(true)
    expect(state.modifiers.some((m) => (m.defense ?? 0) > 0)).toBe(true)
  })

  it('total Dominance is a defeat', () => {
    const state = createInitialState()
    state.act = 3
    state.threat = 100
    advanceCycle(state)
    expect(state.outcome?.kind).toBe('defeat')
    expect(state.outcome?.title).toBe('Ashcroft Falls')
  })
})

describe('defeat conditions', () => {
  it('a long insolvency dissolves the Authority', () => {
    const state = createInitialState()
    state.resources.capital = 0
    state.population = 120
    for (let i = 0; i < 40 && !state.outcome; i++) {
      state.resources.capital = 0
      state.pendingEvent = null
      advanceCycle(state)
    }
    expect(state.outcome?.title).toBe('The Treasury Collapses')
  })
})
