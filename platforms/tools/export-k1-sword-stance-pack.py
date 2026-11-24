"""Sword-equipped editing copies of current guards and attack/chain controllers.

Run with installed Blender background. No game launch, source-scene writes,
custom controls or uniform keyframe baking. Existing output files are protected.
"""
from pathlib import Path
import ast
import copy
import hashlib
import importlib.util
import json
import math
import shutil
import subprocess
import sys
import bpy
from mathutils import Matrix, Quaternion, Vector

repo = Path(__file__).resolve().parents[2]
out = repo/'platforms/out/sword-stance-edit-pack'
subprocess.run(['git','-C',str(repo),'check-ignore','--quiet',str(out/'source')],check=True)
source = out/'source'
source.mkdir(parents=True,exist_ok=True)
selected=sys.argv[sys.argv.index('--stance')+1] if '--stance' in sys.argv else None
assert not list(out.glob((selected if selected else '*')+'_sword_animations.blend')), 'Preserve existing editing copies.'
sys.path[:0] = [str(repo/'platforms/out/kotor-animation-import/tools'),str(repo/'tools'),str(repo/'platforms/tools')]
import io_scene_kotor
from io_scene_kotor.constants import ImportOptions
from io_scene_kotor.format.mdl.reader import MdlReader
from padglyphs import erf_resource
io_scene_kotor.register()

def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def nodes(n): return [n]+[v for c in n.children for v in nodes(c)]
def curves(action):
    slot=action.slots[0] if action.slots else action.slots.new(id_type='OBJECT',name=action.name)
    layer=action.layers[0] if action.layers else action.layers.new('Animation')
    strip=layer.strips[0] if layer.strips else layer.strips.new(type='KEYFRAME')
    return strip.channelbag(slot,ensure=True).fcurves
if not hasattr(bpy.types.Action,'fcurves'): bpy.types.Action.fcurves=property(curves)

profile=repo/'platforms/out/k1-engage-prototype/elbow-arena/profile/override'
old=repo/'platforms/out/stance-head-original-keys'
walk=repo/'platforms/out/stance-head-guard-walk'
custom=repo/'platforms/out/custom-stance-base'
paths={
 'yellow':[old/'yellow_idle_with_head.blend',walk/'yellow_guard_walking_with_head.blend'],
 'red':[old/'red_idle_with_head.blend',walk/'red_guard_walking_with_head.blend'],
 'blue':[old/'red_idle_with_head.blend',walk/'red_guard_walking_with_head.blend'],
 'purple':[custom/'purple_idle_guard.blend',custom/'purple_guard_walking.blend'],
 'double_blade':[repo/'platforms/out/staff-guard-edit/kotor_staff_idle_guard_with_head.blend',walk/'double_blade_guard_walking_with_head.blend'],
 'duals':[walk/'duals_idle_guard_with_head.blend',walk/'duals_guard_walking_with_head.blend']}
resources=dict(zip(paths,('k1jarig','k1jared','k1jablue','k1japurp','k1jastaff','k1jadual')))
protected={p:digest(p) for p in profile.glob('*.md*')}
protected.update({p:digest(p) for pair in paths.values() for p in pair})
models={name:MdlReader(str(profile/(name+'.mdl'))).load() for name in resources.values()}
models.update({name:MdlReader(str(repo/'platforms/out/swords'/(name+'.mdl'))).load()
               for name in ('w_lngswrd_001','w_dblswrd_001')})

# The existing importer resolves native textures through the same owned-data
# reader as previous editing packs. Pack images so the deliverables are portable.
spec=importlib.util.spec_from_file_location('owned',repo/'platforms/out/k1-engage-prototype/dark-jedi/inspect-owned.py')
owned=importlib.util.module_from_spec(spec);spec.loader.exec_module(owned)
for model in models.values():
    for n in nodes(model.root_node):
        for field in ('bitmap','bitmap2','texture1'):
            name=getattr(n,field,'')
            if isinstance(name,str) and name.lower() not in ('','null'):
                p=source/(name.lower()+'.tpc')
                if not p.exists():
                    data=erf_resource(str(owned.game/'TexturePacks/swpc_tex_tpa.erf'),name.lower(),3007)
                    if data:p.write_bytes(data)

def snapshot(path):
    bpy.ops.wm.open_mainfile(filepath=str(path))
    rig=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
    action=rig.animation_data.action
    return {'curves':[(c.data_path,c.array_index,[(float(k.co.x),float(k.co.y),k.interpolation,
                      tuple(k.handle_left),tuple(k.handle_right),k.handle_left_type,k.handle_right_type)
                      for k in c.keyframe_points]) for c in curves(action)],
            'rest':{b.name:b.matrix_local.copy() for b in rig.data.bones},
            'end':bpy.context.scene.frame_end,'fps':bpy.context.scene.render.fps/bpy.context.scene.render.fps_base}

