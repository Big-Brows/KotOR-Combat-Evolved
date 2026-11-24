"""Close the native backpack opening on current stance assets, offline in Blender.

Reuses the reviewed staff closure. Only ignored generated assets are written;
--install updates the isolated profile and its canonical component hashes.
"""
from pathlib import Path
from datetime import datetime
import argparse
import ast
import copy
import hashlib
import json
import shutil
import subprocess
import sys
from mathutils import Matrix, Quaternion, Vector

repo=Path(__file__).resolve().parents[2]
out=repo/'platforms/out/back-panel-all-stances'
subprocess.run(['git','-C',str(repo),'check-ignore','--quiet',str(out/'assets/check.mdl')],check=True)
out.mkdir(exist_ok=True)
args=argparse.ArgumentParser()
args.add_argument('--install',action='store_true')
options=args.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
sys.path.insert(0,str(repo/'platforms/out/kotor-animation-import/tools'))
from io_scene_kotor.format.mdl.reader import MdlReader
from io_scene_kotor.format.mdl.writer import MdlWriter

def nodes(n):return [n]+[v for c in n.children for v in nodes(c)]
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def clips(model):return [(a.name,a.length,a.transtime,a.animroot,[(n.name,n.keyframes) for n in nodes(a.root_node)]) for a in model.animations]
def mark(n):
    children=[mark(c) for c in n.children]
    n.animated=bool(n.keyframes) or any(children)
    return n.animated

