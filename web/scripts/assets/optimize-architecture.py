"""Run glTF Transform without changing silhouette, named parts, or decoder requirements."""
import json, subprocess, sys
from pathlib import Path
root = Path(sys.argv[1] if len(sys.argv) > 1 else 'docs/art-assets/models')
cli = sys.argv[2] if len(sys.argv) > 2 else 'gltf-transform'
manifest = json.loads((root / 'manifest.json').read_text())
for entry in manifest:
    source = root / entry['file']; temporary = source.with_suffix('.optimized.glb')
    subprocess.run([cli, 'optimize', str(source), str(temporary), '--compress', 'false', '--simplify', 'false', '--palette', 'false', '--join', 'false', '--flatten', 'false', '--instance', 'false'], check=True, stdout=subprocess.DEVNULL)
    before = source.stat().st_size
    temporary.replace(source)
    entry.update(bytes=source.stat().st_size, unoptimizedBytes=before, optimization='glTF Transform 4.5.0: dedup, prune, weld; no lossy simplification or compression')
(root / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
print(f'Optimized {len(manifest)} assets: {sum(e["unoptimizedBytes"] for e in manifest):,} -> {sum(e["bytes"] for e in manifest):,} bytes')
