"""Import saved headhook transforms for the four player guards and guard-walks.

Run using Blender --background --factory-startup --disable-autoexec
--python-exit-code 1 --python this-file. No game launch or animation-file writes.
"""
from pathlib import Path
from datetime import datetime
import hashlib
import json
import shutil
import subprocess
import bpy

repo=Path(__file__).resolve().parents[2]
out=repo/'platforms/out/stance-head-applied'
subprocess.run(['git','-C',str(repo),'check-ignore','--quiet',str(out/'backups')],check=True)
out.mkdir(exist_ok=True)
backup=out/'backups'/datetime.now().strftime('%Y%m%d-%H%M%S')
backup.mkdir(parents=True)
old=repo/'platforms/out/stance-head-original-keys'
walk=repo/'platforms/out/stance-head-guard-walk'
paths={
 'yellow_idle':old/'yellow_idle_with_head.blend',
 'red_idle':old/'red_idle_with_head.blend',
 'duals_idle':walk/'duals_idle_guard_with_head.blend',
 'double_blade_idle':repo/'platforms/out/staff-guard-edit/kotor_staff_idle_guard_with_head.blend'}
for stance in ('yellow','red','duals','double_blade'):
    paths[stance+'_walk']=walk/(stance+'_guard_walking_with_head.blend')
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
hashes={name:digest(p) for name,p in paths.items()}
profile=repo/'platforms/out/k1-engage-prototype/elbow-arena/profile/override'
assets={p:digest(p) for p in profile.glob('*.md*')}
report={}
poses={}
for name,path in paths.items():
    shutil.copy2(path,backup/(name+'.blend'))
    bpy.ops.wm.open_mainfile(filepath=str(path))
    scene=bpy.context.scene
    rig=next(o for o in scene.objects if o.type=='ARMATURE')
    assert 'HEAD_EDIT' not in rig.pose.bones
    action=rig.animation_data.action
    times={1.,float(scene.frame_end)}
    for layer in action.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                for curve in bag.fcurves:
                    if '"headhook"' in curve.data_path:
                        times.update(float(k.co.x) for k in curve.keyframe_points)
    values=[]
    for frame in sorted(times):
        integral=int(frame)
        scene.frame_set(integral,subframe=frame-integral)
        bpy.context.view_layer.update()
        b=rig.pose.bones['headhook']
        local=b.parent.matrix.inverted()@b.matrix if b.parent else b.matrix.copy()
        p,q,s=local.decompose()
        assert max(abs(v-1) for v in s)<1e-5, (name,'head scaling is outside this import')
        values.append((p.copy(),q.normalized()))
    p,q=values[0]
    assert max((p-p2).length for p2,q2 in values)<1e-5, (name,'head position varies; hold import would lose animation')
    assert max(1-abs(q.dot(q2)) for p2,q2 in values)<1e-6, (name,'head rotation varies; hold import would lose animation')
    floats=[*p,*q]
    encoded=[float(f'{v:.9f}') for v in floats]
    assert max(abs(a-b) for a,b in zip(floats,encoded))<1e-8
    poses[name]=encoded
    report[name]={'source':str(path.relative_to(repo)),'sha256':hashes[name],
        'position_xyz':list(p),'orientation_wxyz':list(q),'sampled_frames':sorted(times),
        'held_pose_verified':True,'backup':str((backup/(name+'.blend')).relative_to(repo))}
dest=repo/'src/game/clientcore/include/k1guardheadedits.inc'
if dest.exists():shutil.copy2(dest,backup/dest.name)
text='''// Player-authored headhook poses: yellow, red, duals, double-blade.
// Each row contains standing guard then guard-walking. Positions are local.
struct AuthoredGuardHeadPose { float px,py,pz,qw,qx,qy,qz; };
constexpr AuthoredGuardHeadPose authoredGuardHeads[4][2] = {
'''
for stance in ('yellow','red','duals','double_blade'):
    text+='    { // '+stance+'\n'
    for state in ('idle','walk'):
        text+='        {'+', '.join(f'{v:.9f}f' for v in poses[stance+'_'+state])+'},\n'
    text+='    },\n'
text+='};\n'
assert hashes=={name:digest(p) for name,p in paths.items()}
assert assets=={p:digest(p) for p in assets}
dest.write_text(text)
(out/'import-report.json').write_text(json.dumps({'poses':report,'source_blends_unchanged':True,
    'animation_assets_unchanged':True,'generated_include_sha256':digest(dest),'runtime_tested':False},indent=2))
print('GUARD_HEAD_EDITS_IMPORT_PASS',len(poses),'sources preserved; animation assets unchanged',flush=True)
