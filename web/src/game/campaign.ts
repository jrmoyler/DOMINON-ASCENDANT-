import { ALL_DEFINITIONS, FOUNDER_HALL_ID, definition } from './content'
import { DIFFICULTY } from './economy'
import { relabelQuests } from './quests'
import type { CardDefinition, CityTotals, ConquestPath, Doctrine, GameState, Modifier, Resources } from './types'
import { log, nextId, randInt, refillHand } from './util'

/**
 * Campaign layer: crises and story beats, tactic cards, card drafts, the
 * Ironheart rival, and the conditions that end a campaign. Everything here is
 * web-slice design built on the cast and event names authored in
 * Content/DA/Manifests (RegionalCrisisCampaign, ForgeweaveConquest,
 * DaxtonEncounter, FirstAscension).
 */

// ---- effects ---------------------------------------------------------------

export interface Effect {
  text: string
  tone: 'good' | 'bad' | 'neutral'
  /** Resources this effect spends, so a choice can be disabled when unaffordable. */
  cost?: Partial<Resources>
  apply: (state: GameState) => void
}

const RES_LABEL: Record<keyof Resources, string> = { capital: 'Capital', insight: 'Insight', influence: 'Influence' }

export function gain(resource: keyof Resources, amount: number): Effect {
  return {
    text: `${amount >= 0 ? '+' : '−'}${Math.abs(amount)} ${RES_LABEL[resource]}`,
    tone: amount >= 0 ? 'good' : 'bad',
    cost: amount < 0 ? { [resource]: -amount } : undefined,
    apply: (s) => { s.resources[resource] = Math.max(0, s.resources[resource] + amount) },
  }
}

const FIELD_TEXT: Record<string, [string, boolean, boolean]> = {
  // field: [label, isPercent, higherIsGood]
  capital: ['Capital income', true, true],
  insight: ['Insight income', true, true],
  influence: ['Influence income', true, true],
  approval: ['approval', false, true],
  upkeep: ['citizen upkeep', true, false],
  power: ['Power supply', true, true],
  water: ['Water supply', true, true],
  defense: ['Defense', false, true],
  threat: ['Dominance / cycle', false, false],
}

type ModFields = Omit<Modifier, 'id' | 'label' | 'cyclesLeft'>

export function describeFields(fields: ModFields): { text: string; good: boolean }[] {
  return Object.entries(fields).filter(([key, value]) => FIELD_TEXT[key] && typeof value === 'number' && value !== 0).map(([key, value]) => {
    const [label, percent, higherGood] = FIELD_TEXT[key]
    const v = value as number
    const amount = percent ? `${Math.round(Math.abs(v) * 100)}%` : `${Math.abs(v)}`
    return { text: `${v >= 0 ? '+' : '−'}${amount} ${label}`, good: (v >= 0) === higherGood }
  })
}

export function addModifier(state: GameState, label: string, cycles: number, fields: ModFields) {
  state.modifiers.push({ id: nextId('mod'), label, cyclesLeft: cycles, ...fields })
}

export function mod(label: string, cycles: number, fields: ModFields): Effect {
  const parts = describeFields(fields)
  const span = cycles < 0 ? 'permanently' : `for ${cycles} cycles`
  return {
    text: `${parts.map((p) => p.text).join(', ')} ${span}`,
    tone: parts.every((p) => p.good) ? 'good' : parts.every((p) => !p.good) ? 'bad' : 'neutral',
    apply: (s) => addModifier(s, label, cycles, fields),
  }
}

export function threat(amount: number): Effect {
  return {
    text: `${amount >= 0 ? '+' : '−'}${Math.abs(amount)} Ironheart Dominance`,
    tone: amount > 0 ? 'bad' : 'good',
    apply: (s) => { s.threat = Math.max(0, Math.min(100, s.threat + amount)) },
  }
}

export function citizens(amount: number): Effect {
  return {
    text: `${amount >= 0 ? '+' : '−'}${Math.abs(amount)} citizens`,
    tone: amount >= 0 ? 'good' : 'bad',
    apply: (s) => { s.population = Math.max(0, s.population + amount) },
  }
}

export function sabotageRandom(cycles: number, count = 1): Effect {
  return {
    text: `${count === 1 ? 'A building goes' : `${count} buildings go`} offline for ${cycles} cycles`,
    tone: 'bad',
    apply: (s) => sabotage(s, count, cycles),
  }
}

export function grantCard(definitionId: string): Effect {
  return {
    text: `Add ${definition(definitionId).displayName} to your deck`,
    tone: 'good',
    apply: (s) => addCardToDeck(s, definitionId),
  }
}

function offerDraftEffect(reason: string): Effect {
  return { text: 'Draft a new card for your deck', tone: 'good', apply: (s) => offerDraft(s, reason) }
}

function flag(name: string): Effect {
  return { text: '', tone: 'neutral', apply: (s) => { if (!s.flags.includes(name)) s.flags.push(name) } }
}

function custom(text: string, tone: Effect['tone'], apply: (s: GameState) => void): Effect {
  return { text, tone, apply }
}

// ---- events ----------------------------------------------------------------

export interface Choice {
  label: string
  detail?: string
  effects: Effect[]
}

