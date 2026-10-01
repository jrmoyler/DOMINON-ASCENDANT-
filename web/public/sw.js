/* DOMINION // ASCENDANT service worker: offline play after first visit. */
// Replaced at build time by vite.config.ts (dominion-sw-precache plugin).
const VERSION = 'dev'
const PRECACHE_ASSETS = [] /* __PRECACHE_ASSETS__ */
const SHELL_CACHE = `dominion-shell-${VERSION}`
const ASSET_CACHE = `dominion-assets-${VERSION}`
const RUNTIME_CACHE = `dominion-runtime-${VERSION}`
const KNOWN = [SHELL_CACHE, ASSET_CACHE, RUNTIME_CACHE]

const SHELL = [
  '/',
  '/index.html',
  '/manifest.webmanifest',
  '/favicon.svg',
  '/icons/icon-192.png',
  '/icons/icon-512.png',
  '/icons/apple-touch-icon.png',
]

self.addEventListener('install', (event) => {
  event.waitUntil(
    caches.open(SHELL_CACHE).then(async (cache) => {
      // Add individually so one missing file never blocks installation.
      await Promise.all(SHELL.map((url) => cache.add(new Request(url, { cache: 'reload' })).catch(() => {})))
      // Precache the hashed bundles referenced by the shell so the very first
      // offline launch works without having to play online first.
      try {
        const res = await cache.match('/index.html') || await cache.match('/')
        if (res) {
          const html = await res.clone().text()
          const urls = [...html.matchAll(/(?:src|href)="(\/assets\/[^"]+)"/g)].map((m) => m[1])
          const assets = await caches.open(ASSET_CACHE)
          const all = [...new Set([...urls, ...PRECACHE_ASSETS])]
          await Promise.all(all.map((u) => assets.add(u).catch(() => {})))
        }
      } catch { /* best effort */ }
    }).then(() => self.skipWaiting()),
  )
})

self.addEventListener('activate', (event) => {
  event.waitUntil(
    caches.keys()
      .then((keys) => Promise.all(keys.filter((k) => k.startsWith('dominion-') && !KNOWN.includes(k)).map((k) => caches.delete(k))))
      .then(() => self.clients.claim()),
  )
})

function isCacheable(res) {
  return res && res.ok && (res.type === 'basic' || res.type === 'default')
}

async function cacheFirst(request) {
  const cache = await caches.open(ASSET_CACHE)
  const hit = await cache.match(request)
  if (hit) return hit
  const res = await fetch(request)
  if (isCacheable(res)) cache.put(request, res.clone())
  return res
}

async function staleWhileRevalidate(request, cacheName) {
  const cache = await caches.open(cacheName)
  const hit = await cache.match(request)
  const network = fetch(request)
    .then((res) => {
      if (isCacheable(res)) cache.put(request, res.clone())
      return res
    })
    .catch(() => undefined)
  return hit || (await network) || Response.error()
}

async function navigation(request) {
  // Network first for HTML so new deploys are picked up immediately;
  // fall back to the cached shell when offline.
  const cache = await caches.open(SHELL_CACHE)
  try {
    const res = await fetch(request)
    if (isCacheable(res)) cache.put('/index.html', res.clone())
    return res
  } catch {
    return (await cache.match('/index.html')) || (await cache.match('/')) || Response.error()
  }
}

self.addEventListener('fetch', (event) => {
  const { request } = event
  if (request.method !== 'GET') return
  const url = new URL(request.url)
  if (url.origin !== self.location.origin) return

  if (request.mode === 'navigate') {
    event.respondWith(navigation(request))
  } else if (url.pathname.startsWith('/assets/')) {
    event.respondWith(cacheFirst(request))
  } else {
    event.respondWith(staleWhileRevalidate(request, RUNTIME_CACHE))
  }
})

self.addEventListener('message', (event) => {
  if (event.data === 'SKIP_WAITING') self.skipWaiting()
})
