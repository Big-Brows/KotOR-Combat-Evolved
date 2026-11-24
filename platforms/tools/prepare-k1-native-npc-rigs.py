"""Blender entry point: transfer existing NPC clips to native humanoid bodies.

Creates separate body/style resources in ignored output. Completed stance
resources are inputs only. Keeps native mesh topology, materials, bind frames,
heads and attachments; adds the existing Engage helper skeleton and partitions
skin palettes without dropping faces. Run prepare-k1-native-npcs.py first.
"""
from pathlib import Path
import copy
import hashlib
import importlib.util
import inspect
import json
import math
import os
import subprocess
import sys
import types

from mathutils import Matrix, Quaternion, Vector

REPO = Path(__file__).resolve().parents[2]
OUT = REPO / 'platforms/out/unnamed-melee-npcs'
PROFILE = REPO / 'platforms/out/k1-engage-prototype/elbow-arena/profile/override'
sys.path.insert(0, str(REPO / 'platforms/out/kotor-animation-import/tools'))
from io_scene_kotor.format.mdl import reader as native_reader
from io_scene_kotor.format.mdl.writer import MdlWriter
from io_scene_kotor.scene.modelnode.dummy import DummyNode
from io_scene_kotor.scene.animnode import AnimationNode
from io_scene_kotor.scene.animation import Animation

# The established importer flags tangent-space data but does not read its MDX
# vectors. Preserve those original vectors for bump-mapped bodies rather than
# disabling their materials or altering the shared importer.
reader_source = inspect.getsource(native_reader)
marker = '                    self.mdx.seek(mdx_offset + i * mdx_data_size + off_mdx_verts)'
assert reader_source.count(marker) == 1
reader_source = reader_source.replace(marker, '''                    if node.tangentspace:
                        assert not self.xbox, "This generator targets PC K1"
                        self.mdx.seek(mdx_offset + i * mdx_data_size + off_mdx_tan_space1)
                        node.bitangents.append(tuple(self.mdx.read_float() for _ in range(3)))
                        node.tangents.append(tuple(self.mdx.read_float() for _ in range(3)))
                        node.tangentspacenormals.append(tuple(self.mdx.read_float() for _ in range(3)))
''' + marker)
# Some original armor bodies contain distinct joints with identical names.
# Preserve the bone-index identity before the importer converts skin indices to
# names, retaining the first native GetPart match as the public semantic joint.
reader_source = reader_source.replace('        self.node_names.append(name)', '''        if not hasattr(self, "native_names_by_offset"):
            self.native_names_by_offset = {}
            self.native_seen_names = set()
        unique = name if name.lower() not in self.native_seen_names else name + "_k1dup" + str(len(self.node_names))
        self.native_seen_names.add(name.lower())
        self.native_names_by_offset[offset] = unique
        self.node_names.append(unique)''')
reader_source = reader_source.replace('        name = self.names[name_index]\n        node_type =',
    '        name = self.native_names_by_offset[offset]\n        node_type =')
# Preserve serialized inverse binds for validation. The normal reader rebuilds
# from_root from the hierarchy and otherwise discards this renderer input.
skin_marker = '            bone_indices = [self.mdl.read_uint16() for _ in range(16)]'
assert reader_source.count(skin_marker) == 1
reader_source = reader_source.replace(skin_marker, '''            saved_position = self.mdl.tell()
            node.skin_mdx_layout = (mdx_offset, mdx_data_size, off_mdx_bone_indices, num_verts)
            self.mdl.seek(MDL_OFFSET + qbone_arr.offset)
            inverse_q = [Quaternion([self.mdl.read_float() for _ in range(4)]) for _ in range(qbone_arr.count)]
            self.mdl.seek(MDL_OFFSET + tbone_arr.offset)
            inverse_t = [Vector([self.mdl.read_float() for _ in range(3)]) for _ in range(tbone_arr.count)]
            assert len(inverse_q) == len(inverse_t)
            node.serialized_inverse_binds = [Matrix.LocRotScale(p, q, Vector((1, 1, 1))) for p, q in zip(inverse_t, inverse_q)]
            self.mdl.seek(saved_position)
''' + skin_marker)
reader_adapter = types.ModuleType('native_npc_reader')
reader_adapter.__package__ = native_reader.__package__
exec(compile(reader_source, native_reader.__file__, 'exec'), reader_adapter.__dict__)
MdlReader = reader_adapter.MdlReader


