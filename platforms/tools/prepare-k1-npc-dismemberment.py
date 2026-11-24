"""Blender entry point: native NPC cut pairs, using the established cut services.

Reads owned body models and writes only ignored output. Does not edit completed
stance models or native creature/item templates.
"""
from pathlib import Path
import collections
import copy
import hashlib
import importlib.util
import json
import os
import subprocess
import inspect
import math

from mathutils import Vector
from mathutils.geometry import tessellate_polygon

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'platforms/out/npc-dismemberment'


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


rig = module('npc_rig', Path(__file__).with_name('prepare-k1-native-npc-rigs.py'))
cut = module('native_cuts', Path(__file__).with_name('prepare-k1-dismemberment.py'))
decode_orientation = rig.MdlReader.orientation_controller_to_quaternion


def native_orientation(self, values):
    result=decode_orientation(self,values)
    # K12's native packed-quaternion decoder normalizes XYZ on an overflow.
    # The importer otherwise leaves those rare quantized keys non-unit.
    if len(values)==1:
        square=sum(v*v for v in result[:3])
        if square>1:
            result=[v/math.sqrt(square) for v in result[:3]]+[0.0]
    return result


rig.MdlReader.orientation_controller_to_quaternion=native_orientation
original_interpolate = cut.interpolate
# Partition larger native palettes through the already validated NPC exporter.
# This relaxes only the helper's early limit; every serialized piece stays <=16.
assign_source = inspect.getsource(cut.assign_mesh)
assign_source = assign_source.replace("    assert len({b for ww in n.weights for b, w in ww if w > 0}) <= 16, 'Skin palette exceeds native limit'\n", '')
exec(compile(assign_source, cut.__file__, 'exec'), cut.__dict__)
closure_gaps = []


def interpolate(a, b, t):
    result = original_interpolate(a, b, t)
    for attr in ('tangent', 'bitangent', 'tangentnormal'):
        if attr in a:
            result[attr] = a[attr].lerp(b[attr], t).normalized()
    return result


cut.interpolate = interpolate


def assign_mesh(node, triangles, frame):
    cut.assign_mesh(node, triangles, frame)
    if node.tangentspace and triangles:
        inverse = frame.to_3x3().inverted()
        for attr, target in (('tangent','tangents'),('bitangent','bitangents'),('tangentnormal','tangentspacenormals')):
            setattr(node,target,[tuple((inverse @ v[attr]).normalized()) for t in triangles for v in t])


def influence(tri, names):
    return max(sum(w for bone, w in v['w'] if bone.lower() in names) for v in tri)


def geometry(model):
    frames = rig.worlds(model)
    skins, inputs = {}, {}
    for n in rig.nodes(model.root_node):
        n.from_root = frames[n.name].copy()
        if hasattr(n, 'facelist'):
            n.facelist.normals = [tuple((Vector(n.verts[f[1]])-Vector(n.verts[f[0]])).cross(
                Vector(n.verts[f[2]])-Vector(n.verts[f[0]])).normalized()) for f in n.facelist.vertices]
            n.facelist.materials = [0] * len(n.facelist.vertices)
        if n.nodetype != 'SKIN' or not n.render:
            continue
        frame = frames[n.name]
        skins[n.name] = n
        inputs[n.name] = [[{'p': frame @ Vector(n.verts[i]),
                           'n': (frame.to_3x3() @ Vector(n.normals[i])).normalized(),
                           'uv': Vector(n.uv1[n.facelist.uv[fi][j]]), 'w': n.weights[i]}
                          for j, i in enumerate(face)] for fi, face in enumerate(n.facelist.vertices)]
        if n.tangentspace:
            for face, triangle in zip(n.facelist.vertices, inputs[n.name]):
                for i, vertex in zip(face, triangle):
                    for attr, source in (('tangent','tangents'),('bitangent','bitangents'),('tangentnormal','tangentspacenormals')):
                        vertex[attr]=(frame.to_3x3() @ Vector(getattr(n,source)[i])).normalized()
    return frames, skins, inputs


