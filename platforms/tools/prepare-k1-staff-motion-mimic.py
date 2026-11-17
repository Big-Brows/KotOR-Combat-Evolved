"""Preview-only staff swing proposals. Never writes the live profile or editors.

Uses the preserved JKA retarget/paired-grip services. Candidate poses are saved
under ignored staff-swing-review, for offline Blender review before deployment.
"""
from pathlib import Path
import builtins
import hashlib
import json
import math
import os
import shutil
import sys

import bpy
from mathutils import Matrix, Quaternion, Vector

repo = Path(__file__).resolve().parents[2]
review_root = repo / 'platforms/out/k1-engage-prototype/staff-swing-review'
attempt = os.environ.get('K1_STAFF_REVIEW_ATTEMPT', 'mimic-02')
assert attempt.replace('-', '').isalnum()
review_dir = review_root / attempt
assert not (review_dir / 'poses.json').exists(), 'Preserve existing review attempts.'
review_dir.mkdir(parents=True, exist_ok=True)
shutil.copy2(Path(__file__), review_dir/'proposal-recipe.py')
live_profile = review_root.parent / 'elbow-arena/profile'
protected = [live_profile / 'override/k1jastaff.mdl', live_profile / 'override/k1jastaff.mdx',
             review_root.parent / 'staff-editor/kotor_staff_blocks.blend',
             review_root.parent / 'staff-editor/attacks/kotor_staff_attacks.blend']
protected_hashes = {str(p.relative_to(repo)): hashlib.sha256(p.read_bytes()).hexdigest() for p in protected}

# Bootstrap existing solver functions into an isolated output root. The script
# stops before the generator's sample/export/deployment section. Only literal
# output destinations are changed; the completed stance and solver stay intact.
generator = repo / 'platforms/tools/prepare-k1-staff-prototype.py'
bootstrap = generator.read_text().split('samples = {}; lengths = {}; clip_report = {}')[0]
assert bootstrap.count("output = engage / 'staff-prototype'") == 1
bootstrap = bootstrap.replace("output = engage / 'staff-prototype'", "output = review_dir / 'retarget'")
bootstrap = bootstrap.replace('output.mkdir(exist_ok=True)', 'output.mkdir(parents=True, exist_ok=True)')
assert bootstrap.count('profile = baseline_profile  #') == 1
bootstrap = bootstrap.replace('profile = baseline_profile  #', "profile = review_dir / 'profile'  #")
(review_dir / 'profile/override').mkdir(parents=True, exist_ok=True)
# Wrist-domain calibration reads this preserved native reference beside its
# solver. Copy only that diagnostic input, never a save/configuration profile.
reference_dir = review_dir / 'retarget/profile'
reference_dir.mkdir(parents=True, exist_ok=True)
shutil.copy2(review_root.parent / 'staff-prototype/profile/yellow-native-motion.txt',
             reference_dir / 'yellow-native-motion.txt')
__file__ = str(generator)
exec(compile(bootstrap, str(generator), 'exec'), builtins.globals())

post_time = 0.
policy = {k: dict(turn=0., arm=0., mode='source-axial-followthrough', application='rigid-whole-body') for k in attack_sources}

# Restore only the whole-body axial turn removed by compaction. Apply one
# rigid transform to the full source hierarchy, rather than changing the relative
# yaw of individual spine joints (which increased torso-edge stretch in mimic01).
def mimic_compact(raw, arm_excursion, arm_roll):
    compact = compact_staff_attack(raw, arm_excursion, arm_roll)
    def axial(pose):
        q = (pose['thoracic'].to_quaternion().normalized() @ sr['thoracic'].to_quaternion().normalized().inverted()).normalized()
        q = Quaternion((q.w, 0, 0, q.z))
        return q.normalized() if q.magnitude > 1e-8 else Quaternion()
    correction = axial(raw) @ axial(compact).inverted()
    correction = Quaternion().slerp(correction, axial_weight).normalized()
    pivot = compact['pelvis'].translation
    transform = Matrix.Translation(pivot) @ correction.to_matrix().to_4x4() @ Matrix.Translation(-pivot)
    return {n: transform @ m for n,m in compact.items()}

def phase_roll(short, t, parts):
    length = sum(parts)
    if short == 'top':
        # Keep the pre-swing and spin free. Only the downward crossing receives
        # the vertical cut correction; fade it out before the return/spin.
        return 35.*staff_smooth((t-.38)/.12)*(1.-staff_smooth((t-.64)/.06))
    amount = {'trbl': -30., 'lr': -10., 'bltr': 15.}.get(short, 0.)
    u = max(0., min(1., t/.1, (length-t)/.1))
    return amount*u*u*(3.-2.*u)

