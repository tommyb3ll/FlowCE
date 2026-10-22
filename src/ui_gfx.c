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
static char band_busy, strip_given; // a band is being drawn; the strip went to a calculation

void ui_clip(int x0, int y0, int x1, int y1) {
  ui_cx0 = x0 < 0 ? 0 : x0; ui_cy0 = y0 < band_y0 ? band_y0 : y0;
  ui_cx1 = x1 > UI_W ? UI_W : x1; ui_cy1 = y1 > band_y1 ? band_y1 : y1;
}
void ui_noclip(void) { ui_cx0 = 0; ui_cy0 = band_y0; ui_cx1 = UI_W; ui_cy1 = band_y1; }

// The strip is allocated once, at its full size, and kept: freed and allocated again around
// every repaint, it came back each time from fresh heap (small allocations took the freed space
// in between), and a session of calculations ran the heap out (KhiCAS aborts then: the user's
// crashes after a while, 2026-10-06). Its 20 KB are not lost to calculations, though: when the
// heap runs out, the allocator takes it first (ui_strip_release), before the out-of-memory stop;
// the screen is drawn directly until the calculation ends (ui_strip_rearm).
enum { STRIP_ROWS = 64 };
int ui_band_open(int maxrows) {
  (void)maxrows;
  if (!strip && !strip_given)
    for (int r = STRIP_ROWS; r >= 8 && !strip; r /= 2)
      if ((strip = (unsigned char *)malloc(UI_W * r))) strip_rows = r;
  return strip_rows;
}
void ui_band_close(void) {}
int ui_strip_release(void) {
  if (!strip || band_busy) return 0;
  free(strip);
  strip = 0; strip_rows = 0; strip_given = 1;
  return 1;
}
void ui_strip_rearm(void) { strip_given = 0; }
unsigned ui_strip_size(void) { return strip && !band_busy ? (unsigned)UI_W * strip_rows : 0; }
void ui_band_begin(int y0, int y1) {
  band_busy = 1;
  band_y0 = y0; band_y1 = y1;
  ui_fb = strip - y0 * UI_W; // screen row y lands at strip row y - y0
  ui_noclip();
}
void ui_band_load(void) { memcpy(strip, FB + band_y0 * UI_W, (band_y1 - band_y0) * UI_W); }
void ui_band_end(void) {
  memcpy(FB + band_y0 * UI_W, strip, (band_y1 - band_y0) * UI_W);
  ui_fb = FB; band_y0 = 0; band_y1 = UI_H;
  ui_noclip();
  band_busy = 0;
}

#ifdef FLOWCE_TILOOK
// the TI-84 look's only theme: the calculator's own colors (white screen, black text and
// selections, its gray status bar with white text, quiet gray F-key labels), no dimming
static const ui_theme T_ti = {{
  {255, 255, 255}, {0, 0, 0}, {81, 85, 81}, {160, 160, 160}, {0, 0, 0}, {222, 222, 222},
  {255, 255, 255}, {231, 227, 231}, {255, 255, 255}, {81, 85, 81}, {0, 158, 0}, {180, 180, 180},
  {255, 255, 255}, {255, 0, 0}, {255, 0, 255}, {255, 128, 0}, {0, 0, 255}, {255, 255, 255},
  {81, 85, 81}, {255, 255, 255}, {199, 195, 199}, {255, 255, 255}, {255, 255, 255}, {255, 255, 255}},
  {255, 255, 255}, 0};
#define TH0 &T_ti // (the starting theme)
const ui_theme * const ui_themes[UI_NTHEMES] = {TH0, TH0, TH0, TH0, TH0, TH0, TH0, TH0, TH0};
const char * const ui_theme_names[UI_NTHEMES] = {"TI-84", "TI-84", "TI-84", "TI-84", "TI-84", "TI-84", "TI-84", "TI-84", "TI-84"};
#else
// Paper: the prototype's light theme, FlowCE's blue accent; the status bar is the background (its
// slots UC_HEAD...: DeltaCE's colored bar), the F-key bar white
const ui_theme ui_theme_paper = {{
  {247, 248, 250}, {17, 20, 25}, {108, 117, 129}, {219, 224, 231}, {36, 92, 204}, {226, 234, 250},
  {255, 255, 255}, {255, 255, 255}, {255, 255, 255}, {78, 86, 98}, {39, 140, 74}, {208, 214, 222},
  {255, 255, 255}, {199, 68, 64}, {112, 72, 184}, {226, 118, 24}, {36, 92, 204}, {247, 248, 250},
  {247, 248, 250}, {36, 92, 204}, {108, 117, 129}, {219, 224, 231}, {236, 240, 246}, {255, 255, 255}},
  {30, 36, 46}, 36};
