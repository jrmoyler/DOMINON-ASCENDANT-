import questManifest from '@content/DA/Manifests/FirstHourQuests.json'
import { definition } from './content'
import type { GameState, QuestState } from './types'

/**
 * First-hour quest chain.
 *
 * Titles, ids and ordering come from Content/DA/Manifests/FirstHourQuests.json
 * (campaign.vertical_slice.first_hour). The Unreal slice evaluates authored
 * node graphs against live world state; the browser slice evaluates the same
 * quest spine against the city simulation using the predicates below.
 */

interface QuestSpec {
  id: string
  objectives: {
    id: string
    label: string
    target: number
    /** Current progress, given the live game state. */
    measure: (state: GameState) => number
  }[]
}

const operationalOf = (state: GameState, predicate: (typeId: string) => boolean) =>
  state.assets.filter((a) => a.operational && predicate(a.definitionId)).length

const byType = (type: string) => (id: string) => definition(id).cardType === type

const QUEST_SPECS: QuestSpec[] = [
  {
    id: 'quest.wake_the_hall',
    objectives: [
      {
        id: 'power_hall',
        label: 'Bring an Infrastructure asset online (relay, transit or utility)',
        target: 1,
        measure: (s) => operationalOf(s, byType('Infrastructure')),
      },
    ],
  },
  {
    id: 'quest.a_place_to_stay',
    objectives: [
      {
        id: 'housing',
        label: 'Raise housing capacity to 40',
        target: 40,
        measure: (s) => s.totals.housingCapacity,
      },
      {
        id: 'residential',
        label: 'Complete 3 residential assets',
        target: 3,
        measure: (s) => operationalOf(s, byType('Residential')),
      },
    ],
  },
  {
    id: 'quest.power_water_people',
    objectives: [
      {
        id: 'no_deficit',
        label: 'Cover Power and Water demand in full',
        target: 1,
        measure: (s) =>
          s.totals.powerSupply >= s.totals.powerDemand &&
          s.totals.waterSupply >= s.totals.waterDemand
            ? 1
            : 0,
      },
      {
        id: 'population',
        label: 'Grow the population to 30',
        target: 30,
        measure: (s) => s.population,
      },
    ],
  },
  {
    id: 'quest.nia_needs_a_job',
    objectives: [
      {
        id: 'jobs',
        label: 'Offer 24 jobs across the city',
        target: 24,
        measure: (s) => s.totals.jobCapacity,
      },
      {
        id: 'employed',
        label: 'Employ 20 citizens',
        target: 20,
        measure: (s) => s.totals.employed,
      },
    ],
  },
  {
    id: 'quest.replacement_model',
    objectives: [
      {
        id: 'industry',
        label: 'Complete 2 industrial assets',
        target: 2,
        measure: (s) => operationalOf(s, byType('Industrial')),
      },
      {
        id: 'capital',
        label: 'Hold a 60 Capital reserve',
        target: 60,
        measure: (s) => Math.floor(s.resources.capital),
      },
    ],
  },
  {
    id: 'quest.agency_has_a_price',
    objectives: [
      {
        id: 'happiness',
        label: 'Hold citizen approval at 60 or better',
        target: 60,
        measure: (s) => s.totals.happiness,
      },
      {
        id: 'civic',
        label: 'Complete 2 civic assets',
        target: 2,
        measure: (s) => operationalOf(s, byType('Civic')),
      },
    ],
  },
  {
    id: 'quest.signal_in_foundation',
    objectives: [
      {
        id: 'insight',
        label: 'Accumulate 30 Insight',
        target: 30,
        measure: (s) => Math.floor(s.resources.insight),
      },
      {
        id: 'offices',
        label: 'Complete 3 office or research assets',
        target: 3,
        measure: (s) =>
          operationalOf(s, (id) => ['Office', 'Research'].includes(definition(id).cardType)),
      },
    ],
  },
  {
    id: 'quest.iron_at_border',
    objectives: [
      {
        id: 'influence',
        label: 'Accumulate 25 Influence',
        target: 25,
        measure: (s) => Math.floor(s.resources.influence),
      },
      {
        id: 'assets',
        label: 'Hold 14 placed assets',
        target: 14,
        measure: (s) => s.assets.length,
      },
    ],
  },
  {
    id: 'quest.basin_speaks',
    objectives: [
      {
        id: 'population_final',
        label: 'Grow the population to 60',
        target: 60,
        measure: (s) => s.population,
      },
      {
        id: 'stability',
        label: 'Reach 20 placed assets with approval at 65+',
        target: 20,
        measure: (s) => (s.totals.happiness >= 65 ? s.assets.length : 0),
      },
    ],
  },
]

const TITLES: Record<string, string> = Object.fromEntries(
  (questManifest.quests as { id: string; title: string }[]).map((q) => [q.id, q.title]),
)

export const QUEST_COUNT = QUEST_SPECS.length

export function initialQuests(): QuestState[] {
  return QUEST_SPECS.map((spec, index) => ({
    id: spec.id,
    title: TITLES[spec.id] ?? spec.id,
    status: index === 0 ? 'active' : 'locked',
    objectives: spec.objectives.map((o) => ({
      id: o.id,
      label: o.label,
      done: false,
      progress: 0,
      target: o.target,
    })),
  }))
}

/**
 * Re-evaluate quest progress against live state.
 *
 * Objectives latch once met, so a quest never regresses because the player
 * spent a resource it had previously accumulated.
 *
 * Returns the ids of quests that completed during this evaluation.
 */
export function evaluateQuests(state: GameState): string[] {
  const completed: string[] = []

  for (let i = 0; i < state.quests.length; i++) {
    const quest = state.quests[i]
    if (quest.status === 'locked') continue
    if (quest.status === 'complete') continue

    const spec = QUEST_SPECS[i]
    let allDone = true
    for (let j = 0; j < quest.objectives.length; j++) {
      const objective = quest.objectives[j]
      const value = spec.objectives[j].measure(state)
      objective.progress = Math.max(objective.progress, Math.min(value, objective.target))
      if (objective.progress >= objective.target) objective.done = true
      if (!objective.done) allDone = false
    }

    if (allDone) {
      quest.status = 'complete'
      completed.push(quest.id)
      const next = state.quests[i + 1]
      if (next && next.status === 'locked') next.status = 'active'
    }
  }

  return completed
}

/** Ascension progress: the share of the first-hour chain the player has closed. */
export function ascensionProgress(state: GameState): number {
  const done = state.quests.filter((q) => q.status === 'complete').length
  return done / QUEST_SPECS.length
}