export interface Speaker {
  name: string
  role: string
  faction: 'Synara' | 'Forgeweave' | 'EdenCircuit' | 'Universal'
}

export interface CampaignEvent {
  id: string
  title: string
  speaker: Speaker
  text: string | ((s: GameState) => string)
  story?: boolean
  minAct?: 1 | 2 | 3
  minCycle?: number
  condition?: (s: GameState) => boolean
  choices: Choice[] | ((s: GameState) => Choice[])
}

export const CAST = {
  mira: { name: 'Archon Mira Vey', role: 'Synara Authority', faction: 'Synara' },
  tal: { name: 'Tal Arden', role: 'Chief Engineer', faction: 'Universal' },
  mara: { name: 'Mara Kest', role: 'Forgeweave Foreman', faction: 'Forgeweave' },
  ori: { name: 'Ori Sen', role: 'Eden Watershed Warden', faction: 'EdenCircuit' },
  daxton: { name: 'Forge Lord Daxton Rhe', role: 'Ironheart Directorate', faction: 'Forgeweave' },
  amara: { name: 'Caretaker Amara Venn', role: 'Eden Circuit', faction: 'EdenCircuit' },
} satisfies Record<string, Speaker>

const countType = (s: GameState, type: string) =>
  s.assets.filter((a) => a.operational && definition(a.definitionId).cardType === type).length

