"""Blender background recipe; owned inputs and all output stay in platforms/out.

Reuses the preserved, applied single-saber solver. Does not install game files.
Run from the checkout root with Blender --background --factory-startup --python.
"""
from pathlib import Path
import hashlib
import json
import math
import os
import shutil
import sys
import subprocess
import zipfile

import builtins
import bpy
from mathutils import Vector

repo = Path(__file__).resolve().parents[2]
workspace = repo / 'platforms/out'
engage = workspace / 'k1-engage-prototype'
output = engage / 'staff-prototype'
subprocess.run(['git', '-C', str(repo), 'check-ignore', '--quiet', str(output / 'assets')], check=True)
output.mkdir(exist_ok=True)
source_dir = output / 'source'; source_dir.mkdir(exist_ok=True)
archive = Path(os.environ['ProgramFiles(x86)']) / 'Steam/steamapps/common/Jedi Academy/GameData/base/assets1.pk3'
with zipfile.ZipFile(archive) as z:
    weapon_bytes = z.read('models/weapons2/saber_dual_1/saber_dual_1.glm')
(source_dir / 'saber_dual_1.glm').write_bytes(weapon_bytes)
sys.path.insert(0, str(workspace / 'jka-animation-import/tools'))
from jka_importer import JAG2GLA, JAG2Math, JAG2GLM

cfg = workspace / 'jka-animation-import/source/models/players/_humanoid/animation.cfg'
gla = cfg.with_name('_humanoid.gla')
aliases = {
    'j_guard': 'BOTH_SABERSTAFF_STANCE',
}
staff_attacks = {
    'top': ('T_', 'T__B_', 'B_'), 'tlbr': ('TL', 'TL_BR', 'BR'),
    'trbl': ('TR', 'TR_BL', 'BL'), 'lr': ('_L', '_L__R', '_R'),
    'rl': ('_R', '_R__L', '_L'), 'bltr': ('BL', 'BL_TR', 'TR'),
    'brtl': ('BR', 'BR_TL', 'TL'),
}
attack_sources = {short: ['BOTH_S7_S7_' + start, 'BOTH_A7_' + cut, 'BOTH_R7_' + end + '_S7']
                  for short, (start, cut, end) in staff_attacks.items()}
# Defender-facing mapping from the established JKA LS_PARRY_* convention.
# Lower-right is BL and lower-left is BR, as for the completed stance.
staff_blocks = {label: 'BOTH_P7_S7_' + suffix for label, suffix in
                {'up': 'T_', 'ur': 'TR', 'ul': 'TL', 'lr': 'BL', 'll': 'BR'}.items()}
names = set(aliases.values()) | set(staff_blocks.values()) | {n for clips in attack_sources.values() for n in clips}
table = {}
for line in cfg.read_text().splitlines():
    f = line.split()
    if f and f[0] in names:
        table[f[0]] = dict(first=int(f[1]), count=int(f[2]), loop=int(f[3]), fps=int(f[4]))
assert set(table) == names
data = json.loads((engage / 'corrections/jka-grounded-motion.json').read_text())
for name, c in table.items():
    loader = JAG2GLA.GLA()
    ok, msg = loader.loadFromFile(str(gla), JAG2GLA.AnimationLoadMode.RANGE, c['first'], c['count'])
    assert ok, msg
    poses = loader.animation.computeAbsoluteFrameTransforms(loader.skeleton)
    frames = [{loader.skeleton.bones[i].name: [list(r) for r in m] for i, m in p.items()} for p in poses]
    if c['fps'] < 0:
        frames.reverse()
    assert all(math.isfinite(v) for f in frames for m in f.values() for r in m for v in r)
    data['clips'][name] = {**c, 'frames': frames, 'reversed_for_negative_fps': c['fps'] < 0}
(output / 'staff-motion.json').write_text(json.dumps(data))

# Compare actual weapon bolt axes, rather than assuming a single saber convention.
glm = JAG2GLM.GLM()
ok, msg = glm.loadFromFile(str(output / 'source/saber_dual_1.glm'))
assert ok, msg
tags = {}
for info, surface in zip(glm.surfaceDataCollection.surfaces, glm.LODCollection.LODs[0].surfaces):
    if info.flags & 1:
        tags[info.name.decode().rstrip('\0')] = {'verts': [v.co for v in surface.vertices]}
assert '*blade1' in tags and '*blade2' in tags
(output / 'staff-tags.json').write_text(json.dumps(tags, indent=2))
staff_tags = tags

