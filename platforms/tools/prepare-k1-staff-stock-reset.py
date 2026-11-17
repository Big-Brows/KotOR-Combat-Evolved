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
attempt = os.environ.get('K1_STAFF_REVIEW_ATTEMPT', 'stock-reset-01')
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

# Intentionally restart from the authored stock paths. No posture compaction,
# cut-plane rolls, synthetic continuation or released-arm pose editing.
post_time=0.
policy={k:dict(turn=0.,arm=0.,mode='unedited-stock-source') for k in attack_sources}
result={}
for short,names in attack_sources.items():
    parts=[(data['clips'][n]['count']-1)/abs(data['clips'][n]['fps']) for n in names]
    old_length=length=sum(parts);end=sum(parts[:2])
    times=sorted(set([i/120 for i in range(math.floor(length*120)+1)]+[length,parts[0],end]))
    raw_poses={t:joined_source(names,parts,t) for t in times}
    poses=raw_poses
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
              source_policy='stock authored paths; existing key interpolation/100ms joins and rig/grip conversion only; no swing tuning',
              source_gla_sha256=hashlib.sha256(gla.read_bytes()).hexdigest(),
              generator_sha256=hashlib.sha256(generator.read_bytes()).hexdigest(),
              candidate_sha256=hashlib.sha256((review_dir/'poses.json').read_bytes()).hexdigest())
(review_dir/'generation.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print('STAFF_REVIEW_POSES_READY', str(review_dir.relative_to(repo)))
