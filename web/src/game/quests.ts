import questManifest from '@content/DA/Manifests/FirstHourQuests.json'
import { definition } from './content'
import type { ConquestPath, GameState, QuestState } from './types'

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
  title?: string
  act: 1 | 2 | 3
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

const utilityPlants = (s: GameState) => operationalOf(s, (id) => {
  const d = definition(id)
  return id !== 'special.founder_hall' && (d.powerProduced > 0 || d.waterProduced > 0 || d.dataProduced > 0)
})

const PATH_OBJECTIVES: Record<ConquestPath, QuestSpec['objectives']> = {
  force: [
    { id: 'path_a', label: 'Operate 4 Defense assets', target: 4, measure: (s) => operationalOf(s, byType('Defense')) },
    { id: 'path_b', label: 'Repel 4 Ironheart raids', target: 4, measure: (s) => s.stats.raidsRepelled },
  ],
  economic: [
    { id: 'path_a', label: 'Operate 5 industrial assets', target: 5, measure: (s) => operationalOf(s, byType('Industrial')) },
    { id: 'path_b', label: 'Hold a 450 Capital reserve', target: 450, measure: (s) => Math.floor(s.resources.capital) },
  ],
  influence: [
    { id: 'path_a', label: 'Accumulate 100 Influence', target: 100, measure: (s) => Math.floor(s.resources.influence) },
    { id: 'path_b', label: 'Hold citizen approval at 75', target: 75, measure: (s) => s.totals.happiness },
  ],
  alliance: [
    { id: 'path_a', label: 'Complete a Wonder', target: 1, measure: (s) => operationalOf(s, byType('Wonder')) },
    { id: 'path_b', label: 'Accumulate 180 Insight', target: 180, measure: (s) => Math.floor(s.resources.insight) },
  ],
}

const PATH_PENDING: QuestSpec['objectives'] = [
  { id: 'path_a', label: 'Answer the Forge Lord', target: 1, measure: () => 0 },
  { id: 'path_b', label: 'Your chosen path decides the terms', target: 1, measure: () => 0 },
]

export const BROKER_QUEST_ID = 'quest.broker_of_ironheart'
export const OVERDRIVE_QUEST_ID = 'quest.act3.overdrive'
export const ACT1_FINAL_QUEST_ID = 'quest.basin_speaks'
export const ACT2_FINAL_QUEST_ID = 'quest.act2.foundry_shortage'

const QUEST_SPECS: QuestSpec[] = [
  {
    id: 'quest.wake_the_hall',
    act: 1,
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
    act: 1,
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
    act: 1,
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
    act: 1,
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
    act: 1,
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
    act: 1,
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
    act: 1,
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
    act: 1,
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
    act: 1,
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
  {
    id: 'quest.act2.grid_strain',
    title: 'Grid Strain',
    act: 2,
    objectives: [
      { id: 'plants', label: 'Operate 4 utility plants (power, water or data)', target: 4, measure: utilityPlants },
      { id: 'population', label: 'Grow the population to 100', target: 100, measure: (s) => s.population },
    ],
  },
  {
    id: 'quest.act2.housing_surge',
    title: 'Housing Surge',
    act: 2,
    objectives: [
      { id: 'housing', label: 'Raise housing capacity to 150', target: 150, measure: (s) => s.totals.housingCapacity },
      { id: 'employed', label: 'Employ 100 citizens', target: 100, measure: (s) => s.totals.employed },
    ],
  },
  {
    id: 'quest.act2.green_line',
    title: 'The Green Line',
    act: 2,
    objectives: [
      { id: 'approval', label: 'Hold citizen approval at 70', target: 70, measure: (s) => s.totals.happiness },
      { id: 'civic', label: 'Operate 5 civic assets', target: 5, measure: (s) => operationalOf(s, byType('Civic')) },
    ],
  },
  {
    id: ACT2_FINAL_QUEST_ID,
    title: 'Foundry Shortage',
    act: 2,
    objectives: [
      { id: 'capital', label: 'Hold a 250 Capital reserve', target: 250, measure: (s) => Math.floor(s.resources.capital) },
      { id: 'insight', label: 'Accumulate 90 Insight', target: 90, measure: (s) => Math.floor(s.resources.insight) },
    ],
  },
  {
    id: BROKER_QUEST_ID,
    title: 'Broker of Ironheart',
    act: 3,
    objectives: PATH_PENDING,
  },
  {
    id: OVERDRIVE_QUEST_ID,
    title: 'Overdrive',
    act: 3,
    objectives: [
      { id: 'endure', label: 'Endure 15 cycles of Ironheart Overdrive', target: 15, measure: (s) => s.overdriveCycles },
      { id: 'population', label: 'Keep the city growing to 150 citizens', target: 150, measure: (s) => s.population },
    ],
  },
]

const TITLES: Record<string, string> = Object.fromEntries(
  (questManifest.quests as { id: string; title: string }[]).map((q) => [q.id, q.title]),
)

export const QUEST_COUNT = QUEST_SPECS.length
export const ACT1_QUEST_COUNT = QUEST_SPECS.filter((q) => q.act === 1).length

function objectivesFor(spec: QuestSpec, state?: Pick<GameState, 'path'>): QuestSpec['objectives'] {
  if (spec.id !== BROKER_QUEST_ID) return spec.objectives
  return state?.path ? PATH_OBJECTIVES[state.path] : PATH_PENDING
}

export function questAct(id: string): 1 | 2 | 3 {
  return QUEST_SPECS.find((q) => q.id === id)?.act ?? 1
}

export function initialQuests(state?: Pick<GameState, 'path'>): QuestState[] {
  return QUEST_SPECS.map((spec, index) => ({
    id: spec.id,
    title: spec.title ?? TITLES[spec.id] ?? spec.id,
    status: index === 0 ? 'active' : 'locked',
    objectives: objectivesFor(spec, state).map((o) => ({
      id: o.id,
      label: o.label,
      done: false,
      progress: 0,
      target: o.target,
    })),
  }))
}

/** Rewrite path-dependent objective wording after the player answers the Forge Lord. */
export function relabelQuests(state: GameState) {
  const canonical = initialQuests(state)
  state.quests.forEach((quest, index) => {
    quest.title = canonical[index].title
    quest.objectives.forEach((objective, j) => {
      objective.label = canonical[index].objectives[j].label
      objective.target = canonical[index].objectives[j].target
      objective.progress = Math.min(objective.progress, objective.target)
      objective.done = objective.progress >= objective.target
    })
  })
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
    if (quest.status !== 'active') continue
    if (quest.id === BROKER_QUEST_ID && !state.path) continue

    const objectives = objectivesFor(QUEST_SPECS[i], state)
    let allDone = true
    for (let j = 0; j < quest.objectives.length; j++) {
      const objective = quest.objectives[j]
      const value = objectives[j].measure(state)
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
  const done = state.quests.filter((q, i) => q.status === 'complete' && QUEST_SPECS[i].act === 1).length
  return done / ACT1_QUEST_COUNT
}

/** Whole-campaign progress, 0..1. */
export function campaignProgress(state: GameState): number {
  return state.quests.filter((q) => q.status === 'complete').length / QUEST_COUNT
}