def bind_action(rig,action):
    rig.animation_data_create();rig.animation_data.action=action
    if action.slots:rig.animation_data.action_slot=action.slots[0]
    for b in rig.pose.bones:
        b.location=(0,0,0);b.rotation_quaternion=(1,0,0,0);b.scale=(1,1,1)

def copy_action(rig,data,name):
    for b in rig.data.bones:
        assert b.name in data['rest']
        assert max(abs(b.matrix_local[i][j]-data['rest'][b.name][i][j]) for i in range(4) for j in range(4))<1e-5,(name,b.name,'rest mismatch')
    a=bpy.data.actions.new(name);fc=curves(a)
    for path,index,keys in data['curves']:
        c=fc.new(path,index=index,group_name=path.split('"')[1])
        c.keyframe_points.add(len(keys))
        for k,(t,v,interp,left,right,lt,rt) in zip(c.keyframe_points,keys):
            k.co=(t,v)
            k.interpolation=interp;k.handle_left_type=lt;k.handle_right_type=rt
            k.handle_left=left;k.handle_right=right
        c.update()
    a.use_fake_user=True;a['last_frame']=data['end'];a['policy']='Exact saved editor curves and fractional key times; no resampling.'
    return a

def native_action(rig,model,clip,name):
    a=bpy.data.actions.new(name);bind_action(rig,a)
    tracks={n.name:n.keyframes for n in nodes(clip.root_node)}
    lookup={n.name:n for n in nodes(model.root_node)}
    count=0
    for b in rig.pose.bones:
        rest=b.parent.bone.matrix_local.inverted()@b.bone.matrix_local if b.parent else b.bone.matrix_local.copy()
        r=rest.to_quaternion().normalized();p=Vector(lookup[b.name].position)
        b.location=rest.to_3x3().inverted()@(p-rest.translation)
        b.rotation_quaternion=(r.inverted()@Quaternion(lookup[b.name].orientation)).normalized()
        for kind,rows in tracks.get(b.name,{}).items():
            assert kind in ('position','orientation'),(clip.name,b.name,kind)
            prop='location' if kind=='position' else 'rotation_quaternion'
            values=[]
            for row in rows:
                value=rest.to_3x3().inverted()@(p+Vector(row[1:4])-rest.translation) if kind=='position' else (r.inverted()@Quaternion((row[4],*row[1:4]))).normalized()
                values.append((1+30*row[0],tuple(value)))
            # Native clip junctions can be closer than Blender's insertion
            # tolerance. Allocate explicitly so no source keys are merged.
            for index in range(3 if kind=='position' else 4):
                c=curves(a).new('pose.bones["'+b.name+'"].'+prop,index=index,group_name=b.name)
                c.keyframe_points.add(len(values))
                for k,(t,v) in zip(c.keyframe_points,values):k.co=(t,v[index]);k.interpolation='LINEAR'
            count+=len(rows)*(3 if kind=='position' else 4)
    for c in curves(a):
        for k in c.keyframe_points:k.interpolation='LINEAR'
    assert sum(len(c.keyframe_points) for c in curves(a))==count,(clip.name,count,sum(len(c.keyframe_points) for c in curves(a)),len(a.slots),[(c.data_path,c.array_index,len(c.keyframe_points)) for c in curves(a)][:8])
    a.use_fake_user=True;a['last_frame']=1+30*clip.length;a['source_clip']=clip.name
    a['policy']='Current in-game MDL controller keys at original times, 30fps display; no resampling.'
    return a

