"""Produce a naming proposal; never edits the live catalog, game or templates.

Use the verified loaded-stat roster, existing factions and the user's annotated
roster. Exact source identities remain in the JSON even when the naming list
combines their appearances. Output must be ignored local analysis storage.
"""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from gffdump import gff
from k1_spawn_menu_catalog import FACTIONS, combat_stats, list_ids


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True).encode()).hexdigest()[:16]


def included_sources(text):
    included = re.findall(r'spawn resource: (\w+)', text)
    assert len(included) == len(set(included)), 'Repeated source in edited roster'
    return included


def build(edited):
    origin = ROOT / 'platforms/out/unnamed-melee-npcs'
    roster = json.loads((origin / 'spawn-menu-roster.json').read_text())
    catalog = {r['template']: r for r in json.loads((origin / 'catalog-report.json').read_text())['coverage']}
    included = included_sources(edited)
    # Retain only the attachment's temporary exclusions. All custom Black
    # Vulkar names and merge instructions have been explicitly superseded.
    names = {}
    assert set(included) <= {r['spawn_resource'] for r in roster}
    rows = []
    for item in roster:
        alias = item['spawn_resource']
        if alias not in included:
            continue
        row = dict(item)
        native = gff((origin / 'assets' / (alias + '.utc')).read_bytes())
        row['faction'] = catalog[alias]['menu']['faction']
        row['model_family'] = catalog[alias]['appearance_data'].get('race', '').lower()
        row['assigned_name'] = names.get(alias)
        row['feat_ids'] = sorted(f['Feat'] for f in native.get('FeatList', []))
        assert len(row['feat_ids']) == row['feat_count']
        row['native_combat_fields'] = combat_stats(native)
        row['native_equipment'] = native.get('Equip_ItemList', [])
        row['native_inventory'] = native.get('ItemList', [])
        stat_profile = dict(row['native_combat_fields'])
        stat_profile['FeatList'] = row['feat_ids']
        row['stat_profile_key'] = digest([row['level'], row['hp'], row['force'], row['feat_count'], stat_profile])
        visual_items = []
        for slot, equipment in zip(list_ids((origin / 'assets' / (alias + '.utc')).read_bytes(), 'Equip_ItemList'), row['native_equipment']):
            uti = gff((origin / 'assets' / (equipment['EquippedRes'] + '.uti')).read_bytes())
            # The appearance picker is for NPC art, not a second weapon picker.
            # Fixed full-body models also ignore equipped armor's body art.
            if slot in (1, 2) and catalog[alias]['appearance_data'].get('modeltype') == 'B':
                visual_items.append([slot, {k: uti.get(k) for k in ('BaseItem', 'ModelVariation', 'TextureVar', 'BodyVariation')}])
        row['appearance_key'] = digest([row['appearance_row'], row['body'].lower(),
            native.get('Appearance_Head'), native.get('BodyVariation'), native.get('TextureVar'), visual_items])
        row['visual_weapons'] = catalog[alias]['menu']['indicator']
        rows.append(row)
    assert len(rows) == len(included)
    buckets = defaultdict(list)
    for row in rows:
        weapons = tuple(sorted(row['equipped_weapons']))
        # Only original name and weapon combination distinguish middle-menu
        # entries. Level and every other native stat belong to variants.
        key = (row['faction'], row['npc'], weapons)
        buckets[key].append(row)
    groups = []
    for key, entries in buckets.items():
        parts = [entries]
        for part in parts:
            faction, foundation, weapons = key
            original_labels = sorted({r['npc'] for r in part})
            foundation = part[0]['assigned_name'] or ' / '.join(original_labels)
            variants = defaultdict(list)
            for row in part:
                identity = (row['level'], row['hp'], row['force'], row['feat_count'], tuple(row['feat_ids']), row['stat_profile_key'])
                variants[identity].append(row)
            appearances = {}
            for row in part:
                appearances.setdefault(row['appearance_key'], {'key': row['appearance_key'],
                    'label': row['appearance'], 'row': row['appearance_row'], 'body': row['body'],
                    'weapon_visual': row['visual_weapons'], 'example_source': row['module'] + '/' + row['source']})
            appearances = sorted(appearances.values(), key=lambda a: (a['row'], a['body'], a['weapon_visual'], a['key']))
            for i, appearance in enumerate(appearances, 1):appearance['id'] = 'A' + str(i)
            app_ids = {a['key']: a['id'] for a in appearances}
            details = []
            for i, (_, copies) in enumerate(sorted(variants.items()), 1):
                example = copies[0]
                details.append({'id': 'V' + str(i), 'level': example['level'], 'hp': example['hp'], 'force': example['force'],
                    'stat_profile_key': example['stat_profile_key'],
                    'feat_count': example['feat_count'], 'feat_sets': sorted({tuple(r['feat_ids']) for r in copies}),
                    'native_weapon_visuals': sorted({r['visual_weapons'] for r in copies}),
                    'other_native_stat_profiles': len({digest({k: r['native_combat_fields'].get(k) for k in ('Str', 'Dex', 'Con', 'Int', 'Wis', 'Cha', 'SkillList', 'ClassList', 'SpecAbilityList')}) for r in copies}),
                    'original_appearance_ids': sorted({app_ids[r['appearance_key']] for r in copies}),
                    'sources': copies})
            groups.append({'faction': faction, 'foundation': foundation,
                'assigned_name': part[0]['assigned_name'], 'original_labels': original_labels,
                'equipped_weapons': weapons,
                'variants': details, 'shared_appearances': appearances,
                'carried_loadouts': sorted({tuple(sorted(r['carried_weapons'])) for r in part}),
                'source_count': len(part)})
    def order(group):
        faction = group['faction']
        label = group['original_labels'][0]
        return (list(FACTIONS).index(faction), FACTIONS[faction].index(label),
                group['assigned_name'] or '', tuple(group['equipped_weapons']),
                min(v['hp'] for v in group['variants']), min(v['force'] for v in group['variants']))
    groups.sort(key=order)
    for i, group in enumerate(groups, 1):group['id'] = 'NPC-' + str(i).zfill(3)
    flattened = [r['spawn_resource'] for g in groups for v in g['variants'] for r in v['sources']]
    assert len(flattened) == len(set(flattened)) == len(included)
    assert set(flattened) == set(included)
    for group in groups:
        source = [r for v in group['variants'] for r in v['sources']]
        assert all(tuple(sorted(r['equipped_weapons'])) == tuple(group['equipped_weapons']) for r in source)
        assert len({r['npc'] for r in source}) == 1
        assert not group['assigned_name']
    return {'kind': 'Naming proposal only; live menu and native data unchanged',
        'group_policy': 'original name plus equipped weapon combination; all stat differences are variants',
        'feat_policy': 'distinct', 'label_policy': 'separate', 'source_count': len(rows), 'npc_type_count': len(groups),
        'stat_variant_count': sum(len(g['variants']) for g in groups),
        'omitted': [r for r in roster if r['spawn_resource'] not in included], 'groups': groups}


