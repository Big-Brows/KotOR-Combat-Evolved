"""Extend a copy of the staff Blender scene with seven editable base attacks.

Run using installed Blender in background mode. Never overwrites either editor
or deploys animations. Reuses the existing scene, rig, materials and attachments.
"""
from pathlib import Path
import hashlib
import json
import math
import subprocess
import sys

import bpy
from mathutils import Matrix, Quaternion, Vector

repo = Path(__file__).resolve().parents[2]
editor = repo / 'platforms/out/k1-engage-prototype/staff-editor'
destination = editor / 'attacks'
source_blend = editor / 'kotor_staff_blocks.blend'
output = destination / 'kotor_staff_attacks.blend'
profile = editor.parent / 'elbow-arena/profile'
subprocess.run(['git', '-C', str(repo), 'check-ignore', '--quiet', str(output)], check=True)
assert not output.exists(), 'Refusing to overwrite an editable scene.'
destination.mkdir(exist_ok=True)
sys.path.insert(0, str(repo / 'platforms/out/kotor-animation-import/tools'))
from io_scene_kotor.format.mdl.reader import MdlReader

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def nodes(node):
    return [node] + [n for child in node.children for n in nodes(child)]

def curves(action):
    return [curve for layer in action.layers for strip in layer.strips
            for bag in strip.channelbags for curve in bag.fcurves]

def action_digest(action):
    keys = [(curve.data_path, curve.array_index,
             [(list(k.co), list(k.handle_left), list(k.handle_right), k.interpolation)
              for k in curve.keyframe_points]) for curve in curves(action)]
    return hashlib.sha256(json.dumps(keys, sort_keys=True).encode()).hexdigest()

def matrix_error(a, b):
    return max(abs(x-y) for aa, bb in zip(a, b) for x, y in zip(aa, bb))

inputs = [source_blend, profile / 'override/k1jastaff.mdl', profile / 'override/k1jastaff.mdx']
before = {str(p.relative_to(repo)): digest(p) for p in inputs}
body = MdlReader(str(inputs[1])).load()
body_nodes = nodes(body.root_node)
bind = {n.name: Matrix.LocRotScale(Vector(n.position), Quaternion(n.orientation), Vector((n.scale,)*3))
        for n in body_nodes}
bpy.ops.wm.open_mainfile(filepath=str(source_blend), use_scripts=False, load_ui=True)
scene = bpy.context.scene
rig = bpy.data.objects['KOTOR_Staff_Edit']
assert matrix_error(rig.matrix_world, Matrix.Identity(4)) < .00001
scene.render.fps = 120
scene.render.fps_base = 1
inherited = {a.name: action_digest(a) for a in bpy.data.actions}
ordered = list(rig.pose.bones)
clips = [('top', 'W'), ('trbl', 'W+A'), ('tlbr', 'W+D'), ('rl', 'A'),
         ('lr', 'D'), ('brtl', 'S+A'), ('bltr', 'S+D')]
manifest = []
expected = {}
for index, (suffix, direction) in enumerate(clips, 10):
    clip = next(a for a in body.animations if a.name == 's_' + suffix)
    animated = {n.name: n for n in nodes(clip.root_node)}
    times = sorted({float(row[0]) for n in animated.values() for row in n.keyframes.get('orientation', [])})
    label = f'{index:02d} Attack - {direction}'
    assert label not in bpy.data.actions, ('An attack with this name already exists; preserve edits', label)
    action = bpy.data.actions.new(label)
    action.use_fake_user = True
    action.use_frame_range = True
    action.frame_start = 1
    action.frame_end = round(1+clip.length*120)
    action['source_clip'] = clip.name
    action['direction'] = direction
    action['duration_seconds'] = float(clip.length)
    action['source_space'] = 'native base full-body clip; no live aim/Motion/grip correction'
    rig.animation_data.action = action
    start, end = (.4, .7) if suffix == 'top' else (.25, .55)
    for name, t in (('Windup', 0), ('Strike', start), ('Recovery', end), ('End', clip.length)):
        marker = action.pose_markers.new(name)
        marker.frame = round(1+t*120)
    expected[label] = {}
    previous_rotations = {}
    for t in times:
        frame = round(1+t*120)
        assert abs((frame-1)/120-t) < .000001
        world = {}
        for node in body_nodes:
            local = bind[node.name].copy()
            p, q, scale = local.decompose()
            controllers = animated[node.name].keyframes if node.name in animated else {}
            if controllers.get('position'):
                row = min(controllers['position'], key=lambda row: abs(row[0]-t))
                assert abs(row[0]-t) < .000001
                p += Vector(row[1:4])
            if controllers.get('orientation'):
                row = min(controllers['orientation'], key=lambda row: abs(row[0]-t))
                assert abs(row[0]-t) < .000001
                q = Quaternion((row[4], *row[1:4])).normalized()
            local = Matrix.LocRotScale(p, q, scale)
            world[node.name] = world[node.parent.name] @ local if node.parent else local
        scene.frame_set(frame)
        for bone in ordered:
            bone.matrix = world[bone.name]
            bpy.context.view_layer.update()
        for bone in ordered:
            # q and -q encode the same pose, but Blender's component curves
            # interpolate through zero if decomposition switches signs.
            # Native keys matched while29 such pairs failed midframe playback.
            q = bone.rotation_quaternion.copy()
            if bone.name in previous_rotations and previous_rotations[bone.name].dot(q) < 0:
                q.negate()
            bone.rotation_quaternion = q
            previous_rotations[bone.name] = q.copy()
            bone.keyframe_insert('location', frame=frame, group=bone.name)
            bone.keyframe_insert('rotation_quaternion', frame=frame, group=bone.name)
            bone.keyframe_insert('scale', frame=frame, group=bone.name)
        expected[label][str(frame)] = {bone.name: [list(r) for r in world[bone.name]] for bone in ordered}
    for curve in curves(action):
        for point in curve.keyframe_points:
            point.interpolation = 'LINEAR'
    manifest.append({'action': label, 'clip': clip.name, 'frames': [1, int(action.frame_end)],
                     'duration': float(clip.length), 'samples': len(times),
                     'strike_frames': [round(1+start*120), round(1+end*120)]})
    print('STAFF_ATTACK_BAKED', label, len(times), flush=True)

