# Enhanced Input asset setup (Unreal Engine 5.8)

The repository does not contain placeholder `.uasset` files. Unreal packages are opaque binaries and must be created and saved by Unreal Editor 5.8 so their package headers, object versions, and Git LFS payloads are genuine.

Create these Input Actions under `/Game/Input/Actions`:

| Asset | Value type | Default keyboard/mouse binding in `IMC_Founder` |
| --- | --- | --- |
| `IA_FounderMove` | Axis2D | W/S -> Y +/-; A/D -> X -/+ |
| `IA_FounderLook` | Axis2D | Mouse X/Y |
| `IA_FounderSprint` | Bool | Left Shift |
| `IA_FounderDodge` | Bool | Space |
| `IA_FounderInteract` | Bool | E |
| `IA_FounderTraversal` | Bool | Q |

Create and save these Input Mapping Context assets directly under `/Game/Input`:

- `IMC_Founder`: map all six Founder actions above. Use Enhanced Input's Negate and Swizzle Input Axis Values modifiers for the WASD Axis2D mapping.
- `IMC_City`: map project City pan, zoom, rotate, grid-select, cancel, and return-to-Founder actions. This context is the authority for grid selection; do not duplicate Founder movement bindings.
- `IMC_Command`: map project tactical pan, zoom, rotate, squad-select, objective/order, cancel, and return-to-Founder actions. This context is the authority for squad selection; do not duplicate Founder movement bindings.

The C++ soft references already target these exact package paths:

```text
/Game/Input/IMC_Founder.IMC_Founder
/Game/Input/IMC_City.IMC_City
/Game/Input/IMC_Command.IMC_Command
```

After saving, verify the three `.uasset` files are tracked through the repository's existing Git LFS rule. Create `/Game/Tests/Maps/L_ModeTransitionBenchmark` from the vertical-slice city map, keep its streaming configuration representative, place a Founder pawn with `DACameraModeController`, and place `DACameraModeFunctionalTest`. Run the map through Unreal's Functional Testing framework. The test drives each blend on real frames, fails when recorded wall-clock duration exceeds 1.5 seconds, and fails if the pawn, world, or level package identity changes.
