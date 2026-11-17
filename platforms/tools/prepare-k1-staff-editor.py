"""Create a separate Blender authoring scene; never deploy or overwrite edits.

Run with installed Blender --background --factory-startup --python-exit-code 1
--python platforms/tools/prepare-k1-staff-editor.py. Uses existing local readers.
Owned resources, packed textures, scene and reports stay under ignored out/.
"""
from pathlib import Path
import hashlib
import importlib.util
import json
import math
import shutil
import subprocess
import sys

import bpy
from mathutils import Euler, Matrix, Quaternion, Vector

repo = Path(__file__).resolve().parents[2]
out = repo / 'platforms/out'
engage = out / 'k1-engage-prototype'
dest = engage / 'staff-editor'
blend = dest / 'kotor_staff_blocks.blend'
subprocess.run(['git', '-C', str(repo), 'check-ignore', '--quiet', str(blend)], check=True)
assert not blend.exists(), 'Refusing to overwrite an editable scene; preserve or rename it first.'
dest.mkdir(exist_ok=True)
source = dest / 'source'
source.mkdir(exist_ok=True)
profile = engage / 'elbow-arena/profile'
sys.path.insert(0, str(out / 'kotor-animation-import/tools'))
sys.path.insert(0, str(repo / 'tools'))
sys.path.insert(0, str(repo / 'platforms/tools'))
import io_scene_kotor
from io_scene_kotor.constants import ImportOptions
from io_scene_kotor.format.mdl.reader import MdlReader
from io_scene_kotor.scene import armature
from padglyphs import erf_resource

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def nodes(node):
    return [node] + [n for child in node.children for n in nodes(child)]

def matrix_error(a, b):
    return max(abs(x-y) for aa, bb in zip(a, b) for x, y in zip(aa, bb))

def verify_native_blocks(rig, body, scene):
    """Independent of the importer's object animation conversion."""
    maximum = 0.
    comparisons = 0
    for action in bpy.data.actions:
        clip_name = action.get('source_clip', '')
        if not clip_name.startswith('j_block_'):
            continue
        clip = next(a for a in body.animations if a.name == clip_name)
        animated = {n.name: n for n in nodes(clip.root_node)}
        times = sorted({row[0] for n in animated.values() for row in n.keyframes.get('orientation', [])})
        rig.animation_data.action = action
        for t in times:
            scene.frame_set(round(1+t*scene.render.fps))
            world = {}
            for n in nodes(body.root_node):
                p, q = Vector(n.position), Quaternion(n.orientation)
                controllers = animated[n.name].keyframes if n.name in animated else {}
                if controllers.get('position'):
                    row = min(controllers['position'], key=lambda row: abs(row[0]-t))
                    assert abs(row[0]-t) < .000001
                    p += Vector(row[1:4])
                if controllers.get('orientation'):
                    row = min(controllers['orientation'], key=lambda row: abs(row[0]-t))
                    assert abs(row[0]-t) < .000001
                    q = Quaternion((row[4], *row[1:4])).normalized()
                local = Matrix.LocRotScale(p, q, Vector((n.scale,)*3))
                world[n.name] = world[n.parent.name] @ local if n.parent else local
                if n.name in rig.pose.bones:
                    maximum = max(maximum, matrix_error(world[n.name], rig.pose.bones[n.name].matrix))
                    comparisons += 1
    assert maximum < .00001, ('Native key/Blender Action mismatch', maximum)
    return {'comparisons': comparisons, 'max_matrix_error': maximum}

tracked_inputs = [profile / 'override/k1jastaff.mdl', profile / 'override/k1jastaff.mdx',
                  profile / 'staff-connected-poses.txt', profile / 'staff-native-bind.txt']
before = {str(p.relative_to(repo)): digest(p) for p in tracked_inputs}
for p in tracked_inputs[:2]:
    shutil.copy2(p, source / p.name)