export const EVENTS: CampaignEvent[] = [
  // ---- story -------------------------------------------------------------
  {
    id: 'story.ascension',
    title: 'Convergence Authority 1/20',
    speaker: CAST.mira,
    story: true,
    text: 'The basin answered. Ashcroft is no longer a frontier camp — it is a city, and the Convergence has recognised it. Every Ascension binds a city to a doctrine. Choose the principle Ashcroft will be built on. It cannot be unmade.',
    choices: [
      {
        label: 'Doctrine of Replication',
        detail: 'What works, multiplies. Industry and commerce above all.',
        effects: [mod('Doctrine of Replication', -1, { capital: 0.2, approval: -3 }), custom('', 'neutral', (s) => setDoctrine(s, 'replication'))],
      },
      {
        label: 'Doctrine of Concord',
        detail: 'A city is its people. Goodwill is the strongest wall.',
        effects: [mod('Doctrine of Concord', -1, { approval: 6, influence: 0.3 }), custom('', 'neutral', (s) => setDoctrine(s, 'concord'))],
      },
      {
        label: 'Doctrine of Verdance',
        detail: 'Grow with the basin, not against it. Knowledge and clean supply.',
        effects: [mod('Doctrine of Verdance', -1, { insight: 0.35, power: 0.15, water: 0.15 }), custom('', 'neutral', (s) => setDoctrine(s, 'verdance'))],
      },
    ],
  },
  {
    id: 'story.forge_lord',
    title: 'The Forge Lord',
    speaker: CAST.daxton,
    story: true,
    text: 'A column of Ironheart haulers stops at the edge of your district. Daxton Rhe does not dismount. “Ashcroft grew fast. Too fast to have done it alone. My foundries fed your supply lines, and Ironheart collects its debts. Kneel to the Directorate, or learn what a furnace does to a city.” Dominance will rise every cycle from now on. Raids will come. How will Ashcroft answer?',
    choices: [
      {
        label: 'Force — Operation Iron Veil',
        detail: 'Meet steel with steel. Fortify the district and break his raids.',
        effects: [mod('Iron Veil fortifications', -1, { defense: 10 }), grantCard('universal.barrier_hub'), grantCard('universal.watch_post'), custom('Win by operating 4 Defense assets and repelling 4 raids', 'neutral', (s) => setPath(s, 'force'))],
      },
      {
        label: 'Economy — The Supply Noose',
        detail: 'Outproduce Ironheart until his foundries need you more.',
        effects: [gain('capital', 60), mod('Supply Noose contracts', -1, { capital: 0.1 }), custom('Win by operating 5 industrial assets and holding 450 Capital', 'neutral', (s) => setPath(s, 'economic'))],
      },
      {
        label: 'Influence — The Workers’ Signal',
        detail: 'Ironheart’s workers are tired. Give them a reason to turn.',
        effects: [gain('influence', 20), mod('Workers’ Signal', -1, { threat: -0.4 }), custom('Win by holding 100 Influence and 75 approval', 'neutral', (s) => setPath(s, 'influence'))],
      },
      {
        label: 'Alliance — The Third Foundry',
        detail: 'Offer Daxton a partnership neither city could build alone.',
        effects: [mod('Third Foundry accord', -1, { threat: -0.5 }), grantCard('forgeweave.the_grand_forge'), custom('Win by completing a Wonder and holding 180 Insight', 'neutral', (s) => setPath(s, 'alliance'))],
      },
    ],
  },
  {
    id: 'story.overdrive',
    title: 'Overdrive',
    speaker: CAST.daxton,
    story: true,
    text: (s) => `Your answer reached Ironheart, and the Forge Lord did not like it. Every furnace in the Directorate is running past its limits. ${s.path === 'alliance' ? '“A partnership needs a partner who survives,” he says.' : '“Let us see how long Ashcroft stays standing.”'} Overdrive raids will come faster and hit harder. Hold for fifteen cycles.`,
    choices: [
      { label: 'Brace the city', detail: 'Hold the line with what you have.', effects: [mod('Civic resolve', 15, { approval: 4 })] },
      { label: 'Mobilise every crew', detail: 'Pay now so the walls hold later.', effects: [gain('capital', -40), mod('Mobilised crews', 15, { defense: 18 })] },
      { label: 'Call in the Synara reserve', detail: 'Spend Insight on predictive defense grids.', effects: [gain('insight', -30), mod('Predictive defense grid', 15, { defense: 14, threat: -0.3 })] },
    ],
  },
  {
    id: 'story.the_choice',
    title: 'The Choice',
    speaker: CAST.daxton,
    story: true,
    text: 'The Overdrive burns out. Ironheart’s furnaces fall quiet one by one, and the Forge Lord comes to Ashcroft on foot. “You held. I did not think you would.” For the first time, the Directorate’s fate is yours to decide.',
    choices: (s) => {
      const pathEnding: Record<ConquestPath, Choice> = {
        force: { label: 'Annex Ironheart', detail: 'The Directorate becomes an Ashcroft protectorate.', effects: [victory('Ironheart Annexed', 'Ashcroft’s walls became Ashcroft’s border. Ironheart’s foundries now burn under your banner, and the basin will remember who held when the furnaces came.')] },
        economic: { label: 'Buy the foundries', detail: 'Purchase Ironheart’s debt and its furnaces with it.', effects: [victory('The Supply Noose Closes', 'No army marched. Ashcroft simply became the market Ironheart could not live without — and then bought it. The Directorate’s ledgers now balance in your favour.')] },
        influence: { label: 'Free the workers', detail: 'Ironheart’s workers vote to join Ashcroft.', effects: [victory('The Workers’ Signal', 'The signal spread from forge to forge. When the Directorate called its workers to the furnaces, they came to Ashcroft instead. Daxton rules an empty foundry.')] },
        alliance: { label: 'Light the Third Foundry', detail: 'Two cities, one forge.', effects: [victory('The Third Foundry', 'Synara design and Forgeweave fire, joined. The Third Foundry lights the basin, and for the first time Ashcroft and Ironheart build the same future.')] },
      }
      return [
        pathEnding[s.path ?? 'force'],
        { label: 'Offer the Convergence Compact', detail: 'Mercy. Daxton keeps his seat under shared law.', effects: [victory('The Convergence Compact', 'You offered the Forge Lord what he refused to offer you: a place at the table. Ironheart joins the Convergence as an equal, and the basin’s long winter ends in a handshake.')] },
      ]
    },
  },

  // ---- regional crises ---------------------------------------------------
  {
    id: 'event.grid_strain',
    title: 'Grid Strain',
    speaker: CAST.tal,
    minCycle: 10,
    condition: (s) => s.totals.powerDemand > s.totals.powerSupply * 0.7,
    text: 'The relays are running hot. Demand on the grid is climbing faster than the plants can follow, and the Founder Hall’s cores were never meant to carry a whole district.',
    choices: [
      { label: 'Emergency generation', detail: 'Overclock the plants.', effects: [gain('capital', -18), mod('Emergency generation', 10, { power: 0.35 })] },
      { label: 'Rolling brownouts', detail: 'Ration power by district.', effects: [gain('capital', 10), mod('Rolling brownouts', 6, { approval: -7 })] },
      { label: 'Import Ironheart power', detail: 'Cheap, reliable — and a debt.', effects: [mod('Ironheart power line', 10, { power: 0.35 }), threat(6)] },
    ],
  },
  {
    id: 'event.housing_surge',
    title: 'Housing Surge',
    speaker: CAST.mira,
    minCycle: 12,
    condition: (s) => s.population >= s.totals.housingCapacity - 4,
    text: 'Word of Ashcroft has spread. Families are arriving faster than homes are being finished, and the gate wardens want to know whether to keep the gates open.',
    choices: [
      { label: 'Open the gates', detail: 'More hands, more crowding.', effects: [citizens(8), mod('Crowded districts', 8, { approval: -6, capital: 0.12 })] },
      { label: 'Subsidise housing', detail: 'Pay builders to work double shifts.', effects: [gain('capital', -22), mod('Housing subsidy', 8, { approval: 5 })] },
      { label: 'Restrict intake', detail: 'Order first, growth later.', effects: [gain('influence', 5), mod('Closed gates', 6, { approval: -3 })] },
    ],
  },
  {
    id: 'event.foundry_shortage',
    title: 'Foundry Shortage',
    speaker: CAST.mara,
    minCycle: 14,
    condition: (s) => countType(s, 'Industrial') >= 1,
    text: 'Component stocks are gone. My crews can push the foundries past their rating to cover it, but Ori Sen says the runoff will poison the watershed. Somebody has to decide, Governor.',
    choices: [
      { label: 'Industrial support', detail: 'Run the foundries hot.', effects: [mod('Foundry overdrive', 8, { capital: 0.25, approval: -6 })] },
      { label: 'Eden restriction', detail: 'Protect the watershed.', effects: [gain('influence', 4), mod('Watershed protection', 8, { approval: 6, capital: -0.12 })] },
      { label: 'Brokered compact', detail: 'Pay for both.', effects: [gain('influence', -8), mod('Audited transition', 8, { capital: 0.1, approval: 3 })] },
    ],
  },
  {
    id: 'event.green_line',
    title: 'The Green Line',
    speaker: CAST.ori,
    minAct: 2,
    text: 'The Eden Circuit wants a treaty line drawn around the watershed — no industry past it, ever. It would protect the basin for generations. It would also cost you every cheap site on the river.',
    choices: [
      { label: 'Honour the full boundary', effects: [gain('influence', 6), mod('Green Line treaty', 10, { approval: 7, capital: -0.1 })] },
      { label: 'Negotiate an industrial exception', effects: [gain('capital', 25), mod('Broken faith', 6, { approval: -8 })] },
      { label: 'Engineered mitigation', detail: 'Expensive, but nobody loses.', effects: [gain('capital', -25), gain('insight', -8), mod('Filtration works', 10, { approval: 4, water: 0.2 })] },
    ],
  },
  {
    id: 'event.corridor_failure',
    title: 'Corridor Failure',
    speaker: CAST.tal,
    minCycle: 16,
    text: 'A freight corridor collapsed under the last convoy. Deliveries are stacking up at the basin edge and the warehouses are emptying.',
    choices: [
      { label: 'Repair the corridor', effects: [gain('capital', -20)] },
      { label: 'Reroute contracts', effects: [mod('Rerouted freight', 6, { capital: -0.18 })] },
      { label: 'Airlift essentials', effects: [gain('influence', -10), gain('insight', 6)] },
    ],
  },
  {
    id: 'event.migration_wave',
    title: 'Migration Wave',
    speaker: CAST.amara,
    minCycle: 18,
    text: 'Refugees from the eastern ridge have reached your border — farmers, mostly, burned out by Ironheart’s expansion. They ask for shelter. They can work.',
    choices: [
      { label: 'Shelter them', detail: 'Ashcroft keeps its promises.', effects: [citizens(12), gain('capital', -14), gain('influence', 7)] },
      { label: 'Recruit them as labour', effects: [citizens(8), mod('Labour drive', 6, { capital: 0.12, approval: -4 })] },
      { label: 'Turn them away', effects: [mod('Closed border', 8, { approval: -5 })] },
    ],
  },
  {
    id: 'event.breakthrough',
    title: 'A Breakthrough',
    speaker: CAST.mira,
    minCycle: 14,
    condition: (s) => countType(s, 'Office') + countType(s, 'Research') >= 1,
    text: 'Your analysts have modelled the basin’s groundwater with a precision nobody thought possible. The work is worth a fortune — or a reputation.',
    choices: [
      { label: 'Publish openly', effects: [gain('influence', 12)] },
      { label: 'Patent and license', effects: [gain('capital', 40)] },
      { label: 'Classify it', effects: [gain('insight', 20), threat(-4)] },
    ],
  },
  {
    id: 'event.founders_day',
    title: 'Founder’s Day',
    speaker: CAST.mira,
    minCycle: 20,
    condition: (s) => s.totals.happiness >= 55,
    text: 'It has been a year since the Founder Hall woke. The districts want to celebrate. The treasury would prefer they didn’t.',
    choices: [
      { label: 'A grand festival', effects: [gain('capital', -28), mod('Founder’s Day', 10, { approval: 10 })] },
      { label: 'A modest ceremony', effects: [gain('capital', -8), mod('Founder’s Day', 6, { approval: 4 })] },
      { label: 'Work through it', effects: [gain('capital', 10), mod('Skipped holiday', 5, { approval: -4 })] },
    ],
  },
  {
    id: 'event.dust_storm',
    title: 'Dust Storm',
    speaker: CAST.tal,
    minCycle: 22,
    text: 'A wall of dust is rolling down from the ridge. We can shelter the crews and lose a few days, or keep them working and hope the sites hold.',
    choices: [
      { label: 'Shelter everyone', effects: [mod('Storm shelter', 4, { capital: -0.3 })] },
      { label: 'Keep working', effects: [gain('capital', 14), sabotageRandom(2)] },
      { label: 'Deploy storm crews', effects: [gain('capital', -16)] },
    ],
  },
  {
    id: 'event.ironheart_envoy',
    title: 'An Envoy from Ironheart',
    speaker: CAST.mara,
    minAct: 2,
    text: 'An Ironheart envoy is waiting in the Hall. Daxton’s Directorate offers a supply contract — cheap steel, fast. Everyone knows what Ironheart contracts cost later.',
    choices: [
      { label: 'Accept the contract', effects: [gain('capital', 55), threat(10), flag('ironheart_debt')] },
      { label: 'Refuse politely', effects: [gain('influence', 6)] },
      { label: 'Counter-offer', detail: 'Trade on your terms.', effects: [gain('influence', -10), gain('capital', 30)] },
    ],
  },
  {
    id: 'event.labour_strike',
    title: 'Labour Strike',
    speaker: CAST.mara,
    minCycle: 18,
    condition: (s) => s.totals.employed >= 20 && s.totals.happiness < 62,
    text: 'The shift crews have downed tools. They say the city grew on their backs and they haven’t seen a share of it. They are not wrong.',
    choices: [
      { label: 'Raise wages', effects: [mod('Raised wages', 10, { upkeep: 0.18, approval: 8 })] },
      { label: 'Negotiate', effects: [gain('influence', -12), mod('New labour accord', 8, { approval: 4 })] },
      { label: 'Break the strike', effects: [mod('Broken strike', 10, { approval: -10 }), gain('influence', -4)] },
    ],
  },
  {
    id: 'event.seed_vault',
    title: 'The Seed Vault',
    speaker: CAST.amara,
    minAct: 2,
    text: 'The Eden Circuit has opened its seed vault to cities it trusts. Amara Venn offers Ashcroft a share — living infrastructure, grown rather than built.',
    choices: [
      { label: 'Accept the gift', effects: [grantCard('eden.balance_grove'), grantCard('eden.pollinator_corridor')] },
      { label: 'Trade knowledge for it', effects: [gain('insight', -15), grantCard('eden.living_waterway'), mod('Eden partnership', 10, { approval: 4 })] },
      { label: 'Decline', effects: [gain('influence', 4)] },
    ],
  },
  {
    id: 'event.wandering_engineer',
    title: 'A Wandering Engineer',
    speaker: CAST.tal,
    minCycle: 12,
    text: 'One of my old apprentices turned up at the gate with a cart full of designs. She’d work for Ashcroft — for a price.',
    choices: [
      { label: 'Hire her', effects: [gain('capital', -12), offerDraftEffect('A wandering engineer')] },
      { label: 'Buy her designs outright', effects: [gain('insight', -10), offerDraftEffect('Purchased designs')] },
      { label: 'Send her on', effects: [] },
    ],
  },
  {
    id: 'event.data_breach',
    title: 'Signal Breach',
    speaker: CAST.mira,
    minAct: 2,
    condition: (s) => s.totals.dataSupply > 0,
    text: 'Someone has been reading the city’s relay traffic. The intrusion pattern looks like Ironheart work.',
    choices: [
      { label: 'Purge and rebuild', effects: [gain('capital', -15), gain('insight', -6)] },
      { label: 'Feed them false data', effects: [gain('insight', -12), threat(-6)] },
      { label: 'Ignore it', effects: [threat(7), mod('Compromised relays', 6, { insight: -0.25 })] },
    ],
  },
  {
    id: 'event.clinic_outbreak',
    title: 'Fever in the Districts',
    speaker: CAST.ori,
    minCycle: 26,
    text: 'A fever is moving through the denser districts. It is not deadly, but it is spreading, and people are frightened.',
    choices: [
      { label: 'Fund emergency clinics', effects: [gain('capital', -24), mod('Public health drive', 6, { approval: 5 })] },
      { label: 'Quarantine districts', effects: [mod('Quarantine', 5, { capital: -0.2, approval: -3 })] },
      { label: 'Let it run its course', effects: [citizens(-6), mod('Fear in the streets', 8, { approval: -7 })] },
    ],
  },
]

