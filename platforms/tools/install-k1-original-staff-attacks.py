"""Export the accepted original-JKA preview attacks onto the reweighted staff body."""
from pathlib import Path
import ast
import copy
import hashlib
import json
import math
import shutil
import sys

from mathutils import Matrix, Quaternion, Vector

repo=Path(__file__).resolve().parents[2]
engage=repo/'platforms/out/k1-engage-prototype'
work=engage/'staff-prototype/original-attack-integration'
candidate=work/'candidate-02';assert not candidate.exists();candidate.mkdir()
before=work/'before-02';before.mkdir()
profile=engage/'elbow-arena/profile'
sys.path.insert(0,str(repo/'platforms/out/kotor-animation-import/tools'))
from io_scene_kotor.format.mdl.reader import MdlReader
from io_scene_kotor.format.mdl.writer import MdlWriter
from io_scene_kotor.scene.animation import Animation
from io_scene_kotor.scene.animnode import AnimationNode
helper=repo/'platforms/tools/apply-k1-staff-approved-weights.py'
tree=ast.parse(helper.read_text())
exec(compile(ast.Module(body=[n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name in ('digest','nodes','animation','record')],type_ignores=[]),str(helper),'exec'))

inputs=[profile/'override'/('k1jastaff'+e) for e in ('.mdl','.mdx')]
inputs += [engage/'staff-prototype/assets'/('k1jastaff'+e) for e in ('.mdl','.mdx')]
inputs += [engage/'staff-prototype/generation-report.json',profile/'staff-expected.txt']
protected=[profile/'override'/(n+e) for n in ('k1jarig','k1yellow','k1jadark','k1jasith') for e in ('.mdl','.mdx')]
protected += [repo/'src/game/clientcore/k1engage.cpp',repo/'platforms/out/k1-host-pc/windows/k1.exe',repo/'platforms/tools/launch-k1-engage-prototype.ps1']
original_hashes={str(p.relative_to(repo)):digest(p) for p in inputs}
protected_hashes={str(p.relative_to(repo)):digest(p) for p in protected}
for i,p in enumerate(inputs):shutil.copy2(p,before/(f'{i:02d}-'+p.name))
shutil.copy2(__file__,work/'export-recipe-02.py')
reference=engage/'staff-swing-review/weights-02/poses.json'
proposals=json.loads(reference.read_text())
assert reference.read_bytes()==(engage/'staff-swing-review/stock-reset-01/poses.json').read_bytes()
model=MdlReader(str(inputs[0])).load()
nn=nodes(model.root_node);rig_names=set(n.name for n in nn)
original_nodes={n.name:record(n) for n in nn}
original_weights={n.name:copy.deepcopy(n.weights) for n in nn if n.nodetype=='SKIN'}
old_clips={a.name:animation(a) for a in model.animations}
upper=set()
for n in nn:
    p=n
    while p and p.name!='torso_g':p=p.parent
    if p:upper.add(n.name)
upper.add('j_motion')
samples={};lengths={}
for clip,d in proposals.items():
    frames={float(t):{n:Matrix.LocRotScale(Vector(v[:3]),Quaternion((v[6],*v[3:6])).normalized(),Vector((1,1,1))) for n,v in pose.items()} for t,pose in d['poses'].items()}
    assert all(set(p)==rig_names for p in frames.values())
    samples[clip]=frames;lengths[clip]=d['length']
    overlay='y_'+clip[2:];samples[overlay]=frames;lengths[overlay]=d['length']
recipe=(engage/'elbow-arena/generate-candidate-applied.py').read_text()
definition=recipe[recipe.index('def make_animation(name):'):recipe.index('model.animations=[make_animation(n)')]
exec(compile(definition,'<preserved-native-animation-writer>','exec'))
model.animations=[make_animation(a.name) if a.name in samples else a for a in model.animations]
assert len(samples)==14 and samples.keys()<=old_clips.keys()
for n in nn:
    if getattr(n,'facelist',None):
        n.facelist.normals=[]
        for a,b,c in n.facelist.vertices:
            normal=(Vector(n.verts[b])-Vector(n.verts[a])).cross(Vector(n.verts[c])-Vector(n.verts[a]))
            n.facelist.normals.append(list(normal.normalized() if normal.length>1e-10 else Vector(n.normals[a])))
