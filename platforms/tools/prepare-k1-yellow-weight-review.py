"""Separate yellow-stance weight previews on the actual player/Dark Jedi bodies."""
from pathlib import Path
import ast
import bisect
import copy
import hashlib
import importlib.util
import json
import math
import os
import shutil
import subprocess
import sys

import bpy
import numpy as np
from mathutils import Matrix, Quaternion, Vector
from mathutils.kdtree import KDTree

repo=Path(__file__).resolve().parents[2]
engage=repo/'platforms/out/k1-engage-prototype'
kind=os.environ.get('K1_YELLOW_REVIEW_BODY','player')
assert kind in ('player','dark')
resource='k1jarig' if kind=='player' else 'k1jadark'
dest=engage/'yellow-weight-review'/(kind+'-02')
assert not dest.exists(), 'Preserve earlier trials.'
dest.mkdir()
shutil.copy2(__file__,dest/'recipe.py')
source=dest/'source';source.mkdir()
profile=engage/'elbow-arena/profile'
protected={}
for name in ('k1jarig','k1jadark','k1yellow','k1jastaff'):
    for ext in ('.mdl','.mdx'):
        p=profile/'override'/(name+ext)
        if p.exists():protected[str(p.relative_to(repo))]=hashlib.sha256(p.read_bytes()).hexdigest()
for ext in ('.mdl','.mdx'):shutil.copy2(profile/'override'/(resource+ext),source/(resource+ext))
sys.path.insert(0,str(repo/'platforms/out/kotor-animation-import/tools'))
sys.path.insert(0,str(repo/'platforms/tools'))
sys.path.insert(0,str(repo/'tools'))
import io_scene_kotor
from io_scene_kotor.constants import ImportOptions
from io_scene_kotor.format.mdl.reader import MdlReader
from io_scene_kotor.scene import armature
from k1staff_preview_weights import PreviewSkinWeights
from padglyphs import erf_resource

# Reuse the established native-key sampler, ownership compensation and baker.
renderer=repo/'platforms/tools/render-k1-staff-swing-review.py'
tree=ast.parse(renderer.read_text())
functions={'nodes','worlds','interpolate_keys','native','wrap','displayed','curves','bake','mat_error'}
exec(compile(ast.Module(body=[n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name in functions],type_ignores=[]),str(renderer),'exec'))
body=MdlReader(str(source/(resource+'.mdl'))).load()
nn=nodes(body.root_node)
parents={n.name:n.parent.name if n.parent else None for n in nn}
bind={n.name:Matrix.LocRotScale(Vector(n.position),Quaternion(n.orientation),Vector((n.scale,)*3)) for n in nn}
rest=worlds(bind)
clips={a.name:{n.name:n.keyframes for n in nodes(a.root_node)} for a in body.animations}
lengths={a.name:a.length for a in body.animations}
upper=set()
for n in nn:
    p=n
    while p and p.name!='torso_g':p=p.parent
    if p:upper.add(n.name)
upper.add('j_motion')

# Execute the exact accepted weight policy on this body's own geometry/binds.
policy=engage/'staff-swing-review/weights-02/proposal-recipe.py'
code=policy.read_text().split('lookup={n.name:n for n in nn}',1)[1].split('def install(weights):',1)[0]
# This body has a separate face/neck surface. Preserve existing boundaries
# whose original influences differ; do not turn a torso review into head edits.
marker='changed=[]'
assert code.count(marker)==1
code=code.replace(marker,"""frozen_boundaries=set()
for pair in seams:
    aa,ii=pair['a'];bb,jj=pair['b']
    wa,wb=dict(original[aa][ii]),dict(original[bb][jj])
    if max(abs(wa.get(k,0)-wb.get(k,0)) for k in wa.keys()|wb.keys())>=1e-5:
        frozen_boundaries.update(((aa,ii),(bb,jj)))
changed=[]""")
code=code.replace('for i,row in enumerate(rows):','for i,row in enumerate(rows):\n        if (name,i) in frozen_boundaries:continue')
code=code.replace("    a,i=pair['a'];b,j=pair['b']", "    a,i=pair['a'];b,j=pair['b']\n    if (a,i) in frozen_boundaries or (b,j) in frozen_boundaries:continue")
exec(compile('lookup={n.name:n for n in nn}'+code,str(policy),'exec'))
assert all(trial[n][i]==original[n][i] for n,i in frozen_boundaries)
weight_data=dict(weights=trial,original_weights=original,bind_points={n.name:[list(rest[n.name]@Vector(v)) for v in n.verts] for n in nn if n.nodetype=='SKIN'})
(dest/'skin-weights.json').write_text(json.dumps(weight_data))