maximum = 0.
count = 0
for name, frames in expected.items():
    rig.animation_data.action = bpy.data.actions[name]
    for frame, targets in frames.items():
        scene.frame_set(int(frame))
        for bone, matrix in targets.items():
            maximum = max(maximum, matrix_error(rig.pose.bones[bone].matrix, Matrix(matrix)))
            count += 1
assert maximum < .00001, ('Native attack keys changed during Blender bake', maximum)
assert all(action_digest(bpy.data.actions[name]) == h for name, h in inherited.items())
assert all(image.packed_file for image in bpy.data.images if image.source == 'FILE' and image.has_data)
for name in ('10 Attack - W', '11 Attack - W+A', '12 Attack - W+D', '13 Attack - A',
             '14 Attack - D', '15 Attack - S+A', '16 Attack - S+D'):
    assert bpy.data.actions[name].use_fake_user

instructions = '''STAFF ATTACK EDITOR

Choose10..16 Attack in the bottom Action Editor dropdown. Existing00 Guard and
01..05 blocks are retained. These are the current full native staff clips,
including their present tuning/issues, not new attack corrections.

W: frames1..115 (windup1..49; strike49..85; recovery85..115).
W+A, W+D, A, D, S+A, S+D: frames1..109
(windup1..31; strike31..67; recovery67..109).
The scene is120fps. Set the timeline End to115 for W or109 for the others;
guard/blocks use13. Action pose markers label Windup/Strike/Recovery/End.

The selected KOTOR_Staff_Edit rig opens in Pose Mode. Select a bone, R rotates,
I inserts keyframes. Save As a separate working copy to preserve your changes.
Head/hilt follow headhook/rhand; blades are display-only native centerline guides.
Live Motion/aim/grip/transition corrections are not included in these base clips.
Export into the game is not connected. Do not overwrite game/profile models.
No installed add-on or script execution is required. Textures are packed.
This file is separate from kotor_staff_blocks.blend; that source is untouched.
'''
bpy.data.texts.new('START HERE - Staff attack editing').write(instructions)
(destination / 'README.txt').write_text(instructions, encoding='utf-8')
rig['Instructions'] = 'Choose10..16 Attack in the Action Editor. W ends115; others109;120fps. See START HERE - Staff attack editing.'
rig.animation_data.action = bpy.data.actions['10 Attack - W']
scene.frame_start = 1
scene.frame_end = 115
scene.frame_set(1)
for obj in bpy.context.selected_objects:
    obj.select_set(False)
rig.hide_set(False)
rig.select_set(True)
bpy.context.view_layer.objects.active = rig
if rig.mode != 'POSE':
    bpy.ops.object.mode_set(mode='POSE')
bpy.context.preferences.filepaths.save_version = 0
if hasattr(bpy.context.preferences.filepaths, 'file_preview_type'):
    bpy.context.preferences.filepaths.file_preview_type = 'NONE'
bpy.ops.wm.save_as_mainfile(filepath=str(output), check_existing=False)
assert all(digest(p) == before[str(p.relative_to(repo))] for p in inputs)
report = {'inputs': before, 'source_action_hashes': inherited, 'attacks': manifest,
          'native_key_comparisons': count, 'max_native_key_matrix_error': maximum,
          'expected': expected, 'output_sha256': digest(output), 'blender': bpy.app.version_string,
          'scope': 'editable base staff clips only; no runtime export or retuning'}
(destination / 'creation-validation.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print('STAFF_ATTACK_EDITOR_SAVED', json.dumps({k: v for k, v in report.items() if k != 'expected'}))
