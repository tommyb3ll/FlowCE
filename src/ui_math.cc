// ui_math.cc - math layouts drawn with the Focus fonts (STIX Two, 4 shades) and primitives.
// Text conventions: variables italic, numbers and function names upright, - drawn as the minus
// sign, <= >= != -> as single symbols; pi, infinity, sigma, arrows from the symbol literals.
#include "ui_math.h"
extern "C" volatile unsigned char focus_phase; // focus.cc: timing probe (tools/emu/phases.py)
#include "ui_gfx.h"
#include "ui_font.h"
#include "ui_fontdata.h"
#include <stdlib.h>
#include <string.h>

const unsigned char ui_math_sizes[UI_NSIZES] = {34, 28, 24, 20, 17, 14, 12, 10};
static const ui_face * const MU[UI_NSIZES] = {&ui_mu34, &ui_mu28, &ui_mu24, &ui_mu20, &ui_mu17, &ui_mu14, &ui_mu12, &ui_mu10};
static const ui_face * const MI[UI_NSIZES] = {&ui_mi34, &ui_mi28, &ui_mi24, &ui_mi20, &ui_mi17, &ui_mi14, &ui_mi12, &ui_mi10};

static int script(int lv) { return lv + 2 < UI_NSIZES ? lv + 2 : UI_NSIZES - 1; }
static int rnd(int a, int pct) { return (a * pct + 50) / 100; }

// glyph of cp at level lv, italic or upright, falling back to the other face (uppercase
// letters exist only in the italic face, digits only in the upright one)
static const ui_glyph * glyph(int lv, int it, unsigned cp, const ui_face ** f) {
  const ui_face * a = it ? MI[lv] : MU[lv], * b = it ? MU[lv] : MI[lv];
  const ui_glyph * g = ui_glyph_of(a, cp);
  if (g) { *f = a; return g; }
  *f = b;
  return ui_glyph_of(b, cp);
}

// next code point of buffer text s[i,n) in style st, with the math substitutions
static unsigned next_cp(const char * s, int n, int & i) {
  unsigned char c = (unsigned char)s[i];
  if (i + 1 < n) {
    char d = s[i + 1];
    if (d == '=' && (c == '<' || c == '>' || c == '!')) { i += 2; return c == '<' ? 0x2264 : c == '>' ? 0x2265 : 0x2260; }
    if (c == '-' && d == '>') { i += 2; return 0x2192; }
  }
  if (c == '-') { ++i; return 0x2212; }
  const char * p = s + i;
  unsigned cp = ui_utf8(&p, s + n);
  if (p == s + i) ++p; // never stall
  i = (int)(p - s);
  return cp;
}

// symbol literals: their glyphs (at most 3) and style
static int sym(const char * s, int n, unsigned * cp, int & it) {
  it = 0;
  if (n == 2 && s[0] == 'p' && s[1] == 'i') { cp[0] = 0x3c0; return 1; }
  if (n == 2 && s[0] == 'o' && s[1] == 'o') { cp[0] = 0x221e; return 1; }
  if (n == 5 && !strncmp(s, "theta", 5)) { cp[0] = 0x3b8; it = 1; return 1; }
  if (n == 1 && s[0] == '\x1e') { cp[0] = 0x2192; return 1; }
  if (n == 1 && s[0] == 'e') { cp[0] = 'e'; it = 1; return 1; }
  int k = 0;
  for (; k < n && k < 6; ++k) cp[k] = (unsigned char)s[k]; // lim, taylor, " deg "
  return k;
}

static int width_at(int lv, const char * s, int n, int st) {
  const ui_face * f;
  const ui_glyph * g;
  int w = 0;
  if (st == MI_SYM) {
    if (n == 1 && s[0] == 'S') return rnd(ui_math_sizes[lv], 85); // sigma: drawn with lines
    unsigned cp[6];
    int it, k = sym(s, n, cp, it);
    for (int j = 0; j < k; ++j)
      if ((g = glyph(lv, it, cp[j], &f))) w += g->adv;
    return w;
  }
  for (int i = 0; i < n;) {
    unsigned cp = next_cp(s, n, i);
    if ((g = glyph(lv, st == MI_IT, cp, &f))) w += g->adv;
  }
  return w;
}