export const EVENT_BY_ID: Record<string, CampaignEvent> = Object.fromEntries(EVENTS.map((e) => [e.id, e]))

export function eventChoices(state: GameState, event: CampaignEvent): Choice[] {
  return typeof event.choices === 'function' ? event.choices(state) : event.choices
}

export function eventText(state: GameState, event: CampaignEvent): string {
  return typeof event.text === 'function' ? event.text(state) : event.text
}

export function choiceCost(choice: Choice): Resources {
  const cost = { capital: 0, insight: 0, influence: 0 }
  for (const effect of choice.effects) for (const [key, value] of Object.entries(effect.cost ?? {})) {
    cost[key as keyof Resources] += value ?? 0
  }
  return cost
}

export function canAfford(state: GameState, cost: Partial<Resources>): boolean {
  return (Object.keys(cost) as (keyof Resources)[]).every((key) => state.resources[key] >= (cost[key] ?? 0))
}

/** Queue a story beat. Story outranks any open crisis, which is withdrawn. */
export function queueStory(state: GameState, id: string) {
  state.pendingEvent = id
}

export function resolveEvent(state: GameState, choiceIndex: number): { ok: boolean; reason?: string } {
  const event = state.pendingEvent ? EVENT_BY_ID[state.pendingEvent] : null
  if (!event) return { ok: false, reason: 'No decision is waiting' }
  const choice = eventChoices(state, event)[choiceIndex]
  if (!choice) return { ok: false, reason: 'Unknown choice' }
  if (!canAfford(state, choiceCost(choice))) return { ok: false, reason: 'The treasury cannot cover that choice' }
  state.pendingEvent = null
  for (const effect of choice.effects) effect.apply(state)
  if (!event.story) state.stats.eventsResolved += 1
  log(state, `${event.title}: ${choice.label}.`, event.story ? 'quest' : 'info')
  return { ok: true }
}