# Keep helper reports isolated; preserve the accepted production skin, not an old
# reconstruction of its weights. Only animation controllers will be replaced.
solver_dir = output / 'solver'
solver_dir.mkdir(exist_ok=True)
baseline_profile = engage / 'elbow-arena/profile'
profile = baseline_profile  # Extend the existing arena; do not clone scenes/saves/config.
for path in (engage / 'rig-candidate').iterdir():
    if path.suffix in ('.py', '.json'):
        if path.suffix == '.py':
            code = path.read_text().replace("candidate.parent/'profile'", "engage/'profile'")
            (solver_dir / path.name).write_text(code)
        else:
            shutil.copy2(path, solver_dir / path.name)
settings = json.loads((engage / 'rig-candidate/retarget-validation.json').read_text())['settings']
for key, value in settings.items():
    if value is None:
        os.environ.pop(key, None)
    else:
        os.environ[key] = value
recipe = (engage / 'elbow-arena/generate-candidate-applied.py').read_text()
prefix = recipe.split('samples={};worlds={};lengths={};hit_windows={};clip_report={}')[0]
prefix = prefix.replace('candidate=Path(__file__).resolve().parent', 'candidate=solver_dir')
prefix = prefix.replace("root=candidate.parent/'corrections';workspace=root.parent.parent", "root=engage/'corrections';workspace=root.parent.parent")
prefix = prefix.replace("data=json.loads((root/'jka-grounded-motion.json').read_text())", "data=json.loads((output/'staff-motion.json').read_text())")
# OpenJK's normal surface-bolt convention uses fixed sides0/2, not the
# longest/shortest edges recalculated after each deformed triangle sample.
# Pinned reference260c59c:code/rd-common/mdx_format.h and G2_ProcessSurfaceBolt2.
assert prefix.count('sides[max(range(3),key=lambda i:sides[i].length)]') == 1
assert prefix.count('sides[min(range(3),key=lambda i:sides[i].length)]') == 1
prefix = prefix.replace('sides[max(range(3),key=lambda i:sides[i].length)]', 'sides[0]')
prefix = prefix.replace('sides[min(range(3),key=lambda i:sides[i].length)]', 'sides[2]')
# Keep successful legacy solutions intact; extend only its failed staff poses.
# The diagnosed fixed damping swamps two weak axes near left-arm extension.
refinement_record = "        metrics['pair_refinement'].append({'pose':current_context,'before':best_pair_error,'after':error.length})"
adaptive_fallback = r"""
        legacy_error=error.length
        if error.length>.00005:
            legacy_pair=(target.copy(),error.copy(),ll,ww)
            target,error,ll,ww=staff_initial_pair
            import numpy as np
            for adaptive_iteration in range(16):
                columns=[]
                for axis_index in range(3):
                    step=Vector((0,0,0));step[axis_index]=.001
                    plus,_,_=evaluate_pair(target+step);minus,_,_=evaluate_pair(target-step)
                    columns.append((plus-minus)/.002)
                u,sv,vh=np.linalg.svd(np.array(Matrix(columns).transposed()))
                damping=max(1e-10,float(sv[-1]**2*.01))
                delta=Vector(-(vh.T@((sv/(sv*sv+damping))*(u.T@np.array(error)))))
                if delta.length>.015:delta*=.015/delta.length
                improved=False
                for scale in (1,.5,.25,.125,.0625,.03125):
                    next_error,next_ll,next_ww=evaluate_pair(target+delta*scale)
                    if next_error.length<error.length:
                        target+=delta*scale;error,ll,ww=next_error,next_ll,next_ww;improved=True;break
                if error.length<.00001 or not improved:break
            if legacy_pair[1].length<error.length:target,error,ll,ww=legacy_pair
            # Projected wrist domains can have a flat local basin. Poll bounded,
            # deterministic feasible-target directions before accepting failure;
            # retain the exact same hand-spacing objective and best legacy pose.
            if error.length>.00005:
                origin=target.copy();seed_world=globals(seed)
                directions=[Vector((1,0,0)),Vector((0,1,0)),Vector((0,0,1))]
                directions += [(seed_world[arm[s]['names'][0]].translation-origin).normalized() for s in ('R','L')]
                for radius in (.002,.005,.01,.02):
                    for direction in directions:
                        for sign in (-1,1):
                            candidate=origin+direction*(radius*sign)
                            ne,nl,nw=evaluate_pair(candidate)
                            if ne.length<error.length:target,error,ll,ww=candidate,ne,nl,nw
                    if error.length<.00001:break
            if error.length<best_pair_error:local=ll;world=ww
        metrics['pair_refinement'][-1].update(legacy_after=legacy_error,after=error.length)
"""
assert prefix.count(refinement_record) == 1
initial_pair = '        error,ll,ww=evaluate_pair(target)'
assert prefix.count(initial_pair) == 1
prefix = prefix.replace(initial_pair, initial_pair + '\n        staff_initial_pair=(target.copy(),error.copy(),ll,ww)')
prefix = prefix.replace(refinement_record, refinement_record + adaptive_fallback)
# Staff hand reattachment must approach the paired target before contact, rather
# than switch from independent shoulder-scaled reach to the hilt on one sample.
# Keep the completed solver untouched. Released hands retain their source target
# away from attachment and use the same wrist compatibility as held hands.
free_arm = "    if not (source_grip and twohand):world=ik(local,world,'L',targets['L'],pose,goals['Lhand_g'])"
assert prefix.count(free_arm) == 1
prefix = prefix.replace(free_arm, r"""
    if source_grip and not twohand:
        independent=targets['L'].copy();lq=goals['Lhand_g']
        expected=world['rhand'].translation+source_gap
        weight=staff_attachment_weight
        for _ in range(16):
            attached=expected-lq@local['lhand'].translation
            goal=independent.lerp(attached,weight)
            world=ik(local,world,'L',goal,pose,lq)
            lq=compatible_wrist(world,'L',raw_goals['Lhand_g'],pose)
            world=set_world_q(local,world,'Lhand_g',lq)
    elif not (source_grip and twohand):
        world=ik(local,world,'L',targets['L'],pose,goals['Lhand_g'])
""")
staff_attachment_weight = 0.
staff_grip_gap = None
# Body compaction must not redefine the authored relationship of hands to hilt.
# Supply the original hilt-local gap in the compacted right-hand frame; retain
# the existing paired IK, wrist limits, marker offsets and error checks.
gap_definition = "source_gap=axis@(tag_l-tag_r)*unit"
assert prefix.count(gap_definition) == 1
prefix = prefix.replace(gap_definition, "source_gap=staff_grip_gap.copy() if staff_grip_gap is not None else axis@(tag_l-tag_r)*unit")
exec(compile(prefix, '<preserved-applied-retarget-solver>', 'exec'), builtins.globals())

