"""Build a separate player stance from the user's Purple guards and Red attacks.

Run in installed Blender background. Never saves the editing scenes or changes
completed stance assets. Authored key times are retained; no uniform resampling.
Remove only template-identical half-frame spine keys bracketed by edited poses.
"""
from pathlib import Path
from datetime import datetime
import copy
import hashlib
import json
import math
import shutil
import subprocess
import sys
import bpy
from mathutils import Matrix, Quaternion, Vector

repo=Path(__file__).resolve().parents[2]
out=repo/'platforms/out/purple-stance'
subprocess.run(['git','-C',str(repo),'check-ignore','--quiet',str(out/'assets')],check=True)
out.mkdir(exist_ok=True)
assets=out/'assets';assets.mkdir(exist_ok=True)
backup=out/'backups'/datetime.now().strftime('%Y%m%d-%H%M%S')
backup.mkdir(parents=True)
sys.path.insert(0,str(repo/'platforms/out/kotor-animation-import/tools'))
from io_scene_kotor.format.mdl.reader import MdlReader
from io_scene_kotor.format.mdl.writer import MdlWriter
from io_scene_kotor.scene.animation import Animation
from io_scene_kotor.scene.animnode import AnimationNode
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def nodes(n):return [n]+[v for c in n.children for v in nodes(c)]
def rest_local(model):return {n.name:Matrix.LocRotScale(Vector(n.position),Quaternion(n.orientation),Vector((n.scale,)*3)) for n in nodes(model.root_node)}
def globals_for(model,local):
    result={}
    for n in nodes(model.root_node):result[n.name]=result[n.parent.name]@local[n.name] if n.parent else local[n.name]
    return result
red=repo/'platforms/out/red-stance/assets/k1jared.mdl'
profile=repo/'platforms/out/k1-engage-prototype/elbow-arena/profile/override'
protected={p:digest(p) for p in profile.glob('*.md*')}
protected.update({p:digest(p) for p in red.parent.glob('k1jared.md*')})
model=MdlReader(str(red)).load()
original=copy.deepcopy(model)
lookup={n.name:n for n in nodes(model.root_node)}
rest=rest_local(model)
rest_world=globals_for(model,rest)
paths={'j_guard':repo/'platforms/out/custom-stance-base/purple_idle_guard.blend',
       'j_purpwalk':repo/'platforms/out/custom-stance-base/purple_guard_walking.blend'}
base_paths={'j_guard':repo/'platforms/out/custom-stance-base/custom_idle_guard.blend',
            'j_purpwalk':repo/'platforms/out/custom-stance-base/custom_guard_walking.blend'}
source_hashes={name:digest(path) for name,path in paths.items()}
def rig_in_scene():
    rigs=[o for o in bpy.context.scene.objects if o.type=='ARMATURE']
    assert len(rigs)==1
    return rigs[0]