# The existing staff exporter already closes precisely this verified native rim.
tree=ast.parse((repo/'platforms/tools/prepare-k1-staff-prototype.py').read_text())
definition=[n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name=='close_staff_back_panel']
assert len(definition)==1
exec(compile(ast.Module(body=definition,type_ignores=[]),'reviewed-staff-panel-closure','exec'))
profile=repo/'platforms/out/k1-engage-prototype/elbow-arena/profile'
components={
    'k1jarig':None,
    'k1jared':repo/'platforms/out/red-stance',
    'k1jablue':repo/'platforms/out/blue-stance',
    'k1japurp':repo/'platforms/out/purple-stance',
    'k1jadual':repo/'platforms/out/dual-stance',
    'k1jastaff':repo/'platforms/out/k1-engage-prototype/staff-prototype',
}
backup=out/'backups'/datetime.now().strftime('%Y%m%d-%H%M%S')
backup.mkdir(parents=True)
assets=out/'assets';assets.mkdir(exist_ok=True)
report={'pass':False,'runtime_tested':False,'models':{}}
pending=[]
for resource,component in components.items():
    installed=profile/'override'/(resource+'.mdl')
    source=component/'assets'/(resource+'.mdl') if component else installed
    assert source.is_file() and installed.is_file(),resource
    # Staff/dual already have a closed panel and can have independent snapshots.
    model=MdlReader(str(source)).load()
    before=copy.deepcopy(model)
    rest={}
    for n in nodes(model.root_node):
        m=Matrix.LocRotScale(Vector(n.position),Quaternion(n.orientation),Vector((n.scale,)*3))
        rest[n.name]=rest[n.parent.name]@m if n.parent else m
    torso=next(n for n in nodes(model.root_node) if n.name=='Torso')
    try:cap=close_staff_back_panel(torso)
    except AssertionError as error:
        assert len(torso.verts)==791 and len(torso.facelist.vertices)==749,(resource,'unexpected closed topology')
        report['models'][resource]={'already_closed':True,'sha256':digest(source)}
        print('BACK_PANEL_ALREADY_CLOSED',resource,flush=True)
        continue
    assert digest(source)==digest(installed),(resource,'canonical/profile divergence')
    if cap['added_faces']==0:
        report['models'][resource]={'already_closed':True,'sha256':digest(source)}
        print('BACK_PANEL_ALREADY_CLOSED',resource,flush=True)
        continue
    assert cap['added_faces']==14
    # UV-seam copies on the rim must share skinning: then welded boundary
    # vertices coincide for every animation, not merely in the bind pose.
    rim_weights={}
    for index in cap['ring']:
        matches=[i for i,p in enumerate(torso.verts) if (Vector(p)-Vector(torso.verts[index])).length<.00001]
        expected=dict(torso.weights[index])
        for i in matches:
            actual=dict(torso.weights[i])
            assert set(actual)==set(expected) and all(abs(actual[b]-expected[b])<.00001 for b in expected),(resource,index,i,'rim skin mismatch')
        rim_weights[str(index)]=matches
    for a in model.animations:mark(a.root_node)
    for n in nodes(model.root_node):
        if getattr(n,'facelist',None):
            n.facelist.normals=[]
            for a,b,c in n.facelist.vertices:
                normal=(Vector(n.verts[b])-Vector(n.verts[a])).cross(Vector(n.verts[c])-Vector(n.verts[a]))
                n.facelist.normals.append(list(normal.normalized() if normal.length>1e-10 else Vector(n.normals[a])))
    dest=assets/(resource+'.mdl')
    MdlWriter(str(dest),model,False,False,False).save()
    after=MdlReader(str(dest)).load()
    assert clips(before)==clips(after),(resource,'animation changed')
    aa=nodes(before.root_node);bb=nodes(after.root_node)
    assert [n.name for n in aa]==[n.name for n in bb]
    attributes=('nodetype','position','orientation','scale','verts','normals','weights','uv1','uv2','texture','texture2','render','shadow','alpha')
    for a,b in zip(aa,bb):
        assert (a.parent.name if a.parent else None)==(b.parent.name if b.parent else None)
        for attr in attributes:assert getattr(a,attr,None)==getattr(b,attr,None),(resource,a.name,attr)
        if getattr(a,'facelist',None):
            for attr in ('vertices','materials','uv'):
                original=getattr(a.facelist,attr);actual=getattr(b.facelist,attr)
                assert actual[:len(original)]==original,(resource,a.name,attr)
                assert len(actual)==len(original)+(14 if a.name=='Torso' else 0)
    entry={**cap,'animations_unchanged':len(before.animations),'original_geometry_and_weights_unchanged':True,
           'rim_skin_copies':rim_weights,'before_mdl_sha256':digest(source),
           'mdl_sha256':digest(dest),'mdx_sha256':digest(dest.with_suffix('.mdx'))}
    report['models'][resource]=entry
    pending.append((resource,source,component,entry))
    print('BACK_PANEL_COMPONENT_PASS',resource,'added_faces=14','animations='+str(len(before.animations)),flush=True)

if options.install:
    # Same profile lock used by the launcher: never replace loaded resources.
    with (profile/'engage-launch.lock').open('a+b'):
        for resource,source,component,entry in pending:
            for ext in ('.mdl','.mdx'):
                targets={source.with_suffix(ext),profile/'override'/(resource+ext)}
                for target in targets:
                    relative=target.relative_to(repo)
                    saved=backup/relative;saved.parent.mkdir(parents=True,exist_ok=True)
                    shutil.copy2(target,saved)
                    shutil.copy2(assets/(resource+ext),target)
                    assert digest(target)==entry['mdl_sha256' if ext=='.mdl' else 'mdx_sha256']
            if component:
                path=component/'generation-report.json'
                saved=backup/path.relative_to(repo);saved.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,saved)
                identity=json.loads(path.read_text())
                identity['mdl_sha256']=entry['mdl_sha256'];identity['mdx_sha256']=entry['mdx_sha256']
                identity['back_panel_postprocess']=entry
                path.write_text(json.dumps(identity,indent=2))
report['pass']=True;report['installed']=options.install
(out/'report.json').write_text(json.dumps(report,indent=2))
print('BACK_PANEL_ALL_STANCES_PASS','installed='+str(options.install),flush=True)