#define WFN(L) static int wf##L(const char * s, int n, int sm, int st) { return width_at(sm ? script(L) : L, s, n, st); }
WFN(0) WFN(1) WFN(2) WFN(3) WFN(4) WFN(5) WFN(6) WFN(7)
static const mi_wfn WF[UI_NSIZES] = {wf0, wf1, wf2, wf3, wf4, wf5, wf6, wf7};

static mi_metrics (*MET)[UI_NSIZES]; // heap: KhiCAS's static data must fit pixelShadow (8400 B)
static int met_ready;

const mi_metrics & ui_math_metrics(int lv, int flags) {
  if (lv < 0) lv = 0;
  if (lv >= UI_NSIZES) lv = UI_NSIZES - 1;
  if (!met_ready) {
    MET = (mi_metrics (*)[UI_NSIZES])calloc(3 * UI_NSIZES, sizeof(mi_metrics));
    for (int k = 0; k < 3; ++k)
      for (int l = 0; l < UI_NSIZES; ++l) {
        mi_metrics & m = MET[k][l];
        int s = ui_math_sizes[l], ss = ui_math_sizes[script(l)];
        m.big.adv = (short)rnd(s, 50); m.big.asc = MU[l]->asc; m.big.desc = MU[l]->desc;
        m.small.adv = (short)rnd(ss, 50); m.small.asc = MU[script(l)]->asc; m.small.desc = MU[script(l)]->desc;
        m.opgap = (short)rnd(s, 22);
        m.wf = WF[l];
        m.bar = (short)(s >= 26 ? 2 : 1);
        m.gap = (short)(rnd(s, 13) < 2 ? 2 : rnd(s, 13));
        m.rad = (short)rnd(s, 56);
        m.isw = (short)rnd(s, 50);
        m.flags = (short)(k == 1 ? MI_F_IMPLDOT : k == 2 ? MI_F_CALLBOX : 0);
      }
    met_ready = 1;
  }
  return MET[(flags & MI_F_IMPLDOT) ? 1 : (flags & MI_F_CALLBOX) ? 2 : 0][lv];
}

int ui_math_fit(const char * s, int n, int caret, int maxlv, int w, int h, int flags, mi_layout & L) {
  int lv = maxlv < 0 ? 0 : maxlv >= UI_NSIZES ? UI_NSIZES - 1 : maxlv;
  mi_build(s, n, caret, ui_math_metrics(lv, flags), L);
  if ((L.width <= w && L.asc + L.desc <= h) || lv == UI_NSIZES - 1) return lv;
  // jump close to the size that fits (widths scale about linearly with the size), then step
  int sz = ui_math_sizes[lv], tw = L.width > w ? sz * w / (L.width ? L.width : 1) : sz;
  int th = L.asc + L.desc > h ? sz * h / (L.asc + L.desc) : sz;
  int target = tw < th ? tw : th, nl = lv;
  while (nl + 1 < UI_NSIZES && ui_math_sizes[nl] > target) ++nl;
  if (nl > lv + 1) nl = nl - 1 > lv ? nl - 1 : nl; // one size above the estimate first
  for (lv = nl; ; ++lv) {
    mi_build(s, n, caret, ui_math_metrics(lv, flags), L);
    if ((L.width <= w && L.asc + L.desc <= h) || lv == UI_NSIZES - 1) return lv;
  }
}

int ui_math_refit(const char * s, int n, int caret, int maxlv, int lv0, int w, int h, int flags, mi_layout & L) {
  int lv = lv0 < maxlv ? maxlv : lv0 >= UI_NSIZES ? UI_NSIZES - 1 : lv0;
  mi_build(s, n, caret, ui_math_metrics(lv, flags), L);
  if (L.width > w || L.asc + L.desc > h) // too big now: step down from here
    return lv == UI_NSIZES - 1 ? lv : ui_math_fit(s, n, caret, lv + 1, w, h, flags, L);
  if (lv > maxlv) { // room for one size up? (widths scale about linearly; 8% margin)
    int a = ui_math_sizes[lv], b = ui_math_sizes[lv - 1];
    if ((long)L.width * b * 108 <= (long)w * a * 100 && (long)(L.asc + L.desc) * b * 108 <= (long)h * a * 100) {
      mi_layout U;
      mi_build(s, n, caret, ui_math_metrics(lv - 1, flags), U);
      if (U.width <= w && U.asc + U.desc <= h) { L.ops.swap(U.ops); L.width = U.width; L.asc = U.asc; L.desc = U.desc; L.cx = U.cx; L.cy = U.cy; L.ch = U.ch; return lv - 1; }
    }
  }
  return lv;
}

