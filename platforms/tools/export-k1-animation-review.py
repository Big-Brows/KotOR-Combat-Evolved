"""Export current stance/native animation sources to an ignored Blender review folder.

Run using the existing Blender installation in background mode. Does not deploy edits.
"""
from pathlib import Path
import copy
import csv
import hashlib
import importlib.util
import json
import shutil
import subprocess
import sys

import bpy
from mathutils import Matrix

repo = Path(__file__).resolve().parents[2]
out = repo / 'platforms/out/animation-head-review'
subprocess.run(['git', '-C', str(repo), 'check-ignore', '--quiet', str(out / 'source')], check=True)
out.mkdir(exist_ok=True)
assert not any(out.glob('*.blend')) or '--replace-generated' in sys.argv, 'Preserve existing editable scenes before exporting again.'
source = out / 'source'
source.mkdir(exist_ok=True)
sys.path.insert(0, str(repo / 'platforms/out/kotor-animation-import/tools'))
sys.path.insert(0, str(repo / 'tools'))
sys.path.insert(0, str(repo / 'platforms/tools'))
import io_scene_kotor
from io_scene_kotor.constants import ImportOptions
from io_scene_kotor.format.mdl.reader import MdlReader
from io_scene_kotor.scene import armature
from padglyphs import erf_resource

spec = importlib.util.spec_from_file_location('owned', repo / 'platforms/out/k1-engage-prototype/dark-jedi/inspect-owned.py')
owned = importlib.util.module_from_spec(spec)
spec.loader.exec_module(owned)
profile = repo / 'platforms/out/k1-engage-prototype/elbow-arena/profile/override'
custom = ['k1jarig', 'k1jastaff', 'k1jadual', 'k1jadark', 'k1jadstaff', 'k1jaddual', 'k1jasith', 'k1yellow']
native = ['pmbbl', 'n_darkjedim', 'n_sithsoldier']
models = {}
provenance = {}

def load(name):
    name = name.lower()
    if name in models:
        return models[name]
    for kind, ext in [(2002, 'mdl'), (3008, 'mdx')]:
        p = source / (name + '.' + ext)
        local = profile / p.name
        if local.exists():
            shutil.copy2(local, p)
            origin = 'isolated profile override'
        else:
            try:
                p.write_bytes(owned.resource(name, kind))
            except KeyError:
                if ext == 'mdx':
                    continue
                raise
            origin = 'owned K1 KEY/BIF resource'
        provenance[p.name] = {'origin': origin, 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()}
    model = MdlReader(str(source / (name + '.mdl'))).load()
    models[name] = model
    if model.supermodel and model.supermodel.lower() not in ('null', name):
        load(model.supermodel)
    return model

for name in custom + native:
    load(name)
appearance = owned.twoda(owned.resource('appearance', 2017))
heads = owned.twoda(owned.resource('heads', 2017))
head_name = heads[appearance['168']['normalhead']]['head'].lower()
head = load(head_name)

def walk(node):
    yield node
    for child in node.children:
        yield from walk(child)

textures = {getattr(node, field, '').lower() for model in models.values()
            for node in walk(model.root_node) for field in ('bitmap', 'bitmap2')}
for name in sorted(textures - {'', 'null'}):
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

rows = []
for name in custom + native:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.render.fps = 30
    model = copy.deepcopy(models['k1jarig'] if name == 'k1yellow' else models[name])
    # Resolve inherited animation names exactly once, nearest source winning.
    clips = {}
    ancestor = name
    while ancestor and ancestor != 'null':
        m = models[ancestor]
        for clip in m.animations:
            clips.setdefault(clip.name.lower(), (ancestor, clip))
        ancestor = m.supermodel.lower() if m.supermodel else 'null'
    model.animations = []
    options = ImportOptions()
    options.import_animations = False
    options.import_walkmeshes = False
    options.build_armature = False
    options.texture_search_paths = [str(source)]
    options.lightmap_search_paths = []
    collection = bpy.data.collections.new('Editable native nodes and meshes')
    scene.collection.children.link(collection)
    root = model.add_to_collection(collection, options)
    by_name = {n.name.lower(): n.node_number for n in walk(model.root_node)}
    for clip_name, (origin, original) in clips.items():
        clip = copy.deepcopy(original)
        for node in walk(clip.root_node):
            node.node_number = by_name.get(node.name.lower(), -999999)
        # Native supermodels name their root after themselves, while the body
        # root has the body resource name. Map that anchor before traversal.
        clip.root_node.node_number = model.root_node.node_number
        clip.add_to_objects(root, 1.0)
        record = root.kb.anim_list[-1]
        scene.timeline_markers.new(clip.name, frame=record.frame_start)
        rows.append({'file': name + '.blend', 'clip': clip.name, 'source': origin,
                     'start': record.frame_start, 'end': record.frame_end, 'seconds': clip.length})
    scene.frame_set(0)
    bpy.context.view_layer.update()
    rig = armature.rebuild_armature(root)
    assert rig is not None, ('No preview armature', name)
    rig.name = 'Preview rig - edit the native node objects'
    # Exact world transforms preserve the source hierarchy without the older
    # importer's lossy direct clavicle-to-pose conversion.
    for bone in rig.pose.bones:
        target = bpy.data.objects.get(bone.name)
        assert target is not None
        constraint = bone.constraints.new('COPY_TRANSFORMS')
        constraint.target = target
        constraint.owner_space = 'WORLD'
        constraint.target_space = 'WORLD'
    hook = next((o for o in collection.objects if o.name.lower() == 'headhook'), None)
    if name in ('k1jarig', 'k1jastaff', 'k1jadual', 'k1yellow', 'pmbbl') and hook:
        attachment = bpy.data.collections.new('Player head preview')
        scene.collection.children.link(attachment)
        head_root = head.add_to_collection(attachment, options)
        head_root.parent = hook
        head_root.matrix_parent_inverse = Matrix.Identity(4)
    scene.frame_start = min(r['start'] for r in rows if r['file'] == name + '.blend')
    scene.frame_end = max(r['end'] for r in rows if r['file'] == name + '.blend')
    scene.frame_set(scene.frame_start)
    for obj in bpy.context.selected_objects:
        obj.select_set(False)
    selected = next((o for o in collection.objects if o.name.lower() in ('head_g', 'headhook')), root)
    selected.select_set(True)
    bpy.context.view_layer.objects.active = selected
    scene['editing_help'] = 'Edit original node object transforms/keyframes, not constrained preview bones. See README.md and clips.csv.'
    bpy.ops.file.pack_all()
    bpy.ops.wm.save_as_mainfile(filepath=str(out / (name + '.blend')))
    assert any(o.animation_data and o.animation_data.action for o in collection.objects)
    print('ANIMATION_REVIEW_PASS', name, len(clips), flush=True)

with (out / 'clips.csv').open('w', newline='') as f:
    writer = csv.DictWriter(f, fieldnames=['file', 'clip', 'source', 'start', 'end', 'seconds'])
    writer.writeheader()
    writer.writerows(rows)
(out / 'manifest.json').write_text(json.dumps({'source_resources': provenance, 'blend_files': custom + native,
                                            'clips': len(rows)}, indent=2))
print('ANIMATION_REVIEW_COMPLETE', len(custom + native), len(rows), flush=True)
