#!/usr/bin/env python3
"""theme_tints.py <ui_gfx.c> [out.h | --pick Name=level,...]: stronger versions of FlowCE's themes.
  <ui_gfx.c>          the report: each theme's background at levels 0-3, contrast, distances
  out.h               the theme test build's tables, T_tint[3][9] (levels 1-3)
  --pick Sand=3,...   rewrites those themes' tables in <ui_gfx.c> at the levels given
Run it on the themes as 2.1 had them (git show v2.1:src/ui_gfx.c): 2.2 has the levels picked
on the calculator (Sand, Mint, Rose, Arctic 3; Ember, Grape, Forest 2) in place. The calculator's LCD shows 32 levels per channel and squashes shades near white
and black, so themes whose backgrounds are 1-3 levels apart look alike there.

Per theme and level, in OKLab:
- surfaces (background, cards, lines, tiles, bars, shadow) move together by the change that takes
  the background to its target (hue h, chroma C; dark themes also lighter: L); cards and bars of
  the light themes by half of it, so they stay near white;
- accents (accent, its soft tile, the graph colors, the status bar's accent text) get more chroma;
- text colors are kept as they are.
Paper and Night are the references: the same at every level."""
import re, sys, math

ORDER = ['ui_theme_paper', 'ui_theme_night', 'T_sand', 'T_mint', 'T_arctic', 'T_ember', 'T_rose',
         'T_grape', 'T_forest']
NAMES = ['Paper', 'Night', 'Sand', 'Mint', 'Arctic', 'Ember', 'Rose', 'Grape', 'Forest']
SLOTS = ['BG', 'INK', 'SUB', 'LINE', 'ACC', 'ACCSOFT', 'ONACC', 'CARD', 'BAR', 'BARINK', 'GREEN',
         'SHADOW', 'WHITE', 'RED', 'PURPLE', 'ORANGE', 'BLUE', 'DIM', 'HEAD', 'HEADINK', 'HEADSUB',
         'BARLINE', 'TILE', 'DIMBAR']
SURF = {'BG', 'LINE', 'CARD', 'BAR', 'SHADOW', 'DIM', 'HEAD', 'BARLINE', 'TILE', 'DIMBAR'}
HALF = {'CARD', 'BAR'}  # light themes: half the move
ACCENT = {'ACC', 'ACCSOFT', 'GREEN', 'RED', 'PURPLE', 'ORANGE', 'BLUE', 'HEADINK'}

# background targets per level 1..3: (L or None = keep, C, hue in degrees); accent chroma factor
TARGET = {
    'Sand':   dict(h=88,  C=(0.040, 0.055, 0.070), L=(0.955, 0.935, 0.915), acc=(1.04, 1.08, 1.12)),
    'Mint':   dict(h=166, C=(0.030, 0.043, 0.056), L=(0.960, 0.945, 0.930), acc=(1.04, 1.08, 1.12)),
    'Rose':   dict(h=12,  C=(0.025, 0.037, 0.050), L=(0.950, 0.930, 0.910), acc=(1.04, 1.08, 1.12)),
    'Arctic': dict(h=245, C=(0.040, 0.054, 0.068), L=(0.350, 0.375, 0.400), acc=(1.10, 1.20, 1.30)),
    'Ember':  dict(h=52,  C=(0.022, 0.036, 0.050), L=(0.320, 0.340, 0.360), acc=(1.06, 1.12, 1.18)),
    'Grape':  dict(h=315, C=(0.040, 0.055, 0.070), L=(0.320, 0.340, 0.360), acc=(1.08, 1.16, 1.24)),
    'Forest': dict(h=155, C=(0.032, 0.045, 0.058), L=(0.250, 0.280, 0.310), acc=(1.12, 1.24, 1.36)),
}

def lin(c):
    c /= 255
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
def unlin(c):
    c = 12.92 * c if c <= 0.0031308 else 1.055 * c ** (1 / 2.4) - 0.055
    return c * 255
def to_lab(rgb):
    r, g, b = (lin(v) for v in rgb)
    l = 0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b
    m = 0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b
    s = 0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b
    l, m, s = (math.copysign(abs(v) ** (1 / 3), v) for v in (l, m, s))
    return (0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
            1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
            0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s)
def from_lab_raw(L, a, b):
    l = (L + 0.3963377774 * a + 0.2158037573 * b) ** 3
    m = (L - 0.1055613458 * a - 0.0638541728 * b) ** 3
    s = (L - 0.0894841775 * a - 1.2914855480 * b) ** 3
    return (4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s,
            -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
            -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s)
def from_lab(L, a, b):  # in gamut: less chroma, same L and hue
    k = 1.0
    for _ in range(40):
        rgb = from_lab_raw(L, a * k, b * k)
        if all(-1e-6 <= v <= 1 + 1e-6 for v in rgb): break
        k *= 0.97
    return tuple(max(0, min(255, round(unlin(min(max(v, 0), 1))))) for v in rgb)
def lch(lab):
    L, a, b = lab
    return L, math.hypot(a, b), math.degrees(math.atan2(b, a)) % 360
def lum(rgb):
    r, g, b = (lin(v) for v in rgb)
    return 0.2126 * r + 0.7152 * g + 0.0722 * b
def contrast(x, y):
    a, b = sorted((lum(x), lum(y)))
    return (b + 0.05) / (a + 0.05)
def q5(rgb):  # what the LCD gets (rgb565() keeps 5 bits per channel)
    return tuple(v >> 3 for v in rgb)