preserved_matrix_sample = sample_local
preserved_matrix_join = joined_source
preserved_top_sources = {'BOTH_S7_S7_T_', 'BOTH_A7_T__B_', 'BOTH_R7_B__S7'}

def blend_staff_locals(a, b, fraction):
    # Ghoul2 can skin affine matrix interpolation directly. KotOR's rigid Part
    # rotations cannot represent the intermediate shear; convert key rotations
    # before interpolating, rather than extracting quaternions from that shear.
    result = {}
    for name in a:
        pa, qa, sa = a[name].decompose(); pb, qb, sb = b[name].decompose()
        result[name] = Matrix.LocRotScale(pa.lerp(pb, fraction), qa.slerp(qb, fraction), sa.lerp(sb, fraction))
    return result

def sample_local(name, t, loop):
    if name in preserved_top_sources or name.startswith('BOTH_P7_') or name == 'BOTH_SABERSTAFF_STANCE':
        return preserved_matrix_sample(name, t, loop)
    c = data['clips'][name]; frame = t*abs(c['fps']); count = c['count']
    frame = frame % count if loop else max(0, min(count-1, frame))
    a = int(frame); b = (a+1) % count if loop else min(a+1, count-1)
    return blend_staff_locals(frame_locals(name, a), frame_locals(name, b), frame-a)

def joined_source(names, durations, t):
    if names[0] in preserved_top_sources:
        return preserved_matrix_join(names, durations, t)
    remaining = t; index = 0
    while index < 2 and remaining > durations[index]:
        remaining -= durations[index]; index += 1
    local = sample_local(names[index], remaining, False)
    if index and remaining < .1:
        local = blend_staff_locals(sample_local(names[index-1], durations[index-1], False), local, remaining/.1)
    return compose_source(local)

staff_axis = -(tag_basis([Vector(v) for v in staff_tags['*blade1']['verts']]) @ Vector((1, 0, 0)))
axis_error = blade_local.angle(staff_axis)
print('STAFF_WEAPON_AXIS', axis_error)
# Use verified staff blade1 axis for its own clips, even if it differs.
blade_local = staff_axis
staff_vertices = [Vector(v.co) for info, surface in zip(glm.surfaceDataCollection.surfaces, glm.LODCollection.LODs[0].surfaces)
                  if not info.flags & 1 for v in surface.vertices]
