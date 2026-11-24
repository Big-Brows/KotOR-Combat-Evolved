"""Derive sandbox browsing metadata without changing native template data."""
import hashlib
import json
import struct

FACTIONS = {
    'Black Vulkar': ['Black Vulkar', 'Black Vulkar Captain', 'Black Vulkar Thug', 'Garage Head', 'Vulkar Enforcer', 'Vulkar Gang Boss', 'Vulkar Guard', 'Vulkar Lieutenant'],
    'Bounty Hunter': ['Bounty Hunter'],
    'Czerka': ['Czerka Guard', 'Czerka Thug', 'Guard'],
    'Dark Jedi': ['Dark Jedi', 'Dark Jedi Apprentice', 'Dark Jedi Master'],
    'Genoharadan': ['Genoharadan Hunter'],
    'Hidden Bek': ['Hidden Bek', 'Hidden Bek Captain', 'Hidden Bek Guard', 'Hidden Bek Lookout', 'Hidden Bek Thug'],
    'Jedi': ['Jedi', 'Jedi Knight'],
    'Mandalorian': ['Mandalorian', 'Mandalorian Captain', 'Mandalorian Commander', 'Mandalorian Elite', 'Mandalorian Warrior'],
    'Mercenary': ['Mercenary'],
    'Miscellaneous': ['Bouncer', 'Miner', 'Mining Team Captain'],
    'Wookiee': ['Wookiee', 'Wookiee Rebel', 'Wookiee Guard', 'Wookiee Warrior', 'Wookiee Elite Warrior', 'Wookiee Thug'],
    'Gamorrean': ['Gamorrean', 'Gamorrean Elite', 'Gamorrean Chief', 'Gamorrean Raider', 'Gamorrean Raid Leader', 'Gamorrean Patrol', 'Gamorrean Warrior'],
    'Rakata': ['Rakata', 'Rakatan Elite Warrior', 'Rakatan Warrior', 'Elder Elite Warrior', 'Elder Rakata', 'Elder Warrior'],
    'Republic': ['Captive Republic Soldier', 'Republic Soldier'],
    'Sand People (Tuskans)': ['Elite Warrior', 'Holy Warrior', 'Sand People Chieftain', 'Sand People Guard', 'Sand People Warrior'],
    'Sith': ['Sith Acolyte', 'Sith Advanced Trooper', 'Sith Apprentice', 'Sith Assault Trooper', 'Sith Captain', 'Sith Commander', 'Sith Diplomat', 'Sith Duelist', 'Sith Elite Trooper', 'Sith Elite Warrior', 'Sith Governor', 'Sith Grenadier', 'Sith Guard', 'Sith Heavy Trooper', 'Sith Lieutenant', 'Sith Lord', 'Sith Master', 'Sith Patrol Leader', 'Sith Security Officer', 'Sith Soldier', 'Sith Student', 'Sith Teacher', 'Sith Thug', 'Sith Thug Leader', 'Sith Trooper', 'Technician'],
    'Thugs': ['Aqualish Thug', 'Duros Warrior', 'Guard', 'Rodian Crony', 'Rodian Thug', 'Thug', "Twi'lek Thug", 'Lavin Thug'],
    'Trandoshans': ['Trandoshan'],
}


def saber_color(raw):
    """Read active blade-node diffuse textures, ignoring unused MDL strings."""
    textures = set()
    def visit(offset):
        node = 12 + offset
        flags = struct.unpack_from('<H', raw, node)[0]
        if flags & 0x0800:
            textures.add(raw[node + 168:node + 200].split(b'\0')[0].decode().lower())
        children, count = struct.unpack_from('<II', raw, node + 44)
        for i in range(count):
            visit(struct.unpack_from('<I', raw, 12 + children + i * 4)[0])
    visit(struct.unpack_from('<I', raw, 52)[0])
    colors = {'blue': 'Blue', 'red': 'Red', 'gren': 'Green', 'yelo': 'Yellow', 'purp': 'Purple'}
    found = {name for texture, name in colors.items() if 'w_lsabre' + texture + '01' in textures}
    assert len(found) == 1, ('Unknown active saber blade textures', textures)
    return found.pop()