# Reuse the existing read-only KEY/BIF reader; no installation or add-on setup.
spec = importlib.util.spec_from_file_location('owned_k1', engage / 'dark-jedi/inspect-owned.py')
owned = importlib.util.module_from_spec(spec)
spec.loader.exec_module(owned)
resources = []
def extract(name, kind, extension):
    data = owned.resource(name, kind)
    p = source / (name + '.' + extension)
    p.write_bytes(data)
    resources.append({'resource': p.name, 'sha256': digest(p)})
    return p

weapon_path = extract('w_dblsbr_001', 2002, 'mdl')
extract('w_dblsbr_001', 3008, 'mdx')
appearance = owned.twoda(owned.resource('appearance', 2017))
heads = owned.twoda(owned.resource('heads', 2017))
# Established isolated player's saved appearance, resolved in the existing
# dismemberment head investigation; native heads.2da resolves the actual model.
head_name = heads[appearance['168']['normalhead']]['head'].lower()
head_path = extract(head_name, 2002, 'mdl')
extract(head_name, 3008, 'mdx')
body = MdlReader(str(source / 'k1jastaff.mdl')).load()
weapon = MdlReader(str(weapon_path)).load()
head = MdlReader(str(head_path)).load()
textures = set()
for model in (body, weapon, head):
    for node in nodes(model.root_node):
        for attr in ('bitmap', 'bitmap2'):
            name = getattr(node, attr, '').lower()
            if name and name != 'null':
                textures.add(name)
for name in sorted(textures):
    data = erf_resource(str(owned.game / 'TexturePacks/swpc_tex_tpa.erf'), name, 3007)
    assert data is not None, ('Missing owned texture', name)
    p = source / (name + '.tpc')
    p.write_bytes(data)
    resources.append({'resource': p.name, 'sha256': digest(p)})

bpy.ops.wm.read_factory_settings(use_empty=True)
io_scene_kotor.register()
# Existing importer's single-slot compatibility bridge, local to this process.
def single_slot_curves(action):
    slot = action.slots[0] if action.slots else action.slots.new(id_type='OBJECT', name=action.name)
    layer = action.layers[0] if action.layers else action.layers.new('Imported animation')
    strip = layer.strips[0] if layer.strips else layer.strips.new(type='KEYFRAME')
    return strip.channelbag(slot, ensure=True).fcurves
if not hasattr(bpy.types.Action, 'fcurves'):
    bpy.types.Action.fcurves = property(single_slot_curves)

options = ImportOptions()
options.import_animations = False
options.import_walkmeshes = False
options.build_armature = False
options.build_materials = True
options.texture_search_paths = [str(source)]
options.lightmap_search_paths = []
scene = bpy.context.scene
scene.render.fps = 120
scene.render.fps_base = 1
# The importer converts seconds with its own30fps constant, independently of
# scene FPS. Match it locally before importing keys, avoiding rounding7 native
# keys into4 frames and accidentally previewing the return-to-rest padding.
from io_scene_kotor import utils as kotor_utils
kotor_utils.ANIM_FPS = scene.render.fps
character = bpy.data.collections.new('01 KotOR character - editable rig')
scene.collection.children.link(character)
root = body.add_to_collection(character, options)
rig = armature.rebuild_armature(root)
assert rig is not None
rig.name = 'KOTOR_Staff_Edit'
rig.data.name = rig.name
rig.animation_data_create()
rig.show_in_front = True
rig.data.display_type = 'OCTAHEDRAL'
skin = [obj for obj in character.objects if obj.type == 'MESH' and
        any(m.type == 'ARMATURE' and m.object == rig for m in obj.modifiers)]
assert skin
for obj in list(character.objects):
    if obj not in skin and obj != rig:
        obj.hide_set(True)
        obj.hide_render = True
for bone in rig.data.bones:
    bone.color.palette = 'THEME04' if bone.name.startswith(('l', 'L')) else 'THEME03'
