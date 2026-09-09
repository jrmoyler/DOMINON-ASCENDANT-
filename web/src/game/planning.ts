import { projectNextCycle as projectCycle } from './economy'
import type { GameState, Resources } from './types'

export { previewPlacement } from './state'

/** Resource forecast uses the same resolution rules as the authoritative cycle. */
export function projectNextCycle(state: GameState): Resources {
  const next = projectCycle(state)
  return { capital: next.capitalDelta, insight: next.insightDelta, influence: next.influenceDelta }
}