def groups(model):
    result = collections.defaultdict(set)
    for n in rig.nodes(model.root_node):
        name = n.name.lower()
        ancestors=set();parent=n
        while parent:
            ancestors.add(parent.name.lower());parent=parent.parent
        # Native finger names are RaFngr/LaFngr, not Rfinger/Lfinger.
        # Anatomical ownership follows the full native arm/hand subtree.
        for side in ('r','l'):
            if ancestors & {side+'bicep_g',side+'forearm_g',side+'hand_g'}:
                result[side+'arm'].add(name)
        if name.startswith(('rhand', 'rfinger', 'rthumb', 'rforearm', 'rbicep')):
            result['rarm'].add(name)
        if name.startswith(('lhand', 'lfinger', 'lthumb', 'lforearm', 'lbicep')):
            result['larm'].add(name)
        if name.startswith(('rshin', 'rfoot', 'rtoe', 'rthigh')):
            result['rleg'].add(name)
        if name.startswith(('lshin', 'lfoot', 'ltoe', 'lthigh')):
            result['lleg'].add(name)
        if name.startswith(('head', 'neck', 'jaw', 'eye', 'tongue', 'teeth', 'f_')):
            result['head'].add(name)
    return result


def garment_rings(segments, code, mesh):
    try:
        return cut.loops_from_segments(segments)
    except AssertionError:
        # Native garments and mouths can be open surfaces. Close only an open
        # cut contour's local ends on the cut plane, retaining all source faces.
        graph=collections.defaultdict(set); points={}
        for a,b in segments:
            ka,kb=cut.key(a['p']),cut.key(b['p'])
            if ka==kb:continue
            graph[ka].add(kb);graph[kb].add(ka);points[ka]=a;points[kb]=b
        assert graph and all(len(v) in (1,2) for v in graph.values()), 'Branched native cut contour'
        ends={k for k,v in graph.items() if len(v)==1}
        assert ends and len(ends)%2==0
        segments=list(segments)
        limit=.35 if code=='wa' else .22
        while ends:
            a,b=min(((a,b) for a in ends for b in ends if a<b),key=lambda ab:(points[ab[0]]['p']-points[ab[1]]['p']).length)
            gap=(points[a]['p']-points[b]['p']).length
            assert gap<limit,(mesh,code,'native opening exceeds anatomical cap bound',gap)
            segments.append((points[a],points[b]));ends.remove(a);ends.remove(b)
            closure_gaps.append({'mesh':mesh,'cut':code,'native_opening':gap})
        return cut.loops_from_segments(segments)


def split(inputs, origin, axis, code, domains):
    kept, detached, rings = {}, {}, []
    group = domains.get(code[0] + ('leg' if code.endswith('l') else 'arm'), set())
    if code == 'he':
        group = domains['head']
    for name, triangles in inputs.items():
        kt, dt, segments = [], [], []
        for tri in triangles:
            if code == 'wa' and influence(tri, domains['rarm'] | domains['larm'] | domains['head']) > .5:
                dt.append(tri)
                continue
            eligible = code == 'wa' or influence(tri, group) > .01 or code=='he' and name.lower()=='head'
            if code.endswith('l'):
                opposite=domains[('l' if code[0]=='r' else 'r')+'leg']
                eligible=influence(tri,group)>influence(tri,opposite)
                # A separate inner leg skin must not cut its overlying robe.
                if any(n.lower()=='legs' for n in inputs):
                    eligible=eligible and name.lower()=='legs'
            if not eligible:
                kt.append(tri)
                continue
            a, b = cut.clip(tri, origin, axis, -1), cut.clip(tri, origin, axis, 1)
            kt.extend(cut.triangles(a)); dt.extend(cut.triangles(b))
            rim = [v for v in a if abs((v['p']-origin).dot(axis)) < 1e-6]
            if len(rim) == 2 and len(a) >= 3 and len(b) >= 3:
                segments.append(rim)
        assert abs(cut.area(triangles)-cut.area(kt)-cut.area(dt)) < 1e-5, (code, name, 'lost native surface')
        kept[name], detached[name] = kt, dt
        if segments:rings.extend(garment_rings(segments,code,name))
    assert rings,'No native cut contour'
    return kept, detached, rings


def external_neck(inputs, hook):
    """Cap the existing native body opening; preserve any selected head whole."""
    edges = collections.Counter(); points = {}
    for tt in inputs.values():
        for tri in tt:
            for a, b in zip(tri, tri[1:] + tri[:1]):
                ka, kb = cut.key(a['p']), cut.key(b['p'])
                edges[tuple(sorted((ka, kb)))] += 1
                points[ka] = a; points[kb] = b
    candidates = [(points[a], points[b]) for (a, b), count in edges.items() if count == 1
                  and max((points[k]['p']-hook).length for k in (a, b)) < .20]
    rings = cut.loops_from_segments(candidates)
    assert rings and max((v['p']-hook).length for r in rings for v in r) < .20
    return rings


