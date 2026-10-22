// focus_menu.cc - the popovers of the Focus interface (focus.h; the prototype in notes/ui): F1-F5
// and the math key open a card of templates or actions over the stage, which dims at once (palette
// bank 0, no redraw) while the card is drawn bright in bank 1. Larger than the prototype's: rows
// of 30 px, labels at 12 px, previews at 17-20 px. Repaints go through bands loaded from the
// screen (ui_band_load), so moving the selection never shows a blank frame.
#include <string.h>
#include <time.h>              // clock (before console.h: giac's first.h defines a clock macro)
#include "console.h"           // Console_*, Line[]; maps std to ustl
#include "focus.h"
#include "k_defs.h"
#include "ui_gfx.h"
#include "ui_font.h"
#include "ui_fontdata.h"
#include "ui_math.h"
#include "mathedit.h"

extern "C" void GetKey(int * key); // k_csdk.c
const char * console_fkey_label(int layer, int k); // console.cc: an F-key's label (layer 0, 2nd, alpha)

// an item: a template (KhiCAS text, the caret moved back by `back` after insertion; empty
// arguments are drawn as boxes) or an action; pv: the preview when it differs from the template
struct fm_item { const char * text; signed char back; const char * pv; const char * label; const char * hint; char act; };
// kind: 0 a list (number, preview, label), 1 a grid of previews, 2 a grid of cards (a large
// preview over its label); wide: the last item is a bar across the card under the grid (its
// preview, label and hint in a row: the steps build's Applications)
struct fm_menu { char grid, cols, n; const fm_item * items; char wide; };

static const fm_item m_alg[] = {
  {"solve(,x)", 3, 0, "Solve for x", 0, 0},
  {"solve([,],[x,y])", 9, 0, "Solve a system", 0, 0}, // (DeltaCE: its steps, elimination)
  {"factor()", 1, 0, "Factor", 0, 0},
  {"expand()", 1, 0, "Expand", 0, 0},
  {"simplify()", 1, 0, "Simplify", 0, 0},
  {"partfrac()", 1, 0, "Partial fractions", 0, 0},
  {"subst(,x=)", 4, 0, "Value at x =", 0, 0}, // (DeltaCE; a fraction is the / key)
};
static const fm_item m_calc[] = {
  {"integrate(,x)", 3, 0, "Antiderivative", 0, 0},
  {"integrate(,x,,)", 5, 0, "Definite integral", 0, 0},
  {"diff(,x)", 3, 0, "Derivative", 0, 0},
  {"limit(,x,)", 4, 0, "Limit", 0, 0},
  {"sum(,k,,)", 5, 0, "Sum", 0, 0},
  {"series(,x,0,5)", 7, 0, "Taylor series", 0, 0},
  {"implicitdiff(,x,y)", 5, "(dy)/(dx)", "Implicit dy/dx", 0, 0}, // (DeltaCE: kmisc.cc, steps.cc)
#ifdef FLOWCE_STEPS // (the steps build: three by three, then Applications across the card: the 0 key)
  {"analyze()", 1, "f(x)", "Analyze f(x)", 0, 0}, // (DeltaCE: analyze.cc, the report on the steps' page)
  {"desolve(diff(y,x)=,x,y)", 5, "diff(y,x)=f", "Diff. equation", 0, 0},
  {0, 0, "integrate(f-g,x,a,b)", "Applications", "area, volume, arc length, more", FA_APPS},
#endif
};
#ifdef FLOWCE_STEPS
// the calculus card's applications (steps.cc): each card shows its integral; a, b left empty: where
// the curves meet
static const fm_item m_apps[] = { // (each opens its form: app_form)
  {0, 0, "integrate(f-g,x,a,b)", "Area between curves", 0, 0},
  {0, 0, "pi*integrate(R^2-r^2,x,a,b)", "Disks / washers", 0, 0},
  {0, 0, "2*pi*integrate(r*h,x,a,b)", "Shells", 0, 0},
  {0, 0, "integrate(sqrt(1+(dy/dx)^2),x,a,b)", "Arc length", 0, 0},
  {0, 0, "integrate(2*pi*r,s)", "Surface area", 0, 0},
  {0, 0, "1/(b-a)*integrate(f,x,a,b)", "Average value", 0, 0},
};
#endif
static const fm_item m_trig[] = {
  {"sin()", 1, 0, 0, 0, 0}, {"cos()", 1, 0, 0, 0, 0}, {"tan()", 1, 0, 0, 0, 0},
  {"asin()", 1, 0, 0, 0, 0}, {"acos()", 1, 0, 0, 0, 0}, {"atan()", 1, 0, 0, 0, 0},
  {"sec()", 1, 0, 0, 0, 0}, {"csc()", 1, 0, 0, 0, 0}, {"cot()", 1, 0, 0, 0, 0},
  {"sinh()", 1, 0, 0, 0, 0}, {"cosh()", 1, 0, 0, 0, 0}, {"tanh()", 1, 0, 0, 0, 0},
};
static const fm_item m_sym[] = { // what the keypad has no key for, or hides behind 2nd/alpha
  {"pi", 0, 0, 0, 0, 0}, {"e", 0, 0, 0, 0, 0}, {"oo", 0, 0, 0, 0, 0}, {"theta", 0, 0, 0, 0, 0},
  {"sqrt()", 1, 0, 0, 0, 0}, {"surd(,)", 2, 0, 0, 0, 0}, {"abs()", 1, 0, 0, 0, 0}, {"!", 0, "n!", 0, 0, 0},
  // the prime: y'=2y in desolve, f'(x) of a defined f (no key has it; in place of ^(), the ^ key)
  {"'", 0, "y'", 0, 0, 0}, {"exp()", 1, 0, 0, 0, 0}, {"ln()", 1, 0, 0, 0, 0}, {"()/()", 4, 0, 0, 0, 0},
  {"=", 0, 0, 0, 0, 0}, {"<=", 0, 0, 0, 0, 0}, {">=", 0, 0, 0, 0, 0}, {"!=", 0, 0, 0, 0, 0},
};
static const fm_item m_more[] = {
  {0, 0, 0, "Graph", "line or answer", FA_GRAPH},
  {0, 0, 0, "All commands", "search", FA_CATALOG},
#ifndef FLOWCE_TILOOK // (the TI-84 look has the OS's colors only)
  {0, 0, 0, "Themes", "colors", FA_THEME},
#endif
  {0, 0, 0, "Other plots", "kinds", FA_PLOT},
#ifdef FLOWCE_STEPS
  {0, 0, 0, "Statistics", "data, fits", FA_STATS}, // (DeltaCE: focus_stats.cc)
  {0, 0, 0, "Probability", "curves, tests", FA_PROB}, // (DeltaCE: focus_stats.cc)
#endif
  {0, 0, 0, "File", "sessions", FA_FILE},
  {0, 0, 0, "Clear history", 0, FA_CLEAR},
};
#ifdef FLOWCE_TILOOK // (a list as the OS's: named, the template small at the right when it fits)
static const fm_item m_math[] = {
  {"()/()", 4, 0, "Fraction", 0, 0}, {"sqrt()", 1, 0, "Square root", 0, 0}, {"abs()", 1, 0, "Absolute value", 0, 0},
  {"integrate(,x)", 3, 0, "Antiderivative", 0, 0}, {"integrate(,x,,)", 5, 0, "Definite integral", 0, 0}, {"diff(,x)", 3, 0, "Derivative", 0, 0},
  {"limit(,x,)", 4, 0, "Limit", 0, 0}, {"sum(,k,,)", 5, 0, "Sum", 0, 0}, {"^()", 1, 0, "Power", 0, 0},
};
#else
static const fm_item m_math[] = {
  {"()/()", 4, 0, 0, 0, 0}, {"sqrt()", 1, 0, 0, 0, 0}, {"abs()", 1, 0, 0, 0, 0},
  {"integrate(,x)", 3, 0, 0, 0, 0}, {"integrate(,x,,)", 5, 0, 0, 0, 0}, {"diff(,x)", 3, 0, 0, 0, 0},
  {"limit(,x,)", 4, 0, 0, 0, 0}, {"sum(,k,,)", 5, 0, 0, 0, 0}, {"^()", 1, 0, 0, 0, 0},
};
#endif
#define N(a) (char)(sizeof(a) / sizeof(a[0]))
#ifdef FLOWCE_STEPS
#ifdef FLOWCE_TILOOK
static const fm_menu M_APPS = {0, 1, N(m_apps), m_apps}; // (a list, as the OS's menus)
#else
static const fm_menu M_APPS = {2, 2, N(m_apps), m_apps};
#endif
#define CALC_MENU {2, 3, N(m_calc), m_calc, 1}
#else
#define CALC_MENU {2, 2, N(m_calc), m_calc}
#endif
#ifdef FLOWCE_TILOOK // the TI-84 look: every menu a list, as the OS's
#undef CALC_MENU
#define CALC_MENU {0, 1, N(m_calc), m_calc}
static const fm_menu MENUS[6] = { // F1 F2 F3 F4 F5 math
  {0, 1, N(m_alg), m_alg}, CALC_MENU, {0, 1, N(m_trig), m_trig},
  {0, 1, N(m_sym), m_sym}, {0, 1, N(m_more), m_more}, {0, 1, N(m_math), m_math},
};
#else
static const fm_menu MENUS[6] = { // F1 F2 F3 F4 F5 math
  {0, 1, N(m_alg), m_alg}, CALC_MENU, {1, 3, N(m_trig), m_trig},
  {1, 4, N(m_sym), m_sym}, {0, 1, N(m_more), m_more}, {1, 3, N(m_math), m_math},
};
#endif
#undef N

enum { RH = 30, LW = 252, CH = 40, CH2 = 60, BH = 36, ST = FOCUS_SB, SBOT = UI_H - FOCUS_MB }; // BH: a wide item's bar
enum { IN = ST > 16 ? 2 : 5 }; // a list's rows from the card's edge (TI look: inside its 2 px frame)
static int grid_rows(const fm_menu & m) { return (m.n - m.wide + m.cols - 1) / m.cols; } // (without the bar)
// a card's cell height: lower cells when its rows would cover the status bar (the steps build's
// calculus card: three rows and the bar, 52)
static int cellh(const fm_menu & m) {
  const int rows = grid_rows(m), h = m.grid == 2 ? CH2 : CH, room = SBOT - ST - 10 - (m.wide ? BH : 0);
  return rows * h <= room ? h : room / rows;
}

static int px, py, pw, ph, cw; // the card
// a list with formulas (the algebra card, the fits, the graph's points): the card's width, its labels at
// the right; its rows taller (a fraction, d/dx)
static int formulas(const fm_menu & m) { for (int k = 0; k < m.n; ++k) if (m.items[k].text) return 1; return 0; }
static int hdr;                // focus_list: the title's height at the top of the card
static const char * title;
static int top, vis = 99, rh = RH; // focus_list: the first row shown, the rows shown, their height
static int rh_need; // (prepare) the rows a list's formulas need, at one size
// rmin: a list's rows at least so tall (its formulas'), fewer of them shown when it scrolls
static void geometry(int id, const fm_menu & m, int rmin = 0) {
#ifdef FLOWCE_TILOOK
  if (!m.grid) { // as the OS's menus: rows of 20 (taller for a formula), 9 at most shown, in a box
    // from column 34 to 302 over the F-key labels (a key's menu) or in the middle
    const int r0 = rmin > 20 ? rmin : 20, room = (SBOT - ST - 4 - hdr) / r0;
    int rows = m.n < vis ? m.n : vis;
    if (rows > room) rows = vis = room;
    rh = r0; pw = 268; ph = rows * rh + 4 + hdr; px = 34;
    py = id == 5 ? ST + (SBOT - ST - ph) / 2 : SBOT - ph;
    return;
  }
#endif
  if (m.grid) {
    cw = m.grid == 2 ? (m.cols == 3 ? 100 : 150) : m.cols == 3 ? 76 : 60;
    pw = m.cols * cw + 10; ph = grid_rows(m) * cellh(m) + (m.wide ? BH : 0) + 10;
  } else { // (a long list: vis rows, scrolled; DeltaCE: rows lower than RH when they would cover the status bar)
    const int f = formulas(m), r0 = f ? (rmin > RH + 2 ? rmin : RH + 2) : RH;
    int rows = m.n < vis ? m.n : vis;
    if (rows * r0 + 10 > SBOT - ST - 2) {
      if (rmin && vis < 99) rows = vis = (SBOT - ST - 12) / r0;
      rh = (SBOT - ST - 12) / rows;
    } else rh = r0;
    pw = f ? 310 : LW; ph = rows * rh + 10;
  }
  int anchor = id < 5 ? 32 + 64 * id : UI_W / 2, mg = id < 5 ? 1 : 4; // (DeltaCE: a key's card over its whole tab)
  px = anchor - pw / 2;
  if (px < mg) px = mg;
  if (px > UI_W - pw - mg) px = UI_W - pw - mg;
  py = id == 5 ? ST + (SBOT - ST - ph) / 2 : SBOT - ph; // (DeltaCE: on the bar, joined to its tab)
}