// Night: the prototype's dark theme (UC_WHITE, text on a colored chip: dark here)
const ui_theme ui_theme_night = {{
  {12, 15, 20}, {234, 238, 243}, {134, 145, 158}, {38, 46, 58}, {100, 165, 255}, {24, 40, 64},
  {7, 11, 17}, {22, 28, 36}, {16, 21, 28}, {150, 160, 173}, {86, 200, 130}, {4, 6, 9},
  {7, 11, 17}, {255, 112, 102}, {182, 146, 255}, {255, 170, 82}, {100, 165, 255}, {12, 15, 20},
  {12, 15, 20}, {100, 165, 255}, {134, 145, 158}, {38, 46, 58}, {33, 41, 53}, {16, 21, 28}},
  {0, 0, 0}, 50};

// more > Themes: seven more, after well-liked palettes (Solarized, Nord, Gruvbox, Rose Pine Dawn,
// Dracula), each slot as Paper's or Night's: Sand (cream, burnt orange), Mint (teal), Arctic (blue
// slate, frost), Ember (brown, orange), Rose (blush, rose), Grape (plum, purple), Forest (green).
// Their surfaces are tinted more than the palettes they come from: the calculator's LCD has 32
// levels per channel and squashes shades near white and black, so backgrounds 1-3 levels apart
// looked alike there. The levels were picked on a calculator (tools/dev/theme_tints.py).
static const ui_theme T_sand = {{
  {246, 225, 174}, {0, 39, 49}, {89, 110, 118}, {225, 203, 150}, {207, 71, 4}, {232, 210, 186},
  {255, 255, 255}, {252, 241, 214}, {252, 241, 214}, {82, 104, 111}, {133, 153, 5}, {215, 191, 133},
  {255, 255, 255}, {229, 21, 31}, {107, 111, 204}, {181, 137, 6}, {13, 139, 216}, {246, 225, 174},
  {246, 225, 174}, {207, 71, 4}, {89, 110, 118}, {225, 203, 150}, {236, 214, 159}, {248, 230, 187}},
  {0, 43, 54}, 36};
static const ui_theme T_mint = {{
  {198, 244, 223}, {4, 18, 13}, {88, 107, 98}, {167, 223, 199}, {3, 128, 112}, {201, 226, 219},
  {255, 255, 255}, {232, 253, 244}, {232, 253, 244}, {66, 88, 79}, {25, 142, 53}, {157, 213, 189},
  {255, 255, 255}, {204, 52, 66}, {118, 77, 195}, {217, 114, 9}, {28, 98, 211}, {198, 244, 223},
  {198, 244, 223}, {3, 128, 112}, {88, 107, 98}, {167, 223, 199}, {186, 238, 215}, {209, 252, 232}},
  {20, 38, 32}, 36};
static const ui_theme T_arctic = {{
  {37, 75, 106}, {254, 255, 255}, {202, 212, 230}, {59, 101, 138}, {119, 195, 216}, {73, 102, 121},
  {36, 41, 51}, {51, 90, 125}, {43, 83, 116}, {202, 212, 230}, {158, 193, 127}, {19, 56, 82},
  {36, 41, 51}, {206, 83, 99}, {187, 137, 178}, {222, 127, 97}, {120, 162, 203}, {37, 75, 106},
  {37, 75, 106}, {119, 195, 216}, {200, 210, 228}, {59, 101, 138}, {63, 105, 144}, {43, 83, 116}},
  {0, 0, 0}, 45};
