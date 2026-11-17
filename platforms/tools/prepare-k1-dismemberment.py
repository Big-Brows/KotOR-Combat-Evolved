"""Prepare ignored native KotOR cut-body candidates with Blender's bundled Python.

Run with Blender --background --factory-startup --python <this file>.
Requires the existing local KotOR reader/writer; never modifies installed assets.
Candidates are not installed by this tool. JKA eligibility remains a runtime concern.
"""
from pathlib import Path
import collections
import copy
import hashlib
import json
import math
import subprocess
import sys

from mathutils import Matrix, Quaternion, Vector
from mathutils.geometry import tessellate_polygon

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'platforms/out/k1-engage-prototype/dismemberment'
subprocess.run(['git', '-C', str(ROOT), 'check-ignore', '--quiet', str(OUT / 'candidate')], check=True)
sys.path.insert(0, str(ROOT / 'platforms/out/kotor-animation-import/tools'))
from io_scene_kotor.format.mdl.reader import MdlReader
from io_scene_kotor.format.mdl.writer import MdlWriter
from io_scene_kotor.scene.modelnode.skinmesh import SkinmeshNode
from io_scene_kotor.scene.modelnode.trimesh import FaceList

SOURCES = {
    'p': ROOT / 'platforms/out/k1-engage-prototype/corrections/source/pmbbl.mdl',
    's': ROOT / 'platforms/out/kotor-animation-import/source/n_sithsoldier.mdl',
    'd': ROOT / 'platforms/out/k1-engage-prototype/dark-jedi/source/n_darkjedim.mdl',
}
EPS = 1e-7


def nodes(n):
    return [n] + [v for c in n.children for v in nodes(c)]


def key(p):
    return tuple(round(v * 100000) for v in p)


def weights(values):
    values = sorted(((b, w) for b, w in values.items() if w > 1e-7), key=lambda x: (-x[1], x[0]))
    if len(values) > 4:
        raise ValueError(f'Cut intersection exceeds native four-weight limit: {values}')
    total = sum(w for _, w in values)
    assert total > 0
    return [(b, w / total) for b, w in values]


def interpolate(a, b, t):
    ww = collections.defaultdict(float)
    for bone, w in a['w']:
        ww[bone] += w * (1 - t)
    for bone, w in b['w']:
        ww[bone] += w * t
    return {'p': a['p'].lerp(b['p'], t), 'uv': a['uv'].lerp(b['uv'], t),
            'n': a['n'].lerp(b['n'], t).normalized(), 'w': weights(ww)}


def clip(triangle, origin, axis, sign):
    out = []
    for a, b in zip(triangle, triangle[1:] + triangle[:1]):
        da, db = (a['p'] - origin).dot(axis) * sign, (b['p'] - origin).dot(axis) * sign
        if da >= -EPS:
            out.append(a)
        if (da > EPS and db < -EPS) or (da < -EPS and db > EPS):
            out.append(interpolate(a, b, da / (da - db)))
    return out


def triangles(poly):
    return [[poly[0], poly[i], poly[i + 1]] for i in range(1, len(poly) - 1)
            if (poly[i]['p'] - poly[0]['p']).cross(poly[i + 1]['p'] - poly[0]['p']).length > 1e-10]


def area(tt):
    return sum((b['p'] - a['p']).cross(c['p'] - a['p']).length * .5 for a, b, c in tt)


def loops_from_segments(segments):
    points, graph = {}, collections.defaultdict(set)
    for a, b in segments:
        ka, kb = key(a['p']), key(b['p'])
        if ka == kb:
            continue
        # A geometric seam shared by two native meshes must have identical
        # endpoint weights; otherwise a single cap would open during playback.
        for k, v in ((ka, a), (kb, b)):
            if k in points:
                aw, bw = dict(points[k]['w']), dict(v['w'])
                assert max((abs(aw.get(n, 0) - bw.get(n, 0)) for n in aw.keys() | bw.keys()), default=0) < 1e-4, 'Cut rim has conflicting skin weights'
            points[k] = v
        graph[ka].add(kb)
        graph[kb].add(ka)
    assert graph and all(len(v) == 2 for v in graph.values()), f'Cut rim is open/branched: {collections.Counter(map(len, graph.values()))}; endpoints={[(list(points[k]["p"]), points[k]["w"]) for k in graph if len(graph[k]) != 2]}'
    remaining, loops = set(graph), []
    while remaining:
        start = min(remaining)
        current, previous, ring = start, None, []
        while True:
            ring.append(points[current])
            remaining.remove(current)
            following = min(graph[current] - ({previous} if previous is not None else set()))
            previous, current = current, following
            if current == start:
                break
            assert current in remaining, 'Cut rim self-intersection'
        assert len(ring) >= 3
        loops.append(ring)
    return loops