for bone in rig.pose.bones:
    bone.rotation_mode = 'QUATERNION'
# The importer first creates1mm bones. Reconstructing their axes from float
# head/tail positions loses enough precision to move collar-weighted skin0.46mm.
# Use longer display bones and reapply the original bind matrices before baking.
bpy.context.view_layer.objects.active = rig
bpy.ops.object.mode_set(mode='EDIT')
for bone in rig.data.edit_bones:
    bone.length = .1 if any(s in bone.name.lower() for s in ('bicep', 'forearm', 'torso', 'collar', 'thigh', 'shin')) else .05
    bone.matrix = bpy.data.objects[bone.name].matrix_world
bpy.ops.object.mode_set(mode='OBJECT')

ordered = list(rig.pose.bones)
assert all(b.parent is None or ordered.index(b.parent) < i for i, b in enumerate(ordered))
expected = {}
actions = []
def bake_action(label, poses, clip, duration, provenance):
    action = bpy.data.actions.new(label)
    action.use_fake_user = True
    action['source_clip'] = clip
    action['duration_seconds'] = duration
    action['source_space'] = provenance
    action.use_frame_range = True
    action.frame_start = 1
    action.frame_end = 13
    rig.animation_data.action = action
    expected[label] = {}
    for at, targets in poses:
        scene.frame_set(at)
        for bone in ordered:
            bone.matrix = targets[bone.name]
            bpy.context.view_layer.update()
        for bone in ordered:
            bone.keyframe_insert('location', frame=at, group=bone.name)
            bone.keyframe_insert('rotation_quaternion', frame=at, group=bone.name)
            bone.keyframe_insert('scale', frame=at, group=bone.name)
        expected[label][str(at)] = {name: [list(row) for row in mat] for name, mat in targets.items()}
    for curve in single_slot_curves(action):
        for key in curve.keyframe_points:
            key.interpolation = 'LINEAR'
    actions.append({'action': label, 'source_clip': clip, 'duration': duration, 'samples': len(poses)})
    return action

# Side-held KotOR guard captured after settling, zero residual leg yaw. This is
# the runtime-selected guard, rather than the unused raw JKA staff-ready clip.
guard_local = {}
for line in tracked_inputs[2].read_text().splitlines():
    f = line.split()
    if int(f[0]) == 109 and int(f[1]) == 25:
        guard_local[f[6]] = Matrix.LocRotScale(Vector(tuple(map(float, f[7:10]))),
            Quaternion((float(f[13]), *map(float, f[10:13]))), Vector((1, 1, 1)))
body_nodes = nodes(body.root_node)
assert len(guard_local) == len(body_nodes)
guard_world = {}
for node in body_nodes:
    parent = node.parent.name if node.parent else None
    guard_world[node.name] = guard_world[parent] @ guard_local[node.name] if parent else guard_local[node.name]
guard_action = bake_action('00 Guard - KotOR side stance', [(1, guard_world), (13, guard_world)],
                          'native g3r1', .1, 'settled native pose: state109 frame25, residual yaw0')

# Use the importer's native controller conversion, then bake model-space matrices
# to the rig (its older direct clavicle-to-armature conversion is inaccurate).
block_labels = [('up', '01 Block - Overhead'), ('ur', '02 Block - Upper Right'),
                ('ul', '03 Block - Upper Left'), ('lr', '04 Block - Lower Right'),
                ('ll', '05 Block - Lower Left')]
for suffix, label in block_labels:
    clip = next(a for a in body.animations if a.name == 'j_block_' + suffix)
    clip.add_to_objects(root, 1.)
    imported = root.kb.anim_list[-1]
    poses = []
    for step in range(13):
        scene.frame_set(imported.frame_start + step)
        bpy.context.view_layer.update()
        poses.append((step+1, {bone.name: bpy.data.objects[bone.name].matrix_world.copy() for bone in ordered}))
    bake_action(label, poses, clip.name, clip.length, 'native asset base clip; live aim/Motion/grip transitions excluded')
