"""Generate isolated JKA dual-saber animation components; never deploy to a profile.

Run from the project root with the existing Blender installation:
blender --background --factory-startup --disable-autoexec --python-exit-code 1
        --python platforms/tools/prepare-k1-dual-prototype.py
Requires the existing local retarget tooling and owned JKA data. All generated
game-derived content and reports belong in ignored platforms/out/dual-stance.
"""
from pathlib import Path
import builtins
import ast
import copy
import hashlib
import json
import math
import os
import shutil
import subprocess
import sys
import zipfile

import bpy
from mathutils import Matrix, Quaternion, Vector

repo = Path(__file__).resolve().parents[2]
workspace = repo / 'platforms/out'
engage = workspace / 'k1-engage-prototype'
npc_mode = '--npc' in sys.argv
output = workspace / 'dual-stance' / 'npc' if npc_mode else workspace / 'dual-stance'
resource = 'k1jaddual' if npc_mode else 'k1jadual'
subprocess.run(['git', '-C', str(repo), 'check-ignore', '--quiet', str(output / 'assets')], check=True)
output.mkdir(parents=True, exist_ok=True)
solver_dir = output / 'solver'
solver_dir.mkdir(exist_ok=True)
asset_dir = output / 'assets'
asset_dir.mkdir(exist_ok=True)

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

# Completed resources are inputs only. Preserve both models and animation packs.
profile = engage / 'elbow-arena/profile'
protected = sorted((profile / 'override').glob('k1ja*.md*'))
protected += sorted((profile / 'override').glob('k1yellow.md*'))
protected = [p for p in protected if p.stem not in ('k1jadual', 'k1jaddual')]
before_hashes = {p.name: digest(p) for p in protected}
assert {'k1jarig.mdl', 'k1jarig.mdx', 'k1jastaff.mdl', 'k1jastaff.mdx',
        'k1yellow.mdl', 'k1yellow.mdx'} <= before_hashes.keys()

archive = Path(os.environ['ProgramFiles(x86)']) / 'Steam/steamapps/common/Jedi Academy/GameData/base/assets1.pk3'
cfg_member = 'models/players/_humanoid/animation.cfg'
gla_member = 'models/players/_humanoid/_humanoid.gla'
source_dir = output / 'source'
source_dir.mkdir(exist_ok=True)
with zipfile.ZipFile(archive) as owned:
    cfg_bytes = owned.read(cfg_member)
    gla_bytes = owned.read(gla_member)
(source_dir / 'animation.cfg').write_bytes(cfg_bytes)
gla_path = source_dir / '_humanoid.gla'
gla_path.write_bytes(gla_bytes)
data = json.loads((engage / 'corrections/jka-grounded-motion.json').read_text())
assert hashlib.sha256(gla_bytes).hexdigest() == data['gla_sha256'], 'Owned GLA differs from retarget calibration'
assert hashlib.sha256(cfg_bytes).hexdigest() == data['cfg_sha256'], 'Owned cfg differs from retarget calibration'

directions = {
    'top': ('T_', 'T__B_', 'B_'), 'tlbr': ('TL', 'TL_BR', 'BR'),
    'trbl': ('TR', 'TR_BL', 'BL'), 'lr': ('_L', '_L__R', '_R'),
    'rl': ('_R', '_R__L', '_L'), 'bltr': ('BL', 'BL_TR', 'TR'),
    'brtl': ('BR', 'BR_TL', 'TL'),
}
attacks = {short: ['BOTH_S6_S6_' + start, 'BOTH_A6_' + cut, 'BOTH_R6_' + end + '_S6']
           for short, (start, cut, end) in directions.items()}
blocks = {'j_block_' + label: 'BOTH_P6_S6_' + suffix for label, suffix in
          {'up': 'T_', 'ur': 'TR', 'ul': 'TL', 'lr': 'BL', 'll': 'BR'}.items()}
locomotion = {'j_guard': 'BOTH_SABERDUAL_STANCE', 'j_walk': 'BOTH_WALK_DUAL',
              'j_backwalk': 'BOTH_WALKBACK_DUAL', 'j_run': 'BOTH_RUN_DUAL',
              'j_backrun': 'BOTH_RUNBACK_DUAL'}