static void draw_text(const mi_op & o, const char * s, int lv, int x, int y, const unsigned char * r) {
  const ui_face * f;
  const ui_glyph * g;
  int l = o.small ? script(lv) : lv;
  if (o.lit && o.style != MI_SYM) { // a function's display name (asin: arcsin), in its style
    int n = 0;
    while (o.lit[n]) ++n;
    for (int i = 0; i < n;) {
      unsigned cp = next_cp(o.lit, n, i);
      if ((g = glyph(l, o.style == MI_IT, cp, &f))) x += ui_draw_glyph(f, cp, x, y, r, 0);
    }
    return;
  }
  if (o.style == MI_SYM || o.lit) {
    const char * t = o.lit ? o.lit : s + o.pos;
    int n = 0;
    while (t[n] && (o.lit || n < o.len)) ++n;
    unsigned cp[6];
    int it, k = sym(t, n, cp, it);
    for (int j = 0; j < k; ++j)
      if ((g = glyph(l, it, cp[j], &f))) x += ui_draw_glyph(f, cp[j], x, y, r, 0);
    return;
  }
  const char * t = s + o.pos;
  for (int i = 0; i < o.len;) {
    unsigned cp = next_cp(t, o.len, i);
    if ((g = glyph(l, o.style == MI_IT, cp, &f))) x += ui_draw_glyph(f, cp, x, y, r, 0);
  }
}

// a cubic Bezier as a polyline in 1/16 px
static void bezier(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, int th, const unsigned char * r) {
  enum { N = 8 };
  int p[2 * (N + 1)];
  for (int i = 0; i <= N; ++i) {
    long t = i, u = N - i; // Bernstein weights over N^3
    long a = u * u * u, b = 3 * u * u * t, c = 3 * u * t * t, d = t * t * t, D = (long)N * N * N;
    p[2 * i] = (int)((a * x0 + b * x1 + c * x2 + d * x3) / D);
    p[2 * i + 1] = (int)((a * y0 + b * y1 + c * y2 + d * y3) / D);
  }
  ui_poly16(p, N + 1, th, r);
}