def curve_list(action):return [c for l in action.layers for s in l.strips for bag in s.channelbags for c in bag.fcurves]
def skeleton(rig):return {b.name:(b.parent.name if b.parent else None,[list(row) for row in b.matrix_local]) for b in rig.data.bones}
source_info={}
animations={}
sampled_expected={}
walk_key_repairs=[]
for name,path in paths.items():
    bpy.ops.wm.open_mainfile(filepath=str(base_paths[name]))
    baseline=skeleton(rig_in_scene())
    baseline_curves={(c.data_path,c.array_index):{float(k.co.x):float(k.co.y) for k in c.keyframe_points}
                     for c in curve_list(rig_in_scene().animation_data.action)}
    bpy.ops.wm.open_mainfile(filepath=str(path))
    scene=bpy.context.scene;rig=rig_in_scene()
    assert skeleton(rig)==baseline,(name,'rest skeleton was changed')
    assert all(b.rotation_mode=='QUATERNION' for b in rig.pose.bones)
    assert set(b.name for b in rig.pose.bones)<=lookup.keys()
    fps=scene.render.fps/scene.render.fps_base
    start=float(scene.frame_start);end=float(scene.frame_end)
    assert fps>0 and end>start
    if name=='j_purpwalk':
        # The edited whole-frame spine poses can coexist with the template's
        # old half-frame compensation keys. Keep every authored key and only
        # discard a half-frame key proven identical to the original template,
        # with edited whole-frame rotations on both sides. Work in memory only.
        curves=curve_list(rig.animation_data.action)
        for bone in ('torsoMid_g','torsoUpr_g'):
            prop='pose.bones["'+bone+'"].rotation_quaternion'
            channels=sorted((c for c in curves if c.data_path==prop),key=lambda c:c.array_index)
            if len(channels)!=4:continue
            current=[{float(k.co.x):float(k.co.y) for k in c.keyframe_points} for c in channels]
            template_channels=[baseline_curves.get((prop,i),{}) for i in range(4)]
            times=set.intersection(*(set(c) for c in current))
            def edited(t):
                return all(t in c for c in current+template_channels) and max(abs(current[i][t]-template_channels[i][t]) for i in range(4))>.001
            stale=[t for t in sorted(times) if abs(t-math.floor(t)-.5)<1e-5
                   and all(t in c for c in template_channels)
                   and max(abs(current[i][t]-template_channels[i][t]) for i in range(4))<1e-6
                   and edited(math.floor(t)) and edited(math.ceil(t))]
            if not stale:continue
            for c in channels:
                for key in list(c.keyframe_points):
                    if float(key.co.x) in stale:c.keyframe_points.remove(key,fast=True)
                c.update()
            walk_key_repairs.append({'bone':bone,'property':'rotation_quaternion','removed_template_times':stale})
            print('PURPLE_STALE_TEMPLATE_KEYS_REMOVED',bone,len(stale),'per quaternion channel',flush=True)
    tracks={b.name:{} for b in rig.pose.bones}
    for c in curve_list(rig.animation_data.action):
        assert c.data_path.startswith('pose.bones["'),c.data_path
        bone=c.data_path.split('"')[1];prop=c.data_path.rsplit('.',1)[1]
        assert prop in ('location','rotation_quaternion','scale'),prop
        tracks[bone].setdefault(prop,set()).update(float(k.co.x) for k in c.keyframe_points)
    all_times={start,end}|{f for props in tracks.values() for times in props.values() for f in times}
    assert min(all_times)>=start-1e-4 and max(all_times)<=end+1e-4
    ordered=sorted(all_times)
    checks=all_times|{(a+b)/2 for a,b in zip(ordered,ordered[1:])}
    worlds={};locals_at={}
    for frame in sorted(checks):
        scene.frame_set(math.floor(frame),subframe=frame-math.floor(frame));bpy.context.view_layer.update()
        world={b.name:rig.matrix_world@b.matrix for b in rig.pose.bones}
        full={}
        local={}
        for node in nodes(model.root_node):
            parent=full[node.parent.name] if node.parent else Matrix.Identity(4)
            full[node.name]=world[node.name] if node.name in world else parent@rest[node.name]
            if node.name in world:
                m=parent.inverted()@full[node.name]
                assert max(abs(s-1) for s in m.to_scale())<1e-5,(name,node.name,'non-unit scale')
                local[node.name]=m
        worlds[frame]={n:full[n].copy() for n in world};locals_at[frame]=local
    clip=Animation(name);clip.length=(end-start)/fps;clip.transtime=.1;clip.animroot=model.root_node.name
    counts={}
    def build(node,parent=None):
        a=AnimationNode(node.name);a.node_number=node.node_number;a.parent=parent
        if node.name in tracks:
            props=tracks[node.name]
            # Full guards own the whole pose, including constant bones that had
            # no editable channel. Only endpoint keys are needed for those.
            for prop,kind in (('location','position'),('rotation_quaternion','orientation')):
                times=sorted(props.get(prop,{start,end}))
                rows=[];previous=None
                for frame in times:
                    p,q,_=locals_at[frame][node.name].decompose();q.normalize()
                    t=(frame-start)/fps
                    if kind=='position':rows.append([t,*(p-Vector(node.position))])
                    else:
                        if previous is not None and q.dot(previous)<0:q.negate()
                        rows.append([t,q.x,q.y,q.z,q.w]);previous=q.copy()
                a.keyframes[kind]=rows
                counts[node.name+':'+kind]=len(rows)
        a.children=[build(c,a) for c in node.children]
        a.animated=bool(a.keyframes) or any(c.animated for c in a.children)
        return a
    clip.root_node=build(model.root_node)
    animations[name]=clip
    sampled_expected[name]={'worlds':worlds,'start':start,'fps':fps}
    source_info[name]={'file':str(path.relative_to(repo)),'sha256':source_hashes[name],
        'fps':fps,'frames':[start,end],'seconds':clip.length,'source_controller_times':sorted(all_times),'controller_key_counts':counts}
    shutil.copy2(path,backup/path.name)
    print('PURPLE_GUARD_IMPORTED',name,'length',clip.length,'source_times',len(all_times),flush=True)
reverse=copy.deepcopy(animations['j_purpwalk']);reverse.name='j_purpbackwalk'
for node in nodes(reverse.root_node):
    for kind,rows in node.keyframes.items():
        node.keyframes[kind]=[[reverse.length-row[0],*row[1:]] for row in reversed(rows)]