required = set(locomotion.values()) | set(blocks.values()) | {n for seq in attacks.values() for n in seq}
table = {}
for line in cfg_bytes.decode('ascii').splitlines():
    fields = line.split()
    if fields and fields[0] in required:
        assert fields[0] not in table, fields[0]
        table[fields[0]] = dict(first=int(fields[1]), count=int(fields[2]), loop=int(fields[3]), fps=int(fields[4]))
assert table.keys() == required, required - table.keys()
sys.path.insert(0, str(workspace / 'jka-animation-import/tools'))
from jka_importer import JAG2GLA
for name, clip in table.items():
    assert clip['count'] > 1 and clip['fps'] != 0, name
    loader = JAG2GLA.GLA()
    ok, message = loader.loadFromFile(str(gla_path), JAG2GLA.AnimationLoadMode.RANGE, clip['first'], clip['count'])
    assert ok, message
    poses = loader.animation.computeAbsoluteFrameTransforms(loader.skeleton)
    frames = [{loader.skeleton.bones[i].name: [list(row) for row in m] for i, m in pose.items()} for pose in poses]
    if clip['fps'] < 0:
        frames.reverse()
    assert len(frames) == clip['count']
    assert all(math.isfinite(v) for f in frames for m in f.values() for row in m for v in row)
    data['clips'][name] = {**clip, 'frames': frames}
(output / 'dual-motion.json').write_text(json.dumps(data))

# Reuse the completed retarget service without executing its generation/deploy
# section. Its settings are calibration, not approval of its historical reports.
for path in (engage / 'rig-candidate').iterdir():
    if path.suffix in ('.py', '.json'):
        if path.suffix == '.py':
            code = path.read_text().replace("candidate.parent/'profile", "engage/'profile")
            (solver_dir / path.name).write_text(code)
        else:
            shutil.copy2(path, solver_dir / path.name)
settings = json.loads((engage / 'rig-candidate/retarget-validation.json').read_text())['settings']
for key, value in settings.items():
    if value is None:
        os.environ.pop(key, None)
    else:
        os.environ[key] = value
# Dual hands remain independent, including ready and parry poses. Do not run the
# single-hilt pair solver or its staff-only grip/weight adaptations.
os.environ.pop('K1_GUARD_POSTURE', None)
recipe_path = engage / 'elbow-arena/generate-candidate-applied.py'
recipe = recipe_path.read_text()
prefix = recipe.split('samples={};worlds={};lengths={};hit_windows={};clip_report={}')[0]
if npc_mode:
    prefix = prefix.replace("model_file=root/'source/pmbbl.mdl';model=MdlReader(str(model_file)).load()",
        "model_file=engage/'dark-jedi/source/n_darkjedim.mdl';model=MdlReader(str(model_file)).load()\nnext(n for n in model.root_node.children if n.name=='torso').name='Torso'\nnext(n for n in model.root_node.children if n.name=='Larm').name='LArm'")
assert len(prefix) < len(recipe), 'Retarget service boundary changed'
prefix = prefix.replace('candidate=Path(__file__).resolve().parent', 'candidate=solver_dir')
prefix = prefix.replace("root=candidate.parent/'corrections';workspace=root.parent.parent", "root=engage/'corrections';workspace=root.parent.parent")
prefix = prefix.replace("data=json.loads((root/'jka-grounded-motion.json').read_text())", "data=json.loads((output/'dual-motion.json').read_text())")
# Same fixed surface-bolt convention already used by the staff recipe.
for old, new in [('sides[max(range(3),key=lambda i:sides[i].length)]', 'sides[0]'),
                 ('sides[min(range(3),key=lambda i:sides[i].length)]', 'sides[2]')]:
    assert prefix.count(old) == 1, old
    prefix = prefix.replace(old, new)
exec(compile(prefix, '<existing-dual-retarget-service>', 'exec'), builtins.globals())

