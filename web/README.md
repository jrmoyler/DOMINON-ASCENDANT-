# DOMINION // ASCENDANT — browser slice

A browser-playable vertical slice of DOMINION // ASCENDANT, built on the same
content manifests as the Unreal Engine project in the repository root.

```bash
npm install
npm run dev      # http://localhost:5173
```

| Command | What it does |
|---|---|
| `npm run dev` | Dev server with hot reload |
| `npm run build` | Typecheck, then bundle to `dist/` |
| `npm run preview` | Serve the production bundle |
| `npm run typecheck` | Types only |
| `npx tsx --tsconfig tsconfig.app.json sim.ts` | Headless balance harness |

## Layout

```
src/
  game/          Simulation — no rendering, no React
    content.ts     Resolves Content/DA/Manifests/*.json into playable cards
    types.ts       Core types
    grid.ts        32x32 logical grid, footprints, placement rules
    economy.ts     Development Cycle resolution, utilities, migration
    quests.ts      First-hour quest chain evaluated against live state
    state.ts       Campaign state, deck, placement, save/load
  render/        Three.js — no React, no game rules
    Scene.ts       Camera rig, raycasting, overlays, mesh reconciliation
    buildings.ts   Procedural graybox meshes
    palette.ts     Visual direction
  ui/            React HUD layered over the canvas
```

The three layers only depend downward: `ui` reads `game` and `render`, `render`
reads `game`, and `game` depends on nothing but the content manifests. That
mirrors the module direction the Unreal slice enforces.

## Content is the single source of truth

`vite.config.ts` aliases `@content` to `../Content`, so card definitions, the
exact 60-card starter deck and the quest chain are imported directly from the
manifests the Unreal project consumes. Changing a manifest changes both builds.

The manifests intentionally leave most per-card economy values unauthored. The
derivation table in `src/game/content.ts` fills those gaps for the browser
slice; it is tuning, not balance canon, and the in-game inspector says so.

See `../docs/PLAYING.md` for controls, design notes and the Unreal build path.