def list_ids(raw, name):
    """Retain GFF list struct IDs (equipment slots), omitted by gffdump."""
    so, sc, fo, fc, lo, lc, _, _, _, _, lio, _ = struct.unpack_from('<12I', raw, 8)
    for i in range(fc):
        kind, label, off = struct.unpack_from('<III', raw, fo + i * 12)
        key = raw[lo + label * 16:lo + label * 16 + 16].rstrip(b'\0').decode()
        if kind == 15 and key == name:
            count = struct.unpack_from('<I', raw, lio + off)[0]
            indices = struct.unpack_from('<' + 'I' * count, raw, lio + off + 4)
            return [struct.unpack_from('<I', raw, so + j * 12)[0] for j in indices]
    return []


def canonical(value):
    if isinstance(value, dict):
        return {k: canonical(v) for k, v in sorted(value.items())}
    if isinstance(value, list):
        return [canonical(v) for v in value]
    return value


def combat_stats(native):
    cosmetic = set('TemplateResRef FirstName LastName Tag Description Comment PaletteID Appearance_Type Appearance_Head PortraitId BodyVariation TextureVar Gender Phenotype SoundSetFile Conversation FactionID BodyBag Deity Subrace PartyInteract'.split())
    return {k: v for k, v in native.items() if k not in cosmetic and not k.startswith('Script') and k not in ('Equip_ItemList', 'ItemList')}


def combat_item(item, slot, field):
    cosmetic = set('TemplateResRef Tag LocalizedName Description DescIdentified Comment PaletteID ModelVariation TextureVar BodyVariation'.split())
    gameplay = {k: v for k, v in item.items() if k not in cosmetic}
    if item.get('BaseItem') not in (8, 9, 10) or item.get('ModelVariation', 1) not in range(1, 6):
        if slot in (16, 32) or field == 'ItemList':
            for key in ('LocalizedName', 'ModelVariation'):
                if key in item:gameplay[key] = item[key]
    return gameplay


def metadata(rows, assets, decode, names, indicators=None):
    labels = {label: faction for faction, values in FACTIONS.items() for label in values}
    # Identity, presentation and campaign placement do not distinguish combat stats.
    signatures = {}
    for row in rows:
        raw = (assets / (row['template'] + '.utc')).read_bytes()
        native = decode(raw)
        stats = combat_stats(native)
        loadout, appearance, weapons = [], [], []
        for field, rrfield in [('Equip_ItemList', 'EquippedRes'), ('ItemList', 'InventoryRes')]:
            entries = native.get(field, [])
            ids = list_ids(raw, field)
            assert len(entries) == len(ids)
            for slot, entry in zip(ids, entries):
                rr = entry[rrfield]
                item = decode((assets / (rr + '.uti')).read_bytes())
                # Actual base item/properties/upgrades remain in the key. Only art and
                # descriptive fields are ignored, allowing saber colour alternatives.
                gameplay = combat_item(item, slot, field)
                loadout.append([field, slot, gameplay])
                if field == 'Equip_ItemList':
                    appearance.append([slot, rr])
                    if slot in (16, 32):
                        weapons.append(names(item))
        signature = json.dumps(canonical([stats, loadout]), sort_keys=True)
        key = hashlib.sha256(signature.encode()).hexdigest()[:16]
        if key in signatures:
            assert signatures[key] == signature
        signatures[key] = signature
        faction = labels.get(row['label'])
        if row['label'] == 'Guard':
            faction = 'Czerka' if 'czerka' in json.dumps(row['appearance_data']).lower() else 'Thugs'
        assert faction, ('Unmapped NPC', row['label'])
        row['menu'] = {'faction': faction, 'variant': key,
                       'type_order': FACTIONS[faction].index(row['label']),
                       'indicator': (indicators or {}).get(key, ' + '.join(weapons) or 'Native loadout'),
                       'appearance': row['appearance_data'].get('label', row['body']),
                       'appearance_key': hashlib.sha256(json.dumps([row['appearance'], native.get('BodyVariation'), native.get('TextureVar'), appearance]).encode()).hexdigest()[:16]}
    return rows