assert staff_vertices
shaft_min = min(v.dot(staff_axis) for v in staff_vertices)
shaft_max = max(v.dot(staff_axis) for v in staff_vertices)
shaft_radius = max((v-staff_axis*v.dot(staff_axis)).length for v in staff_vertices)
palm_radius = (sr['l_d2_j1'].translation-sr['l_d4_j1'].translation).length*.5
grip_samples = {'held': 0, 'released': 0}

def source_holds_staff(pose):
    right, orientation = source_marker(pose, '*r_hand')
    left, _ = source_marker(pose, '*l_hand')
    point = orientation.inverted() @ (left-right)
    along = point.dot(staff_axis)
    nearest = staff_axis*max(shaft_min, min(shaft_max, along))
    # Contact envelope comes from the owned hilt and source palm dimensions.
    return (point-nearest).length <= shaft_radius+palm_radius

def staff_arm_excursion(short, t, parts):
    if short == 'top':
        return .6
    start, end, length = parts[0], sum(parts[:2]), sum(parts)
    strike = .6 if short == 'brtl' else .75
    def smooth(a, b, u):
        u = max(0., min(1., u)); return a+(b-a)*u*u*(3.-2.*u)
    if short == 'brtl':
        # Preserve the enlarged preparation through its actual release pose;
        # fading before this point had almost removed the added S+A windup.
        return smooth(.8, .6, (t-start)/.1)
    if t < start:
        return smooth(.8, strike, (t-(start-.1))/.1)
    follow = smooth(strike, .85, (t-end)/.1)
    return smooth(follow, .6, (t-(length-.1))/.1)


def rotate_staff_arms(pose, turn):
    pivot = (pose['lhumerus'].translation+pose['rhumerus'].translation)*.5
    change = Matrix.Translation(pivot) @ turn.to_matrix().to_4x4() @ Matrix.Translation(-pivot)
    result = dict(pose)
    for n in result:
        parent = n
        while parent is not None and parent not in ('lhumerus', 'rhumerus'):
            parent = source_parents[parent]
        if parent is not None:
            result[n] = change @ pose[n]
    return result


def staff_smooth(u):
    u = max(0., min(1., u))
    return u*u*u*(u*(u*6.-15.)+10.)


def turn_staff_spine(pose, degrees):
    # The requested winding/finish is turn about up, not restored back bend.
    # Share it across the already-adapted three spine joints, preserving lengths.
    result = dict(pose)
    up = (axis.inverted() @ Vector((0,0,1))).normalized()
    for joint, share in (('lower_lumbar', .4), ('upper_lumbar', .4), ('thoracic', .2)):
        pivot = result[joint].translation
        change = Matrix.Translation(pivot) @ Quaternion(up, math.radians(degrees*share)).to_matrix().to_4x4() @ Matrix.Translation(-pivot)
        previous = dict(result)
        for n in result:
            parent = n
            while parent is not None and parent != joint:
                parent = source_parents[parent]
            if parent is not None:
                result[n] = change @ previous[n]
    return result


# Native camera/controller capture of the previous staff asset7b7b6356, standing.
# Unwrapped shoulder heading during return; the source-only pose proxy omitted
# the live Motion/spine controller and could not identify the early counterturn.
staff_return_heading = {
    'tlbr': (-24.606, -25.010, -18.163, -4.711, 15.415, 37.060, 54.378, 62.046),
    'lr': (-29.257, -20.773, -2.129, 11.010, 27.093, 43.534, 56.458, 62.086),
}


def staff_spine_turn(short, t, parts):
    start, end, length = parts[0], sum(parts[:2]), sum(parts)
    if short in ('rl', 'brtl'):
        rear = -20. if short == 'rl' else -40.
        return rear*staff_smooth(t/(start*.8))*(1.-staff_smooth((t-start)/.1))
    if short not in staff_return_heading or t <= end:
        return 0.
    knots = [end+x for x in (0., .05, .1, .15, .2, .25, .3, .349)]
    values = staff_return_heading[short]
    index = next((i for i in range(len(knots)-1) if t <= knots[i+1]), len(knots)-2)
    u = max(0., min(1., (t-knots[index])/(knots[index+1]-knots[index])))
    old = values[index]*(1.-u)+values[index+1]*u
    extra = 35. if short == 'tlbr' else 25.
    desired = values[0]-extra*staff_smooth((t-end)/.12)
    release = 1.-staff_smooth((t-(end+.12))/(length-(end+.12)))
    return (desired-old)*release


