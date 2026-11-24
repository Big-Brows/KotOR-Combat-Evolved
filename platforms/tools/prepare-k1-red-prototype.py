"""Export red/strong stance with the existing yellow retarget and binary services.

Run with the existing Blender installation, --background --factory-startup
--disable-autoexec --python-exit-code 1 --python this-file [-- --npc].
Owned source and generated assets remain in ignored platforms/out/red-stance.
"""
from pathlib import Path
import ast
import builtins
import copy
import hashlib
import json
import math
import re
import bisect
import os
import shutil
import struct
import subprocess
import sys
import zipfile
import bpy
from mathutils import Matrix, Quaternion, Vector

repo = Path(__file__).resolve().parents[2]
workspace = repo / 'platforms/out'
engage = workspace / 'k1-engage-prototype'
npc_mode = '--npc' in sys.argv
output = workspace / 'red-stance' / 'npc' if npc_mode else workspace / 'red-stance'
resource = 'k1jadred' if npc_mode else 'k1jared'
subprocess.run(['git', '-C', str(repo), 'check-ignore', '--quiet', str(output / 'assets')], check=True)
output.mkdir(parents=True, exist_ok=True)
asset_dir = output / 'assets'; asset_dir.mkdir(exist_ok=True)
solver_dir = output / 'solver'; solver_dir.mkdir(exist_ok=True)
profile = engage / 'elbow-arena/profile'
def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()
protected = sorted((profile / 'override').glob('k1ja*.md*')) + sorted((profile / 'override').glob('k1yellow.md*'))
protected = [p for p in protected if p.stem not in ('k1jared', 'k1jadred')]
before = {p.name: digest(p) for p in protected}
assert {'k1jarig.mdl', 'k1jastaff.mdl', 'k1jadual.mdl', 'k1yellow.mdl'} <= before.keys()
archive = Path(os.environ['ProgramFiles(x86)']) / 'Steam/steamapps/common/Jedi Academy/GameData/base/assets1.pk3'
with zipfile.ZipFile(archive) as owned:
    cfg_bytes = owned.read('models/players/_humanoid/animation.cfg')
    gla_bytes = owned.read('models/players/_humanoid/_humanoid.gla')
data = json.loads((engage / 'corrections/jka-grounded-motion.json').read_text())
assert hashlib.sha256(gla_bytes).hexdigest() == data['gla_sha256']
assert hashlib.sha256(cfg_bytes).hexdigest() == data['cfg_sha256']
source_dir = output / 'source'; source_dir.mkdir(exist_ok=True)
gla_path = source_dir / '_humanoid.gla'; gla_path.write_bytes(gla_bytes)
(source_dir / 'animation.cfg').write_bytes(cfg_bytes)
directions = {'top': ('T_', 'T__B_', 'B_'), 'tlbr': ('TL', 'TL_BR', 'BR'),
              'trbl': ('TR', 'TR_BL', 'BL'), 'lr': ('_L', '_L__R', '_R'),
              'rl': ('_R', '_R__L', '_L'), 'bltr': ('BL', 'BL_TR', 'TR'), 'brtl': ('BR', 'BR_TL', 'TL')}
attacks = {short: ['BOTH_S3_S1_' + start, 'BOTH_A3_' + cut, 'BOTH_R3_' + end + '_S1']
           for short, (start, cut, end) in directions.items()}
required = {'BOTH_SABERSLOW_STANCE'} | {n for seq in attacks.values() for n in seq}
# Reuse the preserved SP transition table, including its bottom-quadrant fallback.
transition_source=workspace/'jka-animation-import/research/openjk/code/game/bg_panimate.cpp'
source_text=transition_source.read_text()
transition_text=source_text.split('saberMoveName_t transitionMove[Q_NUM_QUADS][Q_NUM_QUADS]')[1].split('};')[0]
transition_text=re.sub(r'//[^\n]*','',transition_text)
transition_table=re.findall(r'LS_(?:NONE|T1_[A-Z_]+)',transition_text)
assert len(transition_table)==64
quadrants={'BR':0,'_R':1,'TR':2,'T_':3,'TL':4,'_L':5,'BL':6,'B_':7}
chain_sources={}
for outgoing,(_,_,end) in enumerate(directions.values()):
    for incoming,(start,_,_) in enumerate(directions.values()):
        move=transition_table[quadrants[end]*8+quadrants[start]]
        transition=None if move=='LS_NONE' else move.replace('LS_T1_','BOTH_T3_')
        seq=([transition] if transition else [])+list(attacks.values())[incoming][1:]
        chain_sources[outgoing,incoming]=seq
        required.update(seq)
