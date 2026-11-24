"""Bake a local Blender guard edit into player red guard clips only.

Run with Blender --background --disable-autoexec --python-exit-code 1 --python
this-file -- --blend <edited.blend>. Reads frame 1; does not deploy or launch.
"""
from pathlib import Path
import argparse
import copy
import hashlib
import json
import shutil
import subprocess
import sys
from datetime import datetime
import bpy
from mathutils import Vector

repo = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('--blend', type=Path, required=True)
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
blend = args.blend.resolve()
root = repo / 'platforms/out/red-stance'
out = root / 'guard-edits' / datetime.now().strftime('%Y%m%d-%H%M%S')
subprocess.run(['git', '-C', str(repo), 'check-ignore', '--quiet', str(out / 'backup')], check=True)
out.mkdir(parents=True)
sys.path.insert(0, str(repo / 'platforms/out/kotor-animation-import/tools'))
from io_scene_kotor.format.mdl.reader import MdlReader
from io_scene_kotor.format.mdl.writer import MdlWriter
def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def nodes(n):
    yield n
    for c in n.children:
        yield from nodes(c)
def pose(path):
    bpy.ops.wm.open_mainfile(filepath=str(path))
    bpy.context.scene.frame_set(1)
    bpy.context.view_layer.update()
    rig = next(o for o in bpy.context.scene.objects if o.type == 'ARMATURE')
    return {b.name: b.parent.matrix.inverted() @ b.matrix if b.parent else b.matrix.copy()
            for b in rig.pose.bones}
blend_hash = digest(blend)
baseline = pose(repo / 'platforms/out/red-guard-edit/red_idle_guard_easy_select.blend')
edited = pose(blend)
assert baseline.keys() == edited.keys()
changed = {name: m for name, m in edited.items()
           if max(abs(m[i][j]-baseline[name][i][j]) for i in range(4) for j in range(4)) > 1e-5}
assert changed, 'No keyed frame-1 pose changes found'
allowed = {'Rforearm_g', 'Lforearm_g', 'Rhand_g', 'Lhand_g', 'rhand', 'lhand',
           'rbicep_g', 'lbicep_g', 'RbicepL_g', 'LbicepL_g',
           'rradiusX', 'lradiusX', 'rhumerusX', 'lhumerusX'}
assert all(n in allowed or 'Fngr' in n or 'Thumb' in n for n in changed), changed.keys()
assets = root / 'assets'
src = assets / 'k1jared.mdl'
backup = out / 'backup'
backup.mkdir()
for p in (src, src.with_suffix('.mdx'), assets / 'red-guard-player.txt', root / 'generation-report.json'):
    shutil.copy2(p, backup / p.name)
protected = list(assets.glob('*')) + list((root / 'npc/assets').glob('*'))
profile = repo / 'platforms/out/k1-engage-prototype/elbow-arena/profile/override'
protected += list(profile.glob('k1ja*.md*')) + list(profile.glob('k1yellow.md*'))
protected = [p for p in protected if p.is_file() and p not in (src, src.with_suffix('.mdx'), assets / 'red-guard-player.txt')]
protected_hashes = {str(p): digest(p) for p in protected}
model = MdlReader(str(src)).load()
original = copy.deepcopy(model)
lookup = {n.name: n for n in nodes(model.root_node)}
for clip in model.animations:
    if clip.name not in ('j_guard', 'y_guard'):
        continue
    tracks = {n.name: n for n in nodes(clip.root_node)}
    for name, matrix in changed.items():
        node = tracks[name]
        assert 'position' in node.keyframes and 'orientation' in node.keyframes, name
        p, q, scale = matrix.decompose()
        assert max(abs(s-1) for s in scale) < 1e-4, (name, scale)
        pos = p-Vector(lookup[name].position)
        node.keyframes['position'] = [[row[0], *pos] for row in node.keyframes['position']]
        node.keyframes['orientation'] = [[row[0], q.x, q.y, q.z, q.w] for row in node.keyframes['orientation']]