// the box of item k (screen coordinates)
static void item_box(const fm_menu & m, int k, int & x, int & y, int & w, int & h) {
  if (m.grid && m.wide && k == m.n - 1) { x = px + 5; y = py + 5 + grid_rows(m) * cellh(m); w = m.cols * cw; h = BH; }
  else if (m.grid) { x = px + 5 + (k % m.cols) * cw; h = cellh(m); y = py + 5 + (k / m.cols) * h; w = cw; }
  else if (IN == 2) { x = px + 2; y = py + 2 + hdr + (k - top) * rh; w = pw - 4; h = rh; }
  else { x = px + 5; y = py + 5 + hdr + (k - top) * rh; w = pw - (m.n > vis ? 18 : 10); h = rh; }
}

// a list row's width for its formula: from after the keycap to its label (at the right), apart
static int lroom(const fm_menu & m, int k) {
  const fm_item & it = m.items[k];
  const int w = pw - (m.n > vis ? 18 : 10), lw = it.label ? ui_text_width(&ui_tr12, it.label, -1) + 18 : 0;
  return w - 40 - lw > 40 ? w - 40 - lw : 40;
}
// the previews of the open menu, laid out once (empty arguments drawn as boxes); PLV: their levels
// (a list's items: 60 at most; 16 overran into the next variables with the graph's points)
static mi_layout * PL;
static signed char PLV[60];
// The fixed menus' previews (the F-keys' cards, the applications'), kept once laid out: laying them
// out again took a third of the calculus card's opening. Given back when the heap runs out.
struct kept { mi_layout * L; short w, h; signed char lv[16]; };
enum { NKEPT = 7 };
static kept * KP[NKEPT];
static void drop_pl() { // PL let go (kept ones stay)
  for (int c = 0; c < NKEPT; ++c) if (PL && KP[c] && PL == KP[c]->L) { PL = 0; return; }
  delete[] PL;
  PL = 0;
}
#ifdef FLOWCE_STEPS
int steps_release(); // steps.cc: the steps' templates kept parsed
#endif
extern "C" int focus_menu_release(void) { // (allocator_custom.c) the kept previews not in use
  int r = 0;
  for (int c = 0; c < NKEPT; ++c)
    if (KP[c] && KP[c]->L != PL) { delete[] KP[c]->L; delete KP[c]; KP[c] = 0; r = 1; }
#ifdef FLOWCE_STEPS
  r |= steps_release();
#endif
  return r;
}
static void prepare(const fm_menu & m);
static void prepare_kept(const fm_menu & m, int c) { // menu c's previews: kept, or laid out and kept
  kept * k = KP[c];
  const short h = (short)(m.grid ? 0 : rh); // (a list's previews fit its rows; a grid's, its cells)
  if (k && k->w == pw && k->h == h) { drop_pl(); PL = k->L; memcpy(PLV, k->lv, sizeof k->lv); return; }
  if (k) { if (PL == k->L) PL = 0; delete[] k->L; delete k; KP[c] = 0; }
  prepare(m);
  if (m.n > 16 || !PL || !(k = new kept)) return;
  k->L = PL; k->w = (short)pw; k->h = h; memcpy(k->lv, PLV, sizeof k->lv);
  KP[c] = k;
}
static void prepare(const fm_menu & m) {
  drop_pl();
  PL = new mi_layout[m.n];
  for (int k = 0; k < m.n; ++k) {
    const fm_item & it = m.items[k];
    const char * t = it.pv ? it.pv : it.text;
    PLV[k] = -1;
    if (!t || (!m.grid && !it.text)) continue; // (a list's symbol column is text)
#ifdef FLOWCE_TILOOK
    if (!m.grid) { // (the OS's menus: a named item's formula small, at the right; else the name, large)
      PLV[k] = (signed char)ui_math_fit(t, strlen(t), -1, it.label ? 5 : 3, it.label ? 110 : 220, 60, it.pv ? MI_F_IMPLDOT : MI_F_CALLBOX, PL[k]);
      continue;
    }
#endif
    // (a card's formula, its pv: products without dots, 2 pi r h; a template: boxes in its calls;
    // the bar's preview at its left)
    const int fl = it.pv ? MI_F_IMPLDOT : MI_F_CALLBOX;
    key_latch(); // (the calculus card takes half a second: a key pressed meanwhile is kept)
    if (m.grid && m.wide && k == m.n - 1) PLV[k] = (signed char)ui_math_fit(t, strlen(t), -1, 3, 64, BH - 10, fl, PL[k]);
    else if (m.grid == 2) PLV[k] = (signed char)ui_math_fit(t, strlen(t), -1, 3, cw - 12, cellh(m) - 24, fl, PL[k]);
    else if (m.grid) PLV[k] = (signed char)ui_math_fit(t, strlen(t), -1, 3, cw - 8, cellh(m) - 4, MI_F_CALLBOX, PL[k]);
    else PLV[k] = (signed char)ui_math_fit(t, strlen(t), -1, 4, lroom(m, k), 200, fl, PL[k]); // (its width)
  }
  if (m.grid) return;
#ifdef FLOWCE_TILOOK
  rh_need = 0; // (rows as tall as the tallest formula, 20 at least)
  for (int k = 0; k < m.n; ++k) if (PLV[k] >= 0 && PL[k].asc + PL[k].desc + 4 > rh_need) rh_need = PL[k].asc + PL[k].desc + 4;
  return;
#endif
  // a list's formulas one size: the smallest any needs for its width (12 px at least: smaller
  // ones alone); rh_need: the rows as tall as the tallest at that size (focus_card_list makes
  // them so); in shorter rows, the tall ones smaller
  int lv = 0, hm;
  for (int k = 0; k < m.n; ++k) if (PLV[k] > lv && PLV[k] <= 6) lv = PLV[k];
  for (int pass = 0; pass < 2; ++pass)
    for (;;) { // (pass 0: the size; a size smaller when the tallest needs more than 40 px: 4 rows shown)
      hm = 0;
      for (int k = 0; k < m.n; ++k) {
        if (PLV[k] < 0) continue;
        const fm_item & it = m.items[k];
        const char * t = it.pv ? it.pv : it.text;
        if (pass ? PL[k].asc + PL[k].desc > rh - 8 : PLV[k] < lv)
          PLV[k] = (signed char)ui_math_fit(t, strlen(t), -1, pass ? PLV[k] : lv, lroom(m, k), pass ? rh - 8 : 200, it.pv ? MI_F_IMPLDOT : MI_F_CALLBOX, PL[k]);
        if (PL[k].asc + PL[k].desc > hm) hm = PL[k].asc + PL[k].desc;
      }
      if (!pass) rh_need = hm + 8;
      if (pass || hm <= 40 || lv >= 6) break;
      ++lv;
    }
}
// getkey at the console, idle (no card open): the next fixed menu's previews laid out and kept, so
// even its first opening is quick (laying them out was a third of the calculus card's 0.46 s); a key
// pressed meanwhile is kept (prepare latches keys) and read right after
extern "C" void focus_warm(void) {
  static signed char next;
#ifdef FLOWCE_STEPS
  enum { NW = 7 };
#else
  enum { NW = 6 };
#endif
  while (next < NW && KP[next]) ++next;
  if (next >= NW || (int)(size_t)malloc(0xffffff) < 40 * 1024) return; // (the heap short: the screen's strip would go)
  const int c = next++;
#ifdef FLOWCE_STEPS
  const fm_menu & m = c < 6 ? MENUS[c] : M_APPS;
#else
  const fm_menu & m = MENUS[c];
#endif
  geometry(c < 6 ? c : 1, m);
  prepare_kept(m, c);
  drop_pl();
}
// item k's preview, centered vertically at ymid; centered in w if center, else from x
static void preview(int k, const char * t, int x, int ymid, int w, int ink, int bg, int center) {
  if (PLV[k] < 0) return;
  const mi_layout & L = PL[k];
  int xx = center ? x + (w - L.width) / 2 : x, base = ymid - (L.asc + L.desc) / 2 + L.asc;
  ui_math_draw(L, t, PLV[k], xx, base, 1, ink, bg, ink == UC_ONACC ? UC_ONACC : UC_ACC);
}

// a list's symbol (==, %, and): large and crisp in the math face when it has the glyphs (words and
// # %: bold UI text), its ink centered at (cx, cy)
static void sym_text(const char * s, int cx, int cy, int fg, int bg) {
  const ui_face * f = &ui_mu17;
  for (const char * p = s; *p; ++p)
    if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p != ' ' && !ui_glyph_of(f, (unsigned char)*p))) { f = &ui_tb12; break; }
  int t = 99, b = -99;
  for (const char * p = s; *p; ++p) {
    const ui_glyph * g = ui_glyph_of(f, (unsigned char)*p);
    if (g && g->h) { if (g->oy < t) t = g->oy; if (g->oy + g->h > b) b = g->oy + g->h; }
  }
  if (t > b) return;
  ui_draw_text(f, s, -1, cx - ui_text_width(f, s, -1) / 2, cy - (t + b) / 2, ui_ramp(1, fg, bg), 0);
}

