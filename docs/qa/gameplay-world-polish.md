# Gameplay and world polish — verification record

## Scope

Browser campaign upgrade on `feat/gameplay-world-polish`, based on main `0df073d`.
The Unreal source and the frozen content manifests are preserved. This pass upgrades the
existing Synara city-management slice; it does not add the unimplemented combat or world-map campaign.

## Implemented

- Guided opening hand drawn from the unchanged 60-card deck; unified placement validation and local utility warnings.
- Canonical save graph validation, previous-checkpoint recovery, cycle autosave, paused restores and protected campaign actions.
- Independent simulation clock and UI pulse, memoized economy projection, cached/batched utility overlays.
- Civic field-journal UI: title, compact currency/time controls, expandable objectives/services/events, accessible deployment cards, field guide and native modal dialogs.
- Daylight terrain, civic streetscape, surrounding skyline and eight instanced municipal vehicles; original detailed Three.js architecture adapted into the existing Babylon runtime.
- Anime.js construction progression and completion, controlled UI transitions, reduced-motion support and animation cleanup.
- Correct rotated façades, high-DPI picking, touch placement, pinch cancellation and camera recentering.

## Verified

- All 127 existing Python portable tests passed: Content 69, UI 18, Performance 8, Release 32.
- The existing 400-cycle headless browser simulation completed all nine quests and reached Ascension. This is a simulation run, not a browser interaction test or proof of balance across every shuffle.
- Final web suite: 50 tests passed across 9 files. TypeScript + Vite production build passed (812 modules, main bundle 492.61 kB gzip).
- Actual GLB geometry received a CPU silhouette review. The first pass was rejected for repetitive massing; the corrected pass has distinct family silhouettes. Both sheets and the correction history are preserved.
- Independent agent review caught and corrected façade orientation, excessive overlay draw calls, keyboard shortcut focus handling, duplicate campaign confirmations, and terrain/context elevation conflicts.

## Evidence limits

The supported cloud browser could not open the local preview: navigation returned
`net::ERR_BLOCKED_BY_CLIENT`. The preview service itself reported running. No desktop or
mobile screenshots, actual WebGL gameplay validation, visual acceptance scores, or physical-device
frame-rate measurements are claimed. NullEngine geometry tests verify structure, not rendered appearance.

The repository supplied no reference images. Image-to-Three.js guidance informed the geometry and
quality workflow, but reference reconstruction/likeness acceptance was not run. Original authored
architecture and matching GLB interchange exports are documented under `docs/art-assets/`.
Blender execution status and reproducible source are documented there separately; exported files alone
must not be treated as proof of a Blender rendering pass.

## Browser acceptance still required

At desktop, narrow portrait and short landscape sizes:

1. Inspect title and command entry; start and continue only with a ready renderer.
2. Select cards by pointer and keys; rotate after a card click; cancel with Escape. Check all currency shortfalls.
3. Build near and outside utility coverage. Verify the footprint, true façade rotation, construction animation, services and next-cycle income.
4. Orbit/pan/zoom; tap on touch; pinch without placing an accidental building; use Recenter. UI clicks must not place buildings beneath controls.
5. Open every drawer, city metric and campaign control; check reachable 44px targets, card scrolling, safe areas and readable text.
6. Save, restore, cancel/confirm new campaign; verify paused restoration and autosave after a cycle.
7. Check modal focus/Escape and reduced motion. Hide and restore the tab without background progression.
8. Inspect actual river visibility, scenery foundations, shadows and building windows at multiple angles. Measure performance with a developed city and a coverage overlay.

Keep the PR in draft until the missing visual/browser acceptance is completed.
