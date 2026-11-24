"""Simple headhook editing files: original walk controller keys, 30fps idle holds.

Uses owned, already-local model resources. Does not launch or change the game.
Run using installed Blender --background --factory-startup --disable-autoexec
--python-exit-code 1 --python this-file. Never overwrites existing editor files.
"""
from pathlib import Path
import copy
import hashlib
import json
import shutil
import subprocess
import sys
import bpy
from mathutils import Matrix, Quaternion, Vector

repo=Path(__file__).resolve().parents[2]
out=repo/'platforms/out/stance-head-original-keys'
subprocess.run(['git','-C',str(repo),'check-ignore','--quiet',str(out/'source')],check=True)
out.mkdir(exist_ok=True)
assert not list(out.glob('*.blend')), 'Preserve existing editor files.'
source=out/'source'
source.mkdir(exist_ok=True)
previous=repo/'platforms/out/stance-head-edit-pack'
profile=repo/'platforms/out/k1-engage-prototype/elbow-arena/profile/override'
sys.path.insert(0,str(repo/'platforms/out/kotor-animation-import/tools'))
import io_scene_kotor
from io_scene_kotor.constants import ImportOptions
from io_scene_kotor.format.mdl.reader import MdlReader
from io_scene_kotor.scene import armature
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def nodes(n):return [n]+[x for c in n.children for x in nodes(c)]
protected={p:digest(p) for p in profile.glob('*.md*')}
protected.update({p:digest(p) for p in previous.rglob('*.blend')})
resources={'red':'k1jared','yellow':'k1jarig','duals':'k1jadual','double_blade':'k1jastaff'}
models={}
hashes={}
for texture in (previous/'source').glob('*.tpc'):
    shutil.copy2(texture,source/texture.name)
for name in list(resources.values())+['pmhc01','w_lghtsbr_001','w_dblsbr_001']:
    for ext in ('mdl','mdx'):
        p=source/(name+'.'+ext)
        origin=profile/p.name if name in resources.values() else previous/'source'/p.name
        shutil.copy2(origin,p)
        hashes[p.name]=digest(p)
    models[name]=MdlReader(str(source/(name+'.mdl'))).load()
io_scene_kotor.register()
def curves(action):
    slot=action.slots[0] if action.slots else action.slots.new(id_type='OBJECT',name=action.name)
    layer=action.layers[0] if action.layers else action.layers.new('Animation')
    strip=layer.strips[0] if layer.strips else layer.strips.new(type='KEYFRAME')
    return strip.channelbag(slot,ensure=True).fcurves