def nodes(node):
    return [node] + [v for child in node.children for v in nodes(child)]


def local(node):
    return Matrix.LocRotScale(Vector(node.position), Quaternion(node.orientation), Vector((node.scale,) * 3))


def worlds(model):
    result = {}
    for n in nodes(model.root_node):
        result[n.name] = result[n.parent.name] @ local(n) if n.parent else local(n)
    return result


def set_world(node, frame, frames):
    relative = frames[node.parent.name].inverted() @ frame if node.parent else frame
    p, q, _ = relative.decompose()
    node.position, node.orientation, node.scale = list(p), list(q), 1.0


def maximum_skin_bind_error(model):
    """Rest skinning must reproduce the native mesh, not merely stay finite."""
    ordered = nodes(model.root_node)
    index = {n.name: i for i, n in enumerate(ordered)}
    frames = worlds(model)
    maximum = 0.0
    for mesh in ordered:
        if mesh.nodetype != 'SKIN':
            continue
        inverse = mesh.serialized_inverse_binds
        assert len(inverse) == len(ordered), mesh.name
        transforms = {b: frames[b] @ inverse[index[b]] for ww in mesh.weights for b, w in ww if w > 0}
        for vertex, weights in zip(mesh.verts, mesh.weights):
            v = Vector(vertex)
            expected = frames[mesh.name] @ v
            actual = sum((transforms[b] @ v * w for b, w in weights if w > 0), Vector())
            maximum = max(maximum, (actual - expected).length)
    return maximum


def partition(model):
    """Native renderer has sixteen bones per mesh; retain every source face."""
    report = []
    attrs = ('verts', 'normals', 'uv1', 'uv2', 'tangents', 'bitangents',
             'tangentspacenormals', 'weights', 'constraints')
    for mesh in list(nodes(model.root_node)):
        if mesh.nodetype != 'SKIN':
            continue
        palette = {b for ww in mesh.weights for b, w in ww if w > 1e-8}
        if len(palette) <= 16:
            continue
        assert not mesh.children, (mesh.name, 'Cannot partition a mesh with children')
        snapshot = copy.copy(mesh)
        snapshot.facelist = copy.deepcopy(mesh.facelist)
        for attr in attrs:
            setattr(snapshot, attr, copy.deepcopy(getattr(mesh, attr)))
        groups, palettes = [], []
        for i, face in enumerate(snapshot.facelist.vertices):
            bones = {b for v in face for b, w in snapshot.weights[v] if w > 1e-8}
            assert len(bones) <= 16
            group = next((j for j, existing in enumerate(palettes) if len(existing | bones) <= 16), None)
            if group is None:
                group = len(groups)
                groups.append([])
                palettes.append(set())
            groups[group].append(i)
            palettes[group].update(bones)
        for number, faces in enumerate(groups):
            part = mesh if number == 0 else copy.copy(snapshot)
            part.name = mesh.name if number == 0 else f'{snapshot.name}_p{number}'
            part.parent, part.children = mesh.parent, []
            vertices = sorted({v for f in faces for v in snapshot.facelist.vertices[f]})
            remap = {v: i for i, v in enumerate(vertices)}
            for attr in attrs:
                values = getattr(snapshot, attr)
                assert not values or len(values) == len(snapshot.verts), (mesh.name, attr)
                setattr(part, attr, [copy.deepcopy(values[v]) for v in vertices] if values else [])
            part.facelist = copy.copy(snapshot.facelist)
            for attr in ('vertices', 'uv', 'materials', 'normals'):
                values = getattr(snapshot.facelist, attr)
                selected = [copy.deepcopy(values[f]) for f in faces] if values else []
                if attr in ('vertices', 'uv'):
                    selected = [[remap[v] for v in face] for face in selected]
                setattr(part.facelist, attr, selected)
            if number:
                part.parent.children.append(part)
            report.append({'source': snapshot.name, 'part': part.name, 'faces': faces, 'vertices': vertices})
        assert sorted(f for group in groups for f in group) == list(range(len(snapshot.facelist.vertices)))
    return report