def compact_staff_attack(pose, arm_excursion=.6, arm_roll=0.):
    # Intentional staff-only posture tuning after gameplay review. Reduce each
    # axial joint's absolute lean, not its turn about up. Recompose the source
    # hierarchy before IK so shoulders, hands and weapon tags still agree.
    axial = {'pelvis', 'lower_lumbar', 'upper_lumbar', 'thoracic', 'Motion'}
    # Compact arm excursion as well as the spine. The earlier spine-only
    # correction left the extreme S7/R7 shoulder poses unchanged.
    ready = sample('BOTH_SABERSTAFF_STANCE', 0, False)
    attack_local = {n: pose[p].inverted() @ pose[n] if p else pose[n].copy()
             for n, p in source_parents.items()}
    ready_local = {n: ready[p].inverted() @ ready[n] if p else ready[n].copy()
                   for n, p in source_parents.items()}
    compact = blend_staff_locals(ready_local, attack_local, .6)
    # Keep the trunk compact, but give the arms their own phase-dependent reach.
    # Uniform local reduction shortened both preparation and the finished cut.
    arm_bones = {'lhumerus', 'rhumerus', 'lradius', 'rradius', 'lhand', 'rhand'}
    for n in arm_bones:
        compact[n] = blend_staff_locals({n: ready_local[n]}, {n: attack_local[n]}, arm_excursion)[n]
    # Weapon bolt rotations are authored attachment frames, not body excursion.
    # Dampening them toward stance crossed the shortest-arc branch at overhead
    # windup frame5, creating a3.12rad source jump from a1.09rad source step.
    # Preserve their local transforms while the compacted hand carries them.
    for n in compact:
        if n.endswith('_tag_bone'):
            compact[n] = attack_local[n]
    pose = compose_source(compact)
    local = {n: pose[p].inverted() @ pose[n] if p else pose[n].copy()
             for n, p in source_parents.items()}
    result = {}
    def get(name):
        if name in result:
            return result[name]
        parent = source_parents[name]
        parent_world = get(parent) if parent else Matrix.Identity(4)
        m = local[name]
        if name in axial:
            delta = (pose[name].to_quaternion().normalized() @ sr[name].to_quaternion().normalized().inverted()).normalized()
            turn = Quaternion((delta.w, 0, 0, delta.z))
            turn = turn.normalized() if turn.magnitude > 1e-8 else Quaternion()
            bend = (delta @ turn.inverted()).normalized()
            desired = Quaternion().slerp(bend, .5) @ turn @ sr[name].to_quaternion().normalized()
            position, _, scale = m.decompose()
            m = Matrix.LocRotScale(position, parent_world.to_quaternion().normalized().inverted() @ desired, scale)
        result[name] = parent_world @ m
        return result[name]
    for name in source_parents:
        get(name)
    if arm_roll:
        # Intentional staff-only cut-plane tuning. Rotate both complete arm
        # subtrees together about forward; paired grip follows the right hilt.
        forward = axis.inverted() @ Vector((0,1,0))
        turn = Quaternion(forward.normalized(), math.radians(arm_roll))
        result = rotate_staff_arms(result, turn)
    return result

samples = {}; lengths = {}; clip_report = {}; authored_grip_rows = []
for name, src in aliases.items():
    c = data['clips'][src]
    length = c['count'] / abs(c['fps'])
    lengths[name] = length; samples[name] = {}
    floor = min(min(f['ltalus'].translation.z, f['rtalus'].translation.z) for f in source[src])
    for t in sorted(set([i / 60 for i in range(math.floor(length * 60) + 1)] + [length])):
        p = sample(src, t, True)
        air = max(0, (min(p['ltalus'].translation.z, p['rtalus'].translation.z) - floor) * unit)
        current_context = (name, t)
        local, world = solve(p, name == 'j_guard', air)
        samples[name][t] = local
    clip_report[name] = {'source': src, 'length': length, 'frames': len(samples[name])}
for short, source_names in attack_sources.items():
    parts = [(data['clips'][n]['count'] - 1) / abs(data['clips'][n]['fps']) for n in source_names]
    length = sum(parts); name = 's_' + short
    lengths[name] = length; samples[name] = {}
    times = sorted(set([i / 120 for i in range(math.floor(length * 120) + 1)] + [length, parts[0], sum(parts[:2])]))
    authored = {t: joined_source(source_names, parts, t) for t in times}
    # Original authored JKA attack motion, matching stock-reset-01/weights-02.
    # Rig/proportion/grip conversion and the existing sequence joins remain;
    # no compaction, alignment rolls, added turns or synthetic follow-through.
    poses = authored
    contacts = {t: short == 'top' or source_holds_staff(authored[t]) for t in times}
    held_times = [t for t in times if contacts[t]]
    for t in times:
        current_context = (name, t)
        pose = poses[t]; held = contacts[t]
        right, right_q = source_marker(authored[t], '*r_hand')
        left, _ = source_marker(authored[t], '*l_hand')
        _, compact_q = source_marker(pose, '*r_hand')
        staff_grip_gap = axis @ (compact_q @ (right_q.inverted() @ (left-right))) * unit
        native_gap = Vector((0,0,1)).rotation_difference(blade_local).inverted() @ (right_q.inverted() @ (left-right)) * unit
        authored_grip_rows.append((name, t, int(held), native_gap))
        approach = max(0., 1.-min((abs(t-h) for h in held_times), default=1.)/.1)
        staff_attachment_weight = approach*approach*(3.-2.*approach)
        grip_samples['held' if held else 'released'] += 1
        local, world = solve(pose, held, 0)
        samples[name][t] = local
    overlay = 'y_' + short
    samples[overlay] = samples[name]; lengths[overlay] = length
    clip_report[name] = {'source': source_names, 'length': length, 'hit_window': [parts[0], sum(parts[:2])]}
    clip_report[overlay] = {**clip_report[name], 'controllers': 'upper body; existing Motion spine compensation'}
