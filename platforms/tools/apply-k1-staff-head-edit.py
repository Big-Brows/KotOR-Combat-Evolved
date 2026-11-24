"""Import only the authored head orientation for the implemented staff guard.

Run using Blender --background --disable-autoexec --python-exit-code 1 --python
this-file. Writes a small authored orientation include, never animation assets.
"""
from pathlib import Path
import hashlib
import json
import subprocess
import bpy
from mathutils import Matrix, Quaternion, Vector

repo = Path(__file__).resolve().parents[2]
out = repo / 'platforms/out/staff-guard-edit'
subprocess.run(['git','-C',str(repo),'check-ignore','--quiet',str(out/'head-import.json')],check=True)
blend = out / 'kotor_staff_idle_guard_with_head.blend'
before = hashlib.sha256(blend.read_bytes()).hexdigest()
bpy.ops.wm.open_mainfile(filepath=str(blend))
rig = next(o for o in bpy.context.scene.objects if o.type == 'ARMATURE')
local = {}
for frame in (1,30):
    bpy.context.scene.frame_set(frame)
    bpy.context.view_layer.update()
    b = rig.pose.bones['headhook']
    local[frame] = b.parent.matrix.inverted() @ b.matrix
assert max(abs(local[1][i][j]-local[30][i][j]) for i in range(4) for j in range(4)) < 1e-5
p,q,s = local[1].decompose()
assert max(abs(x-1) for x in s) < 1e-5
q.normalize()
dest = repo / 'src/game/clientcore/include/k1staffguardhead.inc'
if dest.exists():
    (out / 'k1staffguardhead-before.inc').write_bytes(dest.read_bytes())
dest.write_text('// Authored player double-blade guard head orientation (w, x, y, z).\n'
                '// Imported from the local Blender editing scene; other stances are unchanged.\n'
                'constexpr float staffGuardHeadOrientation[4] = {' + ', '.join(f'{x:.9f}f' for x in q) + '};\n')
assert before == hashlib.sha256(blend.read_bytes()).hexdigest()
(out/'head-import.json').write_text(json.dumps({'blend_sha256':before,'frame':1,'bone':'headhook',
    'orientation_wxyz':list(q),'position_unchanged_by_import':True,'source_file_preserved':True},indent=2))
print('STAFF_HEAD_IMPORT_PASS', list(q),flush=True)
