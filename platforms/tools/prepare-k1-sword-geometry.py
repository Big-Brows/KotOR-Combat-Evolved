"""Measure native sword blade/hilt boundaries; no game assets enter source.

Run in the installed Blender Python. Uses the existing owned-resource reader.
Only derived blade endpoints are written to the engine's geometry contract.
"""
from pathlib import Path
import hashlib
import json
import runpy
import subprocess
import sys
from mathutils import Matrix, Quaternion, Vector

repo=Path(__file__).resolve().parents[2]
out=repo/'platforms/out/swords'
subprocess.run(['git','-C',str(repo),'check-ignore','--quiet',str(out/'geometry-report.json')],check=True)
out.mkdir(exist_ok=True)
owned=runpy.run_path(str(repo/'platforms/out/k1-engage-prototype/dark-jedi/inspect-owned.py'))
sys.path.insert(0,str(repo/'platforms/out/kotor-animation-import/tools'))
from io_scene_kotor.format.mdl.reader import MdlReader
prefixes=('w_lngswrd_','w_shortswrd_','w_vbroswrd_','w_vbroshort_','w_dblswrd_','w_vbrdblswd_','w_warblade_')
names=sorted(n for n,t in owned['entries'] if t==2002 and n.startswith(prefixes))
assert len(names)>=25
rows=[];reports=[]
for name in names:
    hashes={}
    for kind,ext in ((2002,'mdl'),(3008,'mdx')):
        data=owned['resource'](name,kind);(out/(name+'.'+ext)).write_bytes(data)
        hashes[ext]=hashlib.sha256(data).hexdigest()
    model=MdlReader(str(out/(name+'.mdl'))).load();points=[];rest={}
    def visit(n):
        local=Matrix.LocRotScale(Vector(n.position),Quaternion(n.orientation),Vector((n.scale,)*3))
        rest[n.name]=rest[n.parent.name]@local if n.parent else local
        if getattr(n,'render',True):points.extend(rest[n.name]@Vector(v) for v in getattr(n,'verts',[]))
        for c in n.children:visit(c)
    visit(model.root_node)
    lo=min(p.z for p in points);hi=max(p.z for p in points)
    double=name.startswith(('w_dblswrd_','w_vbrdblswd_','w_warblade_'))
    def guard(a,b,lower):
        band=[p for p in points if a<=p.z<=b];assert band,(name,a,b)
        width=max(p.x*p.x+p.y*p.y for p in band)
        rim=[p.z for p in band if p.x*p.x+p.y*p.y>=width*.49]
        return min(rim) if lower else max(rim)
    low_grip=guard(-.3,.05,True) if double else lo
    high_grip=guard(.05,.6,False) if double else guard(0,.2,False)
    assert lo<=low_grip<high_grip<hi,(name,lo,low_grip,high_grip,hi)
    assert hi-high_grip>.25 and (not double or low_grip-lo>.25)
    grip=[p for p in points if low_grip-1e-6<=p.z<=high_grip+1e-6]
    bounds=[min(p.x for p in grip),max(p.x for p in grip),min(p.y for p in grip),max(p.y for p in grip)]
    rows.append(f'    {{"{name}",{low_grip:.9f}f,{high_grip:.9f}f,{lo:.9f}f,{hi:.9f}f,{str(double).lower()},'+','.join(f'{v:.9f}f' for v in bounds)+'},')
    reports.append({'resource':name,'native_sha256':hashes,'grip':[low_grip,high_grip],'tips':[lo,hi],'double':double,'grip_xy_bounds':bounds})
dest=repo/'src/game/clientcore/include/k1swordgeometry.inc'
dest.write_text('// Derived native sword guard-rim and tip measurements; no mesh data.\n'
    'struct SwordGeometry {const char* resource;float gripLow,gripHigh,tipLow,tipHigh;bool doubleBlade;float gripXLow,gripXHigh,gripYLow,gripYHigh;};\n'
    'constexpr SwordGeometry swordGeometry[]={\n'+'\n'.join(rows)+'\n};\n')
(out/'geometry-report.json').write_text(json.dumps({'models':reports,'count':len(rows),'policy':'native guard-rim width in authored grip neighborhoods; retain the full central grip; world pose uses existing item model transforms'},indent=2))
print('SWORD_GEOMETRY_PASS',len(rows),'native sword models',flush=True)