# Interpolate local rigid rotations, avoiding the historical matrix shearing.
def sample_local(name, t, loop):
    clip = data['clips'][name]
    f = t * abs(clip['fps'])
    f = f % clip['count'] if loop else max(0, min(clip['count'] - 1, f))
    a = int(f)
    b = (a + 1) % clip['count'] if loop else min(a + 1, clip['count'] - 1)
    u = f - a
    aa, bb = frame_locals(name, a), frame_locals(name, b)
    result = {}
    for bone in aa:
        pa, qa, sa = aa[bone].decompose()
        pb, qb, sb = bb[bone].decompose()
        result[bone] = Matrix.LocRotScale(pa.lerp(pb, u), qa.slerp(qb, u), sa.lerp(sb, u))
    return result

samples, lengths, clip_report = {}, {}, {}
axis_errors = {'R': 0., 'L': 0.}
minimum_hand_gap = float('inf')

def store(name, times, poses, report):
    global current_context, minimum_hand_gap
    samples[name] = {}
    poses = list(poses)
    assert len(poses) == len(times)
    floor = min(min(p['ltalus'].translation.z, p['rtalus'].translation.z) for p in poses)
    for t, pose in zip(times, poses):
        current_context = (name, t)
        air = max(0., (min(pose['ltalus'].translation.z, pose['rtalus'].translation.z) - floor) * unit) if report.get('loop') else 0.
        local, world = solve(pose, False, air)
        minimum_hand_gap = min(minimum_hand_gap, (world['lhand'].translation - world['rhand'].translation).length)
        for side in ('R', 'L'):
            _, orientation = source_marker(pose, '*' + side.lower() + '_hand')
            expected = (axis @ orientation @ blade_local).normalized()
            actual = (world[side.lower() + 'hand'].to_quaternion() @ Vector((0, 0, 1))).normalized()
            axis_errors[side] = max(axis_errors[side], (actual - expected).length)
        samples[name][t] = local
    clip_report[name] = {**report, 'samples': len(times)}

for name, src in {**locomotion, **blocks}.items():
    clip = data['clips'][src]
    loop = name in locomotion and name != 'j_guard'
    length = (clip['count'] if loop else clip['count'] - 1) / abs(clip['fps'])
    lengths[name] = length
    times = sorted({i / 60 for i in range(math.floor(length * 60) + 1)} | {length})
    store(name, times, (sample(src, t, loop) for t in times), {'source': [src], 'length': length, 'loop': loop})
    if name.startswith('j_block_') or name == 'j_guard':
        overlay = 'y_' + name[2:]
        samples[overlay], lengths[overlay] = samples[name], length

def dual_attack_pose(short, names, parts, t):
    pose = joined_source(names, parts, t)
    if short == 'top':
        # The main blade otherwise passes beside a centered opponent at normal
        # contact distance while the offhand hits. Bring the entire right arm
        # inward, keeping the actual grip/blade coherent and the shoulder fixed.
        weight = max(0., min(1., t / parts[0], (sum(parts) - t) / parts[2]))
        turn = Quaternion(axis.inverted() @ Vector((0, 0, 1)), math.radians(18.) * weight)
        center = pose['rhumerus'].translation.copy()
        transform = Matrix.Translation(center) @ turn.to_matrix().to_4x4() @ Matrix.Translation(-center)
        result = dict(pose)
        for bone in pose:
            parent = bone
            while parent and parent != 'rhumerus':
                parent = source_parents[parent]
            if parent:
                result[bone] = transform @ pose[bone]
        return result
    if short != 'trbl':
        return pose
    # W+A: the original main blade falls almost twice as far as it travels
    # laterally. Roll the whole upper chain, including both actual grips,
    # toward a diagonal plane. Keep the authored legs and clip boundaries.
    weight = min(1., t / parts[0], (sum(parts) - t) / parts[2])
    turn = Quaternion(axis.inverted() @ Vector((0, 1, 0)), math.radians(16.) * max(0., weight))
    center = pose['lower_lumbar'].translation.copy()
    transform = Matrix.Translation(center) @ turn.to_matrix().to_4x4() @ Matrix.Translation(-center)
    result = dict(pose)
    for bone in pose:
        parent = bone
        while parent and parent != 'lower_lumbar':
            parent = source_parents[parent]
        if parent:
            result[bone] = transform @ pose[bone]
    return result

