// ui_font_ti.c - the TI-84 look (FLOWCE_TILOOK): ui_font.h drawn with the calculator's own fonts,
// read from the OS at runtime (FlowCE carries none of TI's font data). The faces of ui_fontdata.h
// become the OS's large font (12 x 14, the home screen's) or its small one (variable width, 12
// rows: the status bar, exponents, fractions), by nominal size.
#ifdef FLOWCE_TILOOK
#include "ui_font.h"
#include "ui_gfx.h"

// a face: its nominal size picks the font (17 and up: large); asc/desc are the OS's text cells
// above and below the baseline (the large cell is 16 rows: 2 blank, then the 14 of the glyph;
// the small one 12: 2 blank, 10 of capitals). count 0: no glyph table (bits, index unused).
#define TIL(n, s) const ui_face n = {s, 16, 0, 0, 0, 0, 0};
#define TIS(n, s) const ui_face n = {s, 12, 0, 0, 0, 0, 0};
TIL(ui_mu34, 34) TIL(ui_mi34, 34) TIL(ui_mu28, 28) TIL(ui_mi28, 28) TIL(ui_mu24, 24) TIL(ui_mi24, 24)
TIL(ui_mu20, 20) TIL(ui_mi20, 20) TIL(ui_mu17, 17) TIL(ui_mi17, 17)
TIS(ui_mu14, 14) TIS(ui_mi14, 14) TIS(ui_mu12, 12) TIS(ui_mi12, 12) TIS(ui_mu10, 10) TIS(ui_mi10, 10)
TIS(ui_tr9, 9) TIS(ui_tb9, 9) TIS(ui_tr10, 10) TIS(ui_tb10, 10) TIS(ui_tr12, 12) TIS(ui_tb12, 12)
#define LARGE(f) ((f)->size >= 17)
// FlowCE's own faces (the start screen's, ui_fx_*: they have glyphs) are drawn by ui_font.c
#define FX(f) ((f)->count)
const ui_glyph * ui_fx_glyph_of(const ui_face * f, unsigned cp);
int ui_fx_text_width(const ui_face * f, const char * s, int n);
int ui_fx_draw_glyph(const ui_face * f, unsigned cp, int x, int y, const unsigned char * ramp, int opaque);
int ui_fx_draw_glyph_v(const ui_face * f, unsigned cp, int x, int ytop, int h, const unsigned char * ramp);

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

// Unicode -> the OS's character set (ASCII but '[': 0x5B is theta there); 0x100 + i: OWN[i]
static const unsigned short map_cp[] = {
  0x3c0, 0xc4, 0x2212, '-', 0xb7, '*', 0x3b8, 0x5b, 0x3a3, 0xc6, 0xd7, 0x09, 0x2192, 0x1c,
  0x2264, 0x17, 0x2265, 0x19, 0x2260, 0x18, 0x2026, 0xce, 0x3bc, 0xc3, 0x3c3, 0xc7, 0x3bb, 0xc2,
  0x221a, 0x10, 0x222b, 0x08, 0xb2, 0x12, 0xb3, 0x0e, 0xb0, 0x14, 0x3b1, 0xbb, 0x3b2, 0xbc,
  0x3b3, 0xbd, 0x394, 0xbe, 0x3b4, 0xbf, 0x3b5, 0xc0, 0x3c6, 0xc9, 0x3a9, 0xca, 0x2191, 0x1e,
  0x2193, 0x1f, 0x2220, 0x13, 0xb1, 0x100, 0x221e, 0x101, 0x2248, 0x102};
static int code_of(unsigned cp) {
  if (cp < 0x80) return cp == '[' ? 0xc1 : (int)cp;
  if (cp >= 0xe000 && cp < 0xe100) return cp - 0xe000; // (private use: the OS's character cp - 0xE000)
  for (unsigned i = 0; i < sizeof(map_cp) / sizeof(map_cp[0]); i += 2)
    if (map_cp[i] == cp) return map_cp[i + 1];
  return -1;
}

