import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import { resolve } from 'node:path'
import { fileURLToPath } from 'node:url'

// The canonical game content lives in ../Content (shared with the Unreal
// project). We import those manifests directly so the web slice and the
// Unreal slice can never drift apart.
const configDir = fileURLToPath(new URL('.', import.meta.url))
const repoRoot = resolve(configDir, '..')

export default defineConfig({
  plugins: [react()],
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
    chunkSizeWarningLimit: 2000,
  },
})