for obj in list(character.objects):
    if obj != rig:
        obj.animation_data_clear()
scene.frame_set(0)

attachments = bpy.data.collections.new('02 Saber and head - follow rig')
scene.collection.children.link(attachments)
def attach(label, model, bone):
    obj = model.add_to_collection(attachments, options)
    anchor = bpy.data.objects.new(label, None)
    attachments.objects.link(anchor)
    con = anchor.constraints.new('COPY_TRANSFORMS')
    con.target = rig
    con.subtarget = bone
    obj.parent = anchor
    obj.matrix_parent_inverse = Matrix.Identity(4)
    anchor.hide_set(True)
    return obj, anchor

head_root, head_anchor = attach('Head attachment - headhook', head, 'headhook')
weapon_root, weapon_anchor = attach('Saber attachment - rhand', weapon, 'rhand')
for model_root in (head_root, weapon_root):
    for obj in [model_root] + list(model_root.children_recursive):
        if obj.type != 'MESH' or not obj.kb.render or (model_root == weapon_root and obj.kb.meshtype == 'LIGHTSABER'):
            obj.hide_set(True)
            obj.hide_render = True

# Billboard blade meshes need the game renderer. Preview their actual measured
# powered centerlines instead; never export these display-only cylinders.
beams = []
for line in (profile / 'staff-weapon-geometry.txt').read_text().splitlines():
    _, *values = line.split()
    a, b = Vector(tuple(map(float, values[:3]))), Vector(tuple(map(float, values[3:])))
    beams.append((a, b))
assert len(beams) == 2
def blade_material(name, color):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = (*color, 1)
    mat.use_nodes = True
    shader = mat.node_tree.nodes.get('Principled BSDF')
    shader.inputs['Base Color'].default_value = (*color, 1)
    shader.inputs['Emission Color'].default_value = (*color, 1)
    shader.inputs['Emission Strength'].default_value = 2
    return mat
blue = blade_material('Preview blade - blue', (.015, .13, 1.))
white = blade_material('Preview blade - white core', (.8, .92, 1.))
blade_objects = []
for index, (a, b) in enumerate(beams):
    for radius, mat, kind in ((.012, blue, 'blue'), (.005, white, 'core')):
        bpy.ops.mesh.primitive_cylinder_add(vertices=16, radius=radius, depth=(b-a).length)
        obj = bpy.context.object
        obj.name = f'Preview only - blade{index+1} {kind}'
        for coll in list(obj.users_collection):
            coll.objects.unlink(obj)
        attachments.objects.link(obj)
        obj.parent = weapon_anchor
        obj.location = (a+b)*.5
        obj.rotation_mode = 'QUATERNION'
        obj.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(b-a)
        obj.data.materials.append(mat)
        obj['export'] = False
        blade_objects.append(obj)

for image in bpy.data.images:
    if image.source == 'FILE' and image.has_data and not image.packed_file:
        image.pack()

report = {'blender': bpy.app.version_string, 'input_hashes': before, 'resources': resources,
          'rig': rig.name, 'bones': len(ordered), 'skin_meshes': [o.name for o in skin],
          'head': head_name, 'actions': actions, 'expected_bones': expected,
          'limitations': 'Base block clips only; live aim/Motion/grip smoothing excluded. No game export or automatic installation.'}
maximum = 0.
for label, frames in expected.items():
    rig.animation_data.action = bpy.data.actions[label]
    for f, targets in frames.items():
        scene.frame_set(int(f))
        bpy.context.view_layer.update()
        for name, matrix in targets.items():
            if name in rig.pose.bones:
                maximum = max(maximum, matrix_error(rig.pose.bones[name].matrix, Matrix(matrix)))
assert maximum < .00001, ('Native node/rig mismatch', maximum)
report['max_native_node_to_bone_matrix_error'] = maximum
report['native_key_comparison'] = verify_native_blocks(rig, body, scene)

