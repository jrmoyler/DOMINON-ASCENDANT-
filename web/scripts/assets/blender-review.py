"""Blender 4.5 source assembly, geometry cleanup, GLB export and contact-sheet review.
Run after export-architecture.ts:
  blender -b --python web/scripts/assets/blender-review.py -- docs/art-assets
"""
import bpy, bmesh, json, math, sys
from pathlib import Path
from mathutils import Vector

root = Path(sys.argv[sys.argv.index('--') + 1]).resolve()
source = root / 'models'
output = root / 'blender'
output.mkdir(parents=True, exist_ok=True)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
manifest = json.loads((source / 'manifest.json').read_text())
report = []
for i, entry in enumerate(manifest):
    bpy.ops.object.select_all(action='DESELECT')
    bpy.ops.import_scene.gltf(filepath=str(source / entry['file']))
    imported = list(bpy.context.selected_objects)
    for obj in imported:
        if obj.type != 'MESH':
            continue
        bm = bmesh.new(); bm.from_mesh(obj.data)
        bmesh.ops.remove_doubles(bm, verts=list(bm.verts), dist=0.00001)
        bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
        bm.to_mesh(obj.data); bm.free(); obj.data.update()
    bpy.ops.export_scene.gltf(filepath=str(output / entry['file']), export_format='GLB', use_selection=True, export_apply=True)
    report.append({'family': entry['family'], 'objects': len(imported), 'meshObjects': sum(o.type == 'MESH' for o in imported), 'cleanup': 'weld 0.00001m and recalculate exterior normals', 'bytes': (output / entry['file']).stat().st_size})
    # Translate only hierarchy roots so each model stays assembled.
    for obj in imported:
        if obj.parent not in imported:
            obj.location.x += (i % 4 - 1.5) * 24
            obj.location.y += (i // 4 - 1) * 27

scene = bpy.context.scene
scene.render.engine = 'CYCLES'
scene.cycles.samples = 24
scene.cycles.use_denoising = True
scene.world.color = (0.38, 0.38, 0.38)
bpy.ops.mesh.primitive_plane_add(size=240, location=(0, 0, -0.035))
plane = bpy.context.object; plane.name = 'review-ground'
mat = bpy.data.materials.new('warm-stone-review-ground'); mat.diffuse_color = (0.49, 0.46, 0.40, 1); plane.data.materials.append(mat)
bpy.ops.object.light_add(type='AREA', location=(15, -45, 85))
light = bpy.context.object; light.data.energy = 75000; light.data.shape = 'DISK'; light.data.size = 75
light.rotation_euler = (Vector((0, 0, 0)) - light.location).to_track_quat('-Z', 'Y').to_euler()
bpy.ops.object.camera_add(location=(98, -130, 100))
camera = bpy.context.object; camera.rotation_euler = (Vector((0, 0, 5)) - camera.location).to_track_quat('-Z', 'Y').to_euler()
camera.data.type = 'ORTHO'; camera.data.ortho_scale = 155; scene.camera = camera
scene.render.resolution_x = 1600; scene.render.resolution_y = 1100; scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
scene.render.filepath = str(output / 'architecture-review.png')
scene.view_settings.view_transform = 'AgX'
bpy.ops.wm.save_as_mainfile(filepath=str(output / 'architecture-library.blend'))
(root / 'blender-report.json').write_text(json.dumps({'blender': bpy.app.version_string, 'referenceStatus': 'No reference images supplied or committed; original authored geometry, no likeness score', 'assets': report}, indent=2) + '\n')
bpy.ops.render.render(write_still=True)