src = open(sys.argv[1]).read()
themes = {}
for name in ORDER:
    m = re.search(r'ui_theme\s+' + name + r'\s*=\s*\{\{(.*?)\}\s*,\s*\{([^}]*)\}\s*,\s*(\d+)\s*\}\s*;', src, re.S)
    trip = [tuple(int(v) for v in t.split(',')) for t in re.findall(r'\{\s*(\d+\s*,\s*\d+\s*,\s*\d+)\s*\}', m.group(1))]
    assert len(trip) == 24, (name, len(trip))
    themes[name] = (trip, tuple(int(v) for v in m.group(2).split(',')), int(m.group(3)))

def variant(k, lv):  # theme k at level lv (0: as it is)
    trip, dim_to, pct = themes[ORDER[k]]
    t = TARGET.get(NAMES[k])
    if not lv or not t: return list(trip), dim_to, pct
    bg = to_lab(trip[0])
    light = bg[0] > 0.6
    L = t['L'][lv - 1]
    C, h = t['C'][lv - 1], math.radians(t['h'])
    d = (L - bg[0], C * math.cos(h) - bg[1], C * math.sin(h) - bg[2])
    out = []
    for s, rgb in zip(SLOTS, trip):
        if s in SURF:
            w = 0.5 if (light and s in HALF) else 1.0
            lab = to_lab(rgb)
            out.append(from_lab(lab[0] + w * d[0], lab[1] + w * d[1], lab[2] + w * d[2]))
        elif s in ACCENT:
            f = t['acc'][lv - 1]
            L0, a0, b0 = to_lab(rgb)
            out.append(from_lab(L0 + (d[0] if s == 'ACCSOFT' else 0), a0 * f, b0 * f))
        else: out.append(rgb)
    # text: as much contrast as at level 0 against the surfaces it sits on (only its lightness moves)
    ix = {n: i for i, n in enumerate(SLOTS)}
    for s, refs in (('INK', ('BG', 'CARD', 'TILE')), ('SUB', ('BG', 'CARD', 'TILE')), ('BARINK', ('BAR',)),
                    ('HEADSUB', ('HEAD',))):
        want = min(contrast(trip[ix[s]], trip[ix[r]]) for r in refs)
        L0, a0, b0 = to_lab(trip[ix[s]])
        step = -0.002 if light else 0.002
        L1, rgb = L0, trip[ix[s]]
        while min(contrast(rgb, out[ix[r]]) for r in refs) < want and 0 < L1 + step < 1:
            L1 += step
            rgb = from_lab(L1, a0, b0)
        out[ix[s]] = rgb
    return out, dim_to, pct

def dist5(x, y): return max(abs(p - q) for p, q in zip(q5(x), q5(y)))
for lv in range(4):
    print(f'== level {lv}')
    rows = [variant(k, lv)[0] for k in range(9)]
    for k in range(9):
        r = rows[k]
        L, C, h = lch(to_lab(r[0]))
        print(f'  {NAMES[k]:7} bg {str(r[0]):16} 5-bit {str(q5(r[0])):13} L {L:.3f} C {C:.3f} h {h:5.1f}'
              f' | ink {contrast(r[1], r[0]):5.2f} sub {contrast(r[2], r[0]):5.2f} sub/card {contrast(r[2], r[7]):5.2f}'
              f' bar {contrast(r[9], r[8]):5.2f} acc {str(r[4])}')
    for grp in ((0, 2, 3, 6), (1, 4, 5, 7, 8)):
        worst = min(((dist5(rows[i][0], rows[j][0]), NAMES[i], NAMES[j]) for i in grp for j in grp if i < j))
        print(f'  closest backgrounds ({"light" if grp[0] == 0 else "dark"}): {worst[0]} levels apart ({worst[1]}, {worst[2]})')

if len(sys.argv) > 3 and sys.argv[2] == '--pick':
    pick = dict((n, int(v)) for n, v in (a.split('=') for a in sys.argv[3].split(',')))
    for name, lv in pick.items():
        k = NAMES.index(name)
        trip, dim_to, pct = variant(k, lv)
        rows = ['  ' + ', '.join('{%d, %d, %d}' % c for c in trip[i:i + 6]) for i in range(0, 24, 6)]
        table = ('static const ui_theme %s = {{\n' % ORDER[k] + ',\n'.join(rows)
                 + '},\n  {%d, %d, %d}, %d};' % (*dim_to, pct))
        src, n = re.subn(r'static const ui_theme ' + ORDER[k] + r' = \{\{.*?\};', lambda m: table, src, count=1, flags=re.S)
        assert n == 1, name
    open(sys.argv[1], 'w').write(src)
    print('rewrote', ', '.join('%s %d' % kv for kv in pick.items()), 'in', sys.argv[1])
elif len(sys.argv) > 2:
    with open(sys.argv[2], 'w') as f:
        f.write('// generated by theme_tints.py (the theme test build): the themes at levels 1-3, in\n'
                '// ui_themes order; Paper and Night as they are\n'
                'static const ui_theme T_tint[3][UI_NTHEMES] = {\n')
        for lv in (1, 2, 3):
            f.write('  {\n')
            for k in range(9):
                trip, dim_to, pct = variant(k, lv)
                body = ', '.join('{%d, %d, %d}' % c for c in trip)
                f.write('    {{%s}, {%d, %d, %d}, %d}, // %s\n' % (body, *dim_to, pct, NAMES[k]))
            f.write('  },\n')
        f.write('};\n')
    print('wrote', sys.argv[2])
