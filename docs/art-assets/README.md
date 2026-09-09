# DOMINION architectural asset pass

Original, stylized civic architecture replaces the former radial neon shells. Twelve family factories author actual Three.js geometry: a U-shaped terraced residential courtyard, two pitched-roof market arcades, an office tower over a podium, a low sawtooth factory, an exposed cylindrical tank yard, a broad colonnaded forum, a domed observatory, a four-bastion fortress, a stepped obelisk monument, a motor hangar, a campanile assembly hall and twin towers connected by skybridges. Shared entrances, planting and facade details support these distinct massing systems.

The browser uses this source directly through a Three.js-to-Babylon vertex bridge. Repeated architecture is cached by family and unrotated lot dimensions; six material batches per instance limit draw calls. Instance rotation turns doors and facades with the played card. Materials and pick metadata remain instance-owned; disposal and construction scaling remain under CityScene. No external GLB fetch is needed for gameplay. The exported GLBs contain the same original geometry with named parts for DCC work; they are development/review assets, not a second network dependency.

## Reproduce

From `web`, run `node --import tsx scripts/assets/export-architecture.ts`. Then from the repository root:

```
python web/scripts/assets/optimize-architecture.py docs/art-assets/models /path/to/gltf-transform
python web/scripts/assets/render-architecture.py docs/art-assets
blender -b --factory-startup --threads 1 --python web/scripts/assets/blender-review.py -- docs/art-assets
```

The optimization uses glTF Transform 4.5.0: deduplication, welding and pruning without lossy simplification, new texture dependencies, renamed parts or codec requirements. The twelve exported lots total approximately 1.06 MB after optimization; exact sizes are recorded in the manifest. Every lot is 16×16 metres, Y-up, ground-centre pivot. Runtime dimensions follow the card footprint. The manifest records source path, triangles, mesh count and bytes. There are no textures. Gameplay grid occupancy is the collision contract; these architectural meshes are for selection and rendering, not dynamic body simulation. Distant skyline props use the world's independent simplified geometry.

## Evidence and limitations

- No reference images were attached to this request or committed in the repository. Image-to-Three.js's reference-admission/reconstruction gate therefore cannot run honestly. These are original authored models; there is no claimed reference fidelity score, accepted reconstruction gate, or photo match.
- `architecture-review.png` projects the actual exported GLB triangle positions/materials through an orthographic CPU rasterizer with depth testing. The sheet is useful for silhouette, family identity and topology review. It is explicitly **not** a WebGL gameplay screenshot or a Blender render, and does not establish runtime lighting/performance quality.
- Blender execution was attempted through installed-binary/bpy discovery (absent), pip (no available bpy distribution), apt (privilege operation errors), apt download (package unavailable), and official portable Blender 4.5.3. Portable download/extraction succeeded. The binary exited with SIGBUS on `--version` and SIGSEGV on background execution, both in the workspace and after a `/tmp` copy. No Blender render, cleaned Blender GLB or `.blend` file is claimed. `blender-review.py` preserves the reproducible next step: import actual GLBs, weld duplicates, recalculate normals, export cleaned models, save the assembly and render a contact sheet.
- Structural tests verify finite geometry/normals, occupied-lot bounds, family-specific components and <4,000 triangles at a 1×1 lot. Babylon tests verify six render batches, all pickable metadata, quarter-turn orientation, surface winding/normal agreement and upward-facing selection geometry. Physical mobile performance and actual WebGL appearance need runtime review.

The first CPU contact sheet exposed painter-order artifacts in long wall triangles. The renderer was corrected to interpolate depth per pixel; no geometry score was fabricated from the flawed image.

## Bounded corrective review, 2026-09-09

The first rendered sheet was rejected by the parent reviewer: twelve variants shared an office-box silhouette and differed mainly in floor count and rooftop equipment. `architecture-review-v1-rejected.png` preserves that evidence. The correction replaced whole massing systems with the twelve forms described above, retaining runtime batches, card occupancy, rotation and pick links. No prior image or rejection was removed.

The parent inspected the second sheet and accepted the distinct silhouettes for **stylized architecture scope only**, explicitly excluding WebGL and fidelity gates. A final requested landmark detail pass added stepped approach, facade pilasters and horizontal friezes to the Wonder. `architecture-review-v2.png` and the current `architecture-review.png` show the corrected assets. The tests preserve the original winding threshold; for smooth curved surfaces they compare each face against the average of its three shading normals, rather than incorrectly requiring every individual smooth normal to equal a flat face normal.