def text_report(report):
    lines = ['NPC NAMING REVIEW — PROPOSED MIDDLE-MENU ENTRIES',
        f"{report['source_count']} source templates / {report['npc_type_count']} NPC types / {report['stat_variant_count']} stat variants", '',
        'This is a review list. The live menu, templates and native stats are unchanged.',
        'Custom Black Vulkar names/merges are reverted. Entries use ORIGINAL NPC NAME + EQUIPPED WEAPONS.',
        'Same original name and equipped weapon combination form ONE NPC type within its faction.',
        'Original display labels are kept separate inside each faction.',
        'ALL level, HP, Force, feat and other native combat-stat differences become variants, with no size limit.',
        'Identical stat/feat profiles are appearance choices, not separate middle-menu entries.',
        'Same-count but different native feat sets remain separate variants.' if report['feat_policy'] == 'distinct' else 'Variant grouping follows displayed totals; differing feat sets remain recorded in the source map.',
        'Saber colours can share a type. Different equipped weapon combinations are separate types.',
        'Each type lists a shared appearance pool for ALL its variants, as requested.',
        'Originally on: shows which choices existed natively on each variant; sharing the whole pool is proposed.',
        'Carried/backup weapons are recorded but do not split NPC types.',
        'Other native attributes, skills, powers and item references are retained per source in NPC-SOURCE-MAP.json.',
        'Bouncer and the three Aqualish Thugs are temporarily omitted, not deleted.', '']
    for faction in FACTIONS:
        selected = [g for g in report['groups'] if g['faction'] == faction]
        if not selected:continue
        lines += [faction.upper(), '=' * len(faction), '']
        for group in selected:
            weapons = ' + '.join(' '.join(word[:1].upper() + word[1:] for word in name.split()) for name in group['equipped_weapons'])
            title = f"{group['foundation']} ({weapons})"
            lines += [f"{group['id']}. {title}",
                f"  Equipped: {weapons} | {len(group['variants'])} variant(s), {len(group['shared_appearances'])} appearance(s), {group['source_count']} source template(s)"]
            totals = Counter((v['level'], v['hp'], v['force'], v['feat_count']) for v in group['variants'])
            for variant in group['variants']:
                extra = ' | Different native stat/feat profile despite equal displayed totals' if totals[(variant['level'], variant['hp'], variant['force'], variant['feat_count'])] > 1 else ''
                lines.append(f"  {variant['id']}: Level {variant['level']}, HP {variant['hp']}, Force {variant['force']}, Feats {variant['feat_count']} | Originally on: {', '.join(variant['original_appearance_ids'])}{extra}")
                if extra:lines.append('      Feat IDs: ' + ' / '.join(', '.join(map(str, feats)) for feats in variant['feat_sets']))
                if extra:
                    peers = [v['sources'][0]['native_combat_fields'] for v in group['variants'] if (v['level'], v['hp'], v['force'], v['feat_count']) == (variant['level'], variant['hp'], variant['force'], variant['feat_count'])]
                    current = variant['sources'][0]['native_combat_fields']
                    different = [k for k in sorted(set().union(*(p.keys() for p in peers))) if k != 'FeatList' and len({json.dumps(p.get(k), sort_keys=True) for p in peers}) > 1]
                    if different:lines.append('      Distinguishing native stats: ' + '; '.join(k + '=' + json.dumps(current.get(k), sort_keys=True) for k in different))
                if len(variant['native_weapon_visuals']) > 1:lines.append('      Native weapon colours/models: ' + ' / '.join(variant['native_weapon_visuals']))
                if variant['other_native_stat_profiles'] > 1:lines.append(f"      Other native ability/skill/power profiles: {variant['other_native_stat_profiles']} (retained per source; not discarded).")
            lines.append('  Shared appearances (available to every variant in this proposed type):')
            for appearance in group['shared_appearances']:
                lines.append(f"    {appearance['id']}: {appearance['label']} [row {appearance['row']}] | Model: {appearance['body']}")
            if any(group['carried_loadouts']):
                lines.append('  Native carried weapons: ' + ' / '.join(', '.join(loadout) or 'None' for loadout in group['carried_loadouts']))
            lines.append('')
    return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--edited-roster', type=Path, required=True)
    parser.add_argument('--out', type=Path, default=ROOT / 'platforms/out/npc-naming-review')
    args = parser.parse_args()
    subprocess.run(['git', '-C', str(ROOT), 'check-ignore', '--quiet', str(args.out / 'NPC-NAMING-LIST.txt')], check=True)
    args.out.mkdir(parents=True, exist_ok=True)
    report = build(args.edited_roster.read_text(encoding='utf-8-sig'))
    (args.out / 'NPC-SOURCE-MAP.json').write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding='utf-8')
    (args.out / 'NPC-NAMING-LIST.txt').write_text(text_report(report), encoding='utf-8-sig')
    summary = [{faction: sum(g['faction'] == faction for g in report['groups'])} for faction in FACTIONS]
    print(json.dumps({k: report[k] for k in ('source_count', 'npc_type_count', 'stat_variant_count')}))
    print(json.dumps(summary))


if __name__ == '__main__':
    main()
