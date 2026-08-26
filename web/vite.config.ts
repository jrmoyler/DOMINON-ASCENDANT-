import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import { resolve } from 'node:path'

// The canonical game content lives in ../Content (shared with the Unreal
// project). We import those manifests directly so the web slice and the
// Unreal slice can never drift apart.
const repoRoot = resolve(__dirname, '..')

export default defineConfig({
  plugins: [react()],
  resolve: {
    alias: {
      '@content': resolve(repoRoot, 'Content'),
      '@': resolve(__dirname, 'src'),
    },
  },
  server: {
    fs: { allow: [repoRoot] },
  },
  build: {
    outDir: 'dist',
    sourcemap: false,
    chunkSizeWarningLimit: 1200,
  },
})
