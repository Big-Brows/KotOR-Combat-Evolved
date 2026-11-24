"""Generate a separate staff Dark Jedi using the established staff solver.
Owned assets and diagnostic/generated output remain ignored; no completed assets change.
Run with the installed Blender in background mode from the checkout root.
"""
from pathlib import Path
import hashlib,json,subprocess,sys
repo=Path(__file__).resolve().parents[2]
engage=repo/'platforms/out/k1-engage-prototype'
work=engage/'dark-staff'
subprocess.run(['git','check-ignore','--quiet',str(work/'assets')],check=True)
work.mkdir(exist_ok=True)
profile=engage/'elbow-arena/profile'
protected=[profile/'override'/(n+e) for n in ('k1jastaff','k1jarig','k1yellow','k1jadark','k1jasith') for e in ('.mdl','.mdx')]
digests={str(p.relative_to(repo)):hashlib.sha256(p.read_bytes()).hexdigest() for p in protected}
code=(repo/'platforms/tools/prepare-k1-staff-prototype.py').read_text()
def replace(a,b):
 global code
 assert a in code,a
 code=code.replace(a,b)
replace("output = engage / 'staff-prototype'","output = engage / 'dark-staff/generation'")
replace('output.mkdir(exist_ok=True)','output.mkdir(parents=True, exist_ok=True)')
# Keep expected-pose diagnostics separate from the completed player staff.
replace('profile = baseline_profile  #',"profile = work / 'diagnostics'  #")
(work/'diagnostics/override').mkdir(parents=True,exist_ok=True)
# The solver builds helper joints from this body's own native binds, using the
# established semantic mesh aliases for its preserved Dark Jedi skin.
replace("recipe = (engage / 'elbow-arena/generate-candidate-applied.py').read_text()", """recipe = (engage / 'elbow-arena/generate-candidate-applied.py').read_text()
recipe = recipe.replace("model_file=root/'source/pmbbl.mdl';model=MdlReader(str(model_file)).load()", "model_file=engage/'dark-jedi/source/n_darkjedim.mdl';model=MdlReader(str(model_file)).load()\\nnext(n for n in model.root_node.children if n.name=='torso').name='Torso'\\nnext(n for n in model.root_node.children if n.name=='Larm').name='LArm'")
""")
replace("override/k1jarig", "override/k1jadark")
replace("model.name = 'k1jastaff'", "model.name = 'k1jadstaff'")
replace("'k1jastaff.mdl'", "'k1jadstaff.mdl'")
replace("('k1jastaff' + extension)", "('k1jadstaff' + extension)")
# Backpack-only panel closure is inapplicable to the closed Dark Jedi robe.
replace("mesh_cap = close_staff_back_panel(next(n for n in nodes(model.root_node) if n.name == 'Torso'))", "mesh_cap = {'added_faces': 0, 'policy': 'Dark Jedi native mesh retained'}")
replace("(mesh_cap['added_faces'] if before.name == 'Torso' else 0)", "0")
# Use the already reviewed body-specific weight distribution, including frozen
# native neck boundaries. Preserve all other original skin/geometry.
replace('staff_weight_report = apply_staff_torso_weights(model, staff_weight_bind)', """review=json.loads((engage/'yellow-weight-review/dark-02/skin-weights.json').read_text())
for n in nodes(model.root_node):
    if n.nodetype=='SKIN':
        assert len(review['weights'][n.name])==len(n.weights)
        n.weights=review['weights'][n.name]
staff_weight_report={'policy':'reviewed Dark Jedi waist/chest weights; native neck boundary preserved'}""")
# Generated bind helpers reference the original calibration beside their solver.
(work/'generation/attacks-blocks').mkdir(parents=True,exist_ok=True)
(output_ref:=work/'generation/profile').mkdir(parents=True,exist_ok=True)
import shutil
shutil.copy2(engage/'staff-prototype/profile/yellow-native-motion.txt',output_ref/'yellow-native-motion.txt')
(work/'generation-recipe.py').write_text(code)
exec(compile(code,str(repo/'platforms/tools/prepare-k1-staff-prototype.py'),'exec'),dict(__file__=str(repo/'platforms/tools/prepare-k1-staff-prototype.py'),work=work))
assert all(hashlib.sha256((repo/p).read_bytes()).hexdigest()==h for p,h in digests.items())
(work/'protected-assets.json').write_text(json.dumps(digests,indent=2))
print('DARK_STAFF_PROTECTED_ASSETS_PASS',len(digests))

# Distinct UTC identity; retain native Dark Jedi stats, powers and scripts-cleared
# fixture setup. Runtime equips the native Dark Jedi red double saber through existing API.
import ast,struct,importlib.util
spec=importlib.util.spec_from_file_location('owned_dark',engage/'dark-jedi/inspect-owned.py')
owned=importlib.util.module_from_spec(spec);spec.loader.exec_module(owned)
tree=ast.parse((engage/'build-arena.py').read_text())
patch_class=next(n for n in tree.body if isinstance(n,ast.ClassDef) and n.name=='Patch')
scope={'struct':struct};exec(compile(ast.Module(body=[patch_class],type_ignores=[]),'<existing-gff-patch>','exec'),scope)
raw=(profile/'override/k1darktest.utc').read_bytes();patch=scope['Patch'](raw)
patch.set('Tag','k1arena_darkstaff');patch.set('TemplateResRef','k1darkstaff');patch.set('FirstName','Dark Jedi (Double Blade)')
result=patch.save();before=owned.gff(raw);after=owned.gff(result)
assert all(before[k]==after[k] for k in before if k not in ('Tag','TemplateResRef','FirstName'))
(work/'k1darkstaff.utc').write_bytes(result)
for extension in ('.mdl','.mdx'):
    shutil.copy2(work/'generation/assets'/('k1jadstaff'+extension),profile/'override'/('k1jadstaff'+extension))
shutil.copy2(work/'k1darkstaff.utc',profile/'override/k1darkstaff.utc')
assert all(hashlib.sha256((repo/p).read_bytes()).hexdigest()==h for p,h in digests.items())
print('DARK_STAFF_TEMPLATE_INSTALL_PASS native_stats_and_powers=1 completed_assets_unchanged=1')
