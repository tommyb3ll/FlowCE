// ui_font.c - draws the 4-shade fonts of ui_fontdata.c (see ui_font.h, tools/fonts/mkfonts.py).
#include "ui_font.h"
#include "ui_gfx.h"

// glyph codes above 0x7f: 0x80 + index here (same order as EXTRA in mkfonts.py)
static const unsigned short extra_cp[] = {0x3c0, 0x2212, 0xb7, 0x3b8, 0x3a3, 0xb1, 0xd7, 0x221e,
                                          0x2192, 0x2264, 0x2265, 0x2260, 0x2248, 0x2026};

unsigned ui_utf8(const char ** sp, const char * end) {
  const unsigned char * s = (const unsigned char *)*sp;
  if ((end && (const char *)s >= end) || !*s) return 0;
  unsigned c = *s++;
  int n = c >= 0xf0 ? 3 : c >= 0xe0 ? 2 : c >= 0xc0 ? 1 : 0;
  if (n) c &= 0x3f >> n;
  for (; n && (!end || (const char *)s < end) && (*s & 0xc0) == 0x80; --n) c = (c << 6) | (*s++ & 0x3f);
  *sp = (const char *)s;
  return c;
}

static int code_of(unsigned cp) {
  if (cp < 0x80) return cp;
  for (unsigned i = 0; i < sizeof(extra_cp) / sizeof(extra_cp[0]); ++i)
    if (extra_cp[i] == cp) return 0x80 + i;
  return -1;
}

const ui_glyph * ui_glyph_of(const ui_face * f, unsigned cp) {
  int c = code_of(cp), lo = 0, hi = f->count - 1;
  if (c < 0) return 0;
  while (lo <= hi) {
    int mid = (lo + hi) >> 1, m = f->glyphs[mid].code;
    if (m == c) return f->glyphs + mid;
    if (m < c) lo = mid + 1; else hi = mid - 1;
  }
  return 0;
}

int ui_text_width(const ui_face * f, const char * s, int n) {
  const char * e = n < 0 ? 0 : s + n;
  int w = 0;
  unsigned cp;
  while ((cp = ui_utf8(&s, e))) {
    const ui_glyph * g = ui_glyph_of(f, cp);
    if (g) w += g->adv;
  }
  return w;
}

int ui_draw_glyph(const ui_face * f, unsigned cp, int x, int y, const unsigned char * ramp, int opaque) {
  const ui_glyph * g = ui_glyph_of(f, cp);
  if (!g) return 0;
  int x0 = x + g->ox, y0 = y + g->oy, w = g->w, h = g->h;
  if (x0 >= ui_cx1 || y0 >= ui_cy1 || x0 + w <= ui_cx0 || y0 + h <= ui_cy0 || !w) return g->adv;
  const unsigned char * b = f->bits + g->off;
  int xa = x0 < ui_cx0 ? ui_cx0 - x0 : 0, xb = x0 + w > ui_cx1 ? ui_cx1 - x0 : w; // visible columns
  unsigned i = 0;
  for (int yy = 0; yy < h; ++yy, i += w) {
    int sy = y0 + yy;
    if (sy < ui_cy0 || sy >= ui_cy1) continue;
    unsigned char * row = ui_fb + sy * UI_W + x0;
    for (int xx = xa; xx < xb; ++xx) {
      unsigned k = i + xx;
      unsigned char byte = b[k >> 2];
      if (!byte && !opaque) { xx += 3 - (k & 3); continue; } // 4 transparent pixels
      int lv = (byte >> (6 - 2 * (k & 3))) & 3;
      if (!lv) { if (opaque) row[xx] = ramp[0]; continue; }
      unsigned char * p = row + xx, o = *p;
      int ol = o == ramp[3] ? 3 : o == ramp[2] ? 2 : o == ramp[1] ? 1 : 0; // overlaps keep the darker shade
      if (lv > ol) *p = ramp[lv];
    }
  }
  return g->adv;
}

int ui_draw_text(const ui_face * f, const char * s, int n, int x, int y, const unsigned char * ramp, int opaque) {
  const char * e = n < 0 ? 0 : s + n;
  int x0 = x;
  unsigned cp;
  while ((cp = ui_utf8(&s, e))) x += ui_draw_glyph(f, cp, x, y, ramp, opaque);
  return x - x0;
}