// glyphs the OS lacks, drawn in its style (2 px strokes): +- infinity approx; large 14 rows of 12
// bits (high bits first), small 12 rows of 8
static const unsigned short own_l[3][14] = {
  {0, 0x0c00, 0x0c00, 0x0c00, 0xffc0, 0xffc0, 0x0c00, 0x0c00, 0x0c00, 0, 0xffc0, 0xffc0, 0, 0},
  {0, 0, 0, 0, 0x71c0, 0xdb60, 0xce60, 0xce60, 0xdb60, 0x71c0, 0, 0, 0, 0},
  {0, 0, 0, 0x3200, 0x7f60, 0xcfc0, 0x0600, 0, 0x3200, 0x7f60, 0xcfc0, 0x0600, 0, 0}};
static const unsigned char own_s[3][12] = {
  {0, 0, 0x30, 0x30, 0xfc, 0xfc, 0x30, 0x30, 0, 0xfc, 0xfc, 0},
  {0, 0, 0, 0, 0x6c, 0xfe, 0x92, 0xfe, 0x6c, 0, 0, 0},
  {0, 0, 0, 0x64, 0xfc, 0x98, 0, 0x64, 0xfc, 0x98, 0, 0}};
static const unsigned char own_sw[3] = {8, 9, 8};

volatile unsigned char ti_font_ch __attribute__((used)); // read by the assembly in load()
// _LoadPattern (OS jump table 0x021164, IY = the OS flags): copies the glyph of ti_font_ch to
// lFont_record (fontFlags bit 6 set: large) or sFont_record (bits 2 and 6 clear: small); font
// hooks apply as in the OS
static const unsigned char * load(unsigned char c, int large) {
  volatile unsigned char * ff = (volatile unsigned char *)0xD000B2; // fontFlags (IY + 0x32)
  unsigned char keep = *ff;
  *ff = large ? (unsigned char)((keep & ~0x04) | 0x40) : (unsigned char)(keep & ~0x44);
  ti_font_ch = c;
  __asm__ volatile("push af\n\tpush bc\n\tpush de\n\tpush hl\n\tpush ix\n\tpush iy\n\t"
                   "ld iy, 0xD00080\n\tld a, (_ti_font_ch)\n\tcall 0x021164\n\t"
                   "pop iy\n\tpop ix\n\tpop hl\n\tpop de\n\tpop bc\n\tpop af" ::: "memory");
  *ff = keep;
  return (const unsigned char *)(large ? 0xD005A5 : 0xD005C5);
}

// (no cache: KhiCAS's static data must fit pixelShadow, and loading a glyph is a 25-byte copy)
static int width_of(int large, int c) {
  if (c >= 0x100) return large ? 12 : own_sw[c - 0x100];
  if (!large && c == ' ') return 4; // (its space is 2 wide: words ran together)
  return large ? 12 : load((unsigned char)c, 0)[0];
}

// rows of glyph c as 16-bit masks (high bits first) into m; returns its row count
static int rows_of(int large, int c, unsigned short * m) {
  if (c >= 0x100) {
    if (large) for (int r = 0; r < 14; ++r) m[r] = own_l[c - 0x100][r];
    else for (int r = 0; r < 12; ++r) m[r] = (unsigned short)(own_s[c - 0x100][r] << 8);
    return large ? 14 : 12;
  }
  const unsigned char * g = load((unsigned char)c, large);
  if (large) { // 5 px from the first byte, 7 from the second
    for (int r = 0; r < 14; ++r, g += 2) m[r] = (unsigned short)(((g[0] & 0xf8) << 8) | ((g[1] & 0xfe) << 3));
    return 14;
  }
  int w = g[0];
  ++g;
  if (w <= 8) for (int r = 0; r < 12; ++r) m[r] = (unsigned short)(g[r] << 8);
  else for (int r = 0; r < 12; ++r, g += 2) m[r] = (unsigned short)((g[0] << 8) | g[1]);
  return 12;
}