void ui_math_draw(const mi_layout & L, const char * s, int lv, int x, int y, int bank, int ink, int bg, int acc) {
  const unsigned char * r = ui_ramp(bank, ink, bg), * ra = ui_ramp(bank, acc, bg);
  const mi_metrics & m = ui_math_metrics(lv, 0);
  int sz = ui_math_sizes[lv];
  unsigned char ph = focus_phase;
  for (unsigned k = 0; k < L.ops.size(); ++k) {
    const mi_op & o = L.ops[k];
    focus_phase = 40 + o.code;
    int ox = x + o.x, oy = y + o.y, w = o.w, h = o.h, osz = o.small ? ui_math_sizes[script(lv)] : sz;
    int th = osz >= 26 ? 32 : osz >= 17 ? 24 : 18; // stroke width, 1/16 px
    switch (o.code) {
    case MI_TEXT: draw_text(o, s, lv, ox, oy, r); break;
    case MI_DOT: { int d = osz >= 24 ? 3 : 2; ui_rrect(bank, ox + w / 2 - d / 2, oy + h / 2 - d / 2, d, d, d / 2, ink, bg); } break;
    case MI_HLINE: ui_fill(ox, oy, w, h, r[3]); break;
    case MI_SQRT: { // tick, heavy down stroke, up stroke, overbar (the prototype's shape)
      int rw = m.rad * osz / sz, b = oy + h - 1, t16 = oy * 16 + m.bar * 8, bh = osz * 16;
      int p[] = {ox * 16 + 8, b * 16 - bh * 26 / 100, ox * 16 + rw * 4, b * 16 - bh * 34 / 100};
      ui_poly16(p, 2, th - 4, r);
      ui_seg16(ox * 16 + rw * 4, b * 16 - bh * 34 / 100, ox * 16 + rw * 9, b * 16, th + 8, r);
      ui_seg16(ox * 16 + rw * 9, b * 16, (ox + rw) * 16 - 8, t16, th - 2, r);
      ui_fill(ox + rw - 1, oy, w - rw + 1, m.bar, r[3]);
    } break;
    case MI_INTEGRAL: { // an elongated S: hooks at both ends (box = sign)
      int x0 = ox * 16, y0 = oy * 16, W = w * 16, H = h * 16;
      bezier(x0 + W * 98 / 100, y0 + W * 20 / 100, x0 + W * 90 / 100, y0 - W * 5 / 100,
             x0 + W * 60 / 100, y0, x0 + W * 58 / 100, y0 + W * 38 / 100, th, r);
      ui_seg16(x0 + W * 58 / 100, y0 + W * 38 / 100, x0 + W * 42 / 100, y0 + H - W * 38 / 100, th + 4, r);
      bezier(x0 + W * 42 / 100, y0 + H - W * 38 / 100, x0 + W * 40 / 100, y0 + H,
             x0 + W * 10 / 100, y0 + H + W * 5 / 100, x0 + W * 2 / 100, y0 + H - W * 20 / 100, th, r);
    } break;
    case MI_SIGMA: { // top bar, two diagonals, bottom bar
      int x0 = ox * 16, y0 = oy * 16, W = w * 16, H = h * 16;
      int p[] = {x0 + W - 8, y0 + 8, x0 + 8, y0 + 8, x0 + W * 55 / 100, y0 + H / 2, x0 + 8, y0 + H - 8, x0 + W - 8, y0 + H - 8};
      ui_poly16(p, 5, th, r);
    } break;
    case MI_LPAREN: case MI_RPAREN: {
      int l = o.code == MI_LPAREN, fl = o.small ? script(lv) : lv;
      const ui_face * pf = MU[fl];
      if (h <= (pf->asc + pf->desc) * 115 / 100) { // text height: the font's own parenthesis
        const ui_glyph * g = ui_glyph_of(pf, l ? '(' : ')');
        if (g) { ui_draw_glyph(pf, l ? '(' : ')', ox + (w - g->w) / 2 - g->ox, oy + h - pf->desc, r, 0); break; }
      }
      const ui_glyph * g = ui_glyph_of(pf, l ? '(' : ')');
      if (g && h <= 3 * g->h) { // taller: the font's parenthesis stretched (STIX's shape, fast)
        ui_draw_glyph_v(pf, l ? '(' : ')', ox + (w - g->w) / 2 - g->ox, oy, h, r);
        break;
      }
      int xo = (l ? ox + w - 1 : ox + 1) * 16, xi = (l ? ox + 1 : ox + w - 1) * 16;
      int y0 = oy * 16 + 8, y1 = (oy + h) * 16 - 8, mid = (y0 + y1) / 2;
      bezier(xo, y0, xi, y0 + (mid - y0) / 2, xi, mid - (mid - y0) / 3, xi, mid, th - 4, r);
      bezier(xi, mid, xi, mid + (y1 - mid) / 3, xi, y1 - (y1 - mid) / 2, xo, y1, th - 4, r);
    } break;
    case MI_LBRACKET: case MI_RBRACKET: {
      int l = o.code == MI_LBRACKET, xs = l ? ox + 1 : ox + w - 2;
      ui_fill(xs, oy, 1, h, r[3]);
      ui_fill(l ? xs : ox, oy, w - 1, 1, r[3]);
      ui_fill(l ? xs : ox, oy + h - 1, w - 1, 1, r[3]);
    } break;
    case MI_LBRACE: case MI_RBRACE: {
      int l = o.code == MI_LBRACE, xo = (l ? ox + w - 1 : ox + 1) * 16, xm = (ox * 16) + w * 8, xi = (l ? ox : ox + w) * 16;
      int y0 = oy * 16 + 8, y1 = (oy + h) * 16 - 8, mid = (y0 + y1) / 2;
      int p[] = {xo, y0, xm, y0 + 24, xm, mid - 16, xi, mid, xm, mid + 16, xm, y1 - 24, xo, y1};
      ui_poly16(p, 7, th - 6, r);
    } break;
    case MI_BAR: ui_fill(ox + w / 2, oy, osz >= 24 ? 2 : 1, h, r[3]); break;
    case MI_BOX: ui_rframe(bank, ox, oy, w, h, osz >= 24 ? 3 : 2, acc, bg); (void)ra; break;
    }
  }
  focus_phase = ph;
}

void ui_math_caret(const mi_layout & L, int x, int y, int bank, int acc) {
  ui_fill(x + L.cx, y + L.cy - 1, 2, L.ch + 2, ui_col(bank, acc));
}

