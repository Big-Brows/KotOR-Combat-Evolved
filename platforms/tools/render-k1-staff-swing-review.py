"""Assemble and render isolated staff proposals; no live-game modifications.

Blender background entry point. Existing ffmpeg encodes labelled comparisons.
The preview applies the current MotionSpine math to forward-walk overlays;
live arm display damping/transition state is intentionally not simulated.
"""
from pathlib import Path
import bisect
import hashlib
import json
import math
import os
import shutil
import subprocess
import sys

import bpy
from mathutils import Matrix, Quaternion, Vector

repo = Path(__file__).resolve().parents[2]
engage = repo / 'platforms/out/k1-engage-prototype'
review_root = engage / 'staff-swing-review'
review = review_root / os.environ.get('K1_STAFF_REVIEW_ATTEMPT', 'candidate-01')
proposals = json.loads((review/'poses.json').read_text())
generation = json.loads((review/'generation.json').read_text())
assert not (review/'staff_swing_review.blend').exists(), 'Preserve existing review files.'
ffmpeg = shutil.which('ffmpeg')
assert ffmpeg, 'Existing ffmpeg required; do not install without permission.'
sys.path.insert(0, str(repo/'platforms/out/kotor-animation-import/tools'))
from io_scene_kotor.format.mdl.reader import MdlReader

def nodes(n):
    return [n]+[v for c in n.children for v in nodes(c)]

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def mat_error(a, b):
    return max(abs(x-y) for aa, bb in zip(a, b) for x, y in zip(aa, bb))

model = MdlReader(str(engage/'elbow-arena/profile/override/k1jastaff.mdl')).load()
nn = nodes(model.root_node)
parents = {n.name: n.parent.name if n.parent else None for n in nn}
bind = {n.name: Matrix.LocRotScale(Vector(n.position), Quaternion(n.orientation), Vector((n.scale,)*3)) for n in nn}
clips = {a.name: {n.name: n.keyframes for n in nodes(a.root_node)} for a in model.animations}
lengths = {a.name: a.length for a in model.animations}

def worlds(local):
    result = {}
    for n in nn:
        result[n.name] = result[parents[n.name]]@local[n.name] if parents[n.name] else local[n.name]
    return result

def interpolate_keys(keys, t, quaternion=False):
    if len(keys) == 1 or t <= keys[0][0]:
        a = b = keys[0]
        u = 0.
    elif t >= keys[-1][0]:
        a = b = keys[-1]
        u = 0.
    else:
        index = bisect.bisect_right([r[0] for r in keys], t)-1
        a, b = keys[index:index+2]
        u = (t-a[0])/(b[0]-a[0])
    if quaternion:
        qa, qb = Quaternion((a[4], *a[1:4])), Quaternion((b[4], *b[1:4]))
        return qa.normalized().slerp(qb.normalized(), u).normalized()
    return Vector(a[1:4]).lerp(Vector(b[1:4]), u)

def native(clip, t, loop=False):
    t = t%lengths[clip] if loop else min(t, lengths[clip])
    result = {}
    for n in nn:
        p, q, s = bind[n.name].decompose()
        keys = clips[clip].get(n.name, {})
        if keys.get('position'):
            p += interpolate_keys(keys['position'], t)
        if keys.get('orientation'):
            q = interpolate_keys(keys['orientation'], t, True)
        result[n.name] = Matrix.LocRotScale(p, q, s)
    return result

candidate_frames = {}
walking_candidate_frames = {}
for clip, data in proposals.items():
    if data.get('walk_poses'):
        walking_candidate_frames[clip] = [(float(t), {n: Matrix.LocRotScale(Vector(v[:3]), Quaternion((v[6], *v[3:6])), Vector((1,1,1))) for n,v in pose.items()}) for t,pose in data['walk_poses'].items()]
    candidate_frames[clip] = [(float(t), {n: Matrix.LocRotScale(Vector(v[:3]),
        Quaternion((v[6], *v[3:6])), Vector((1,1,1))) for n, v in pose.items()}) for t, pose in data['poses'].items()]

def sample_frames(frames, t):
    t = min(t, frames[-1][0])
    index = max(0, min(len(frames)-2, bisect.bisect_right([f[0] for f in frames], t)-1))
    ta, a = frames[index]
    tb, b = frames[index+1]
    u = min(1., max(0., (t-ta)/(tb-ta)))
    result = {}
    for name in a:
        p, q, s = a[name].decompose()
        p2, q2, s2 = b[name].decompose()
        result[name] = Matrix.LocRotScale(p.lerp(p2,u), q.slerp(q2,u).normalized(), s.lerp(s2,u))
    return result

def candidate(clip, t, walking=False):
    return sample_frames(walking_candidate_frames.get(clip,candidate_frames[clip]) if walking else candidate_frames[clip],t)