staff_grip_gap = None
for label, src in staff_blocks.items():
    name = 'j_block_' + label; c = data['clips'][src]
    length = c['count'] / abs(c['fps']); lengths[name] = length; samples[name] = {}
    floor = min(min(f['ltalus'].translation.z, f['rtalus'].translation.z) for f in source[src])
    for t in sorted(set([i / 60 for i in range(math.floor(length * 60) + 1)] + [length])):
        pose = sample(src, t, False)
        air = max(0, (min(pose['ltalus'].translation.z, pose['rtalus'].translation.z) - floor) * unit)
        current_context = (name, t); local, world = solve(pose, True, air)
        samples[name][t] = local
    overlay = 'y_block_' + label
    samples[overlay] = samples[name]; lengths[overlay] = length
    clip_report[name] = {'source': [src], 'length': length}
    clip_report[overlay] = {**clip_report[name], 'controllers': 'upper body; existing block reaction path'}
samples['y_guard'] = samples['j_guard']; lengths['y_guard'] = lengths['j_guard']

accepted = MdlReader(str(engage / 'elbow-arena/profile/override/k1jarig.mdl')).load()
accepted_nodes = {n.name: n for n in nodes(accepted.root_node)}
assert set(accepted_nodes) == set(lookup), 'Adapted rig hierarchy has changed'
bind_difference = max(abs(x-y) for name, n in accepted_nodes.items() for x,y in zip(n.position, lookup[name].position))
assert bind_difference < .000002, bind_difference
assert all((n.parent.name if n.parent else None) == parents[name] for name,n in accepted_nodes.items())
(output / 'attacks-blocks/retarget-measurements.json').write_text(json.dumps({'metrics': metrics, 'clips': clip_report}, indent=2))
assert metrics['bone_length_error'] < .0001, metrics
assert metrics['wrist_target_error'] < .0001, metrics
assert metrics['source_relative_grip_error'] < .0002, metrics
assert metrics['blade_direction_error'] < .0001, metrics

# Reuse the established writer/controller convention against the current mesh.
model = accepted
model.name = 'k1jastaff'
sys.path.insert(0, str(repo / 'platforms/tools'))
from k1staff_weights import apply_staff_torso_weights
staff_weight_bind = {}
for n in nodes(model.root_node):
    local = Matrix.LocRotScale(Vector(n.position), Quaternion(n.orientation), Vector((n.scale,)*3))
    staff_weight_bind[n.name] = staff_weight_bind[n.parent.name] @ local if n.parent else local
staff_weight_report = apply_staff_torso_weights(model, staff_weight_bind)
staff_weight_snapshot = {n.name: [[(b,w) for b,w in row] for row in n.weights]
                         for n in nodes(model.root_node) if n.nodetype == 'SKIN'}