static const ui_theme T_ember = {{
  {71, 51, 39}, {255, 247, 226}, {190, 175, 153}, {114, 85, 67}, {253, 128, 29}, {98, 68, 42},
  {40, 40, 40}, {82, 59, 45}, {77, 55, 41}, {188, 173, 151}, {184, 187, 9}, {51, 32, 20},
  {40, 40, 40}, {253, 70, 49}, {216, 130, 154}, {253, 188, 21}, {128, 166, 152}, {71, 51, 39},
  {71, 51, 39}, {253, 128, 29}, {187, 172, 150}, {114, 85, 67}, {97, 71, 55}, {77, 55, 41}},
  {0, 0, 0}, 50};
static const ui_theme T_rose = {{
  {255, 213, 217}, {57, 50, 89}, {106, 102, 131}, {246, 199, 200}, {185, 95, 121}, {228, 208, 210},
  {255, 255, 255}, {255, 236, 235}, {255, 236, 235}, {103, 99, 127}, {27, 106, 135}, {236, 187, 186},
  {255, 255, 255}, {221, 126, 122}, {145, 120, 173}, {240, 154, 3}, {78, 149, 162}, {255, 213, 217},
  {255, 213, 217}, {185, 95, 121}, {107, 103, 132}, {246, 199, 200}, {252, 204, 206}, {255, 221, 224}},
  {40, 34, 52}, 36};
static const ui_theme T_grape = {{
  {67, 47, 76}, {255, 255, 253}, {167, 178, 223}, {97, 77, 113}, {190, 145, 253}, {80, 68, 119},
  {40, 42, 54}, {78, 57, 90}, {72, 52, 83}, {165, 176, 221}, {29, 254, 108}, {49, 29, 53},
  {40, 42, 54}, {255, 85, 85}, {255, 121, 198}, {255, 184, 108}, {136, 233, 254}, {67, 47, 76},
  {67, 47, 76}, {190, 145, 253}, {165, 175, 221}, {97, 77, 113}, {97, 77, 113}, {72, 52, 83}},
  {0, 0, 0}, 50};
static const ui_theme T_forest = {{
  {21, 47, 31}, {252, 255, 254}, {163, 188, 174}, {49, 85, 63}, {83, 212, 143}, {43, 80, 62},
  {14, 22, 18}, {30, 60, 41}, {25, 54, 36}, {157, 182, 169}, {150, 209, 78}, {11, 33, 19},
  {14, 22, 18}, {254, 99, 112}, {188, 151, 254}, {253, 159, 103}, {118, 161, 255}, {21, 47, 31},
  {21, 47, 31}, {83, 212, 143}, {154, 179, 165}, {49, 85, 63}, {43, 77, 56}, {25, 54, 36}},
  {0, 0, 0}, 50};
const ui_theme * const ui_themes[UI_NTHEMES] = {&ui_theme_paper, &ui_theme_night, &T_sand, &T_mint, &T_arctic, &T_ember, &T_rose, &T_grape, &T_forest};
const char * const ui_theme_names[UI_NTHEMES] = {"Paper", "Night", "Sand", "Mint", "Arctic", "Ember", "Rose", "Grape", "Forest"};
#define TH0 &ui_theme_paper // (the starting theme)
#endif

// bank 0 (the scene) is entries 128 to 128 + BANK0 - 1, bank 1 (popovers) the rest. Ramps shown at
// once (2.0, counted on the emulator): the scene's screens 14 at most (the action row; history and
// search 12, the graph 9), the cards 11 (an application's form; the graph's table 10): 23 and 17
enum { BANK0 = 70, MAXR = (BANK0 - UC_COUNT) / 2, MAXR1 = (128 - BANK0 - UC_COUNT) / 2 };
static const ui_theme * TH = TH0;
static int dimmed;
static unsigned char rfg[2][MAXR], rbg[2][MAXR], nr[2];
static unsigned char ridx[2][MAXR][4];