static ui_glyph G; // ui_glyph_of's answer (one at a time)
const ui_glyph * ui_glyph_of(const ui_face * f, unsigned cp) {
  if (FX(f)) return ui_fx_glyph_of(f, cp);
  int c = code_of(cp);
  if (c < 0) return 0;
  int large = LARGE(f);
  unsigned short m[14];
  int n = rows_of(large, c, m), top = 0, bot = n;
  while (top < n && !m[top]) ++top;
  while (bot > top && !m[bot - 1]) --bot;
  G.code = (unsigned char)c; G.adv = (unsigned char)width_of(large, c);
  G.w = top < bot ? G.adv : 0; G.h = (unsigned char)(bot - top);
  G.ox = 0; G.oy = (signed char)(top - n); G.off = 0;
  return &G;
}

int ui_text_width(const ui_face * f, const char * s, int n) {
  if (FX(f)) return ui_fx_text_width(f, s, n);
  const char * e = n < 0 ? 0 : s + n;
  int w = 0, large = LARGE(f), c;
  unsigned cp;
  while ((cp = ui_utf8(&s, e)))
    if ((c = code_of(cp)) >= 0) w += width_of(large, c);
  return w;
}

// rows [r0, r1) of masks m, w columns, drawn with their first row at screen row y
static void blit(const unsigned short * m, int r0, int r1, int w, int x, int y, const unsigned char * ramp, int opaque) {
  unsigned char ink = ramp[3], bg = ramp[0];
  for (int r = r0; r < r1; ++r, ++y) {
    if (y < ui_cy0 || y >= ui_cy1) continue;
    unsigned char * q = ui_fb + y * UI_W + x;
    unsigned short v = m[r], b = 0x8000;
    for (int xx = 0; xx < w; ++xx, ++q, b >>= 1) {
      if (x + xx < ui_cx0 || x + xx >= ui_cx1) continue;
      if (v & b) *q = ink;
      else if (opaque) *q = bg;
    }
  }
}

int ui_draw_glyph(const ui_face * f, unsigned cp, int x, int y, const unsigned char * ramp, int opaque) {
  if (FX(f)) return ui_fx_draw_glyph(f, cp, x, y, ramp, opaque);
  int c = code_of(cp);
  if (c < 0) return 0;
  int large = LARGE(f), w = width_of(large, c);
  unsigned short m[14];
  int n = rows_of(large, c, m);
  if (x < ui_cx1 && x + w > ui_cx0 && y > ui_cy0 && y - n < ui_cy1) blit(m, 0, n, w, x, y - n, ramp, opaque);
  return w;
}

int ui_draw_glyph_v(const ui_face * f, unsigned cp, int x, int ytop, int h, const unsigned char * ramp) {
  if (FX(f)) return ui_fx_draw_glyph_v(f, cp, x, ytop, h, ramp);
  int c = code_of(cp);
  if (c < 0 || h <= 0) return 0;
  int large = LARGE(f), w = width_of(large, c);
  unsigned short m[14];
  int n = rows_of(large, c, m), top = 0, bot = n;
  while (top < n && !m[top]) ++top;
  while (bot > top && !m[bot - 1]) --bot;
  if (top == bot) return w;
  for (int yy = 0; yy < h; ++yy) // each row from the ink row at the same height
    blit(m, top + (yy * 2 + 1) * (bot - top) / (2 * h), top + (yy * 2 + 1) * (bot - top) / (2 * h) + 1, w, x, ytop + yy, ramp, 0);
  return w;
}

int ui_draw_text(const ui_face * f, const char * s, int n, int x, int y, const unsigned char * ramp, int opaque) {
  const char * e = n < 0 ? 0 : s + n;
  int x0 = x;
  unsigned cp;
  while ((cp = ui_utf8(&s, e))) x += ui_draw_glyph(f, cp, x, y, ramp, opaque);
  return x - x0;
}
#endif
