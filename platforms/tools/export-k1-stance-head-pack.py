"""Local head-editing scenes; never deploys assets or launches the game.

Run with installed Blender --background --factory-startup --disable-autoexec
--python-exit-code 1 --python this-file. Existing editable files are preserved.
"""
from pathlib import Path
import bisect
import copy
import hashlib
import importlib.util
import json
import math
import re
import shutil
import subprocess
import sys
import bpy
from mathutils import Matrix, Quaternion, Vector

repo = Path(__file__).resolve().parents[2]
out = repo / 'platforms/out/stance-head-edit-pack'
subprocess.run(['git','-C',str(repo),'check-ignore','--quiet',str(out/'source')],check=True)
out.mkdir(exist_ok=True)
assert not list(out.glob('*.blend')), 'Preserve existing editable files.'
source = out/'source'
source.mkdir(exist_ok=True)
for folder in ('platforms/out/kotor-animation-import/tools','tools','platforms/tools'):
    sys.path.insert(0,str(repo/folder))
import io_scene_kotor
from io_scene_kotor.constants import ImportOptions
from io_scene_kotor.format.mdl.reader import MdlReader
from io_scene_kotor.scene import armature
from padglyphs import erf_resource
spec=importlib.util.spec_from_file_location('owned',repo/'platforms/out/k1-engage-prototype/dark-jedi/inspect-owned.py')
owned=importlib.util.module_from_spec(spec)
spec.loader.exec_module(owned)
profile=repo/'platforms/out/k1-engage-prototype/elbow-arena/profile/override'
def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()
protected={p:digest(p) for p in profile.glob('*.md*')}
sources={}
models={}
def load(name):
    name=name.lower()
    if name in models:return models[name]
    for kind,ext in ((2002,'mdl'),(3008,'mdx')):
        p=source/(name+'.'+ext)
        if (profile/p.name).exists():shutil.copy2(profile/p.name,p)
        else:
            try:p.write_bytes(owned.resource(name,kind))
            except KeyError:
                if ext=='mdx':continue
                raise
        sources[p.name]=digest(p)
    model=MdlReader(str(source/(name+'.mdl'))).load()
    models[name]=model
    if model.supermodel and model.supermodel.lower() not in ('null',name):load(model.supermodel)
    return model
def nodes(n):
    return [n]+[v for c in n.children for v in nodes(c)]
def rest_local(model):
    return {n.name:Matrix.LocRotScale(Vector(n.position),Quaternion(n.orientation),Vector((n.scale,)*3)) for n in nodes(model.root_node)}
def worlds(model,local):
    result={}
    for n in nodes(model.root_node):result[n.name]=result[n.parent.name]@local[n.name] if n.parent else local[n.name].copy()
    return result
def clips(model):
    result={a.name:a for a in model.animations}
    if model.supermodel and model.supermodel.lower()!='null':
        for k,v in clips(models[model.supermodel.lower()]).items():result.setdefault(k,v)
    return result
def interpolate(keys,t,quat=False):
    i=max(0,min(len(keys)-2,bisect.bisect_right([r[0] for r in keys],t)-1))
    a,b=keys[i],keys[min(i+1,len(keys)-1)]
    u=max(0,min(1,(t-a[0])/(b[0]-a[0]))) if b[0]>a[0] else 0
    if quat:return Quaternion((a[4],*a[1:4])).normalized().slerp(Quaternion((b[4],*b[1:4])).normalized(),u)
    return Vector(a[1:4]).lerp(Vector(b[1:4]),u)
def sample(model,clip,t):
    result=rest_local(model)
    tracks={n.name:n.keyframes for n in nodes(clip.root_node)}
    for name,m in result.items():
        p,q,s=m.decompose()
        keys=tracks.get(name,{})
        if keys.get('position'):p+=interpolate(keys['position'],t)
        if keys.get('orientation'):q=interpolate(keys['orientation'],t,True)
        result[name]=Matrix.LocRotScale(p,q,s)
    return result