def base_pose(short, names, parts, t):
    raw = joined_source(names, parts, t)
    global axial_weight
    axial_weight = staff_smooth(t/.12)*staff_smooth((sum(parts)-t)/.12)
    pose = mimic_compact(raw, staff_arm_excursion(short, t, parts), phase_roll(short, t, parts))
    start, end = parts[0], sum(parts[:2])
    if short in ('rl', 'brtl'):
        rear = -25. if short == 'rl' else -50.
        pose = turn_staff_spine(pose, rear*staff_smooth(t/(start*.8))*(1.-staff_smooth((t-start)/.1)))
    if short == 'bltr':
        center = -45.*staff_smooth((t-.34)/.055)*(1.-staff_smooth((t-.455)/.085))
        pose = turn_staff_spine(pose, center)
    return raw, pose

result = {}
for short, names in attack_sources.items():
    parts = [(data['clips'][n]['count']-1)/abs(data['clips'][n]['fps']) for n in names]
    old_length = sum(parts)
    end = sum(parts[:2])
    length = old_length+post_time
    times = sorted(set([i/120 for i in range(math.floor(length*120)+1)] + [length, parts[0], end, end+post_time]))
    raw_poses = {}
    poses = {}
    weights = {}
    for t in times:
        raw, pose = base_pose(short, names, parts, t)
        weight = 0.
        raw_poses[t] = raw
        poses[t] = pose
        weights[t] = weight
    contact = {t: short == 'top' or source_holds_staff(raw_poses[t]) for t in times}
    held_times = [t for t in times if contact[t]]
    frames = {}
    grip_error = 0.
    for t in times:
        # Existing source-joint pole guidance and warm starts deliberately use
        # the s_* attack identity. A review_* label bypasses those services.
        current_context = ('s_'+short, t)
        right, right_q = source_marker(raw_poses[t], '*r_hand')
        left, _ = source_marker(raw_poses[t], '*l_hand')
        _, compact_q = source_marker(poses[t], '*r_hand')
        staff_grip_gap = axis@(compact_q@(right_q.inverted()@(left-right)))*unit
        approach = max(0., 1.-min(abs(t-h) for h in held_times)/.1)
        staff_attachment_weight = approach*approach*(3.-2.*approach)
        local, world = solve(poses[t], contact[t], 0)
        if contact[t]:
            grip_error = max(grip_error, (world['lhand'].translation-world['rhand'].translation-staff_grip_gap).length)
        frames[t] = local
    if short in ('trbl', 'tlbr'):
        # Preserve the verified released-offhand recovery correction after its
        # time-shifted final contact, rather than resuming the old elbow branch.
        # W+D also measured a0.536rad step after its final hand release in the
        # draft; use the same released-only correction without changing grip.
        release = max(held_times)
        left_chain = set(arm['L']['names']) | {'lhand'} | {h[0] for h in arm['L']['helpers']}
        for t in times:
            if t <= release:
                continue
            w = staff_smooth((t-release)/(length-release))
            for bone in left_chain:
                p, _, scale = frames[t][bone].decompose()
                q = frames[release][bone].to_quaternion().normalized().slerp(frames[length][bone].to_quaternion().normalized(), w)
                frames[t][bone] = Matrix.LocRotScale(p, q, scale)
    assert grip_error < .0002, (short, grip_error)
    serial = {}
    for t, pose in frames.items():
        serial[f'{t:.9f}'] = {}
        for name, mat in pose.items():
            p, q, s = mat.decompose()
            serial[f'{t:.9f}'][name] = [*p, q.x, q.y, q.z, q.w]
    result['s_'+short] = dict(length=length, original_length=old_length, strike=[parts[0], end],
        post_peak=end+parts[2]*.5, policy=policy[short], poses=serial,
        held=[f'{t:.9f}' for t in held_times], maximum_held_grip_error_m=grip_error)
    print('STAFF_REVIEW_SOLVED', short, len(frames), 'held_grip', grip_error, flush=True)

assert all(hashlib.sha256(p.read_bytes()).hexdigest() == protected_hashes[str(p.relative_to(repo))] for p in protected)
(review_dir/'poses.json').write_text(json.dumps(result), encoding='utf-8')
report = dict(scope='preview only; live assets and editor files unchanged', protected_inputs=protected_hashes,
              extra_post_seconds=post_time, policy=policy, retarget_metrics=metrics,
              source_gla_sha256=hashlib.sha256(gla.read_bytes()).hexdigest(),
              generator_sha256=hashlib.sha256(generator.read_bytes()).hexdigest(),
              candidate_sha256=hashlib.sha256((review_dir/'poses.json').read_bytes()).hexdigest())
(review_dir/'generation.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print('STAFF_REVIEW_POSES_READY', str(review_dir.relative_to(repo)))
