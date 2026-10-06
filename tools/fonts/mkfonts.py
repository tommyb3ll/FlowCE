#!/usr/bin/env python3
"""Rasterize the Focus UI fonts into 4-shade (2-bit) glyphs for the TI-84 Plus CE.

  python3 tools/fonts/mkfonts.py            (from the repo root, in WSL)
  -> src/ui_fontdata.c, src/ui_fontdata.h   (generated; do not edit)

Fonts (SIL OFL 1.1, tools/fonts/src): STIX Two Text (math, upright and italic), STIX Two Math
(symbols STIX Two Text lacks), Atkinson Hyperlegible (interface text).

Shades: coverage a in [0,1] -> 0 if a < 0.14, else a^0.8 -> 1 (< 0.42), 2 (< 0.8), 3.
The same mapping as the prototype (notes/ui/khicas-focus-prototype.html, function q4).

Glyph encoding (decoded by ui_font.c): the glyph's box, rows top to bottom, 2 bits per pixel,
4 pixels per byte, first pixel in the high bits; rows are not padded. (Run-length schemes were
measured: no smaller than this on anti-aliased glyphs, and slower to draw.)
Glyph codes are one byte: ASCII, or 0x80 + the index in EXTRA for the other characters.
"""
import os
import sys
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, 'tools', 'fonts', 'src')

MATH_SIZES = [34, 28, 24, 20, 17, 14, 12, 10]
UI_SIZES = [9, 10, 12]
ASCII = [chr(c) for c in range(32, 127)]
# characters outside ASCII, by glyph code 0x80 + index (ui_font.c maps code points to these)
EXTRA = ['π', '−', '·', 'θ', 'Σ', '±', '×', '∞',
         '→', '≤', '≥', '≠', '≈', '…']
# upright math: digits, lowercase (function names), punctuation, symbols; uppercase upright
# falls back to the italic face. Symbols STIX Two Text lacks come from STIX Two Math.
MATH_UP = [c for c in ASCII if not c.isupper() and c not in '@#$%&`' + chr(92)] + EXTRA[:13]
ITALIC = [chr(c) for c in range(ord('A'), ord('Z') + 1)] + [chr(c) for c in range(ord('a'), ord('z') + 1)] + ['θ']
UI_CHARS = ASCII + ['…', '·', '≈', '−']


def font(path, size, wght=None):
    f = ImageFont.truetype(os.path.join(SRC, path), size)
    if wght is not None:
        try:
            f.set_variation_by_axes([wght])
        except Exception as e:  # static font
            print('  no variation axes for', path, e, file=sys.stderr)
    return f


def has_glyph(path, ch, _cache={}):
    from fontTools.ttLib import TTFont
    if path not in _cache:
        _cache[path] = TTFont(os.path.join(SRC, path)).getBestCmap()
    return ord(ch) in _cache[path]


def shade(v):
    a = v / 255.0
    if a < 0.14:
        return 0
    a = a ** 0.8
    return 1 if a < 0.42 else (2 if a < 0.8 else 3)


def raster(f, ch):
    """(adv, ox, oy, w, h, pixels) with oy = top of the box relative to the baseline (<0 above)."""
    adv = int(round(f.getlength(ch)))
    size = f.size
    W, H = size * 3, size * 3
    base = size * 2
    im = Image.new('L', (W, H), 0)
    ImageDraw.Draw(im).text((size, base), ch, font=f, fill=255, anchor='ls')
    px = im.load()
    pts = [(x, y) for y in range(H) for x in range(W) if shade(px[x, y])]
    if not pts:
        return adv, 0, 0, 0, 0, []
    x0 = min(p[0] for p in pts); x1 = max(p[0] for p in pts) + 1
    y0 = min(p[1] for p in pts); y1 = max(p[1] for p in pts) + 1
    pix = [shade(px[x, y]) for y in range(y0, y1) for x in range(x0, x1)]
    return adv, x0 - size, y0 - base, x1 - x0, y1 - y0, pix


def code_of(ch):
    return ord(ch) if ord(ch) < 128 else 0x80 + EXTRA.index(ch)


def encode(pix):
    out = []
    for i in range(0, len(pix), 4):
        q = pix[i:i + 4] + [0] * (4 - len(pix[i:i + 4]))
        out.append((q[0] << 6) | (q[1] << 4) | (q[2] << 2) | q[3])
    return out


def encode_rle(pix):  # kept for measurements
    out = []
    i, n = 0, len(pix)
    while i < n:
        v = pix[i]
        if v in (0, 3):
            j = i
            while j < n and pix[j] == v and j - i < 64:
                j += 1
            if j - i >= 3:  # a run beats a literal triple only from 3 pixels on
                out.append((0x00 if v == 0 else 0x40) | (j - i - 1))
                i = j
                continue
        t = pix[i:i + 3] + [0] * (3 - len(pix[i:i + 3]))
        out.append(0x80 | (t[0] << 5) | (t[1] << 3) | (t[2] << 1))
        i += 3
    return out