table = {}
for line in cfg_bytes.decode('ascii').splitlines():
    f = line.split()
    if f and f[0] in required:
        table[f[0]] = dict(first=int(f[1]), count=int(f[2]), loop=int(f[3]), fps=int(f[4]))
assert table.keys() == required, required - table.keys()
# The engine contract contains selectors/timings only, never animation resources.
contract=['// JKA SP quadrant-selected strong links; generated from owned clip lengths.',
          'struct RedChainClip {const char* full;const char* upper;const char* transition;float start,end,length;};',
          'constexpr RedChainClip redChains[7][7]={']
for outgoing in range(7):
    contract.append(' {')
    for incoming in range(7):
        seq=chain_sources[outgoing,incoming]
        durations=[(table[n]['count']-1)/abs(table[n]['fps']) for n in seq]
        start=durations[0] if len(seq)==3 else 0.
        end=start+durations[-2]
        transition=seq[0] if len(seq)==3 else 'direct'
        contract.append(f'  {{"s_r{outgoing}{incoming}","y_r{outgoing}{incoming}","{transition}",{start:.9f}f,{end:.9f}f,{sum(durations):.9f}f}},')
    contract.append(' },')
contract.append('};')
(output/'red-chain-contract.inc').write_text('\n'.join(contract)+'\n')
sys.path.insert(0, str(workspace / 'jka-animation-import/tools'))
from jka_importer import JAG2GLA
for name, c in table.items():
    loader = JAG2GLA.GLA()
    ok, message = loader.loadFromFile(str(gla_path), JAG2GLA.AnimationLoadMode.RANGE, c['first'], c['count'])
    assert ok, message
    poses = loader.animation.computeAbsoluteFrameTransforms(loader.skeleton)
    frames = [{loader.skeleton.bones[i].name: [list(row) for row in m] for i, m in p.items()} for p in poses]
    if c['fps'] < 0: frames.reverse()
    assert len(frames) == c['count'] and all(math.isfinite(v) for p in frames for m in p.values() for row in m for v in row)
    data['clips'][name] = {**c, 'frames': frames}
(output / 'red-motion.json').write_text(json.dumps(data))
for path in (engage / 'rig-candidate').iterdir():
    if path.suffix == '.py':
        (solver_dir / path.name).write_text(path.read_text().replace("candidate.parent/'profile", "engage/'profile"))
    elif path.suffix == '.json': shutil.copy2(path, solver_dir / path.name)
settings = json.loads((engage / 'rig-candidate/retarget-validation.json').read_text())['settings']
for key, value in settings.items():
    if value is None: os.environ.pop(key, None)
    else: os.environ[key] = value
recipe_path = engage / 'elbow-arena/generate-candidate-applied.py'
recipe = recipe_path.read_text()
prefix = recipe.split('samples={};worlds={};lengths={};hit_windows={};clip_report={}')[0]
assert len(prefix) < len(recipe)
if npc_mode:
    prefix = prefix.replace("model_file=root/'source/pmbbl.mdl';model=MdlReader(str(model_file)).load()",
        "model_file=engage/'dark-jedi/source/n_darkjedim.mdl';model=MdlReader(str(model_file)).load()\nnext(n for n in model.root_node.children if n.name=='torso').name='Torso'\nnext(n for n in model.root_node.children if n.name=='Larm').name='LArm'")
prefix = prefix.replace('candidate=Path(__file__).resolve().parent', 'candidate=solver_dir')
prefix = prefix.replace("root=candidate.parent/'corrections';workspace=root.parent.parent", "root=engage/'corrections';workspace=root.parent.parent")
prefix = prefix.replace("data=json.loads((root/'jka-grounded-motion.json').read_text())", "data=json.loads((output/'red-motion.json').read_text())")
original_guard_prefix=prefix
# Use the same fixed surface-bolt convention already reviewed for staff/dual.
# Re-selecting longest/shortest edges on a deformed tag can flip the saber 180deg.
for old, new in [('sides[max(range(3),key=lambda i:sides[i].length)]', 'sides[0]'),
                 ('sides[min(range(3),key=lambda i:sides[i].length)]', 'sides[2]')]:
    assert prefix.count(old) == 1, old
    prefix = prefix.replace(old, new)