static char fresh; // paint_rows: the card was just painted under the items (their boxes are the card's color)
static __attribute__((noinline)) void paint_item(const fm_menu & m, int k, int sel) {
  const fm_item & it = m.items[k];
  int x, y, w, h;
  item_box(m, k, x, y, w, h);
  int on = k == sel, bg = on ? UC_ACC : UC_TILE, fg = on ? UC_ONACC : UC_INK, sub = on ? UC_ONACC : UC_SUB;
  ui_clip(x, y, x + w, y + h);
  if (!fresh) ui_fill(x, y, w, h, ui_col(1, UC_CARD));
  if (m.grid) {
    ui_rrect(1, x + 2, y + 2, w - 4, h - 4, 6, bg, UC_CARD); // each item on its tile (the selected one blue)
    if (m.wide && k == m.n - 1) { // the bar: its preview, the label over the hint, a chevron
      preview(k, it.pv ? it.pv : it.text, x + 10, y + h / 2, 64, fg, bg, 0);
      ui_draw_text(&ui_tb12, it.label, -1, x + 84, y + 16, ui_ramp(1, fg, bg), 0);
      if (it.hint) ui_draw_text(&ui_tr10, it.hint, -1, x + 84, y + 29, ui_ramp(1, sub, bg), 0);
      ui_draw_text(&ui_tb12, ">", -1, x + w - 16, y + h / 2 + 5, ui_ramp(1, sub, bg), 0);
    }
    else if (m.grid == 2) { // a card: the preview, its label under it (a label alone: centered)
      int alone = PLV[k] < 0;
      preview(k, it.pv ? it.pv : it.text, x + 4, y + (h - 14) / 2, w - 8, fg, bg, 1);
      if (it.label) {
        const ui_face * f = alone ? (on ? &ui_tb12 : &ui_tr12) : on ? &ui_tb10 : &ui_tr10;
        int lw = ui_text_width(f, it.label, -1);
        ui_draw_text(f, it.label, -1, x + (w - lw) / 2, alone ? y + h / 2 + 5 : y + h - 8, ui_ramp(1, alone ? fg : sub, bg), 0);
      }
    } else preview(k, it.pv ? it.pv : it.text, x + 4, y + h / 2, w - 8, fg, bg, 1);
  } else if (IN == 2) { // TI look: "1:" (white on black when chosen; 9: an arrow when rows follow),
    // the name in the large font; a formula small and blue at the right of a name, else the name
    ui_fill(x, y, w, h, ui_col(1, UC_CARD));
    const int b = y + (h + 14) / 2;
    char num[5] = {(char)(k < 9 ? '1' + k : k == 9 ? '0' : ' '), ':', 0};
    if ((k == top + vis - 1 && k < m.n - 1) || (k == top && k)) strcpy(num + 1, k == top && k ? "\xe2\x86\x91" : "\xe2\x86\x93");
    const int nw = ui_text_width(&ui_mu20, num, -1);
    if (on) ui_fill(x, y + (h - 16) / 2, nw - 2, 16, ui_col(1, UC_INK));
    ui_draw_text(&ui_mu20, num, -1, x, b, ui_ramp(1, on ? UC_WHITE : UC_INK, on ? UC_INK : UC_CARD), 0);
    const unsigned char * ri = ui_ramp(1, UC_INK, UC_CARD);
    if (it.label) {
      const int lw = ui_draw_text(&ui_mu20, it.label, -1, x + nw, b, ri, 0);
      if (it.text && PLV[k] >= 0 && nw + lw + 10 + PL[k].width <= w) { // (only when it fits)
        preview(k, it.pv ? it.pv : it.text, x + w - 4 - PL[k].width, y + h / 2, 0, UC_BLUE, UC_CARD, 0);
      } else if (it.hint) ui_draw_text(&ui_tr9, it.hint, -1, x + w - 4 - ui_text_width(&ui_tr9, it.hint, -1), b, ui_ramp(1, UC_SUB, UC_CARD), 0);
    } else if (PLV[k] >= 0) {
      ui_clip(x + nw, y, x + w, y + h);
      preview(k, it.pv ? it.pv : it.text, x + nw, y + h / 2, 0, UC_INK, UC_CARD, 0);
    } else if (it.pv) ui_draw_text(&ui_mu20, it.pv, -1, x + nw, b, ri, 0);
  } else { // a list: each row on its tile, as the grids' items; a formula's label at the right, gray
    ui_rrect(1, x + 1, y + 1, w - 2, h - 2, 6, bg, UC_CARD);
    if (k < 9) { // a keycap: its digit's ink centered in the frame (from the advance, the 1 sat right)
      const char num[2] = {(char)('1' + k), 0};
      const int fx = x + 9, fy = y + h / 2 - 7;
      ui_rframe(1, fx, fy, 13, 13, 3, sub, bg);
      if (const ui_glyph * g = ui_glyph_of(&ui_tb9, (unsigned char)num[0]))
        ui_draw_text(&ui_tb9, num, -1, fx + 6 - g->ox - (g->w - 1) / 2, fy + 6 - g->oy - (g->h - 1) / 2, ui_ramp(1, sub, bg), 0);
    }
    int lx = x + 30;
    if (it.text) {
      if (it.label) ui_draw_text(&ui_tr12, it.label, -1, x + w - 10 - ui_text_width(&ui_tr12, it.label, -1), y + h / 2 + 5, ui_ramp(1, sub, bg), 0);
      ui_clip(x + 26, y, x + 30 + lroom(m, k), y + h);
      preview(k, it.pv ? it.pv : it.text, x + 30, y + h / 2, 0, fg, bg, 0);
      ui_noclip();
      return;
    }
    else if (it.pv) { // the symbol column
      sym_text(it.pv, x + 50, y + h / 2, fg, bg);
      lx = x + 80;
    }
    ui_draw_text(on ? &ui_tb12 : &ui_tr12, it.label, -1, lx, y + h / 2 + 5, ui_ramp(1, fg, bg), 0);
    if (it.hint) {
      int hw = ui_text_width(&ui_tr10, it.hint, -1);
      ui_draw_text(&ui_tr10, it.hint, -1, x + w - 8 - hw, y + h / 2 + 4, ui_ramp(1, sub, bg), 0);
    }
  }
  ui_noclip();
}

// the card: a shadow (dimmed, bank 0), the white card, a hairline inside its edge
static void card_bg() {
#ifdef FLOWCE_TILOOK
  ui_fill(px, py, pw, ph, ui_col(1, UC_INK)); // as the OS's menus: a 2 px black frame, the gray card
  ui_fill(px + 2, py + 2, pw - 4, ph - 4, ui_col(1, UC_CARD));
  return;
#endif
  // the shadow where the card leaves it showing, along its right and bottom edges (all of it was
  // filled, then covered by the card)
  const int c0 = ui_cx0, r0 = ui_cy0, c1 = ui_cx1, r1 = ui_cy1;
  ui_clip(c0 > px + pw - 8 ? c0 : px + pw - 8, r0, c1, r1);
  ui_rrect(0, px + 1, py + 3, pw, ph, 8, UC_SHADOW, UC_BG);
  ui_clip(c0, r0 > py + ph - 8 ? r0 : py + ph - 8, c1, r1);
  ui_rrect(0, px + 1, py + 3, pw, ph, 8, UC_SHADOW, UC_BG);
  ui_cx0 = c0; ui_cy0 = r0; ui_cx1 = c1; ui_cy1 = r1;
  ui_rrect(1, px, py, pw, ph, 8, UC_CARD, UC_DIM);
  ui_rframe(1, px + 1, py + 1, pw - 2, ph - 2, 7, UC_LINE, UC_CARD);
}
// paints rows [y0, y1) of the card (and its shadow) over what the screen shows
// a band's end at an item's edge, the last one up to e (an item split between two bands was
// painted twice), else e
static int snap(const fm_menu & m, int b, int e) {
  int best = 0;
  for (int k = 0; k < m.n; ++k) {
    if (!m.grid && (k < top || k >= top + vis)) continue;
    int x, y, w, h;
    item_box(m, k, x, y, w, h);
    if (y > b + 8 && y <= e && y > best) best = y;
    if (y + h > b + 8 && y + h <= e && y + h > best) best = y + h;
  }
  return best ? best : e;
}
static void paint_rows(const fm_menu & m, int y0, int y1, int sel, int all) {
  int rows = ui_band_open(48);
  fresh = (char)all;
  for (int b = y0, e; b < y1; b = e) {
    e = rows && b + rows < y1 ? snap(m, b, b + rows) : y1;
    if (rows) { ui_band_begin(b, e); ui_band_load(); }
    else ui_clip(0, b, UI_W, e);
    if (all) {
      card_bg();
      if (hdr) {
        int tw = ui_text_width(&ui_tb12, title, -1);
        ui_draw_text(&ui_tb12, title, -1, px + (pw - tw) / 2, py + 22, ui_ramp(1, UC_INK, UC_CARD), 0);
      }
    }
    for (int k = 0; k < m.n; ++k) { // the items this band touches
      if (!m.grid && (k < top || k >= top + vis)) continue; // scrolled out of a long list
      int x, y, w, h;
      item_box(m, k, x, y, w, h);
      if (y < e && y + h > b) { paint_item(m, k, sel); key_latch(); }
    }
    if (!m.grid && m.n > vis && IN != 2) { // a long list's scroll bar (TI look: the arrows of 1:)
      int ty = py + 5 + hdr, th = vis * rh, tx = px + pw - 10;
      ui_clip(tx, b > ty ? b : ty, tx + 4, e < ty + th ? e : ty + th);
      ui_fill(tx, ty, 4, th, ui_col(1, UC_LINE));
      ui_fill(tx, ty + th * top / m.n, 4, th * vis / m.n, ui_col(1, UC_SUB));
      if (!rows) ui_clip(0, b, UI_W, e);
      else ui_noclip();
    }
    if (rows) ui_band_end();
  }
  ui_noclip();
}

static void repaint_item(const fm_menu & m, int k, int sel) {
  int x, y, w, h;
  item_box(m, k, x, y, w, h);
  paint_rows(m, y, y + h, sel, 0);
}