def assign_mesh(n, tt, frame):
    inverse = frame.inverted()
    n.verts, n.normals, n.uv1, n.weights = [], [], [], []
    n.uv2, n.tangents, n.bitangents, n.tangentspacenormals = [], [], [], []
    n.facelist = FaceList()
    for tri in tt:
        ids = []
        for v in tri:
            ids.append(len(n.verts))
            n.verts.append(tuple(inverse @ v['p']))
            n.normals.append(tuple((inverse.to_3x3() @ v['n']).normalized()))
            n.uv1.append(tuple(v['uv']))
            n.weights.append(v['w'])
        n.facelist.vertices.append(ids)
        n.facelist.uv.append(ids)
        n.facelist.materials.append(0)
        normal = (Vector(n.verts[ids[1]]) - Vector(n.verts[ids[0]])).cross(Vector(n.verts[ids[2]]) - Vector(n.verts[ids[0]])).normalized()
        n.facelist.normals.append(tuple(normal))
    assert len({b for ww in n.weights for b, w in ww if w > 0}) <= 16, 'Skin palette exceeds native limit'
    n.render = int(bool(tt))


def add_cap_glow(model, cap):
    # Original JKA caps_glow texture, through K12's native additive material.
    # Small normal offset avoids coplanar depth fighting; no world GL override.
    glow = copy.deepcopy(cap)
    glow.name = 'dismember_glow'
    glow.parent = model.root_node
    glow.node_number = cap.node_number + 1
    glow.bitmap = 'jkcapgl'
    glow.selfillumcolor = (1.0, 1.0, 1.0)
    glow.verts = [tuple(Vector(v) + Vector(n) * .0001) for v, n in zip(glow.verts, glow.normals)]
    model.root_node.children.append(glow)


def neck_clearance(tt, origin, axis):
    """Place a cut distal to the native sloped neck attachment opening.

    Trace only boundary components actually intersected by the initial plane;
    reject facial/accessory holes rather than bridging arbitrary endpoints.
    """
    counts, values, points = collections.Counter(), {}, {}
    for tri in tt:
        for i in range(3):
            a, b = tri[i], tri[(i + 1) % 3]
            ka, kb = key(a['p']), key(b['p'])
            edge = tuple(sorted((ka, kb)));counts[edge] += 1
            values[edge] = (ka, kb);points[ka] = a;points[kb] = b
    graph, touched = collections.defaultdict(set), set()
    for edge, count in counts.items():
        if count != 1:continue
        a, b = values[edge];graph[a].add(b);graph[b].add(a)
        da, db = (points[a]['p'] - origin).dot(axis), (points[b]['p'] - origin).dot(axis)
        if min(da, db) < 0 < max(da, db):touched.update((a, b))
    if not touched:return origin
    component, todo = set(), list(touched)
    while todo:
        k = todo.pop()
        if k in component:continue
        component.add(k);todo.extend(graph[k] - component)
    assert all(len(graph[k]) == 2 for k in component), 'Native neck attachment itself is not a closed boundary'
    assert all(b.lower() in ('necklwr_g', 'neck_g', 'torsoupr_g')
               for k in component for b, w in points[k]['w'] if w > 1e-6), 'Initial neck cut crosses a non-neck opening'
    depth = max((points[k]['p'] - origin).dot(axis) for k in component) + .001
    assert 0 < depth < .05, 'Neck clearance exceeds anatomical attachment band'
    return origin + axis * depth


