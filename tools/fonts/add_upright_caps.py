#!/usr/bin/env python3
"""Add the upright capitals of units to the upright math faces (ui_mu*) of src/ui_fontdata.c, in place.

DeltaCE draws units upright (29.4 N, 2 kPa); the upright math faces had no capitals (they fell back
to italic). The capitals of unit symbols only (N, J, W, Pa, V, A, K, Hz, MeV, Btu, mmHg, Ohm, Sv,
Wb, T, F, C, Rankine; all 26 were 11 KB): the others stay italic. Only the new glyphs are rasterized (with mkfonts.py's own functions and settings): the
existing ones stay byte for byte as generated (FreeType versions rasterize slightly differently).
  python3 tools/fonts/add_upright_caps.py
"""
import os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mkfonts as M

ROOT = M.ROOT
P = os.path.join(ROOT, 'src', 'ui_fontdata.c')
CAPS = list('ABCFHJKMNOPRSTVW')
s = open(P).read()
newindex = None
for size in M.MATH_SIZES:
    name = 'ui_mu%d' % size
    bm = re.search(r'static const unsigned char %s_bits\[(\d+)\] = \{\n(.*?)\n\};' % name, s, re.S)
    gm = re.search(r'static const ui_glyph %s_glyphs\[(\d+)\] = \{\n(.*?)\n\};' % name, s, re.S)
    bits = [int(x) for x in re.findall(r'\d+', bm.group(2))]
    glyphs = [tuple(int(v) for v in g) for g in re.findall(r'\{(-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+)\}', gm.group(2))]
    assert len(bits) == int(bm.group(1)) and len(glyphs) == int(gm.group(1)), name
    if any(g[0] == ord('N') for g in glyphs):
        print(name, 'has capitals already'); continue
    f = M.font('STIXTwoText-VF.ttf', size, 400)
    for ch in CAPS:
        adv, ox, oy, w, h, pix = M.raster(f, ch)
        data = M.encode(pix)
        glyphs.append((ord(ch), len(bits), adv, w, h, ox, oy))
        bits += data
    glyphs.sort()
    assert len(bits) < 65536
    bl = ['  ' + ','.join(str(b) for b in bits[i:i + 24]) + ',' for i in range(0, len(bits), 24)]
    gl = ['{%d,%d,%d,%d,%d,%d,%d}' % g for g in glyphs]
    gl = ['  ' + ','.join(gl[i:i + 6]) + ',' for i in range(0, len(gl), 6)]
    s = s[:bm.start()] + 'static const unsigned char %s_bits[%d] = {\n%s\n};' % (name, len(bits), '\n'.join(bl)) + s[bm.end():]
    gm = re.search(r'static const ui_glyph %s_glyphs\[(\d+)\] = \{\n(.*?)\n\};' % name, s, re.S)
    s = s[:gm.start()] + 'static const ui_glyph %s_glyphs[%d] = {\n%s\n};' % (name, len(glyphs), '\n'.join(gl)) + s[gm.end():]
    s = re.sub(r'const ui_face %s = \{(\d+),(\d+),(\d+),(\d+),' % name, lambda m: 'const ui_face %s = {%s,%s,%s,%d,' % (name, m.group(1), m.group(2), m.group(3), len(glyphs)), s)
    idx = [255] * (0x80 + len(M.EXTRA))
    for i, g in enumerate(glyphs): idx[g[0]] = i
    assert newindex is None or newindex == idx, 'the sizes differ'
    newindex = idx
    print(name, 'glyphs', len(glyphs), 'bits', len(bits))
if newindex:  # the family's shared index (ui_mu34_index)
    im = re.search(r'static const unsigned char ui_mu34_index\[(\d+)\] = \{\n(.*?)\n\};', s, re.S)
    il = ['  ' + ','.join(str(b) for b in newindex[i:i + 24]) + ',' for i in range(0, len(newindex), 24)]
    s = s[:im.start()] + 'static const unsigned char ui_mu34_index[%d] = {\n%s\n};' % (len(newindex), '\n'.join(il)) + s[im.end():]
open(P, 'w').write(s)
