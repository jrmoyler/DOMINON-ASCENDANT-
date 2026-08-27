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
| `npm run sim` | Headless balance harness |

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
  render/        Babylon.js runtime — no React, no game rules
    Scene.ts       Cinematic world, camera, picking, overlays, reconciliation
    architecture.ts Deterministic authored silhouette plans
    buildings.ts   Custom lofted structures, facade bands and spires
    strategicAtmosphere.ts Three.js curve-authoring boundary
    palette.ts     Faction and material direction
  ui/            React HUD layered over the canvas
    motion.ts      anime.js interface and cinematic reveal choreography
```

The three layers only depend downward: `ui` reads `game` and `render`, `render`
reads `game`, and `game` depends on nothing but the content manifests. That
mirrors the module direction the Unreal slice enforces.

Babylon.js owns the only visible WebGL scene. Three.js is deliberately isolated
to neutral atmosphere-curve authoring, so the engines never compete for the
same canvas or render loop. Shipped 3D content remains GLB/glTF-first; the
built-in city kit uses custom vertex lofts rather than visible box, plane,
cylinder, sphere, or ring placeholders.

## Content is the single source of truth

`vite.config.ts` aliases `@content` to `../Content`, so card definitions, the
exact 60-card starter deck and the quest chain are imported directly from the
manifests the Unreal project consumes. Changing a manifest changes both builds.

The manifests intentionally leave most per-card economy values unauthored. The
derivation table in `src/game/content.ts` fills those gaps for the browser
slice; it is tuning, not balance canon, and the in-game inspector says so.

See `../docs/PLAYING.md` for controls, design notes and the Unreal build path.
