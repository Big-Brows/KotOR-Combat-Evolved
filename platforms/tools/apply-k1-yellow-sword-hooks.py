"""Export exact saved Yellow sword-hook curve edits; never save source scenes."""
from pathlib import Path
import hashlib,json
import bpy

repo=Path(__file__).resolve().parents[2]
out=repo/'platforms/out/yellow-sword-edits'
paths=[out/'source/yellow_baseline.blend',out/'source/yellow_edited.blend']
hashes={p:hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
names={'01 Idle guard':'j_guard','02 Guard walking':'j_walk'}
def read(path):
 bpy.ops.wm.open_mainfile(filepath=str(path));result={}
 for a in bpy.data.actions:
  name=names.get(a.name,a.name.removeprefix('Attack '))
  curves=[c for l in a.layers for s in l.strips for b in s.channelbags for c in b.fcurves]
  q=sorted([c for c in curves if c.data_path=='pose.bones["rhand"].rotation_quaternion'],key=lambda c:c.array_index)
  assert len(q)==4
  assert all(k.interpolation=='LINEAR' for c in q for k in c.keyframe_points)
  result[name]=[[(float(k.co.x),float(k.co.y)) for k in c.keyframe_points] for c in q]
 return result
old,new=(read(p) for p in paths)
text='// Authored Yellow sword-hook quaternion curves, Blender frames at30fps.\n'
rows=[]
for index,(name,channels) in enumerate(new.items()):
 groups=[]
 for kind,values in [('old',old[name]),('new',channels)]:
  curves=[]
  for channel,keys in enumerate(values):
   identifier=f'yellowSword_{index}_{kind}_{channel}'
   text+='constexpr Key '+identifier+'[]={'+','.join('{'+f'{t:.9g}f,{v:.9g}f'+'}' for t,v in keys)+'};\n'
   # C++ floating literals must retain a decimal point or exponent.
   curves.append('{'+identifier+','+str(len(keys))+'}')
  groups.append('{'+','.join(curves)+'}')
 rows.append('{"'+name+'",'+','.join(groups)+'}')
text+='constexpr Clip clips[]={\n'+',\n'.join(rows)+'\n};\n'
import re
text=re.sub(r'(?<![\w.])(-?\d+)f\b',r'\1.f',text)
(repo/'src/game/clientcore/include/k1yellowswordhooks.inc').write_text(text)
assert all(hashlib.sha256(p.read_bytes()).hexdigest()==h for p,h in hashes.items())
(out/'export-report.json').write_text(json.dumps({'source_hashes':{str(p.relative_to(repo)):h for p,h in hashes.items()},'clips':list(new),'policy':'Exact LINEAR component curves, all authored and retained keys; no resampling or source edits.'},indent=2))
print('YELLOW_SWORD_HOOK_EXPORT_PASS',len(new),flush=True)