spec=importlib.util.spec_from_file_location('owned_k1',engage/'dark-jedi/inspect-owned.py')
owned=importlib.util.module_from_spec(spec);spec.loader.exec_module(owned)
def extract(name,typ,ext):
    p=source/(name+'.'+ext);p.write_bytes(owned.resource(name,typ));return p
extract('w_lghtsbr_001',2002,'mdl');extract('w_lghtsbr_001',3008,'mdx')
weapon=MdlReader(str(source/'w_lghtsbr_001.mdl')).load()
head=None
if kind=='player':
    for ext in ('.mdl','.mdx'):shutil.copy2(engage/'staff-editor/source'/('pmhc01'+ext),source/('pmhc01'+ext))
    head=MdlReader(str(source/'pmhc01.mdl')).load()
for name in sorted({getattr(n,a,'').lower() for m in [body,weapon]+([head] if head else []) for n in nodes(m.root_node) for a in ('bitmap','bitmap2')} - {'','null'}):
    texture=erf_resource(str(owned.game/'TexturePacks/swpc_tex_tpa.erf'),name,3007)
    assert texture is not None,name
    (source/(name+'.tpc')).write_bytes(texture)

bpy.ops.wm.read_factory_settings(use_empty=True)
io_scene_kotor.register()
if not hasattr(bpy.types.Action,'fcurves'):
    bpy.types.Action.fcurves=property(lambda a:[c for l in a.layers for s in l.strips for b in s.channelbags for c in b.fcurves])
options=ImportOptions();options.import_animations=False;options.import_walkmeshes=False;options.build_armature=False;options.build_materials=True;options.texture_search_paths=[str(source)];options.lightmap_search_paths=[]
from io_scene_kotor import utils
utils.ANIM_FPS=120
scene=bpy.context.scene;scene.render.fps=120;scene.render.fps_base=1
collection=bpy.data.collections.new('Character');scene.collection.children.link(collection)
model_root=body.add_to_collection(collection,options)
rig=armature.rebuild_armature(model_root);assert rig
rig.name='Yellow_Review_'+kind;rig.animation_data_create();rig.show_in_front=True
skin_objects=[o for o in collection.objects if o.type=='MESH' and any(m.type=='ARMATURE' and m.object==rig for m in o.modifiers)]
for o in collection.objects:
    if o not in skin_objects and o!=rig:o.hide_set(True);o.hide_render=True
for b in rig.pose.bones:b.rotation_mode='QUATERNION'
bpy.context.view_layer.objects.active=rig;bpy.ops.object.mode_set(mode='EDIT')
for b in rig.data.edit_bones:b.length=.1;b.matrix=bpy.data.objects[b.name].matrix_world
bpy.ops.object.mode_set(mode='OBJECT')
ordered=list(rig.pose.bones)
for b in ordered:assert b.parent is None or ordered.index(b.parent)<ordered.index(b)
weight_switch=PreviewSkinWeights(dest/'skin-weights.json');weight_switch.apply(True)

attachments=bpy.data.collections.new('Owned attachments');scene.collection.children.link(attachments)
def attach(model,bone,label):
    obj=model.add_to_collection(attachments,options);anchor=bpy.data.objects.new(label,None);attachments.objects.link(anchor)
    c=anchor.constraints.new('COPY_TRANSFORMS');c.target=rig;c.subtarget=bone
    obj.parent=anchor;obj.matrix_parent_inverse=Matrix.Identity(4);anchor.hide_set(True)
    for o in [obj]+list(obj.children_recursive):
        if o.type!='MESH' or not o.kb.render or (model==weapon and o.kb.meshtype=='LIGHTSABER'):o.hide_set(True);o.hide_render=True
    return anchor
if head:attach(head,'headhook','Head attachment')
weapon_anchor=attach(weapon,'rhand','Saber attachment')
# Reuse the actual central blade mesh bounds, then carry its frame with rhand.
weapon_world={}
for n in nodes(weapon.root_node):
    scale=1. if n.nodetype=='LIGHTSABER' else n.scale
    local=Matrix.LocRotScale(Vector(n.position),Quaternion(n.orientation),Vector((scale,)*3));weapon_world[n.name]=weapon_world[n.parent.name]@local if n.parent else local