static void tab_text(int i, const char * w, int fg);
// opens card m (its geometry set): the scene dims at once, tab col of the bar is drawn open
// (layer 0: its icon and word; 2nd, alpha: its text label), the card is painted
static const char * tab_word_; // (DeltaCE) the open tab's word instead of the console's (the graph's menus)
// (DeltaCE) tab col drawn open, joined to the card: the card rises out of its key
static void card_tab(int col, int layer) {
  int band = ui_band_open(32) >= 32;
  if (band) { ui_band_begin(SBOT - 8, UI_H); ui_band_load(); }
  focus_tab_card(px, px + pw);
  if (tab_word_) tab_text(col, tab_word_, UC_ACC);
  else if (!layer) focus_tab(col, 1, 1);
  else tab_text(col, console_fkey_label(layer, col), layer == 1 ? UC_BLUE : UC_GREEN);
  focus_tab_card(0, 0);
  ui_noclip();
  if (band) ui_band_end();
}
static void open_card(const fm_menu & m, int col, int layer, int sel) {
#ifdef FLOWCE_TILOOK
  if (!m.grid) { // the OS's 20-row items; a formula alone (no name) as tall as it needs, a named
    // item's only when it fits (its name says it); the chosen one shown
    int r = 20;
    for (int k = 0; k < m.n && k < 60; ++k)
      if (PLV[k] >= 0) {
        const int hk = PL[k].asc + PL[k].desc + 2;
        if (!m.items[k].label) { if (hk > r) r = hk; }
        else if (hk > 18) PLV[k] = -1;
      }
    geometry(col, m, r);
    top = sel >= vis ? sel - vis + 1 : 0;
  }
#endif
  ui_dim(1);
  paint_rows(m, py, col < 5 ? SBOT : py + ph + 4, sel, 1);
  if (col < 5) card_tab(col, layer);
  ui_band_close();
}
// closes the card: the scene comes back; under it, the stage is repainted (repaint) unless a
// view owns the screen (the graph redraws itself); bar0: the bar's plain layer
#ifdef FLOWCE_STEPS
static char af_nest; // a card opened from an application's form: it repaints what the card covered
#endif
static void close_card(int bar0, int repaint) {
  drop_pl();
#ifdef FLOWCE_STEPS
  if (af_nest) return;
#endif
  ui_dim(0);
  if (focus_view || !repaint) return;
  focus_repaint(py - 2, py + ph + 6);
  if (bar0) focus_bar_reset();
  else focus_bar_redraw();
}
static int cur; // the open menu (focus_popover's id, focus_fmenu's index): its own key closes it
// the menu keys (1: not one): F1-F5 and math switch popovers (-2 - id), 2nd/alpha F-keys switch
// KhiCAS's menus (-2 - index), a plain F-key closes those
static int fk_pop(int k) {
  int nk = k == KEY_CTRL_SYMB ? 5 : (k >= KEY_CTRL_F1 && k <= KEY_CTRL_F5) ? k - KEY_CTRL_F1 : -1;
  return nk < 0 ? 1 : nk == cur ? -1 : -2 - nk;
}
static int fk_kh(int k) {
  int j = k >= KEY_CTRL_F1 && k <= KEY_CTRL_F6 ? k - KEY_CTRL_F1 : k >= KEY_CTRL_F7 && k <= KEY_CTRL_F20 ? k - KEY_CTRL_F7 + 6 : -1;
  return j < 0 ? 1 : (j == cur || j < 5) ? -1 : -2 - j;
}
// the keys of an open card: arrows, 1-9 and EXE pick (the item, >= 0), EXIT/AC cancel (-1); fk:
// the menu keys; a long list scrolls. The arrows wrap around, as in the TI's menus: in a grid,
// left of the first item of a row is its last one (right: the reverse), up from the top row is
// the bottom of the column (down: the reverse); in a list, up from the top is the bottom. A wide
// item's bar is a row of its own under the grid: up and down come back to the column they left.
static int run_keys(const fm_menu & m, int sel, int (*fk)(int)) {
  static int wcol = 1; // the column the bar was entered from
  for (;;) {
    int k;
    GetKey(&k);
    if (k == KEY_CTRL_SHIFT || k == KEY_CTRL_ALPHA) continue;
    if (k == KEY_CTRL_EXIT || k == KEY_CTRL_AC) return -1;
    int f = fk ? fk(k) : 1, n = m.n, c = m.grid ? m.cols : 1, ns = sel;
    if (f != 1) return f;
    if (m.grid && m.wide && sel == n - 1) { // the bar: left and right stay
      const int gn = n - 1, col = wcol < c ? wcol : 0, last = (gn - 1) / c * c + col;
      if (k == KEY_CTRL_UP) ns = last < gn ? last : gn - 1;
      if (k == KEY_CTRL_DOWN) ns = col < gn ? col : 0;
    }
    else if (m.grid) {
      const int gn = n - m.wide, r0 = sel - sel % c, r1 = r0 + c - 1 < gn - 1 ? r0 + c - 1 : gn - 1; // the row
      if (k == KEY_CTRL_LEFT) ns = sel > r0 ? sel - 1 : r1;
      if (k == KEY_CTRL_RIGHT) ns = sel < r1 ? sel + 1 : r0;
      if (k == KEY_CTRL_UP) ns = sel >= c ? sel - c : m.wide ? n - 1 : sel + (gn - 1 - sel) / c * c;
      if (k == KEY_CTRL_DOWN) ns = sel + c < gn ? sel + c : m.wide ? n - 1 : sel % c;
      if (ns == n - 1 && m.wide) wcol = sel % c;
    } else {
      if (k == KEY_CTRL_UP) ns = sel > 0 ? sel - 1 : n - 1;
      if (k == KEY_CTRL_DOWN) ns = sel < n - 1 ? sel + 1 : 0;
    }
    if (k >= KEY_CHAR_1 && k <= KEY_CHAR_9 && k < KEY_CHAR_1 + n) return k - KEY_CHAR_1; // not = < > (past 9)
    if (k == KEY_CHAR_0 && m.wide) return n - 1; // (the bar: the steps build's Applications)
    if (k == KEY_CHAR_0 && IN == 2 && n >= 10) return 9; // (TI look: 0 is the tenth, as on the OS)
    if (k == KEY_CTRL_EXE || k == KEY_CTRL_OK) return sel;
    if (ns != sel) {
      int old = sel, nt = ns < top ? ns : ns >= top + vis ? ns - vis + 1 : top;
      sel = ns;
      if (nt != top) { // a long list scrolled: all its rows
        top = nt;
        paint_rows(m, py + IN + hdr, py + IN + hdr + vis * rh, sel, 0);
      } else {
        repaint_item(m, old, sel);
        repaint_item(m, sel, sel);
      }
      ui_band_close();
    }
  }
}

// a list card, centered, under a title (or none): KhiCAS's own menus (doMenu: config, variables,
// file...) and confirmations. A long list scrolls. Returns the item chosen, -1 if cancelled.
int focus_list(const char * t, const char * const * labels, int n, int sel, const char * const * hints, const char * const * syms) {
  if (n < 1) return -1;
  if (n > 60) n = 60;
  fm_item * it = new fm_item[n];
  for (int k = 0; k < n; ++k) { it[k].text = 0; it[k].back = 0; it[k].pv = syms ? syms[k] : 0; it[k].label = labels[k]; it[k].hint = hints ? hints[k] : 0; it[k].act = 0; }
  fm_menu m = {0, 1, (char)n, it};
  title = t; hdr = t ? 26 : 0;
  rh = n > 5 ? 24 : RH;
  vis = (SBOT - ST - 16 - hdr) / rh;
  if (vis > n) vis = n;
  if (sel < 0 || sel >= n) sel = 0;
  top = sel >= vis ? sel - vis + 1 : 0;
  pw = 236; ph = hdr + vis * rh + 10;
  px = (UI_W - pw) / 2; py = ST + (SBOT - ST - ph) / 2;
  PL = 0;
  open_card(m, 5, 0, sel);
  int res = run_keys(m, sel, 0);
  hdr = 0; top = 0; vis = 99; rh = RH;
  delete[] it;
  close_card(0, 1);
  return res;
}

int focus_choose(const char * t, const char * const * labels, int n) { return focus_list(t, labels, n, 0, 0); }

// a message card (KhiCAS's print_msg12): title t, text s (cut to the card), over the screen as
// it is; the caller reads the key, the next focus_disp repaints everything
static void note_card(int x, int y, int w, int h) {
  ui_rrect(0, x + 1, y + 3, w, h, 8, UC_SHADOW, UC_BG);
  ui_rrect(1, x, y, w, h, 8, UC_CARD, UC_BG);
  ui_rframe(1, x + 1, y + 1, w - 2, h - 2, 7, UC_LINE, UC_CARD);
}
static int fit(const ui_face * f, const char * s, int w) { // bytes of s that fit in w pixels
  int n = strlen(s);
  while (n > 0 && ui_text_width(f, s, n) > w) --n;
  return n;
}
void focus_note(const char * t, const char * s) {
  const int w = 280, x = (UI_W - w) / 2, h = s && *s ? 70 : 46, y = ST + (SBOT - ST - h) / 2;
  note_card(x, y, w, h);
  if (t) ui_draw_text(&ui_tb12, t, fit(&ui_tb12, t, w - 24), x + 12, y + 24, ui_ramp(1, UC_INK, UC_CARD), 0);
  if (s && *s) ui_draw_text(&ui_tr12, s, fit(&ui_tr12, s, w - 24), x + 12, y + 50, ui_ramp(1, UC_SUB, UC_CARD), 0);
  focus_invalidate();
}

// the graph's table of values in the Focus look: a header row (head[ncol]), nrow rows of cells
// (row by row), alternate rows shaded, the keys on the last line. Drawn in bands (no flicker).
// a column's name: its letter in italics (17 px), its number as an index (y1); its width
static int head_name(const char * h, int x, int y, const unsigned char * r) {
  char a[8];
  int n = 0;
  while (h[n] && !(h[n] >= '0' && h[n] <= '9') && n < 7) { a[n] = h[n]; ++n; }
  a[n] = 0;
  const int w = ui_math_text(a, 4, x, y, r);
  return h[n] ? w + 1 + ui_math_text(h + n, 6, x + w + 1, y + 4, r) : w;
}
// a value: a number in the answers' digits, a word (undefined) in the text face
static int cell(const char * t, int lv, int x, int y, const unsigned char * r) {
  if (t[0] >= 'a' && t[0] <= 'z') return r ? ui_draw_text(&ui_tr10, t, -1, x, y, r, 0) : ui_text_width(&ui_tr10, t, -1);
  return ui_math_text(t, lv, x, y, r);
}
// the graph's table: the names on a soft band, each column's values right-aligned, the widest in
// the column's middle, rows striped (no rules)
void focus_table(const char * const * head, int ncol, const char * const * cells, int nrow, const char * hint) {
  const int y0 = ST, hh = 26, rh2 = 18, cwid = UI_W / ncol, lv = ncol > 3 ? 6 : 5; // (14 px, 12 with 4 columns)
  short R[5];
  for (int c = 0; c < ncol && c < 5; ++c) {
    int w = 0;
    for (int r = 0; r < nrow; ++r) { const int v = cell(cells[r * ncol + c], lv, 0, 0, 0); if (v > w) w = v; }
    R[c] = (short)(c * cwid + (cwid + w) / 2);
  }
  int rows = ui_band_open(48);
  for (int b = y0; b < UI_H; b += rows ? rows : UI_H - y0) {
    int e = rows && b + rows < UI_H ? b + rows : UI_H;
    if (rows) ui_band_begin(b, e);
    else ui_clip(0, b, UI_W, e);
    ui_fill(0, y0, UI_W, UI_H - y0, ui_col(0, UC_BG));
    ui_fill(0, y0, UI_W, hh, ui_col(0, UC_ACCSOFT));
    for (int c = 0; c < ncol; ++c)
      head_name(head[c], c * cwid + (cwid - head_name(head[c], 0, 0, 0)) / 2, y0 + 18, ui_ramp(0, UC_ACC, UC_ACCSOFT));
    for (int r = 0; r < nrow; ++r) {
      int y = y0 + hh + r * rh2, bg = r & 1 ? UC_CARD : UC_BG;
      if (r & 1) ui_fill(0, y, UI_W, rh2, ui_col(0, UC_CARD));
      for (int c = 0; c < ncol; ++c) {
        const char * t = cells[r * ncol + c];
        cell(t, lv, R[c] - cell(t, lv, 0, 0, 0), y + 13, ui_ramp(0, c ? UC_INK : UC_SUB, bg));
      }
    }
    ui_draw_text(&ui_tr10, hint, -1, 10, UI_H - 5, ui_ramp(0, UC_SUB, UC_BG), 0);
    if (rows) ui_band_end();
  }
  ui_noclip();
}

// a text to read (About, Shortcuts): t in the status bar, s word-wrapped (\n: a new paragraph,
// a line starting with a word ending in ':' bold), up/down scroll, EXIT/EXE close. The caller
// repaints the console afterwards.
void focus_text(const char * t, const char * s) {
  enum { LMAX = 120, LH = 17, W = 296, Y0 = 20 };
  static short ls[LMAX], ll[LMAX];
  int n = 0, i = 0;
  while (s[i] && n < LMAX) { // greedy wrap at spaces
    int j = i, last = -1;
    while (s[j] && s[j] != '\n' && ui_text_width(&ui_tr12, s + i, j + 1 - i) <= W) { if (s[j] == ' ') last = j; ++j; }
    if (s[j] && s[j] != '\n' && last > i) j = last;
    ls[n] = i; ll[n] = j - i; ++n;
    i = j;
    if (s[i] == ' ' || s[i] == '\n') ++i;
  }
  const int vis = (UI_H - Y0 - 4) / LH;
  int top = 0;
  focus_status_label(t);
  for (;;) {
    int rows = ui_band_open(48);
    for (int b = Y0 - 4; b < UI_H; b += rows ? rows : UI_H) {
      int e = rows && b + rows < UI_H ? b + rows : UI_H;
      if (rows) ui_band_begin(b, e);
      ui_fill(0, Y0 - 4, UI_W, UI_H - Y0 + 4, ui_col(0, UC_BG));
      for (int k = 0; k < vis && top + k < n; ++k) {
        const char * l = s + ls[top + k];
        const int bold = ll[top + k] > 1 && l[ll[top + k] - 1] == ':';
        ui_draw_text(bold ? &ui_tb12 : &ui_tr12, l, ll[top + k], 12, Y0 + 12 + k * LH, ui_ramp(0, bold ? UC_ACC : UC_INK, UC_BG), 0);
      }
      if (n > vis) { // scroll bar
        int th = vis * LH;
        ui_fill(UI_W - 6, Y0, 3, th, ui_col(0, UC_LINE));
        ui_fill(UI_W - 6, Y0 + th * top / n, 3, th * vis / n, ui_col(0, UC_SUB));
      }
      if (rows) ui_band_end();
    }
    int key;
    GetKey(&key);
    if (key == KEY_CTRL_EXIT || key == KEY_CTRL_AC || key == KEY_CTRL_EXE || key == KEY_CTRL_OK) break;
    if (key == KEY_CTRL_DOWN && top + vis < n) ++top;
    if (key == KEY_CTRL_UP && top) --top;
    if (key == KEY_CTRL_RIGHT) top = top + 2 * vis < n ? top + vis : n > vis ? n - vis : 0;
    if (key == KEY_CTRL_LEFT) top = top > vis ? top - vis : 0;
  }
  focus_status_label(0);
  focus_invalidate();
}

