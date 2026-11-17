"""Three requested phase edits on the preserved stock-JKA preview, never deploy."""
from pathlib import Path
import copy
import hashlib
import json
import math
import os
import shutil

from mathutils import Matrix, Quaternion, Vector

repo = Path(__file__).resolve().parents[2]
root = repo / 'platforms/out/k1-engage-prototype/staff-swing-review'
baseline = root / 'stock-reset-01'
dest = root / os.environ.get('K1_STAFF_REVIEW_ATTEMPT', 'twist-01')
assert not dest.exists(), 'Preserve prior attempts.'
dest.mkdir()
shutil.copy2(__file__, dest/'proposal-recipe.py')

# Reuse the native pose reader and exact forward-walk/MotionSpine display math.
# This prefix only reads assets; it neither opens nor writes a Blender scene.
reader = repo/'platforms/tools/render-k1-staff-swing-review.py'
os.environ['K1_STAFF_REVIEW_ATTEMPT'] = 'stock-reset-01'
code = reader.read_text().split('source_blend =')[0]
code = code.replace("assert not (review/'staff_swing_review.blend').exists(), 'Preserve existing review files.'", 'pass # read-only source')
ns = {'__file__': str(reader)}
exec(compile(code, str(reader), 'exec'), ns)
worlds, displayed = ns['worlds'], ns['displayed']
parents, bind = ns['parents'], ns['bind']
rest_world = worlds(bind)
rest_right = rest_world['rbicep_g'].translation-rest_world['lbicep_g'].translation
pelvis_right = rest_world['pelvis_g'].to_quaternion().inverted() @ rest_right


def decode(pose):
    return {n: Matrix.LocRotScale(Vector(v[:3]), Quaternion((v[6], *v[3:6])).normalized(), Vector((1,1,1))) for n,v in pose.items()}


def encode(pose):
    result = {}
    for n,m in pose.items():
        p,q,_ = m.decompose()
        result[n] = [*p,q.x,q.y,q.z,q.w]
    return result


def angle(local):
    w = worlds(local)
    right = w['rbicep_g'].translation-w['lbicep_g'].translation
    reference = w['pelvis_g'].to_quaternion() @ pelvis_right
    return math.atan2(reference.x*right.y-reference.y*right.x, reference.x*right.x+reference.y*right.y)


def smooth(u):
    u = max(0., min(1., u))
    return u*u*u*(u*(u*6.-15.)+10.)


def change(full, t, walking, correction, shares):
    before = displayed(full,t,walking)
    w = worlds(before)
    for joint, share in shares:
        pivot = w[joint].translation.copy()
        rotation = Quaternion(Vector((0,0,1)), correction*share)
        transform = Matrix.Translation(pivot) @ rotation.to_matrix().to_4x4() @ Matrix.Translation(-pivot)
        for n in w:
            ancestor = n
            while ancestor is not None and ancestor != joint:
                ancestor = parents[ancestor]
            if ancestor is not None:
                w[n] = transform @ w[n]
    after = {n: w[parents[n]].inverted()@m if parents[n] else m for n,m in w.items()}
    result = dict(full)
    for joint,_ in shares:
        # Undo only the existing display compensation, so the saved source
        # variant produces the requested yaw after that compensation is applied.
        compensation = full[joint].to_quaternion().normalized().inverted() @ before[joint].to_quaternion().normalized()
        p,_,s = full[joint].decompose()
        q = after[joint].to_quaternion().normalized() @ compensation.inverted()
        result[joint] = Matrix.LocRotScale(p,q.normalized(),s)
    return result


proposals = json.loads((baseline/'poses.json').read_text())
generation = json.loads((baseline/'generation.json').read_text())
report = {}
for clip,data in proposals.items():
    data['policy'] = dict(turn=0.,arm=0.,mode='phase-twist-reduction')
    if clip not in ('s_trbl','s_tlbr','s_brtl'):
        continue
    source = copy.deepcopy(data['poses'])
    variants = (True,) if clip == 's_tlbr' else (False,True)
    for walking in variants:
        target = copy.deepcopy(source)
        measurements = []
        for key,raw in source.items():
            t = float(key)
            if clip == 's_tlbr':
                weight = smooth(t/.06)*smooth((data['strike'][0]-t)/.06)
            else:
                weight = smooth((t-data['strike'][1])/.06)*smooth((data['length']-t)/.08)
            if weight <= 0:
                continue
            full = decode(raw)
            original = displayed(full,t,walking)
            yaw = angle(original)
            # Whole torso for W+A/W+D; chest joints only for S+A.
            shares = [('torsoMid_g',.6),('torsoUpr_g',.4)] if clip == 's_brtl' else [('torso_g',.4),('torsoMid_g',.4),('torsoUpr_g',.2)]
            changed = change(full,t,walking,-.2*weight*yaw,shares)
            target[key] = copy.deepcopy(raw)
            encoded = encode(changed)
            for joint,_ in shares:
                target[key][joint] = encoded[joint]
            actual = displayed(changed,t,walking)
            expected = yaw*(1.-.2*weight)
            error = abs(math.atan2(math.sin(angle(actual)-expected),math.cos(angle(actual)-expected)))
            assert error < 1e-4, (clip,t,error)
            # Rigidly carry the arms/hilt; never rewrite their local transforms.
            assert all(list(changed[n]) == list(full[n]) for n in full if n not in dict(shares))
            a,b = worlds(original),worlds(actual)
            ga = a['rhand'].inverted() @ a['lhand'].translation
            gb = b['rhand'].inverted() @ b['lhand'].translation
            grip_error = (ga-gb).length
            assert grip_error < 2e-6, (clip,t,grip_error)
            assert all((changed[n].translation-full[n].translation).length<1e-7 for n in full)
            measurements.append(dict(time=t,weight=weight,before_degrees=math.degrees(yaw),after_degrees=math.degrees(angle(actual)),yaw_error_rad=error,grip_invariance_error_m=grip_error))
        field = 'walk_poses' if walking else 'poses'
        data[field] = target
        report[clip+('/walk' if walking else '/stand')] = measurements
        print('PHASE_TWIST_REDUCTION',clip,walking,len(measurements),flush=True)

for p,h in generation['protected_inputs'].items():
    assert hashlib.sha256((repo/p).read_bytes()).hexdigest()==h,p
(dest/'poses.json').write_text(json.dumps(proposals))
generation.update(scope='three requested phase reductions; preview only',baseline='stock-reset-01',comparison_baseline='stock-reset-01',policy='20% less relative axial twist with smooth phase envelopes; W+D walking only',candidate_sha256=hashlib.sha256((dest/'poses.json').read_bytes()).hexdigest())
(dest/'generation.json').write_text(json.dumps(generation,indent=2))
(dest/'twist-checks.json').write_text(json.dumps(report,indent=2))
print('TARGETED_TWIST_PREVIEW_READY',dest.name)
