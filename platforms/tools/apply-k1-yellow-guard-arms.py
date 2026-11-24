"""Import saved yellow guard-walk arm/finger edits without changing the editors.

Run with installed Blender --background --factory-startup --disable-autoexec
--python-exit-code 1 --python this-file. Does not launch the game.
"""
from pathlib import Path
from datetime import datetime
import hashlib
import json
import shutil
import subprocess
import bpy

repo=Path(__file__).resolve().parents[2]
out=repo/'platforms/out/yellow-guard-edits'
subprocess.run(['git','-C',str(repo),'check-ignore','--quiet',str(out/'backups')],check=True)
out.mkdir(exist_ok=True)
backup=out/'backups'/datetime.now().strftime('%Y%m%d-%H%M%S')
backup.mkdir(parents=True)
source=repo/'platforms/out/stance-head-guard-walk/yellow_guard_walking_with_head.blend'
baseline=source.with_suffix('.blend1')
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
source_hash=digest(source)
baseline_hash=digest(baseline)
shutil.copy2(source,backup/source.name)
shutil.copy2(baseline,backup/baseline.name)
def pose(path,frame=1):
    bpy.ops.wm.open_mainfile(filepath=str(path))
    bpy.context.scene.frame_set(frame)
    bpy.context.view_layer.update()
    rig=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
    return {b.name:b.parent.matrix.inverted()@b.matrix if b.parent else b.matrix.copy() for b in rig.pose.bones}
original=pose(baseline)
edited=pose(source)
changed={n:m for n,m in edited.items() if n!='headhook' and max(abs(m[i][j]-original[n][i][j]) for i in range(4) for j in range(4))>1e-5}
allowed={'rbicep_g','lbicep_g','RbicepL_g','LbicepL_g','Rforearm_g','Lforearm_g',
         'Rhand_g','Lhand_g','rhand','lhand','rradiusX','lradiusX','rhumerusX','lhumerusX'}
assert changed, 'No non-head guard-walk edits found'
assert all(n in allowed or 'Fngr' in n or 'Thumb' in n for n in changed), list(changed)
rig=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
times={1.,float(bpy.context.scene.frame_end)}
for layer in rig.animation_data.action.layers:
    for strip in layer.strips:
        for bag in strip.channelbags:
            for c in bag.fcurves:
                if c.data_path.split('"')[1] in changed:times.update(float(k.co.x) for k in c.keyframe_points)
ordered=sorted(times)
times.update((a+b)/2 for a,b in zip(ordered,ordered[1:]))
for frame in sorted(times):
    bpy.context.scene.frame_set(int(frame),subframe=frame-int(frame))
    bpy.context.view_layer.update()
    for name,expected in changed.items():
        b=rig.pose.bones[name]
        local=b.parent.matrix.inverted()@b.matrix if b.parent else b.matrix
        assert max(abs(local[i][j]-expected[i][j]) for i in range(4) for j in range(4))<1e-5,(name,'animated edits require full curve import')
dest=repo/'src/game/clientcore/include/k1yellowguardarms.inc'
if dest.exists():shutil.copy2(dest,backup/dest.name)
text='''// User-authored yellow guard-walking edits; head is handled separately.
struct AuthoredYellowGuardBonePose { const char* name; float px,py,pz,qw,qx,qy,qz; };
constexpr AuthoredYellowGuardBonePose authoredYellowGuardWalk[] = {
'''
report={}
for name,m in sorted(changed.items()):
    p,q,s=m.decompose()
    assert max(abs(v-1) for v in s)<1e-5
    q.normalize()
    values=[*p,*q]
    encoded=[float(f'{v:.9f}') for v in values]
    assert max(abs(a-b) for a,b in zip(values,encoded))<1e-8
    text+='    {"'+name+'", '+', '.join(f'{v:.9f}f' for v in values)+'},\n'
    report[name]={'position':list(p),'quaternion_wxyz':list(q)}
text+='};\n'
assert digest(source)==source_hash and digest(baseline)==baseline_hash
dest.write_text(text)
(out/'import-report.json').write_text(json.dumps({'source_sha256':source_hash,'baseline_sha256':baseline_hash,
    'source_file':str(source.relative_to(repo)),'baseline_file':str(baseline.relative_to(repo)),
    'walking_only':True,'changed_bones':report,'sampled_frames':sorted(times),
    'source_files_unchanged':True,'runtime_tested':False},indent=2))
print('YELLOW_GUARD_ARMS_IMPORT_PASS',len(changed),sorted(changed),flush=True)