# The original body has an open back panel hidden by the backpack in native
# poses. Staff shoulder motion exposes it. Close that verified loop using its
# existing vertices/UVs/weights; do not change original faces or other bodies.
def close_staff_back_panel(node):
    from collections import Counter, defaultdict
    from mathutils.kdtree import KDTree
    from mathutils.geometry import tessellate_polygon
    points = [rest[node.name] @ Vector(v) for v in node.verts]
    tree = KDTree(len(points))
    for i, v in enumerate(points): tree.insert(v, i)
    tree.balance(); representative = list(range(len(points)))
    def find(i):
        while representative[i] != i:
            representative[i] = representative[representative[i]]; i = representative[i]
        return i
    for i, v in enumerate(points):
        for _, j, _ in tree.find_range(v, .00001): representative[find(i)] = find(j)
    faces = [tuple(find(i) for i in f) for f in node.facelist.vertices]
    edges = Counter(tuple(sorted((f[i], f[(i+1)%3]))) for f in faces for i in range(3))
    adjacency = defaultdict(set)
    for (a, b), count in edges.items():
        if count == 1: adjacency[a].add(b); adjacency[b].add(a)
    seen = set(); candidates = []
    for start in adjacency:
        if start in seen: continue
        pending = [start]; component = []
        while pending:
            v = pending.pop()
            if v in seen: continue
            seen.add(v); component.append(v); pending.extend(adjacency[v]-seen)
        center = sum((points[i] for i in component), Vector()) / len(component)
        if len(component) == 16 and abs(center.x) < .02 and center.y < -.1 and 1.4 < center.z < 1.6:
            assert all(len(adjacency[i]) == 2 for i in component)
            candidates.append(component)
    assert len(candidates) == 1, ('Expected verified native back opening', candidates)
    ring = [candidates[0][0]]
    while len(ring) < 16:
        ring.append(next(i for i in adjacency[ring[-1]] if i not in ring))
    assert ring[0] in adjacency[ring[-1]]
    # The added surface traverses every original boundary edge oppositely.
    if any(any(f[i] == ring[0] and f[(i+1)%3] == ring[1] for i in range(3)) for f in faces): ring.reverse()
    polygon = [points[i] for i in ring]
    cap = [tuple(ring[v] if isinstance(v, int) else ring[min(range(16), key=lambda i: (polygon[i]-v).length_squared)] for v in tri)
           for tri in tessellate_polygon([polygon])]
    assert len(cap) == 14 and all((points[b]-points[a]).cross(points[c]-points[a]).length > 1e-8 for a,b,c in cap)
    after = edges.copy()
    for f in cap:
        for i in range(3): after[tuple(sorted((f[i],f[(i+1)%3])))] += 1
    assert all(after[tuple(sorted((ring[i],ring[(i+1)%16])))] == 2 for i in range(16))
    node.facelist.vertices.extend(cap)
    node.facelist.materials.extend([node.facelist.materials[0]]*len(cap))
    node.facelist.uv.extend(cap)
    return {'mesh': node.name, 'ring': ring, 'faces': [list(f) for f in cap], 'added_faces': len(cap),
            'policy': 'existing native rim positions, UVs and weights; original faces unchanged'}
mesh_cap = close_staff_back_panel(next(n for n in nodes(model.root_node) if n.name == 'Torso'))
for n in nodes(model.root_node):
    if hasattr(n, 'facelist') and n.facelist:
        n.facelist.normals = []
        for a, b, c in n.facelist.vertices:
            normal = (Vector(n.verts[b]) - Vector(n.verts[a])).cross(Vector(n.verts[c]) - Vector(n.verts[a]))
            n.facelist.normals.append(list(normal.normalized() if normal.length > 1e-10 else Vector(n.normals[a])))
definition = recipe[recipe.index('def make_animation(name):'):recipe.index('model.animations=[make_animation(n)')]
exec(compile(definition, '<preserved-animation-writer>', 'exec'), builtins.globals())
model.animations = [a for a in model.animations if a.name not in samples] + [make_animation(n) for n in samples]
# Binary import retains keys but does not mark the animation subtree active.
# The writer otherwise prunes inherited tracks. Restore export metadata only;
# completed stance keys and the original asset are never changed.
def retain_tracks(node):
    children = [retain_tracks(c) for c in node.children]
    node.animated = bool(node.keyframes) or any(children)
    return node.animated
for animation in model.animations:
    retain_tracks(animation.root_node)
asset_dir = output / 'assets'; asset_dir.mkdir(exist_ok=True)
dest = asset_dir / 'k1jastaff.mdl'
MdlWriter(str(dest), model, False, False, False).save()
back = MdlReader(str(dest)).load()
assert len(back.animations) == len(model.animations)
with (profile / 'staff-grip-expected.txt').open('w') as stream:
    for name, t, held, gap in authored_grip_rows:
        stream.write(f'{name} {t:.9f} {held} {gap.x:.9f} {gap.y:.9f} {gap.z:.9f}\n')
with (profile / 'staff-expected.txt').open('w') as stream:
    for name, frames in samples.items():
        controlled = upper if name.startswith('y_') else rig_names
        for t, local in frames.items():
            for bone in sorted(controlled):
                p, q, s = local[bone].decompose()
                stream.write(f'{name} {t:.9f} {bone} {p.x:.9f} {p.y:.9f} {p.z:.9f} {q.x:.9f} {q.y:.9f} {q.z:.9f} {q.w:.9f}\n')
