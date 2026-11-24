"""Export the restored red idle guard for local Blender pose editing only."""
from pathlib import Path
import copy
import hashlib
import importlib.util
import json
import shutil
import subprocess
import sys
import bpy
from mathutils import Matrix

repo = Path(__file__).resolve().parents[2]
staff = '--staff-with-head' in sys.argv
out = repo / ('platforms/out/staff-guard-edit' if staff else 'platforms/out/red-guard-edit')
subprocess.run(['git', '-C', str(repo), 'check-ignore', '--quiet', str(out / 'source')], check=True)
source = out / 'source'
source.mkdir(parents=True, exist_ok=True)
target = out / ('kotor_staff_idle_guard_with_head.blend' if staff else 'red_idle_guard.blend')
assert not target.exists(), 'Preserve the existing editable file.'
for folder in ('platforms/out/kotor-animation-import/tools', 'tools', 'platforms/tools'):
    sys.path.insert(0, str(repo / folder))
import io_scene_kotor
from io_scene_kotor.constants import ImportOptions
from io_scene_kotor.format.mdl.reader import MdlReader
from io_scene_kotor.scene import armature
from padglyphs import erf_resource
spec = importlib.util.spec_from_file_location('owned', repo / 'platforms/out/k1-engage-prototype/dark-jedi/inspect-owned.py')
owned = importlib.util.module_from_spec(spec)
spec.loader.exec_module(owned)
profile = repo / 'platforms/out/k1-engage-prototype/elbow-arena/profile/override'
manifest = {}
def load(name, custom=False):
    for kind, ext in ((2002, 'mdl'), (3008, 'mdx')):
        p = source / (name + '.' + ext)
        if custom:
            shutil.copy2(profile / p.name, p)
        else:
            p.write_bytes(owned.resource(name, kind))
        manifest[p.name] = hashlib.sha256(p.read_bytes()).hexdigest()
    return MdlReader(str(source / (name + '.mdl'))).load()
def walk(n):
    yield n
    for child in n.children:
        yield from walk(child)
body = load('k1jastaff' if staff else 'k1jared', True)
weapon = load('w_dblsbr_001' if staff else 'w_lghtsbr_001')
head = None
if staff:
    appearance = owned.twoda(owned.resource('appearance', 2017))
    heads = owned.twoda(owned.resource('heads', 2017))
    head = load(heads[appearance['168']['normalhead']]['head'].lower())
clip = copy.deepcopy(next(a for a in body.animations if a.name.lower() == 'j_guard'))
for model in ([body, weapon, head] if head else [body, weapon]):
    for name in {getattr(n, 'bitmap', '').lower() for n in walk(model.root_node)} - {'', 'null'}:
        data = erf_resource(str(owned.game / 'TexturePacks/swpc_tex_tpa.erf'), name, 3007)
        if data:
            (source / (name + '.tpc')).write_bytes(data)
io_scene_kotor.register()
def curves(action):
    slot = action.slots[0] if action.slots else action.slots.new(id_type='OBJECT', name=action.name)
    layer = action.layers[0] if action.layers else action.layers.new('Imported animation')
    strip = layer.strips[0] if layer.strips else layer.strips.new(type='KEYFRAME')
    return strip.channelbag(slot, ensure=True).fcurves
