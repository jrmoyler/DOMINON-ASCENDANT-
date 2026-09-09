"""Deterministic CPU projection of actual GLB triangles. This is NOT WebGL/Blender evidence."""
import json, struct, sys
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

root = Path(sys.argv[1] if len(sys.argv) > 1 else 'docs/art-assets')
manifest = json.loads((root / 'models/manifest.json').read_text())
fig, axes = plt.subplots(3, 4, figsize=(16, 13.5), facecolor='#e4dfd3')
eye = np.array([1.15, .85, -1.4]); eye /= np.linalg.norm(eye)
right = np.cross([0., 1., 0.], eye); right /= np.linalg.norm(right)
up = np.cross(eye, right)
light = np.array([-.45, .9, -.55]); light /= np.linalg.norm(light)
for ax, entry in zip(axes.flat, manifest):
    raw = (root / 'models' / entry['file']).read_bytes()
    jsize = struct.unpack_from('<I', raw, 12)[0]
    doc = json.loads(raw[20:20 + jsize])
    binary = raw[28 + jsize:]
    def accessor(i):
        a = doc['accessors'][i]; view = doc['bufferViews'][a['bufferView']]
        types = {5126: '<f4', 5125: '<u4', 5123: '<u2', 5121: '<u1'}
        n = {'VEC3': 3, 'VEC2': 2, 'VEC4': 4, 'SCALAR': 1}[a['type']]
        dt = np.dtype(types[a['componentType']]); stride = view.get('byteStride', n * dt.itemsize)
        return np.ndarray((a['count'], n), dtype=dt, buffer=binary, offset=view.get('byteOffset', 0) + a.get('byteOffset', 0), strides=(stride, dt.itemsize))
    polygons, colors, depths = [], [], []
    for mesh in doc['meshes']:
        for primitive in mesh['primitives']:
            p = accessor(primitive['attributes']['POSITION'])
            indices = accessor(primitive['indices']).flatten() if 'indices' in primitive else np.arange(len(p))
            triangles = p[indices].reshape(-1, 3, 3)
            normals = np.cross(triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0])
            lengths = np.linalg.norm(normals, axis=1)
            normals /= np.maximum(lengths[:, None], 1e-9)
            visible = normals @ eye > 0
            triangles = triangles[visible]; normals = normals[visible]
            material = doc['materials'][primitive.get('material', 0)]
            linear = np.array(material.get('pbrMetallicRoughness', {}).get('baseColorFactor', [.6,.6,.6,1])[:3])
            base = np.where(linear <= .0031308, 12.92 * linear, 1.055 * linear ** (1 / 2.4) - .055)
            shades = .58 + .42 * np.maximum(0, normals @ light)
            for triangle, shade in zip(triangles, shades):
                polygons.append(np.column_stack((triangle @ right, triangle @ up)))
                colors.append(np.clip(base * shade, 0, 1))
                depths.append(triangle @ eye)
    # Orthographic triangle rasterizer with per-pixel depth. Painter sorting is
    # insufficient for long wall triangles with inset windows and roof details.
    resolution = 500
    pixels = np.empty((650, resolution, 3)); pixels[:] = np.array([228, 223, 211]) / 255
    zbuffer = np.full((650, resolution), -np.inf)
    for polygon, color, zs in zip(polygons, colors, depths):
        q = polygon.copy(); q[:, 0] = (q[:, 0] + 15) / 30 * resolution; q[:, 1] = (32 - q[:, 1]) / 39 * 650
        x0, x1 = max(0, int(np.floor(q[:, 0].min()))), min(resolution - 1, int(np.ceil(q[:, 0].max())))
        y0, y1 = max(0, int(np.floor(q[:, 1].min()))), min(649, int(np.ceil(q[:, 1].max())))
        if x0 > x1 or y0 > y1: continue
        xx, yy = np.meshgrid(np.arange(x0, x1 + 1) + .5, np.arange(y0, y1 + 1) + .5)
        a, b, c = q
        denominator = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1])
        if abs(denominator) < 1e-8: continue
        u = ((b[1] - c[1]) * (xx - c[0]) + (c[0] - b[0]) * (yy - c[1])) / denominator
        v = ((c[1] - a[1]) * (xx - c[0]) + (a[0] - c[0]) * (yy - c[1])) / denominator
        wgt = 1 - u - v
        depth = u * zs[0] + v * zs[1] + wgt * zs[2]
        region = zbuffer[y0:y1 + 1, x0:x1 + 1]
        mask = (u >= -1e-6) & (v >= -1e-6) & (wgt >= -1e-6) & (depth > region)
        region[mask] = depth[mask]
        pixels[y0:y1 + 1, x0:x1 + 1][mask] = color
    ax.imshow(pixels, extent=(-15, 15, -7, 32), interpolation='bilinear')
    ax.set(xlim=(-15,15), ylim=(-7,32), aspect='equal', facecolor='#e4dfd3')
    ax.axis('off')
    ax.set_title(entry['family'].upper(), fontsize=12, fontweight='bold', color='#303934', pad=-4)
    ax.text(.5, -.035, f"{entry['triangles']:,} triangles · 6 runtime finishes", transform=ax.transAxes, ha='center', color='#5b655d', fontsize=9)
fig.suptitle('DOMINION / ARCHITECTURAL LIBRARY', fontsize=24, fontweight='bold', color='#303934', y=.98)
fig.text(.5, .027, 'Actual exported geometry • original authored design • CPU orthographic review, not a WebGL or Blender render', ha='center', fontsize=11, color='#5b655d')
fig.subplots_adjust(top=.92, bottom=.07, wspace=.06, hspace=.12)
fig.savefig(root / 'architecture-review.png', dpi=130, facecolor=fig.get_facecolor())
