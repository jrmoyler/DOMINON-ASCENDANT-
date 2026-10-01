# Playing DOMINION // ASCENDANT

This repository ships the game in two forms. They share the same content
manifests under `Content/`, but they are different builds with different
requirements.

| | Browser slice (`web/`) | Unreal slice (`Source/`) |
|---|---|---|
| Runs on | Any modern browser | Windows PC |
| Engine | TypeScript + Babylon.js, Three.js geometry, Anime.js | Unreal Engine 5.8 |
| Deployed to | Vercel | Local build only |
| Status | Playable now | Source-complete, never compiled |

---

## 1. Browser game — playable now

Deployed automatically by Vercel from the repository root (see `vercel.json`).

### Getting it to players

| Option | How |
|---|---|
| Share a link | Send the Vercel URL. The page carries a share card image. |
| Install it | Open the link in Chrome, Edge or Android, then choose **Install game** on the title screen (or the browser's install icon). On iPhone use Share → *Add to Home Screen*. The installed game runs full-screen and works offline. |
| One-file download | `cd web && npm run build:single` writes `web/dist-single/DominionAscendant.html` (~3 MB). Anyone can double-click it to play in a browser, offline, with no install or server. |
| Release | Push a tag like `v1.0.0`. `.github/workflows/release.yml` builds and tests the game, then attaches the one-file HTML and a zipped web build to a GitHub Release. |

To run it locally:

```bash
cd web
npm install
npm run dev          # http://localhost:5173
```

Other commands:

```bash
npm run build        # typecheck + production bundle into web/dist
npm run package      # web build plus the single-file offline build
npm test             # unit tests, including campaign rules and save integrity
npm run sim -- governor 0 5   # headless balance bot: difficulty, path (0-3), runs
```

### Controls

| Input | Action |
|---|---|
| Click a card, then click a cell | Place a building |
| Click a tactic card, then **Play** (or `T`) | Play a unit or leader tactic |
| Click a building | Inspect it |
| Drag | Orbit the camera |
| Right-drag | Pan |
| Scroll / pinch | Zoom |
| `R` | Rotate the footprint |
| `1`–`6` | Select a card from hand |
| `1`–`4` | Answer an open decision or draft |
| `Space` | Pause / resume |
| `Esc` | Cancel selection |
| `Home` / Recenter | Restore the city camera |

### The campaign

A full campaign takes roughly 30–60 minutes. It has three acts, 15 objectives and five endings.

- **Act I: The First Hour.** These are the nine authored first-hour quests. You raise Ashcroft from the Founder Hall using the exact 60-card Synara starter deck. Completing the act earns Convergence Authority and a permanent **doctrine**: Replication, Concord or Verdance.
- **Act II: Regional Crisis.** Four regional objectives (Grid Strain, Housing Surge, the Green Line and the Foundry Shortage) test the city at scale.
- **Act III: Iron at the Border.** Forge Lord Daxton Rhe demands Ashcroft kneel. You choose a path (Force, Economy, Influence or Alliance), and that path rewrites your win condition. Ironheart **Dominance** rises every cycle and **raids** sabotage buildings when your Defense is lower than the raid's strength. Survive the Overdrive, then make the final choice.

Systems:

- **Deck building.** Every objective offers a three-card draft. Drafts draw on Forgeweave, Eden Circuit, Universal and Fusion cards, plus Wonders from Act II. The Basin Market sells extra drafts for Capital.
- **Tactics.** Units and leaders are not buildings. You play them from hand for an effect, such as Defense, approval, income, finishing construction or reducing Dominance, and they then return to your deck.
- **Regional crises.** Every few cycles a member of the cast brings a decision with real trade-offs. The cast is Tal Arden, Mara Kest, Ori Sen, Amara Venn and Daxton Rhe. Choices apply timed or permanent effects, shown in the Command panel.
- **Defeat.** A campaign is lost if any of these happens:
  - Dominance reaches 100%.
  - The treasury stays empty for too long (insolvency).
  - Approval stays collapsed for too long (unrest).
- **Difficulty.** Settler, Governor or Ascendant. Difficulty scales starting Capital, income, crisis frequency, Ironheart aggression and how long insolvency or unrest is tolerated before defeat.
- **Scoring and honours.** The end screen grades the campaign (S–D) and offers a shareable result. Fourteen honours, the endings you have seen, and your best score per difficulty are kept in the browser.
- **Presentation.**
  - A procedural score and sound effects, all synthesised with WebAudio and adjustable in Settings. The music changes with the act and the threat.
  - A day/night cycle with lit windows.
  - Floating income numbers, construction bursts, and camera shake on raids.

The campaign autosaves after every Development Cycle and keeps the previous valid checkpoint as a recovery copy. Loading starts paused.

### Where the numbers come from

Card names, factions, types, rarities, footprints and the exact 60-card starter
deck are read directly from `Content/DA/Manifests/VerticalSliceContent.json` at
build time — the same file the Unreal slice consumes. Quest titles and ordering
come from `Content/DA/Manifests/FirstHourQuests.json`.

The manifests deliberately leave most per-card economy values unauthored
(`"absent per-card values remain explicitly unauthored"`). The browser slice
fills those gaps with a derivation table in `web/src/game/content.ts`. Those
numbers are **web-slice tuning, not balance canon**. Their provenance stays in the
content module and developer documentation; the inspector presents player-facing costs and effects.

Pacing multipliers (`CAPITAL_PACE`, `INSIGHT_PACE`, `INFLUENCE_PACE` in `economy.ts`), denser housing, and a 90-asset build limit are also web-game tuning. The campaign layer in `web/src/game/campaign.ts` is web-game design built on the cast and event names authored in the Regional Crisis, Forgeweave Conquest, Daxton Encounter and First Ascension manifests.

Deliberate deviations from Vertical Slice Production Spec v1.1:

- **Cycle length.** The spec fixes a Development Cycle at 30 s. That assumes the
  PC slice, where the player acts in real time between cycles. The browser slice
  is pure city management, so it runs a 10 s cycle. Every other ratio, including
  5 cycles per World Tick, is unchanged.
- **Scope.** The browser game implements Zone A (the Synara frontier capital)
  and plays Ironheart and the Daxton encounter as campaign systems (Dominance,
  raids and story decisions) rather than as separate zones. Eden Basin and the
  World Map are not implemented here.

---

## 2. Unreal slice — building it on a Windows PC

The Unreal project is the real target of the production spec. It is
source-complete but **has never been compiled**: the environment it was authored
in had no engine toolchain, so no build, cook, or Automation run has ever been
proven. Expect to fix compile errors on the first pass.

### Requirements

- Windows 10/11
- Unreal Engine **5.8** (via the Epic Games Launcher)
- Visual Studio 2022 with the **Game development with C++** workload, including
  the Windows 10/11 SDK and MSVC v143 toolset
- Git LFS (`git lfs install`) — `.gitattributes` routes `.uasset`, `.umap`,
  `.ubulk` and `.uexp` through LFS
- Python 3.9+ for the portable content tests

### Build

```powershell
git clone https://github.com/jrmoyler/DOMINON-ASCENDANT-.git
cd DOMINON-ASCENDANT-
git lfs pull

# Generate the Visual Studio solution
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" ^
  -projectfiles -project="%CD%\DominionAscendant.uproject" -game -rocket -progress

# Build the editor target
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" ^
  DominionAscendantEditor Win64 Development -Project="%CD%\DominionAscendant.uproject" -WaitMutex
```

Then open `DominionAscendant.uproject` and press Play.

Alternatively, right-click the `.uproject` → *Generate Visual Studio project
files*, open the `.sln`, and build the `DominionAscendantEditor` target.

### Run the Automation tests

```powershell
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
  "%CD%\DominionAscendant.uproject" -unattended -nop4 -nullrhi ^
  -ExecCmds="Automation RunTests Dominion;Quit" ^
  -TestExit="Automation Test Queue Empty"
```

### Portable content tests

These need no engine and run anywhere Python does:

```bash
python3 -m unittest discover -s Tests/Content -p 'test_*.py'
python3 -m unittest discover -s Tests/UI -p 'test_*.py'
python3 -m unittest discover -s Tests/Performance -p 'test_*.py'
python3 -m unittest discover -s Tests/Release -p 'test_*.py'
```

All 127 currently pass.

### What is still open

`Build/Performance/Task27PerformanceEvidence.json` and
`Build/Release/Task28ReleaseEvidence.json` are both `NOT_READY` by design. They
stay that way until someone runs a real compile, cook, and reference-hardware
capture. Nothing in this repository claims otherwise.

### Why the Unreal build is not on Vercel

Vercel serves static assets and Node/Edge serverless functions. It cannot run
UnrealBuildTool, and Unreal dropped HTML5/WebAssembly export in 4.24, so there
is no path from this C++ project to a browser build. That is why the browser
slice exists as a separate implementation over the same content.
