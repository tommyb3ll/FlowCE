#!/usr/bin/env python3
"""Add μ, σ, λ, ≤, ≥, ≠ and π to the interface text faces (ui_tr*, ui_tb*) of src/ui_fontdata.c, in place, and
lengthen every face's index for the three new codes (the math faces: 255, none).

The probability screen (focus_stats.cc) labels its fields μ, σ, λ; Atkinson Hyperlegible has no
Greek, so they come from STIX Two Text (regular 400, bold 700), as θ did (add_text_theta.py). Only
the new glyphs are rasterized (with mkfonts.py's own functions and settings).
  python3 tools/fonts/add_text_greek.py
"""
import os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mkfonts as M

P = os.path.join(M.ROOT, 'src', 'ui_fontdata.c')
NEW = ['μ', 'σ', 'λ', '≤', '≥', '≠', 'π']
N = 0x80 + len(M.EXTRA)
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
        for ch in NEW:
            code = 0x80 + M.EXTRA.index(ch)
            if any(g[0] == code for g in glyphs): continue
            # (STIX Two Text has no ≤ ≥ ≠: its .notdef box would come out; they come from STIX Two Math)
            f = M.font('STIXTwoText-VF.ttf', size, wght) if ch in 'μσλπ' else M.font('STIXTwoMath.ttf', size)
            adv, ox, oy, w, h, pix = M.raster(f, ch)
            glyphs.append((code, len(bits), adv, w, h, ox, oy))
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
        idx = [255] * N
        for i, g in enumerate(glyphs): idx[g[0]] = i
        assert newindex is None or newindex == idx, 'the faces differ'
        newindex = idx
        print(name, 'glyphs', len(glyphs), 'bits', len(bits))
# every index N long (the math faces' already were: 255 kept for the new codes): the text faces' (ui_tr9_index) from their glyphs, the others 255 for the new codes
for im in list(re.finditer(r'static const unsigned char (ui_\w+_index)\[(\d+)\] = \{\n(.*?)\n\};', s, re.S))[::-1]:
    nm, vals = im.group(1), [int(x) for x in re.findall(r'\d+', im.group(3))]
    vals = newindex if nm == 'ui_tr9_index' else (vals + [255] * N)[:N]
    il = ['  ' + ','.join(str(b) for b in vals[i:i + 24]) + ',' for i in range(0, len(vals), 24)]
    s = s[:im.start()] + 'static const unsigned char %s[%d] = {\n%s\n};' % (nm, len(vals), '\n'.join(il)) + s[im.end():]
    print(nm, len(vals))
open(P, 'w').write(s)
