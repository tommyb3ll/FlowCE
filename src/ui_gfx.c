// ui_gfx.c - palette banks, ramps and anti-aliased primitives of the Focus interface (ui_gfx.h).
#include <string.h>
#include <stdlib.h>
#include "ui_gfx.h"
#ifdef TICE
#include <sys/lcd.h>
#define PALETTE ((unsigned short *)lcd_Palette)
#define FB ((unsigned char *)lcd_Ram)
#else
static unsigned short host_palette[256];
static unsigned char host_fb[UI_W * UI_H];
#define PALETTE host_palette
#define FB host_fb
unsigned short * ui_host_palette(void) { return host_palette; }
#endif

unsigned char * ui_fb = FB;
int ui_cx0 = 0, ui_cy0 = 0, ui_cx1 = UI_W, ui_cy1 = UI_H;
static int band_y0 = 0, band_y1 = UI_H; // the rows drawing may touch
static unsigned char * strip;
static int strip_rows;

void ui_clip(int x0, int y0, int x1, int y1) {
  ui_cx0 = x0 < 0 ? 0 : x0; ui_cy0 = y0 < band_y0 ? band_y0 : y0;
  ui_cx1 = x1 > UI_W ? UI_W : x1; ui_cy1 = y1 > band_y1 ? band_y1 : y1;
}
void ui_noclip(void) { ui_cx0 = 0; ui_cy0 = band_y0; ui_cx1 = UI_W; ui_cy1 = band_y1; }

// The strip is allocated once, at its full size, and kept: freed and allocated again around
// every repaint, it came back each time from fresh heap (small allocations took the freed space
// in between), and a session of calculations ran the heap out (KhiCAS aborts then: the user's
// crashes after a while, 2026-10-06).
enum { STRIP_ROWS = 64 };
int ui_band_open(int maxrows) {
  (void)maxrows;
  if (!strip)
    for (int r = STRIP_ROWS; r >= 8 && !strip; r /= 2)
      if ((strip = (unsigned char *)malloc(UI_W * r))) strip_rows = r;
  return strip_rows;
}
void ui_band_close(void) {}
void ui_band_begin(int y0, int y1) {
  band_y0 = y0; band_y1 = y1;
  ui_fb = strip - y0 * UI_W; // screen row y lands at strip row y - y0
  ui_noclip();
}
void ui_band_load(void) { memcpy(strip, FB + band_y0 * UI_W, (band_y1 - band_y0) * UI_W); }
void ui_band_end(void) {
  memcpy(FB + band_y0 * UI_W, strip, (band_y1 - band_y0) * UI_W);
  ui_fb = FB; band_y0 = 0; band_y1 = UI_H;
  ui_noclip();
}

// Paper: the prototype's light theme
const ui_theme ui_theme_paper = {{
  {247, 248, 250}, {17, 20, 25}, {108, 117, 129}, {219, 224, 231}, {36, 92, 204}, {226, 234, 250},
  {255, 255, 255}, {255, 255, 255}, {255, 255, 255}, {78, 86, 98}, {39, 140, 74}, {208, 214, 222},
  {255, 255, 255}, {199, 68, 64}, {112, 72, 184}, {226, 118, 24}, {247, 248, 250}}, {30, 36, 46}, 36};
// Night: the prototype's dark theme (UC_WHITE, text on a colored chip: dark here)
const ui_theme ui_theme_night = {{
  {12, 15, 20}, {234, 238, 243}, {134, 145, 158}, {38, 46, 58}, {100, 165, 255}, {24, 40, 64},
  {7, 11, 17}, {22, 28, 36}, {16, 21, 28}, {150, 160, 173}, {86, 200, 130}, {4, 6, 9},
  {7, 11, 17}, {255, 112, 102}, {182, 146, 255}, {255, 170, 82}, {12, 15, 20}}, {0, 0, 0}, 50};

