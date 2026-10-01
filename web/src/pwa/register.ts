/// <reference types="vite/client" />
/** Registers the offline service worker in production web builds only. */
export function registerServiceWorker(): void {
  if (!import.meta.env.PROD || import.meta.env.MODE === 'single') return
  if (typeof window === 'undefined' || !('serviceWorker' in navigator)) return
  if (location.protocol !== 'https:' && location.hostname !== 'localhost' && location.hostname !== '127.0.0.1') return
  window.addEventListener('load', () => {
    navigator.serviceWorker.register(`${import.meta.env.BASE_URL}sw.js`).catch((err) => {
      console.warn('[pwa] service worker registration failed', err)
    })
  })
}