for short, names in attacks.items():
    parts = [(data['clips'][n]['count'] - 1) / abs(data['clips'][n]['fps']) for n in names]
    length = sum(parts)
    name = 's_' + short
    lengths[name] = length
    times = sorted({i / 120 for i in range(math.floor(length * 120) + 1)} | {length, parts[0], sum(parts[:2])})
    store(name, times, (dual_attack_pose(short, names, parts, t) for t in times),
          {'source': names, 'length': length, 'hit_window': [parts[0], sum(parts[:2])]})
    samples['y_' + short], lengths['y_' + short] = samples[name], length

assert metrics['bone_length_error'] < .0001, metrics
assert metrics['wrist_target_error'] < .0001, metrics
assert max(axis_errors.values()) < .0001, axis_errors

# Start from the current single-blade body without modifying any source mesh,
# weights or existing locomotion clips. Only dual-owned clips are replaced.
accepted_path = profile / ('override/k1jadark.mdl' if npc_mode else 'override/k1jarig.mdl')
model = MdlReader(str(accepted_path)).load()
assert {n.name for n in nodes(model.root_node)} == set(lookup), 'Rig hierarchy changed'
model.name = resource
definition = recipe[recipe.index('def make_animation(name):'):recipe.index('model.animations=[make_animation(n)')]
exec(compile(definition, '<existing-native-animation-writer>', 'exec'), builtins.globals())
original = MdlReader(str(accepted_path)).load()
model.animations = [a for a in model.animations if a.name not in samples] + [make_animation(n) for n in samples]

# The completed staff formula inherits the established full locomotion clips,
# not a dual upper/lower hybrid. Preserve every controller and the original clock.
normal_names = {'j_walk','j_backwalk','j_run','j_backrun',
                'j_leftwalk','j_rightwalk','j_leftrun','j_rightrun'}
normal_walks = {a.name: a for a in original.animations if a.name in normal_names}
assert normal_walks.keys() == normal_names
model.animations = [a for a in model.animations if a.name not in normal_names] + [copy.deepcopy(a) for a in normal_walks.values()]
for name, animation in normal_walks.items():
    samples.pop(name, None)
    clip_report[name] = {'source': [f'completed {accepted_path.stem} full locomotion'],
                        'length': animation.length, 'full_body_preserved': True}
def retain_tracks(node):
    children = [retain_tracks(c) for c in node.children]
    node.animated = bool(node.keyframes) or any(children)
    return node.animated

back_panel = {'added_faces': 0, 'policy': 'Dark Jedi native robe retained'}
if not npc_mode:
    # Reuse the reviewed staff closure without executing or changing its
    # generator/assets. The player shares that same native concealed opening.
    service = repo / 'platforms/tools/prepare-k1-staff-prototype.py'
    tree = ast.parse(service.read_text())
    function = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == 'close_staff_back_panel')
    back_bind = {}
    for node in nodes(model.root_node):
        local = Matrix.LocRotScale(Vector(node.position), Quaternion(node.orientation), Vector((node.scale,) * 3))
        back_bind[node.name] = back_bind[node.parent.name] @ local if node.parent else local
    scope = {'rest': back_bind, 'Vector': Vector}
    exec(compile(ast.Module(body=[function], type_ignores=[]), '<existing-back-panel-service>', 'exec'), scope)
    back_panel = scope['close_staff_back_panel'](next(n for n in nodes(model.root_node) if n.name == 'Torso'))
    back_panel['service_sha256'] = digest(service)

for animation in model.animations:
    retain_tracks(animation.root_node)