exec(compile(prefix, '<existing-yellow-retarget-service>', 'exec'), builtins.globals())

# Rigid local interpolation is the existing new-stance sampler: matrix lerps
# shear nearly extended red arms. Reuse it without changing yellow's service.
dual_tree = ast.parse((repo / 'platforms/tools/prepare-k1-dual-prototype.py').read_text())
rigid_sampler = next(n for n in dual_tree.body if isinstance(n, ast.FunctionDef) and n.name == 'sample_local')
exec(compile(ast.Module(body=[rigid_sampler], type_ignores=[]), '<existing-rigid-sampler>', 'exec'), builtins.globals())
def joined_source(names, durations, t):
    left=t; index=0
    while index<2 and left>durations[index]: left-=durations[index]; index+=1
    local=sample_local(names[index],left,False)
    if index and left<.1:
        previous=sample_local(names[index-1],durations[index-1],False); alpha=left/.1
        initial=sample_local(names[index],0,False)
        for bone in local:
            pa,qa,sa=previous[bone].decompose(); pb,qb,sb=local[bone].decompose()
            p0,q0,_=initial[bone].decompose()
            # Fade one initial correction over the advancing clip. Rechoosing
            # the shortest path to a moving endpoint can flip at 180 degrees.
            correction=(qa@q0.inverted()).normalized()
            if correction.w<0:correction.negate()
            orientation=correction.slerp(Quaternion(),alpha)@qb
            local[bone]=Matrix.LocRotScale(pb+(pa-p0)*(1-alpha),orientation,sa.lerp(sb,alpha))
    return compose_source(local)
samples, lengths, clip_report = {}, {}, {}
motion_trace=[]
for name, names in [('j_guard', ['BOTH_SABERSLOW_STANCE'])] + [('s_' + short, seq) for short, seq in attacks.items()]:
    parts = [(data['clips'][n]['count'] - 1) / abs(data['clips'][n]['fps']) for n in names]
    length = sum(parts); lengths[name] = length
    times = sorted({i / 120 for i in range(math.floor(length * 120) + 1)} | {length, parts[0], sum(parts[:2])})
    samples[name] = {}
    for t in times:
        pose = sample(names[0], t, False) if len(names) == 1 else joined_source(names, parts, t)
        current_context = (name, t)
        local, world = solve(pose, True, 0)
        if name=='s_tlbr' and .74<t<.86:
            _,tag_q=source_marker(pose,'*r_hand')
            motion_trace.append({'time':t,'source_tag':list((axis@tag_q).normalized()),
                                 'solved_tag':list(world['rhand'].to_quaternion().normalized()),
                                 'position':list(world['rhand'].translation)})
        samples[name][t] = local
    overlay = 'y_guard' if name == 'j_guard' else 'y_' + name[2:]
    samples[overlay] = samples[name]; lengths[overlay] = length
    clip_report[name] = {'source': names, 'length': length, 'samples': len(times)}
    if len(parts) == 3: clip_report[name]['hit_window'] = [parts[0], sum(parts[:2])]
# Restore only the original held-guard controllers. Its retarget namespace keeps
# the initial dynamic tag convention; attack/transition sampling remains current.
original_scope=dict(builtins.globals())
original_solver=output/'original-guard-solver';original_solver.mkdir(exist_ok=True)
for path in solver_dir.iterdir():
    if path.suffix in ('.py','.json'):shutil.copy2(path,original_solver/path.name)
original_scope['solver_dir']=original_solver
exec(compile(original_guard_prefix,'<original-red-guard-service>','exec'),original_scope)
exec(compile(ast.Module(body=[rigid_sampler],type_ignores=[]),'<original-rigid-sampler>','exec'),original_scope)
original_guard={}
for t in samples['j_guard']:
    original_scope['current_context']=('j_guard',t)
    pose=original_scope['sample']('BOTH_SABERSLOW_STANCE',t,False)
    local,_=original_scope['solve'](pose,True,0)
    original_guard[t]=local
samples['j_guard']=samples['y_guard']=original_guard
clip_report['j_guard']['policy']='original red guard restored; later alignment corrections removed'
# Retarget each source component once, then assemble authored links + strike +
# return with the same fixed initial-correction blend. No repeated wind-up in a chain.
component_cache={}
def component(src):
    global current_context
    if src not in component_cache:
        c=data['clips'][src];length=(c['count']-1)/abs(c['fps'])
        times=sorted({i/120 for i in range(math.floor(length*120)+1)}|{length})
        frames=[]
        for t in times:
            current_context=('s_red_'+src,t)
            local,_=solve(sample(src,t,False),True,0)
            frames.append(local)
        component_cache[src]=(times,frames,length)
    return component_cache[src]
