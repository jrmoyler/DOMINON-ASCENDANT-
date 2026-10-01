import type { GameState, LogEntry } from './types'

let idCounter = 0
export const nextId = (prefix: string) =>
  `${prefix}_${(idCounter++).toString(36)}_${Math.random().toString(36).slice(2, 7)}`

export function log(state: GameState, text: string, kind: LogEntry['kind'] = 'info') {
  state.log.unshift({ cycle: state.cycle, text, kind })
  if (state.log.length > 120) state.log.length = 120
}

export function shuffle<T>(items: T[]): T[] {
  const out = [...items]
  for (let i = out.length - 1; i > 0; i--) {
    const j = Math.floor(Math.random() * (i + 1))
    ;[out[i], out[j]] = [out[j], out[i]]
  }
  return out
}

export const randInt = (min: number, max: number) => min + Math.floor(Math.random() * (max - min + 1))

export const HAND_SIZE = 6

/** Draw back up to the hand limit from the draw pile, reshuffling discards. */
export function refillHand(state: GameState) {
  while (state.hand.length < HAND_SIZE) {
    if (state.draw.length === 0) {
      if (state.discard.length === 0) break
      state.draw = shuffle(state.discard)
      state.discard = []
      log(state, 'Deck reshuffled.')
    }
    const next = state.draw.shift()
    if (!next) break
    state.hand.push(next)
  }
}