native=load('pmbbl')
native_clips=clips(native)
native_bind=worlds(native,rest_local(native))
native_nodes={n.name:n for n in nodes(native.root_node)}
def retarget(body,clip_name):
    # Same model-space bind correction and parent-offset policy as CaptureNativePose.
    native_local=sample(native,native_clips[clip_name],.130001)
    posed=worlds(native,native_local)
    bind=rest_local(body)
    target=worlds(body,bind)
    result={}
    rotations={}
    for n in nodes(body.root_node):
        p,q,s=bind[n.name].decompose()
        parent_q=rotations[n.parent.name] if n.parent else Quaternion()
        if n.name in posed:
            q=(parent_q.inverted()@posed[n.name].to_quaternion()@native_bind[n.name].to_quaternion().inverted()@target[n.name].to_quaternion()).normalized()
            old=native_nodes[n.name]
            if n.parent and old.parent and n.parent.name==old.parent.name:
                offset=native_local[n.name].translation-Vector(old.position)
                p+=target[n.parent.name].to_quaternion().inverted()@native_bind[old.parent.name].to_quaternion()@offset
        result[n.name]=Matrix.LocRotScale(p,q,s)
        rotations[n.name]=parent_q@q
    return result
def replace_rotation(local,name,q):
    p,_,s=local[name].decompose()
    local[name]=Matrix.LocRotScale(p,q.normalized(),s)
def motion(body,local,reference=None):
    if not reference:return local # zero yaw and pitch, no overlay compensation
    w=worlds(body,local)
    mixed=(w['pelvis_g'].to_quaternion()@reference['pelvis_g'].to_quaternion().inverted()@w['j_motion'].to_quaternion()).normalized()
    f,r=mixed@Vector((0,1,0)),mixed@Vector((1,0,0))
    wrap=lambda a:(a+math.pi)%(2*math.pi)-math.pi
    pitch=wrap(math.atan2(f.z,math.hypot(f.x,f.y)))
    turn=wrap(-(math.atan2(f.y,f.x)-math.pi/2))
    roll=wrap(-math.atan2(r.z,math.hypot(r.x,r.y)))
    axes=rest_local(body)['j_motion'].to_quaternion()
    for name,py,yr in (('torso_g',.4,.45),('torsoMid_g',.4,.35),('torsoUpr_g',.2,.2)):
        correction=Quaternion(axes@Vector((0,0,1)),roll*yr)@Quaternion(axes@Vector((0,1,0)),pitch*py)@Quaternion(axes@Vector((1,0,0)),turn*yr)
        replace_rotation(local,name,local[name].to_quaternion()@correction)
    return local
resources={'red':'k1jared','yellow':'k1jarig','duals':'k1jadual','double_blade':'k1jastaff'}
for resource in resources.values():load(resource)
single=load('w_lghtsbr_001')
staff=load('w_dblsbr_001')
appearance=owned.twoda(owned.resource('appearance',2017))
heads=owned.twoda(owned.resource('heads',2017))
head=load(heads[appearance['168']['normalhead']]['head'].lower())
for model in list(models.values()):
    for n in nodes(model.root_node):
        for field in ('bitmap','bitmap2','texture1'):
            name=getattr(n,field,'')
            if isinstance(name,str) and name.lower() not in ('','null'):
                data=erf_resource(str(owned.game/'TexturePacks/swpc_tex_tpa.erf'),name.lower(),3007)
                if data:(source/(name.lower()+'.tpc')).write_bytes(data)
io_scene_kotor.register()
def curves(action):
    slot=action.slots[0] if action.slots else action.slots.new(id_type='OBJECT',name=action.name)
    layer=action.layers[0] if action.layers else action.layers.new('Animation')
    strip=layer.strips[0] if layer.strips else layer.strips.new(type='KEYFRAME')
    return strip.channelbag(slot,ensure=True).fcurves
