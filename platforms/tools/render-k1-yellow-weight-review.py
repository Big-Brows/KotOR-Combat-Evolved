"""Render/reload-check separate yellow-stance body weight previews."""
from pathlib import Path
import hashlib
import json
import math
import os
import shutil
import subprocess
import sys

import bpy
from mathutils import Vector

repo=Path(__file__).resolve().parents[2]
kind=os.environ.get('K1_YELLOW_REVIEW_BODY','player')
assert kind in ('player','dark')
dest=repo/'platforms/out/k1-engage-prototype/yellow-weight-review'/(kind+'-02')
report=json.loads((dest/'validation.json').read_text())
out=dest/'movies';assert not out.exists();out.mkdir()
sys.path.insert(0,str(repo/'platforms/tools'))
from k1staff_preview_weights import PreviewSkinWeights
bpy.ops.wm.open_mainfile(filepath=str(dest/'yellow_weight_review.blend'),use_scripts=False)
scene=bpy.context.scene;rig=bpy.data.objects['Yellow_Review_'+kind]
weights=PreviewSkinWeights(dest/'skin-weights.json',loaded_trial=True)
assert all(image.packed_file for image in bpy.data.images if image.source=='FILE' and image.has_data)
assert all(not modifier.use_deform_preserve_volume for obj in bpy.data.objects if obj.type=='MESH' for modifier in obj.modifiers if modifier.type=='ARMATURE')
assert len([a for a in bpy.data.actions if a.name.startswith('Yellow')])==15
data=json.loads((dest/'skin-weights.json').read_text())
for name,mapping in weights.mapping.items():
    obj=bpy.data.objects[name]
    for vertex,i in zip(obj.data.vertices,mapping):
        actual={obj.vertex_groups[g.group].name:g.weight for g in vertex.groups if g.weight>1e-8}
        expected=dict(data['weights'][name][i])
        assert max(abs(actual.get(k,0)-expected.get(k,0)) for k in actual.keys()|expected.keys())<1e-5
(dest/'reload-validation.json').write_text(json.dumps(dict(pass_=True,scripts_disabled=True,weight_mapping_exact=True,packed_textures=True,actions=15,linear_skinning=True),indent=2))

camdata=bpy.data.cameras.new('Front comparison camera');cam=bpy.data.objects.new('Front comparison camera',camdata);scene.collection.objects.link(cam)
cam.location=(1.4,2.8,1.85);cam.rotation_euler=(Vector((0,0,1.1))-cam.location).to_track_quat('-Z','Y').to_euler();camdata.lens=47;camdata.sensor_width=36;scene.camera=cam
world=bpy.data.worlds.new('Review background');world.color=(.045,.045,.045);scene.world=world
scene.render.engine='BLENDER_WORKBENCH';scene.render.resolution_x=720;scene.render.resolution_y=720;scene.render.resolution_percentage=100;scene.render.image_settings.file_format='PNG'
scene.display.shading.light='STUDIO';scene.display.shading.color_type='TEXTURE';scene.display.shading.show_shadows=True;scene.display.shading.show_cavity=True;scene.display.shading.background_type='WORLD'
def label(name,position,size):
    d=bpy.data.curves.new(name,'FONT');d.size=size;o=bpy.data.objects.new(name,d);scene.collection.objects.link(o);o.parent=cam;o.location=position;return d
title=label('Comparison',(-1.1,1.1,-3),.075)
footer=label('Scope',(-1.1,-1.11,-3),.045);footer.body='Yellow stance | same motion | weights only | 0.5x source speed'
directions=[('s_top','W'),('s_trbl','W+A'),('s_tlbr','W+D'),('s_rl','A'),('s_lr','D'),('s_brtl','S+A'),('s_bltr','S+D')]
ffmpeg=shutil.which('ffmpeg');assert ffmpeg
movies=[]
for index,(clip,direction) in enumerate(directions):
    length=report['pose_checks'][clip]['length'];count=math.ceil(length*60)+16;sides=[]
    for after in (False,True):
        weights.apply(after);directory=out/(clip+('-after' if after else '-before'));directory.mkdir();image_index=0
        for walking in (False,True):
            rig.animation_data.action=bpy.data.actions['Yellow '+('walk - ' if walking else '- ')+direction]
            title.body=('PLAYER' if kind=='player' else 'DARK JEDI')+' | '+direction+' | '+('NEW WEIGHTS' if after else 'ORIGINAL')+' | '+('WALK' if walking else 'STAND')
            for f in range(count):
                t=max(0.,min(length,(f-5)/60));frame=1+t*120;scene.frame_set(math.floor(frame),subframe=frame-math.floor(frame))
                scene.render.filepath=str(directory/f'{image_index:05d}.png');bpy.ops.render.render(write_still=True);image_index+=1
        movie=out/(directory.name+'.mp4');subprocess.run([ffmpeg,'-hide_banner','-loglevel','error','-y','-framerate','30','-i',str(directory/'%05d.png'),'-c:v','libx264','-crf','19','-pix_fmt','yuv420p',str(movie)],check=True);sides.append(movie)
    movie=out/(f'{index+1:02d}-'+direction.replace('+','_')+'.mp4');subprocess.run([ffmpeg,'-hide_banner','-loglevel','error','-y','-i',str(sides[0]),'-i',str(sides[1]),'-filter_complex','hstack=inputs=2','-c:v','libx264','-crf','19','-pix_fmt','yuv420p',str(movie)],check=True);movies.append(movie)
    print('YELLOW_WEIGHT_MOVIE_READY',kind,movie.name,flush=True)
playlist=out/'playlist.txt';playlist.write_text(''.join("file '"+p.name+"'\n" for p in movies))
subprocess.run([ffmpeg,'-hide_banner','-loglevel','error','-y','-f','concat','-safe','0','-i',str(playlist),'-c','copy',str(out/'all-yellow-attacks.mp4')],check=True)
for p,h in report['protected'].items():assert hashlib.sha256((repo/p).read_bytes()).hexdigest()==h
(dest/'render-complete.json').write_text(json.dumps(dict(body=kind,videos=[p.name for p in movies],overview='all-yellow-attacks.mp4',protected_assets_unchanged=True,source_speed=.5,standing_then_walking=True),indent=2))
print('YELLOW_WEIGHT_RENDER_COMPLETE',kind,flush=True)
