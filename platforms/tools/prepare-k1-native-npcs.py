"""Prepare the unnamed humanoid sandbox catalog from an owned K1 installation.

No deployment or installation writes. Generated UTC/UTI data and reports stay in
ignored platforms/out. Native character data is retained; sandbox clones clear
story callbacks/dialogue and receive collision-free resource names. Local item
templates are copied byte-for-byte under content-derived aliases.
"""
from pathlib import Path
import argparse
import ast
import hashlib
import importlib.util
import json
import re
import struct
import subprocess
import sys

REPO = Path(__file__).resolve().parents[2]
OUT = REPO / 'platforms/out/unnamed-melee-npcs'
NAMES = {
    'Wookiee', 'Wookiee Rebel', 'Wookiee Guard', 'Wookiee Thug',
    'Wookiee Warrior', 'Wookiee Elite Warrior',
    'Gamorrean', 'Gamorrean Elite', 'Gamorrean Chief', 'Gamorrean Raider',
    'Gamorrean Raid Leader', 'Gamorrean Patrol', 'Gamorrean Warrior', 'Vulkar Lab Guard',
    'Aqualish Thug', 'Black Vulkar', 'Black Vulkar Captain', 'Black Vulkar Thug', 'Bouncer', 'Bounty Hunter',
    'Captive Republic Soldier', 'Czerka Guard', 'Czerka Guard Captain',
    'Czerka Liaison Officer', 'Czerka Merchant', 'Czerka Patrol Guard',
    'Czerka Scientist', 'Czerka Thug', 'Dark Jedi', 'Dark Jedi Apprentice',
    'Dark Jedi Master', 'Drunk Man', 'Duros', 'Duros Warrior',
    'Elder Councillor', 'Elder Rakata', 'Elder Researcher', 'Elder Elite Warrior', 'Elder Warrior', 'Elite Warrior',
    'Garage Head', 'Genoharadan Hunter', 'Guard', 'Hidden Bek',
    'Hidden Bek Guard', 'Hidden Bek Lookout', 'Hidden Bek Captain', 'Hidden Bek Thug', 'Holy Warrior', 'Jedi',
    'Jedi Knight', 'Junior Czerka Scientist', 'Lavin Thug', 'Mandalorian',
    'Mandalorian Captain', 'Mandalorian Commander', 'Mandalorian Elite',
    'Mandalorian Warrior', 'Mercenary', 'Miner', 'Mining Team Captain',
    'Rakata', 'Rakatan Elite Warrior', 'Rakatan Warrior', 'Republic Soldier',
    'Rodian', 'Rodian Prisoner', 'Rodian Thug', 'Rodian Crony', 'Sand People Chieftain',
    'Sand People Guard', 'Sand People Warrior', 'Sith Academy Guard',
    'Sith Acolyte', 'Sith Advanced Trooper', 'Sith Assault Trooper', 'Sith Apprentice', 'Sith Captain',
    'Sith Commander', 'Sith Diplomat', 'Sith Duelist', 'Sith Elite Trooper',
    'Sith Elite Warrior', 'Sith Governor', 'Sith Grenadier', 'Sith Guard',
    'Sith Heavy Trooper', 'Sith Interrogator', 'Sith Lieutenant', 'Sith Lord', 'Sith Master',
    'Sith Patrol Leader', 'Sith Security Officer', 'Sith Soldier', 'Sith Student',
    'Sith Teacher', 'Sith Thug', 'Sith Thug Leader', 'Sith Trooper',
    'Spice Lab Tech', 'Tarisian Noble', 'Technician', 'Thug', 'Trandoshan', "Twi'lek Thug",
    'Vulkar Bartender', 'Vulkar Cook', 'Vulkar Enforcer', 'Vulkar Gang Boss',
    'Vulkar Gang Member', 'Vulkar Guard', 'Vulkar Lieutenant',
}
MELEE = {'Stun_Baton', 'Quarter_Staff', 'Ghaffi_Stick', 'Long_Sword',
         'Short_Sword', 'Vibro_Sword', 'Vibro_Blade', 'Double_Bladed_Sword',
         'Vibro_Double_Blade', 'Lightsaber', 'Short_Lightsaber',
         'Double_Bladed_Lightsaber', 'Wookie_Warblade', 'Gammorean_Battleaxe'}