def decode(data, count):  # reference decoder, mirrors ui_font.c
    pix = []
    for b in data:
        pix += [(b >> 6) & 3, (b >> 4) & 3, (b >> 2) & 3, b & 3]
    return pix[:count]


class Face:
    def __init__(self, name, size, asc, desc, glyphs):
        self.name, self.size, self.asc, self.desc, self.glyphs = name, size, asc, desc, glyphs


def build_face(name, size, chars, sources, asc, desc):
    glyphs = []
    for ch in chars:
        for path, wght in sources:
            if has_glyph(path, ch):
                f = font(path, size, wght)
                adv, ox, oy, w, h, pix = raster(f, ch)
                data = encode(pix)
                assert decode(data, w * h) == pix, (name, ch)
                glyphs.append((code_of(ch), adv, ox, oy, w, h, data))
                break
        else:
            print('  missing glyph', hex(ord(ch)), 'in', name, file=sys.stderr)
    glyphs.sort()
    return Face(name, size, asc, desc, glyphs)


def main():
    faces = []
    up = [('STIXTwoText-VF.ttf', 400), ('STIXTwoMath.ttf', None)]
    it = [('STIXTwoText-Italic-VF.ttf', 400)]
    for s in MATH_SIZES:
        asc, desc = round(s * 0.70), round(s * 0.24)
        faces.append(build_face('ui_mu%d' % s, s, MATH_UP, up, asc, desc))
        faces.append(build_face('ui_mi%d' % s, s, ITALIC, it, asc, desc))
    for s in UI_SIZES:
        asc, desc = round(s * 0.95), round(s * 0.29)
        faces.append(build_face('ui_tr%d' % s, s, UI_CHARS, [('AtkinsonHyperlegible-Regular.ttf', None)], asc, desc))
        faces.append(build_face('ui_tb%d' % s, s, UI_CHARS, [('AtkinsonHyperlegible-Bold.ttf', None)], asc, desc))

    total = 0
    c = ['// Generated by tools/fonts/mkfonts.py from SIL OFL fonts (STIX Two, Atkinson Hyperlegible).',
         '// Do not edit. See tools/fonts/src for the fonts and their licenses.',
         '#include "ui_font.h"', '']
    h = ['// Generated by tools/fonts/mkfonts.py. Do not edit.', '#ifndef UI_FONTDATA_H', '#define UI_FONTDATA_H',
         '#include "ui_font.h"', '#ifdef __cplusplus', 'extern "C" {', '#endif']
    for f in faces:
        blob, gl, off = [], [], 0
        for (cp, adv, ox, oy, w, hh, data) in f.glyphs:
            gl.append('{%d,%d,%d,%d,%d,%d,%d}' % (cp, off, adv, w, hh, ox, oy))
            blob += data
            off += len(data)
        assert off < 65536
        total += len(blob) + 8 * len(gl) + 12
        c.append('static const unsigned char %s_bits[%d] = {' % (f.name, max(1, len(blob))))
        for i in range(0, len(blob), 24):
            c.append('  ' + ','.join(str(b) for b in blob[i:i + 24]) + ',')
        if not blob:
            c.append('  0')
        c.append('};')
        c.append('static const ui_glyph %s_glyphs[%d] = {' % (f.name, len(gl)))
        for i in range(0, len(gl), 6):
            c.append('  ' + ','.join(gl[i:i + 6]) + ',')
        c.append('};')
        # code -> position in the glyph table (255: missing): O(1) lookups on the calculator
        index = [255] * (0x80 + len(EXTRA))
        for i, g in enumerate(f.glyphs):
            index[g[0]] = i
        total += len(index)
        c.append('static const unsigned char %s_index[%d] = {' % (f.name, len(index)))
        for i in range(0, len(index), 24):
            c.append('  ' + ','.join(str(b) for b in index[i:i + 24]) + ',')
        c.append('};')
        c.append('const ui_face %s = {%d,%d,%d,%d,%s_glyphs,%s_bits,%s_index};' % (f.name, f.size, f.asc, f.desc, len(gl), f.name, f.name, f.name))
        c.append('')
        h.append('extern const ui_face %s;' % f.name)
    h += ['#ifdef __cplusplus', '}', '#endif', '#endif', '']
    with open(os.path.join(ROOT, 'src', 'ui_fontdata.c'), 'w') as fp:
        fp.write('\n'.join(c) + '\n')
    with open(os.path.join(ROOT, 'src', 'ui_fontdata.h'), 'w') as fp:
        fp.write('\n'.join(h))
    print('faces', len(faces), 'estimated flash bytes', total)
    for f in faces:
        print('  %-9s glyphs %3d  bits %5d' % (f.name, len(f.glyphs), sum(len(g[6]) for g in f.glyphs)))


if __name__ == '__main__':
    main()