// the start screen goes before the console is painted (in strips, top down: the rest of the start
// screen showed under it for a moment)
void focus_splash_clear() {
  ui_noclip();
  ui_fill(0, 0, UI_W, UI_H, ui_col(0, UC_BG));
}
// a lent ramp (entries e, e+1, e+2: 1/3, 2/3, all of fg over bg) a/256 of the way from bg
static void splash_shade(int e, const unsigned char * fg, const unsigned char * bg, int a) {
  for (int s = 1; s <= 3; ++s) {
    unsigned char c[3];
    for (int j = 0; j < 3; ++j) c[j] = (unsigned char)(bg[j] + (fg[j] - bg[j]) * s * a / 768);
    ui_borrow(e + s - 1, c);
  }
}
// the start screen: a curve drawn left to right under "FlowCE", each letter fading in as the
// curve's end passes under it, then the tag line, then held (about 1.2 s in all, once per start;
// any key skips it and is not typed); first 1 (no saved session): "press any key" (the caller
// reads it); 2: played again (turned on). The text is drawn once, in palette entries lent by
// KhiCAS's screens (32-52) that go from the background to its colors: a frame writes those and
// the curve's new segments only.
#ifdef FLOWCE_TILOOK // the start screen keeps FlowCE's own look: its fonts (ui_fontdata.c's, renamed), blue
extern "C" const ui_face ui_fx_mi34, ui_fx_tr12, ui_fx_tb12, ui_fx_tr10;
#define ui_mi34 ui_fx_mi34
#define ui_tr12 ui_fx_tr12
#define ui_tb12 ui_fx_tb12
#define ui_tr10 ui_fx_tr10
#define UC_ACC UC_BLUE
#endif
void focus_splash(int first) {
  static bool animated;
  const int y = 100;
  ui_noclip();
  if (first == 1 && animated) { // (no session: the screen drawn stays, its key line under it; drawn
    const char * go = "press any key"; // again, it flickered and the name moved)
    ui_fill(0, 0, UI_W, ST, ui_col(0, UC_BG)); // (the loader's status line gone)
    ui_draw_text(&ui_tb12, go, -1, (UI_W - ui_text_width(&ui_tb12, go, -1)) / 2, y + 82, ui_ramp(0, UC_ACC, UC_BG), 0);
    focus_invalidate();
    return;
  }
  ui_fill(0, 0, UI_W, UI_H, ui_col(0, UC_BG));
  static const char * const cr[] = {"by Tommy Bell, based on KhiCAS by Bernard Parisse",
                                    "version " FLOWCE_VERSION " \xc2\xb7 free software, GNU GPL v3"};
  for (int k = 0; k < 2; ++k)
    ui_draw_text(&ui_tr10, cr[k], -1, (UI_W - ui_text_width(&ui_tr10, cr[k], -1)) / 2, UI_H - 24 + 16 * k, ui_ramp(0, UC_SUB, UC_BG), 0);
  const char * name = "FlowCE", * tag = "calculus on your TI-84 Plus CE"; // (the upright math faces have no capitals)
  const int w = ui_text_width(&ui_mi34, name, -1), w4 = ui_text_width(&ui_mi34, name, 4), x0 = (UI_W - w) / 2;
  const int tx = (UI_W - ui_text_width(&ui_tr12, tag, -1)) / 2;
  static const signed char wave[16] = {0, 43, 79, 103, 112, 103, 79, 43, 0, -43, -79, -103, -112, -103, -79, -43}; // 7 sin, 1/16 px
  const int wx = 48, np = 57, wy = (y + 46) * 16; // 57 points 4 px apart: a period of 64 px
  const ui_theme * th = ui_theme_now();
  const unsigned char * bg = th->rgb[UC_BG];
  unsigned char rp[7][4]; // the letters' ramps, then the tag line's (lent entries, the background at first)
  int lx[6];
  for (int i = 0; i < 7; ++i) {
    rp[i][0] = ui_col(0, UC_BG);
    for (int s = 1; s <= 3; ++s) rp[i][s] = (unsigned char)(32 + 3 * i + s - 1);
    splash_shade(32 + 3 * i, bg, bg, 0);
  }
  for (int i = 0; i < 6; ++i) {
    lx[i] = x0 + ui_text_width(&ui_mi34, name, i);
    ui_draw_text(&ui_mi34, name + i, 1, lx[i], y, rp[i], 0);
  }
  ui_draw_text(&ui_tr12, tag, -1, tx, y + 26, rp[6], 0);
  // the curve 0-700 ms (smoothstep from 0.12: moving at once, fast, slow); a letter from 12 px
  // before the curve's end reaches it to 36 px after; the tag line 600-900 ms
  int skip = animated && first != 2, drawn = 0, t = 0;
  animated = true;
  const long t0 = (long)(clock)(); // 32768 Hz ((clock): giac's first.h makes clock() 0)
  while (t < 900) {
    t = skip ? 900 : (int)(((long)(clock)() - t0) * 1000 / 32768);
    const int u = t < 700 ? 120 + t * 880 / 700 : 1000, p = u * u / 1000 * (3000 - 2 * u) / 1000;
    const int pts = 1 + p * (np - 1) / 1000, pen = wx + 4 * (pts - 1);
    for (; drawn + 1 < pts; ++drawn)
      ui_seg16((wx + 4 * drawn) * 16, wy - wave[drawn & 15], (wx + 4 * drawn + 4) * 16, wy - wave[(drawn + 1) & 15], 28, ui_ramp(0, UC_ACC, UC_BG));
    for (int i = 0; i < 7; ++i) {
      int a = i < 6 ? (pen - lx[i] + 12) * 256 / 48 : (t - 600) * 256 / 300;
      a = a < 0 ? 0 : a > 256 ? 256 : a;
      splash_shade(32 + 3 * i, th->rgb[i < 4 ? UC_INK : i < 6 ? UC_ACC : UC_SUB], bg, a);
    }
    if (!skip && key_waiting()) { skip = 1; key_discard(); }
  }
  // the same text in the theme's own shades (the same colors), the lent entries given back
  ui_draw_text(&ui_mi34, name, 4, x0, y, ui_ramp(0, UC_INK, UC_BG), 0);
  ui_draw_text(&ui_mi34, name + 4, 2, x0 + w4, y, ui_ramp(0, UC_ACC, UC_BG), 0);
  ui_draw_text(&ui_tr12, tag, -1, tx, y + 26, ui_ramp(0, UC_SUB, UC_BG), 0);
  ui_borrow_end();
  if (first == 1) { // (the console shows its keys once past this screen)
    const char * go = "press any key";
    ui_draw_text(&ui_tb12, go, -1, (UI_W - ui_text_width(&ui_tb12, go, -1)) / 2, y + 82, ui_ramp(0, UC_ACC, UC_BG), 0);
  }
  else if (!skip) { // held a moment (it flashed by: the session loads fast)
    for (int k = 0; k < 6 && !key_waiting(); ++k)
      os_wait_1ms(50);
    key_discard();
  }
  focus_invalidate();
}
#ifdef FLOWCE_TILOOK
#undef ui_mi34
#undef ui_tr12
#undef ui_tb12
#undef ui_tr10
#undef UC_ACC
#endif

// a value prompt (KhiCAS's inputline): title t, label sub over an edit field. Keys as inputline:
// characters, DEL, AC/CLEAR (clears, then cancels), arrows, EXE. Returns KEY_CTRL_EXE or
// KEY_CTRL_EXIT.
int focus_prompt(const char * t, const char * sub, std::string & s, bool numeric) {
  const int w = 280, x = (UI_W - w) / 2, h = 104, y = ST + (SBOT - ST - h) / 2, fx = x + 12, fy = y + 50, fw = w - 24, fh = 28;
  ui_dim(1);
  note_card(x, y, w, h);
  if (t) ui_draw_text(&ui_tb12, t, fit(&ui_tb12, t, fw), fx, y + 24, ui_ramp(1, UC_INK, UC_CARD), 0);
  const int ok = focus_chip(1, -1, 0, "OK", "enter", 1, UC_CARD), no = focus_chip(1, -1, 0, "cancel", "clear", 0, UC_CARD);
  focus_chip(1, fx + fw - ok - no - 6, y + h - 10, "cancel", "clear", 0, UC_CARD); // (the TI's keys)
  focus_chip(1, fx + fw - ok, y + h - 10, "OK", "enter", 1, UC_CARD);
  int pos = s.size(), res = 0;
  while (!res) {
    // the field: its label, the text scrolled so that the caret shows, the caret
    ui_rrect(1, fx, fy, fw, fh, 6, UC_ACCSOFT, UC_CARD);
    int lw = sub && *sub ? ui_text_width(&ui_tr10, sub, -1) + 8 : 0;
    if (lw) ui_draw_text(&ui_tr10, sub, -1, fx + 6, fy + 18, ui_ramp(1, UC_SUB, UC_ACCSOFT), 0);
    const char * c = s.c_str();
    int beg = 0, room = fw - lw - 14;
    while (ui_text_width(&ui_tr12, c + beg, pos - beg) > room) ++beg;
    int n = beg;
    while (c[n] && ui_text_width(&ui_tr12, c + beg, n + 1 - beg) <= room) ++n;
    const int tx = fx + 6 + lw;
    ui_draw_text(&ui_tr12, c + beg, n - beg, tx, fy + 19, ui_ramp(1, UC_INK, UC_ACCSOFT), 0);
    ui_fill(tx + ui_text_width(&ui_tr12, c + beg, pos - beg), fy + 5, 2, fh - 10, ui_col(1, UC_ACC));
    int key;
    GetKey(&key);
    if (key == KEY_CHAR_PMINUS) key = '-';
    if (key == KEY_CTRL_EXE || key == KEY_CTRL_OK) res = KEY_CTRL_EXE;
    else if (key == KEY_CTRL_EXIT) res = KEY_CTRL_EXIT;
    else if (key == KEY_CTRL_AC) { if (s.empty()) res = KEY_CTRL_EXIT; else { s = ""; pos = 0; } }
    else if (key == KEY_CTRL_DEL) { if (pos) { s.erase(s.begin() + pos - 1); --pos; } }
    else if (key == KEY_CTRL_LEFT) { if (pos) --pos; }
    else if (key == KEY_CTRL_RIGHT) { if (pos < (int)s.size()) ++pos; }
    else if (key >= 32 && key < 128 && s.size() < 60 &&
             (!numeric || key == '-' || key == '.' || key == 'e' || key == 'E' || (key >= '0' && key <= '9'))) {
      s.insert(s.begin() + pos, char(key));
      ++pos;
    }
    else if (!numeric) // the keys that type a word: pi, sqrt(, sin(, ^2 (as on the edit line)
      if (const char * a = keytostring(key, 0, false))
        for (; *a && s.size() < 60; ++a, ++pos) s.insert(s.begin() + pos, *a);
  }
  ui_dim(0);
  if (!focus_view) {
    focus_repaint(y - 2, y + h + 6);
    focus_bar_redraw();
  }
  return res;
}