if not hasattr(bpy.types.Action,'fcurves'):bpy.types.Action.fcurves=property(curves)
report=[]
for stance,resource in resources.items():
    body=models[resource]
    animations=clips(body)
    idle=retarget(body,'pause1')
    guard=retarget(body,'g4r1' if stance=='duals' else 'g3r1') if stance in ('duals','double_blade') else sample(body,animations['j_guard'],0)
    if stance=='double_blade':
        values=re.findall(r'[-+]?\d+\.\d+(?=f)',(repo/'src/game/clientcore/include/k1staffguardhead.inc').read_text())
        replace_rotation(guard,'headhook',Quaternion(tuple(map(float,values))))
    upper=set()
    for n in nodes(body.root_node):
        p=n
        while p and p.name!='torso_g':p=p.parent
        if p:upper.add(n.name)
    upper.add('j_motion')
    def displayed(t,walking,held):
        if not walking:
            local={k:v.copy() for k,v in (guard if held else idle).items()}
            return motion(body,local)
        local=sample(body,animations['j_walk'],t)
        if held:
            for name in upper:local[name]=guard[name].copy()
            if stance in ('duals','double_blade'):
                replace_rotation(local,'torso_g',local['pelvis_g'].to_quaternion().inverted()@guard['pelvis_g'].to_quaternion()@guard['torso_g'].to_quaternion())
            else:motion(body,local,guard)
        if stance=='duals':
            ref=worlds(body,guard if held else idle)
            for name in ('torso_g','torsoMid_g','torsoUpr_g'):
                parent=next(n.parent.name for n in nodes(body.root_node) if n.name==name)
                replace_rotation(local,name,worlds(body,local)[parent].to_quaternion().inverted()@ref[name].to_quaternion())
            if not held:
                for name in local:
                    if name in ('rhand','lhand') or 'Fngr' in name or 'Thumb' in name:local[name]=guard[name].copy()
                for wrist,hook in (('Rhand_g','rhand'),('Lhand_g','lhand')):
                    w=worlds(body,local)
                    arc=(w[hook].to_quaternion()@Vector((0,0,1))).rotation_difference(Vector((0,1,.35)).normalized())
                    parent=next(n.parent.name for n in nodes(body.root_node) if n.name==wrist)
                    replace_rotation(local,wrist,w[parent].to_quaternion().inverted()@arc@w[wrist].to_quaternion())
        return local
    for walking in ([True] if stance=='double_blade' else [False,True]):
        name=f'{stance}_{"walking" if walking else "idle"}_with_head'
        bpy.ops.wm.read_factory_settings(use_empty=True)
        scene=bpy.context.scene
        scene.render.fps=60
        options=ImportOptions()
        options.import_animations=False
        options.import_walkmeshes=False
        options.build_armature=False
        options.build_materials=True
        options.texture_search_paths=[str(source)]
        options.lightmap_search_paths=[]
        character=bpy.data.collections.new('01 Character - editable rig')
        scene.collection.children.link(character)
        mesh_model=copy.deepcopy(body)
        mesh_model.animations=[]
        root=mesh_model.add_to_collection(character,options)
        bpy.context.view_layer.update()
        rig=armature.rebuild_armature(root)
        assert rig
        rig.name=stance.upper()+' - Pose Mode - headhook'
        rig.show_in_front=True
        bpy.context.view_layer.objects.active=rig
        bpy.ops.object.mode_set(mode='EDIT')
        for b in rig.data.edit_bones:
            b.length=.06
            b.matrix=bpy.data.objects[b.name].matrix_world
        bpy.ops.object.mode_set(mode='OBJECT')
        for b in rig.pose.bones:b.rotation_mode='QUATERNION'
        for o in character.objects:
            if o.type!='MESH' or not o.kb.render:o.hide_set(True);o.hide_render=True
        rig.animation_data_create()
        attachments=bpy.data.collections.new('02 Head and sabers - preview attachments')
        scene.collection.children.link(attachments)
        def attach(model,bone,label):
            clean=copy.deepcopy(model)
            clean.animations=[]
            obj=clean.add_to_collection(attachments,options)
            anchor=bpy.data.objects.new(label,None)
            attachments.objects.link(anchor)
            c=anchor.constraints.new('COPY_TRANSFORMS')
            c.target=rig;c.subtarget=bone
            obj.parent=anchor;obj.matrix_parent_inverse=Matrix.Identity(4)
            anchor.hide_set(True)
            for o in [obj]+list(obj.children_recursive):
                if o.type!='MESH' or not o.kb.render or (model!=head and o.kb.meshtype=='LIGHTSABER'):
                    o.hide_set(True);o.hide_render=True
        attach(head,'headhook','Head follows headhook')
        attach(staff if stance=='double_blade' else single,'rhand','Right saber')
        if stance=='duals':attach(single,'lhand','Left saber')
        frames=round(animations['j_walk'].length*60)+1 if walking else 61
        scene.frame_start=1;scene.frame_end=frames
        maximum=0.
        actions=[]
        for held in (True,False):
            action=bpy.data.actions.new(('01 Guarded ' if held else '02 Relaxed ')+('walk' if walking else 'idle'))
            action.use_fake_user=True
            action['head_edit_stance']=stance
            action['head_edit_state']='guarded' if held else 'relaxed'
            action['body_pose_policy']='steady forward, zero yaw/pitch; runtime smoothing/transitions excluded'
            rig.animation_data.action=action
            targets={}
            for frame in (range(1,frames+1) if walking else (1,frames)):
                scene.frame_set(frame)
                local=displayed((frame-1)/60,walking,held)
                w=worlds(body,local)
                for b in rig.pose.bones:
                    kw={'parent_matrix':w[b.parent.name],'parent_matrix_local':b.parent.bone.matrix_local} if b.parent else {}
                    b.matrix_basis=b.bone.convert_local_to_pose(w[b.name],b.bone.matrix_local,invert=True,**kw)
                    for prop in ('location','rotation_quaternion','scale'):b.keyframe_insert(prop,frame=frame,group=b.name)
                bpy.context.view_layer.update()
                maximum=max(maximum,max(abs(b.matrix[i][j]-w[b.name][i][j]) for b in rig.pose.bones for i in range(4) for j in range(4)))
                if frame in (1,frames):targets[str(frame)]={k:[list(row) for row in v] for k,v in w.items()}
            for curve in list(curves(action)):
                for key in curve.keyframe_points:key.interpolation='LINEAR'
            actions.append({'name':action.name,'end_frame':frames,'expected':targets})
        assert maximum<1e-5,maximum
        rig.animation_data.action=bpy.data.actions[actions[0]['name']]
        scene.frame_set(1)
        bpy.context.view_layer.update()
        for o in bpy.context.selected_objects:o.select_set(False)
        rig.hide_set(False);rig.select_set(True)
        bpy.context.view_layer.objects.active=rig
        bpy.ops.object.mode_set(mode='POSE')
        for b in rig.pose.bones:b.select=False
        rig.pose.bones['headhook'].select=True
        rig.data.bones.active=rig.data.bones['headhook']
        rig.data.show_names=True
        for screen in bpy.data.screens:
            for area in screen.areas:
                if area.type=='VIEW_3D':
                    area.spaces.active.region_3d.view_distance=2.8
                    area.spaces.active.region_3d.view_location=rig.pose.bones['headhook'].matrix.translation
        guide='''HEAD EDITING PACK
Guarded action opens by default. Action Editor dropdown also contains Relaxed.
Stay in Pose Mode. The native headhook bone is selected.
Edit headhook, not the head mesh. Use R Z angle Enter to turn around world Z,
or R X X / R Y Y for local tilt. G moves the head; keep translations small.
At frame 1: insert Location, Rotation & Scale via F3 > Insert Keyframe with
Keying Set. Ctrl+C, go to the last frame, Ctrl+V, insert the same keys again.
Walking has baked keys throughout the cycle: adjust the required headhook keys,
including both endpoints for a smooth loop. Idle only needs the two endpoint poses.
Choose each action you want edited and repeat. Save an edited copy and return
the file path. The game is unchanged until a separately requested import.
Source is current body data composed with native ready/idle selections and
steady forward guard/walk rules. Aim, strafe, transition smoothing and combat
are not simulated. These files are not an in-game visual verification.
Owned assets are local; do not publish extracted models.
'''
        bpy.data.texts.new('READ ME - edit headhook').write(guide)
        (out/'EDITING.txt').write_text(guide)
        bpy.ops.file.pack_all()
        bpy.ops.wm.save_as_mainfile(filepath=str(out/(name+'.blend')))
        report.append({'file':name+'.blend','resource':resource,'walking':walking,'max_matrix_error':maximum,'actions':actions})
        print('HEAD_PACK_FILE_PASS',name,frames,maximum,flush=True)
assert protected=={p:digest(p) for p in protected}
(out/'manifest.json').write_text(json.dumps({'sources':sources,'files':report,'game_files_unchanged':True,'runtime_tested':False},indent=2))
print('HEAD_PACK_EXPORT_PASS',len(report),flush=True)
