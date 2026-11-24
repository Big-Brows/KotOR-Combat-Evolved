"""Deployable browsing table for the approved name/weapon review; no UTC edits."""
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'platforms/out/unnamed-melee-npcs'
subprocess.run(['git', '-C', str(ROOT), 'check-ignore', '--quiet', str(OUT / 'assets/k1npcmenu.2da')], check=True)
spec = importlib.util.spec_from_file_location('review', Path(__file__).with_name('categorize-k1-npc-roster.py'))
review = importlib.util.module_from_spec(spec);spec.loader.exec_module(review)
roster = json.loads((OUT / 'spawn-menu-roster.json').read_text())
coverage = json.loads((OUT / 'catalog-report.json').read_text())['coverage']
assert {r['spawn_resource'] for r in roster} == {r['template'] for r in coverage}, 'Refresh loaded-stat roster for the current catalog before preparing the menu'
selected = [r for r in roster if r['npc'] not in ('Bouncer', 'Aqualish Thug')]
report = review.build('\n'.join('spawn resource: ' + r['spawn_resource'] for r in selected))
lines = ['2DA V2.0', '', 'faction label variant source look']
row = 0
for group in report['groups']:
    weapons = ' + '.join(' '.join(w[:1].upper() + w[1:] for w in name.split()) for name in group['equipped_weapons'])
    label = group['foundation'] + ' (' + weapons + ')'
    sources = [s for v in group['variants'] for s in v['sources']]
    for number, variant in enumerate(group['variants']):
        source = variant['sources'][0]['spawn_resource']
        for look in group['shared_appearances']:
            appearance = next(s['spawn_resource'] for s in sources if s['appearance_key'] == look['key'])
            lines.append(f'{row} "{group["faction"]}" "{label}" {number} {source} {appearance}')
            row += 1
path = OUT / 'assets/k1npcmenu.2da'
path.write_text('\n'.join(lines) + '\n', encoding='ascii')
manifest = OUT / 'catalog-report.json'
catalog = json.loads(manifest.read_text())
catalog['files'][path.name] = hashlib.sha256(path.read_bytes()).hexdigest()
# Obsolete custom overlay is no longer deployed; native alignment effects are used.
catalog['files'].pop('k1npcglow.tga', None)
catalog['weapon_menu'] = {'types': report['npc_type_count'], 'variants': report['stat_variant_count'], 'sources': report['source_count'], 'rows': row}
manifest.write_text(json.dumps(catalog, indent=2))
(OUT / 'weapon-menu-report.json').write_text(json.dumps(report, indent=2))
subprocess.run([__import__('sys').executable, str(Path(__file__).with_name('prepare-k1-npc-combat-policy.py'))], check=True)
print('WEAPON_MENU_PASS', catalog['weapon_menu'])
