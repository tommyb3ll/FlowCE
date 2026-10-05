// mi_preview.cc - render a buffer laid out by mathinput (default metrics) as a PGM image:
// black ink on white, scale x2, caret in gray. For visual review only.
//   mi_preview "<buffer>" <caret|-1> <out.pgm>
#include "mathinput.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

// 5x8 glyphs for ASCII 32..126, one byte per column, bit 0 = top row, bit 7 = descender
static const unsigned char F[95][5] = {
  {0,0,0,0,0},{0,0,0x5F,0,0},{0,7,0,7,0},{0x14,0x7F,0x14,0x7F,0x14},
  {0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,8,0x64,0x62},{0x36,0x49,0x55,0x22,0x50},{0,5,3,0,0},
  {0,0x1C,0x22,0x41,0},{0,0x41,0x22,0x1C,0},{0x14,8,0x3E,8,0x14},{8,8,0x3E,8,8},
  {0,0x50,0x30,0,0},{8,8,8,8,8},{0,0x60,0x60,0,0},{0x20,0x10,8,4,2},
  {0x3E,0x51,0x49,0x45,0x3E},{0,0x42,0x7F,0x40,0},{0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},
  {0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x30},{1,0x71,9,5,3},
  {0x36,0x49,0x49,0x49,0x36},{6,0x49,0x49,0x29,0x1E},{0,0x36,0x36,0,0},{0,0x56,0x36,0,0},
  {8,0x14,0x22,0x41,0},{0x14,0x14,0x14,0x14,0x14},{0,0x41,0x22,0x14,8},{2,1,0x51,9,6},
  {0x32,0x49,0x79,0x41,0x3E},{0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
  {0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,9,9,9,1},{0x3E,0x41,0x49,0x49,0x7A},
  {0x7F,8,8,8,0x7F},{0,0x41,0x7F,0x41,0},{0x20,0x40,0x41,0x3F,1},{0x7F,8,0x14,0x22,0x41},
  {0x7F,0x40,0x40,0x40,0x40},{0x7F,2,0x0C,2,0x7F},{0x7F,4,8,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
  {0x7F,9,9,9,6},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,9,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},
  {1,1,0x7F,1,1},{0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},
  {0x63,0x14,8,0x14,0x63},{7,8,0x70,8,7},{0x61,0x51,0x49,0x45,0x43},{0,0x7F,0x41,0x41,0},
  {2,4,8,0x10,0x20},{0,0x41,0x41,0x7F,0},{4,2,1,2,4},{0x40,0x40,0x40,0x40,0x40},
  {0,1,2,4,0},{0x20,0x54,0x54,0x54,0x78},{0x7F,0x48,0x44,0x44,0x38},{0x38,0x44,0x44,0x44,0x20},
  {0x38,0x44,0x44,0x48,0x7F},{0x38,0x54,0x54,0x54,0x18},{8,0x7E,9,1,2},{0x18,0xA4,0xA4,0xA4,0x7C},
  {0x7F,8,4,4,0x78},{0,0x44,0x7D,0x40,0},{0x40,0x80,0x84,0x7D,0},{0x7F,0x10,0x28,0x44,0},
  {0,0x41,0x7F,0x40,0},{0x7C,4,0x18,4,0x78},{0x7C,8,4,4,0x78},{0x38,0x44,0x44,0x44,0x38},
  {0xFC,0x24,0x24,0x24,0x18},{0x18,0x24,0x24,0x18,0xFC},{0x7C,8,4,4,8},{0x48,0x54,0x54,0x54,0x20},
  {4,0x3F,0x44,0x40,0x20},{0x3C,0x40,0x40,0x20,0x7C},{0x1C,0x20,0x40,0x20,0x1C},{0x3C,0x40,0x30,0x40,0x3C},
  {0x44,0x28,0x10,0x28,0x44},{0x1C,0xA0,0xA0,0xA0,0x7C},{0x44,0x64,0x54,0x4C,0x44},{0,8,0x36,0x41,0},
  {0,0,0x7F,0,0},{0,0x41,0x36,8,0},{8,4,8,0x10,8}};
static const unsigned char PI[5] = {4, 0x7C, 4, 0x3C, 0x44}, INF[5] = {8, 0x14, 8, 0x14, 8},
                           ARROW[5] = {8, 8, 8, 0x1C, 8};

static int W, H, OX, OY;
static std::vector<unsigned char> img;
static void px(int x, int y, int v = 0) {
  x += OX; y += OY;
  if (x >= 0 && x < W && y >= 0 && y < H) img[y * W + x] = (unsigned char)v;
}
static void hl(int x0, int x1, int y) { for (int x = x0; x <= x1; ++x) px(x, y); }
static void vl(int x, int y0, int y1, int v = 0) { for (int y = y0; y <= y1; ++y) px(x, y, v); }
static void line(int x0, int y0, int x1, int y1) { // Bresenham
  int dx = abs(x1 - x0), dy = -abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, e = dx + dy;
  for (;;) {
    px(x0, y0);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * e;
    if (e2 >= dy) { e += dy; x0 += sx; }
    if (e2 <= dx) { e += dx; y0 += sy; }
  }
}
static void rect(int x, int y, int w, int h) { hl(x, x + w - 1, y); hl(x, x + w - 1, y + h - 1); vl(x, y, y + h - 1); vl(x + w - 1, y, y + h - 1); }

// one glyph in a cell at x; small: 5x8 at 1:1, big: 6 wide with doubled rows (14 + 2 desc)
static void glyph(const unsigned char * g, int x, int base, int sm) {
  int gw = sm ? 5 : 6, rh = sm ? 1 : 2;
  if (!g) { rect(x + 1, base - 7 * rh, gw - 1, 7 * rh); return; } // unknown glyph: box
  for (int c = 0; c < gw; ++c)
    for (int r = 0; r < 8; ++r)
      if (g[c * 5 / gw] >> r & 1)
        for (int k = 0; k < rh; ++k) px(x + c, base - 7 * rh + r * rh + k);
}
static const unsigned char * ascii(int c) { return c >= 32 && c <= 126 ? F[c - 32] : 0; }

static void text(const mi_op & o, const char * s) {
  int adv = o.small ? mi_default_metrics.small.adv : mi_default_metrics.big.adv, x = o.x;
  if (o.lit && !strcmp(o.lit, "pi")) { glyph(PI, x, o.y, o.small); return; }
  if (o.lit && !strcmp(o.lit, "oo")) { glyph(INF, x, o.y, o.small); return; }
  const char * p = o.lit ? o.lit : s + o.pos;
  int n = o.lit ? (int)strlen(o.lit) : o.len;
  for (int i = 0; i < n; ++i) {
    unsigned char c = p[i];
    if ((c & 0xC0) == 0x80 && i) continue; // UTF-8 continuation: same glyph
    glyph(c == 0x1e ? ARROW : ascii(c), x, o.y, o.small);
    x += adv;
  }
}

static void draw(const mi_op & o, const char * s) {
  int x = o.x, y = o.y, w = o.w, h = o.h, m = y + h / 2;
  switch (o.code) {
  case MI_TEXT: text(o, s); break;
  case MI_DOT: hl(x + w / 2 - 1, x + w / 2, m - 1); hl(x + w / 2 - 1, x + w / 2, m); break;
  case MI_HLINE: for (int i = 0; i < h; ++i) hl(x, x + w - 1, y + i); break;
  case MI_SQRT: line(x, y + h * 2 / 3, x + 2, y + h - 1); line(x + 2, y + h - 1, x + 5, y); hl(x + 5, x + w - 1, y); break;
  case MI_INTEGRAL: vl(x + 3, y + 2, y + h - 3); px(x + 4, y + 1); px(x + 5, y); px(x + 2, y + h - 2); px(x + 1, y + h - 1); break;
  case MI_SIGMA: hl(x, x + w - 1, y); hl(x, x + w - 1, y + h - 1); line(x, y, x + w / 2, m); line(x + w / 2, m, x, y + h - 1); break;
  case MI_LPAREN: vl(x + 1, y + 2, y + h - 3); px(x + 2, y + 1); px(x + 3, y); px(x + 2, y + h - 2); px(x + 3, y + h - 1); break;
  case MI_RPAREN: vl(x + w - 2, y + 2, y + h - 3); px(x + w - 3, y + 1); px(x + w - 4, y); px(x + w - 3, y + h - 2); px(x + w - 4, y + h - 1); break;
  case MI_LBRACKET: vl(x + 1, y, y + h - 1); hl(x + 1, x + w - 2, y); hl(x + 1, x + w - 2, y + h - 1); break;
  case MI_RBRACKET: vl(x + w - 2, y, y + h - 1); hl(x + 1, x + w - 2, y); hl(x + 1, x + w - 2, y + h - 1); break;
  case MI_LBRACE: vl(x + 2, y + 1, y + h - 2); px(x + 1, m); px(x + 3, y); px(x + 3, y + h - 1); break;
  case MI_RBRACE: vl(x + w - 3, y + 1, y + h - 2); px(x + w - 2, m); px(x + w - 4, y); px(x + w - 4, y + h - 1); break;
  case MI_BAR: vl(x + w / 2, y, y + h - 1); break;
  case MI_BOX: rect(x, y, w, h); break;
  }
}

int main(int argc, char ** argv) {
  if (argc < 4) { fprintf(stderr, "usage: mi_preview \"<buffer>\" <caret|-1> <out.pgm>\n"); return 2; }
  const char * s = argv[1];
  int caret = atoi(argv[2]), sc = 2;
  mi_layout L;
  mi_build(s, (int)strlen(s), caret, mi_default_metrics, L);
  W = L.width + 8; H = L.asc + L.desc + 8; OX = 4; OY = L.asc + 4;
  img.assign((size_t)W * H, 255);
  for (size_t i = 0; i < L.ops.size(); ++i) draw(L.ops[i], s);
  if (caret >= 0) vl(L.cx, L.cy, L.cy + L.ch - 1, 128);
  FILE * f = fopen(argv[3], "wb");
  if (!f) { perror(argv[3]); return 1; }
  fprintf(f, "P5\n%d %d\n255\n", W * sc, H * sc);
  for (int y = 0; y < H * sc; ++y)
    for (int x = 0; x < W * sc; ++x) fputc(img[(y / sc) * W + x / sc], f);
  fclose(f);
  printf("%s: %d ops, %dx%d px (x%d), caret (%d,%d,%d)\n", argv[3], (int)L.ops.size(), W, H, sc, L.cx, L.cy, L.ch);
  return 0;
}