// KhiCAS's own F-key menus (their config is in console.cc: the 2nd and alpha layers, plot,
// matrices, lists...) as a grid of cards: the command drawn in 2D over a short label.
static const char * const fm_lab[] = { // command, label
  "mod", "Modulo", "irem", "Remainder", "ifactor", "Prime factors", "gcd", "GCD", "isprime", "Is prime?",
  "nextprime", "Next prime", "powmod", "Power mod", "iegcd", "Bezout", "exact", "Exact", "approx", "Decimal",
  "floor", "Floor", "ceil", "Ceiling", "round", "Round", "sign", "Sign", "max", "Maximum", "min", "Minimum",
  "abs", "Modulus", "arg", "Argument", "re", "Real part", "im", "Imag. part", "conj", "Conjugate",
  "csolve", "Solve in C", "cfactor", "Factor in C", "cpartfrac", "Partial fr. C", "proot", "Roots",
  "pcoeff", "From roots", "quo", "Quotient", "rem", "Remainder", "egcd", "Bezout", "resultant", "Resultant",
  "matrix", "New matrix", "det", "Determinant", "matpow", "Power", "ranm", "Random", "rref", "Row reduce",
  "tran", "Transpose", "egvl", "Eigenvalues", "egv", "Eigenvectors", "makelist", "New list", "range", "Range",
  "seq", "Sequence", "len", "Length", "append", "Append", "ranv", "Random", "sort", "Sort", "apply", "Apply",
  "hex", "Hexadecimal", "bin", "Binary", "debug", "Debug", "python", "Python", "f", "Function",
  "plot", "Function", "plotparam", "Parametric", "plotpolar", "Polar", "plotfield", "Slope field",
  "plotode", "Solution curve",
  "draw_pixel", "Pixel", "draw_line", "Line", "draw_rectangle", "Rectangle", "draw_polygon", "Polygon",
  "draw_circle", "Circle", "draw_string", "Text", "get_pixel", "Read pixel", "clearscreen", "Clear",
  "rand", "Random", "randint", "Random integer", "binomial", "Binomial", "and", "And", "or", "Or",
};
static int namec(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'; }
static const char * fm_label(const char * s) {
  while (*s == ' ') ++s;
  int n = 0;
  while (namec(s[n])) ++n;
  if (!n) return 0;
  for (unsigned i = 0; i < sizeof(fm_lab) / sizeof(fm_lab[0]); i += 2)
    if (!strncmp(fm_lab[i], s, n) && !fm_lab[i][n]) return fm_lab[i + 1];
  return 0;
}
// the open menu's tab when the bar shows text (2nd, alpha): bright, in bank 1
static void tab_text(int i, const char * w, int fg) {
  char b[16];
  int n = 0;
  while (*w == ' ') ++w;
  while (w[n] && n < 15) { b[n] = w[n]; ++n; }
  while (n && b[n - 1] == ' ') --n;
  b[n] = 0;
  int cx = 32 + 64 * i, tw = ui_text_width(&ui_tb10, b, -1);
  focus_tab_open_box(i);
  ui_draw_text(&ui_tb10, b, -1, cx - tw / 2, SBOT + 15, ui_ramp(1, fg, UC_CARD), 0);
  ui_noclip();
}
void focus_tab_open(int i, const char * label) { tab_text(i, label, UC_ACC); }
// (DeltaCE) a list on F-key col's tab (word: the tab's), its items' math (or 0) and words, 6 rows
// shown at most (scrolled): the item picked, -1 if none
int focus_card_list(int col, const char * word, const char * const * maths, const char * const * labels, const char * const * hints, int n, int sel) {
  if (n <= 0) return -1;
  fm_item * it = (fm_item *)malloc(n * sizeof(fm_item));
  if (!it) return -1;
  for (int k = 0; k < n; ++k) { // (the math as formulas: a x + b, x^3 sin(x), without product dots)
    it[k].text = it[k].pv = maths ? maths[k] : 0; it[k].back = 0; it[k].label = labels[k]; it[k].hint = hints ? hints[k] : 0; it[k].act = 0;
  }
  const fm_menu m = {0, 1, (char)(n > 60 ? 60 : n), it};
  vis = 6; top = sel >= vis ? sel - vis + 1 : 0; hdr = 0; rh = RH;
  tab_word_ = word;
  geometry(col, m);
  prepare(m);
  if (maths && rh_need > rh) { // (a tall formula, d/dx: taller rows, fewer of them shown)
    geometry(col, m, rh_need);
    top = sel >= vis ? sel - vis + 1 : 0;
    prepare(m);
  }
  open_card(m, col, 0, sel);
  cur = col;
  const int r = run_keys(m, sel, 0);
  close_card(0, 0);
  tab_word_ = 0; vis = 99; top = 0;
  free(it);
  return r;
}
// idx: the menu, numbered as console_menu does (key - F1: 0-4 plain, 5-9 2nd, 10-14 alpha, 15+
// others). Returns the entry chosen, -1 if cancelled, -2 - j to open menu j instead.
int focus_fmenu(int idx, const char * const * e, int n) {
  if (n > 9) n = 9;
  if (n < 1) return -1;
  fm_item it[9];
  char pv[9][24];
  for (int k = 0; k < n; ++k) {
    const char * s = e[k];
    int L = strlen(s), w = L > 0, sym = L > 0;
    for (int i = 0; i < L; ++i) {
      if (!namec(s[i]) || (s[i] >= '0' && s[i] <= '9')) w = 0;
      if (namec(s[i])) sym = 0;
    }
    it[k].text = s; it[k].back = 0; it[k].pv = 0; it[k].hint = 0; it[k].act = 0;
    it[k].label = fm_label(s);
    if (!strcmp(s, "()/()")) it[k].label = "Fraction"; // misc (ALPHA Y=): drawn as the template
    else if (s[0] == ' ' || sym) { // an operator ( mod ) or a symbol (: & #): the text alone, upright
      int a = 0, b = L;
      while (s[a] == ' ') ++a;
      while (b > a && s[b - 1] == ' ') --b;
      if (b - a > 22) b = a + 22;
      memcpy(pv[k], s + a, b - a); pv[k][b - a] = 0;
      it[k].label = pv[k]; it[k].text = 0;
    } else if (!it[k].label && w) { it[k].label = s; it[k].text = 0; } // red, filled: the word alone
    else if (L > 1 && L < 23 && s[L - 1] == '(') { // irem( drawn as it goes in: irem(□,□)
      if (!focus_call_template(s, pv[k], sizeof(pv[k]), 0)) { memcpy(pv[k], s, L); pv[k][L] = ')'; pv[k][L + 1] = 0; }
      it[k].pv = pv[k];
    }
  }
  fm_menu m = {2, 3, (char)n, it};
  int col = idx < 5 ? 4 : idx < 15 ? idx % 5 : 5;
  geometry(col, m);
  prepare(m);
  open_card(m, col, idx / 5, 0);
  cur = idx;
  int res = run_keys(m, 0, fk_kh);
  close_card(1, 1); // 2nd and alpha were used up by the key that opened the menu
  return res;
}

#ifdef WITH_UNITS
// DeltaCE's units menu (alpha F4, where KhiCAS's draw menu was; its commands stay in the search):
// nine categories, then up to nine units of one (as typed: giac's unit names, with an SI prefix
// where giac has none: _cm, _km, _kWh), each over its name. Not in giac: degrees Celsius; its
// _degreeF converts to nothing, and _grad reads as the gradient (_gon is the same unit). EXIT in a category goes back to the
// categories; Convert types => (5_mi=>_km)
static const char * const un_cat[9] = {"Convert", "Length", "Area, volume", "Mass", "Time, speed",
                                       "Force, energy", "Pressure, electric", "Temp., angle", "Constants"};
static const char * const un_tab[8][18] = {
  {"_m", "meter", "_cm", "centimeter", "_mm", "millimeter", "_km", "kilometer", "_inch", "inch",
   "_ft", "foot", "_yd", "yard", "_mi", "mile", "_nmi", "nautical mile"},
  {"_m^2", "square meter", "_ha", "hectare", "_acre", "acre", "_m^3", "cubic meter", "_l", "liter",
   "_ml", "milliliter", "_galUS", "gallon (US)", "_qt", "quart", "_cu", "cup (US)"},
  {"_kg", "kilogram", "_g", "gram", "_mg", "milligram", "_t", "metric ton", "_lb", "pound",
   "_oz", "ounce", "_ton", "short ton", "_u", "atomic mass", "_mol", "mole"},
  {"_s", "second", "_min", "minute", "_h", "hour", "_d", "day", "_yr", "year",
   "_m/_s", "meter/second", "_kph", "km/hour", "_mph", "mile/hour", "_knot", "knot"},
  {"_N", "newton", "_lbf", "pound-force", "_J", "joule", "_cal", "calorie", "_kcal", "kilocalorie",
   "_eV", "electron-volt", "_W", "watt", "_hp", "horsepower", "_kWh", "kilowatt-hour"},
  {"_Pa", "pascal", "_kPa", "kilopascal", "_bar", "bar", "_atm", "atmosphere", "_psi", "psi",
   "_mmHg", "mm of mercury", "_V", "volt", "_A", "ampere", "_Ohm", "ohm"},
  // \x01: a temperature conversion, applied to the line (console.cc: giac has no degrees Celsius)
  {"\x01" "CF", "Celsius to F", "\x01" "FC", "F to Celsius", "\x01" "CK", "Celsius to K", "\x01" "KC", "K to Celsius",
   "_K", "kelvin", "_deg", "degree", "_rad", "radian", "_arcmin", "arc minute", "_rev", "revolution"},
  {"_c_", "light speed", "_g_", "gravity (Earth)", "_G_", "gravitation", "_h_", "Planck", "_k_", "Boltzmann",
   "_NA_", "Avogadro", "_R_", "gas constant", "_qe_", "electron charge", "_me_", "electron mass"},
};
// a card grid of n items, from menu idx's F-key; the item chosen, -1 cancelled, -2 another F-key
static int un_cards(int idx, fm_item * it, int n) {
  fm_menu m = {2, 3, (char)n, it};
  const int col = idx % 5;
  geometry(col, m);
  prepare(m);
  open_card(m, col, idx / 5, 0);
  cur = idx;
  const int r = run_keys(m, 0, fk_kh);
  close_card(1, 1);
  return r < -1 ? -2 : r;
}
const char * focus_units(int idx) {
  for (;;) {
    fm_item it[9];
    for (int k = 0; k < 9; ++k) { it[k].text = 0; it[k].back = 0; it[k].pv = 0; it[k].label = un_cat[k]; it[k].hint = 0; it[k].act = 0; }
    const int c = un_cards(idx, it, 9);
    if (c < 0) return 0;
    if (c == 0) return "=>";
    for (int k = 0; k < 9; ++k) { // (a temperature conversion: its name alone)
      it[k].text = un_tab[c - 1][2 * k][0] == 1 ? 0 : un_tab[c - 1][2 * k]; it[k].label = un_tab[c - 1][2 * k + 1];
    }
    const int u = un_cards(idx, it, 9);
    if (u >= 0) return un_tab[c - 1][2 * u];
    if (u == -2) return 0;
  }
}
#endif

#ifdef FLOWCE_STEPS
// ------------------------------------------------------------------ the applications' forms
// An application picked on the calculus card's Applications opens its form in the card's place:
// a field per input, typed and drawn as on the edit line (mathedit.h, mathinput.h), its label on
// its left (between y =), the hint of the field typed under them. enter: the next field; on the
// last, the call (area(f,g,a,b)...) for the console to run. clear: empties a field; on an empty
// one, the applications' card again. The values stay for the next form (the volume of the region
// whose area was just found); a line typed before the menu goes in the first field.
const char * focus_form_line;
enum { AF_F, AF_G, AF_A, AF_B, AF_X, AF_W = 64, AF_TOP = 28, AF_RH = 28, AF_BOT = 22 };
static char af_val[5][AF_W];     // the curve, the other curve, the bounds, the axis
static signed char af_last = -1; // the application the axis was typed for (each has its own)
static const char * af_err;      // why enter could not compute (the field to fill is selected)
struct af_field { char role; const char * word, * var, * empty, * hint; }; // empty: an empty field's meaning (0: needed)
// field c of an application: 1 2 its curves, a b its bounds (empty: where the curves meet), c f its
// curve, A B its bounds, y x its axis (empty: y = 0, x = 0)
static const af_field & af_field_of(char c) {
  static const char codes[] = "12abcfAByx";
  static const af_field F[] = {
    {AF_F, "between", "y", 0, "a curve, in x"},
    {AF_G, "and", "y", "0: the x-axis", "the other curve"},
    {AF_A, "from", "x", "where they meet", "where the region starts"},
    {AF_B, "to", "x", "where they meet", "where it ends"},
    {AF_F, "curve", "y", 0, "the curve, in x"},
    {AF_F, 0, "f(x)", 0, "the function, in x"},
    {AF_A, "from", "x", 0, "where it starts"},
    {AF_B, "to", "x", 0, "where it ends"},
    {AF_X, "around", 0, "y = 0: the x-axis", "the axis: y = c, or x = c"},
    {AF_X, "around", 0, "x = 0: the y-axis", "the axis: x = c, or y = c"},
  };
  return F[strchr(codes, c) - codes];
}
static const char * const AF_SPEC[6] = {"12ab", "12aby", "12abx", "cAB", "cABy", "fAB"}; // m_apps' order
static const char * const AF_NAME[6] = {"area(", "volume(", "volume(", "arclength(", "surfacearea(", "average("};

// application k's call from its fields; 0 (*bad: the field to fill first, af_err why) if one is missing
static const char * af_call(int k, int * bad) {
  static char c[16 + 5 * AF_W];
  const char * sp = AF_SPEC[k];
  const bool a = af_val[AF_A][0], b = af_val[AF_B][0];
  for (int i = 0; sp[i]; ++i) { // needed, or a bound without the other
    const af_field & f = af_field_of(sp[i]);
    if (!af_val[f.role][0] && (!f.empty || (f.role == AF_A && b) || (f.role == AF_B && a))) {
      *bad = i;
      af_err = f.empty ? "this one too, or neither" : "fill this in first";
      return 0;
    }
  }
  strcpy(c, AF_NAME[k]);
  strcat(c, af_val[AF_F]);
  if (strchr(sp, '2') && (af_val[AF_G][0] || !a)) { // one curve and no bounds: with the x-axis
    strcat(c, ",");
    strcat(c, af_val[AF_G][0] ? af_val[AF_G] : "0");
  }
  if (a) { strcat(c, ","); strcat(c, af_val[AF_A]); strcat(c, ","); strcat(c, af_val[AF_B]); }
  if (const char * x = strpbrk(sp, "xy")) { // the axis: c alone is y = c (x = c for shells)
    strcat(c, *x == 'x' ? ",x=" : ",y=");
    const char * v = af_val[AF_X];
    if (strchr(v, '=')) c[strlen(c) - 2] = 0;
    strcat(c, *v ? v : "0");
  }
  strcat(c, ")");
  return c;
}

// key typed in field s at *p (m: the metrics it is drawn with), as on the edit line (console.cc):
// its 2D keys, ^ and sin( with their parentheses, deleting by structure. 0: not an editing key
static int af_edit(char * s, int * p, int key, const mi_metrics & m) {
  const int len = strlen(s), q = *p;
  if (key == KEY_CTRL_LEFT || key == KEY_CTRL_RIGHT) { *p = mi_move(s, len, q, key == KEY_CTRL_RIGHT, m); return 1; }
  if (key == KEY_CTRL_DEL) {
    int depth = 0, from, to;
    for (int i = 0; i < q - 1; ++i) depth += s[i] == '(' ? 1 : s[i] == ')' ? -1 : 0;
    if (q && s[q - 1] == ')' && depth > 0) { *p = q - 1; return 1; } // sin(x)| -> sin(x|)
    if (me_backspace(s, p)) return 1;
    if (q >= 2 && q < len && (s[q - 2] == '/' || s[q - 2] == '^') && s[q - 1] == '(' && s[q] == ')') { from = q - 2; to = q + 1; }
    else if (q && s[q - 1] == '/') { from = q - 1; to = q; }
    else if (q && q < len && s[q - 1] == '(' && s[q] == ')' && !(q >= 2 && namec(s[q - 2]))) { from = q - 1; to = q + 1; }
    else { from = to = q; *p = mi_backspace(s, len, q, from, to, m); }
    if (to > from) { memmove(s + from, s + to, len - to + 1); *p = from; }
    return 1;
  }
  if (key == '\t') key = KEY_CHAR_RECIP; // (the CE's x^-1 key)
  if (key == KEY_CHAR_PMINUS) key = '-';
  const int mk = key == KEY_CHAR_SQUARE ? ME_SQUARE : key == KEY_CHAR_RECIP ? ME_RECIP : key > 0 && key < 128 && strchr("/(),=<>+-[]{}", key) ? key : 0;
  if (mk && me_key(s, AF_W, p, mk)) return 1;
  char t[24];
  const char * w = key >= ' ' && key < 127 ? 0 : keytostring(key, 0, false);
  if (w ? strlen(w) > 20 : key < ' ' || key >= 127) return 0;
  if (w) strcpy(t, w); else { t[0] = (char)key; t[1] = 0; }
  int n = strlen(t), in = n; // in: the caret after the text
  if (!strcmp(t, "^")) { strcpy(t, "^()"); n = 3; in = 2; }
  else if (n > 1 && t[n - 1] == '(') { t[n++] = ')'; t[n] = 0; }
  if (len + n >= AF_W) return 1;
  memmove(s + *p + n, s + *p, len - *p + 1);
  memcpy(s + *p, t, n);
  *p += in;
  return 1;
}

static mi_layout AFL[5]; // the fields laid out
static signed char AFV[5], af_k, af_sel; // (af_k, af_sel: the form shown, its field typed)
static void af_layout(const char * sp, int i, int caret) {
  const char * s = af_val[af_field_of(sp[i]).role];
  AFV[i] = (signed char)ui_math_fit(s, strlen(s), caret, 4, 2 * (pw - 122), AF_RH - 8, MI_F_CALLBOX, AFL[i]);
  key_latch(); // (the form opens in half a second: a key typed meanwhile is kept)
}
// the form's rows [b, e) in the band being drawn (all: with the card)
static void af_band(int b, int e, int all) {
  const int k = af_k, sel = af_sel;
  const char * sp = AF_SPEC[k];
  const int n = strlen(sp), fx = px + (IN == 2 ? 124 : 100), fw = pw - (IN == 2 ? 134 : 110), hy = py + ph - AF_BOT; // (TI look: wider letters)
  if (e <= py) return;
  if (all) { // the card, the application's name
    card_bg();
    ui_draw_text(&ui_tb12, m_apps[k].label, -1, px + 12, py + 20, ui_ramp(1, UC_INK, UC_CARD), 0);
  }
  for (int i = 0; i < n; ++i) { // a field: its label, right-aligned (the word, the variable in math), its box
    const int y = py + AF_TOP + i * AF_RH, by = y + AF_RH / 2 + 4, on = i == sel, bg = on ? UC_ACCSOFT : UC_TILE;
    if (y >= e || y + AF_RH <= b) continue;
    const af_field & f = af_field_of(sp[i]);
    const char * s = af_val[f.role];
    ui_clip(px + 4, y, px + pw - 4, y + AF_RH);
    ui_fill(px + 4, y, pw - 8, AF_RH, ui_col(1, UC_CARD));
    int lx = fx - 8;
    if (f.var) { // (= apart: alone after it, the layout's = shows an empty box)
      mi_layout V;
      const int lv = ui_math_fit(f.var, strlen(f.var), -1, 4, 60, 20, 0, V);
      lx -= ui_text_width(&ui_mu17, "=", -1);
      ui_draw_text(&ui_mu17, "=", -1, lx, by, ui_ramp(1, UC_INK, UC_CARD), 0);
      lx -= V.width + 4;
      ui_math_draw(V, f.var, lv, lx, by, 1, UC_INK, UC_CARD, UC_ACC);
      lx -= 6;
    }
    if (f.word) ui_draw_text(&ui_tr12, f.word, -1, lx - ui_text_width(&ui_tr12, f.word, -1), by, ui_ramp(1, UC_SUB, UC_CARD), 0);
    ui_rrect(1, fx, y + 2, fw, AF_RH - 4, 6, bg, UC_CARD);
    ui_clip(fx + 3, y + 2, fx + fw - 3, y + AF_RH - 2);
    if (*s) {
      const mi_layout & L = AFL[i];
      int x = fx + 8;
      if (on && x + L.cx > fx + fw - 10) x = fx + fw - 10 - L.cx; // (long: scrolled to the caret)
      const int base = y + AF_RH / 2 - (L.asc + L.desc) / 2 + L.asc;
      ui_math_draw(L, s, AFV[i], x, base, 1, UC_INK, bg, UC_ACC);
      if (on) ui_math_caret(L, x, base, 1, UC_ACC);
    } else {
      if (f.empty) ui_draw_text(&ui_tr10, f.empty, -1, fx + 13, by - 1, ui_ramp(1, UC_SUB, bg), 0);
      if (on) ui_fill(fx + 8, y + 7, 2, AF_RH - 14, ui_col(1, UC_ACC));
    }
    ui_noclip();
  }
  if (hy < e && hy + AF_BOT > b) { // the field's hint (or why enter could not compute), what enter does
    ui_clip(px + 4, hy, px + pw - 4, hy + AF_BOT - 3);
    ui_fill(px + 4, hy, pw - 8, AF_BOT - 3, ui_col(1, UC_CARD));
    ui_draw_text(&ui_tr10, af_err ? af_err : af_field_of(sp[sel]).hint, -1, px + 12, hy + 12, ui_ramp(1, af_err ? UC_RED : UC_SUB, UC_CARD), 0);
    const char * w = sel < n - 1 ? "next" : "compute";
    const int kw = ui_text_width(&ui_tb10, "enter", -1), ww = ui_text_width(&ui_tr10, w, -1);
    ui_draw_text(&ui_tr10, w, -1, px + pw - 16 - kw - ww, hy + 12, ui_ramp(1, UC_SUB, UC_CARD), 0);
    ui_draw_text(&ui_tb10, "enter", -1, px + pw - 12 - kw, hy + 12, ui_ramp(1, UC_ACC, UC_CARD), 0);
    ui_noclip();
  }
}
static void af_over(int b, int e) { af_band(b, e, 1); } // (over the stage repainted under a card)
// rows [y0, y1) of the form (all: with the card)
static void af_paint(int y0, int y1, int all) {
  const int rows = ui_band_open(48);
  for (int b = y0; b < y1; b += rows ? rows : y1 - y0) {
    const int e = rows && b + rows < y1 ? b + rows : y1;
    if (rows) { ui_band_begin(b, e); ui_band_load(); }
    else ui_clip(0, b, UI_W, e);
    af_band(b, e, all);
    if (rows) ui_band_end();
    ui_noclip();
    key_latch();
  }
}

// application k's form over the dimmed scene, in the place of the applications' card (its top
// oy): the call to run, 0 when closed. F3, F4 and math open their cards over it: what they insert
// goes in the field (sec, abs, a template)
static const char * app_form(int k, int oy) {
  if (k != af_last) af_val[AF_X][0] = 0;
  af_last = (signed char)k;
  if (focus_form_line && *focus_form_line && strlen(focus_form_line) < AF_W) strcpy(af_val[AF_F], focus_form_line);
  focus_form_line = 0;
  const char * sp = AF_SPEC[k];
  const int n = strlen(sp);
  int caret[5];
  for (int i = 0; i < n; ++i) caret[i] = strlen(af_val[af_field_of(sp[i]).role]);
  af_k = (signed char)k;
  af_sel = 0;
  pw = 310; ph = AF_TOP + n * AF_RH + AF_BOT; px = 1; py = SBOT - ph;
  if (py > oy) focus_repaint(oy - 2, py); // (the applications' card was taller)
  for (int i = 0; i < n; ++i) af_layout(sp, i, i ? -1 : caret[0]);
  af_err = 0;
  af_paint(py, SBOT, 1);
  card_tab(1, 0);
  for (;;) {
    int key, sel = af_sel;
    GetKey(&key);
    if (key == KEY_CTRL_SHIFT || key == KEY_CTRL_ALPHA) continue;
    char * s = af_val[af_field_of(sp[sel]).role];
    const int os = sel, oe = af_err != 0;
    const mi_metrics & m = ui_math_metrics(AFV[sel], MI_F_CALLBOX);
    af_err = 0;
    if (key == KEY_CTRL_EXE || key == KEY_CTRL_OK) {
      if (sel < n - 1) ++sel;
      else if (const char * c = af_call(k, &sel)) return c;
    }
    else if (key == KEY_CTRL_EXIT || key == KEY_CTRL_AC) {
      if (!*s) return 0;
      *s = 0;
      caret[sel] = 0;
    }
    else if (key == KEY_CTRL_QUIT) return 0;
    else if (key == KEY_CTRL_UP || key == KEY_CTRL_DOWN) { // in a fraction, else to the field above or below
      const int up = key == KEY_CTRL_UP, q = mi_move(s, strlen(s), caret[sel], up ? 2 : 3, m);
      if (q >= 0) caret[sel] = q;
      else if (up ? sel > 0 : sel < n - 1) sel += up ? -1 : 1;
    }
    else if (key == KEY_CTRL_F3 || key == KEY_CTRL_F4 || key == KEY_CTRL_SYMB) {
      const int x0 = px, y0 = py, w0 = pw, h0 = ph;
      const char * t = 0;
      int back = 0;
      af_nest = 1;
      const int a = focus_popover(key, &t, &back);
      af_nest = 0;
      const int top = py < y0 ? py : y0, len = strlen(s), tl = t ? strlen(t) : 0;
      px = x0; py = y0; pw = w0; ph = h0;
      if (a == FA_INSERT && tl && len + tl < AF_W) {
        memmove(s + caret[sel] + tl, s + caret[sel], len - caret[sel] + 1);
        memcpy(s + caret[sel], t, tl);
        caret[sel] += tl - back;
      }
      af_layout(sp, sel, caret[sel]);
      focus_repaint_over(top - 2, SBOT, af_over); // the stage under the card, the form over it
      focus_bar_redraw();
      card_tab(1, 0);
      continue;
    }
    else if (!af_edit(s, &caret[sel], key, m)) continue;
    af_sel = (signed char)sel;
    af_layout(sp, sel, caret[sel]);
    const int y = py + AF_TOP + sel * AF_RH, hy = py + ph - AF_BOT;
    af_paint(y, y + AF_RH, 0);
    if (os != sel) {
      af_layout(sp, os, -1);
      af_paint(py + AF_TOP + os * AF_RH, py + AF_TOP + (os + 1) * AF_RH, 0);
    }
    if (os != sel || oe || af_err) af_paint(hy, hy + AF_BOT, 0);
  }
}
#endif

// ------------------------------------------------------------------ the themes
// more > Themes: the themes as a grid in the more card's place, each tile in its own colors
// (palette entries lent by KhiCAS's screens: ui_borrow): its name, four of its colors; the one
// selected shows at once, on the whole screen. enter or its digit keeps it, clear the one before.
enum { TW = 100, THH = 54 };
static void theme_inks() { // the tiles' lent entries; their corners on the card of the theme shown
  const unsigned char * card = ui_theme_now()->rgb[UC_CARD];
  static const unsigned char dot[4] = {UC_ACC, UC_RED, UC_ORANGE, UC_GREEN};
  for (int k = 0; k < UI_NTHEMES; ++k) {
    const ui_theme & t = *ui_themes[k];
    const int b = 32 + 10 * k;
    ui_borrow(b, t.rgb[UC_BG]);
    ui_borrow_mix(b + 1, card, t.rgb[UC_BG], 1); ui_borrow_mix(b + 2, card, t.rgb[UC_BG], 2);
    ui_borrow_mix(b + 3, t.rgb[UC_BG], t.rgb[UC_INK], 1); ui_borrow_mix(b + 4, t.rgb[UC_BG], t.rgb[UC_INK], 2);
    ui_borrow(b + 5, t.rgb[UC_INK]);
    for (int j = 0; j < 4; ++j) ui_borrow(b + 6 + j, t.rgb[dot[j]]);
  }
}
static void theme_tile(int k, int on) {
  const int x = px + 5 + k % 3 * TW, y = py + 5 + k / 3 * THH, b = 32 + 10 * k;
  const unsigned char fill[4] = {ui_col(1, UC_CARD), (unsigned char)(b + 1), (unsigned char)(b + 2), (unsigned char)b};
  const unsigned char ink[4] = {(unsigned char)b, (unsigned char)(b + 3), (unsigned char)(b + 4), (unsigned char)(b + 5)};
  ui_clip(x, y, x + TW, y + THH);
  ui_fill(x, y, TW, THH, ui_col(1, UC_CARD));
  ui_rrect_r(x + 3, y + 3, TW - 6, THH - 6, 7, fill);
  ui_rframe(1, x + 3, y + 3, TW - 6, THH - 6, 7, on ? UC_ACC : UC_LINE, UC_CARD);
  if (on) ui_rframe(1, x + 2, y + 2, TW - 4, THH - 4, 8, UC_ACC, UC_CARD); // (the one shown: 2 px)
  ui_draw_text(&ui_tb12, ui_theme_names[k], -1, x + 13, y + 24, ink, 0);
  for (int j = 0; j < 4; ++j) { // accent, red, orange, green: 7 px squares, their corners cut
    const int dx = x + 13 + 12 * j, dy = y + 32;
    ui_fill(dx + 1, dy, 5, 7, (unsigned char)(b + 6 + j));
    ui_fill(dx, dy + 1, 7, 5, (unsigned char)(b + 6 + j));
  }
  ui_noclip();
}
static void theme_paint(int sel, int y0, int y1, int all) { // rows [y0, y1): the card (all), its tiles
  const int rows = ui_band_open(48);
  for (int b = y0; b < y1; b += rows ? rows : y1 - y0) {
    const int e = rows && b + rows < y1 ? b + rows : y1;
    if (rows) { ui_band_begin(b, e); ui_band_load(); }
    else ui_clip(0, b, UI_W, e);
    if (all) card_bg();
    for (int k = 0; k < UI_NTHEMES; ++k) {
      const int y = py + 5 + k / 3 * THH;
      if (y < e && y + THH > b) theme_tile(k, k == sel);
    }
    if (rows) ui_band_end();
    ui_noclip();
    key_latch();
  }
}
// the grid on tab id, in the place of the card shown there (its top oy)
static void theme_card(int id, int oy) {
  const int k0 = focus_theme();
  int sel = k0;
  pw = 3 * TW + 10; ph = 3 * THH + 10;
  px = 32 + 64 * id - pw / 2;
  if (px < 1) px = 1;
  if (px > UI_W - pw - 1) px = UI_W - pw - 1;
  py = SBOT - ph;
  if (py > oy) focus_repaint(oy - 2, py);
  theme_inks();
  theme_paint(sel, py, SBOT, 1);
  card_tab(id, 0);
  for (;;) {
    int key;
    GetKey(&key);
    if (key == KEY_CTRL_SHIFT || key == KEY_CTRL_ALPHA) continue;
    int ns = sel, done = 0;
    if (key == KEY_CTRL_EXIT || key == KEY_CTRL_AC || key == KEY_CTRL_F5 || key == KEY_CTRL_QUIT) { ns = k0; done = 1; }
    else if (key == KEY_CTRL_EXE || key == KEY_CTRL_OK) done = 2;
    else if (key >= KEY_CHAR_1 && key < KEY_CHAR_1 + UI_NTHEMES) { ns = key - KEY_CHAR_1; done = 2; }
    else if (key == KEY_CTRL_LEFT) ns = sel % 3 ? sel - 1 : sel + 2;
    else if (key == KEY_CTRL_RIGHT) ns = sel % 3 < 2 ? sel + 1 : sel - 2;
    else if (key == KEY_CTRL_UP) ns = sel >= 3 ? sel - 3 : sel + 6;
    else if (key == KEY_CTRL_DOWN) ns = sel < 6 ? sel + 3 : sel - 6;
    if (ns != sel) { // that theme at once, everywhere (the tiles' corners on its card)
      ui_set_theme(ui_themes[ns]);
      theme_inks();
      const int o = py + 5 + sel / 3 * THH, n = py + 5 + ns / 3 * THH;
      sel = ns;
      if (!done) { theme_paint(sel, o, o + THH, 0); if (n != o) theme_paint(sel, n, n + THH, 0); }
    }
    if (done) {
      if (done == 2) focus_set_theme(sel); // (kept: saved)
      return;
    }
  }
}

static signed char pop_last[6], sub_last; // (DeltaCE) each menu opens on the item picked last (sub_last: the applications' card)
int focus_popover(int key, const char ** text, int * back) {
  int id = key == KEY_CTRL_SYMB ? 5 : key - KEY_CTRL_F1;
  if (id < 0 || id > 5) return FA_NONE;
  const fm_menu * sub = 0; // the card opened from a card (the calculus card's applications)
  for (;;) {
    const fm_menu & m = sub ? *sub : MENUS[id];
    const int sel = sub ? sub_last : pop_last[id] < m.n ? pop_last[id] : 0;
    geometry(id, m);
    prepare_kept(m, sub ? 6 : id);
    open_card(m, id, 0, sel);
    cur = id;
    int r = run_keys(m, sel, fk_pop), res = FA_NONE;
    if (r >= 0) {
      if (sub) sub_last = (signed char)r; else pop_last[id] = (signed char)r;
      const fm_item & it = m.items[r];
      if (it.act) res = it.act;
      else { *text = it.text; *back = it.back; res = FA_INSERT; }
    }
#ifdef FLOWCE_STEPS
    if (sub == &M_APPS && r >= 0) { // an application: its form in the card's place; closed, the
      drop_pl();                  // applications' card again
      if ((*text = app_form(r, py))) {
        close_card(0, 1);
        return FA_RUN;
      }
      const int fy = py;
      geometry(id, *sub);
      if (py > fy) focus_repaint(fy - 2, py);
      continue;
    }
    if (res == FA_APPS && !af_nest) { // the applications' card in its place, the scene still dimmed: only the
      const int oy = py;  // strip the calculus card showed above it is repainted (the stage under
      drop_pl();        // it all was, 0.2 s: keys typed meanwhile were lost)
      sub = &M_APPS;
      geometry(id, *sub);
      if (py > oy) focus_repaint(oy - 2, py);
      continue;
    }
#endif
    if (res == FA_THEME) { // the themes' grid in the card's place (not over a form: nothing)
      res = FA_NONE;
#ifdef FLOWCE_STEPS
      if (!af_nest)
#endif
      {
        drop_pl();
        theme_card(id, py);
        close_card(0, 1);
        ui_borrow_end(); // (the tiles gone)
        return FA_NONE;
      }
    }
    // the card's rows are repainted from the stage (not under the command search, which covers
    // the screen at once)
    close_card(0, res != FA_CATALOG);
    if (r > -2) return res;
    id = -2 - r;
    sub = 0;
  }
}