def caps(rings, origin, axis):
    tangent = axis.cross(Vector((0, 1, 0)))
    if tangent.length < .01:
        tangent = axis.cross(Vector((1, 0, 0)))
    tangent.normalize(); bitangent = axis.cross(tangent).normalized()
    radius = max((v['p']-origin).length for r in rings for v in r)
    result = []
    for ring in rings:
        for tri in tessellate_polygon([[v['p'] for v in ring]]):
            face = []
            for p in tri:
                p = ring[p]['p'] if isinstance(p, int) else p
                v = min(ring, key=lambda x: (x['p']-p).length_squared).copy()
                v['uv'] = Vector((.5+(p-origin).dot(tangent)/(2*radius), .5+(p-origin).dot(bitangent)/(2*radius)))
                v['n'] = axis.copy(); face.append(v)
            if (face[1]['p']-face[0]['p']).cross(face[2]['p']-face[0]['p']).dot(axis) < 0:
                face.reverse()
            result.append(face)
    assert result
    return result


def prepare(body, model):
    frames, skins, inputs = geometry(model)
    lookup = {n.lower(): n for n in frames}
    pos = lambda name: frames[lookup[name.lower()]].translation.copy()
    domains = groups(model)
    specs = []
    for side in ('r', 'l'):
        shoulder, elbow, wrist = pos(side+'bicep_g'), pos(side+'forearm_g'), pos(side+'hand_g')
        axis = (elbow-shoulder).normalized()
        arm_planes=(.45,.55,.65,.75,.85,.95) if body=='c_gammorean' else (.45,.55,.65,.75)
        specs.append((side+'a', [shoulder+(elbow-shoulder)*f for f in arm_planes], axis, side+'bicep_g'))
        axis = (wrist-elbow).normalized()
        specs.append((side+'h', [wrist+axis*f for f in (0, -.01, -.02, -.03, .01)], axis, side+'hand_g'))
        if body != 'n_tusken':  # User policy: Tusken legs cannot be severed.
            knee, ankle = pos(side+'shin_g'), pos(side+'foot_g')
            specs.append((side+'l', [knee+(ankle-knee)*f for f in (0, .05, .1, .15, .25, .35, .5, .65)], (ankle-knee).normalized(), side+'shin_g'))
    specs.append(('wa', [pos(n)+Vector((0,0,f)) for n in ('pelvis_g','torso_g') for f in (0,.01,.02,.03)], Vector((0,0,1)), 'pelvis_g'))
    embedded = any(influence(t, {n for n in domains['head'] if n.startswith('head')}) > .5 for tt in inputs.values() for t in tt)
    neck = pos('necklwr_g' if 'necklwr_g' in lookup else 'headhook')
    head_origins=[neck+Vector((0,0,f)) for f in (0,.005,.01,.02,.03)]
    # Hooded models can blend the lower neck into the shoulder palette. Stay
    # within the native neck/head joint interval rather than dropping weights.
    if 'head_g' in lookup:
        head_origins += [neck+(pos('head_g')-neck)*f for f in (.25,.5,.75)]
    if embedded:
        head_triangles=[t for name,tt in inputs.items() if name.lower()=='head' for t in tt]
        if head_triangles:
            try:head_origins.insert(0,cut.neck_clearance(head_triangles,neck,Vector((0,0,1))))
            except AssertionError:pass
    head_anchor='necklwr_g' if embedded and 'necklwr_g' in lookup else 'headhook'
    specs.append(('he', head_origins, Vector((0,0,1)), head_anchor))
    result = {}
    for code, origins, axis, anchor in specs:
        errors = []
        finger_bones={n for n in domains.get(code[0]+'arm',set()) if 'fngr' in n or 'thumb' in n} if code in ('ra','rh','la','lh') else set()
        def pure_finger(t):return bool(finger_bones) and all(sum(w for b,w in v['w'] if b.lower() in finger_bones)>.999 for v in t)
        def verify_fingers(kept,detached):
            if not finger_bones:return
            assert not any(pure_finger(t) for tt in kept.values() for t in tt),(body,code,'retained fingers')
            assert sum(pure_finger(t) for tt in detached.values() for t in tt)>=sum(pure_finger(t) for tt in inputs.values() for t in tt),(body,code,'missing detached fingers')
        if code == 'he' and not embedded:
            try:
                rings = external_neck(inputs, pos('headhook'))
                origin = sum((v['p'] for r in rings for v in r), Vector()) / sum(map(len,rings))
                kept, detached = inputs, {n: [] for n in inputs}
            except (AssertionError,ValueError):
                # Some native bodies close the neck instead of leaving a rim.
                # Cut that closed neck just below the attachment, and detach
                # the selected head whole through its existing headhook.
                all_domains=dict(domains)
                all_domains['head']={b.lower() for tt in inputs.values() for tri in tt for v in tri for b,w in v['w']}
                for depth in (.005,.01,.02,.03,.04,.06,.08):
                    origin=pos('headhook')-Vector((0,0,depth))
                    try:
                        kept,detached,rings=split(inputs,origin,axis,code,all_domains)
                        break
                    except (AssertionError,ValueError):
                        pass
                else:
                    raise ValueError((body,'closed external neck has no valid contour'))
        else:
            for origin in origins:
                try:
                    kept, detached, rings = split(inputs, origin, axis, code, domains)
                    verify_fingers(kept,detached)
                    break
                except (AssertionError, ValueError) as error:
                    errors.append(str(error))
            else:
                raise ValueError((body, code, errors))
        cap_triangles = caps(rings, origin, axis)
        verify_fingers(kept,detached)
        entry = {'anchor': lookup[anchor.lower()], 'origin': list(frames[lookup[anchor.lower()]].inverted() @ origin),
                 'axis': list((frames[lookup[anchor.lower()]].to_3x3().inverted() @ axis).normalized()),
                 'external_head': code == 'he' and not embedded, 'rings': list(map(len,rings)), 'sides': {}}
        for side, meshes in (('c',kept),('l',detached)):
            candidate = copy.deepcopy(model)
            candidate.name = 'k1c'+hashlib.sha256((body+code+side).encode()).hexdigest()[:12]
            # The reader leaves AnimationNode.animated at its editor default
            # False; the writer omits every such child, even with native keys.
            # Retain the complete original animation-node tree on export.
            for animation in candidate.animations:
                for node in rig.nodes(animation.root_node):node.animated=True
            cn = {n.name:n for n in rig.nodes(candidate.root_node)}
            for name in inputs:
                if meshes[name]:
                    assign_mesh(cn[name],meshes[name],frames[name])
                else:
                    cn[name].render=0
            for n in rig.nodes(candidate.root_node):
                if not getattr(n,'render',False) or n.nodetype=='SKIN':
                    continue
                parent=n
                ancestry=set()
                while parent:
                    ancestry.add(parent.name.lower());parent=parent.parent
                head=bool(ancestry & domains['head'])
                upper=bool(ancestry & (domains['rarm']|domains['larm']|domains['head'])) or frames[n.name].translation.z>origin.z
                removed=(code=='he' and head) or (code=='wa' and upper)
                # The native Gamorrean axe is part of the body, rather than a
                # separately attached item. Keep it with its severed hand/arm.
                if body=='c_gammorean' and n.name.lower()=='waraxe_001':
                    removed=code in ('ra','rh','wa')
                n.render=int(removed == (side=='l'))
            cap=cut.SkinmeshNode('dismember_cap');cap.parent=candidate.root_node
            cap.node_number=max(n.node_number for n in rig.nodes(candidate.root_node))+1
            cap.bitmap='jkcap';cap.shadow=0;cap.from_root=frames[model.root_node.name].copy()
            faces=cap_triangles if side=='c' else [[{**v,'n':-axis} for v in reversed(t)] for t in cap_triangles]
            cut.assign_mesh(cap,faces,cap.from_root);candidate.root_node.children.append(cap)
            cut.add_cap_glow(candidate,cap)
            partitions=rig.partition(candidate)
            for n in rig.nodes(candidate.root_node):
                if n.nodetype=='SKIN':
                    n.skin_matrices={}
            path=OUT/'assets'/(candidate.name+'.mdl')
            cut.MdlWriter(str(path),candidate,False,False,False).save()
            back=rig.MdlReader(str(path)).load()
            assert back.supermodel==model.supermodel and [(a.name,a.length) for a in back.animations]==[(a.name,a.length) for a in model.animations]
            for source_anim,saved_anim in zip(candidate.animations,back.animations):
                source_nodes=rig.nodes(source_anim.root_node);saved_nodes=rig.nodes(saved_anim.root_node)
                assert [n.name for n in source_nodes]==[n.name for n in saved_nodes],(body,code,'animation node tree')
                for a,b in zip(source_nodes,saved_nodes):
                    assert set(a.keyframes)==set(b.keyframes),(body,code,a.name,'animation channels')
                    for channel,keys in a.keyframes.items():
                        actual_keys=b.keyframes[channel]
                        assert len(keys)==len(actual_keys)
                        assert max((abs(x-y) for k,l in zip(keys,actual_keys) for x,y in zip(k,l)),default=0)<1e-6,(body,code,a.name,channel,'animation values')
            actual={n.name:n for n in rig.nodes(back.root_node)}
            if body=='c_gammorean':
                axe=next(n for name,n in actual.items() if name.lower()=='waraxe_001')
                assert bool(axe.render)==((code in ('ra','rh','wa')) == (side=='l')),(body,code,side,'embedded axe ownership')
            for n in rig.nodes(model.root_node):
                b=actual[n.name]
                assert (Vector(n.position)-Vector(b.position)).length<1e-6,(body,code,n.name,'joint translation')
                assert max(abs(a-v) for a,v in zip(n.orientation,b.orientation))<1e-6,(body,code,n.name,'joint rotation')
            error=rig.maximum_skin_bind_error(back)
            assert error < .0001,(body,code,side,'bind',error)
            entry['sides'][side]={'model':candidate.name,'bind_error':error,'partitions':len(partitions),'files':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (path,path.with_suffix('.mdx'))}}
        result[code]=entry
        print('NPC_CUT_PAIR_PASS',body,code,'external_head',entry['external_head'],flush=True)
    return result