# The binary writer requires face normals on previously loaded meshes.
def retain_tracks(node):
    descendants = [retain_tracks(c) for c in node.children]
    node.animated = bool(node.keyframes) or any(descendants)
    return node.animated
for clip in model.animations:
    retain_tracks(clip.root_node)
for n in nodes(model.root_node):
    if hasattr(n, 'facelist') and n.facelist:
        n.facelist.normals = []
        for a,b,c in n.facelist.vertices:
            normal = (Vector(n.verts[b])-Vector(n.verts[a])).cross(Vector(n.verts[c])-Vector(n.verts[a]))
            n.facelist.normals.append(list(normal.normalized() if normal.length > 1e-10 else Vector(n.normals[a])))
dest = out / 'k1jared.mdl'
MdlWriter(str(dest), model, False, False, False).save()
back = MdlReader(str(dest)).load()
assert len(back.animations) == len(original.animations)
maximum = 0.
unchanged = 0
for old, expected, actual in zip(original.animations, model.animations, back.animations):
    assert old.name == expected.name == actual.name
    assert abs(old.length-actual.length) < 1e-6
    assert len(list(nodes(old.root_node))) == len(list(nodes(actual.root_node)))
    for a,b,c in zip(nodes(old.root_node),nodes(expected.root_node),nodes(actual.root_node)):
        assert a.name == b.name == c.name
        assert b.keyframes.keys() == c.keyframes.keys()
        if old.name not in ('j_guard','y_guard') or a.name not in changed:
            assert a.keyframes == c.keyframes, (old.name,a.name)
        for kind, keys in b.keyframes.items():
            assert len(keys)==len(c.keyframes[kind])
            maximum=max(maximum,max((abs(x-y) for row,row2 in zip(keys,c.keyframes[kind]) for x,y in zip(row,row2)),default=0.))
    if old.name not in ('j_guard','y_guard'): unchanged += 1
assert maximum < 1e-6, maximum
for a,b in zip(nodes(original.root_node),nodes(back.root_node)):
    assert a.name == b.name
    for attr in ('position','orientation','verts','normals','weights','uv1','texture1'):
        assert getattr(a,attr,None)==getattr(b,attr,None),(a.name,attr)
expected_path = out / 'red-guard-player.txt'
lines = []
for line in (assets / expected_path.name).read_text().splitlines():
    fields=line.split()
    if fields[2] in changed:
        p,q,_=changed[fields[2]].decompose()
        line=' '.join(fields[:3])+f' {p.x:.9f} {p.y:.9f} {p.z:.9f} {q.x:.9f} {q.y:.9f} {q.z:.9f} {q.w:.9f}'
    lines.append(line)
expected_path.write_text('\n'.join(lines)+'\n')
assert digest(blend)==blend_hash
assert protected_hashes=={str(p):digest(p) for p in protected}
report=json.loads((root/'generation-report.json').read_text())
report['mdl_sha256']=digest(dest)
report['mdx_sha256']=digest(dest.with_suffix('.mdx'))
report['extra_assets'][expected_path.name]=digest(expected_path)
report['guard_policy']='user-authored Blender frame-1 pose, player j_guard/y_guard only'
report['clips']['j_guard']['policy']=report['guard_policy']
report['original_guard_snapshot_error']=None
report['guard_edit']={'blend_name':blend.name,'sha256':blend_hash,'frame':1,'changed_local_bones':sorted(changed),'key_roundtrip_max_error':maximum,'unchanged_clips':unchanged,'runtime_tested':False}
for p in (dest,dest.with_suffix('.mdx'),expected_path):
    shutil.copy2(p,assets/p.name)
(root/'generation-report.json').write_text(json.dumps(report,indent=2))
(out/'import-report.json').write_text(json.dumps(report['guard_edit'],indent=2))
print('RED_GUARD_EDIT_IMPORT_PASS', report['guard_edit'], flush=True)