def prepare_external_head(out):
    # Head is its own native animation/attachment model. Replace that model's
    # lower neck with a capped stub; never cut the backpack/body as a proxy.
    source = OUT / 'source/pmhc01.mdl'
    model = MdlReader(str(source)).load()
    world, inputs = {}, {}
    for n in nodes(model.root_node):
        local = Matrix.LocRotScale(Vector(n.position), Quaternion(n.orientation), Vector((n.scale,) * 3))
        world[n.name] = world[n.parent.name] @ local if n.parent else local
        n.from_root = world[n.name].copy()
        if hasattr(n, 'facelist'):
            n.facelist.normals = [tuple((Vector(n.verts[f[1]]) - Vector(n.verts[f[0]])).cross(
                Vector(n.verts[f[2]]) - Vector(n.verts[f[0]])).normalized()) for f in n.facelist.vertices]
            n.facelist.materials = [0] * len(n.facelist.vertices)
        if n.nodetype == 'SKIN' and n.render:
            inputs[n.name] = [[{'p': world[n.name] @ Vector(n.verts[i]),
                'n': (world[n.name].to_3x3() @ Vector(n.normals[i])).normalized(),
                'uv': Vector(n.uv1[n.facelist.uv[fi][j]]), 'w': n.weights[i]}
                for j, i in enumerate(face)] for fi, face in enumerate(n.facelist.vertices)]
    origin, axis = Vector((0, 0, 0)), Vector((0, 0, 1))
    origin = neck_clearance([tri for tt in inputs.values() for tri in tt], origin, axis)
    pieces, segments = {'c': {}, 'l': {}}, []
    for name, tt in inputs.items():
        for suffix in pieces:
            pieces[suffix][name] = []
        for tri in tt:
            a, b = clip(tri, origin, axis, -1), clip(tri, origin, axis, 1)
            pieces['c'][name].extend(triangles(a));pieces['l'][name].extend(triangles(b))
            rim = [v for v in a if abs((v['p'] - origin).dot(axis)) < 1e-6]
            if len(rim) == 2 and len(a) >= 3 and len(b) >= 3:
                segments.append(rim)
        assert abs(area(tt) - area(pieces['c'][name]) - area(pieces['l'][name])) < 1e-5
    rings = loops_from_segments(segments)
    assert len(rings) == 1, 'External head neck must have one closed cut ring'
    ring = rings[0];radius = max(v['p'].xy.length for v in ring)
    caps = []
    for tri in tessellate_polygon([[v['p'] for v in ring]]):
        face = []
        for p in tri:
            p = ring[p]['p'] if isinstance(p, int) else p
            v = min(ring, key=lambda x: (x['p'] - p).length_squared).copy()
            v['uv'] = Vector((.5 + p.x / (2 * radius), .5 + p.y / (2 * radius)));v['n'] = axis
            face.append(v)
        if (face[1]['p'] - face[0]['p']).cross(face[2]['p'] - face[0]['p']).z < 0:face.reverse()
        caps.append(face)
    result = {'external_head': True, 'source': 'pmhc01', 'attachment': 'headhook',
              'origin': list(origin), 'axis': list(axis), 'rings': [len(ring)], 'sides': {}}
    for suffix, meshes in pieces.items():
        candidate = copy.deepcopy(model);candidate.name = 'k1dphe' + suffix
        for n in nodes(candidate.root_node):
            if n.name in meshes:assign_mesh(n, meshes[n.name], world[n.name])
            elif hasattr(n, 'render') and n.render:n.render = int(suffix == 'l')
        cap = SkinmeshNode('dismember_cap');cap.parent = candidate.root_node
        cap.node_number = max(n.node_number for n in nodes(candidate.root_node)) + 1
        cap.bitmap = 'jkcap';cap.shadow = 0;cap.from_root = world[candidate.root_node.name].copy()
        assign_mesh(cap, caps if suffix == 'c' else [[{**v, 'n': -axis} for v in reversed(t)] for t in caps], cap.from_root)
        candidate.root_node.children.append(cap)
        add_cap_glow(candidate, cap)
        path = out / (candidate.name + '.mdl');MdlWriter(str(path), candidate, False, False, False).save()
        back = MdlReader(str(path)).load();actual = {n.name: n for n in nodes(back.root_node)}
        assert back.supermodel == model.supermodel
        maximum = 0
        for n in nodes(candidate.root_node):
            b = actual[n.name]
            assert tuple(n.position) == tuple(b.position) and tuple(n.orientation) == tuple(b.orientation)
            if n.nodetype == 'SKIN' and n.render:
                maximum = max(maximum, max((Vector(a) - Vector(v)).length for a, v in zip(n.verts, b.verts)))
                assert len(n.facelist.vertices) == len(b.facelist.vertices)
        assert maximum < 1e-6
        result['sides'][suffix] = {'model': candidate.name, 'roundtrip_position_error': maximum,
            'sha256': {ext: hashlib.sha256(path.with_suffix(ext).read_bytes()).hexdigest() for ext in ('.mdl', '.mdx')}}
    print('CUT_CANDIDATE_PASS p he external_head', result['rings'], flush=True)
    return result