def main():
    subprocess.run(['git','-C',str(ROOT),'check-ignore','--quiet',str(OUT/'assets')],check=True)
    (OUT/'assets').mkdir(exist_ok=True)
    roster=json.loads((ROOT/'platforms/out/unnamed-melee-npcs/rig-report.json').read_text())
    bodies=sorted({r['native'] for r in roster['rigs']})
    report={'complete':False,'models':{},'failures':{},'provenance':{}}
    previous=OUT/'geometry-report.json'
    if previous.exists():
        prior=json.loads(previous.read_text())
        report['models']=prior.get('models',{})
        report['provenance']=prior.get('provenance',{})
    for body in bodies:
        if os.environ.get('K1_NPC_CUT_ONLY_BODY') and body!=os.environ['K1_NPC_CUT_ONLY_BODY']:
            continue
        if body in report['models'] and not os.environ.get('K1_NPC_CUT_REBUILD'):
            continue
        try:
            model=rig.MdlReader(str(ROOT/'platforms/out/unnamed-melee-npcs/source'/(body+'.mdl'))).load()
            report['models'][body]=prepare(body,model)
            source=ROOT/'platforms/out/unnamed-melee-npcs/source'/(body+'.mdl')
            report['provenance'][body]={'generator_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                'source':{ext:hashlib.sha256(source.with_suffix(ext).read_bytes()).hexdigest() for ext in ('.mdl','.mdx')}}
        except (AssertionError,ValueError,KeyError,StopIteration) as error:
            report['failures'][body]=str(error);print('NPC_CUT_FAIL',body,str(error),flush=True)
        report['complete']=len(report['models'])==len(bodies) and not report['failures']
        previous.write_text(json.dumps(report,indent=2))
        (OUT/'native-opening-closures.json').write_text(json.dumps(closure_gaps,indent=2))
    print('NPC_CUT_PREPARATION',len(report['models']),len(bodies),'failures',len(report['failures']),flush=True)
    if report['complete']:
        rows=['2DA V2.0','','body source anchor cut retained detached slot px py pz ax ay az probability wholehead']
        files={};index=0
        for body, entries in sorted(report['models'].items()):
            expected={'ra','rh','la','lh','wa','he'} | (set() if body=='n_tusken' else {'rl','ll'})
            assert set(entries)==expected,(body,entries.keys())
            for code,d in entries.items():
                values=[body,'****',d['anchor'],code,d['sides']['c']['model'],d['sides']['l']['model'],'body',
                        *d['origin'],*d['axis'],45,int(d['external_head'])]
                rows.append(str(index)+' '+' '.join(map(str,values)));index+=1
                for side in ('c','l'):
                    for name,sha in d['sides'][side]['files'].items():
                        assert hashlib.sha256((OUT/'assets'/name).read_bytes()).hexdigest()==sha
                        files[name]=sha
        assert index==sum(6 if body=='n_tusken' else 8 for body in report['models'])
        table=OUT/'assets/k1npccuts.2da';table.write_text('\n'.join(rows)+'\n',encoding='ascii')
        files[table.name]=hashlib.sha256(table.read_bytes()).hexdigest()
        component={'component_pass':True,'bodies':len(bodies),'cut_pairs':index,'tusken_legs':False,'files':files}
        (OUT/'runtime-report.json').write_text(json.dumps(component,indent=2))
    else:
        raise RuntimeError('Native NPC cut preparation incomplete; see geometry-report.json')


if __name__=='__main__':
    main()
