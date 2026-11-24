"""Guard-walk headhook editors; original lower-body keys and sparse upper keys.

Offline only. Never overwrites existing editor files or deploys game assets.
"""
from pathlib import Path
import ast
import bisect
import hashlib
import json
import math
import subprocess
import sys
import bpy
from mathutils import Matrix, Quaternion, Vector

repo=Path(__file__).resolve().parents[2]
out=repo/'platforms/out/stance-head-guard-walk'
subprocess.run(['git','-C',str(repo),'check-ignore','--quiet',str(out/'report.json')],check=True)
out.mkdir(exist_ok=True)
assert not list(out.glob('*.blend')), 'Preserve existing edits.'
previous=repo/'platforms/out/stance-head-original-keys'
source=repo/'platforms/out/stance-head-edit-pack/source'
sys.path.insert(0,str(repo/'platforms/out/kotor-animation-import/tools'))
import io_scene_kotor
from io_scene_kotor.format.mdl.reader import MdlReader
io_scene_kotor.register()
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
protected={p:digest(p) for p in previous.glob('*.blend')}
protected.update({p:digest(p) for p in (repo/'platforms/out/k1-engage-prototype/elbow-arena/profile/override').glob('*.md*')})
tree=ast.parse((repo/'platforms/tools/export-k1-stance-head-pack.py').read_text())
functions={'nodes','rest_local','worlds','clips','interpolate','retarget','replace_rotation','motion'}
exec(compile(ast.Module(body=[n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name in functions],type_ignores=[]),'existing-offline-pose-services','exec'))
models={p.stem:MdlReader(str(p)).load() for p in source.glob('*.mdl')}
def sample(model,clip,t):
    # Mirror FindAnimScale: each hop to the animation-owning supermodel scales
    # position controllers. The previous preview omitted PMBBL's 1.028 factor.
    scale=1.
    owner=model
    while clip not in owner.animations:
        scale*=owner.animscale
        owner=models[owner.supermodel.lower()]
    result=rest_local(model)
    tracks={n.name.lower():n.keyframes for n in nodes(clip.root_node)}
    for name,m in result.items():
        p,q,s=m.decompose()
        keys=tracks.get(name.lower(),{})
        if keys.get('position'):p+=scale*interpolate(keys['position'],t)
        if keys.get('orientation'):q=interpolate(keys['orientation'],t,True)
        result[name]=Matrix.LocRotScale(p,q,s)
    return result
native=models['pmbbl']
native_clips=clips(native)
native_bind=worlds(native,rest_local(native))
native_nodes={n.name:n for n in nodes(native.root_node)}
def fcurves(action):return [c for l in action.layers for s in l.strips for bag in s.channelbags for c in bag.fcurves]
def snapshot(path):
    bpy.ops.wm.open_mainfile(filepath=str(path))
    bpy.context.scene.frame_set(1)
    bpy.context.view_layer.update()
    rig=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
    return {b.name:b.parent.matrix.inverted()@b.matrix if b.parent else b.matrix.copy() for b in rig.pose.bones}