def target_sample(src,t):
    times,frames,length=component(src);t=max(0,min(length,t))
    i=max(0,min(len(times)-2,bisect.bisect_right(times,t)-1))
    alpha=(t-times[i])/(times[i+1]-times[i])
    result={}
    for bone in frames[i]:
        pa,qa,sa=frames[i][bone].decompose();pb,qb,sb=frames[i+1][bone].decompose()
        result[bone]=Matrix.LocRotScale(pa.lerp(pb,alpha),qa.slerp(qb,alpha),sa.lerp(sb,alpha))
    return result
for (outgoing,incoming),seq in chain_sources.items():
    parts=[component(src)[2] for src in seq];length=sum(parts)
    name=f's_r{outgoing}{incoming}';samples[name]={};lengths[name]=length
    boundaries=[sum(parts[:i]) for i in range(1,len(parts))]
    times=sorted({i/120 for i in range(math.floor(length*120)+1)}|{length,*boundaries})
    for t in times:
        left=t;index=0
        while index<len(seq)-1 and left>parts[index]:left-=parts[index];index+=1
        local=target_sample(seq[index],left)
        if index and left<.1:
            previous=target_sample(seq[index-1],parts[index-1]);initial=target_sample(seq[index],0);alpha=left/.1
            for bone in local:
                pa,qa,sa=previous[bone].decompose();pb,qb,sb=local[bone].decompose();p0,q0,_=initial[bone].decompose()
                correction=(qa@q0.inverted()).normalized()
                if correction.w<0:correction.negate()
                local[bone]=Matrix.LocRotScale(pb+(pa-p0)*(1-alpha),correction.slerp(Quaternion(),alpha)@qb,sa.lerp(sb,alpha))
        samples[name][t]=local
    start=parts[0] if len(parts)==3 else 0.
    clip_report[name]={'source':seq,'hit_window':[start,start+parts[-2]],'length':length,'samples':len(times)}
    overlay=f'y_r{outgoing}{incoming}';samples[overlay]=samples[name];lengths[overlay]=length
    print('RED_JKA_PAIR',outgoing,incoming,seq,flush=True)
assert metrics['bone_length_error'] < .0001, metrics
(output/'motion-trace.json').write_text(json.dumps(motion_trace,indent=2))
assert metrics['wrist_target_error'] < .0001, metrics
accepted = profile / ('override/k1jadark.mdl' if npc_mode else 'override/k1jarig.mdl')
model = MdlReader(str(accepted)).load()
assert {n.name for n in nodes(model.root_node)} == set(lookup)
model.name = resource
exec(compile(recipe[recipe.index('def make_animation(name):'):recipe.index('model.animations=[make_animation(n)')], '<existing-animation-writer>', 'exec'), builtins.globals())
original = copy.deepcopy(model)
model.animations = [a for a in model.animations if a.name not in samples] + [make_animation(n) for n in samples]
def retain_tracks(node):
    children = [retain_tracks(c) for c in node.children]
    node.animated = bool(node.keyframes) or any(children)
    return node.animated
for animation in model.animations: retain_tracks(animation.root_node)
for node in nodes(model.root_node):
    if hasattr(node, 'facelist') and node.facelist:
        node.facelist.normals = []
        for a, b, c in node.facelist.vertices:
            normal = (Vector(node.verts[b]) - Vector(node.verts[a])).cross(Vector(node.verts[c]) - Vector(node.verts[a]))
            node.facelist.normals.append(list(normal.normalized() if normal.length > 1e-10 else Vector(node.normals[a])))
dest = asset_dir / (resource + '.mdl')
MdlWriter(str(dest), model, False, False, False).save()
back = MdlReader(str(dest)).load()
maximum = 0.
assert len(back.animations) == len(model.animations)
for a, b in zip(model.animations, back.animations):
    assert a.name == b.name and abs(a.length - b.length) < 1e-6
    bn = {n.name: n for n in nodes(b.root_node)}
    for node in nodes(a.root_node):
        if not node.keyframes: continue
        assert node.keyframes.keys() == bn[node.name].keyframes.keys()
        for kind, keys in node.keyframes.items():
            other = bn[node.name].keyframes[kind]; assert len(keys) == len(other)
            maximum = max(maximum, max((abs(x-y) for row, row2 in zip(keys, other) for x,y in zip(row,row2)), default=0.))