original = MdlReader(str(engage / 'elbow-arena/profile/override/k1jarig.mdl')).load()
preserved_animations = 0
for before in original.animations:
    if before.name in samples:
        continue
    after = next(a for a in back.animations if a.name == before.name)
    assert before.length == after.length, before.name
    aa = {n.name: n.keyframes for n in nodes(before.root_node) if n.keyframes}
    bb = {n.name: n.keyframes for n in nodes(after.root_node) if n.keyframes}
    assert aa.keys() == bb.keys(), (before.name, 'missing inherited bone tracks')
    for bone in aa:
        assert aa[bone].keys() == bb[bone].keys(), (before.name, bone)
        for kind, keys in aa[bone].items():
            other = bb[bone][kind]
            assert len(keys) == len(other), (before.name, bone, kind)
            assert max((abs(x-y) for a,b in zip(keys,other) for x,y in zip(a,b)), default=0) < .000001
    preserved_animations += 1
mesh_checks = 0
for before, after in zip(nodes(original.root_node), nodes(back.root_node)):
    assert before.name == after.name
    for attribute in ('position', 'orientation', 'verts', 'normals', 'uv1', 'uv2', 'weights', 'texture1', 'texture2', 'render'):
        if attribute == 'weights' and before.nodetype == 'SKIN':
            expected = staff_weight_snapshot[before.name]
            assert len(expected) == len(after.weights), before.name
            for target, actual in zip(expected, after.weights):
                a, b = dict(target), dict(actual)
                assert max(abs(a.get(n,0)-b.get(n,0)) for n in a.keys()|b.keys()) < .0000001, before.name
        else:
            assert getattr(before, attribute, None) == getattr(after, attribute, None), (before.name, attribute)
        mesh_checks += 1
    if hasattr(before, 'facelist'):
        count = len(before.facelist.vertices)
        assert before.facelist.vertices == after.facelist.vertices[:count], before.name
        assert len(after.facelist.vertices)-count == (14 if before.name == 'Torso' else 0), before.name
mdx_identical = dest.with_suffix('.mdx').read_bytes() == (engage / 'elbow-arena/profile/override/k1jarig.mdx').read_bytes()
report = {'pass': True, 'scope': 'generated animation components only; runtime and visual checks pending',
          'metrics': metrics, 'clips': clip_report, 'weapon_axis_difference': axis_error,
          'solver_sha256': hashlib.sha256(recipe.encode()).hexdigest(),
          'source_gla_sha256': hashlib.sha256(gla.read_bytes()).hexdigest(),
          'source_staff_glm_sha256': hashlib.sha256(weapon_bytes).hexdigest(),
          'mdl_sha256': hashlib.sha256(dest.read_bytes()).hexdigest(), 'bind_position_error': bind_difference,
          'mesh_semantic_checks': mesh_checks, 'mdx_bytes_identical': mdx_identical}
report['preserved_animations'] = preserved_animations
report['staff_interpolation'] = 'new directions use rigid key rotation; preserve passing overhead/block sampling; unchanged times/100ms joins'
report['source_hand_contact'] = {'samples': grip_samples, 'shaft_bounds': [shaft_min, shaft_max],
                               'shaft_radius': shaft_radius, 'palm_radius': palm_radius,
                               'policy': 'original authored ownership and hilt-local hand spacing carried by compact right hand; independent arm targets during authored release'}
report['hand_reattachment'] = '100ms continuous target approach; authored original JKA free-hand recovery retained'
report['staff_posture'] = {'mode': 'original JKA source; no manual attack shaping',
                           'reference': 'stock-reset-01/weights-02',
                           'scope': 'seven full/upper staff attacks only; original source paths/timing with established rig/grip conversion'}
report['mesh_caps'] = [mesh_cap]
report['staff_weights'] = staff_weight_report
report['staff_pair_refinement'] = 'legacy successes retained; adaptive SVD fallback only on failed poses; existing caps/thresholds'
report['staff_pole_policy'] = 'preserved full-span source policy; guide-conditioned trial rejected'
report['surface_bolt_sides'] = {'longest': 0, 'shortest': 2, 'reference': 'OpenJK260c59c/code/rd-common/mdx_format.h'}
report['generator_sha256'] = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
report['locomotion_policy'] = 'unchanged completed single-blade controllers and existing runtime refinement'
(output / 'generation-report.json').write_text(json.dumps(report, indent=2))
for extension in ('.mdl', '.mdx'):
    shutil.copy2(dest.with_suffix(extension), profile / 'override' / ('k1jastaff' + extension))
print('STAFF_COMPONENT_EXPORT_PASS', json.dumps(report))
