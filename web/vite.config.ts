import { defineConfig, type Plugin } from 'vite'
import react from '@vitejs/plugin-react'
import { resolve, join } from 'node:path'
import { createHash } from 'node:crypto'
import { readFileSync, writeFileSync, existsSync } from 'node:fs'
import { fileURLToPath } from 'node:url'

// The canonical game content lives in ../Content (shared with the Unreal
// project). We import those manifests directly so the web slice and the
// Unreal slice can never drift apart.
const configDir = fileURLToPath(new URL('.', import.meta.url))
const repoRoot = resolve(configDir, '..')

// Social crawlers want an absolute og:image URL. Use SITE_URL when given,
// otherwise Vercel's production domain; leave the relative path if neither.
function absoluteShareImage(): Plugin {
  const raw = process.env.SITE_URL || process.env.VERCEL_PROJECT_PRODUCTION_URL || ''
  const origin = raw ? (raw.startsWith('http') ? raw : `https://${raw}`).replace(/\/+$/, '') : ''
  return {
    name: 'dominion-absolute-share-image',
    transformIndexHtml(html) {
      if (!origin) return html
      return html.replace(/content="\/og-image\.png"/g, `content="${origin}/og-image.png"`)
    },
  }
}

// Stamps dist/sw.js with a content-derived cache version and the full list of
// hashed bundles (including Babylon's lazily loaded shader chunks), so a new
// deploy invalidates old caches and the whole game is available offline.
function serviceWorkerPrecache(): Plugin {
  let outDir = 'dist'
  let assets: string[] = []
  return {
    name: 'dominion-sw-precache',
    apply: 'build',
    configResolved(config) {
      outDir = resolve(config.root, config.build.outDir)
    },
    generateBundle(_opts, bundle) {
      assets = Object.keys(bundle).filter((f) => f.startsWith('assets/')).map((f) => `/${f}`).sort()
    },
    writeBundle() {
      const swPath = join(outDir, 'sw.js')
      if (!existsSync(swPath)) return
      const version = createHash('sha256').update(assets.join('|')).digest('hex').slice(0, 12)
      const sw = readFileSync(swPath, 'utf8')
        .replace("const VERSION = 'dev'", () => `const VERSION = '${version}'`)
        .replace('const PRECACHE_ASSETS = [] /* __PRECACHE_ASSETS__ */', () => `const PRECACHE_ASSETS = ${JSON.stringify(assets)}`)
      writeFileSync(swPath, sw)
    },
  }
}

export default defineConfig(({ mode }) => {
  // `--mode single` produces one self-contained HTML file that runs from
  // file:// (see scripts/build-single.mjs). Everything is inlined, so no
  // code splitting, module preload, or absolute base paths.
  const single = mode === 'single'
  return {
    plugins: [react(), absoluteShareImage(), ...(single ? [] : [serviceWorkerPrecache()])],
    base: single ? './' : '/',
    resolve: {
      alias: {
        '@content': resolve(repoRoot, 'Content'),
        '@': resolve(configDir, 'src'),
      },
    },
    server: {
      fs: { allow: [repoRoot] },
    },
    build: {
      outDir: 'dist',
      sourcemap: false,
      // Babylon's PBR, shadow, glow and post-process runtime ships as one
      // cacheable engine chunk; its production gzip size is ~462 kB.
      chunkSizeWarningLimit: single ? 100000 : 2000,
      ...(single
        ? {
            assetsInlineLimit: 100000000,
            modulePreload: false,
            cssCodeSplit: false,
            copyPublicDir: false,
            rollupOptions: { output: { inlineDynamicImports: true } },
          }
        : {}),
    },
  }
})