/** Count down to the next regional crisis and open one when it is due. */
export function maybeTriggerEvent(state: GameState) {
  if (state.outcome || state.pendingEvent || state.draft) return
  state.eventCooldown -= 1
  if (state.eventCooldown > 0 || state.cycle < 10) return
  const eligible = EVENTS.filter((e) => !e.story &&
    !state.recentEvents.includes(e.id) &&
    state.act >= (e.minAct ?? 1) &&
    state.cycle >= (e.minCycle ?? 0) &&
    (!e.condition || e.condition(state)))
  if (!eligible.length) return
  const event = eligible[Math.floor(Math.random() * eligible.length)]
  state.pendingEvent = event.id
  state.recentEvents.unshift(event.id)
  state.recentEvents.length = Math.min(state.recentEvents.length, 6)
  const [lo, hi] = DIFFICULTY[state.difficulty].eventGap
  state.eventCooldown = randInt(lo, hi)
}

function setDoctrine(state: GameState, doctrine: Doctrine) {
  state.doctrine = doctrine
  state.act = 2
  log(state, 'Act II — Regional Crisis. The basin is watching Ashcroft now.', 'quest')
}

function setPath(state: GameState, path: ConquestPath) {
  state.path = path
  state.act = 3
  state.threat = Math.max(state.threat, 12)
  state.raidTimer = 7
  relabelQuests(state)
  log(state, 'Act III — Iron at the Border. Ironheart Dominance is rising.', 'quest')
}