comparison_frames = {}
if generation.get('comparison_baseline'):
    comparison_path = review_root/generation['comparison_baseline']/'poses.json'
    assert comparison_path.resolve().is_relative_to(review_root.resolve())
    for clip,data in json.loads(comparison_path.read_text()).items():
        comparison_frames[clip] = [(float(t), {n: Matrix.LocRotScale(Vector(v[:3]), Quaternion((v[6], *v[3:6])), Vector((1,1,1))) for n,v in pose.items()}) for t,pose in data['poses'].items()]

def comparison(clip,t):
    return sample_frames(comparison_frames[clip],t) if clip in comparison_frames else native(clip,t)

upper = set()
for n in nn:
    p = n
    while p and p.name != 'torso_g':
        p = p.parent
    if p:
        upper.add(n.name)
upper.add('j_motion')

def wrap(a):
    return (a+math.pi)%(2*math.pi)-math.pi

def displayed(full, t, walking):
    if not walking:
        return full
    local = native('j_walk', t, True)
    for name in upper:
        local[name] = full[name].copy()
    w = worlds(local)
    reference = full['pelvis_g'].to_quaternion().normalized()
    mixed = (w['pelvis_g'].to_quaternion().normalized()@reference.inverted()@w['j_motion'].to_quaternion().normalized()).normalized()
    forward, right = mixed@Vector((0,1,0)), mixed@Vector((1,0,0))
    ax = -math.atan2(forward.z, math.hypot(forward.x, forward.y))
    ay = wrap(math.atan2(forward.y,forward.x)-math.pi*.5)
    az = math.atan2(right.z, math.hypot(right.x,right.y))
    pitch, turn, roll = wrap(-ax), wrap(-ay), wrap(-az)
    axes = bind['j_motion'].to_quaternion().normalized()
    for name, py, yr in [('torso_g',.4,.45),('torsoMid_g',.4,.35),('torsoUpr_g',.2,.2)]:
        p,q,s = local[name].decompose()
        correction = Quaternion(axes@Vector((0,0,1)),roll*yr)@Quaternion(axes@Vector((0,1,0)),pitch*py)@Quaternion(axes@Vector((1,0,0)),turn*yr)
        local[name] = Matrix.LocRotScale(p,(q@correction).normalized(),s)
    return local

source_blend = engage/'staff-editor/attacks/kotor_staff_attacks.blend'
bpy.ops.wm.open_mainfile(filepath=str(source_blend), use_scripts=False, load_ui=True)
scene = bpy.context.scene
rig = bpy.data.objects['KOTOR_Staff_Edit']
skin_weight_trial = None
if generation.get('skin_weight_trial'):
    sys.path.insert(0,str(repo/'platforms/tools'))
    from k1staff_preview_weights import PreviewSkinWeights
    skin_weight_trial = PreviewSkinWeights(review/generation['skin_weight_trial'])
    skin_weight_trial.apply(True)
    (review/'blender-weight-mapping.json').write_text(json.dumps(dict(maximum_bind_error_m=skin_weight_trial.maximum_bind_error,maximum_original_weight_error=skin_weight_trial.maximum_old_weight_error,vertices=sum(map(len,skin_weight_trial.mapping.values()))),indent=2))
scene.render.fps = 120
scene.render.fps_base = 1
ordered = list(rig.pose.bones)

def curves(action):
    return [c for l in action.layers for s in l.strips for b in s.channelbags for c in b.fcurves]

def bake(label, frames, length):
    action = bpy.data.actions.new(label)
    action.use_fake_user = True
    action.use_frame_range = True
    action.frame_start = 1
    action.frame_end = 1+length*120
    rig.animation_data.action = action
    last = {}
    for t, pose in frames:
        frame = 1+t*120
        scene.frame_set(math.floor(frame), subframe=frame-math.floor(frame))
        w = worlds(pose)
        for bone in ordered:
            bone.matrix = w[bone.name]
            bpy.context.view_layer.update()
        for bone in ordered:
            q = bone.rotation_quaternion.copy()
            if bone.name in last and last[bone.name].dot(q) < 0:
                q.negate()
            bone.rotation_quaternion = q
            last[bone.name] = q.copy()
            for prop in ('location','rotation_quaternion','scale'):
                bone.keyframe_insert(prop, frame=frame, group=bone.name)
    for c in curves(action):
        for key in c.keyframe_points:
            key.interpolation = 'LINEAR'
    return action

directions = [('s_top','W'),('s_trbl','W+A'),('s_tlbr','W+D'),('s_rl','A'),
              ('s_lr','D'),('s_brtl','S+A'),('s_bltr','S+D')]