enum { BANK = 64, MAXR = (BANK - UC_COUNT) / 2 };
static const ui_theme * TH = &ui_theme_paper;
static int dimmed;
static unsigned char rfg[2][MAXR], rbg[2][MAXR], nr[2];
static unsigned char ridx[2][MAXR][4];

// the LCD palette format in 8 bpp mode: 1555 (x RRRRR GGGGG BBBBB), as graphx's gfx_RGBTo1555.
// (graphic.c writes 565 values for KhiCAS's own 0-127: fine for its saturated cube colors,
// visibly wrong for the mid grays of anti-aliasing.)
static unsigned short rgb565(int r, int g, int b) {
  return (unsigned short)(((r & 0xf8) << 7) | ((g & 0xf8) << 2) | (b >> 3));
}
// slot s of bank: base color s < UC_COUNT, else mid (s - UC_COUNT) & 1 of ramp (s - UC_COUNT) / 2
static void base_rgb(int s, int * c) { // a theme color; UC_DIM: the background dimmed
  const unsigned char * a = TH->rgb[s == UC_DIM ? UC_BG : s];
  for (int i = 0; i < 3; ++i) c[i] = a[i] + (s == UC_DIM ? ((TH->dim_to[i] - a[i]) * TH->dim_pct) / 100 : 0);
}
static void slot_rgb(int bank, int s, int * c) {
  if (s < UC_COUNT) base_rgb(s, c);
  else {
    int k = (s - UC_COUNT) >> 1, a[3], b[3];
    base_rgb(rbg[bank][k], a); base_rgb(rfg[bank][k], b);
    int t = ((s - UC_COUNT) & 1) ? 2 : 1; // 1/3 or 2/3 of the way to the ink
    for (int i = 0; i < 3; ++i) c[i] = a[i] + ((b[i] - a[i]) * t) / 3;
  }
  if (bank == 0 && dimmed)
    for (int i = 0; i < 3; ++i) c[i] += ((TH->dim_to[i] - c[i]) * TH->dim_pct) / 100;
}
static void write_slot(int bank, int s) {
  int c[3];
  slot_rgb(bank, s, c);
  PALETTE[128 + bank * BANK + s] = rgb565(c[0], c[1], c[2]);
}
static void write_bank(int bank) {
  for (int s = 0; s < UC_COUNT + 2 * nr[bank]; ++s) write_slot(bank, s);
}

void ui_set_theme(const ui_theme * t) { TH = t; write_bank(0); write_bank(1); }
void ui_dim(int on) { if (dimmed != on) { dimmed = on; write_bank(0); } }
void ui_restore_os_palette(void) {}
unsigned char ui_col(int bank, int c) { return (unsigned char)(128 + bank * BANK + c); }

const unsigned char * ui_ramp(int bank, int fg, int bg) {
  int k;
  for (k = 0; k < nr[bank]; ++k)
    if (rfg[bank][k] == fg && rbg[bank][k] == bg) return ridx[bank][k];
  if (k >= MAXR) k = MAXR - 1; // full: reuse the last ramp (never happens with the screens' needs)
  else ++nr[bank];
  rfg[bank][k] = fg; rbg[bank][k] = bg;
  unsigned char * r = ridx[bank][k];
  r[0] = ui_col(bank, bg); r[3] = ui_col(bank, fg);
  r[1] = (unsigned char)(128 + bank * BANK + UC_COUNT + 2 * k);
  r[2] = (unsigned char)(r[1] + 1);
  write_slot(bank, UC_COUNT + 2 * k); write_slot(bank, UC_COUNT + 2 * k + 1);
  return r;
}

void ui_fill(int x, int y, int w, int h, unsigned char idx) {
  int x0 = x < ui_cx0 ? ui_cx0 : x, x1 = x + w > ui_cx1 ? ui_cx1 : x + w;
  int y0 = y < ui_cy0 ? ui_cy0 : y, y1 = y + h > ui_cy1 ? ui_cy1 : y + h;
  if (x1 <= x0) return;
  for (; y0 < y1; ++y0) memset(ui_fb + y0 * UI_W + x0, idx, x1 - x0);
}