def mark(n):
    children=[mark(c) for c in n.children];n.animated=bool(n.keyframes) or any(children);return n.animated
for a in model.animations:mark(a.root_node)
out=candidate/'k1jastaff.mdl';MdlWriter(str(out),model,False,False,False).save()
back=MdlReader(str(out)).load()
for n in nodes(back.root_node):
    assert record(n)==original_nodes[n.name],(n.name,'body change')
    if n.nodetype=='SKIN':assert n.weights==original_weights[n.name],(n.name,'weights changed')
untouched=0;maximum=0.;comparisons=0
for a in back.animations:
    if a.name not in samples:
        assert animation(a)==old_clips[a.name],(a.name,'unrequested clip change');untouched+=1;continue
    assert abs(a.length-lengths[a.name])<1e-6
    tracks={n.name:n.keyframes for n in nodes(a.root_node)}
    controlled=upper if a.name.startswith('y_') else rig_names
    for n in nn:
        if n.name not in controlled:assert not tracks.get(n.name);continue
        positions=tracks[n.name].get('position');rotations=tracks[n.name].get('orientation')
        if positions:assert len(positions)==len(samples[a.name])
        if rotations:assert len(rotations)==len(samples[a.name])
        for index,(t,expected) in enumerate(samples[a.name].items()):
            p=Vector(n.position);q=Quaternion(n.orientation).normalized()
            if positions:
                key=positions[index];assert abs(key[0]-t)<1e-6;p+=Vector(key[1:])
            if rotations:
                key=rotations[index];assert abs(key[0]-t)<1e-6;q=Quaternion((key[4],*key[1:4])).normalized()
            actual=Matrix.LocRotScale(p,q,Vector((1,1,1)))
            error=max(abs(x-y) for row,other in zip(actual,expected[n.name]) for x,y in zip(row,other))
            maximum=max(maximum,error);comparisons+=1
assert maximum<1e-5,maximum
assert all(digest(repo/p)==h for p,h in protected_hashes.items())
report=dict(pass_=True,replaced_attack_clips=sorted(samples),untouched_clips=untouched,pose_comparisons=comparisons,maximum_preview_matrix_error=maximum,all_body_and_weights_exact=True,mdx_byte_identical=out.with_suffix('.mdx').read_bytes()==inputs[1].read_bytes(),preview_pose_file=str(reference.relative_to(repo)),preview_pose_sha256=digest(reference),original_hashes=original_hashes,protected_hashes=protected_hashes,mdl_sha256=digest(out),mdx_sha256=digest(out.with_suffix('.mdx')))
(work/'export-validation.json').write_text(json.dumps(report,indent=2))
identity=json.loads(inputs[4].read_text())
identity.update(mdl_sha256=digest(out),mdx_sha256=digest(out.with_suffix('.mdx')),scope='original JKA preview attack tracks plus approved staff weights; non-attack clips preserved',staff_posture=dict(mode='original JKA source; no manual attack shaping',pose_reference=str(reference.relative_to(engage))),hand_reattachment='existing100ms target approach; authored free-hand recovery retained')
identity['original_attack_patch']=dict(report='original-attack-integration/export-validation.json',reference_sha256=digest(reference),attack_clips=14,untouched_clips=untouched)
(candidate/'generation-report.json').write_text(json.dumps(identity,indent=2))
old_lines=inputs[5].read_text().splitlines(keepends=True)
with (candidate/'staff-expected.txt').open('w') as f:
    for line in old_lines:
        if line.split()[0] not in samples:f.write(line)
    for name,frames in samples.items():
        controlled=upper if name.startswith('y_') else rig_names
        for t,pose in frames.items():
            for bone in sorted(controlled):
                p,q,_=pose[bone].decompose()
                f.write(f'{name} {t:.9f} {bone} {p.x:.9f} {p.y:.9f} {p.z:.9f} {q.x:.9f} {q.y:.9f} {q.z:.9f} {q.w:.9f}\n')
print('ORIGINAL_STAFF_ATTACK_EXPORT_PASS',comparisons,maximum,'preserved',untouched,flush=True)