// the LCD palette format in 8 bpp mode: 1555 (x RRRRR GGGGG BBBBB), as graphx's gfx_RGBTo1555.
// (graphic.c writes 565 values for KhiCAS's own 0-127: fine for its saturated cube colors,
// visibly wrong for the mid grays of anti-aliasing.)
static unsigned short rgb565(int r, int g, int b) {
#ifdef FLOWCE_TILOOK // the x bit is green's 6th bit: the OS's white and grays exactly
  return (unsigned short)(((g & 4) << 13) | ((r & 0xf8) << 7) | ((g & 0xf8) << 2) | (b >> 3));
#else
  return (unsigned short)(((r & 0xf8) << 7) | ((g & 0xf8) << 2) | (b >> 3));
#endif
}
// slot s of bank: base color s < UC_COUNT, else mid (s - UC_COUNT) & 1 of ramp (s - UC_COUNT) / 2
static void base_rgb(int s, int * c) { // a theme color; UC_DIM, UC_DIMBAR: the background, the bar dimmed
  const int dim = s == UC_DIM || s == UC_DIMBAR;
  const unsigned char * a = TH->rgb[s == UC_DIM ? UC_BG : s == UC_DIMBAR ? UC_BAR : s];
  for (int i = 0; i < 3; ++i) c[i] = a[i] + (dim ? ((TH->dim_to[i] - a[i]) * TH->dim_pct) / 100 : 0);
}
static void slot_rgb(int bank, int s, int * c) {
  if (s < UC_COUNT) base_rgb(s, c);
  else {
    int k = (s - UC_COUNT) >> 1, a[3], b[3];
    base_rgb(rbg[bank][k], a); base_rgb(rfg[bank][k], b);
    int t = ((s - UC_COUNT) & 1) ? 2 : 1; // 1/3 or 2/3 of the way to the ink
    for (int i = 0; i < 3; ++i) c[i] = a[i] + ((b[i] - a[i]) * t) / 3;
  }
}
// the colors as worked out when a slot is written (bank 0 plain and dimmed): ui_dim and ui_set_theme
// copy them in (working them out, divisions are helper calls, took ~10 ms each time: on every
// display update, and when a card opened or closed)
static unsigned short pal0[2][BANK0], pal1[128 - BANK0];
static void write_slot(int bank, int s) {
  int c[3];
  slot_rgb(bank, s, c);
  if (bank == 0) {
    pal0[0][s] = rgb565(c[0], c[1], c[2]);
    for (int i = 0; i < 3; ++i) c[i] += ((TH->dim_to[i] - c[i]) * TH->dim_pct) / 100;
    pal0[1][s] = rgb565(c[0], c[1], c[2]);
    PALETTE[128 + s] = pal0[dimmed][s];
  } else PALETTE[128 + BANK0 + s] = pal1[s] = rgb565(c[0], c[1], c[2]);
}
static void write_bank(int bank) {
  for (int s = 0; s < UC_COUNT + 2 * nr[bank]; ++s) write_slot(bank, s);
}

// the same theme: its colors copied in again (anything may have reset the palette)
void ui_set_theme(const ui_theme * t) {
  static const ui_theme * done;
  if (t == done) {
    memcpy((void *)&PALETTE[128], pal0[dimmed], 2 * (UC_COUNT + 2 * nr[0]));
    memcpy((void *)&PALETTE[128 + BANK0], pal1, 2 * (UC_COUNT + 2 * nr[1]));
    return;
  }
  TH = done = t;
  write_bank(0); write_bank(1);
}
const ui_theme * ui_theme_now(void) { return TH; }
// entries 32-127 lent to a card (their colors back at ui_borrow_end)
static unsigned short lent[96];
static unsigned char lent_on[96];
void ui_borrow(int i, const unsigned char * c) {
  const int k = i - 32;
  if (k < 0 || k >= 96) return;
  if (!lent_on[k]) { lent_on[k] = 1; lent[k] = PALETTE[i]; }
  PALETTE[i] = rgb565(c[0], c[1], c[2]);
}
void ui_borrow_mix(int i, const unsigned char * a, const unsigned char * b, int t) {
  unsigned char c[3];
  for (int j = 0; j < 3; ++j) c[j] = (unsigned char)(a[j] + (b[j] - a[j]) * t / 3);
  ui_borrow(i, c);
}
void ui_borrow_end(void) {
  for (int k = 0; k < 96; ++k) if (lent_on[k]) { PALETTE[32 + k] = lent[k]; lent_on[k] = 0; }
}
void ui_dim(int on) {
  if (dimmed == on) return;
  dimmed = on;
  memcpy((void *)&PALETTE[128], pal0[on], 2 * (UC_COUNT + 2 * nr[0]));
}
void ui_restore_os_palette(void) {}
unsigned char ui_col(int bank, int c) { return (unsigned char)(128 + bank * BANK0 + c); }