if not hasattr(bpy.types.Action, 'fcurves'):
    bpy.types.Action.fcurves = property(curves)
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.render.fps = 30
options = ImportOptions()
options.import_animations = False
options.import_walkmeshes = False
options.build_armature = False
options.texture_search_paths = [str(source)]
options.lightmap_search_paths = []
collection = bpy.data.collections.new('KotOR body')
scene.collection.children.link(collection)
body.animations = []
root = body.add_to_collection(collection, options)
scene.frame_set(0)
bpy.context.view_layer.update()
bind = {o.name: o.matrix_basis.copy() for o in collection.objects}
rig = armature.rebuild_armature(root)
assert rig
rig.name = 'STAFF GUARD - edit in Pose Mode' if staff else 'RED GUARD - edit in Pose Mode'
clip.add_to_objects(root, 1.0)
scene.frame_set(root.kb.anim_list[-1].frame_start)
bpy.context.view_layer.update()
guard = {b.name: bpy.data.objects[b.name].matrix_world.copy() for b in rig.pose.bones}
if staff:
    # Runtime ApplyStaffGuard replaces the raw JKA j_guard with native g3r1,
    # retargeted by CaptureNativePose. Reuse the established settled capture.
    captured = profile.parent / 'staff-connected-poses.txt'
    captured_local = {}
    for line in captured.read_text().splitlines():
        f = line.split()
        if int(f[0]) == 109 and int(f[1]) == 25:
            from mathutils import Quaternion, Vector
            captured_local[f[6]] = Matrix.LocRotScale(
                Vector(tuple(map(float, f[7:10]))),
                Quaternion((float(f[13]), *map(float, f[10:13]))), Vector((1,1,1)))
    assert set(captured_local) == {n.name for n in walk(body.root_node)}
    world = {}
    for node in walk(body.root_node):
        world[node.name] = world[node.parent.name] @ captured_local[node.name] if node.parent else captured_local[node.name]
    guard = {b.name: world[b.name] for b in rig.pose.bones}
    manifest['runtime_guard_source'] = {'file': captured.name,
        'sha256': hashlib.sha256(captured.read_bytes()).hexdigest(),
        'state': 109, 'frame': 25, 'policy': 'settled KotOR g3r1 runtime guard, residual yaw zero'}
for o in collection.objects:
    o.animation_data_clear()
    o.matrix_basis = bind[o.name]
bpy.context.view_layer.update()
for b in rig.pose.bones:
    b.rotation_mode = 'QUATERNION'
    b.matrix = rig.matrix_world.inverted() @ guard[b.name]
    bpy.context.view_layer.update()
error = max(max(abs(b.matrix[i][j] - guard[b.name][i][j]) for i in range(4) for j in range(4)) for b in rig.pose.bones)
assert error < 0.0001, error
for b in rig.pose.bones:
    for frame in (1, 30):
        for prop in ('location', 'rotation_quaternion', 'scale'):
            b.keyframe_insert(data_path=prop, frame=frame, group=b.name)
anchor = bpy.data.objects.new('SABER_TARGET - fixed grip reference', None)
scene.collection.objects.link(anchor)
anchor.matrix_world = guard['rhand']
constraint = rig.pose.bones['rhand'].constraints.new('COPY_TRANSFORMS')
constraint.target = anchor
constraint.owner_space = constraint.target_space = 'WORLD'
weapon_collection = bpy.data.collections.new('Native saber reference')
scene.collection.children.link(weapon_collection)
weapon_root = weapon.add_to_collection(weapon_collection, options)
weapon_root.parent = anchor
weapon_root.matrix_parent_inverse = Matrix.Identity(4)
if head:
    head_collection = bpy.data.collections.new('Player head preview')
    scene.collection.children.link(head_collection)
    head.animations = []
    head_root = head.add_to_collection(head_collection, options)
    head_anchor = bpy.data.objects.new('HEAD PREVIEW - follows headhook', None)
    scene.collection.objects.link(head_anchor)
    follow = head_anchor.constraints.new('COPY_TRANSFORMS')
    follow.target = rig
    follow.subtarget = 'headhook'
    follow.owner_space = follow.target_space = 'WORLD'
    head_root.parent = head_anchor
    head_root.matrix_parent_inverse = Matrix.Identity(4)
for o in collection.objects:
    if o.type == 'EMPTY':
        o.hide_set(True)
        o.hide_render = True
for b in rig.data.bones:
    group = 'Hands and fingers' if ('fngr' in b.name.lower() or 'thumb' in b.name.lower() or b.name in ('Rhand_g', 'Lhand_g')) else 'Body and arms'
    bc = rig.data.collections.get(group) or rig.data.collections.new(group)
    bc.assign(b)
scene.frame_start = 1
scene.frame_end = 30
scene.frame_set(1)
bpy.context.view_layer.update()
for o in bpy.context.selected_objects:
    o.select_set(False)
rig.select_set(True)
bpy.context.view_layer.objects.active = rig
bpy.ops.object.mode_set(mode='POSE')
selected_bone = 'headhook' if staff else 'Rhand_g'
rig.data.bones.active = rig.data.bones[selected_bone]
rig.pose.bones[selected_bone].select = True
rig.data.show_names = staff
for screen in bpy.data.screens:
    for area in screen.areas:
        if area.type == 'VIEW_3D':
            area.spaces.active.region_3d.view_distance = 2.8
            area.spaces.active.region_3d.view_location = guard[selected_bone].translation
