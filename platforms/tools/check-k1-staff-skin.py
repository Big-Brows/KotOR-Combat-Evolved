"""Run the preserved skin diagnostics on the isolated staff connected-pose dump.

Blender background recipe. These measurements are diagnostics, not a declaration
that every small triangle must stay within KotOR's native animation envelope.
Visual acceptance at gameplay distance remains required.
"""
from pathlib import Path
import json
import shutil
import subprocess
import sys

repo = Path(__file__).resolve().parents[2]
engage = repo / 'platforms/out/k1-engage-prototype'
output = engage / 'staff-prototype'
solver = output / 'solver'
profile = engage / 'elbow-arena/profile'
subprocess.run(['git', '-C', str(repo), 'check-ignore', '--quiet', str(solver)], check=True)
assert (profile / 'staff-connected-poses.txt').is_file(), 'Run engage-k1.cmd -StaffProbe first'
def bindings(path):
    return {tuple(f[:2]): list(map(float, f[2:])) for f in (line.split() for line in path.read_text().splitlines())}
actual = bindings(profile / 'staff-native-bind.txt')
baseline = bindings(engage / 'elbow-arena/profile/rig-native-bind.txt')
assert actual.keys() == baseline.keys(), 'Native palette identity changed'
bind_error = max(abs(a-b) for key in actual for a, b in zip(actual[key], baseline[key]))
assert bind_error < .000002, bind_error
print('STAFF_NATIVE_SKIN_BIND_PASS', len(actual), bind_error)
for extension in ('.mdl', '.mdx'):
    shutil.copy2(output / ('assets/k1jastaff' + extension), solver / ('k1jastaff' + extension))
for p in (engage / 'rig-candidate').glob('full-skin-native*.npz'):
    shutil.copy2(p, solver / p.name)
shutil.copy2(engage / 'rig-candidate/skin-area-native.npz', solver / 'skin-area-native.npz')
code = (engage / 'rig-candidate/check-full-skin.py').read_text()
code = code.replace('out=root.parent.parent', 'out=root.parents[2]')
code = code.replace("profile=root.parent/'profile'", "profile=root.parents[1]/'elbow-arena/profile'")
code = code.replace("'k1jarig", "'k1jastaff")
code = code.replace('override/k1jarig.mdl', 'override/k1jastaff.mdl')
code = code.replace("root.parent/'corrections/source/pmbbl.mdl'", "root.parents[1]/'corrections/source/pmbbl.mdl'")
code = code.replace("profile/'rig-timeline.txt'", "profile/'staff-connected-poses.txt'")
code = code.replace("'rig-native-bind.txt'", "'staff-native-bind.txt'")
(solver / 'check-full-skin.py').write_text(code)
sys.argv = [sys.argv[0], '--expanded-reference']
for name in ('check-full-skin.py', 'check-full-skin-areas.py', 'check-cross-skin-seams.py'):
    p = solver / name
    exec(compile(p.read_text(), str(p), 'exec'), {'__file__': str(p)})
seams = json.loads((solver / 'cross-skin-seams-candidate.json').read_text())
assert seams['poses'] >= 2000 and seams['pairs'] == 18 and seams['maximum_gap'] < .00001, seams
print('STAFF_CLOSED_SEAMS_PASS', seams['poses'], seams['maximum_gap'])
