#!/usr/bin/env node
// Turns `vite build --mode single --outDir dist-single-tmp` output into one
// self-contained HTML file that runs when double-clicked (file://).
// Module scripts loaded by `src` are blocked on file://, but inline module
// scripts are fine, so every script and stylesheet is inlined here.
import fs from 'node:fs'
import path from 'node:path'
import { fileURLToPath } from 'node:url'

const webDir = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..')
const tmpDir = path.join(webDir, 'dist-single-tmp')
const outDir = path.join(webDir, 'dist-single')
const outFile = path.join(outDir, 'DominionAscendant.html')

if (!fs.existsSync(path.join(tmpDir, 'index.html'))) {
  console.error('[build-single] dist-single-tmp/index.html missing; run `vite build --mode single --outDir dist-single-tmp` first.')
  process.exit(1)
}

let html = fs.readFileSync(path.join(tmpDir, 'index.html'), 'utf8')
const read = (ref) => {
  const clean = ref.split(/[?#]/)[0].replace(/^\.?\//, '')
  const file = path.join(tmpDir, clean)
  if (!fs.existsSync(file)) throw new Error(`[build-single] referenced file not found: ${ref}`)
  return fs.readFileSync(file, 'utf8')
}

// Escape sequences that would terminate or confuse an inline <script>/<style>.
const safeScript = (js) => js.replace(/<\/script/gi, '<\\/script').replace(/<!--/g, '<\\!--')
const safeStyle = (css) => css.replace(/<\/style/gi, '<\\/style')

const scripts = []
html = html.replace(/<script\b[^>]*\bsrc="([^"]+)"[^>]*>\s*<\/script>/g, (_m, src) => {
  scripts.push(safeScript(read(src)))
  return ''
})
html = html.replace(/<link\b[^>]*\brel="stylesheet"[^>]*>/g, (tag) => {
  const href = /\bhref="([^"]+)"/.exec(tag)?.[1]
  return href ? `<style>${safeStyle(read(href))}</style>` : tag
})
// No manifest, service worker, preload hints or files from public/ offline.
html = html
  .replace(/<link\b[^>]*\brel="(?:manifest|modulepreload|preload|apple-touch-icon)"[^>]*>\s*/g, '')
  .replace(/<link\b[^>]*\brel="icon"[^>]*>\s*/g, '')
  .replace(/<meta\b[^>]*(?:og:image|twitter:image)[^>]*>\s*/g, '')

// Embed the favicon so the tab still has the crest.
const favicon = path.join(webDir, 'public', 'favicon.svg')
if (fs.existsSync(favicon)) {
  const uri = 'data:image/svg+xml;base64,' + fs.readFileSync(favicon).toString('base64')
  html = html.replace('</head>', `  <link rel="icon" type="image/svg+xml" href="${uri}" />\n  </head>`)
}

// Module scripts are deferred; inlined ones are not, so place them at the end
// of <body> after #root to preserve ordering semantics.
const inline = scripts.map((js) => `<script type="module">${js}</script>`).join('\n')
if (!html.includes('</body>')) throw new Error('[build-single] no </body> in built HTML')
const idx = html.lastIndexOf('</body>')
html = html.slice(0, idx) + inline + '\n' + html.slice(idx)

// Use replacement callbacks above so `$&` etc. in bundles are never interpreted.
fs.mkdirSync(outDir, { recursive: true })
fs.writeFileSync(outFile, html)
fs.rmSync(tmpDir, { recursive: true, force: true })
const mb = (fs.statSync(outFile).size / 1024 / 1024).toFixed(2)
console.log(`[build-single] wrote ${path.relative(webDir, outFile)} (${mb} MB, ${scripts.length} script(s) inlined)`)