// pixel x of the framebuffer row row (inside the clip rows), shade lv: keeps the darker shade
static void put(unsigned char * row, int x, int lv, const unsigned char * r) {
  if (lv <= 0 || x < ui_cx0 || x >= ui_cx1) return;
  unsigned char * p = row + x, o = *p;
  int ol = o == r[3] ? 3 : o == r[2] ? 2 : o == r[1] ? 1 : 0;
  if (lv > ol) *p = r[lv];
}

static int isqrt(long v) { // floor(sqrt(v)), v < 2^31
  long r = 0, b = 1L << 30;
  while (b > v) b >>= 2;
  while (b) { if (v >= r + b) { v -= r + b; r = (r >> 1) + b; } else r >>= 1; b >>= 2; }
  return (int)r;
}

// shade of a pixel at squared distance q (1/64 px^2) from the center of a disc of radius R (1/8
// px), the prototype's quantization of the coverage 0.5 - d of a pixel at distance d (1/8 px)
// outside the edge: 3 if d <= -2, 2 if d <= 1, 1 if d <= 3; d = floor(sqrt(q)) - R is compared
// without the square root: d <= t <=> q < (R + t + 1)^2. The squares, per disc (disc_limits):
// a multiplication is a helper call on the eZ80, and three per pixel made the busy dots cost 1%
// of a calculation.
static void disc_limits(int R, int lim[3]) {
  int a = R - 1, b = R + 2, c = R + 4;
  lim[0] = a > 0 ? a * a : 0; lim[1] = b > 0 ? b * b : 0; lim[2] = c > 0 ? c * c : 0;
}
static int disc_level(int q, const int lim[3]) {
  return q < lim[0] ? 3 : q < lim[1] ? 2 : q < lim[2] ? 1 : 0;
}
// corners of a rounded rectangle: pixels of the r x r corner boxes by distance to the arc
// (24-bit integers only: r <= 60)
static void corners(int x, int y, int w, int h, int r, const unsigned char * ramp, int ring) {
  int R = r * 8, lim[3], lim1[3];
  disc_limits(R, lim);
  disc_limits(R - 8, lim1);
  for (int q = 0; q < 4; ++q) {
    int bx = (q & 1) ? x + w - r : x, by = (q & 2) ? y + h - r : y;
    int cx8 = ((q & 1) ? x + w - r : x + r) * 8, cy8 = ((q & 2) ? y + h - r : y + r) * 8;
    for (int py = by; py < by + r; ++py) {
      if (py < ui_cy0 || py >= ui_cy1) continue;
      // per row the framebuffer row and d2, then additions only: a multiplication or a 24-bit shift
      // is a helper call on the eZ80 (two per pixel made the busy dots 1% of a calculation)
      unsigned char * row = ui_fb + py * UI_W;
      int ey = py * 8 + 4 - cy8, ex = bx * 8 + 4 - cx8, d2 = ex * ex + ey * ey, step = 16 * ex + 64;
      for (int px = bx; px < bx + r; ++px) {
        // a 1 px ring is the disc of radius r minus the disc of radius r - 1
        int lv = ring ? disc_level(d2, lim) - disc_level(d2, lim1) : disc_level(d2, lim);
        put(row, px, lv, ramp);
        d2 += step; step += 128; // d2 = ex^2 + ey^2, ex += 8 per pixel
      }
    }
  }
}

void ui_rrect(int bank, int x, int y, int w, int h, int r, int c, int bg) {
  const unsigned char * ramp = ui_ramp(bank, c, bg);
  if (r * 2 > h) r = h / 2;
  if (r * 2 > w) r = w / 2;
  ui_fill(x + r, y, w - 2 * r, h, ramp[3]);
  ui_fill(x, y + r, r, h - 2 * r, ramp[3]);
  ui_fill(x + w - r, y + r, r, h - 2 * r, ramp[3]);
  corners(x, y, w, h, r, ramp, 0);
}