# The binary reader omits per-face normals used by the existing writer. Restore
# that export metadata as in the staff recipe, without changing vertices/faces.
for node in nodes(model.root_node):
    if hasattr(node, 'facelist') and node.facelist:
        node.facelist.normals = []
        for a, b, c in node.facelist.vertices:
            normal = (Vector(node.verts[b]) - Vector(node.verts[a])).cross(Vector(node.verts[c]) - Vector(node.verts[a]))
            node.facelist.normals.append(list(normal.normalized() if normal.length > 1e-10 else Vector(node.normals[a])))
dest = asset_dir / (resource + '.mdl')
MdlWriter(str(dest), model, False, False, False).save()
back = MdlReader(str(dest)).load()
assert len(back.animations) == len(model.animations)
maximum = 0.
for expected, actual in zip(model.animations, back.animations):
    assert expected.name == actual.name and abs(expected.length - actual.length) < .000001
    a = {n.name: n.keyframes for n in nodes(expected.root_node) if n.keyframes}
    b = {n.name: n.keyframes for n in nodes(actual.root_node) if n.keyframes}
    assert a.keys() == b.keys(), expected.name
    for bone in a:
        assert a[bone].keys() == b[bone].keys()
        for kind, keys in a[bone].items():
            assert len(keys) == len(b[bone][kind])
            maximum = max(maximum, max((abs(x-y) for row, other in zip(keys, b[bone][kind]) for x, y in zip(row, other)), default=0.))
assert maximum < .000001, maximum
walk_checks = 0
for name, normal in normal_walks.items():
    actual = next(a for a in back.animations if a.name == name)
    assert abs(normal.length - actual.length) < .000001
    actual_nodes = {n.name: n for n in nodes(actual.root_node)}
    for expected in nodes(normal.root_node):
        actual_keys = actual_nodes[expected.name].keyframes
        assert expected.keyframes.keys() == actual_keys.keys()
        for kind, keys in expected.keyframes.items():
            assert len(keys) == len(actual_keys[kind])
            assert max((abs(x-y) for a,b in zip(keys,actual_keys[kind]) for x,y in zip(a,b)), default=0.) < .000001
        walk_checks += 1
for expected, actual in zip(nodes(original.root_node), nodes(back.root_node)):
    assert expected.name == actual.name
    for attribute in ('position', 'orientation', 'verts', 'normals', 'uv1', 'uv2', 'weights', 'texture1', 'texture2', 'render',
                      'bitangents', 'tangents', 'tangentspacenormals'):
        assert getattr(expected, attribute, None) == getattr(actual, attribute, None), (expected.name, attribute)
    if hasattr(expected, 'facelist'):
        added = back_panel.get('faces', []) if expected.name == back_panel.get('mesh') else []
        assert expected.facelist.vertices == actual.facelist.vertices[:len(expected.facelist.vertices)], expected.name
        assert list(map(tuple, added)) == list(map(tuple, actual.facelist.vertices[len(expected.facelist.vertices):])), expected.name
# The existing writer builds each skin palette from a Python set. Its order can
# change across processes, changing MDX indices plus the corresponding MDL map.
# Verify the decoded bone/weight identity above, and permit byte differences only
# inside those index slots. Never copy an MDX with a mismatched palette header.
import io_scene_kotor.format.mdl.reader as reader_module
reader_code = Path(reader_module.__file__).read_text()
marker = '            off_mdx_bone_indices = self.mdl.read_uint32()'
assert reader_code.count(marker) == 1
reader_code = reader_code.replace(marker, marker + '\n            node._dual_index_layout = (mdx_offset, mdx_data_size, off_mdx_bone_indices)')
diagnostic_namespace = dict(vars(reader_module))
exec(compile(reader_code, '<read-only-mdx-layout-diagnostic>', 'exec'), diagnostic_namespace)
DiagnosticReader = diagnostic_namespace['MdlReader']
base_layout = DiagnosticReader(str(accepted_path)).load()
dual_layout = DiagnosticReader(str(dest)).load()
allowed_offsets = set()
for a, b in zip(nodes(base_layout.root_node), nodes(dual_layout.root_node)):
    if hasattr(a, '_dual_index_layout'):
        assert a._dual_index_layout == b._dual_index_layout, a.name
        offset, stride, indices = a._dual_index_layout
        for i in range(len(a.verts)):
            allowed_offsets.update(range(offset + i * stride + indices, offset + i * stride + indices + 16))
