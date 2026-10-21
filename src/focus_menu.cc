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

extern "C" void GetKey(int * key); // k_csdk.c
const char * console_fkey_label(int layer, int k); // console.cc: an F-key's label (layer 0, 2nd, alpha)

// an item: a template (KhiCAS text, the caret moved back by `back` after insertion; empty
// arguments are drawn as boxes) or an action; pv: the preview when it differs from the template
struct fm_item { const char * text; signed char back; const char * pv; const char * label; const char * hint; char act; };
// kind: 0 a list (number, preview, label), 1 a grid of previews, 2 a grid of cards (a large
// preview over its label)
struct fm_menu { char grid, cols, n; const fm_item * items; };

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
  {"implicitdiff(,x,y)", 5, 0, "Implicit dy/dx", 0, 0}, // (DeltaCE: kmisc.cc, steps.cc)
#ifdef FLOWCE_STEPS // (the steps build: three columns, four rows; the 0 key is the tenth item)
  {"analyze()", 1, 0, "Analyze f(x)", 0, 0}, // (DeltaCE: analyze.cc, the report on the steps' page)
  {"desolve(diff(y,x)=,x,y)", 5, "diff(y,x)=f", "Diff. equation", 0, 0},
  {0, 0, "integrate(f-g,x,a,b)", "Applications", 0, FA_APPS},
#endif
};
#ifdef FLOWCE_STEPS
// the calculus card's applications (steps.cc): each card shows its integral; a, b left empty: where
// the curves meet
static const fm_item m_apps[] = {
  {"area(,,,)", 4, "integrate(f-g,x,a,b)", "Area between curves", 0, 0},
  {"volume(,,,,y=0)", 8, "pi*integrate(R^2-r^2,x,a,b)", "Disks / washers", 0, 0},
  {"volume(,,,,x=0)", 8, "2*pi*integrate(r*h,x,a,b)", "Shells", 0, 0},
  {"arclength(,,)", 3, "integrate(sqrt(1+(dy/dx)^2),x,a,b)", "Arc length", 0, 0},
  {"surfacearea(,,,y=0)", 7, "integrate(2*pi*r,s)", "Surface area", 0, 0},
  {"average(,,)", 3, "1/(b-a)*integrate(f,x,a,b)", "Average value", 0, 0},
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
  {"^()", 1, 0, 0, 0, 0}, {"exp()", 1, 0, 0, 0, 0}, {"ln()", 1, 0, 0, 0, 0}, {"()/()", 4, 0, 0, 0, 0},
  {"=", 0, 0, 0, 0, 0}, {"<=", 0, 0, 0, 0, 0}, {">=", 0, 0, 0, 0, 0}, {"!=", 0, 0, 0, 0, 0},
  {"'", 0, "y'", 0, 0, 0}, // the prime: y'=2y in desolve, f'(x) of a defined f (no key has it)
};
static const fm_item m_more[] = {
  {0, 0, 0, "Graph", "line or answer", FA_GRAPH},
  {0, 0, 0, "All commands", "search", FA_CATALOG},
  {0, 0, 0, "Paper or Night", "theme", FA_THEME},
  {0, 0, 0, "Other plots", "kinds", FA_PLOT},
#ifdef FLOWCE_STEPS
  {0, 0, 0, "Statistics", "data, fits", FA_STATS}, // (DeltaCE: focus_stats.cc)
  {0, 0, 0, "Probability", "curves, tests", FA_PROB}, // (DeltaCE: focus_stats.cc)
  {0, 0, 0, "Practice", "problems", FA_PRACTICE}, // (DeltaCE: focus_practice.cc)
#endif
  {0, 0, 0, "File", "sessions", FA_FILE},
  {0, 0, 0, "Clear history", 0, FA_CLEAR},
};
static const fm_item m_math[] = {
  {"()/()", 4, 0, 0, 0, 0}, {"sqrt()", 1, 0, 0, 0, 0}, {"abs()", 1, 0, 0, 0, 0},
  {"integrate(,x)", 3, 0, 0, 0, 0}, {"integrate(,x,,)", 5, 0, 0, 0, 0}, {"diff(,x)", 3, 0, 0, 0, 0},
  {"limit(,x,)", 4, 0, 0, 0, 0}, {"sum(,k,,)", 5, 0, 0, 0, 0}, {"^()", 1, 0, 0, 0, 0},
};
#define N(a) (char)(sizeof(a) / sizeof(a[0]))
#ifdef FLOWCE_STEPS
#define CALC_COLS 3
static const fm_menu M_APPS = {2, 2, N(m_apps), m_apps};
#else
#define CALC_COLS 2
#endif
static const fm_menu MENUS[6] = { // F1 F2 F3 F4 F5 math
  {0, 1, N(m_alg), m_alg}, {2, CALC_COLS, N(m_calc), m_calc}, {1, 3, N(m_trig), m_trig},
  {1, 4, N(m_sym), m_sym}, {0, 1, N(m_more), m_more}, {1, 3, N(m_math), m_math},
};
#undef N