actions = {}
metrics = {}
for clip, direction in directions:
    data = proposals[clip]
    frames = candidate_frames[clip]
    action = bake('Review - '+direction, frames, data['length'])
    action['source_clip'] = clip
    action['preview_only'] = True
    for label, t in [('PRE-SWING',0),('SWING',data['strike'][0]),('POST-SWING',data['strike'][1]),('Return midpoint' if data['policy'].get('mode') in ('source-axial-followthrough', 'unedited-stock-source', 'phase-twist-reduction') else 'Follow-through peak',data['post_peak']),('End',data['length'])]:
        marker = action.pose_markers.new(label)
        marker.frame = round(1+t*120)
    actions[clip,True,False] = action
    old_frames = [(i/120, comparison(clip,i/120)) for i in range(round(lengths[clip]*120)+1)]
    actions[clip,False,False] = bake('Before - '+direction, old_frames, lengths[clip])
    # Measure the shoulder heading through the new post-swing phase.
    def measure(pose):
        w = worlds(pose)
        right = w['rbicep_g'].translation-w['lbicep_g'].translation
        return math.degrees(math.atan2(right.y,right.x))
    end = data['strike'][1]
    h0 = measure(candidate(clip,end))
    h1 = measure(candidate(clip,data['post_peak']))
    difference = math.degrees(wrap(math.radians(h1-h0)))
    sign = 1 if data['policy']['turn']>0 else -1
    if data['policy'].get('mode') not in ('source-axial-followthrough', 'unedited-stock-source', 'phase-twist-reduction'):
        assert sign*difference >= abs(data['policy']['turn'])-.25, (clip,difference)
    post_steps = []
    previous = None
    for t, local in frames:
        if previous and t>=end:
            for name in ('rbicep_g','lbicep_g','Rforearm_g','Lforearm_g','Rhand_g','Lhand_g'):
                q = local[name].to_quaternion().normalized()
                p = previous[name].to_quaternion().normalized()
                post_steps.append(min(q.rotation_difference(p).angle,2*math.pi-q.rotation_difference(p).angle))
        previous = local
    metrics[clip] = dict(post_heading_start=h0,post_heading_peak=h1,continued_degrees=difference,
        maximum_post_arm_step_120hz=max(post_steps),held_grip_error=data['maximum_held_grip_error_m'])
    print('STAFF_REVIEW_BAKED',direction,metrics[clip],flush=True)

instructions = '''STAFF SWING REVIEW - PREVIEW ONLY

Choose Review - W / W+A / W+D / A / D / S+A / S+D in the Action Editor.
Before - ... preserves current base motions for comparison. Original editor
Actions are retained. Review copies never overwrite the source editor or game.
Action markers identify PRE-SWING, SWING, POST-SWING, a return sample and end.
Unedited-stock proposals use authored source paths with rig conversion only.
The earlier stock-motion mimic preserves source return timing and axial turning.
Older fixed-turn proposals add0.18 source seconds; their policy is stored separately.
W straightening is confined to the downward pass; its pre-swing/spin is free.
Animations keep the compact body and solve the original authored hand contact.
Live aim, arm display damping and state/chain transitions are not demonstrated.
Movies include stationary and forward Shift-walk with current MotionSpine math.
No gameplay timing, damage, AI, block or completed stance changes are deployed.
'''
bpy.data.texts.new('START HERE - Swing review').write(instructions)
(review/'README.txt').write_text(instructions,encoding='utf-8')
rig.animation_data.action = actions['s_top',True,False]
scene.frame_start = 1
scene.frame_end = math.ceil(1+proposals['s_top']['length']*120)
scene.frame_set(1)
bpy.context.view_layer.objects.active = rig
bpy.context.preferences.filepaths.save_version = 0
if hasattr(bpy.context.preferences.filepaths,'file_preview_type'):
    bpy.context.preferences.filepaths.file_preview_type='NONE'
bpy.ops.wm.save_as_mainfile(filepath=str(review/'staff_swing_review.blend'),check_existing=False)
(review/'motion-checks.json').write_text(json.dumps(metrics,indent=2),encoding='utf-8')

# Render-only walk variants. They do not change the saved editable scene.
for clip,direction in directions:
    for proposed in (False,True):
        length = proposals[clip]['length'] if proposed else lengths[clip]
        times = [i/120 for i in range(math.ceil(length*120)+1)]
        frames = [(t,displayed(candidate(clip,t,True) if proposed else comparison(clip,t),t,True)) for t in times]
        actions[clip,proposed,True] = bake('Render walk '+('after ' if proposed else 'before ')+direction,frames,length)
        actions[clip,proposed,True].use_fake_user = False