rig.animation_data.action = guard_action
scene.frame_start = 1
scene.frame_end = 13
scene.frame_set(1)
rig['Instructions'] = 'Choose one of six Actions in the bottom Action Editor. Pose Mode; rotate bones and insert keyframes. Save As to preserve edits.'
rig['Game export'] = 'Not connected. Do not overwrite profile models. Preview blades are display-only.'
readme = '''KOTOR STAFF BLOCK EDITOR

The selected armature is KOTOR_Staff_Edit. The bottom Action Editor's Action
dropdown selects00 Guard and01..05 directional blocks (defender's viewpoint).
All six are protected with Fake User. Blocks are0.1 seconds, frames1..13 at120fps.
Scrub the timeline to inspect them. Tab toggles Object/Pose Mode; select a bone,
R rotates it, and insert Rotation keyframes (I menu). Save a new .blend copy.
The imported base block clips exclude live Motion/aim/grip/transition corrections;
the guard is the settled KotOR side-held runtime pose. This is an editing scene,
not proof that a revised pose will match live gameplay without integration.
Head and hilt follow the actual headhook/rhand bones. Blue/core cylinders mark
the two measured native powered blades; they are preview-only, never game assets.
Textures are packed. No add-on install or script execution is required to edit.
No game export is connected yet. The normal generator never writes this file.
Do not export over game/profile assets; keep your edited blend for integration.
'''
bpy.data.texts.new('START HERE - Staff block editing').write(readme)
(dest / 'README.txt').write_text(readme, encoding='utf-8')

# Set a useful opening view and make selecting an Action an ordinary UI operation.
for screen in bpy.data.screens:
    for area in screen.areas:
        if area.type == 'VIEW_3D':
            space = area.spaces.active
            space.region_3d.view_location = Vector((0, 0, 1.05))
            space.region_3d.view_distance = 3.8
            space.region_3d.view_rotation = Euler((math.radians(78), 0, math.radians(160)), 'XYZ').to_quaternion()
            space.shading.type = 'MATERIAL'
            space.overlay.show_extras = False
        elif area.type == 'DOPESHEET_EDITOR' and area.spaces.active.mode == 'TIMELINE':
            area.spaces.active.mode = 'ACTION'
            area.spaces.active.dopesheet.show_only_selected = False
helpers = rig.data.collections.new('Helpers - advanced')
controls = rig.data.collections.new('Main pose controls')
for bone in rig.data.bones:
    main = bone.name in {'pelvis_g', 'torso_g', 'torsoMid_g', 'torsoUpr_g', 'lcollar_g', 'rcollar_g',
                        'lbicep_g', 'rbicep_g', 'Lforearm_g', 'Rforearm_g', 'Lhand_g', 'Rhand_g',
                        'lthigh_g', 'rthigh_g', 'lshin_g', 'rshin_g'}
    (controls if main else helpers).assign(bone)
    for coll in list(bone.collections):
        if coll not in (controls, helpers):
            coll.unassign(bone)
helpers.is_visible = False
for obj in bpy.context.selected_objects:
    obj.select_set(False)
rig.hide_set(False)
rig.select_set(True)
bpy.context.view_layer.objects.active = rig
bpy.ops.object.mode_set(mode='POSE')
if hasattr(bpy.context.preferences.filepaths, 'file_preview_type'):
    bpy.context.preferences.filepaths.file_preview_type = 'NONE'
bpy.context.preferences.filepaths.save_version = 0
bpy.ops.wm.save_as_mainfile(filepath=str(blend), check_existing=False)
assert {str(p.relative_to(repo)): digest(p) for p in tracked_inputs} == before
report['blend_sha256'] = digest(blend)
(dest / 'scene-validation.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print('STAFF_EDITOR_SAVED', json.dumps({k: v for k, v in report.items() if k not in ('expected_bones', 'resources')}))