enum { RH = 30, LW = 252, CH = 40, CH2 = 60, ST = 16, SBOT = 218 };
// a card's cell height: lower cells when its rows would cover the status bar (the steps build's
// calculus card of ten, four rows: 48; the symbols with ', five rows: 38)
static int cellh(const fm_menu & m) {
  const int rows = (m.n + m.cols - 1) / m.cols, h = m.grid == 2 ? CH2 : CH;
  return rows * h + 10 <= SBOT - ST ? h : (SBOT - ST - 10) / rows;
}

static int px, py, pw, ph, cw; // the card
static int hdr;                // focus_list: the title's height at the top of the card
static const char * title;
static int top, vis = 99, rh = RH; // focus_list: the first row shown, the rows shown, their height
static void geometry(int id, const fm_menu & m) {
  if (m.grid) {
    cw = m.grid == 2 ? (m.cols == 3 ? 100 : 150) : m.cols == 3 ? 76 : 60;
    pw = m.cols * cw + 10; ph = ((m.n + m.cols - 1) / m.cols) * cellh(m) + 10;
  } else { // (a long list: vis rows, scrolled; DeltaCE: rows lower than RH when they would cover the status bar)
    const int rows = m.n < vis ? m.n : vis;
    rh = rows * RH + 10 > SBOT - ST - 2 ? (SBOT - ST - 12) / rows : RH;
    pw = LW; ph = rows * rh + 10;
  }
  int anchor = id < 5 ? 32 + 64 * id : UI_W / 2, mg = id < 5 ? 1 : 4; // (DeltaCE: a key's card over its whole tab)
  px = anchor - pw / 2;
  if (px < mg) px = mg;
  if (px > UI_W - pw - mg) px = UI_W - pw - mg;
  py = id == 5 ? ST + (SBOT - ST - ph) / 2 : SBOT - ph; // (DeltaCE: on the bar, joined to its tab)
}

// the box of item k (screen coordinates)
static void item_box(const fm_menu & m, int k, int & x, int & y, int & w, int & h) {
  if (m.grid) { x = px + 5 + (k % m.cols) * cw; h = cellh(m); y = py + 5 + (k / m.cols) * h; w = cw; }
  else { x = px + 5; y = py + 5 + hdr + (k - top) * rh; w = pw - (m.n > vis ? 18 : 10); h = rh; }
}