function victory(title: string, text: string): Effect {
  return { text: 'Ends the campaign', tone: 'good', apply: (s) => { s.outcome = { kind: 'victory', title, text } } }
}

// ---- deck: drafts and tactics ---------------------------------------------

export function addCardToDeck(state: GameState, definitionId: string) {
  const id = nextId('ci')
  state.instances[id] = { id, definitionId, worldAssetId: null }
  // New cards go on top of the draw pile so the player meets them soon.
  state.draw.unshift(id)
  refillHand(state)
}

const DRAFT_EXCLUDED = new Set([FOUNDER_HALL_ID, 'forgeweave.forge_lord_daxton_rhe'])

export function draftPool(act: 1 | 2 | 3): CardDefinition[] {
  return ALL_DEFINITIONS.filter((def) => {
    if (DRAFT_EXCLUDED.has(def.id)) return false
    if (!def.placeable && !TACTICS[def.id]) return false
    if ((def.rarity === 'Wonder' || def.rarity === 'Elite' || def.cardType === 'Leader') && act < 2) return false
    return true
  })
}

export function offerDraft(state: GameState, reason: string) {
  if (state.draft || state.outcome) return
  const pool = draftPool(state.act)
  const weighted = pool.flatMap((def) => {
    const weight = def.rarity === 'Wonder' ? 1 : def.rarity === 'Elite' || def.rarity === 'Leader' ? 2 : def.faction === 'Synara' ? 2 : 4
    return Array.from({ length: weight }, () => def.id)
  })
  const options: string[] = []
  for (let guard = 0; options.length < 3 && guard < 200; guard++) {
    const pick = weighted[Math.floor(Math.random() * weighted.length)]
    if (!options.includes(pick)) options.push(pick)
  }
  state.draft = { options, reason }
}

export const DRAFT_SKIP_CAPITAL = 15

export function resolveDraft(state: GameState, definitionId: string | null) {
  if (!state.draft) return
  if (definitionId && state.draft.options.includes(definitionId)) {
    addCardToDeck(state, definitionId)
    state.stats.cardsDrafted += 1
    log(state, `${definition(definitionId).displayName} joins your deck.`, 'good')
  } else {
    state.resources.capital += DRAFT_SKIP_CAPITAL
    log(state, `Draft declined — ${DRAFT_SKIP_CAPITAL} Capital banked instead.`)
  }
  state.draft = null
}

export interface Tactic {
  label: string
  text: string
  cost: Partial<Resources>
  apply: (state: GameState) => void
}

/** Units and leaders are not buildings: they are played from hand as tactics. */
export const TACTICS: Record<string, Tactic> = {
  'synara.guardian_drone_cohort': {
    label: 'Deploy drones', text: '+14 Defense for 10 cycles.', cost: { capital: 8 },
    apply: (s) => addModifier(s, 'Guardian drones', 10, { defense: 14 }),
  },
  'synara.audit_sentinel': {
    label: 'Run an audit', text: 'Recover 20 Capital and restore every sabotaged building.', cost: { insight: 4 },
    apply: (s) => { s.resources.capital += 20; s.sabotaged = {} },
  },
  'synara.archon_mira_vey': {
    label: 'Address the city', text: '+8 approval and +30% Insight for 10 cycles.', cost: { influence: 6 },
    apply: (s) => addModifier(s, 'Archon’s address', 10, { approval: 8, insight: 0.3 }),
  },
  'forgeweave.forge_guard': {
    label: 'Post the Forge Guard', text: '+20 Defense for 10 cycles.', cost: { capital: 12 },
    apply: (s) => addModifier(s, 'Forge Guard', 10, { defense: 20 }),
  },
  'forgeweave.mechanist_crew': {
    label: 'Rush construction', text: 'Finish every construction site next cycle.', cost: { capital: 10 },
    apply: (s) => { for (const a of s.assets) if (!a.operational) a.cyclesRemaining = Math.min(a.cyclesRemaining, 1) },
  },
  'eden.ranger_circle': {
    label: 'Send the rangers', text: '−12 Ironheart Dominance and +4 approval for 6 cycles.', cost: { influence: 4 },
    apply: (s) => { s.threat = Math.max(0, s.threat - 12); addModifier(s, 'Ranger patrols', 6, { approval: 4 }) },
  },
  'eden.symbiosis_keepers': {
    label: 'Tend the districts', text: '+12 approval for 10 cycles.', cost: { capital: 6 },
    apply: (s) => addModifier(s, 'Symbiosis keepers', 10, { approval: 12 }),
  },
  'eden.caretaker_amara_venn': {
    label: 'Invite the Caretaker', text: '+25% Capital, Insight and Influence for 10 cycles.', cost: { influence: 8 },
    apply: (s) => addModifier(s, 'Caretaker’s blessing', 10, { capital: 0.25, insight: 0.25, influence: 0.25 }),
  },
}