guide = '''RED IDLE GUARD - LOCAL EDITING COPY
This is the restored current j_guard on the KotOR body rig. Game files are unchanged.
The original short static pose is held over frames 1-30 for convenient editing.

1. Save As red_idle_guard_edited.blend before editing.
2. Select the RED GUARD armature and stay in Pose Mode. Never edit the rest pose.
3. At frame 1, select Rhand_g or Lhand_g (actual visible wrist/palm bones).
   Use F3 > Select Pattern and enter the exact bone name if selection is difficult.
4. Rotate with R. R X X, R Y Y or R Z Z rotates around a local bone axis.
   Try small 3-5 degree changes; undo an axis that moves the hand the wrong way.
   Do not translate or scale the wrist. The saber target holds the hilt fixed.
5. For arm placement rotate Rforearm_g/Lforearm_g, then rbicep_g/lbicep_g.
   Do not casually change the bicepL, humerusX or radiusX corrective helpers.
6. Select finger base/tip bones in Hands and fingers and rotate to wrap the hilt.
   The exact finger names are listed below. rhand is the weapon hook, not the palm.
7. Select all pose bones (A), F3 > Insert Keyframe with Keying Set... >
   Location, Rotation & Scale. This is also in Pose > Animation.
   Key before scrubbing, or the animation resets edits.
8. With all bones selected, Ctrl+C copies the pose. Go to frame 30, Ctrl+V pastes
   it (without Shift), then insert Location, Rotation & Scale keys again.
9. Scrub frames 1-30: the edited guard should hold still. Save the edited .blend.
10. Return the edited file path for import into both j_guard and moving y_guard.
    Do not export MDL yourself. Other animations and JKA chains must be preserved.

Only rhand has a fixed-reference constraint. Keep SABER_TARGET and that constraint.
This is source-pose editing; runtime aim, movement and blend layers are not shown.
The scene is for local use with owned game data; do not publish extracted assets.

FINGER BONES:
'''
guide += '\n'.join(b.name for b in rig.pose.bones if 'fngr' in b.name.lower() or 'thumb' in b.name.lower())
if staff:
    guide = '''DOUBLE-BLADE IDLE GUARD - HEAD EDITING
Implemented side-held KotOR runtime guard on the player rig; native head and double
saber attached. Game animations and existing Blender files are unchanged.
Settled native g3r1 capture (state109/frame25) held at frames 1 and 30 for editing.

1. Save As staff_idle_guard_edited.blend.
2. Stay in Pose Mode on STAFF GUARD - edit in Pose Mode. Go to frame 1.
3. headhook is already selected. To reselect: hover over the 3D viewport,
   Alt+A to deselect, then F3 > Select Pattern > headhook.
4. Press R, then Z, then Z for local Z; use X X or Y Y for the other local axes.
   Type a small angle (e.g. 5) and Enter; Ctrl+Z undoes. The head preview follows.
   Do not move/scale the hook, edit rest bones, or rotate the head mesh separately.
5. With headhook selected, F3 > Insert Keyframe with Keying Set... >
   Location, Rotation & Scale. Key the change before scrubbing.
6. Ctrl+C copies the selected bone pose. Set frame 30, Ctrl+V, then insert the
   same keying set. Scrub 1-30: the head should hold its new angle. Save.
7. Return the edited file path for a separately requested guard-only import.

Outliner: STAFF GUARD object > armature data > Pose > cutscenedummy > rootdummy >
pelvis_g > torso_g > torsoMid_g > torsoUpr_g > headhook (expand as needed).
Use the select-pattern method if your Outliner hides pose bones.
Only headhook controls the attached head here; the head mesh is a reference.
Runtime aim/look layers are not reproduced in this scene.
Owned local assets: do not publish this extracted model file.
'''
(out / 'EDITING.md').write_text(guide)
bpy.data.texts.new('READ ME - hand editing').write(guide)
bpy.ops.file.pack_all()
bpy.ops.wm.save_as_mainfile(filepath=str(target))
manifest.update({'guard_matrix_max_error': error, 'clip_seconds': clip.length, 'bones': [b.name for b in rig.pose.bones]})
(out / 'manifest.json').write_text(json.dumps(manifest, indent=2))
print('STAFF_GUARD_EXPORT_PASS' if staff else 'RED_GUARD_EXPORT_PASS', error, flush=True)