animations[reverse.name]=reverse
model.name='k1japurp'
model.animations=[a for a in model.animations if a.name!='j_guard']+list(animations.values())
def retain(node):
    children=[retain(c) for c in node.children]
    node.animated=bool(node.keyframes) or any(children)
    return node.animated
for clip in model.animations:retain(clip.root_node)
for n in nodes(model.root_node):
    if hasattr(n,'facelist') and n.facelist:
        n.facelist.normals=[]
        for a,b,c in n.facelist.vertices:
            normal=(Vector(n.verts[b])-Vector(n.verts[a])).cross(Vector(n.verts[c])-Vector(n.verts[a]))
            n.facelist.normals.append(list(normal.normalized() if normal.length>1e-10 else Vector(n.normals[a])))
dest=assets/'k1japurp.mdl'
for p in assets.glob('k1japurp.md*'):shutil.copy2(p,backup/p.name)
MdlWriter(str(dest),model,False,False,False).save()
back=MdlReader(str(dest)).load()
back_clips={a.name:a for a in back.animations}
unchanged=0
for clip in original.animations:
    if clip.name=='j_guard':continue
    other=back_clips[clip.name]
    assert abs(clip.length-other.length)<1e-6
    aa=nodes(clip.root_node);bb=nodes(other.root_node)
    assert len(aa)==len(bb)
    assert all(a.name==b.name and a.keyframes==b.keyframes for a,b in zip(aa,bb)),clip.name
    unchanged+=1
for a,b in zip(nodes(original.root_node),nodes(back.root_node)):
    assert a.name==b.name
    for attr in ('position','orientation','scale','verts','normals','weights','uv1','texture1'):
        assert getattr(a,attr,None)==getattr(b,attr,None),(a.name,attr)
def interpolate(rows,t):
    import bisect
    i=max(0,min(len(rows)-2,bisect.bisect_right([r[0] for r in rows],t)-1))
    a=rows[i];b=rows[min(i+1,len(rows)-1)]
    u=max(0,min(1,(t-a[0])/(b[0]-a[0]))) if b[0]>a[0] else 0
    return Vector(a[1:4]).lerp(Vector(b[1:4]),u)
def sampled(clip,t):
    result=rest_local(back)
    for n in nodes(clip.root_node):
        p,q,s=result[n.name].decompose()
        if n.keyframes.get('position'):p+=interpolate(n.keyframes['position'],t)
        if n.keyframes.get('orientation'):
            rows=n.keyframes['orientation']
            import bisect
            i=max(0,min(len(rows)-2,bisect.bisect_right([r[0] for r in rows],t)-1))
            a=rows[i];b=rows[min(i+1,len(rows)-1)]
            u=max(0,min(1,(t-a[0])/(b[0]-a[0]))) if b[0]>a[0] else 0
            q=Quaternion((a[4],*a[1:4])).normalized().slerp(Quaternion((b[4],*b[1:4])).normalized(),u)
        result[n.name]=Matrix.LocRotScale(p,q,s)
    return globals_for(back,result)
maximum=0.;reverse_error=0.
for name,checks in sampled_expected.items():
    for frame,expected in checks['worlds'].items():
        t=(frame-checks['start'])/checks['fps']
        actual=sampled(back_clips[name],t)
        maximum=max(maximum,max(abs(actual[n][i][j]-m[i][j]) for n,m in expected.items() for i in range(4) for j in range(4)))
        if name=='j_purpwalk':
            reversed_pose=sampled(back_clips['j_purpbackwalk'],back_clips[name].length-t)
            reverse_error=max(reverse_error,max(abs(actual[n][i][j]-reversed_pose[n][i][j]) for n in actual for i in range(4) for j in range(4)))
assert maximum<.0002,maximum
assert reverse_error<.00002,reverse_error
assert protected=={p:digest(p) for p in protected}
assert source_hashes=={name:digest(path) for name,path in paths.items()}
report={'component_pass':True,'resource':'k1japurp','mdl_sha256':digest(dest),'mdx_sha256':digest(dest.with_suffix('.mdx')),
    'sources':source_info,'red_source_sha256':digest(red),'unchanged_red_clips':unchanged,
    'max_source_world_matrix_error':maximum,'reverse_walk_matrix_error':reverse_error,
    'walk_key_repairs':walk_key_repairs,
    'protected_resources':{p.name:h for p,h in protected.items()},'runtime_tested':False}
(out/'generation-report.json').write_text(json.dumps(report,indent=2))
print('PURPLE_COMPONENT_EXPORT_PASS',unchanged,'red clips unchanged; source error',maximum,'reverse error',reverse_error,flush=True)
