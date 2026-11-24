"""Retarget approved player guard corrections to the five supported NPC rigs.

Offline Blender entry point. Generates authored pose data only; no asset writes
or game launch. Preserves each NPC's original bind offsets and guard proportions.
"""
from pathlib import Path
from datetime import datetime
import hashlib
import json
import math
import shutil
import subprocess
import sys
import bpy
from mathutils import Matrix, Quaternion, Vector

repo=Path(__file__).resolve().parents[2]
out=repo/'platforms/out/npc-guard-edits'
subprocess.run(['git','-C',str(repo),'check-ignore','--quiet',str(out/'backups')],check=True)
out.mkdir(exist_ok=True)
sys.path.insert(0,str(repo/'platforms/out/kotor-animation-import/tools'))
from io_scene_kotor.format.mdl.reader import MdlReader
profile=repo/'platforms/out/k1-engage-prototype/elbow-arena/profile/override'
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
protected={p:digest(p) for p in profile.glob('*.md*')}
def nodes(n):return [n]+[v for c in n.children for v in nodes(c)]
def bind_local(model):return {n.name:Matrix.LocRotScale(Vector(n.position),Quaternion(n.orientation),Vector((n.scale,)*3)) for n in nodes(model.root_node)}
def worlds(model,local):
    result={}
    for n in nodes(model.root_node):result[n.name]=result[n.parent.name]@local[n.name] if n.parent else local[n.name]
    return result
def first_guard(model):
    result=bind_local(model)
    clip=next(a for a in model.animations if a.name=='j_guard')
    tracks={n.name:n.keyframes for n in nodes(clip.root_node)}
    for name,m in result.items():
        p,q,s=m.decompose()
        keys=tracks.get(name,{})
        if keys.get('position'):p+=Vector(keys['position'][0][1:4])
        if keys.get('orientation'):
            row=keys['orientation'][0];q=Quaternion((row[4],*row[1:4])).normalized()
        result[name]=Matrix.LocRotScale(p,q,s)
    return result
models={n:MdlReader(str(profile/(n+'.mdl'))).load() for n in ('k1jarig','k1jared','k1jadual','k1jastaff','k1jadark','k1jasith','k1jadred','k1jaddual','k1jadstaff')}
heads=json.loads((repo/'platforms/out/stance-head-applied/import-report.json').read_text())['poses']
yellow=json.loads((repo/'platforms/out/yellow-guard-edits/import-report.json').read_text())['changed_bones']
red_original=MdlReader(str(repo/'platforms/out/red-stance/motion/k1jared-before.mdl')).load()
red_before=first_guard(red_original)
red_after=first_guard(models['k1jared'])
allowed={'Rforearm_g','Lforearm_g','Rhand_g','Lhand_g','rhand','lhand','rbicep_g','lbicep_g','RbicepL_g','LbicepL_g','rradiusX','lradiusX','rhumerusX','lhumerusX'}
red_changed={n:m for n,m in red_after.items() if n in allowed and max(abs(m[i][j]-red_before[n][i][j]) for i in range(4) for j in range(4))>1e-5}
assert red_changed, 'Original red hand edit provenance missing'
rows=[]
report=[]
def append(resource,walking,name,p,q,kind):
    q.normalize()
    assert all(math.isfinite(x) for x in [*p,*q])
    rows.append('    {"'+resource+'", '+('true' if walking else 'false')+', "'+name+'", '+', '.join(f'{v:.9f}f' for v in [*p,*q])+'},\n')
    report.append({'rig':resource,'walking':walking,'bone':name,'position':list(p),'quaternion_wxyz':list(q),'kind':kind})
for resource,stance,player in (
    ('k1jadark','yellow','k1jarig'),('k1jasith','yellow','k1jarig'),
    ('k1jadred','red','k1jared'),('k1jaddual','duals','k1jadual'),
    ('k1jadstaff','double_blade','k1jastaff')):
    target=models[resource];source=models[player]
    tn={n.name:n for n in nodes(target.root_node)}
    sn={n.name:n for n in nodes(source.root_node)}
    tb=bind_local(target);sb=bind_local(source)
    tw=worlds(target,tb);sw=worlds(source,sb)
    target_guard=first_guard(target)
    source_guard=first_guard(source)
    # Integrated faces can use a separate neck/head chain instead of the
    # player's external-head hook. Transfer to the actual weighted head too.
    assert 'headhook' in tn
    head_bones=['headhook']
    # The runtime maps the visible head_g onto this hook through its CURRENT
    # Hturn_g/neck frame. A precomputed local head_g pose cannot cancel tracking.
    def parent_basis(name,source_name=None):
        a=tn[name].parent;b=sn[source_name or name].parent
        assert a and b
        if source_name is None:assert a.name==b.name,(resource,name,'incompatible parent')
        return (tw[a.name].to_quaternion().inverted()@sw[b.name].to_quaternion()).normalized()
    for walking,state in ((False,'idle'),(True,'walk')):
        h=heads[stance+'_'+state]
        # Keep the NPC neck/attachment height. Only the player's authored offset
        # and orientation relative to the corresponding bind are transferred.
        for head_bone in head_bones:
            basis=parent_basis(head_bone,'headhook')
            p=Vector(tn[head_bone].position)+basis@(Vector(h['position_xyz'])-Vector(sn['headhook'].position))
            q=basis@Quaternion(h['orientation_wxyz'])@Quaternion(sn['headhook'].orientation).inverted()@basis.inverted()@Quaternion(tn[head_bone].orientation)
            append(resource,walking,head_bone,p,q,'head bind-relative transfer')
        edits=yellow if stance=='yellow' and walking else red_changed if stance=='red' else {}
        for name,edit in edits.items():
            assert name in tn and name in sn
            basis=parent_basis(name)
            before=red_before[name] if stance=='red' else source_guard[name]
            bp,bq,_=before.decompose()
            if stance=='red':ep,eq,_=edit.decompose()
            else:ep=Vector(edit['position']);eq=Quaternion(edit['quaternion_wxyz'])
            tp,tq,_=target_guard[name].decompose()
            p=tp+basis@(ep-bp)
            q=(basis@(eq@bq.inverted())@basis.inverted()@tq).normalized()
            # Arm segment offsets remain original when the user only rotated.
            if (ep-bp).length<1e-5:p=tp
            append(resource,walking,name,p,q,'guard-relative arm/hand correction')
dest=repo/'src/game/clientcore/include/k1npcguardedits.inc'
backup=out/'backups'/datetime.now().strftime('%Y%m%d-%H%M%S')
backup.mkdir(parents=True)
if dest.exists():shutil.copy2(dest,backup/dest.name)
text='''// Approved guard corrections retargeted to existing NPC body/stance rigs.
struct AuthoredNpcGuardPose { const char* rig; bool walking; const char* bone; float px,py,pz,qw,qx,qy,qz; };
constexpr AuthoredNpcGuardPose authoredNpcGuardPoses[] = {
'''+''.join(rows)+'};\n'
assert protected=={p:digest(p) for p in protected}
dest.write_text(text)
(out/'retarget-report.json').write_text(json.dumps({'records':report,'source_asset_hashes':{p.name:h for p,h in protected.items()},'assets_unchanged':True,'red_changed_bones':sorted(red_changed),'runtime_tested':False},indent=2))
print('NPC_GUARD_EDITS_PREPARE_PASS','rigs=5',f'poses={len(report)}','red_bones='+','.join(sorted(red_changed)),flush=True)