def main(config=None):
    subprocess.run(['git', '-C', str(REPO), 'check-ignore', '--quiet', str(OUT / 'assets')], check=True)
    catalog = json.loads((OUT / 'catalog-report.json').read_text()) if not config else None
    source, assets = OUT / 'source', OUT / 'assets'
    source.mkdir(exist_ok=True)
    spec = importlib.util.spec_from_file_location('owned_k1', REPO /
        'platforms/out/k1-engage-prototype/dark-jedi/inspect-owned.py')
    owned = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(owned)
    bodies = set()
    for entry in catalog['coverage'] if catalog else []:
        for key, value in entry['appearance_data'].items():
            if value and (key == 'race' or key.startswith('model') and len(key) == 6):
                bodies.add(value.lower())
    if config:bodies=set(config['bodies'])
    assets.mkdir(exist_ok=True)
    protected = {p: hashlib.sha256(p.read_bytes()).hexdigest() for p in PROFILE.glob('*.md*')}
    donors = {style: MdlReader(str(PROFILE / (name + '.mdl'))).load() for style, name in
              (config['donors'].items() if config else [('single', 'k1jadark'), ('staff', 'k1jadstaff'), ('dual', 'k1jaddual'), ('red', 'k1jadred'), ('blue', 'k1jadblue')])}
    signature=hashlib.sha256((Path(__file__).read_text()+json.dumps(config,sort_keys=True,default=lambda x:sorted(x) if isinstance(x,set) else str(x))).encode()).hexdigest() if config else None
    if config:
        signature=hashlib.sha256((signature+''.join(hashlib.sha256((PROFILE/(name+'.mdl')).read_bytes()).hexdigest()+hashlib.sha256((PROFILE/(name+'.mdx')).read_bytes()).hexdigest() for name in config['donors'].values())).encode()).hexdigest()
    prior=OUT/('rig-progress.json' if config and (OUT/'rig-progress.json').exists() else 'rig-report.json')
    previous = json.loads(prior.read_text()) if prior.exists() else {'rigs':[],'files':{}}
    if config and previous.get('signature')!=signature:previous={'rigs':[],'files':{}}
    cached = {(r['native'], r['style']): r for r in previous['rigs']}
    primary = donors['single']
    donor_lookup = {n.name: n for n in nodes(primary.root_node)}
    # Native joints may be non-rendering TRIMESH nodes rather than DUMMY nodes.
    canonical = {n.name.lower(): n.name for n in nodes(primary.root_node) if n.nodetype != 'SKIN'}
    # These nodes form the shared combat solver's semantic contract.
    required = {'rootdummy', 'pelvis_g', 'torso_g', 'torsoUpr_g', 'lthigh_g', 'lshin_g',
                'lfoot_g', 'rthigh_g', 'rshin_g', 'rfoot_g', 'lbicep_g', 'rbicep_g',
                'Lforearm_g', 'Rforearm_g', 'Lhand_g', 'Rhand_g', 'rhand', 'lhand'}
    results, mappings, files = [], [], {}
    for body in sorted(bodies):
        if os.environ.get('K1_NATIVE_NPC_ONLY_BODY') and body != os.environ['K1_NATIVE_NPC_ONLY_BODY']:
            continue
        for ext, typ in [('.mdl', 2002), ('.mdx', 3008)]:
            (source / (body + ext)).write_bytes(owned.resource(body, typ))
        model = MdlReader(str(source / (body + '.mdl'))).load()
        original = nodes(model.root_node)
        rename = {n.name: canonical.get(n.name.lower(), n.name) for n in original}
        for n in original:
            n.name = rename[n.name]
            if n.nodetype == 'SKIN':
                n.weights = [[[rename.get(b, b), w] for b, w in ww] for ww in n.weights]
        lookup = {n.name: n for n in original}
        # Native Gamorreans name the right attachment weapondummy. Keep that
        # original hook and add the controller's unweighted semantic alias.
        if body=='c_gammorean' and 'rhand' not in lookup:
            native_hook=lookup['weapondummy']
            hook=DummyNode('rhand');hook.parent=native_hook.parent
            hook.parent.children.append(hook)
            hook.position=copy.deepcopy(native_hook.position)
            hook.orientation=copy.deepcopy(native_hook.orientation)
            lookup['rhand']=hook;original.append(hook)
        assert required <= set(lookup), (body, required - set(lookup))
        before_world = worlds(model)
        # Rakata omit a head attachment hook: add an unweighted hook at their
        # actual native head frame, retaining the original weighted head chain.
        if 'headhook' not in lookup:
            hook = DummyNode('headhook')
            hook.parent = lookup['torsoUpr_g']
            hook.parent.children.append(hook)
            set_world(hook, before_world['head_g'], before_world)
            lookup['headhook'] = hook
            before_world = worlds(model)
        torso, pelvis, chest = (lookup[n] for n in ('torso_g', 'pelvis_g', 'torsoUpr_g'))
        torso.parent.children.remove(torso)
        torso.parent = pelvis
        pelvis.children.append(torso)
        set_world(torso, before_world['torso_g'], before_world)
        mid = DummyNode('torsoMid_g')
        mid.parent, mid.children = torso, [chest]
        torso.children.remove(chest)
        torso.children.append(mid)
        chest.parent = mid
        mid_world = before_world['torso_g'].copy()
        mid_world.translation = before_world['torso_g'].translation.lerp(before_world['torsoUpr_g'].translation, .5)
        before_world[mid.name] = mid_world
        set_world(mid, mid_world, before_world)
        set_world(chest, before_world[chest.name], before_world)
        lookup[mid.name] = mid
        for helper, parent, child in [('rhumerusX', 'rbicep_g', 'Rforearm_g'),
                ('rradiusX', 'Rforearm_g', 'Rhand_g'), ('lhumerusX', 'lbicep_g', 'Lforearm_g'),
                ('lradiusX', 'Lforearm_g', 'Lhand_g'), ('rfemurX', 'rthigh_g', 'rshin_g'),
                ('lfemurX', 'lthigh_g', 'lshin_g')]:
            node = DummyNode(helper)
            node.parent = lookup[parent]
            node.parent.children.append(node)
            dw = worlds(primary)
            segment = dw[child].translation - dw[parent].translation
            fraction = (dw[helper].translation - dw[parent].translation).dot(segment) / segment.length_squared
            frame = before_world[parent].copy()
            frame.translation = before_world[parent].translation.lerp(before_world[child].translation, fraction)
            set_world(node, frame, before_world)
            lookup[helper] = node
        motion = copy.copy(donor_lookup['j_motion'])
        motion.parent, motion.children = lookup['rootdummy'], []
        motion.parent.children.append(motion)
        lookup[motion.name] = motion
        # Reuse the established smooth pelvis-to-chest weight field; other
        # native skin influences (including hands, face and armor) stay native.
        lo, hi = before_world['pelvis_g'].translation.z, before_world['headhook'].translation.z
        assert hi - lo > .1, body
        spine = {'pelvis_g', 'torso_g', 'torsoUpr_g'}
        for n in original:
            if n.nodetype != 'SKIN':
                continue
            for i, (vertex, ww) in enumerate(zip(n.verts, n.weights)):
                if ww and all(b in spine for b, w in ww if w > .001):
                    t = max(0, min(1, ((before_world[n.name] @ Vector(vertex)).z - lo) / (hi - lo)))
                    t = t*t*t*(10+t*(-15+6*t))
                    n.weights[i] = [[b, w] for b, w in zip(
                        ('pelvis_g', 'torso_g', 'torsoMid_g', 'torsoUpr_g'),
                        ((1-t)**3, 3*t*(1-t)**2, 3*t*t*(1-t), t**3)) if w > 1e-8]
        partitions = partition(model)
        for i, n in enumerate(nodes(model.root_node)):
            n.node_number = n.export_order = i
            if hasattr(n, 'facelist') and n.facelist:
                n.facelist.normals = [list((Vector(n.verts[b])-Vector(n.verts[a])).cross(
                    Vector(n.verts[c])-Vector(n.verts[a])).normalized()) for a, b, c in n.facelist.vertices]
        after_world = worlds(model)
        # MdlWriter emits inverse skin binds from this cache, not from the
        # hierarchy it serializes. Newly added joints otherwise retain identity
        # from_root matrices: torsoMid_g then lifts its weighted chest vertices
        # by its entire world-space bind height, even in the rest pose.
        for n in nodes(model.root_node):
            n.from_root = after_world[n.name].copy()
        bind_error = max(abs(a-b) for n in original for x, y in zip(before_world[n.name], after_world[n.name]) for a, b in zip(x, y))
        assert bind_error < 2e-5, (body, bind_error)
        target_lookup = {n.name: n for n in nodes(model.root_node)}
        height_scale = (hi - lo) / (worlds(primary)['headhook'].translation.z - worlds(primary)['pelvis_g'].translation.z)
        mapping = {'native': body}
        for style, donor in donors.items():
            donor_nodes = {n.name: n for n in nodes(donor.root_node)}
            resource = (config.get('prefix','k1h') if config else 'k1h') + hashlib.sha256(((config.get('identity','') if config else '')+body + '/' + style).encode()).hexdigest()[:12]
            mapping[style] = resource
            if (body, style) in cached:
                for ext in ('.mdl', '.mdx'):
                    name = resource + ext
                    actual = hashlib.sha256((assets / name).read_bytes()).hexdigest()
                    assert previous['files'][name] == actual, (name, 'Existing rig changed')
                    files[name] = actual
                results.append(cached[(body, style)])
                continue
            model.name, model.supermodel, model.animscale = resource, 'NULL', 1.0
            animations = []
            key_count = 0
            for clip in donor.animations:
                tracks = {n.name: n for n in nodes(clip.root_node)}
                animation = Animation(clip.name)
                animation.length, animation.transtime, animation.animroot = clip.length, clip.transtime, model.root_node.name
                animation.events = copy.deepcopy(clip.events)

                def transfer(n, parent=None):
                    nonlocal key_count
                    track = AnimationNode(n.name)
                    track.node_number, track.parent = n.node_number, parent
                    semantic = n.name.split('_k1dup')[0]
                    semantic = canonical.get(semantic.lower(), semantic)
                    source_node, source_track = donor_nodes.get(semantic), tracks.get(semantic)
                    if source_node and source_track:
                        track.keyframes = copy.deepcopy(source_track.keyframes)
                        if 'position' in track.keyframes:
                            # Native limb offsets remain body-specific. Only the
                            # donor's animated displacement is scaled to size.
                            for key in track.keyframes['position']:
                                key[1:4] = [v * height_scale for v in key[1:4]]
                        if 'orientation' in track.keyframes:
                            calibration = Quaternion(n.orientation) @ Quaternion(source_node.orientation).inverted()
                            previous = None
                            for key in track.keyframes['orientation']:
                                q = (calibration @ Quaternion((key[4], *key[1:4]))).normalized()
                                if previous and previous.dot(q) < 0:
                                    q.negate()
                                previous = q.copy()
                                key[1:5] = [q.x, q.y, q.z, q.w]
                        key_count += sum(len(keys) for keys in track.keyframes.values())
                    track.children = [transfer(child, track) for child in n.children]
                    track.animated = bool(track.keyframes) or any(c.animated for c in track.children)
                    return track

                animation.root_node = transfer(model.root_node)
                animations.append(animation)
            model.animations = animations
            path = assets / (resource + '.mdl')
            MdlWriter(str(path), model, False, False, False).save()
            back = MdlReader(str(path)).load()
            skin_bind_error = maximum_skin_bind_error(back)
            assert skin_bind_error < .00002, (body, style, 'exported skin bind error', skin_bind_error)
            assert len(back.animations) == len(donor.animations)
            assert {a.name for a in back.animations} == {a.name for a in donor.animations}
            maximum = 0.0
            maximum_where = None
            for a, b in zip(model.animations, back.animations):
                assert a.name == b.name and abs(a.length - b.length) < 1e-6
                bn = {n.name: n for n in nodes(b.root_node)}
                for n in nodes(a.root_node):
                    for kind, keys in n.keyframes.items():
                        other = bn[n.name].keyframes[kind]
                        assert len(keys) == len(other)
                        for row, row2 in zip(keys, other):
                            error = max(abs(x-y) for x, y in zip(row, row2))
                            if error > maximum:
                                maximum, maximum_where = error, (a.name, n.name, kind, row, row2)
            assert maximum < 1e-5, (body, style, maximum, maximum_where)
            assert all(len({b for ww in n.weights for b, w in ww if w > 0}) <= 16 for n in nodes(back.root_node) if n.nodetype == 'SKIN')
            for ext in ('.mdl', '.mdx'):
                name = resource + ext
                files[name] = hashlib.sha256((assets / name).read_bytes()).hexdigest()
            results.append({'native': body, 'style': style, 'resource': resource, 'clips': len(animations),
                            'keys': key_count, 'roundtrip_error': maximum, 'bind_error': bind_error,
                            'height_scale': height_scale, 'partitions': partitions})
            results[-1]['skin_bind_error'] = skin_bind_error
            print('NATIVE_NPC_RIG_PASS', body, style, resource, key_count, flush=True)
        mappings.append(mapping)
        if config:
            (OUT/'rig-progress.json').write_text(json.dumps({'component_pass':False,'signature':signature,'rigs':results,'files':files},indent=2))
    if os.environ.get('K1_NATIVE_NPC_ONLY_BODY'):
        (OUT / ('rig-diagnostic-' + os.environ['K1_NATIVE_NPC_ONLY_BODY'] + '.json')).write_text(json.dumps(results, indent=2))
        return
    if config:mappings += config.get('extra_mappings',[])
    columns=['single','staff','dual','red','blue']+(['purple'] if config else [])
    table = ['2DA V2.0', '', 'native '+' '.join(columns)]
    table += [str(i)+' '+m['native']+' '+' '.join(m[s] for s in columns) for i,m in enumerate(mappings)]
    table_name=(config.get('table','k1npcrigs') if config else 'k1npcrigs')+'.2da'
    (assets / table_name).write_text('\n'.join(table) + '\n')
    files[table_name] = hashlib.sha256((assets / table_name).read_bytes()).hexdigest()
    assert all(hashlib.sha256(p.read_bytes()).hexdigest() == h for p, h in protected.items())
    report = {'component_pass': True, 'files': files, 'rigs': results,'mappings':mappings,'signature':signature,
              'protected_assets': len(protected), 'limits': 'Local bind-relative clip transfer; installed runtime and visual fidelity not yet tested. Native arm/hand/face weights retained.'}
    (OUT / 'rig-report.json').write_text(json.dumps(report, indent=2))
    print('NATIVE_NPC_RIGS_PASS', len(results), 'protected', len(protected))


if __name__ == '__main__':
    main()
