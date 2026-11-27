#!/usr/bin/env python3
"""The web page's customizer (index.html, installer.js): writes <files dir>/packs.json from the FlowCE
bundles there, as tools/dev/packs_build.sh names them: FlowCE.b84 (statistics and differential
equations, 2.4's set: the default), FlowCE-<packs>.b84 (FlowCE-vectors-stats.b84), FlowCE-core.b84
(none). Each with its packs' code (series, vectors, stats, diffeq: 1 in, 0 out, the makefile's
order) and its app's size, the AppIns' data. usage: packs.py <files dir>"""
import json
import pathlib
import re
import sys
import zipfile

PACKS = ['series', 'vectors', 'stats', 'diffeq']
DEFAULT = '0011'


def code_of(name):
    if name == 'FlowCE.b84':
        return DEFAULT
    m = re.fullmatch(r'FlowCE-([a-z-]+)\.b84', name)
    if not m:
        return None
    parts = m.group(1).split('-')
    if parts == ['core']:
        return '0000'
    if any(p not in PACKS for p in parts) or len(set(parts)) != len(parts):
        return None
    return ''.join('1' if p in parts else '0' for p in PACKS)


def app_size(path):
    """the app's bytes: each AppIns appvar's content length (TI file: 55-byte header, the entry's
    header, its data: a 2-byte length, then the content)"""
    n = 0
    with zipfile.ZipFile(path) as z:
        for e in z.infolist():
            if not re.fullmatch(r'AppIns\d+\.8xv', e.filename, re.I):
                continue
            d = z.read(e.filename)
            if d[:8] != b'**TI83F*':
                raise ValueError(f'{path}: {e.filename} is not a TI file')
            hlen = int.from_bytes(d[55:57], 'little')
            off = 55 + 2 + hlen + 2
            n += int.from_bytes(d[off:off + 2], 'little')
    if n == 0:
        raise ValueError(f'{path}: no AppIns')
    return n - 3  # (the pieces carry the app and 3 bytes more: tools/dev/wsl_build_v15.sh)


def main(files):
    files = pathlib.Path(files)
    builds = {}
    for f in sorted(files.glob('FlowCE*.b84')):
        c = code_of(f.name)
        if c is None:
            continue
        if c in builds:
            raise ValueError(f'{f.name}: the same packs as {builds[c]["file"]}')
        builds[c] = {'file': f.name, 'size': app_size(f)}
    if DEFAULT not in builds:
        raise ValueError('no FlowCE.b84')
    out = {'packs': PACKS, 'default': DEFAULT, 'max': 2818045, 'builds': builds}
    (files / 'packs.json').write_text(json.dumps(out, indent=1) + '\n')
    print('packs.json:', ', '.join(f'{c} {b["file"]} {b["size"]}' for c, b in sorted(builds.items())))


if __name__ == '__main__':
    main(sys.argv[1])