world = bpy.data.worlds.new('Neutral preview background')
world.color=(.045,.045,.045)
scene.world = world
cam_data=bpy.data.cameras.new('Whole-body motion review camera')
cam=bpy.data.objects.new('Whole-body motion review camera',cam_data)
scene.collection.objects.link(cam)
cam.location=(0,-3.5,1.75)
cam.rotation_euler=(Vector((0,0,1.10))-cam.location).to_track_quat('-Z','Y').to_euler()
cam_data.lens=25.5
cam_data.sensor_width=36
scene.camera=cam
scene.render.engine='BLENDER_WORKBENCH'
scene.render.resolution_x=720
scene.render.resolution_y=540
scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'
scene.display.shading.light='STUDIO'
scene.display.shading.color_type='TEXTURE'
scene.display.shading.show_shadows=True
scene.display.shading.show_cavity=True
scene.display.shading.background_type='WORLD'

def text(name,location,size):
    data=bpy.data.curves.new(name,'FONT')
    data.size=size
    obj=bpy.data.objects.new(name,data)
    scene.collection.objects.link(obj)
    obj.parent=cam
    obj.location=location
    return data

title=text('Direction and comparison',(-1.95,1.40,-3),.12)
phase=text('Swing phase',(-1.95,1.20,-3),.10)
footer=text('Preview limits',(-1.95,-1.45,-3),.070)
crosshair=text('Crosshair',(-.027,-.035,-1),.09)
crosshair.body='+'
footer.body='0.5x source speed | preview only | no live arm damping'
frames_dir=review/'frames'
frames_dir.mkdir(exist_ok=True)
video_dir=review/'videos'
video_dir.mkdir(exist_ok=True)
videos=[]
for index,(clip,direction) in enumerate(directions):
    selected = os.environ.get('K1_STAFF_RENDER_CLIPS','').split(',')
    if selected != [''] and clip not in selected:
        continue
    data=proposals[clip]
    count=math.ceil(data['length']*60)+20
    side_videos=[]
    for proposed in (False,True):
        if skin_weight_trial:
            skin_weight_trial.apply(proposed)
        label='after' if proposed else 'before'
        directory=frames_dir/(clip+'-'+label)
        directory.mkdir(exist_ok=True)
        image_index=0
        for walking in (False,True):
            rig.animation_data.action=actions[clip,proposed,walking]
            for f in range(count):
                t=max(0.,min(data['length'],(f-6)/60))
                frame=1+t*120
                scene.frame_set(math.floor(frame),subframe=frame-math.floor(frame))
                title.body=direction+' | '+('PROPOSED' if proposed else 'CURRENT')+' | '+('SHIFT-WALK' if walking else 'STANDING')
                length=data['length'] if proposed else lengths[clip]
                stage='PRE-SWING' if t<data['strike'][0] else 'SWING' if t<data['strike'][1] else 'POST-SWING' if t<length else 'END'
                phase.body=stage+f'   source {min(t,length):.2f}s'
                scene.render.filepath=str(directory/f'{image_index:05d}.png')
                bpy.ops.render.render(write_still=True)
                image_index+=1
        movie=video_dir/(clip+'-'+label+'.mp4')
        subprocess.run([ffmpeg,'-hide_banner','-loglevel','error','-y','-framerate','30','-i',str(directory/'%05d.png'),
                        '-c:v','libx264','-crf','19','-pix_fmt','yuv420p',str(movie)],check=True)
        side_videos.append(movie)
    movie=video_dir/(f'{index+1:02d}-'+direction.replace('+','_')+'.mp4')
    subprocess.run([ffmpeg,'-hide_banner','-loglevel','error','-y','-i',str(side_videos[0]),'-i',str(side_videos[1]),
                    '-filter_complex','hstack=inputs=2','-c:v','libx264','-crf','19','-pix_fmt','yuv420p',str(movie)],check=True)
    videos.append(movie)
    print('STAFF_REVIEW_VIDEO_READY',movie.name,flush=True)
concat=video_dir/'playlist.txt'
concat.write_text(''.join("file '"+p.name+"'\n" for p in videos),encoding='utf-8')
subprocess.run([ffmpeg,'-hide_banner','-loglevel','error','-y','-f','concat','-safe','0','-i',str(concat),
                '-c','copy',str(video_dir/'all-seven-swings.mp4')],check=True)
for path,expected in generation['protected_inputs'].items():
    assert digest(repo/path)==expected,path
(review/'render-complete.json').write_text(json.dumps(dict(videos=[p.name for p in videos],
    overview='all-seven-swings.mp4',blend_sha256=digest(review/'staff_swing_review.blend'),
    protected_inputs_unchanged=True, fps=30,source_playback_speed=.5,
    walking='native forward j_walk overlay with current MotionSpine; live arm damping/transitions excluded'),indent=2))
print('STAFF_SWING_REVIEW_RENDER_COMPLETE',flush=True)
