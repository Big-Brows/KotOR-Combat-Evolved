# Modified for KotOR:CE on 2026-11-16; see docs/LOCAL_CHANGES.md.
"""Remove custom player display-name bytes from a release seed, including history.

The same-length replacement preserves archive/GFF offsets. No original file is
written. Reject collisions with resource identifiers or compiled scripts rather
than changing those. This does not establish redistribution rights for game data.
"""
import struct


def archive_items(data):
    if data[:4] not in (b'MOD ', b'ERF ', b'SAV '):
        raise ValueError('Expected an ERF/MOD/SAV seed archive')
    count, keys, table = (struct.unpack_from('<I', data, offset)[0] for offset in (16, 24, 28))
    if count > 100000 or keys + count * 24 > len(data) or table + count * 8 > len(data):
        raise ValueError('Invalid seed archive bounds')
    result = []
    for i in range(count):
        name, identity, kind, padding = struct.unpack_from('<16sIHH', data, keys + 24 * i)
        offset, size = struct.unpack_from('<II', data, table + 8 * i)
        if offset + size > len(data):
            raise ValueError('Invalid seed resource bounds')
        result.append((name.rstrip(b'\0'), kind, data[offset:offset + size]))
    return result


def player_names(module_info):
    data = module_info
    if data[4:8] != b'V3.2':
        raise ValueError('Expected native GFF V3.2')
    so, sc, fo, fc, lo, lc, fdo, fdc, fio, fic, lio, lic = struct.unpack_from('<12I', data, 8)
    labels = [data[lo + 16 * i:lo + 16 * i + 16].rstrip(b'\0') for i in range(lc)]
    def fields(index):
        if index >= sc:
            raise ValueError('Invalid GFF struct')
        identity, location, count = struct.unpack_from('<III', data, so + 12 * index)
        indices = [location] if count == 1 else list(struct.unpack_from('<%dI' % count, data, fio + location)) if count else []
        result = []
        for i in indices:
            if i >= fc:
                raise ValueError('Invalid GFF field')
            kind, label, value = struct.unpack_from('<III', data, fo + 12 * i)
            result.append((labels[label], kind, value))
        return result
    names = []
    for label, kind, value in fields(0):
        if label != b'Mod_PlayerList' or kind != 15:
            continue
        count = struct.unpack_from('<I', data, lio + value)[0]
        for i in range(count):
            index = struct.unpack_from('<I', data, lio + value + 4 + 4 * i)[0]
            for label, kind, value in fields(index):
                if label not in (b'FirstName', b'LastName') or kind != 12:
                    continue
                total, reference, count = struct.unpack_from('<IiI', data, fdo + value)
                if reference >= 0:
                    continue
                position = fdo + value + 12
                for j in range(count):
                    language, size = struct.unpack_from('<II', data, position)
                    name = data[position + 8:position + 8 + size]
                    position += 8 + size
                    if name.strip():
                        names.append(name)
    return names


def anonymize_seed(files):
    """Return sanitized seed bytes and counts only; never return names in reports."""
    modules = []
    protected = []
    for name, kind, raw in archive_items(files['SAVEGAME.sav']):
        protected.append((name, kind, raw))
        if raw[:4] in (b'MOD ', b'ERF ', b'SAV '):
            modules.extend(archive_items(raw))
    names = set()
    for name, kind, raw in modules:
        protected.append((name, kind, raw))
        if name.lower() == b'module' and kind == 2014:
            names.update(player_names(raw))
    if not names:
        raise ValueError('No custom seed-player name located; review this seed version')
    sanitized = dict(files)
    replacements = 0
    changed = []
    for name in names:
        if len(name) < 4 or len(name) > 128:
            raise ValueError('Unsupported display-name length; manual seed review required')
        for identifier, kind, raw in protected:
            if name.lower() in identifier.lower() or (kind == 2010 and name in raw):
                raise ValueError('Display-name collision with protected resource/script data')
        replacement = b'ArenaPlayer'[:len(name)].ljust(len(name), b' ')
        if replacement == name:
            continue
        for file, raw in list(sanitized.items()):
            count = raw.count(name)
            if count:
                sanitized[file] = raw.replace(name, replacement)
                replacements += count
                if file not in changed:
                    changed.append(file)
        if any(name in raw for raw in sanitized.values()):
            raise ValueError('A custom display name remains in seed bytes')
    if any(len(sanitized[file]) != len(raw) for file, raw in files.items()):
        raise ValueError('Seed scrub unexpectedly changed a file length')
    return sanitized, dict(custom_names=len(names),replaced_occurrences=replacements,changed_files=sorted(changed),file_lengths_preserved=True)
