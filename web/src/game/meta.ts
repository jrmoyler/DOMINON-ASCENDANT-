import { computeScore } from './campaign'
import type { Difficulty, GameState } from './types'

/** Cross-campaign profile: honours, endings seen and best scores. Browser-local. */
export interface Profile {
  achievements: string[]
  endings: string[]
  best: Partial<Record<Difficulty, number>>
  victories: number
  campaigns: number
}

export interface Achievement {
  id: string
  title: string
  text: string
  test: (s: GameState) => boolean
}

export const ACHIEVEMENTS: Achievement[] = [
  { id: 'first_light', title: 'First Light', text: 'Bring your first utility online.', test: (s) => s.quests[0]?.status === 'complete' },
  { id: 'ascended', title: 'Convergence Authority', text: 'Complete the First Hour and earn Ascension.', test: (s) => s.ascended },
  { id: 'centurion', title: 'A City of Hundreds', text: 'Reach 100 citizens.', test: (s) => s.population >= 100 },
  { id: 'metropolis', title: 'Metropolis', text: 'Reach 160 citizens.', test: (s) => s.population >= 160 },
  { id: 'beloved', title: 'Beloved', text: 'Hold 95% approval.', test: (s) => s.totals.happiness >= 95 },
  { id: 'treasury', title: 'The Deep Treasury', text: 'Hold 1,000 Capital at once.', test: (s) => s.resources.capital >= 1000 },
  { id: 'wonder', title: 'Wonder of the Basin', text: 'Complete a Wonder.', test: (s) => s.assets.some((a) => a.operational && a.definitionId.includes('.the_')) },
  { id: 'collector', title: 'Collector', text: 'Draft 8 cards in one campaign.', test: (s) => s.stats.cardsDrafted >= 8 },
  { id: 'iron_wall', title: 'Iron Wall', text: 'Repel 5 Ironheart raids.', test: (s) => s.stats.raidsRepelled >= 5 },
  { id: 'crisis_manager', title: 'Crisis Manager', text: 'Resolve 10 regional crises.', test: (s) => s.stats.eventsResolved >= 10 },
  { id: 'victor', title: 'The Basin Is Yours', text: 'Win a campaign.', test: (s) => s.outcome?.kind === 'victory' },
  { id: 'ascendant_victor', title: 'Ascendant', text: 'Win on Ascendant difficulty.', test: (s) => s.outcome?.kind === 'victory' && s.difficulty === 'ascendant' },
  { id: 'untouched', title: 'Untouched', text: 'Win without a single raid breaching your walls.', test: (s) => s.outcome?.kind === 'victory' && s.stats.raidsSuffered === 0 && s.stats.raidsRepelled >= 3 },
  { id: 'all_endings', title: 'Every Road', text: 'See all five endings.', test: () => false },
]

const KEY = 'dominion-ascendant.profile.v1'

export function loadProfile(): Profile {
  const empty: Profile = { achievements: [], endings: [], best: {}, victories: 0, campaigns: 0 }
  try {
    const raw = localStorage.getItem(KEY)
    if (!raw) return empty
    const parsed = JSON.parse(raw) as Partial<Profile>
    return {
      achievements: Array.isArray(parsed.achievements) ? parsed.achievements.filter((a) => typeof a === 'string') : [],
      endings: Array.isArray(parsed.endings) ? parsed.endings.filter((a) => typeof a === 'string') : [],
      best: typeof parsed.best === 'object' && parsed.best ? parsed.best : {},
      victories: Number(parsed.victories) || 0,
      campaigns: Number(parsed.campaigns) || 0,
    }
  } catch {
    return empty
  }
}

function saveProfile(profile: Profile) {
  try { localStorage.setItem(KEY, JSON.stringify(profile)) } catch { /* storage unavailable */ }
}

/** Unlock any newly earned honours. Returns the newly unlocked achievements. */
export function checkAchievements(state: GameState): Achievement[] {
  const profile = loadProfile()
  const fresh = ACHIEVEMENTS.filter((a) => !profile.achievements.includes(a.id) && a.test(state))
  if (!fresh.length) return []
  profile.achievements.push(...fresh.map((a) => a.id))
  saveProfile(profile)
  return fresh
}

export function recordCampaignStart() {
  const profile = loadProfile()
  profile.campaigns += 1
  saveProfile(profile)
}

/** Record the end of a campaign. Returns whether this is a new best score. */
export function recordOutcome(state: GameState): { best: boolean; unlocked: Achievement[] } {
  const profile = loadProfile()
  const score = computeScore(state).total
  const best = score > (profile.best[state.difficulty] ?? 0)
  if (best) profile.best[state.difficulty] = score
  const unlocked: Achievement[] = []
  if (state.outcome?.kind === 'victory') {
    profile.victories += 1
    if (!profile.endings.includes(state.outcome.title)) profile.endings.push(state.outcome.title)
    if (profile.endings.length >= 5 && !profile.achievements.includes('all_endings')) {
      profile.achievements.push('all_endings')
      unlocked.push(ACHIEVEMENTS.find((a) => a.id === 'all_endings')!)
    }
  }
  saveProfile(profile)
  return { best, unlocked: [...unlocked, ...checkAchievements(state)] }
}