# Installed-data evidence: these variants have equipment records but spawn with
# no native right-hand weapon in this checkout. Exclude them under the user's
# explicit unarmed-NPC exclusion; do not force gear or alter their native feats.
UNARMED_VARIANTS = {
    ('danm14ab_s', 'dan14ab_duros01.utc'),
    ('lev_m40aa_s', 'lev_sithtroop003.utc'),
    ('lev_m40aa_s', 'lev_sithtroop004.utc'),
    ('tar_m10ab_s', 'tar10_vulk10ab2.utc'),
    ('tar_m10ab_s', 'tar10_vulk10ab3.utc'),
    ('tar_m10ab_s', 'tar10_vulkar10ab.utc'),
    ('tar_m10ab_s', 'tar10_vulkguard.utc'),
}


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args()
    subprocess.run(['git', '-C', str(REPO), 'check-ignore', '--quiet',
                    str(OUT / 'assets')], check=True)
    OUT.mkdir(parents=True, exist_ok=True)
    assets = OUT / 'assets'
    assets.mkdir(exist_ok=True)
    sys.path.insert(0, str(REPO / 'tools'))
    from gffdump import gff, read_archive
    # Reuse the established typed, section-preserving GFF editor. Load only its
    # class definition; importing the arena builder would regenerate that arena.
    tree = ast.parse((REPO / 'platforms/out/k1-engage-prototype/build-arena.py').read_text())
    cls = next(n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == 'Patch')
    scope = {'struct': struct}
    exec(compile(ast.Module(body=[cls], type_ignores=[]), '<arena-gff-editor>', 'exec'), scope)
    Patch = scope['Patch']
    spec = importlib.util.spec_from_file_location('owned_k1', REPO /
        'platforms/out/k1-engage-prototype/dark-jedi/inspect-owned.py')
    owned = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(owned)
    base = owned.twoda(owned.resource('baseitems', 2017))
    appearances = owned.twoda(owned.resource('appearance', 2017))
    tlk = (owned.game / 'dialog.tlk').read_bytes()
    text_start = struct.unpack_from('<I', tlk, 16)[0]

    def display(value):
        match = re.fullmatch(r'strref (-?\d+) (.*)', str(value))
        if not match:
            return str(value)
        ref = int(match[1])
        if ref < 0:
            return match[2].strip("'")
        off, size = struct.unpack_from('<II', tlk, 20 + 40 * ref + 28)
        return tlk[text_start + off:text_start + off + size].decode('cp1252').strip()

    archives = [(p.stem, read_archive(p)) for p in sorted((owned.game / 'modules').glob('*_s.rim'))]
    archives.append(('global', {n + '.utc': owned.resource(n, t)
                     for n, t in owned.entries if t == 2027}))
    rows, audit, files = [], [], set()
    for module, archive in archives:
        for resource, raw in sorted(archive.items()):
            if not resource.endswith('.utc'):
                continue
            if (module, resource) in UNARMED_VARIANTS:
                continue
            native = gff(raw)
            title = display(native.get('FirstName', ''))
            if title not in NAMES:
                continue
            appearance = appearances.get(str(native.get('Appearance_Type')), {})
            race = appearance.get('race', '').lower()
            if race.startswith(('c_', 'p_zaalb')) and race!='c_gammorean':
                continue
            items, local_items = [], {}
            for slot in native.get('Equip_ItemList', []) + native.get('ItemList', []):
                rr = slot.get('EquippedRes') or slot.get('InventoryRes')
                if not rr:
                    continue
                item_raw = archive.get(rr.lower() + '.uti')
                if item_raw is None:
                    try:
                        item_raw = owned.resource(rr.lower(), 2025)
                    except KeyError:
                        raise RuntimeError(f'Missing item: {module}/{resource}: {rr}')
                item = gff(item_raw)
                label = base.get(str(item.get('BaseItem')), {}).get('label', '')
                items.append({'resource': rr, 'label': label,
                              'weapon': base.get(str(item.get('BaseItem')), {}).get('weaponwield') in ('1','2','3','4','5','6'),
                              'equipped': 'EquippedRes' in slot})
                local_items[rr.lower()] = item_raw
            melee = any(i['label'] in MELEE for i in items)
            if not melee or not any(i['equipped'] and i['weapon'] for i in items):
                continue
            alias = 'k1n' + sha((module + '/' + resource).encode())[:12]
            patch = Patch(raw)
            changes = {}
            for field in native:
                if field.startswith('Script') or field == 'Conversation':
                    # Story callbacks may teleport/delete actors or mutate quests.
                    # Use the native generic combat callbacks in this bare arena.
                    value = native[field]
                    keep = value.startswith(('k_def_', 'k_ai_')) if isinstance(value, str) else False
                    if value and not keep:
                        patch.set(field, '')
                        changes[field] = ''
            if 'TemplateResRef' in native:
                patch.set('TemplateResRef', alias)
                changes['TemplateResRef'] = alias
            # Item lists have nested structs. Patch their ResRef field payloads
            # in place, retaining list IDs, slot IDs, properties and every stat.
            replacements = {}
            for rr, item_raw in local_items.items():
                item_alias = 'k1i' + sha(item_raw)[:12]
                target = assets / (item_alias + '.uti')
                target.write_bytes(item_raw)
                files.add(target.name)
                assert target.read_bytes() == item_raw
                replacements[rr] = item_alias
            for i in range(len(patch.parts[1]) // 12):
                kind, label, offset = struct.unpack_from('<III', patch.parts[1], i * 12)
                if kind != 11 or patch.labels[label] not in ('EquippedRes', 'InventoryRes'):
                    continue
                size = patch.parts[3][offset]
                old = bytes(patch.parts[3][offset + 1:offset + 1 + size]).decode().lower()
                new = replacements[old].encode()
                struct.pack_into('<I', patch.parts[1], i * 12 + 8, len(patch.parts[3]))
                patch.parts[3] += bytes([len(new)]) + new
            generated = patch.save()
            after = gff(generated)
            expected = json.loads(json.dumps(native))
            expected.update(changes)
            for slot in expected.get('Equip_ItemList', []) + expected.get('ItemList', []):
                for field in ('EquippedRes', 'InventoryRes'):
                    if field in slot:
                        slot[field] = replacements[slot[field].lower()]
            assert after == expected, (module, resource, 'Native data changed')
            (assets / (alias + '.utc')).write_bytes(generated)
            files.add(alias + '.utc')
            entry = {'label': title, 'module': module, 'source': resource,
                     'template': alias, 'tag': native.get('Tag'),
                     'appearance': native.get('Appearance_Type'),
                     'body': race, 'appearance_data': appearance, 'items': items,
                     'source_sha256': sha(raw), 'sandbox_fields': changes,
                     'native_data_preserved': True, 'status': 'native fallback; Engage body support pending'}
            rows.append(entry)
            audit.append(entry)
    rows.sort(key=lambda r: (r['label'], r['module'], r['source']))
    from k1_spawn_menu_catalog import metadata, saber_color
    weapon_names = {}
    def weapon_name(item):
        name = display(item.get('LocalizedName', 'Weapon'))
        if item.get('BaseItem') not in (8, 9, 10):
            return name
        model = base[str(item['BaseItem'])]['itemclass'] + '_%03d' % item.get('ModelVariation', 1)
        if model not in weapon_names:
            weapon_names[model] = saber_color(owned.resource(model.lower(), 2002))
        return weapon_names[model] + ' ' + name
    indicators = json.loads(Path(__file__).with_name('k1-spawn-variant-names.json').read_text())
    assert all(re.fullmatch('[0-9a-f]{16}', k) and isinstance(v, str) and v.strip() for k, v in indicators.items())
    metadata(rows, assets, gff, weapon_name, indicators)
    lines = ['2DA V2.0', '', 'label module source template appearance faction variant indicator model look typeorder']
    for i, row in enumerate(rows):
        menu = row['menu']
        quoted = lambda value: '"' + str(value).replace('"', "'") + '"'
        lines.append(f'{i} "{row["label"]}" {row["module"]} {row["source"][:-4]} {row["template"]} {row["appearance"]} ' + ' '.join(quoted(menu[k]) for k in ('faction', 'variant', 'indicator', 'appearance', 'appearance_key', 'type_order')))
    (assets / 'k1npcs.2da').write_text('\n'.join(lines) + '\n', encoding='ascii')
    files.add('k1npcs.2da')
    manifest = {'native_data_pass': True, 'templates': len(rows),
                'groups': len({r['label'] for r in rows}),
                'files': {n: sha((assets / n).read_bytes()) for n in sorted(files)},
                'coverage': audit, 'scope': 'unnamed armed humanoids including Wookiees/Gamorreans; no named characters, Selkath or unarmed actors',
                'excluded_runtime_unarmed': [f'{module}/{rr}' for module, rr in sorted(UNARMED_VARIANTS)],
                'limitations': 'No game integration verification. Story callbacks cleared; native generic callbacks retained. No stats/powers/feats/equipment edits.'}
    (OUT / 'catalog-report.json').write_text(json.dumps(manifest, indent=2))
    browsing = {}
    for row in rows:
        menu = row['menu']
        identity = (menu['faction'], row['label'], menu['variant'])
        group = browsing.setdefault(identity, {'faction': menu['faction'], 'npc': row['label'],
            'variant_key': menu['variant'], 'appearances': {}, 'sources': []})
        group['sources'].append(f'{row["module"]}/{row["source"]}')
        group['appearances'].setdefault(menu['appearance_key'], {
            'indicator': menu['indicator'], 'appearance': menu['appearance'],
            'body': row['body'], 'template': row['template'],
            'source': f'{row["module"]}/{row["source"]}'})
    groups = list(browsing.values())
    for group in groups:
        group['appearances'] = list(group['appearances'].values())
    (OUT / 'menu-report.json').write_text(json.dumps({'variants': len(groups),
        'appearances': sum(len(g['appearances']) for g in groups), 'groups': groups}, indent=2))
    print(f'NATIVE_NPC_CATALOG_PASS templates={len(rows)} groups={manifest["groups"]} assets={len(files)}')
    if (OUT / 'spawn-menu-roster.json').exists():
        subprocess.run([sys.executable, str(Path(__file__).with_name('prepare-k1-weapon-menu.py'))], check=True)


if __name__ == '__main__':
    main()
