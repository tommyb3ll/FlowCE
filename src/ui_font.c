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
  int c = code_of(cp);
  if (c < 0) return 0;
  unsigned char i = f->index[c];
  return i == 255 ? 0 : f->glyphs + i;
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

// the 4 pixels of a glyph byte, high bits first: their masks and the level-2 value under each
static const unsigned char pmask[4] = {0xc0, 0x30, 0x0c, 0x03}, pmid[4] = {0x80, 0x20, 0x08, 0x02};
#ifdef TICE
// the same loop in assembly (tools/asm/ui_blit.asm), for glyphs not clipped horizontally and not
// opaque: KhiCAS runs from flash, where every instruction fetch is slow
#include "ui_blit_code.h"
typedef void (*ui_blit_fn)(unsigned char *, const unsigned char *, unsigned char, unsigned char,
                           unsigned char, const unsigned char *);
unsigned char ui_blit_c; // test switch (tools/emu pokes it): 1 = always the C loop
#endif

// rows of 2-bit pixels (4 per byte, high bits first, rows back to back) starting at pixel mi of
// *bp, w wide, drawn at dst (screen rows); only columns [xa, xb) are touched. No shifts in the
// loop (each one is a helper call on the eZ80): masks walk the byte, consumed bits are cleared,
// so a zero byte means the rest of it is transparent.
static void blit_rows(unsigned char * dst, const unsigned char * bp, unsigned char mi, unsigned char uw,
                      unsigned char rows, unsigned char xa, unsigned char xb, const unsigned char * ramp, int opaque) {
#ifdef TICE
  if (!ui_blit_c && !xa && xb == uw && !opaque) {
    ((ui_blit_fn)(const void *)ui_blit2_code)(dst, bp, mi, uw, rows, ramp);
    return;
  }
#endif
  unsigned char r0 = ramp[0], r1 = ramp[1], r2 = ramp[2], r3 = ramp[3], clipx = xa || xb < uw;
  unsigned char b = *bp++;
  if (mi) b &= 0xff >> (2 * mi);
  for (unsigned char yy = 0; yy < rows; ++yy, dst += UI_W) {
    unsigned char * q = dst;
    for (unsigned char xx = 0; xx < uw;) {
      if (!b && !opaque) { // the rest of this byte is transparent
        unsigned char t = 4 - mi;
        if (t > uw - xx) t = uw - xx;
        xx += t; q += t; mi += t;
        if (mi == 4) { mi = 0; b = *bp++; }
        continue;
      }
      unsigned char m = pmask[mi], v = b & m;
      b ^= v;
      if (!clipx || (xx >= xa && xx < xb)) {
        if (v) {
          unsigned char o = *q, c = v == m ? r3 : v == pmid[mi] ? r2 : r1;
          if (o == r0 || opaque) *q = c;
          else { // overlaps keep the darker shade
            unsigned char lv = v == m ? 3 : v == pmid[mi] ? 2 : 1;
            unsigned char ol = o == r3 ? 3 : o == r2 ? 2 : o == r1 ? 1 : 0;
            if (lv > ol) *q = c;
          }
        } else if (opaque) *q = r0;
      }
      ++xx; ++q;
      if (++mi == 4) { mi = 0; b = *bp++; }
    }
  }
}

int ui_draw_glyph(const ui_face * f, unsigned cp, int x, int y, const unsigned char * ramp, int opaque) {
  const ui_glyph * g = ui_glyph_of(f, cp);
  if (!g) return 0;
  int x0 = x + g->ox, y0 = y + g->oy, w = g->w, h = g->h;
  if (x0 >= ui_cx1 || y0 >= ui_cy1 || x0 + w <= ui_cx0 || y0 + h <= ui_cy0 || !w) return g->adv;
  // visible columns [xa, xb) and rows [ya, yb) of the glyph box (all < 256)
  unsigned char xa = x0 < ui_cx0 ? ui_cx0 - x0 : 0, xb = x0 + w > ui_cx1 ? ui_cx1 - x0 : w;
  unsigned char ya = y0 < ui_cy0 ? ui_cy0 - y0 : 0, yb = y0 + h > ui_cy1 ? ui_cy1 - y0 : h;
  unsigned k = (unsigned)ya * (unsigned)w;
  blit_rows(ui_fb + (y0 + ya) * UI_W + x0, f->bits + g->off + k / 4, k & 3, w, yb - ya, xa, xb, ramp, opaque);
  return g->adv;
}

int ui_draw_glyph_v(const ui_face * f, unsigned cp, int x, int ytop, int h, const unsigned char * ramp) {
  const ui_glyph * g = ui_glyph_of(f, cp);
  if (!g || !g->w || !g->h || h <= 0) return g ? g->adv : 0;
  int x0 = x + g->ox, w = g->w;
  if (x0 >= ui_cx1 || x0 + w <= ui_cx0) return g->adv;
  unsigned char xa = x0 < ui_cx0 ? ui_cx0 - x0 : 0, xb = x0 + w > ui_cx1 ? ui_cx1 - x0 : w;
  for (int yy = 0; yy < h; ++yy) { // each row from the source row at the same height
    int sy = ytop + yy;
    if (sy < ui_cy0 || sy >= ui_cy1) continue;
    unsigned k = (unsigned)((yy * 2 + 1) * g->h / (2 * h)) * (unsigned)w;
    blit_rows(ui_fb + sy * UI_W + x0, f->bits + g->off + k / 4, k & 3, w, 1, xa, xb, ramp, 0);
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
