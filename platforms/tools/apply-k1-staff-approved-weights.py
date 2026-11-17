"""Export accepted staff-only skin weights; deployment is a separate locked step."""
from pathlib import Path
import copy
import hashlib
import json
import shutil
import sys

from mathutils import Vector

repo=Path(__file__).resolve().parents[2]
engage=repo/'platforms/out/k1-engage-prototype'
work=engage/'staff-prototype/weight-integration'
candidate=work/'candidate'
assert not candidate.exists(), 'Preserve previous exports.'
candidate.mkdir()
before=work/'before';before.mkdir()
profile=engage/'elbow-arena/profile'
sys.path.insert(0,str(repo/'platforms/out/kotor-animation-import/tools'))
from io_scene_kotor.format.mdl.reader import MdlReader
from io_scene_kotor.format.mdl.writer import MdlWriter

def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def nodes(n):return [n]+[x for c in n.children for x in nodes(c)]
def animation(a):return (a.name,a.length,a.transtime,a.animroot,[(n.name,n.keyframes) for n in nodes(a.root_node)])
def record(n):
    fields=('node_number','nodetype','position','orientation','scale','verts','normals','uv1','uv2','tangentspace','tangents','bitangents','tangentspacenormals','bitmap','bitmap2','render','shadow','selfillumcolor','alpha','animateuv','uvdirectionx','uvdirectiony')
    return dict(parent=n.parent.name if n.parent else None,fields={k:copy.deepcopy(getattr(n,k,None)) for k in fields},faces=copy.deepcopy(n.facelist.vertices) if getattr(n,'facelist',None) else None)

paths=[profile/'override'/('k1jastaff'+e) for e in ('.mdl','.mdx')]
paths += [engage/'staff-prototype/assets'/('k1jastaff'+e) for e in ('.mdl','.mdx')]
paths += [engage/'staff-prototype/generation-report.json']
protected=[profile/'override'/(name+ext) for name in ('k1jarig','k1yellow','k1jadark','k1jasith') for ext in ('.mdl','.mdx')]
protected += [repo/'src/game/clientcore/k1engage.cpp',repo/'platforms/out/k1-host-pc/windows/k1.exe',repo/'platforms/tools/launch-k1-engage-prototype.ps1']
original_hashes={str(p.relative_to(repo)):digest(p) for p in paths}
protected_hashes={str(p.relative_to(repo)):digest(p) for p in protected}
for i,p in enumerate(paths):shutil.copy2(p,before/(f'{i:02d}-'+p.name))
shutil.copy2(__file__,work/'export-recipe.py')
payload=engage/'staff-swing-review/weights-02/skin-weights.json'
weights=json.loads(payload.read_text())
model=MdlReader(str(paths[0])).load()
parts=nodes(model.root_node)
original={n.name:record(n) for n in parts}
old_weights={n.name:copy.deepcopy(n.weights) for n in parts if n.nodetype=='SKIN'}
clips=[animation(a) for a in model.animations]
expected=set()
for n in parts:
    if n.nodetype!='SKIN':continue
    assert n.weights==weights['original_weights'][n.name],(n.name,'preview baseline drift')
    for i,(old,new) in enumerate(zip(n.weights,weights['weights'][n.name])):
        if dict(old)!=dict(new):expected.add((n.name,i))
    n.weights=copy.deepcopy(weights['weights'][n.name])
assert len(expected)==249 and all(n=='Torso' for n,i in expected)
for n in parts:
    if getattr(n,'facelist',None):
        n.facelist.normals=[]
        for a,b,c in n.facelist.vertices:
            normal=(Vector(n.verts[b])-Vector(n.verts[a])).cross(Vector(n.verts[c])-Vector(n.verts[a]))
            n.facelist.normals.append(list(normal.normalized() if normal.length>1e-10 else Vector(n.normals[a])))
def mark(n):
    children=[mark(c) for c in n.children]
    n.animated=bool(n.keyframes) or any(children)
    return n.animated
for a in model.animations:mark(a.root_node)
out=candidate/'k1jastaff.mdl'
MdlWriter(str(out),model,False,False,False).save()
back=MdlReader(str(out)).load()
changed=set();palettes={};maximum_weight_error=0.
for n in nodes(back.root_node):
    assert record(n)==original[n.name],(n.name,'non-weight change')
    if n.nodetype!='SKIN':continue
    for i,(old,new) in enumerate(zip(old_weights[n.name],n.weights)):
        if (n.name,i) in expected:
            target=dict(weights['weights'][n.name][i]);actual=dict(new)
            error=max(abs(actual.get(b,0)-target.get(b,0)) for b in actual.keys()|target.keys())
            maximum_weight_error=max(maximum_weight_error,error)
            assert error<1e-7,(n.name,i,error)
            changed.add((n.name,i))
        else:assert old==new,(n.name,i,'unrequested weight change')
        assert len(new)<=4 and abs(sum(w for _,w in new)-1)<1e-5
    palettes[n.name]=len({b for row in n.weights for b,w in row if w>0})
assert changed==expected
assert max(palettes.values())<=16
assert [animation(a) for a in back.animations]==clips,'animation changed'
assert all(digest(repo/p)==h for p,h in protected_hashes.items())
report=dict(pass_=True,scope='accepted double-blade body skin weights only',original_hashes=original_hashes,protected_hashes=protected_hashes,weight_payload=str(payload.relative_to(repo)),weight_payload_sha256=digest(payload),changed_vertices=sorted(changed),all_animations_exact=len(clips),geometry_bind_uv_normals_materials_hierarchy_exact=True,other_weights_exact=True,palettes=palettes,maximum_float_weight_error=maximum_weight_error,files={p.name:digest(p) for p in candidate.iterdir()},recipe_sha256=digest(Path(__file__)))
(work/'export-validation.json').write_text(json.dumps(report,indent=2))
identity=json.loads(paths[-1].read_text())
identity['mdl_sha256']=digest(out)
identity['mdx_sha256']=digest(out.with_suffix('.mdx'))
identity['skin_weight_patch']=dict(report='weight-integration/export-validation.json',baseline_generation_report_sha256=original_hashes[str(paths[-1].relative_to(repo))],changed_vertices=249,source_payload_sha256=digest(payload),scope='skin weights only; all existing animation tracks exact; native integration results tracked in STATUS.md')
(candidate/'generation-report.json').write_text(json.dumps(identity,indent=2))
print('STAFF_WEIGHT_EXPORT_PASS',len(changed),'animations_exact',len(clips),'palettes',palettes,flush=True)