assert maximum < 1e-6
# Independently compare the restored player guard with the preserved first export.
snapshot=workspace/'red-stance/motion/k1jared-before.mdl'
snapshot_error=0.
if not npc_mode and snapshot.exists():
    initial=MdlReader(str(snapshot)).load()
    for name in ('j_guard','y_guard'):
        old=next(a for a in initial.animations if a.name==name)
        new=next(a for a in back.animations if a.name==name)
        assert abs(old.length-new.length)<1e-6
        expected={n.name:n.keyframes for n in nodes(old.root_node) if n.keyframes}
        actual={n.name:n.keyframes for n in nodes(new.root_node) if n.keyframes}
        assert expected.keys()==actual.keys()
        for bone in expected:
            assert expected[bone].keys()==actual[bone].keys()
            for kind,rows in expected[bone].items():
                other=actual[bone][kind];assert len(rows)==len(other)
                snapshot_error=max(snapshot_error,max((abs(x-y) for a,b in zip(rows,other) for x,y in zip(a,b)),default=0.))
    assert snapshot_error<1e-6,snapshot_error
    print('RED_ORIGINAL_GUARD_SNAPSHOT_PASS',snapshot_error,flush=True)
# Unreplaced yellow clips (walking, ready, blocks) and all mesh data stay intact.
for a, b in zip(nodes(original.root_node), nodes(back.root_node)):
    assert a.name == b.name
    for attr in ('position', 'orientation', 'verts', 'normals', 'weights', 'uv1', 'texture1'):
        assert getattr(a, attr, None) == getattr(b, attr, None), (a.name, attr)
extra = {}
expected_guard=asset_dir/('red-guard-npc.txt' if npc_mode else 'red-guard-player.txt')
with expected_guard.open('w') as stream:
    for name in ('j_guard','y_guard'):
        for t,local in samples[name].items():
            for bone in sorted(upper if name.startswith('y_') else rig_names):
                p,q,_=local[bone].decompose()
                stream.write(f'{name} {t:.9f} {bone} {p.x:.9f} {p.y:.9f} {p.z:.9f} {q.x:.9f} {q.y:.9f} {q.z:.9f} {q.w:.9f}\n')
extra[expected_guard.name]=digest(expected_guard)
if npc_mode:
    tree = ast.parse((engage / 'build-arena.py').read_text())
    cls = next(n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == 'Patch')
    scope = {'struct': struct}
    exec(compile(ast.Module(body=[cls], type_ignores=[]), '<existing-gff-patch>', 'exec'), scope)
    patch = scope['Patch']((profile / 'override/k1darktest.utc').read_bytes())
    patch.set('Tag', 'k1arena_darkred'); patch.set('TemplateResRef', 'k1darkred'); patch.set('FirstName', 'Dark Jedi (Red)')
    utc = asset_dir / 'k1darkred.utc'; utc.write_bytes(patch.save()); extra[utc.name] = digest(utc)
assert before == {p.name: digest(p) for p in protected}, 'Completed resource changed'
report = {'component_pass': True, 'runtime_tested': False, 'resource': resource,
          'transition_table_sha256':digest(transition_source),
          'original_guard_snapshot_error':snapshot_error,
          'guard_policy':'original initial red guard restored',
          'source_gla_sha256': data['gla_sha256'], 'source_cfg_sha256': data['cfg_sha256'],
          'retarget_service_sha256': digest(recipe_path), 'generator_sha256': digest(Path(__file__)),
          'clips': clip_report, 'metrics': metrics, 'maximum_key_roundtrip_error': maximum,
          'mdl_sha256': digest(dest), 'mdx_sha256': digest(dest.with_suffix('.mdx')),
          'extra_assets': extra, 'protected_resources': before,
          'policy': 'Yellow services and rate; strong stance/attacks and source timing; inherited single-saber blocks and walks. Native KotOR run applied at runtime.',
          'limitations': ['No JKA balance or special moves', 'Visual acceptance and campaign compatibility not tested']}
(output / 'generation-report.json').write_text(json.dumps(report, indent=2))
print('RED_COMPONENT_EXPORT_PASS', resource, maximum, {k: v.get('hit_window') for k,v in clip_report.items()})