base_mdx = accepted_path.with_suffix('.mdx').read_bytes()
dual_mdx = dest.with_suffix('.mdx').read_bytes()
assert len(base_mdx) == len(dual_mdx)
changed_offsets = [i for i, (a, b) in enumerate(zip(base_mdx, dual_mdx)) if a != b]
assert all(i in allowed_offsets for i in changed_offsets), 'MDX changed outside remapped bone indices'
assert before_hashes == {p.name: digest(p) for p in protected}, 'Completed stance resource changed'
extra_assets = {}
if npc_mode:
    # Reuse the fixture's GFF patch service; preserve native stats/powers/scripts.
    import ast, struct
    tree = ast.parse((engage / 'build-arena.py').read_text())
    patch_class = next(n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == 'Patch')
    scope = {'struct': struct}
    exec(compile(ast.Module(body=[patch_class], type_ignores=[]), '<existing-gff-patch>', 'exec'), scope)
    patch = scope['Patch']((profile / 'override/k1darktest.utc').read_bytes())
    patch.set('Tag', 'k1arena_darkdual'); patch.set('TemplateResRef', 'k1darkdual')
    patch.set('FirstName', 'Dark Jedi (Dual Sabers)')
    creature = asset_dir / 'k1darkdual.utc'; creature.write_bytes(patch.save())
    extra_assets[creature.name] = digest(creature)
report = {'component_pass': True, 'runtime_tested': False, 'visual_accepted': False,
          'resource': resource, 'extra_assets': extra_assets,
          'back_panel': back_panel,
          'scope': 'isolated animation export only; no profile deployment or dual gameplay selection',
          'source_gla_sha256': hashlib.sha256(gla_bytes).hexdigest(),
          'source_cfg_sha256': hashlib.sha256(cfg_bytes).hexdigest(),
          'retarget_service_sha256': digest(recipe_path), 'generator_sha256': digest(Path(__file__)),
          'source_table': table, 'clips': clip_report, 'metrics': metrics,
          'blade_axis_error': axis_errors, 'minimum_hand_gap_m': minimum_hand_gap,
          'maximum_key_roundtrip_error': maximum, 'mdl_sha256': digest(dest),
          'retarget_helpers': {p.name: digest(p) for p in sorted(solver_dir.glob('*.py'))},
          'mdx_sha256': digest(dest.with_suffix('.mdx')), 'protected_resources': before_hashes,
          'mdx_preservation': {'semantic_identity': True, 'identical_bytes': not changed_offsets,
                               'changed_bytes_in_remapped_bone_index_slots': len(changed_offsets)},
          'normal_locomotion': {'pass': True, 'clips': list(normal_walks), 'checked_nodes': walk_checks,
                                     'oracle': f'unchanged completed {accepted_path.stem} full walk/run controllers'},
          'settings': {key: os.environ.get(key) for key in settings},
          'limitations': ['Component generation does not run the separate installed-data probes',
                         'No visual acceptance from this generator',
                         'No dual-specific balance or special moves',
                         'Existing single-blade mesh weights retained; dual deformation unverified',
                         'Established 100ms sequence joins retained; not a JKA engine parity claim']}
(output / 'generation-report.json').write_text(json.dumps(report, indent=2))
with (output / 'dual-expected.txt').open('w') as stream:
    for name, frames in samples.items():
        if name in normal_walks:
            # These mixed clips use the completed native walk as their lower
            # oracle, rather than the replaced source-only sampling below.
            continue
        for t, local in frames.items():
            for bone in sorted(upper if name.startswith('y_') else rig_names):
                p, q, _ = local[bone].decompose()
                stream.write(f'{name} {t:.9f} {bone} {p.x:.9f} {p.y:.9f} {p.z:.9f} {q.x:.9f} {q.y:.9f} {q.z:.9f} {q.w:.9f}\n')
print('DUAL_COMPONENT_EXPORT_PASS', len(samples), 'clips; blade axis errors', axis_errors)

