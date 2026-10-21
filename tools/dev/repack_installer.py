#!/usr/bin/env python3
"""Replace only INST in a TI bundle, preserving every other file's bytes and ZIP order.

Usage: repack_installer.py <source.b84> <output.b84> [INST.8xp]
Defaults to the reviewed app_tools/INST.8xp. Does not rebuild the calculator app.
"""
from pathlib import Path
import sys
import zipfile


def repack(source, target, installer):
    source, target, installer = map(Path, (source, target, installer))
    if source.resolve() == target.resolve():
        raise ValueError('Use a separate output file; keep the original bundle')
    data = installer.read_bytes()
    if not data.startswith(b'**TI83F*') or data[60:68] != b'INST\0\0\0\0':
        raise ValueError('Expected a TI program named INST')
    if int.from_bytes(data[-2:], 'little') != sum(data[55:-2]) & 65535:
        raise ValueError('Invalid installer checksum')
    with zipfile.ZipFile(source) as original:
        matches = [n for n in original.namelist() if n.split('/')[-1].upper() == 'INST.8XP']
        if len(matches) != 1 or len(set(original.namelist())) != len(original.namelist()):
            raise ValueError('Expected exactly one installer and no duplicate ZIP entries')
        with zipfile.ZipFile(target, 'w') as result:
            result.comment = original.comment
            for entry in original.infolist():
                result.writestr(entry, data if entry.filename == matches[0] else original.read(entry.filename))
    with zipfile.ZipFile(source) as original, zipfile.ZipFile(target) as result:
        if original.namelist() != result.namelist():
            raise ValueError('Bundle entry order changed')
        for name in original.namelist():
            expected = data if name == matches[0] else original.read(name)
            if result.read(name) != expected:
                raise ValueError('Unexpected change in ' + name)
    print('Updated only INST:', target)


if __name__ == '__main__':
    source, target = sys.argv[1:3]
    installer = sys.argv[3] if len(sys.argv) > 3 else Path(__file__).resolve().parents[2] / 'app_tools/INST.8xp'
    repack(source, target, installer)