// (DeltaCE) Ramps are not freed: a session's screens used 28 of the 30. A full table reuses a ramp
// whose two mids show nowhere (the screen, the band being drawn) and that was not handed out
// among the last four (callers draw with the ramps they just got); none: the last one, as before.
// The screen scan (76,800 bytes, about 20 ms) happens only then. UI_MAXR_TEST: a smaller table.
#ifdef UI_MAXR_TEST
#define RMAX(b) ((b) ? (MAXR1 < UI_MAXR_TEST ? MAXR1 : UI_MAXR_TEST) : UI_MAXR_TEST)
#else
#define RMAX(b) ((b) ? MAXR1 : MAXR)
#endif
static unsigned char rrec[2][4], rrpos[2]; // the ramps handed out last
static void rtouch(int bank, int k) { rrec[bank][rrpos[bank]++ & 3] = (unsigned char)(k + 1); }
static int revict(int bank) {
  unsigned char seen[256];
  memset(seen, 0, sizeof seen);
  for (const unsigned char * p = FB, * e = FB + UI_W * UI_H; p < e; ++p) seen[*p] = 1;
  if (band_busy && strip)
    for (const unsigned char * p = strip, * e = strip + UI_W * (band_y1 - band_y0); p < e; ++p) seen[*p] = 1;
  const int mr = RMAX(bank);
  for (int k = mr - 1; k >= 0; --k) {
    const int m = 128 + bank * BANK0 + UC_COUNT + 2 * k;
    if (seen[m] || seen[m + 1]) continue;
    int r = 0;
    for (int j = 0; j < 4; ++j) r |= rrec[bank][j] == k + 1;
    if (!r) return k;
  }
  return mr - 1;
}
static unsigned char lfg[2] = {255, 255}, lbg[2], lk[2]; // the ramp handed out last (drawing loops ask again and again)
const unsigned char * ui_ramp(int bank, int fg, int bg) {
  int k;
  if (fg == lfg[bank] && bg == lbg[bank]) return ridx[bank][lk[bank]]; // (already the latest touched)
  for (k = 0; k < nr[bank]; ++k)
    if (rfg[bank][k] == fg && rbg[bank][k] == bg) { rtouch(bank, k); lfg[bank] = (unsigned char)fg; lbg[bank] = (unsigned char)bg; lk[bank] = (unsigned char)k; return ridx[bank][k]; }
  if (k >= RMAX(bank)) k = revict(bank); // full: one no pixel uses
  else ++nr[bank];
  rtouch(bank, k);
  lfg[bank] = (unsigned char)fg; lbg[bank] = (unsigned char)bg; lk[bank] = (unsigned char)k;
  rfg[bank][k] = fg; rbg[bank][k] = bg;
  unsigned char * r = ridx[bank][k];
  r[0] = ui_col(bank, bg); r[3] = ui_col(bank, fg);
  r[1] = (unsigned char)(128 + bank * BANK0 + UC_COUNT + 2 * k);
  r[2] = (unsigned char)(r[1] + 1);
  write_slot(bank, UC_COUNT + 2 * k); write_slot(bank, UC_COUNT + 2 * k + 1);
  return r;
}

void ui_fill(int x, int y, int w, int h, unsigned char idx) {
  int x0 = x < ui_cx0 ? ui_cx0 : x, x1 = x + w > ui_cx1 ? ui_cx1 : x + w;
  int y0 = y < ui_cy0 ? ui_cy0 : y, y1 = y + h > ui_cy1 ? ui_cy1 : y + h;
  if (x1 <= x0 || y1 <= y0) return;
  // the row's address once (a multiplication is a helper call on the eZ80), then UI_W further per
  // row; a column (the graph's lines) without memset, the full width in one
  unsigned char * p = ui_fb + y0 * UI_W + x0;
  const int n = x1 - x0;
  if (n == UI_W) { memset(p, idx, (size_t)(y1 - y0) * UI_W); return; }
  if (n == 1) { for (; y0 < y1; ++y0, p += UI_W) *p = idx; return; }
  for (; y0 < y1; ++y0, p += UI_W) memset(p, idx, n);
}

