#!/usr/bin/env python3
"""Add θ to the interface text faces (ui_tr*, ui_tb*) of src/ui_fontdata.c, in place.

The polar graph says θ in its readout, prompts and menus; Atkinson Hyperlegible has no Greek, so θ
comes from STIX Two Text (regular 400, bold 700). Only the new glyph is rasterized (with
mkfonts.py's own functions and settings): the existing ones stay byte for byte as generated.
  python3 tools/fonts/add_text_theta.py
"""
import os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mkfonts as M

ROOT = M.ROOT
P = os.path.join(ROOT, 'src', 'ui_fontdata.c')
CODE = 0x80 + M.EXTRA.index('θ')
s = open(P).read()
newindex = None
for size in M.UI_SIZES:
    for kind, wght in (('tr', 400), ('tb', 700)):
        name = 'ui_%s%d' % (kind, size)
        bm = re.search(r'static const unsigned char %s_bits\[(\d+)\] = \{\n(.*?)\n\};' % name, s, re.S)
        gm = re.search(r'static const ui_glyph %s_glyphs\[(\d+)\] = \{\n(.*?)\n\};' % name, s, re.S)
        bits = [int(x) for x in re.findall(r'\d+', bm.group(2))]
        glyphs = [tuple(int(v) for v in g) for g in re.findall(r'\{(-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+)\}', gm.group(2))]
        assert len(bits) == int(bm.group(1)) and len(glyphs) == int(gm.group(1)), name
        if any(g[0] == CODE for g in glyphs):
            print(name, 'has θ already'); continue
        adv, ox, oy, w, h, pix = M.raster(M.font('STIXTwoText-VF.ttf', size, wght), 'θ')
        glyphs.append((CODE, len(bits), adv, w, h, ox, oy))
        bits += M.encode(pix)
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
        assert newindex is None or newindex == idx, 'the faces differ'
        newindex = idx
        print(name, 'glyphs', len(glyphs), 'bits', len(bits))
if newindex:  # the text faces' shared index (ui_tr9_index)
    im = re.search(r'static const unsigned char ui_tr9_index\[(\d+)\] = \{\n(.*?)\n\};', s, re.S)
    il = ['  ' + ','.join(str(b) for b in newindex[i:i + 24]) + ',' for i in range(0, len(newindex), 24)]
    s = s[:im.start()] + 'static const unsigned char ui_tr9_index[%d] = {\n%s\n};' % (len(newindex), '\n'.join(il)) + s[im.end():]
open(P, 'w').write(s)