guards={s:snapshot(previous/(s+'_idle_with_head.blend')) for s in ('red','yellow')}
guards['double_blade']=snapshot(repo/'platforms/out/staff-guard-edit/kotor_staff_idle_guard_with_head.blend')
guards['duals']=retarget(models['k1jadual'],'g4r1')
results=[]
for stance,resource in {'duals':'k1jadual','red':'k1jared','yellow':'k1jarig','double_blade':'k1jastaff'}.items():
    body=models[resource]
    guard=rest_local(body)
    guard.update(guards[stance])
    guard_world=worlds(body,guard)
    upper=set()
    for n in nodes(body.root_node):
        p=n
        while p and p.name!='torso_g':p=p.parent
        if p:upper.add(n.name)
    upper.add('j_motion')
    for walking in ([False,True] if stance=='duals' else [True]):
        template=previous/(stance+('_walking' if walking else '_idle')+'_with_head.blend')
        bpy.ops.wm.open_mainfile(filepath=str(template))
        scene=bpy.context.scene
        rig=next(o for o in scene.objects if o.type=='ARMATURE')
        action=rig.animation_data.action
        clip=next(a for a in body.animations if a.name=='j_walk') if walking else None
        end=1+30*clip.length if walking else 30
        name=stance+('_guard_walking' if walking else '_idle_guard')+'_with_head'
        action.name=name
        controlled=upper if walking else {b.name for b in rig.pose.bones}
        original_lower=[(c.data_path,c.array_index,[list(k.co) for k in c.keyframe_points]) for c in fcurves(action) if c.data_path.split('"')[1] not in controlled]
        drivers={1.,end}
        if walking:
            drivers.update(float(k.co.x) for c in fcurves(action) if c.data_path.split('"')[1] in ('pelvis_g','j_motion') and c.data_path.endswith('rotation_quaternion') for k in c.keyframe_points)
        for layer in action.layers:
            for strip in layer.strips:
                for bag in strip.channelbags:
                    for c in list(bag.fcurves):
                        if c.data_path.split('"')[1] in controlled:bag.fcurves.remove(c)
        def compose(frame):
            if not walking:return {k:v.copy() for k,v in guard.items()}
            local=sample(body,clip,(frame-1)/30)
            for key in upper:local[key]=guard[key].copy()
            if stance in ('duals','double_blade'):
                replace_rotation(local,'torso_g',local['pelvis_g'].to_quaternion().inverted()@guard['pelvis_g'].to_quaternion()@guard['torso_g'].to_quaternion())
            else:motion(body,local,guard)
            if stance=='duals':
                for key in ('torso_g','torsoMid_g','torsoUpr_g'):
                    parent=next(n.parent.name for n in nodes(body.root_node) if n.name==key)
                    replace_rotation(local,key,worlds(body,local)[parent].to_quaternion().inverted()@guard_world[key].to_quaternion())
            return local
        expected={}
        for frame in sorted(drivers):
            scene.frame_set(math.floor(frame),subframe=frame-math.floor(frame))
            local=compose(frame)
            w=worlds(body,local)
            for b in rig.pose.bones:
                if b.name not in controlled:continue
                # Only spine compensation varies during the cycle. All other
                # held-guard bones (headhook included) use endpoint keys only.
                if frame not in (1.,end) and b.name not in ('torso_g','torsoMid_g','torsoUpr_g'):continue
                kw={'parent_matrix':w[b.parent.name],'parent_matrix_local':b.parent.bone.matrix_local} if b.parent else {}
                b.matrix_basis=b.bone.convert_local_to_pose(w[b.name],b.bone.matrix_local,invert=True,**kw)
                for prop in ('location','rotation_quaternion','scale'):b.keyframe_insert(prop,frame=frame,group=b.name)
            if frame in (1.,end):expected[str(frame)]={k:[list(row) for row in v] for k,v in w.items() if k in rig.pose.bones}
        for c in fcurves(action):
            if c.data_path.split('"')[1] in controlled:
                for k in c.keyframe_points:k.interpolation='LINEAR'
        after_lower=[(c.data_path,c.array_index,[list(k.co) for k in c.keyframe_points]) for c in fcurves(action) if c.data_path.split('"')[1] not in controlled]
        assert original_lower==after_lower
        assert all(len(c.keyframe_points)==2 for c in fcurves(action) if '"headhook"' in c.data_path)
        scene.frame_start=1;scene.frame_end=math.ceil(end)
        action['source_policy']='Settled held guard with existing forward gait; original lower keys unchanged, spine compensation keyed only at source driver times. No uniform resampling.'
        scene.frame_set(1)
        bpy.context.view_layer.update()
        rig.data.bones.active=rig.data.bones['headhook']
        for b in rig.pose.bones:b.select=b.name=='headhook'
        path=out/(name+'.blend')
        bpy.ops.wm.save_as_mainfile(filepath=str(path))
        bpy.ops.wm.open_mainfile(filepath=str(path))
        rig=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
        maximum=0.
        for frame,pose in expected.items():
            f=float(frame);bpy.context.scene.frame_set(math.floor(f),subframe=f-math.floor(f))
            bpy.context.view_layer.update()
            maximum=max(maximum,max(abs(rig.pose.bones[k].matrix[i][j]-m[i][j]) for k,m in pose.items() for i in range(4) for j in range(4)))
        assert maximum<1e-5,maximum
        results.append({'file':path.name,'lower_keys_unchanged':True,'headhook_keys_per_channel':2,'driver_times':sorted(drivers),'saved_pose_max_error':maximum,'guard_source':'native g4r1 at .13s, inherited position scale1.028' if stance=='duals' else 'saved user guard editor frame1','expected':expected})
        print('GUARD_WALK_EDITOR_PASS',path.name,maximum,flush=True)
assert protected=={p:digest(p) for p in protected}
(out/'report.json').write_text(json.dumps({'files':results,'preserved_source_hashes':{p.name:v for p,v in protected.items()},'runtime_tested':False},indent=2))
print('GUARD_WALK_PACK_PASS',len(results),flush=True)