static int isqrt(long v) { // floor(sqrt(v)), v < 2^31
  long r = 0, b = 1L << 30;
  while (b > v) b >>= 2;
  while (b) { if (v >= r + b) { v -= r + b; r = (r >> 1) + b; } else r >>= 1; b >>= 2; }
  return (int)r;
}
static int isqrt24(unsigned v) { // the same for v < 2^23, in 24-bit integers (cheaper helpers)
  unsigned r = 0, b = 1u << 22;
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
// The shades of a corner box (r x r, the top left one: the others are its mirror images) by
// distance to the arc, 24-bit integers only (r <= 60); ring 1: the 1 px arc, 2: the box but the
// disc (ui_fillet). Worked out once per radius and kind and kept (a few in use: tiles, cards,
// chips): the arithmetic per pixel was a third of a menu's opening.
static void corner_shades(unsigned char * t, int r, int ring) {
  int R = r * 8, lim[3], lim1[3];
  disc_limits(R, lim);
  disc_limits(R - 8, lim1);
  for (int j = 0; j < r; ++j) {
    // per row d2, then additions only: a multiplication or a 24-bit shift is a helper call on the eZ80
    int ey = j * 8 + 4 - R, ex = 4 - R, d2 = ex * ex + ey * ey, step = 16 * ex + 64;
    for (int i = 0; i < r; ++i, ++t) {
      // a 1 px ring is the disc of radius r minus the disc of radius r - 1
      int lv = ring == 1 ? disc_level(d2, lim) - disc_level(d2, lim1) : disc_level(d2, lim);
      *t = (unsigned char)(ring == 2 ? 3 - lv : lv);
      d2 += step; step += 128; // d2 = ex^2 + ey^2, ex += 8 per pixel
    }
  }
}
enum { CT_N = 16 };
static unsigned char * ct_tab[CT_N], ct_key[CT_N], ct_next; // (kept: r | ring << 6)
// the table (r * r + 2 r bytes): the r x r shades, then (ring 0: a disc's corner, its shades rising
// along a row) per row the first pixel shaded and the first full one
static void corner_build(unsigned char * t, int r, int ring) {
  corner_shades(t, r, ring);
  const unsigned char * s = t;
  unsigned char * za = t + r * r;
  for (int j = 0; j < r; ++j, s += r, za += 2) {
    int z = 0, a;
    while (z < r && !s[z]) ++z;
    for (a = z; a < r && s[a] < 3; ++a) ;
    za[0] = (unsigned char)z; za[1] = (unsigned char)a;
  }
}
static const unsigned char * corner_table(int r, int ring) {
  const unsigned char key = (unsigned char)(r | ring << 6);
  for (int i = 0; i < CT_N; ++i) if (ct_tab[i] && ct_key[i] == key) return ct_tab[i];
  const int i = ct_next;
  ct_next = (unsigned char)(i + 1 == CT_N ? 0 : i + 1);
  free(ct_tab[i]);
  unsigned char * t = (unsigned char *)malloc(r * r + 2 * r);
  if (!(ct_tab[i] = t)) return 0;
  ct_key[i] = key;
  corner_build(t, r, ring);
  return t;
}
// (keeps a darker shade of the ramp already there)
static void shade(unsigned char * p, int lv, const unsigned char * ramp) {
  const unsigned char o = *p;
  const int ol = o == ramp[3] ? 3 : o == ramp[2] ? 2 : o == ramp[1] ? 1 : 0;
  if (lv > ol) *p = ramp[lv];
}
// the corners of a 1 px rounded frame (ring 1) or of a fillet (ring 2): their r x r boxes from the
// table, inside the clip
static void corners(int x, int y, int w, int h, int r, const unsigned char * ramp, int ring) {
  if (r <= 0 || r > 60) return;
  unsigned char tmp[8 * 8 + 16];
  const unsigned char * t = corner_table(r, ring);
  if (!t) { if (r > 8) return; corner_build(tmp, r, ring); t = tmp; } // (no memory: worked out here)
  for (int q = 0; q < 4; ++q) {
    const int bx = (q & 1) ? x + w - r : x, by = (q & 2) ? y + h - r : y;
    int j0 = ui_cy0 - by, j1 = ui_cy1 - by, i0 = ui_cx0 - bx, i1 = ui_cx1 - bx; // (the box inside the clip)
    if (j0 < 0) j0 = 0;
    if (j1 > r) j1 = r;
    if (i0 < 0) i0 = 0;
    if (i1 > r) i1 = r;
    if (i0 >= i1) continue;
    unsigned char * row = ui_fb + (by + j0) * UI_W + bx;
    for (int j = j0; j < j1; ++j, row += UI_W) {
      const unsigned char * s = t + ((q & 2) ? r - 1 - j : j) * r;
      for (int i = i0; i < i1; ++i) {
        const int lv = s[(q & 1) ? r - 1 - i : i];
        if (lv) shade(row + i, lv, ramp);
      }
    }
  }
}

// one corner row of a filled rounded rectangle [x, x + w): its run from a to w - a, the shaded edge
// pixels [z, a) at both ends (s: the table's row, za: its z and a)
static void rr_row(unsigned char * row, int x, int w, const unsigned char * s, const unsigned char * za, const unsigned char * ramp) {
  const int z = za[0], a = za[1], cx0 = ui_cx0, cx1 = ui_cx1;
  int f0 = x + a, f1 = x + w - a;
  if (f0 < cx0) f0 = cx0;
  if (f1 > cx1) f1 = cx1;
  if (f1 > f0) memset(row + f0, ramp[3], f1 - f0);
  for (int i = z; i < a; ++i) {
    const int lv = s[i], l = x + i, rt = x + w - 1 - i;
    if (!lv) continue;
    if (l >= cx0 && l < cx1) shade(row + l, lv, ramp);
    if (rt >= cx0 && rt < cx1) shade(row + rt, lv, ramp);
  }
}
void ui_rrect(int bank, int x, int y, int w, int h, int r, int c, int bg) { ui_rrect_r(x, y, w, h, r, ui_ramp(bank, c, bg)); }
// by rows, inside the clip: the top corners' rows, the middle ones (one run each), the bottom
// corners' (their table rows upward). (Its fills and corners apart made ~100 memset calls a tile.)
void ui_rrect_r(int x, int y, int w, int h, int r, const unsigned char * ramp) {
  if (w <= 0 || h <= 0) return;
  if (r * 2 > h) r = h / 2;
  if (r * 2 > w) r = w / 2;
  if (r > 60) r = 60;
  unsigned char tmp[8 * 8 + 16];
  const unsigned char * t = r > 0 ? corner_table(r, 0) : 0;
  if (r > 0 && !t) { if (r > 8) r = 8; corner_build(tmp, r, 0); t = tmp; } // (no memory: worked out here)
  int j = ui_cy0 - y, j1 = ui_cy1 - y; // the rows and columns inside the clip (it may be empty)
  if (j < 0) j = 0;
  if (j1 > h) j1 = h;
  const int f0 = x < ui_cx0 ? ui_cx0 : x, n = (x + w > ui_cx1 ? ui_cx1 : x + w) - f0;
  if (j >= j1 || n <= 0) return;
  unsigned char * row = ui_fb + (y + j) * UI_W;
  if (j < r)
    for (const unsigned char * s = t + j * r, * za = t + r * r + 2 * j; j < r && j < j1; ++j, row += UI_W, s += r, za += 2)
      rr_row(row, x, w, s, za, ramp);
  const int m1 = h - r < j1 ? h - r : j1;
  for (; j < m1; ++j, row += UI_W) memset(row + f0, ramp[3], n);
  // the bottom corners' rows from the last one up, the table's rows downward. (Walked downward with
  // s -= r, clang 19 (CE toolchain 15.0) counted the rows from a subtraction that kept the borrow
  // of -r: one row short, and none left for a 3 px high tile: 16 M rows, a reset.)
  if (j < j1) {
    const int k = h - j1; // the table row of the last row drawn
    row += (j1 - 1 - j) * UI_W;
    for (const unsigned char * s = t + k * r, * za = t + r * r + 2 * k; j < j1; ++j, row -= UI_W, s += r, za += 2)
      rr_row(row, x, w, s, za, ramp);
  }
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

void ui_fillet(int bank, int x, int y, int r, int q, int c, int bg) {
  // the box is a corner box of a 2r x 2r square whose corner arc is centered at its corner q (the
  // other boxes clipped away)
  const int c0 = ui_cx0, c1 = ui_cx1, r0 = ui_cy0, r1 = ui_cy1;
  if (x > ui_cx0) ui_cx0 = x;
  if (x + r < ui_cx1) ui_cx1 = x + r;
  if (y > ui_cy0) ui_cy0 = y;
  if (y + r < ui_cy1) ui_cy1 = y + r;
  corners(q & 1 ? x : x - r, q & 2 ? y : y - r, 2 * r, 2 * r, r, ui_ramp(bank, c, bg), 2);
  ui_cx0 = c0; ui_cx1 = c1; ui_cy0 = r0; ui_cy1 = r1;
}

// A thick anti-aliased segment by spans: along its major axis (rows for a steep segment, columns
// for a flat one) each pixel line crosses the stroke over [v - hw, v + hw] (v: the center line,
// hw: half the stroke width measured along the line; 1/256 px), and each pixel is shaded by the
// part of it that span covers (the font's quantization). Per pixel only 24-bit additions and
// compares; the ends are cut along the lines, extended a quarter stroke so polylines join.
void ui_seg16(int ax, int ay, int bx, int by, int th, const unsigned char * ramp) {
  int dx = bx - ax, dy = by - ay, adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy, steep = ady >= adx;
  // u increasing: from the other end. (Negating du and dv in turn, clang 19 kept the first's borrow
  // in the second: -dv-2. tools/dev/v15_carrycheck.py refused that build.)
  if (steep ? dy < 0 : dx < 0) { const int tx = ax, ty = ay; ax = bx; ay = by; dx = tx - ax; dy = ty - ay; }
  const int au = steep ? ay : ax, av = steep ? ax : ay, dv = steep ? dx : dy;
  int du = steep ? dy : dx;
  // a short segment (the math signs', the bar's icons): its values in 24-bit integers, the same
  // ones (32-bit multiplications, divisions and the square root were slow helper calls: ~19 ms an
  // integral sign at the steps' size)
  const int small = adx < 2048 && ady < 2048 && th < 64 && av > -16384 && av < 16384;
  const int L = small ? isqrt24((unsigned)(dx * dx + dy * dy)) : isqrt((long)dx * dx + (long)dy * dy);
  if (!du) du = 1;
  int hw = small ? th * 8 * (L ? L : 1) / du : (int)((long)th * 8 * (L ? L : 1) / du); // (th/2) * L/du, 1/16 -> 1/256 px
  int ext = th / 4;
  // pixel lines i (centers at 16 i + 8) within [au - ext, au + du + ext]
  int i0 = (au - ext - 8 + 15 + 4096) / 16 - 256, i1 = (au + du + ext - 8 + 4096) / 16 - 256;
  int lo0 = steep ? ui_cy0 : ui_cx0, hi0 = steep ? ui_cy1 : ui_cx1; // the clip along u
  int vlo = steep ? ui_cx0 : ui_cy0, vhi = steep ? ui_cx1 : ui_cy1;  // and along v
  if (i0 < lo0) i0 = lo0;
  if (i1 >= hi0) i1 = hi0 - 1;
  int step = small ? dv * 256 / du : (int)((long)dv * 256 / du);   // v per line, 1/256 px
  const int d0 = i0 * 16 + 8 - au; // (the first line drawn from the start: far when clipped)
  int vc = small && d0 > -128 && d0 < 128 ? av * 16 + d0 * dv * 16 / du : (int)((long)av * 16 + (long)d0 * dv * 16 / du);
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
