/**
 * Progressive Web App install helpers.
 *
 * Usage:
 *   const off = onInstallAvailable((prompt) => {
 *     showInstallButton(async () => { const accepted = await prompt(); hideInstallButton() })
 *   })
 *   // later: off()
 */

interface BeforeInstallPromptEvent extends Event {
  prompt(): Promise<void>
  readonly userChoice: Promise<{ outcome: 'accepted' | 'dismissed'; platform: string }>
}

let deferred: BeforeInstallPromptEvent | null = null
const listeners = new Set<(prompt: () => Promise<boolean>) => void>()

function makePrompt(evt: BeforeInstallPromptEvent): () => Promise<boolean> {
  return async () => {
    if (deferred !== evt) return false // already used or superseded
    deferred = null
    try {
      await evt.prompt()
      const choice = await evt.userChoice
      return choice.outcome === 'accepted'
    } catch {
      return false
    }
  }
}

if (typeof window !== 'undefined') {
  window.addEventListener('beforeinstallprompt', (e) => {
    e.preventDefault()
    deferred = e as BeforeInstallPromptEvent
    const prompt = makePrompt(deferred)
    listeners.forEach((cb) => cb(prompt))
  })
  window.addEventListener('appinstalled', () => {
    deferred = null
  })
}

/**
 * Calls `cb` whenever the browser says the app can be installed. If that has
 * already happened before subscription, `cb` fires immediately. The supplied
 * `prompt()` shows the native install dialog (must be called from a user
 * gesture) and resolves true if the player accepted. Returns an unsubscribe.
 */
export function onInstallAvailable(cb: (prompt: () => Promise<boolean>) => void): () => void {
  listeners.add(cb)
  if (deferred) cb(makePrompt(deferred))
  return () => {
    listeners.delete(cb)
  }
}

/** True when running as an installed app (standalone/fullscreen window or iOS home screen). */
export function isStandalone(): boolean {
  if (typeof window === 'undefined') return false
  const nav = navigator as Navigator & { standalone?: boolean }
  if (nav.standalone) return true
  return ['standalone', 'fullscreen', 'minimal-ui', 'window-controls-overlay'].some(
    (m) => window.matchMedia?.(`(display-mode: ${m})`).matches,
  )
}