results=[]
for stance,resource in resources.items():
    if selected and stance!=selected:continue
    guard_data,walk_data=(snapshot(p) for p in paths[stance])
    # Keep the complete established editing scene, head, materials and hierarchy.
    bpy.ops.wm.open_mainfile(filepath=str(paths[stance][0]))
    scene=bpy.context.scene;scene.render.fps=30;scene.render.fps_base=1
    rig=next(o for o in scene.objects if o.type=='ARMATURE')
    if rig.mode!='OBJECT':bpy.ops.object.mode_set(mode='OBJECT')
    assert 'headhook' in rig.pose.bones and 'HEAD_EDIT' not in rig.pose.bones
    for b in rig.pose.bones:b.rotation_mode='QUATERNION'
    for a in list(bpy.data.actions):
        if a!=rig.animation_data.action:a.use_fake_user=False
    actions=[]
    model=models[resource]
    if stance in ('blue','purple'):
        guard=native_action(rig,model,next(a for a in model.animations if a.name=='j_guard'),'01 Idle guard')
    else:guard=copy_action(rig,guard_data,'01 Idle guard')
    actions.append(guard)
    if stance=='purple':
        walking=native_action(rig,model,next(a for a in model.animations if a.name=='j_purpwalk'),'02 Guard walking')
    else:walking=copy_action(rig,walk_data,'02 Guard walking')
    if stance=='blue':
        # Same established sparse gait editor, with Blue's own held upper pose.
        bind_action(rig,guard);scene.frame_set(1);bpy.context.view_layer.update()
        upper={b.name for b in rig.pose.bones if b.name=='torso_g' or any(p.name=='torso_g' for p in b.parent_recursive)}|{'j_motion'}
        guard_local={b.name:b.parent.matrix.inverted()@b.matrix if b.parent else b.matrix.copy() for b in rig.pose.bones}
        drivers={1.,float(walk_data['end'])}
        drivers.update(t for path,index,keys in walk_data['curves'] if path.split('"')[1] in ('pelvis_g','j_motion') and path.endswith('rotation_quaternion') for t,*_ in keys)
        tree=ast.parse((repo/'platforms/tools/export-k1-stance-head-pack.py').read_text())
        exec(compile(ast.Module(body=[n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name in {'rest_local','worlds','replace_rotation','motion'}],type_ignores=[]),'existing-sparse-guard-composition','exec'))
        poses={}
        for f in sorted(drivers):
            bind_action(rig,walking);scene.frame_set(int(f),subframe=f-int(f));bpy.context.view_layer.update()
            local=rest_local(model)
            local.update({b.name:b.parent.matrix.inverted()@b.matrix if b.parent else b.matrix.copy() for b in rig.pose.bones})
            local.update({name:guard_local[name].copy() for name in upper})
            motion(model,local,guard_local)
            poses[f]=worlds(model,local)
        for c in list(curves(walking)):
            if c.data_path.split('"')[1] in upper:curves(walking).remove(c)
        bind_action(rig,walking)
        for f,w in poses.items():
            for name in upper:
                if f not in (1.,float(walk_data['end'])) and name not in ('torso_g','torsoMid_g','torsoUpr_g'):continue
                b=rig.pose.bones[name]
                kw={'parent_matrix':w[b.parent.name],'parent_matrix_local':b.parent.bone.matrix_local} if b.parent else {}
                b.matrix_basis=b.bone.convert_local_to_pose(w[name],b.bone.matrix_local,invert=True,**kw)
                for prop in ('location','rotation_quaternion','scale'):b.keyframe_insert(prop,frame=f,group=name)
        for c in curves(walking):
            if c.data_path.split('"')[1] in upper:
                for k in c.keyframe_points:k.interpolation='LINEAR'
        walking['policy']='Established sparse forward gait with Blue upper guard and source-time spine compensation. Lower keys unchanged; no uniform resampling.'
    actions.append(walking)
    attacks=[a for a in model.animations if a.name.startswith('s_')]
    assert len(attacks)>=7,(stance,len(attacks))
    for clip in attacks:actions.append(native_action(rig,model,clip,'Attack '+clip.name))
    # Remove the previous saber reference objects, never the attached native head.
    remove=[]
    for o in list(scene.objects):
        c=o.constraints.get('Copy Transforms')
        if any(c.type=='COPY_TRANSFORMS' and c.target==rig and c.subtarget in ('rhand','lhand') for c in o.constraints):
            remove.extend([o]+list(o.children_recursive))
    for o in set(remove):
        if o.name in bpy.data.objects:bpy.data.objects.remove(o,do_unlink=True)
    attachments=bpy.data.collections.new('Swords - rotate reference here to check orientation')
    scene.collection.children.link(attachments)
    options=ImportOptions();options.import_animations=False;options.import_walkmeshes=False;options.build_armature=False
    options.texture_search_paths=[str(source)];options.lightmap_search_paths=[]
    weapon='w_dblswrd_001' if stance=='double_blade' else 'w_lngswrd_001'
    sword_names=[]
    for bone in (('rhand','lhand') if stance=='duals' else ('rhand',)):
        m=copy.deepcopy(models[weapon]);m.animations=[]
        obj=m.add_to_collection(attachments,options)
        anchor=bpy.data.objects.new('Sword reference '+bone,None);attachments.objects.link(anchor)
        follow=anchor.constraints.new('COPY_TRANSFORMS');follow.target=rig;follow.subtarget=bone
        follow.owner_space=follow.target_space='WORLD'
        obj.parent=anchor;obj.matrix_parent_inverse=Matrix.Identity(4)
        obj.rotation_mode='QUATERNION';obj.rotation_quaternion=Quaternion((0,0,1),math.pi/2)
        sword_names.append(obj.name)
        for child in [obj]+list(obj.children_recursive):
            if child.type=='MESH' and not child.kb.render:child.hide_set(True);child.hide_render=True
    # Hide rigid A-pose construction meshes; retain the armature-deformed body.
    for o in scene.objects:
        if o.type=='MESH' and o.kb.classification=='CHARACTER' and not any(m.type=='ARMATURE' for m in o.modifiers):
            o.hide_set(True);o.hide_render=True
    rig.name=stance.upper()+' - native KotOR rig'
    # Blender's action dropdown reuses the last slot identifier. All actions
    # animate this same armature, so give their slots the same identity.
    for a in actions:
        for slot in a.slots:slot.identifier='OB'+rig.name
    bind_action(rig,guard);scene.frame_start=1;scene.frame_end=30;scene.frame_set(1)
    bpy.context.view_layer.update()
    for o in bpy.context.selected_objects:o.select_set(False)
    rig.select_set(True);bpy.context.view_layer.objects.active=rig
    bpy.ops.object.mode_set(mode='POSE')
    for b in rig.pose.bones:b.select=b.name=='rhand'
    rig.data.bones.active=rig.data.bones['rhand']
    for screen in bpy.data.screens:
        for area in screen.areas:
            if area.type=='VIEW_3D':area.spaces.active.shading.type='MATERIAL'
            elif area.type=='DOPESHEET_EDITOR':area.spaces.active.mode='ACTION'
    manifest=[{'name':a.name,'last_frame':float(a['last_frame']),
               'keys':sum(len(c.keyframe_points) for c in curves(a))} for a in actions]
    instructions=bpy.data.texts.new('READ ME - sword orientation')
    instructions.write('Choose an action in the Action Editor dropdown. 01 = idle guard, 02 = guard walking; Attack = full-body attack or chain.\n'
        'Set timeline End to the action last_frame listed in its custom properties (round up). Display rate is 30fps.\n'
        'Pose Mode: edit native bones such as Rhand_g, Lhand_g, rhand/lhand hooks and headhook. Key changes as usual.\n'
        'The sword reference has the current in-game 90-degree local shaft roll. Changing the sword reference object alone is preview-only; save any proposed attachment offset separately.\n'
        'No head_edit controls and no uniform baking. Current authored guard keys and attack controller times are retained.\n')
    for a in list(bpy.data.actions):
        if a not in actions and a.users<=1:bpy.data.actions.remove(a)
    bpy.ops.file.pack_all()
    dest=out/(stance+'_sword_animations.blend')
    bpy.ops.wm.save_as_mainfile(filepath=str(dest))
    bpy.ops.wm.open_mainfile(filepath=str(dest))
    rig=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
    assert 'HEAD_EDIT' not in rig.pose.bones
    for item in manifest:
        a=bpy.data.actions[item['name']]
        assert sum(len(c.keyframe_points) for c in curves(a))==item['keys']
        rig.animation_data.action=a
        assert rig.animation_data.action_slot is not None,(stance,item['name'],'automatic slot assignment')
        for f in (1.,float(item['last_frame'])):
            bpy.context.scene.frame_set(int(f),subframe=f-int(f));bpy.context.view_layer.update()
            for name in sword_names:
                sword=bpy.data.objects[name];bone=sword.parent.constraints[0].subtarget
                expected=rig.matrix_world@rig.pose.bones[bone].matrix@Matrix.Rotation(math.pi/2,4,'Z')
                assert max(abs(sword.matrix_world[i][j]-expected[i][j]) for i in range(4) for j in range(4))<2e-5,(stance,item['name'],bone)
    results.append({'file':dest.name,'resource':resource,'actions':manifest,'sword':weapon,'saved_reload_verified':True})
    print('SWORD_STANCE_EXPORT_PASS',stance,len(actions),flush=True)
assert all(digest(p)==h for p,h in protected.items()),'Source files changed'
report_path=out/'report.json'
if selected and report_path.exists():
    previous=json.loads(report_path.read_text())
    results=[r for r in previous['files'] if r['resource']!=resources[selected]]+results
report_path.write_text(json.dumps({'files':results,'source_hashes':{str(p.relative_to(repo)):h for p,h in protected.items()},'protected_sources_unchanged':True,'in_game_tested':False},indent=2))
print('SWORD_STANCE_PACK_PASS',len(results),flush=True)