void ui_rframe(int bank, int x, int y, int w, int h, int r, int c, int bg) {
  const unsigned char * ramp = ui_ramp(bank, c, bg);
  unsigned char k = ramp[3];
  if (r * 2 > h) r = h / 2;
  if (r * 2 > w) r = w / 2;
  ui_fill(x + r, y, w - 2 * r, 1, k); ui_fill(x + r, y + h - 1, w - 2 * r, 1, k);
  ui_fill(x, y + r, 1, h - 2 * r, k); ui_fill(x + w - 1, y + r, 1, h - 2 * r, k);
  corners(x, y, w, h, r, ramp, 1);
}

// A thick anti-aliased segment by spans: along its major axis (rows for a steep segment, columns
// for a flat one) each pixel line crosses the stroke over [v - hw, v + hw] (v: the center line,
// hw: half the stroke width measured along the line; 1/256 px), and each pixel is shaded by the
// part of it that span covers (the font's quantization). Per pixel only 24-bit additions and
// compares; the ends are cut along the lines, extended a quarter stroke so polylines join.
void ui_seg16(int ax, int ay, int bx, int by, int th, const unsigned char * ramp) {
  int dx = bx - ax, dy = by - ay, adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy, steep = ady >= adx;
  int au = steep ? ay : ax, av = steep ? ax : ay, du = steep ? dy : dx, dv = steep ? dx : dy;
  if (du < 0) { au += du; av += dv; du = -du; dv = -dv; } // u increasing
  long L = isqrt((long)dx * dx + (long)dy * dy);
  if (!du) du = 1;
  int hw = (int)((long)th * 8 * (L ? L : 1) / du); // (th/2) * L/du, 1/16 -> 1/256 px
  int ext = th / 4;
  // pixel lines i (centers at 16 i + 8) within [au - ext, au + du + ext]
  int i0 = (au - ext - 8 + 15 + 4096) / 16 - 256, i1 = (au + du + ext - 8 + 4096) / 16 - 256;
  int lo0 = steep ? ui_cy0 : ui_cx0, hi0 = steep ? ui_cy1 : ui_cx1; // the clip along u
  int vlo = steep ? ui_cx0 : ui_cy0, vhi = steep ? ui_cx1 : ui_cy1;  // and along v
  if (i0 < lo0) i0 = lo0;
  if (i1 >= hi0) i1 = hi0 - 1;
  int step = (int)((long)dv * 256 / du);                            // v per line, 1/256 px
  int vc = (int)((long)av * 16 + (long)(i0 * 16 + 8 - au) * dv * 16 / du);
  for (int i = i0; i <= i1; ++i, vc += step) {
    int lo = vc - hw, hi = vc + hw, p0 = lo >> 8, p1 = (hi - 1) >> 8;
    if (p0 < vlo) p0 = vlo;
    if (p1 >= vhi) p1 = vhi - 1;
    unsigned char * q = steep ? ui_fb + i * UI_W + p0 : ui_fb + p0 * UI_W + i;
    for (int px = p0; px <= p1; ++px, q += steep ? 1 : UI_W) {
      int c0 = px << 8, c1 = c0 + 256, cov = (hi < c1 ? hi : c1) - (lo > c0 ? lo : c0);
      int lv = cov >= 200 ? 3 : cov >= 108 ? 2 : cov >= 36 ? 1 : 0;
      if (lv) {
        unsigned char o = *q;
        int ol = o == ramp[3] ? 3 : o == ramp[2] ? 2 : o == ramp[1] ? 1 : 0;
        if (lv > ol) *q = ramp[lv];
      }
    }
  }
}

void ui_poly16(const int * xy, int n, int th, const unsigned char * ramp) {
  for (int i = 0; i + 1 < n; ++i) ui_seg16(xy[2 * i], xy[2 * i + 1], xy[2 * i + 2], xy[2 * i + 3], th, ramp);
}