def prepare(model, actor, out, report):
    world, lookup = {}, {}
    for n in nodes(model.root_node):
        local = Matrix.LocRotScale(Vector(n.position), Quaternion(n.orientation), Vector((n.scale,) * 3))
        world[n.name] = world[n.parent.name] @ local if n.parent else local
        n.from_root = world[n.name].copy()
        # Binary reader retains vertex normals but not every guide mesh's
        # temporary face arrays required by the writer. Reconstruct only those.
        if hasattr(n, 'facelist'):
            if len(n.facelist.normals) != len(n.facelist.vertices):
                n.facelist.normals = [tuple((Vector(n.verts[f[1]]) - Vector(n.verts[f[0]])).cross(
                    Vector(n.verts[f[2]]) - Vector(n.verts[f[0]])).normalized()) for f in n.facelist.vertices]
            if len(n.facelist.materials) != len(n.facelist.vertices):
                n.facelist.materials = [0] * len(n.facelist.vertices)
        lookup[n.name.lower()] = n
    pos = lambda name: world[lookup[name.lower()].name].translation.copy()
    skins = [n for n in nodes(model.root_node) if n.nodetype == 'SKIN' and n.render]
    inputs = {}
    for n in skins:
        frame = world[n.name]
        inputs[n.name] = [[{'p': frame @ Vector(n.verts[i]),
                           'n': (frame.to_3x3() @ Vector(n.normals[i])).normalized(),
                           'uv': Vector(n.uv1[n.facelist.uv[fi][j]]), 'w': n.weights[i]}
                          for j, i in enumerate(face)] for fi, face in enumerate(n.facelist.vertices)]
    specs = []
    for side, prefix in (('r', 'R'), ('l', 'L')):
        arm = next(n.name for n in skins if n.name.lower() == side + 'arm')
        shoulder, elbow, wrist = pos(side + 'bicep_g'), pos(prefix + 'forearm_g'), pos(prefix + 'hand_g')
        # The native torso includes part of the upper arm. Its guide joint is
        # therefore proximal to the visible arm skin's open attachment rim.
        # Cut just distal to the entire existing rim, not an assumed fraction
        # of the guide bone (which can lie outside this skin altogether).
        axis = (elbow - shoulder).normalized()
        boundary = collections.Counter(tuple(sorted((key(tri[i]['p']), key(tri[(i + 1) % 3]['p']))))
                                       for tri in inputs[arm] for i in range(3))
        points = {key(v['p']): v['p'] for tri in inputs[arm] for v in tri}
        rim = {k for edge, count in boundary.items() if count == 1 for k in edge}
        assert rim, f'{actor}/{arm}: native attachment rim missing'
        depth = max((points[k] - shoulder).dot(axis) for k in rim) + .001
        assert 0 < depth < (elbow - shoulder).length, 'Attachment rim is not on the upper arm'
        specs.append((side + 'a', shoulder + axis * depth, axis, {arm}, side))
        specs.append((side + 'h', wrist, (wrist - elbow).normalized(), {arm}, side))
        knee, ankle = pos(side + 'shin_g'), pos(side + 'foot_g')
        specs.append((side + 'l', knee, (ankle - knee).normalized(), {n.name for n in skins if n.name.lower() == 'torso'}, side))
    # Soldier pelvis plane crosses its native belt opening. The actual torso
    # joint is13.5mm above it and has a verified closed native surface contour.
    specs.append(('wa', pos('torso_g' if actor == 's' else 'pelvis_g'), Vector((0, 0, 1)), {n.name for n in skins if n.name.lower() == 'torso'}, None))
    if 'necklwr_g' in lookup:
        head_meshes = {n.name for n in skins if n.name.lower() == 'head'}
        origin = neck_clearance([tri for name in head_meshes for tri in inputs[name]], pos('necklwr_g'), Vector((0, 0, 1)))
        specs.append(('he', origin, Vector((0, 0, 1)), head_meshes, None))
    else:
        # External player head is a separate native model. Cut the visible neck
        # on the body; runtime must detach the actual equipped head separately.
        specs.append(('he', pos('headhook'), Vector((0, 0, 1)), set(), None))
    for cut, origin, axis, selected, side in specs:
        kept, detached, segments = {}, {}, []
        external_head = cut == 'he' and 'necklwr_g' not in lookup
        if external_head:
            report[cut] = prepare_external_head(out)
            continue
        for name, tt in inputs.items():
            kt, dt = [], []
            for tri in tt:
                eligible = name in selected
                if cut.endswith('l'):
                    # Knee cuts must not remove the opposite lower leg. A
                    # triangle crossing both the plane and midline is rejected.
                    xs = [v['p'].x for v in tri]
                    distal = any((v['p'] - origin).dot(axis) > EPS for v in tri)
                    if distal and min(xs) < 0 < max(xs):
                        raise ValueError(f'{actor}/{cut}: knee cut intersects midline')
                    eligible = eligible and (sum(xs) > 0 if side == 'r' else sum(xs) < 0)
                if not eligible:
                    # Hands/arms remain attached to the removed upper torso;
                    # do not saw them again merely because the bind hands lie
                    # below the waist plane.
                    (dt if cut == 'wa' and name.lower() in ('rarm', 'larm', 'head') else kt).append(tri)
                    continue
                a, b = clip(tri, origin, axis, -1), clip(tri, origin, axis, 1)
                kt.extend(triangles(a))
                dt.extend(triangles(b))
                rim = [v for v in a if abs((v['p'] - origin).dot(axis)) < 1e-6]
                if len(rim) == 2 and len(a) >= 3 and len(b) >= 3:
                    segments.append(rim)
            assert abs(area(tt) - area(kt) - area(dt)) < 1e-5, f'{actor}/{cut}/{name}: lost surface'
            kept[name], detached[name] = kt, dt
        print('CUT_DOMAIN', actor, cut, 'segments', len(segments), 'signed_range',
              [(name, min((v['p']-origin).dot(axis) for tri in inputs[name] for v in tri),
                max((v['p']-origin).dot(axis) for tri in inputs[name] for v in tri)) for name in selected], flush=True)
        rings = loops_from_segments(segments)
        tangent = axis.cross(Vector((0, 1, 0))).normalized()
        bitangent = axis.cross(tangent).normalized()
        radius = max((v['p'] - origin).length for ring in rings for v in ring)
        cap_tris = []
        for ring in rings:
            for tri in tessellate_polygon([[v['p'] for v in ring]]):
                face = []
                for p in tri:
                    if isinstance(p, int):
                        p = ring[p]['p']  # Blender 5 returns flattened vertex indices.
                    v = min(ring, key=lambda x: (x['p'] - p).length_squared).copy()
                    v['uv'] = Vector((.5 + (p - origin).dot(tangent) / (2 * radius), .5 + (p - origin).dot(bitangent) / (2 * radius)))
                    v['n'] = axis.copy()
                    face.append(v)
                if (face[1]['p'] - face[0]['p']).cross(face[2]['p'] - face[0]['p']).dot(axis) < 0:
                    face.reverse()
                cap_tris.append(face)
        assert cap_tris
        report[cut] = {'origin': list(origin), 'axis': list(axis), 'external_head': external_head,
                       'rings': [len(r) for r in rings], 'cap_triangles': len(cap_tris), 'sides': {}}
        for suffix, pieces in (('c', kept), ('l', detached)):
            candidate = copy.deepcopy(model)
            candidate.name = f'k1d{actor}{cut}{suffix}'
            cn = {n.name: n for n in nodes(candidate.root_node)}
            for name in inputs:
                if pieces[name]:
                    assign_mesh(cn[name], pieces[name], world[name])
                else:
                    cn[name].render = 0
            # Rigid eye/eyelid meshes belong to the native head. They are not
            # part of inputs (skin triangles) and must not be duplicated into
            # detached arms/hands/legs. Keep guide bones for native animation.
            for n in nodes(candidate.root_node):
                if getattr(n, 'render', False) and n.nodetype != 'SKIN':
                    ancestors = []; parent = n.parent
                    while parent:
                        ancestors.append(parent.name.lower()); parent = parent.parent
                    assert 'head_g' in ancestors, f'Unclassified rigid cut attachment: {n.name}'
                    detached_head = cut in ('he', 'wa')
                    n.render = int(detached_head == (suffix == 'l'))
            cap = SkinmeshNode('dismember_cap')
            cap.parent = candidate.root_node
            cap.node_number = max(n.node_number for n in nodes(candidate.root_node)) + 1
            cap.bitmap = 'jkcap'
            cap.shadow = 0
            cap.position = (0, 0, 0)
            cap.orientation = (1, 0, 0, 0)
            cap.scale = 1
            cap.from_root = world[candidate.root_node.name].copy()
            faces = cap_tris if suffix == 'c' else [[{**v, 'n': -axis} for v in reversed(tri)] for tri in cap_tris]
            assign_mesh(cap, faces, world[candidate.root_node.name])
            candidate.root_node.children.append(cap)
            add_cap_glow(candidate, cap)
            output = out / (candidate.name + '.mdl')
            MdlWriter(str(output), candidate, False, False, False).save()
            back = MdlReader(str(output)).load()
            assert back.supermodel == model.supermodel
            assert [(a.name, a.length) for a in back.animations] == [(a.name, a.length) for a in model.animations]
            actual = {n.name: n for n in nodes(back.root_node)}
            maximum = 0
            for n in nodes(candidate.root_node):
                b = actual[n.name]
                assert tuple(n.position) == tuple(b.position) and tuple(n.orientation) == tuple(b.orientation), f'Native joint changed: {n.name}'
                if n.nodetype == 'SKIN' and n.render:
                    maximum = max(maximum, max((Vector(a) - Vector(v)).length for a, v in zip(n.verts, b.verts)))
                    assert len(n.facelist.vertices) == len(b.facelist.vertices)
            assert maximum < 1e-6
            report[cut]['sides'][suffix] = {'model': candidate.name, 'roundtrip_position_error': maximum,
                'sha256': {ext: hashlib.sha256(output.with_suffix(ext).read_bytes()).hexdigest() for ext in ('.mdl', '.mdx')}}
        print('CUT_CANDIDATE_PASS', actor, cut, report[cut]['rings'])
    return report


def main():
    out = OUT / 'candidate'
    out.mkdir(parents=True, exist_ok=True)
    report = {'complete': False, 'sources': {}, 'models': {}, 'failures': {}}
    try:
        for actor, source in SOURCES.items():
            report['sources'][actor] = {ext: hashlib.sha256(source.with_suffix(ext).read_bytes()).hexdigest() for ext in ('.mdl', '.mdx')}
            report['models'][actor] = {}
            try:
                prepare(MdlReader(str(source)).load(), actor, out, report['models'][actor])
            except (AssertionError, ValueError) as error:
                report['failures'][actor] = str(error)
                print('CUT_ACTOR_FAIL', actor, str(error), flush=True)
        report['complete'] = not report['failures']
    finally:
        (OUT / 'geometry-preparation.json').write_text(json.dumps(report, indent=2))
    if not report['complete']:
        raise RuntimeError(f"Cut preparation incomplete: {report['failures']}")


if __name__ == '__main__':
    main()
