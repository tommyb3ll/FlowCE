// ui_gfx.c - palette banks, ramps and anti-aliased primitives of the Focus interface (ui_gfx.h).
#include <string.h>
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

void ui_clip(int x0, int y0, int x1, int y1) {
  ui_cx0 = x0 < 0 ? 0 : x0; ui_cy0 = y0 < 0 ? 0 : y0;
  ui_cx1 = x1 > UI_W ? UI_W : x1; ui_cy1 = y1 > UI_H ? UI_H : y1;
}
void ui_noclip(void) { ui_cx0 = ui_cy0 = 0; ui_cx1 = UI_W; ui_cy1 = UI_H; }

// Paper: the prototype's light theme
const ui_theme ui_theme_paper = {{
  {247, 248, 250}, {17, 20, 25}, {108, 117, 129}, {219, 224, 231}, {36, 92, 204}, {226, 234, 250},
  {255, 255, 255}, {255, 255, 255}, {255, 255, 255}, {78, 86, 98}, {39, 140, 74}, {208, 214, 222},
  {255, 255, 255}}, {30, 36, 46}, 36};

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
static void slot_rgb(int bank, int s, int * c) {
  const unsigned char * a, * b;
  int k;
  if (s < UC_COUNT) { a = TH->rgb[s]; c[0] = a[0]; c[1] = a[1]; c[2] = a[2]; }
  else {
    k = (s - UC_COUNT) >> 1;
    a = TH->rgb[rbg[bank][k]]; b = TH->rgb[rfg[bank][k]];
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

static void put(int x, int y, int lv, const unsigned char * r) { // keeps the darker shade
  if (lv <= 0 || x < ui_cx0 || x >= ui_cx1 || y < ui_cy0 || y >= ui_cy1) return;
  unsigned char * p = ui_fb + y * UI_W + x, o = *p;
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
// without the square root: d <= t <=> q < (R + t + 1)^2
static int disc_level(int q, int R) {
  int a = R - 1, b = R + 2, c = R + 4;
  return a > 0 && q < a * a ? 3 : b > 0 && q < b * b ? 2 : c > 0 && q < c * c ? 1 : 0;
}
// corners of a rounded rectangle: pixels of the r x r corner boxes by distance to the arc
// (24-bit integers only: r <= 60)
static void corners(int x, int y, int w, int h, int r, const unsigned char * ramp, int ring) {
  int R = r * 8;
  for (int q = 0; q < 4; ++q) {
    int bx = (q & 1) ? x + w - r : x, by = (q & 2) ? y + h - r : y;
    int cx8 = ((q & 1) ? x + w - r : x + r) * 8, cy8 = ((q & 2) ? y + h - r : y + r) * 8;
    for (int py = by; py < by + r; ++py) {
      int ey = py * 8 + 4 - cy8, ex = bx * 8 + 4 - cx8, d2 = ex * ex + ey * ey;
      for (int px = bx; px < bx + r; ++px) {
        // a 1 px ring is the disc of radius r minus the disc of radius r - 1
        int lv = ring ? disc_level(d2, R) - disc_level(d2, R - 8) : disc_level(d2, R);
        put(px, py, lv, ramp);
        d2 += 16 * ex + 64; ex += 8;
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

void ui_seg16(int ax, int ay, int bx, int by, int th, const unsigned char * ramp) {
  // work in 1/8 px so that the products below fit in 32 bits
  ax >>= 1; ay >>= 1; bx >>= 1; by >>= 1; th >>= 1;
  int half = th / 2, m = half + 4;
  int x0 = ((ax < bx ? ax : bx) - m) >> 3, x1 = ((ax > bx ? ax : bx) + m) >> 3;
  int y0 = ((ay < by ? ay : by) - m) >> 3, y1 = ((ay > by ? ay : by) + m) >> 3;
  if (x0 < ui_cx0) x0 = ui_cx0;
  if (y0 < ui_cy0) y0 = ui_cy0;
  if (x1 >= ui_cx1) x1 = ui_cx1 - 1;
  if (y1 >= ui_cy1) y1 = ui_cy1 - 1;
  long dx = bx - ax, dy = by - ay, len2 = dx * dx + dy * dy;
  long L = isqrt(len2);
  // shade thresholds on the distance d (1/8 px): 3 if d <= half-2, 2 if <= half+1, 1 if <= half+3;
  // compared without square roots or divisions: |cross| against T*L inside the segment, the
  // squared distance against T*T past its ends. Everything is updated by additions along a row.
  long t3 = half - 2, t2 = half + 1, t1 = half + 3;
  long c3 = t3 * L, c2 = t2 * L, c1 = t1 * L, s3 = t3 < 0 ? -1 : t3 * t3, s2 = t2 * t2, s1 = t1 * t1;
  for (int py = y0; py <= y1; ++py) {
    long cx = x0 * 8 + 4 - ax, cy = py * 8 + 4 - ay;
    long t = cx * dx + cy * dy, cr = cx * dy - cy * dx;
    long da = cx * cx + cy * cy, ex = cx - dx, ey = cy - dy, db = ex * ex + ey * ey;
    unsigned char * row = ui_fb + py * UI_W;
    for (int px = x0; px <= x1; ++px) {
      int lv;
      if (len2 == 0 || t <= 0) lv = da <= s3 ? 3 : da <= s2 ? 2 : da <= s1 ? 1 : 0;
      else if (t >= len2) lv = db <= s3 ? 3 : db <= s2 ? 2 : db <= s1 ? 1 : 0;
      else { long a = cr < 0 ? -cr : cr; lv = a <= c3 ? 3 : a <= c2 ? 2 : a <= c1 ? 1 : 0; }
      if (lv) {
        unsigned char * p = row + px, o = *p;
        int ol = o == ramp[3] ? 3 : o == ramp[2] ? 2 : o == ramp[1] ? 1 : 0;
        if (lv > ol) *p = ramp[lv];
      }
      t += 8 * dx; cr += 8 * dy;              // next pixel: cx += 8
      da += 16 * cx + 64; db += 16 * ex + 64; cx += 8; ex += 8;
    }
  }
}

void ui_poly16(const int * xy, int n, int th, const unsigned char * ramp) {
  for (int i = 0; i + 1 < n; ++i) ui_seg16(xy[2 * i], xy[2 * i + 1], xy[2 * i + 2], xy[2 * i + 3], th, ramp);
}
