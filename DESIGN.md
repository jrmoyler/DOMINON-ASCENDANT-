# DOMINION // ASCENDANT — Ashcroft campaign interface

## Purpose and observable outcomes
A tactile city-conquest card game for mouse, keyboard and touch. Read the basin, select a building card, place it within utility reach, sustain citizens and finish the first-hour objectives. The world remains the primary surface. Players can identify their currencies, current objective, selected card cost and simulation state without opening a manual. Campaign management, detailed city metrics, overlays and event history remain reachable at every breakpoint.

## Direction
Industrial civic field journal: charcoal enamel command instruments, warm limestone lettering, restrained brass rules and mineral-green success states. Monumental editorial title, precise tabular economic information, numbered deployment cards. The real rendered basin provides the title backdrop; no reference image panels or generic stock imagery. Exclude glassmorphism, glowing borders, particle wallpaper, decorative clipping, fabricated factions and engine/development jargon in gameplay.

## Tokens and typography
Surfaces #151b1c and #202829; backdrop #101617. Primary text #f2eee3, secondary #b8c1bc, muted #929f9a, border #48514c. Brass action #d6bc80, mineral success #a8c8a2, warning #e6b478, danger #f1a19a. Title Georgia / Times New Roman, 58–112px with tight tracking, line-height .88. UI Trebuchet MS / Segoe UI / sans-serif 12–16px, line-height 1.4; numbers ui-monospace / Menlo 11–18px. Labels 10–11px, .12em tracking. No downloaded fonts or third-party component styles.

## Layout and inventory
8px spacing basis with 4px half-steps; 1px borders, 2px radius, opaque instruments with minimal shadow. Persistent interface: one small top currency/control assembly; edge objective/layer/event disclosure tabs; bottom scrollable hand. Center view remains unobstructed. Desktop hand caps at 1080px; mobile card widths stay readable and horizontally scroll, never compress six cards to illegibility. Detailed drawers overlay only when intentionally opened; inspector shows on building selection. Safe-area insets at screen boundaries, dynamic viewport height, 44px minimum interactive targets. Breakpoints 1100px, 760px, 480px and short landscape height.

Components remain local React: Intro (title, clear primary action, short field guide); Hud (currencies, cycle and speed, city ledger, campaign menu); CardHand (accessible selection, visible full cost, replacement, selected-card briefing and rotation); QuestPanel (next objective + progress and expandable quest list); OverlayPanel (map layers); InspectPanel (status, outputs/demand, confirmed decommission); LogPanel (event history). All bind directly to game state. No external component source copied; resource catalog evaluated as candidates, local components fit existing stack and interaction model.

## State, accessibility and motion
Native buttons for card selection; aria-pressed reflects selected cards, speeds and layers. Keyboard-visible focus, explicit labels on replacement and close actions, disclosure summaries keyboard operable. Unaffordable cards remain inspectable with visible funding deficit. Empty hand explains replacement/decommission options. Continue obeys renderer availability. Campaign reset and decommission require local confirmation because they erase progress or destroy an asset. Menus can be dismissed without committing. Success/error feedback derives from real state and remains distinct.

Anime.js gives title and command entry a brief stagger, and selected cards an intentional settling transition; no perpetual decorative motion. All motion respects prefers-reduced-motion and cleans up on unmount. CSS transitions 120–180ms; intro sequence below one second. Validate build/typecheck plus desktop/mobile browser passes for title start, readable hand, keyboard selection, rotation, economy/quest disclosure, inspector and save actions. Do not claim physical mobile validation from desktop emulation.