// the previews of the open menu, laid out once (empty arguments drawn as boxes)
static mi_layout * PL;
static signed char PLV[16];
static void prepare(const fm_menu & m) {
  delete[] PL;
  PL = new mi_layout[m.n];
  for (int k = 0; k < m.n; ++k) {
    const fm_item & it = m.items[k];
    const char * t = it.pv ? it.pv : it.text;
    PLV[k] = -1;
    if (!t || (!m.grid && !it.text)) continue; // (a list's symbol column is text)
    if (m.grid == 2) PLV[k] = (signed char)ui_math_fit(t, strlen(t), -1, 3, cw - 12, cellh(m) == CH2 ? 36 : 26, MI_F_CALLBOX, PL[k]);
    else if (m.grid) PLV[k] = (signed char)ui_math_fit(t, strlen(t), -1, 3, cw - 8, cellh(m) - 4, MI_F_CALLBOX, PL[k]);
    else PLV[k] = (signed char)ui_math_fit(t, strlen(t), -1, 4, 100, RH - 2, MI_F_CALLBOX, PL[k]);
  }
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

static __attribute__((noinline)) void paint_item(const fm_menu & m, int k, int sel) {
  const fm_item & it = m.items[k];
  int x, y, w, h;
  item_box(m, k, x, y, w, h);
  int on = k == sel, bg = on ? UC_ACC : UC_CARD, fg = on ? UC_ONACC : UC_INK, sub = on ? UC_ONACC : UC_SUB;
  ui_clip(x, y, x + w, y + h);
  ui_fill(x, y, w, h, ui_col(1, UC_CARD));
  if (m.grid) {
    if (on) ui_rrect(1, x + 1, y + 1, w - 2, h - 2, 6, UC_ACC, UC_CARD);
    if (m.grid == 2) { // a card: the preview, its label under it (a label alone: centered)
      int alone = PLV[k] < 0;
      preview(k, it.pv ? it.pv : it.text, x + 4, y + (h == CH2 ? 23 : 18), w - 8, fg, bg, 1);
      if (it.label) {
        const ui_face * f = alone ? (on ? &ui_tb12 : &ui_tr12) : on ? &ui_tb10 : &ui_tr10;
        int lw = ui_text_width(f, it.label, -1);
        ui_draw_text(f, it.label, -1, x + (w - lw) / 2, alone ? y + h / 2 + 5 : y + h - 8, ui_ramp(1, alone ? fg : sub, bg), 0);
      }
    } else preview(k, it.pv ? it.pv : it.text, x + 4, y + h / 2, w - 8, fg, bg, 1);
  } else {
    if (on) ui_rrect(1, x, y, w, h, 6, UC_ACC, UC_CARD);
    char num[2] = {(char)('1' + k), 0};
    int nw = ui_text_width(&ui_tb9, num, -1);
    if (k < 9) { // a keycap
      ui_rframe(1, x + 9, y + h / 2 - 7, 13, 13, 3, sub, bg);
      ui_draw_text(&ui_tb9, num, -1, x + 16 - nw / 2, y + h / 2 + 4, ui_ramp(1, sub, bg), 0);
    }
    int lx = x + 30;
    if (it.text) {
      ui_clip(x + 26, y, x + 128, y + h);
      preview(k, it.pv ? it.pv : it.text, x + 28, y + h / 2, 98, fg, bg, 0);
      ui_clip(x, y, x + w, y + h);
      lx = x + 132;
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

// paints rows [y0, y1) of the card (and its shadow) over what the screen shows
static void paint_rows(const fm_menu & m, int y0, int y1, int sel, int all) {
  int rows = ui_band_open(48);
  for (int b = y0; b < y1; b += rows ? rows : y1 - y0) {
    int e = rows && b + rows < y1 ? b + rows : y1;
    if (rows) { ui_band_begin(b, e); ui_band_load(); }
    else ui_clip(0, b, UI_W, e);
    if (all) { // the card: a shadow (dimmed, bank 0), the white card, a hairline inside its edge
      ui_rrect(0, px + 1, py + 3, pw, ph, 8, UC_SHADOW, UC_BG);
      ui_rrect(1, px, py, pw, ph, 8, UC_CARD, UC_DIM);
      ui_rframe(1, px + 1, py + 1, pw - 2, ph - 2, 7, UC_LINE, UC_CARD);
      if (hdr) {
        int tw = ui_text_width(&ui_tb12, title, -1);
        ui_draw_text(&ui_tb12, title, -1, px + (pw - tw) / 2, py + 22, ui_ramp(1, UC_INK, UC_CARD), 0);
      }
    }
    for (int k = 0; k < m.n; ++k) { // the items this band touches
      if (!m.grid && (k < top || k >= top + vis)) continue; // scrolled out of a long list
      int x, y, w, h;
      item_box(m, k, x, y, w, h);
      if (y < e && y + h > b) paint_item(m, k, sel);
    }
    if (!m.grid && m.n > vis) { // a long list's scroll bar
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
static void open_card(const fm_menu & m, int col, int layer, int sel) {
  ui_dim(1);
  paint_rows(m, py, col < 5 ? SBOT : py + ph + 4, sel, 1);
  if (col < 5) { // (DeltaCE) its tab, joined to the card: the card rises out of its key
    int band = ui_band_open(32) >= 32;
    if (band) { ui_band_begin(SBOT - 4, UI_H); ui_band_load(); }
    if (tab_word_) tab_text(col, tab_word_, UC_ACC);
    else if (!layer) focus_tab(col, 1, 1);
    else tab_text(col, console_fkey_label(layer, col), layer == 1 ? UC_BLUE : UC_GREEN);
    ui_clip(0, SBOT - 4, UI_W, UI_H);
    ui_fill(32 + 64 * col - 29, SBOT - 3, 58, 4, ui_col(1, UC_CARD)); // (the card's edge over the tab: gone)
    ui_noclip();
    if (band) ui_band_end();
  }
  ui_band_close();
}
// closes the card: the scene comes back; under it, the stage is repainted (repaint) unless a
// view owns the screen (the graph redraws itself); bar0: the bar's plain layer
static void close_card(int bar0, int repaint) {
  delete[] PL;
  PL = 0;
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
// the bottom of the column (down: the reverse); in a list, up from the top is the bottom.
static int run_keys(const fm_menu & m, int sel, int (*fk)(int)) {
  for (;;) {
    int k;
    GetKey(&k);
    if (k == KEY_CTRL_SHIFT || k == KEY_CTRL_ALPHA) continue;
    if (k == KEY_CTRL_EXIT || k == KEY_CTRL_AC) return -1;
    int f = fk ? fk(k) : 1, n = m.n, c = m.grid ? m.cols : 1, ns = sel;
    if (f != 1) return f;
    if (m.grid) {
      const int r0 = sel - sel % c, r1 = r0 + c - 1 < n - 1 ? r0 + c - 1 : n - 1; // the row
      if (k == KEY_CTRL_LEFT) ns = sel > r0 ? sel - 1 : r1;
      if (k == KEY_CTRL_RIGHT) ns = sel < r1 ? sel + 1 : r0;
      if (k == KEY_CTRL_UP) ns = sel >= c ? sel - c : sel + (n - 1 - sel) / c * c;
      if (k == KEY_CTRL_DOWN) ns = sel + c < n ? sel + c : sel % c;
    } else {
      if (k == KEY_CTRL_UP) ns = sel > 0 ? sel - 1 : n - 1;
      if (k == KEY_CTRL_DOWN) ns = sel < n - 1 ? sel + 1 : 0;
    }
    if (k >= KEY_CHAR_1 && k <= KEY_CHAR_9 && k < KEY_CHAR_1 + n) return k - KEY_CHAR_1; // not = < > (past 9)
    if (k == KEY_CHAR_0 && n == 10) return 9; // (the steps build's calculus card: its tenth item)
    if (k == KEY_CTRL_EXE || k == KEY_CTRL_OK) return sel;
    if (ns != sel) {
      int old = sel, nt = ns < top ? ns : ns >= top + vis ? ns - vis + 1 : top;
      sel = ns;
      if (nt != top) { // a long list scrolled: all its rows
        top = nt;
        paint_rows(m, py + 5 + hdr, py + 5 + hdr + vis * rh, sel, 0);
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
void focus_table(const char * const * head, int ncol, const char * const * cells, int nrow, const char * hint) {
  const int y0 = 18, hh = 24, rh2 = 18, cwid = UI_W / ncol;
  int rows = ui_band_open(48);
  for (int b = y0; b < UI_H; b += rows ? rows : UI_H - y0) {
    int e = rows && b + rows < UI_H ? b + rows : UI_H;
    if (rows) ui_band_begin(b, e);
    else ui_clip(0, b, UI_W, e);
    ui_fill(0, y0, UI_W, UI_H - y0, ui_col(0, UC_BG));
    ui_fill(0, y0, UI_W, hh, ui_col(0, UC_ACCSOFT));
    for (int c = 0; c < ncol; ++c)
      ui_draw_text(&ui_tb12, head[c], fit(&ui_tb12, head[c], cwid - 12), c * cwid + 10, y0 + 17, ui_ramp(0, UC_ACC, UC_ACCSOFT), 0);
    for (int r = 0; r < nrow; ++r) {
      int y = y0 + hh + r * rh2, bg = r & 1 ? UC_CARD : UC_BG;
      if (r & 1) ui_fill(0, y, UI_W, rh2, ui_col(0, UC_CARD));
      for (int c = 0; c < ncol; ++c) {
        const char * t = cells[r * ncol + c];
        ui_draw_text(&ui_tr12, t, fit(&ui_tr12, t, cwid - 12), c * cwid + 10, y + 14, ui_ramp(0, c ? UC_INK : UC_SUB, bg), 0);
      }
    }
    for (int c = 1; c < ncol; ++c)
      ui_fill(c * cwid, y0, 1, hh + nrow * rh2, ui_col(0, UC_LINE));
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
// the start screen: FlowCE written letter by letter over a curve drawn left to right, then held
// (about 1 s in all, once per start; any key skips it and is not typed); first (no saved
// session): three tips and "press any key" (the caller reads it)
void focus_splash(int first) {
  static bool animated;
  ui_noclip();
  ui_fill(0, 0, UI_W, UI_H, ui_col(0, UC_BG));
  static const char * const cr[] = {"by Tommy Bell, based on KhiCAS by Bernard Parisse",
                                    "version " FLOWCE_VERSION " \xc2\xb7 free software, GNU GPL v3"};
  for (int k = 0; k < 2; ++k)
    ui_draw_text(&ui_tr10, cr[k], -1, (UI_W - ui_text_width(&ui_tr10, cr[k], -1)) / 2, UI_H - 24 + 16 * k, ui_ramp(0, UC_SUB, UC_BG), 0);
  const char * name = "FlowCE"; // (the upright math faces have no capitals)
  const int w = ui_text_width(&ui_mi34, name, -1), w4 = ui_text_width(&ui_mi34, name, 4), x0 = (UI_W - w) / 2, y = first ? 88 : 100;
  static const signed char wave[16] = {0, 43, 79, 103, 112, 103, 79, 43, 0, -43, -79, -103, -112, -103, -79, -43}; // 7 sin, 1/16 px
  const int wx = 48, np = 57, wy = (y + 46) * 16; // 57 points 4 px apart: a period of 64 px
  int skip = animated, drawn = 0;
  animated = true;
  for (int f = 1; f <= 12; ++f) {
    const int k = f < 6 ? f : 6, p = f == 12 ? np : f * np / 12;
    ui_draw_text(&ui_mi34, name, k < 4 ? k : 4, x0, y, ui_ramp(0, UC_INK, UC_BG), 0);
    if (k > 4) ui_draw_text(&ui_mi34, name + 4, k - 4, x0 + w4, y, ui_ramp(0, UC_ACC, UC_BG), 0);
    for (; drawn + 1 < p; ++drawn)
      ui_seg16((wx + 4 * drawn) * 16, wy - wave[drawn & 15], (wx + 4 * drawn + 4) * 16, wy - wave[(drawn + 1) & 15], 28, ui_ramp(0, UC_ACC, UC_BG));
    if (skip) continue; // the rest at once
    if (key_waiting()) { skip = 1; key_discard(); continue; }
    os_wait_1ms(40);
  }
  const char * tag = "calculus on your TI-84 Plus CE";
  ui_draw_text(&ui_tr12, tag, -1, (UI_W - ui_text_width(&ui_tr12, tag, -1)) / 2, y + 26, ui_ramp(0, UC_SUB, UC_BG), 0);
  if (first) { // (the console shows its keys once past this screen)
    const char * go = "press any key";
    ui_draw_text(&ui_tb12, go, -1, (UI_W - ui_text_width(&ui_tb12, go, -1)) / 2, y + 82, ui_ramp(0, UC_ACC, UC_BG), 0);
  }
  else if (!skip) { // held a moment (it flashed by: the session loads fast)
    for (int t = 0; t < 10 && !key_waiting(); ++t)
      os_wait_1ms(50);
    key_discard();
  }
  focus_invalidate();
}

// a value prompt (KhiCAS's inputline): title t, label sub over an edit field. Keys as inputline:
// characters, DEL, AC/CLEAR (clears, then cancels), arrows, EXE. Returns KEY_CTRL_EXE or
// KEY_CTRL_EXIT.
int focus_prompt(const char * t, const char * sub, std::string & s, bool numeric) {
  const int w = 280, x = (UI_W - w) / 2, h = 104, y = ST + (SBOT - ST - h) / 2, fx = x + 12, fy = y + 50, fw = w - 24, fh = 28;
  ui_dim(1);
  note_card(x, y, w, h);
  if (t) ui_draw_text(&ui_tb12, t, fit(&ui_tb12, t, fw), fx, y + 24, ui_ramp(1, UC_INK, UC_CARD), 0);
  ui_draw_text(&ui_tr10, "EXE: OK    EXIT: cancel", -1, fx, y + h - 10, ui_ramp(1, UC_SUB, UC_CARD), 0);
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
  for (int k = 0; k < n; ++k) { it[k].text = maths ? maths[k] : 0; it[k].back = 0; it[k].pv = 0; it[k].label = labels[k]; it[k].hint = hints ? hints[k] : 0; it[k].act = 0; }
  const fm_menu m = {0, 1, (char)(n > 60 ? 60 : n), it};
  vis = 6; top = sel >= vis ? sel - vis + 1 : 0; hdr = 0; rh = RH;
  tab_word_ = word;
  geometry(col, m);
  prepare(m);
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

static signed char pop_last[6], sub_last; // (DeltaCE) each menu opens on the item picked last (sub_last: the applications' card)
int focus_popover(int key, const char ** text, int * back) {
  int id = key == KEY_CTRL_SYMB ? 5 : key - KEY_CTRL_F1;
  if (id < 0 || id > 5) return FA_NONE;
  const fm_menu * sub = 0; // the card opened from a card (the calculus card's applications)
  for (;;) {
    const fm_menu & m = sub ? *sub : MENUS[id];
    const int sel = sub ? sub_last : pop_last[id] < m.n ? pop_last[id] : 0;
    geometry(id, m);
    prepare(m);
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
    if (res == FA_APPS) { // the applications' card in its place
      close_card(0, 1);
      sub = &M_APPS;
      continue;
    }
#endif
    // the card's rows are repainted from the stage (not under the command search, which covers
    // the screen at once)
    close_card(0, res != FA_CATALOG);
    if (r > -2) return res;
    id = -2 - r;
    sub = 0;
  }
}