blade=next(n for n in nodes(weapon.root_node) if n.nodetype=='LIGHTSABER')
points=[weapon_world[blade.name]@Vector(v) for v in blade.verts]
span=[max(p[i] for p in points)-min(p[i] for p in points) for i in range(3)];along=max(range(3),key=lambda i:span[i]);a=sum(points,Vector())/len(points);b=a.copy();a[along]=min(p[along] for p in points);b[along]=max(p[along] for p in points)
assert .2<(b-a).length<2.,(a,b)
for radius,color in ((.012,(1,.04,.015) if kind=='dark' else (.015,.13,1)),(.005,(.9,.95,1))):
    bpy.ops.mesh.primitive_cylinder_add(vertices=16,radius=radius,depth=(b-a).length);o=bpy.context.object;o.name='Preview blade guide';o.parent=weapon_anchor;o.location=(a+b)*.5;o.rotation_mode='QUATERNION';o.rotation_quaternion=Vector((0,0,1)).rotation_difference(b-a);mat=bpy.data.materials.new('Display blade');mat.diffuse_color=(*color,1);o.data.materials.append(mat)

directions=[('s_top','W'),('s_trbl','W+A'),('s_tlbr','W+D'),('s_rl','A'),('s_lr','D'),('s_brtl','S+A'),('s_bltr','S+D')]
actions={};pose_checks={}
for clip,direction in [('j_guard','Guard')]+directions:
    length=lengths[clip];times=sorted(set([i/60 for i in range(math.floor(length*60)+1)]+[length]))
    frames=[(t,native(clip,t)) for t in times]
    actions[clip,False]=bake('Yellow - '+direction,frames,length)
    if clip!='j_guard':actions[clip,True]=bake('Yellow walk - '+direction,[(t,displayed(p,t,True)) for t,p in frames],length)
    pose_checks[clip]=dict(length=length,samples=len(times))
    print('YELLOW_BAKED',kind,clip,flush=True)
for image in bpy.data.images:
    if image.source=='FILE' and image.has_data and not image.packed_file:image.pack()
rig.animation_data.action=actions['j_guard',False];scene.frame_start=1;scene.frame_end=round(1+lengths['s_top']*120);scene.frame_set(1)
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(dest/'yellow_weight_review.blend'),check_existing=False)

# Verify source key poses and independent linear skinning in the saved rig.
max_bone=0.;max_skin=0.;max_attachment=0.;comparisons=0
for clip,_ in directions:
    for walking in (False,True):
        rig.animation_data.action=actions[clip,walking]
        for t in (0.,lengths[clip]*.25,lengths[clip]*.5,lengths[clip]*.75,lengths[clip]):
            # Test actual60Hz baked keys; fractional in-between fidelity is separate.
            t=min(lengths[clip],round(t*60)/60);frame=1+t*120;scene.frame_set(math.floor(frame),subframe=frame-math.floor(frame))
            expected=worlds(displayed(native(clip,t),t,True) if walking else native(clip,t));actual={b.name:rig.matrix_world@b.matrix for b in ordered}
            max_bone=max(max_bone,max(mat_error(expected[n],actual[n]) for n in actual));max_attachment=max(max_attachment,mat_error(weapon_anchor.matrix_world,actual['rhand']))
            deps=bpy.context.evaluated_depsgraph_get()
            for obj in skin_objects:
                ev=obj.evaluated_get(deps);mesh=ev.to_mesh();points=[ev.matrix_world@v.co for v in mesh.vertices];ev.to_mesh_clear();tree=KDTree(len(points))
                for i,p in enumerate(points):tree.insert(p,i)
                tree.balance();node=lookup[obj.name]
                for v,row in zip(node.verts,trial[obj.name]):
                    p=sum(((expected[bone]@rest[bone].inverted()@rest[node.name]@Vector(v))*w for bone,w in row),Vector());max_skin=max(max_skin,tree.find(p)[2]);comparisons+=1
assert max_bone<1e-5,max_bone
assert max_skin<2e-5,max_skin
assert max_attachment<1e-5,max_attachment
report=dict(body=kind,resource=resource,protected=protected,changed_vertices=changed,palettes=palettes,seams=seams,frozen_original_boundaries=sorted(frozen_boundaries),pose_checks=pose_checks,maximum_bone_error=max_bone,maximum_skin_error_m=max_skin,skin_samples=comparisons,maximum_attachment_error=max_attachment,animation='existing per-body yellow clips unchanged;60Hz bake',blade_guide=[list(a),list(b)],limitations='offline preview; live aim/damping/chains/native handoffs excluded; no runtime export')
(dest/'validation.json').write_text(json.dumps(report,indent=2))
print('YELLOW_WEIGHT_SCENE_PASS',kind,max_bone,max_skin,flush=True)