if not hasattr(bpy.types.Action,'fcurves'):bpy.types.Action.fcurves=property(curves)
results=[]
for stance,resource in resources.items():
    for walking in ([True] if stance=='double_blade' else [False,True]):
        name=stance+('_walking' if walking else '_idle')+'_with_head'
        idle_pose=None
        if not walking:
            # Reuse the settled guard selection already established for each stance,
            # not the unused raw JKA dual ready clip or a newly sampled idle cycle.
            bpy.ops.wm.open_mainfile(filepath=str(previous/'headhook-backup'/(name+'.blend')))
            oldrig=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
            oldrig.animation_data.action=next(a for a in bpy.data.actions if a.name.startswith('01 Guarded'))
            bpy.context.scene.frame_set(1)
            bpy.context.view_layer.update()
            idle_pose={b.name:b.matrix.copy() for b in oldrig.pose.bones if b.name!='HEAD_EDIT'}
        bpy.ops.wm.read_factory_settings(use_empty=True)
        scene=bpy.context.scene
        scene.render.fps=30
        scene.render.fps_base=1
        options=ImportOptions()
        options.import_animations=False
        options.import_walkmeshes=False
        options.build_armature=False
        options.texture_search_paths=[str(source)]
        options.lightmap_search_paths=[]
        body=models[resource]
        clean=copy.deepcopy(body)
        clean.animations=[]
        collection=bpy.data.collections.new('KotOR body')
        scene.collection.children.link(collection)
        root=clean.add_to_collection(collection,options)
        bpy.context.view_layer.update()
        rig=armature.rebuild_armature(root)
        assert rig and 'HEAD_EDIT' not in rig.pose.bones
        rig.name=stance.upper()+' - edit headhook in Pose Mode'
        for b in rig.pose.bones:b.rotation_mode='QUATERNION'
        key_report={}
        if walking:
            clip=next(a for a in body.animations if a.name=='j_walk')
            tracks={n.name:n.keyframes for n in nodes(clip.root_node)}
            lookup={n.name:n for n in nodes(body.root_node)}
            for b in rig.pose.bones:
                rest=b.parent.bone.matrix_local.inverted()@b.bone.matrix_local if b.parent else b.bone.matrix_local.copy()
                r=rest.to_quaternion().normalized()
                # Blender's tiny native display bones can round the reconstructed
                # parent-relative rest offset. Preserve the actual source local
                # transform rather than treating that rounded offset as exact.
                source_position=Vector(lookup[b.name].position)
                b.location=rest.to_3x3().inverted()@(source_position-rest.translation)
                b.rotation_quaternion=(r.inverted()@Quaternion(lookup[b.name].orientation)).normalized()
                keys=tracks.get(b.name,{})
                assert not (set(keys)-{'position','orientation'}), (b.name,keys.keys())
                for kind,rows in keys.items():
                    prop='location' if kind=='position' else 'rotation_quaternion'
                    for row in rows:
                        if kind=='position':b.location=rest.to_3x3().inverted()@(source_position+Vector(row[1:4])-rest.translation)
                        else:b.rotation_quaternion=(r.inverted()@Quaternion((row[4],*row[1:4]))).normalized()
                        b.keyframe_insert(prop,frame=1+30*row[0],group=b.name)
                    key_report[b.name+':'+prop]=[1+30*row[0] for row in rows]
            end=1+30*clip.length
            scene.frame_start=1
            scene.frame_end=int(__import__('math').ceil(end))
            policy='Original j_walk controllers; exact source key times/counts, fractional frames retained. No runtime guard overlay, aim or smoothing added.'
        else:
            for b in rig.pose.bones:
                kw={'parent_matrix':idle_pose[b.parent.name],'parent_matrix_local':b.parent.bone.matrix_local} if b.parent else {}
                b.matrix_basis=b.bone.convert_local_to_pose(idle_pose[b.name],b.bone.matrix_local,invert=True,**kw)
                for prop in ('location','rotation_quaternion','scale'):
                    for f in (1,30):b.keyframe_insert(prop,frame=f,group=b.name)
                    key_report[b.name+':'+prop]=[1,30]
            end=30
            scene.frame_start=1;scene.frame_end=30
            policy='Settled implemented guard, held at frames 1 and 30, matching the original double-blade idle editor setup.'
        action=rig.animation_data.action
        action.name=name
        action['source_policy']=policy
        action['source_duration_seconds']=clip.length if walking else 29/30
        action['exact_last_key_frame']=end
        maximum_time_error=0.
        for c in curves(action):
            for k in c.keyframe_points:k.interpolation='LINEAR'
            bone=c.data_path.split('"')[1]
            prop=c.data_path.rsplit('.',1)[1]
            expected=key_report[bone+':'+prop]
            assert len(c.keyframe_points)==len(expected)
            maximum_time_error=max(maximum_time_error,max(abs(k.co.x-f) for k,f in zip(c.keyframe_points,expected)))
        assert maximum_time_error<1e-4
        for o in collection.objects:
            if o.type=='EMPTY' or (o.type=='MESH' and not o.kb.render):
                o.hide_set(True);o.hide_render=True
        attachments=bpy.data.collections.new('Head and saber previews')
        scene.collection.children.link(attachments)
        def attach(resource,bone,label):
            m=copy.deepcopy(models[resource]);m.animations=[]
            obj=m.add_to_collection(attachments,options)
            anchor=bpy.data.objects.new(label,None)
            attachments.objects.link(anchor)
            follow=anchor.constraints.new('COPY_TRANSFORMS')
            follow.target=rig;follow.subtarget=bone
            follow.owner_space=follow.target_space='WORLD'
            obj.parent=anchor;obj.matrix_parent_inverse=Matrix.Identity(4)
            anchor.hide_set(True)
            for child in [obj]+list(obj.children_recursive):
                if child.type=='MESH' and not child.kb.render:
                    child.hide_set(True);child.hide_render=True
        attach('pmhc01','headhook','Head follows headhook')
        attach('w_dblsbr_001' if stance=='double_blade' else 'w_lghtsbr_001','rhand','Right saber')
        if stance=='duals':attach('w_lghtsbr_001','lhand','Left saber')
        scene.frame_set(1)
        bpy.context.view_layer.update()
        for o in bpy.context.selected_objects:o.select_set(False)
        rig.select_set(True);bpy.context.view_layer.objects.active=rig
        bpy.ops.object.mode_set(mode='POSE')
        for b in rig.pose.bones:b.select=False
        rig.pose.bones['headhook'].select=True
        rig.data.bones.active=rig.data.bones['headhook']
        for screen in bpy.data.screens:
            for area in screen.areas:
                if area.type=='VIEW_3D':
                    area.spaces.active.region_3d.view_distance=2.8
                    area.spaces.active.region_3d.view_location=rig.pose.bones['headhook'].matrix.translation
        bpy.ops.file.pack_all()
        bpy.ops.wm.save_as_mainfile(filepath=str(out/(name+'.blend')))
        results.append({'file':name+'.blend','walking':walking,'resource':resource,'fps':30,
            'last_key_frame':end,'policy':policy,'source_key_frames':key_report,
            'max_key_time_error':maximum_time_error})
        print('ORIGINAL_KEYS_FILE_PASS',name,len(key_report),maximum_time_error,flush=True)
assert protected=={p:digest(p) for p in protected}
(out/'manifest.json').write_text(json.dumps({'sources':hashes,'files':results,'protected_files_unchanged':True},indent=2))
print('ORIGINAL_KEYS_EXPORT_PASS',len(results),flush=True)