export function isTactic(definitionId: string): boolean {
  return Boolean(TACTICS[definitionId])
}

export function playTactic(state: GameState, instanceId: string): { ok: boolean; reason?: string } {
  const instance = state.instances[instanceId]
  if (!instance || !state.hand.includes(instanceId)) return { ok: false, reason: 'Card is not in hand' }
  const tactic = TACTICS[instance.definitionId]
  if (!tactic) return { ok: false, reason: 'That card is a building — place it on the map' }
  if (!canAfford(state, tactic.cost)) return { ok: false, reason: 'Not enough resources for this tactic' }
  for (const [key, value] of Object.entries(tactic.cost)) state.resources[key as keyof Resources] -= value ?? 0
  tactic.apply(state)
  state.hand = state.hand.filter((id) => id !== instanceId)
  state.discard.push(instanceId)
  if (state.selectedInstanceId === instanceId) state.selectedInstanceId = null
  state.stats.tacticsPlayed += 1
  log(state, `${definition(instance.definitionId).displayName}: ${tactic.label}.`, 'good')
  refillHand(state)
  return { ok: true }
}

export const marketCost = (state: GameState) => 35 + state.stats.marketBuys * 20

/** Capital sink: buy an extra draft from the basin market. */
export function buyDraft(state: GameState): { ok: boolean; reason?: string } {
  if (state.draft) return { ok: false, reason: 'Finish your current draft first' }
  const cost = marketCost(state)
  if (state.resources.capital < cost) return { ok: false, reason: `Needs ${cost} Capital` }
  state.resources.capital -= cost
  state.stats.marketBuys += 1
  offerDraft(state, 'Basin market')
  return { ok: true }
}

export const GARRISON_COST = 30
export const GARRISON_DEFENSE = 12
export const GARRISON_CYCLES = 8

/** Capital sink: hire a temporary garrison against Ironheart raids. */
export function fundGarrison(state: GameState): { ok: boolean; reason?: string } {
  if (state.act < 3) return { ok: false, reason: 'There is no one to defend against yet' }
  if (state.resources.capital < GARRISON_COST) return { ok: false, reason: `Needs ${GARRISON_COST} Capital` }
  state.resources.capital -= GARRISON_COST
  addModifier(state, 'Hired garrison', GARRISON_CYCLES, { defense: GARRISON_DEFENSE })
  log(state, `A garrison is hired (+${GARRISON_DEFENSE} Defense for ${GARRISON_CYCLES} cycles).`, 'good')
  return { ok: true }
}

// ---- Ironheart --------------------------------------------------------------

export const PRESSURE_COST = 12
export const PRESSURE_EFFECT = 8

/** Spend Influence on diplomacy to push Ironheart Dominance back. */
export function applyPressure(state: GameState): { ok: boolean; reason?: string } {
  if (state.act < 3) return { ok: false, reason: 'Ironheart has not moved against you yet' }
  if (state.resources.influence < PRESSURE_COST) return { ok: false, reason: `Needs ${PRESSURE_COST} Influence` }
  state.resources.influence -= PRESSURE_COST
  state.threat = Math.max(0, state.threat - PRESSURE_EFFECT)
  log(state, `Diplomatic pressure pushes Ironheart back (−${PRESSURE_EFFECT} Dominance).`, 'good')
  return { ok: true }
}

export function sabotage(state: GameState, count: number, cycles: number): string[] {
  const targets = state.assets.filter((a) => a.operational && a.definitionId !== FOUNDER_HALL_ID && !state.sabotaged[a.id])
  const hit: string[] = []
  for (let i = 0; i < count && targets.length; i++) {
    const [target] = targets.splice(Math.floor(Math.random() * targets.length), 1)
    state.sabotaged[target.id] = cycles
    hit.push(definition(target.definitionId).displayName)
  }
  return hit
}

export function threatRate(state: GameState, totals: CityTotals, threatFx: number): number {
  if (state.act < 3) return 0
  const overdrive = state.flags.includes('overdrive')
  const base = (overdrive ? 2.0 : 1.15) * DIFFICULTY[state.difficulty].threat
  return Math.max(overdrive ? 0.45 : 0.1, base + threatFx - totals.defense * 0.018)
}

export function raidStrength(state: GameState): number {
  const overdrive = state.flags.includes('overdrive') ? 1.25 : 1
  return Math.round((18 + state.raidCount * 7) * DIFFICULTY[state.difficulty].raid * overdrive)
}

