# Playing DOMINION // ASCENDANT

This repository ships the game in two forms. They share the same content
manifests under `Content/`, but they are different builds with different
requirements.

| | Browser slice (`web/`) | Unreal slice (`Source/`) |
|---|---|---|
| Runs on | Any modern browser | Windows PC |
| Engine | TypeScript + Three.js | Unreal Engine 5.8 |
| Deployed to | Vercel | Local build only |
| Status | Playable now | Source-complete, never compiled |

---

## 1. Browser slice — playable now

Deployed automatically by Vercel from the repository root (see `vercel.json`).

To run it locally:

```bash
cd web
npm install
npm run dev          # http://localhost:5173
```

Other commands:

```bash
npm run build        # typecheck + production bundle into web/dist
npm run preview      # serve the production bundle
npx tsx --tsconfig tsconfig.app.json sim.ts   # headless balance harness
```

### Controls

| Input | Action |
|---|---|
| Click a card, then click a cell | Place a building |
| Click a building | Inspect it |
| Drag | Orbit the camera |
| Shift+drag / right-drag | Pan |
| Scroll | Zoom |
| `R` | Rotate the footprint |
| `1`–`6` | Select a card from hand |
| `Space` | Pause / resume |
| `Esc` | Cancel selection |

### How it plays

You start with the frozen campaign opening from the production spec: the
Founder Hall, the exact 60-card Synara starter deck, 40 Capital, 12 Insight,
8 Influence and 24 citizens on a 32×32 grid.

The loop is: **utilities reach six cells.** Anything outside a supplier's range
gets no Power or Water, produces nothing, and drags approval down. Housing
raises the population ceiling, jobs employ the citizens who arrive, and retail
and industry pay for all of it. Approval falls when any of the three is
missing, and a large city costs goodwill just for being large.

Nine first-hour quests gate the run. Closing all nine reaches
`CONVERGENCE AUTHORITY: 1/20`.

### Where the numbers come from

Card names, factions, types, rarities, footprints and the exact 60-card starter
deck are read directly from `Content/DA/Manifests/VerticalSliceContent.json` at
build time — the same file the Unreal slice consumes. Quest titles and ordering
come from `Content/DA/Manifests/FirstHourQuests.json`.

The manifests deliberately leave most per-card economy values unauthored
(`"absent per-card values remain explicitly unauthored"`). The browser slice
fills those gaps with a derivation table in `web/src/game/content.ts`. Those
numbers are **web-slice tuning, not balance canon**, and the in-game inspector
labels them as derived.

Two deliberate deviations from Vertical Slice Production Spec v1.1:

- **Cycle length.** The spec fixes a Development Cycle at 30 s. That assumes the
  PC slice, where the player acts in real time between cycles. The browser slice
  is pure city management, so it runs a 10 s cycle. Every other ratio, including
  5 cycles per World Tick, is unchanged.
- **Scope.** The browser slice implements Zone A (the Synara frontier capital)
  and the first-hour quest spine. Ironheart, Eden Basin, the Daxton encounter,
  combat and the World Map are not implemented here.

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