export interface RaidReport { repelled: boolean; strength: number; defense: number; hit: string[]; stolen: number }

/** Advance Ironheart by one cycle. Returns a raid report when a raid lands. */
export function advanceIronheart(state: GameState, totals: CityTotals, threatFx: number): RaidReport | null {
  if (state.act < 3 || state.outcome) return null
  state.threat = Math.min(100, state.threat + threatRate(state, totals, threatFx))
  if (state.flags.includes('overdrive')) state.overdriveCycles += 1
  state.raidTimer -= 1
  if (state.raidTimer > 0) return null

  const overdrive = state.flags.includes('overdrive')
  state.raidTimer = overdrive ? randInt(4, 5) : randInt(6, 8)
  const strength = raidStrength(state)
  state.raidCount += 1
  if (totals.defense >= strength) {
    state.threat = Math.max(0, state.threat - 6)
    state.resources.influence += 3
    state.stats.raidsRepelled += 1
    log(state, `Ironheart raid repelled (${totals.defense} Defense vs ${strength}). −6 Dominance.`, 'good')
    return { repelled: true, strength, defense: totals.defense, hit: [], stolen: 0 }
  }
  const breach = strength - totals.defense
  const hit = sabotage(state, 1 + Math.floor(breach / 18), 3)
  const stolen = Math.round(state.resources.capital * 0.12)
  state.resources.capital -= stolen
  state.threat = Math.min(100, state.threat + 4)
  state.stats.raidsSuffered += 1
  log(state, `Ironheart raid breached the district (${totals.defense} Defense vs ${strength}). ${hit.join(', ') || 'Nothing'} sabotaged, ${stolen} Capital seized.`, 'warn')
  return { repelled: false, strength, defense: totals.defense, hit, stolen }
}

// ---- endings ---------------------------------------------------------------

export function checkDefeat(state: GameState, capitalDelta: number) {
  if (state.outcome) return
  const limit = DIFFICULTY[state.difficulty].strikes
  if (state.resources.capital < 1 && capitalDelta < 0) state.strikes.insolvency += 1
  else state.strikes.insolvency = Math.max(0, state.strikes.insolvency - 1)
  if (state.cycle > 10 && state.totals.happiness < 20) state.strikes.unrest += 1
  else state.strikes.unrest = Math.max(0, state.strikes.unrest - 1)

  if (state.strikes.insolvency === Math.ceil(limit / 2)) log(state, 'The council warns of insolvency. Cut costs or raise income now.', 'warn')
  if (state.strikes.unrest === Math.ceil(limit / 2)) log(state, 'The districts are close to revolt. Raise approval now.', 'warn')

  if (state.threat >= 100) {
    state.outcome = { kind: 'defeat', title: 'Ashcroft Falls', text: 'Ironheart’s Dominance became total. The Forge Lord’s banners hang from the Founder Hall, and Ashcroft’s furnaces now burn for someone else.' }
  } else if (state.strikes.insolvency >= limit) {
    state.outcome = { kind: 'defeat', title: 'The Treasury Collapses', text: 'Maintenance outran income for too long. The council dissolved the Authority, and the basin’s builders went home unpaid.' }
  } else if (state.strikes.unrest >= limit) {
    state.outcome = { kind: 'defeat', title: 'The Districts Revolt', text: 'Approval collapsed and the districts stopped listening. A city is a promise, and Ashcroft broke its own.' }
  }
}

export function tickModifiers(state: GameState) {
  for (const m of state.modifiers) if (m.cyclesLeft > 0) m.cyclesLeft -= 1
  const expired = state.modifiers.filter((m) => m.cyclesLeft === 0)
  for (const m of expired) log(state, `${m.label} has ended.`)
  state.modifiers = state.modifiers.filter((m) => m.cyclesLeft !== 0)
  for (const id of Object.keys(state.sabotaged)) {
    state.sabotaged[id] -= 1
    if (state.sabotaged[id] <= 0) delete state.sabotaged[id]
  }
}

export interface Score { total: number; grade: string; lines: [string, number][] }

export function computeScore(state: GameState): Score {
  const quests = state.quests.filter((q) => q.status === 'complete').length
  const lines: [string, number][] = [
    ['Objectives completed', quests * 120],
    ['Peak population', state.stats.peakPopulation * 6],
    ['City assets', state.assets.length * 15],
    ['Raids repelled', state.stats.raidsRepelled * 60],
    ['Crises resolved', state.stats.eventsResolved * 25],
    ['Approval', state.totals.happiness * 4],
    ['Victory', state.outcome?.kind === 'victory' ? 1000 : 0],
    ['Swiftness', state.outcome?.kind === 'victory' ? Math.max(0, 600 - state.cycle * 2) : 0],
  ]
  const multiplier = state.difficulty === 'ascendant' ? 1.4 : state.difficulty === 'settler' ? 0.75 : 1
  const total = Math.round(lines.reduce((sum, [, v]) => sum + v, 0) * multiplier)
  const grade = total >= 4200 ? 'S' : total >= 3400 ? 'A' : total >= 2600 ? 'B' : total >= 1600 ? 'C' : 'D'
  return { total, grade, lines }
}
