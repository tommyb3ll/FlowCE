// focus_stats.cc - DeltaCE's statistics (more > Statistics) and probability (more > Probability,
// below): data in two columns, x and y (giac's
// lists L1 and L2, shared with the console: mean(L1) there), a column's statistics with its box
// plot, a curve fitted to the points (linear, quadratic, cubic, exponential, power, logarithmic)
// with r², and the points with the curve on the graph screen. The numbers are giac's (kmisc.cc
// built WITH_STATS): exact when the data are (the mean of 1, 2, 3, 4 is 5/2); the quartiles the
// TI's way (the medians of the halves, the median left out of them).
#ifdef FLOWCE_STEPS
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <giac/giacPCH.h> // first: its "#define std ustl"
#include "k_csdk.h"
#include "k_defs.h"
#include "focus.h"
#include "ui_gfx.h"
#include "ui_font.h"
#include "ui_fontdata.h"
#include "ui_math.h"
#define PNOINL __attribute__((noinline))
#include "prob_math.h"
namespace giac { gen _logarithmic_regression(const gen & g, GIAC_CONTEXT); } // (kmisc.cc: not in misc.h)
using namespace giac;
extern giac::context * contextptr;
extern "C" void GetKey(int * key); // k_csdk.c
// a key: clear and ON also set giac's stop flags, and every evaluation after them would fail
// ("Stopped by user interruption"): the screens here compute after such keys
static __attribute__((noinline)) int key_() { int k; GetKey(&k); giac::ctrl_c = giac::interrupted = false; return k; }
#define NOINL __attribute__((noinline))

int focus_prompt(const char * title, const char * label, std::string & s, bool numeric); // focus_menu.cc
static void plot_points(const char * f); // (below)

enum { SB = 16, MB = 22, SBOT = UI_H - MB, Y0 = SB, HH = 24, RH = 18, NV = 8, FY = Y0 + HH + NV * RH, MAXN = 99,
       CW0 = 34, CW1 = 177 };
static const int CX[3] = {CW0, CW1, UI_W}; // the columns' right edges: row numbers, x, y

static unsigned char col(int c) { return ui_col(0, c); }
static const unsigned char * ramp(int fg, int bg) { return ui_ramp(0, fg, bg); }
static int text(const ui_face * f, const char * s, int x, int base, int fg, int bg, int align = 0) {
  const int w = ui_text_width(f, s, -1);
  ui_draw_text(f, s, -1, align == 2 ? x - w : align == 1 ? x - w / 2 : x, base, ramp(fg, bg), 0);
  return w;
}
// s in giac's syntax drawn as math at most w wide (smaller levels if needed): its width
static NOINL int math(const char * s, int x, int base, int maxlv, int w, int fg, int bg, int align = 0) {
  mi_layout L;
  const int lv = ui_math_fit(s, strlen(s), -1, maxlv, w, 60, MI_F_IMPLDOT, L);
  ui_math_draw(L, s, lv, align == 2 ? x - L.width : align == 1 ? x - L.width / 2 : x, base, 0, fg, bg, UC_ACC);
  return L.width;
}

// ---------------------------------------------------------------- the data
static vecteur * D_; // the two columns (heap: KhiCAS has no static constructors)
static const char * const LNAME[2] = {"L1", "L2"};
static NOINL gen lname(int k) { return gen(identificateur(LNAME[k])); }
static NOINL void load() { // from L1 and L2 (the console may have set them)
  if (!D_) D_ = new vecteur[2];
  for (int k = 0; k < 2; ++k) {
    const gen v = eval(lname(k), 1, contextptr);
    D_[k] = v.type == _VECT ? *v._VECTptr : vecteur();
  }
}
static NOINL void save(int k) { sto(gen(D_[k], 0), lname(k), contextptr); }
static int npts() { const int a = D_[0].size(), b = D_[1].size(); return a < b ? a : b; } // (x, y) pairs
static int nrows() { const int a = D_[0].size(), b = D_[1].size(); return a > b ? a : b; }

// a value as shown: exact if short (an integer, 5/2), else 6 significant digits
static NOINL bool ends_dec(const gen & g) { // a fraction whose decimals end (its denominator 2^a 5^b): 5/2, 3/8
  if (g.type != _FRAC || g._FRACptr->den.type != _INT_) return false;
  int d = g._FRACptr->den.val;
  while (d % 2 == 0) d /= 2;
  while (d % 5 == 0) d /= 5;
  return d == 1;
}
static NOINL void shown(const gen & g, char * b, int n) {
  std::string s = g.print(contextptr);
  if (ends_dec(g) || s.size() > 9 || s.find("sqrt") != std::string::npos || s.find('e') != std::string::npos) s = evalf(g, 1, contextptr).print(contextptr);
  if (s.size() > 2 && s.compare(s.size() - 2, 2, ".0") == 0) s = s.substr(0, s.size() - 2);
  strncpy(b, s.c_str(), n - 1); b[n - 1] = 0;
}
static NOINL void itos(int v, char * b) { // (no sprintf: it is big)
  char t[8]; int k = 0;
  do t[k++] = (char)('0' + v % 10); while ((v /= 10) && k < 7);
  for (int j = 0; j < k; ++j) b[j] = t[k - 1 - j];
  b[k] = 0;
}
static NOINL bool fin_(float v) { // a number (the CE's float routines do not compare NaN as such: its bits)
  union { float f; unsigned long u; } q;
  q.f = v;
  return (q.u & 0x7f800000UL) != 0x7f800000UL;
}
static NOINL float fv(const gen & g) { const gen e = evalf_double(g, 1, contextptr); return e.type == _DOUBLE_ ? (float)e._DOUBLE_val : NAN; }

// ---------------------------------------------------------------- the data screen
static int sr, sc, top;     // the cell selected (row, 0: x, 1: y), the first row shown
static char edb[24];        // the value being typed (editing), edn characters
static int edn, editing;
static const char * emsg;   // a message in the footer until the next key ("Not a number")
static const char * const DATA_TABS[5] = {"x stats", "y stats", "fit", "plot", "edit"};

static NOINL void paint_data() {
  const int rows = ui_band_open(48), n0 = D_[0].size(), n1 = D_[1].size(), nr = nrows();
  for (int b = Y0; b < SBOT; b += rows ? rows : SBOT - Y0) {
    const int e = rows && b + rows < SBOT ? b + rows : SBOT;
    if (rows) ui_band_begin(b, e); else ui_clip(0, b, UI_W, e);
    ui_fill(0, Y0, UI_W, SBOT - Y0, col(UC_BG));
    ui_fill(0, Y0, UI_W, HH, col(UC_ACCSOFT));
    for (int k = 0; k < 2; ++k) { // x  L1 | y  L2
      const int x = CX[k] + 14;
      const int w = text(&ui_mi17, k ? "y" : "x", x, Y0 + 17, UC_ACC, UC_ACCSOFT);
      text(&ui_tr10, LNAME[k], x + w + 6, Y0 + 16, UC_SUB, UC_ACCSOFT);
    }
    for (int r = 0; r < NV; ++r) {
      const int i = top + r, y = Y0 + HH + r * RH, bg = r & 1 ? UC_CARD : UC_BG;
      if (r & 1) ui_fill(0, y, UI_W, RH, col(UC_CARD));
      if (i > nr) continue;
      char num[8];
      if (i < nr) { itos(i + 1, num); text(&ui_tr10, num, CW0 - 8, y + 13, UC_SUB, bg, 2); }
      for (int k = 0; k < 2; ++k) {
        const bool on = i == sr && k == sc, has = i < (k ? n1 : n0);
        const int x0 = CX[k] + 4, w = CX[k + 1] - CX[k] - 8;
        int cbg = bg;
        if (on) { ui_rrect(0, x0, y + 1, w, RH - 2, 5, UC_ACCSOFT, bg); ui_rframe(0, x0, y + 1, w, RH - 2, 5, UC_ACC, bg); cbg = UC_ACCSOFT; }
        if (on && editing) { // the value as typed, the caret after it
          text(&ui_tr12, edb, CX[k + 1] - 14, y + 14, UC_INK, cbg, 2);
          ui_fill(CX[k + 1] - 13, y + 4, 2, RH - 8, col(UC_ACC));
        } else if (has) {
          char c[24]; shown(D_[k][i], c, sizeof c);
          text(&ui_tr12, c, CX[k + 1] - 14, y + 14, on ? UC_ACC : UC_INK, cbg, 2);
        } else if (i == nr) text(&ui_tr12, "+", CX[k + 1] - 14, y + 14, UC_LINE, cbg, 2); // (the row to add)
      }
    }
    for (int k = 1; k < 3; ++k) if (CX[k - 1] < UI_W) ui_fill(CX[k - 1], Y0, 1, HH + NV * RH, col(UC_LINE));
    ui_fill(0, FY, UI_W, 1, col(UC_LINE));
    char f[24]; // the footer: how many points, the keys
    const int np = npts();
    if (emsg) text(&ui_tb10, emsg, 10, FY + 21, UC_RED, UC_BG);
    else if (editing) text(&ui_tr10, "enter: keep it    clear: cancel", 10, FY + 21, UC_SUB, UC_BG);
    else if (!nr) text(&ui_tr10, "Type the first x, then enter. Right arrow: the y column.", 10, FY + 21, UC_SUB, UC_BG);
    else {
      itos(np, f);
      strcat(f, np == 1 ? " point" : " points");
      const int w = text(&ui_tb10, f, 10, FY + 21, UC_INK, UC_BG);
      text(&ui_tr10, "type to change a value    del: delete", 24 + w, FY + 21, UC_SUB, UC_BG);
    }
    if (rows) ui_band_end();
  }
  ui_band_close();
  ui_noclip();
}

// ---------------------------------------------------------------- a column's statistics
struct onevar { int n; gen mean, sum, sx, px, mn, q1, med, q3, mx; };
static NOINL gen medof(const vecteur & s, int a, int b) { // the median of s[a, b), sorted
  const int m = b - a;
  if (m <= 0) return undef;
  return m & 1 ? s[a + m / 2] : ratnormal((s[a + m / 2 - 1] + s[a + m / 2]) / 2, contextptr);
}
static NOINL bool stats1(const vecteur & v, onevar & o) {
  giac::ctrl_c = giac::interrupted = false; // (and below: a menu's clear before)
  o.n = v.size();
  if (!o.n) return false;
  vecteur s(v); // sorted by value (insertion: a few dozen values)
  float * f = (float *)malloc(o.n * sizeof(float));
  if (!f) return false;
  for (int i = 0; i < o.n; ++i) f[i] = fv(s[i]);
  for (int i = 1; i < o.n; ++i)
    for (int j = i; j > 0 && f[j - 1] > f[j]; --j) { const float t = f[j]; f[j] = f[j - 1]; f[j - 1] = t; const gen g = s[j]; s[j] = s[j - 1]; s[j - 1] = g; }
  free(f);
  gen sum = 0;
  for (int i = 0; i < o.n; ++i) sum = sum + s[i];
  o.sum = ratnormal(sum, contextptr);
  o.mean = ratnormal(o.sum / o.n, contextptr);
  o.px = _stddev(gen(v, 0), contextptr);
  o.sx = o.n > 1 ? _stdDev(gen(v, 0), contextptr) : undef;
  o.mn = s[0]; o.mx = s[o.n - 1];
  o.med = medof(s, 0, o.n);
  o.q1 = medof(s, 0, o.n / 2); o.q3 = medof(s, (o.n + 1) / 2, o.n);
  return true;
}

static NOINL void cellv(const char * label, const gen & g, int x, int y, int w) {
  char b[24];
  if (is_undef(g)) strcpy(b, "-"); else shown(g, b, sizeof b);
  text(&ui_tr10, label, x, y, UC_SUB, UC_BG);
  text(&ui_tb12, b, x + w, y, UC_INK, UC_BG, 2);
}
static NOINL void paint_one(int k, const onevar & o) {
  const int rows = ui_band_open(48);
  const float a = fv(o.mn), z = fv(o.mx), q1 = fv(o.q1), md = fv(o.med), q3 = fv(o.q3);
  const float lo = a == z ? a - 1 : a, hi = a == z ? z + 1 : z;
  const int L = 24, R = UI_W - 24, by = 76; // the box plot: from L to R, its middle at by
  const auto px = [&](float v) { return L + (int)((v - lo) * (R - L) / (hi - lo) + 0.5f); };
  for (int b = Y0; b < SBOT; b += rows ? rows : SBOT - Y0) {
    const int e = rows && b + rows < SBOT ? b + rows : SBOT;
    if (rows) ui_band_begin(b, e); else ui_clip(0, b, UI_W, e);
    ui_fill(0, Y0, UI_W, SBOT - Y0, col(UC_BG));
    // the title: x  L1  ·  8 values
    int x = 12;
    x += text(&ui_mi17, k ? "y" : "x", x, Y0 + 22, UC_ACC, UC_BG) + 6;
    x += text(&ui_tr10, LNAME[k], x, Y0 + 21, UC_SUB, UC_BG) + 10;
    char t[24];
    itos(o.n, t);
    strcat(t, o.n == 1 ? " value" : " values");
    text(&ui_tr10, t, x, Y0 + 21, UC_SUB, UC_BG);
    // the box plot: whiskers from the min to Q1 and Q3 to the max, the box Q1 to Q3, the median
    const int A = px(a), B = px(q1), M = px(md), C = px(q3), Z = px(z);
    ui_fill(A, by, B - A, 2, col(UC_INK)); ui_fill(C, by, Z - C + 1, 2, col(UC_INK));
    ui_fill(A, by - 7, 2, 16, col(UC_INK)); ui_fill(Z - 1, by - 7, 2, 16, col(UC_INK));
    ui_rrect(0, B, by - 15, C - B + 1, 32, 4, UC_ACCSOFT, UC_BG);
    ui_rframe(0, B, by - 15, C - B + 1, 32, 4, UC_ACC, UC_BG);
    ui_fill(M - 1, by - 15, 3, 32, col(UC_ACC));
    // the five numbers under it (each centered on its place, kept apart)
    const gen * fives[5] = {&o.mn, &o.q1, &o.med, &o.q3, &o.mx};
    const int px5[5] = {A, B, M, C, Z};
    int last = -100;
    for (int i = 0; i < 5; ++i) {
      char v[24]; shown(*fives[i], v, sizeof v);
      const int w = ui_text_width(&ui_tr10, v, -1);
      int xx = px5[i] - w / 2;
      if (xx < 4) xx = 4;
      if (xx + w > UI_W - 4) xx = UI_W - 4 - w;
      if (xx < last + 6) continue; // (Q1 at the min: once)
      text(&ui_tr10, v, xx, by + 32, i == 2 ? UC_ACC : UC_SUB, UC_BG);
      last = xx + w;
    }
    // the numbers: two columns
    ui_fill(12, 124, UI_W - 24, 1, col(UC_LINE));
    const int c1 = 14, c2 = 172, w = 132, y1 = 141, dy = 15;
    cellv("n", gen(o.n), c1, y1, w);
    cellv("mean", o.mean, c1, y1 + dy, w);
    cellv("sum", o.sum, c1, y1 + 2 * dy, w);
    cellv("sample SD", o.sx, c1, y1 + 3 * dy, w);
    cellv("population SD", o.px, c1, y1 + 4 * dy, w);
    cellv("min", o.mn, c2, y1, w);
    cellv("Q1", o.q1, c2, y1 + dy, w);
    cellv("median", o.med, c2, y1 + 2 * dy, w);
    cellv("Q3", o.q3, c2, y1 + 3 * dy, w);
    cellv("max", o.mx, c2, y1 + 4 * dy, w);
    ui_fill(UI_W / 2, 131, 1, 76, col(UC_LINE));
    if (rows) ui_band_end();
  }
  ui_band_close();
  ui_noclip();
}
static NOINL void one_var(int k) {
  onevar o;
  if (!stats1(D_[k], o)) { focus_note("No data", k ? "Type some values in the y column first." : "Type some values in the x column first."); key_(); return; }
  focus_status_label(k ? "STATS  y" : "STATS  x");
  static const char * const tabs[5] = {"x", "y", "", "", "back"};
  for (;;) {
    focus_bar_tabs(tabs, k);
    paint_one(k, o);
    const int key = key_();
    if ((key == KEY_CTRL_F1 || key == KEY_CTRL_F2) && (key == KEY_CTRL_F2) != k) {
      const int j = key == KEY_CTRL_F2;
      onevar p;
      if (stats1(D_[j], p)) { k = j; o = p; focus_status_label(k ? "STATS  y" : "STATS  x"); }
      continue;
    }
    if (key == KEY_CTRL_EXIT || key == KEY_CTRL_F5 || key == KEY_CTRL_EXE || key == KEY_CTRL_AC) break;
  }
}

// ---------------------------------------------------------------- a fitted curve
static const char * const FIT_NAME[6] = {"Linear", "Quadratic", "Cubic", "Exponential", "Power", "Logarithmic"};
static const char * const FIT_FORM[6] = {"a*x+b", "a*x^2+b*x+c", "a*x^3+b*x^2+c*x+d", "a*b^x", "a*x^b", "a+b*ln(x)"};
struct fitr { int m; gen f; float r2, r; bool has_r; std::string eq, ap; };
// a term c x^p in giac's syntax, after the terms before it (first: no "+"): "+33/7*x", "-x^2"
static NOINL void term(std::string & s, gen c, int p, bool approx, const char * v = "x") { // (v: the variable, ln(x))
  if (approx) c = evalf(c, 1, contextptr);
  if (is_zero(c)) return;
  const bool neg = is_strictly_positive(-c, contextptr);
  if (neg) c = -c;
  s += neg ? "-" : s.size() > 2 ? "+" : "";
  const bool one = is_one(c) && p > 0;
  if (!one) { std::string t = c.print(contextptr); if (t.size() > 2 && t.compare(t.size() - 2, 2, ".0") == 0) t.resize(t.size() - 2); s += t; }
  if (p > 0) { if (!one) s += "*"; s += v; }
  if (p > 1) { s += "^"; s += char('0' + p); }
}
static NOINL std::string dec(const gen & g) { // a coefficient's 6 significant digits
  std::string t = evalf(g, 1, contextptr).print(contextptr);
  if (t.size() > 2 && t.compare(t.size() - 2, 2, ".0") == 0) t.resize(t.size() - 2);
  return t;
}
static NOINL bool fit(int m, fitr & o) {
  giac::ctrl_c = giac::interrupted = false;
  const int n = npts();
  if (n < (m == 2 ? 4 : m == 1 ? 3 : 2)) return false;
  vecteur X(D_[0].begin(), D_[0].begin() + n), Y(D_[1].begin(), D_[1].begin() + n);
  const gen x(identificateur("x"));
  gen f;
  o.m = m; o.has_r = false;
  if (m == 0) {
    const gen r = _linear_regression(makesequence(gen(X, 0), gen(Y, 0)), contextptr);
    if (r.type != _VECT || r._VECTptr->size() != 2) return false;
    const gen a = (*r._VECTptr)[0], b = (*r._VECTptr)[1];
    f = a * x + b;
    o.eq = "y="; term(o.eq, a, 1, false); term(o.eq, b, 0, false);
    o.ap = "y="; term(o.ap, a, 1, true); term(o.ap, b, 0, true);
    o.r = fv(_correlation(makesequence(gen(X, 0), gen(Y, 0)), contextptr)); o.has_r = true;
  } else if (m <= 2) {
    const gen r = _polynomial_regression(makesequence(gen(X, 0), gen(Y, 0), m + 1), contextptr);
    if (r.type != _VECT) return false;
    const vecteur & c = *r._VECTptr;
    f = 0; o.eq = "y="; o.ap = "y=";
    for (unsigned i = 0; i < c.size(); ++i) {
      const int p = c.size() - 1 - i;
      f = f + c[i] * pow(x, gen(p), contextptr);
      term(o.eq, c[i], p, false); term(o.ap, c[i], p, true);
    }
  } else {
    const gen Xf = evalf(gen(X, 0), 1, contextptr), Yf = evalf(gen(Y, 0), 1, contextptr);
    const gen r = m == 3 ? _exponential_regression(makesequence(Xf, Yf), contextptr) : m == 4 ? _power_regression(makesequence(Xf, Yf), contextptr) : _logarithmic_regression(makesequence(Xf, Yf), contextptr);
    if (r.type != _VECT || r._VECTptr->size() != 2) return false;
    const gen p = evalf((*r._VECTptr)[0], 1, contextptr), q = evalf((*r._VECTptr)[1], 1, contextptr);
    // giac: exponential (a, b) is y = b a^x; power (m, b) y = b x^m; logarithmic (a, b) y = a ln(x) + b
    f = m == 3 ? q * pow(p, x, contextptr) : m == 4 ? q * pow(x, p, contextptr) : p * ln(x, contextptr) + q;
    // written as the model is: a*b^x, a*x^b, a+b*ln(x)
    if (m == 3) o.eq = "y=" + dec(q) + "*" + dec(p) + "^x";
    else if (m == 4) o.eq = "y=" + dec(q) + "*x^" + dec(p);
    else { o.eq = "y=" + dec(q); term(o.eq, p, 1, true, "ln(x)"); }
    o.ap.clear();
  }
  if (o.ap == o.eq) o.ap.clear(); // (whole numbers: no decimals to add)
  o.f = f;
  // r² = 1 - SSres / SStot, over the points (floats)
  float my = 0;
  for (int i = 0; i < n; ++i) my += fv(Y[i]);
  my /= n;
  float sr2 = 0, st = 0;
  for (int i = 0; i < n; ++i) {
    const float yi = fv(Y[i]), fi = fv(subst(f, x, X[i], false, contextptr));
    sr2 += (yi - fi) * (yi - fi); st += (yi - my) * (yi - my);
  }
  o.r2 = st > 0 ? 1 - sr2 / st : 1;
  return true;
}
static NOINL void fmt4(char * b, float v) { // 0.9648 (4 decimals)
  const long k = (long)(v * 10000 + (v < 0 ? -0.5f : 0.5f));
  const std::string s = gen(k / 10000.0).print(contextptr);
  strncpy(b, s.c_str(), 15); b[15] = 0;
}
static NOINL void paint_fit(const fitr & o) {
  const int rows = ui_band_open(48), n = npts();
  const std::string & eq = o.eq, & ap = o.ap;
  // the mini plot: the points and the curve in a box at the bottom right
  float x0 = 1e30f, x1 = -1e30f, y0 = 1e30f, y1 = -1e30f;
  for (int i = 0; i < n; ++i) { const float a = fv(D_[0][i]), b = fv(D_[1][i]); if (a < x0) x0 = a; if (a > x1) x1 = a; if (b < y0) y0 = b; if (b > y1) y1 = b; }
  if (x1 - x0 < 1e-6f) { x0 -= 1; x1 += 1; }
  if (y1 - y0 < 1e-6f) { y0 -= 1; y1 += 1; }
  const float mx = (x1 - x0) / 12, my = (y1 - y0) / 8;
  x0 -= mx; x1 += mx; y0 -= my; y1 += my;
  const int PL = 168, PR = UI_W - 12, PT = 132, PB = SBOT - 10;
  const gen x(identificateur("x"));
  int cy[34];
  for (int j = 0; j <= 32; ++j) { const float xv = x0 + (x1 - x0) * j / 32, yv = fv(subst(o.f, x, gen((double)xv), false, contextptr)); cy[j] = fin_(yv) ? PB - (int)((yv - y0) * (PB - PT) / (y1 - y0)) : -9999; }
  for (int b = Y0; b < SBOT; b += rows ? rows : SBOT - Y0) {
    const int e = rows && b + rows < SBOT ? b + rows : SBOT;
    if (rows) ui_band_begin(b, e); else ui_clip(0, b, UI_W, e);
    ui_fill(0, Y0, UI_W, SBOT - Y0, col(UC_BG));
    // the model in words and as a formula
    int x = 12;
    x += text(&ui_tb10, FIT_NAME[o.m], x, Y0 + 20, UC_ACC, UC_BG) + 8;
    math(("y=" + std::string(FIT_FORM[o.m])).c_str(), x, Y0 + 20, 12, 200, UC_SUB, UC_BG);
    // the card: the curve's equation, large; its decimals under it
    ui_rframe(0, 8, Y0 + 30, UI_W - 16, 84, 8, UC_LINE, UC_BG);
    ui_rrect(0, 8, Y0 + 38, 3, 68, 1, UC_ACC, UC_BG);
    math(eq.c_str(), UI_W / 2, ap.empty() ? Y0 + 80 : Y0 + 72, 2, UI_W - 40, UC_INK, UC_BG, 1);
    if (!ap.empty()) { // its decimals, "≈" before them
      const int w = math(ap.c_str(), UI_W / 2 + 8, Y0 + 103, 10, UI_W - 70, UC_SUB, UC_BG, 1);
      text(&ui_tr12, "\xe2\x89\x88", UI_W / 2 + 8 - w / 2 - 8, Y0 + 103, UC_SUB, UC_BG, 2);
    }
    // r and r² (r for a line only), the points
    char v[16];
    int y = PT + 16;
    if (o.has_r) { fmt4(v, o.r); math((std::string("r=") + v).c_str(), 16, y, 6, 140, UC_INK, UC_BG); y += 26; }
    fmt4(v, o.r2); math((std::string("r^2=") + v).c_str(), 16, y, 6, 140, UC_INK, UC_BG); y += 26;
    char np[16];
    itos(n, np);
    strcat(np, " points");
    text(&ui_tr10, np, 16, y - 6, UC_SUB, UC_BG);
    // the mini plot
    ui_rframe(0, PL - 4, PT - 4, PR - PL + 8, PB - PT + 8, 6, UC_LINE, UC_BG);
    ui_clip(PL - 2, b > PT - 2 ? b : PT - 2, PR + 2, e < PB + 2 ? e : PB + 2);
    for (int j = 0; j < 32; ++j)
      if (cy[j] > -9999 && cy[j + 1] > -9999)
        ui_seg16((PL + (PR - PL) * j / 32) * 16, cy[j] * 16, (PL + (PR - PL) * (j + 1) / 32) * 16, cy[j + 1] * 16, 26, ramp(UC_ACC, UC_BG));
    for (int i = 0; i < n; ++i) {
      const int X = PL + (int)((fv(D_[0][i]) - x0) * (PR - PL) / (x1 - x0)), Y = PB - (int)((fv(D_[1][i]) - y0) * (PB - PT) / (y1 - y0));
      ui_rframe(0, X - 3, Y - 3, 7, 7, 3, UC_INK, UC_BG);
    }
    ui_clip(0, b, UI_W, e);
    if (rows) ui_band_end();
  }
  ui_band_close();
  ui_noclip();
}
static NOINL void show_fit(int m) {
  fitr o;
  if (!fit(m, o)) {
    focus_bar_tabs(DATA_TABS, -1); paint_data(); // (the menu's card and open tab gone first: cards do not repaint a view)
    focus_note("Not enough points", m == 2 ? "A cubic needs 4 points: x and y on a row." : m == 1 ? "A quadratic needs 3 points: x and y on a row." : "A fit needs 2 points: x and y on a row.");
    key_();
    return;
  }
  focus_status_label("FIT");
  static const char * const tabs[5] = {"graph", "predict", "", "", "back"};
  for (;;) {
    focus_bar_tabs(tabs, -1);
    paint_fit(o);
    const int key = key_();
    if (key == KEY_CTRL_F1 || key == KEY_CTRL_EXE) { // the points and the curve on the graph screen
      plot_points(o.f.print(contextptr).c_str());
      focus_status_label("FIT");
      continue;
    }
    if (key == KEY_CTRL_F2) { // y at an x: exact when the curve is
      std::string v;
      if (focus_prompt("Predict y", "x =", v, true) == KEY_CTRL_EXE && !v.empty()) {
        giac::ctrl_c = giac::interrupted = false;
        paint_fit(o); // (the prompt's card gone first)
        const gen xv = eval(gen(v, contextptr), 1, contextptr), y = ratnormal(subst(o.f, gen(identificateur("x")), xv, false, contextptr), contextptr);
        char b[24], c[24], t[80];
        shown(xv, b, sizeof b); shown(y, c, sizeof c);
        strcpy(t, "At x = "); strcat(t, b); strcat(t, ", the curve gives y = "); strcat(t, fin_(fv(y)) ? c : "none"); strcat(t, ".");
        focus_note("Predict y", t);
        key_();
      }
      focus_status_label("FIT");
      continue;
    }
    if (key == KEY_CTRL_EXIT || key == KEY_CTRL_F5 || key == KEY_CTRL_AC) break;
  }
}

// ---------------------------------------------------------------- the screen
// a key that types into a value: its character, else 0
static int typed(int key) {
  if (key >= '0' && key <= '9') return key;
  if (key == KEY_CHAR_DP) return '.';
  if (key == KEY_CHAR_PMINUS || key == KEY_CHAR_MINUS) return '-';
  if (key == KEY_CHAR_DIV) return '/';
  if (key == KEY_CHAR_EXPN10) return 'e';
  return 0;
}
// the value typed into the cell selected (a number: 2.5, -3, 7/2, 1e3); false: not one (kept)
static NOINL bool commit() {
  giac::ctrl_c = giac::interrupted = false;
  if (!edn) { editing = 0; return true; }
  edb[edn] = 0;
  gen g(edb, contextptr);
  g = eval(g, 1, contextptr);
  if (!fin_(fv(g)) || g.type == _VECT || (g.type == _SYMB && lidnt(g).size())) { emsg = "Not a number"; return false; }
  vecteur & c = D_[sc];
  if (sr < (int)c.size()) c[sr] = g;
  else { c.push_back(g); sr = c.size() - 1; } // (past the column's end: its next row)
  save(sc);
  editing = 0; edn = 0;
  return true;
}
static NOINL void down() { if (sr < nrows() && sr < MAXN - 1) { ++sr; if (sr >= top + NV) top = sr - NV + 1; } }
static NOINL void del_row(int i) {
  for (int k = 0; k < 2; ++k) if (i < (int)D_[k].size()) { D_[k].erase(D_[k].begin() + i); save(k); }
  if (sr > nrows()) sr = nrows();
}
static NOINL void edit_menu() {
  static const char * const lab[4] = {"Insert a row", "Delete the row", "Sort by x", "Clear all data"};
  static const char * const hint[4] = {"0, 0 above", "del", "y follows", 0};
  const int c = focus_card_list(4, "edit", 0, lab, hint, 4, 0);
  if (c == 0) { for (int k = 0; k < 2; ++k) if (sr <= (int)D_[k].size()) { D_[k].insert(D_[k].begin() + sr, gen(0)); save(k); } }
  if (c == 1) del_row(sr);
  if (c == 2) { // the rows by x (the pairs; values without a pair stay after them)
    const int n = npts();
    for (int i = 1; i < n; ++i)
      for (int j = i; j > 0 && fv(D_[0][j - 1]) > fv(D_[0][j]); --j)
        for (int k = 0; k < 2; ++k) { const gen t = D_[k][j]; D_[k][j] = D_[k][j - 1]; D_[k][j - 1] = t; }
    save(0); save(1);
  }
  if (c == 3) {
    static const char * const yn[2] = {"Clear all data", "Cancel"};
    focus_bar_tabs(DATA_TABS, -1); paint_data();
    if (focus_choose("Clear L1 and L2?", yn, 2) == 0) { D_[0].clear(); D_[1].clear(); save(0); save(1); sr = top = 0; }
  }
}
static NOINL void plot_points(const char * f) { // the points on the graph screen, with f (0: alone)
  giac::ctrl_c = giac::interrupted = false;
  const int n = npts();
  float * px = (float *)malloc(2 * n * sizeof(float) + 1);
  if (!px) return;
  float a = 1e30f, z = -1e30f;
  for (int i = 0; i < n; ++i) { px[i] = fv(D_[0][i]); px[n + i] = fv(D_[1][i]); if (px[i] < a) a = px[i]; if (px[i] > z) z = px[i]; }
  const float w = z - a > 1e-6f ? z - a : 2;
  focus_graph_points(px, px + n, n);
  free(px);
  std::string s = "plot(";
  s += f ? f : "[]";
  s += ",x=" + gen((double)(a - w / 8)).print(contextptr) + ".." + gen((double)(z + w / 8)).print(contextptr) + ")";
  focus_graph_input(s.c_str());
  focus_graph_points(0, 0, 0);
}
void focus_stats() {
  giac::ctrl_c = giac::interrupted = false; // (a key read as a stop before: every evaluation would fail)
  load();
  sr = sc = top = 0; editing = edn = 0; emsg = 0;
  const int view0 = focus_view;
  focus_view = 1;
  focus_status_label("DATA");
  for (;;) {
    focus_bar_tabs(DATA_TABS, -1);
    paint_data();
    const int key = key_();
    emsg = 0;
    if (const int ch = typed(key)) { // a value typed: a new one in the cell
      if (!editing) { editing = 1; edn = 0; }
      if (edn < (int)sizeof(edb) - 2) { edb[edn++] = (char)ch; edb[edn] = 0; }
      continue;
    }
    if (key == KEY_CTRL_DEL) {
      if (editing) { if (edn) edb[--edn] = 0; else editing = 0; }
      else if (sr < nrows()) del_row(sr);
      continue;
    }
    if (key == KEY_CTRL_EXIT || key == KEY_CTRL_AC) { if (editing) { editing = 0; continue; } break; }
    if (key == KEY_CTRL_EXE) {
      if (editing) { if (commit()) down(); }
      else if (sr < (int)D_[sc].size()) { // the value to change: as it is, the caret after it
        std::string v = D_[sc][sr].print(contextptr);
        strncpy(edb, v.c_str(), sizeof(edb) - 2); edb[sizeof(edb) - 2] = 0; edn = strlen(edb); editing = 1;
      }
      continue;
    }
    if (editing && (key == KEY_CTRL_UP || key == KEY_CTRL_DOWN || key == KEY_CTRL_LEFT || key == KEY_CTRL_RIGHT) && !commit()) continue;
    if (key == KEY_CTRL_F1 || key == KEY_CTRL_F2) { one_var(key == KEY_CTRL_F2); focus_status_label("DATA"); continue; }
    if (key == KEY_CTRL_F3) {
      const int m = focus_card_list(2, "fit", FIT_FORM, FIT_NAME, 0, 6, 0);
      if (m >= 0) { show_fit(m); focus_status_label("DATA"); }
      continue;
    }
    if (key == KEY_CTRL_F4) {
      if (npts() < 1) { emsg = "No points yet: x and y in a row"; continue; }
      plot_points(0); focus_status_label("DATA");
      continue;
    }
    if (key == KEY_CTRL_F5) { edit_menu(); continue; }
    if (key == KEY_CTRL_UP && sr > 0) { --sr; if (sr < top) top = sr; }
    if (key == KEY_CTRL_DOWN) down();
    if (key == KEY_CTRL_LEFT) sc = 0;
    if (key == KEY_CTRL_RIGHT) sc = 1;
  }
  focus_view = view0;
  focus_status_label(0);
  focus_invalidate();
}

// ---------------------------------------------------------------- probability (DeltaCE 2.0)
// more > Probability: a distribution (normal, t, chi-square, binomial, Poisson) drawn with the area
// asked for shaded and its value: P(a <= X <= b), a left or right tail, P(X = k); the inverse (the
// x for an area: left, right, centered); and the one-sample intervals and tests (a proportion's z,
// a mean's t, the mean from L1 too) on their curve. The numbers are prob_math.h's floats (giac's
// distributions are compiled out of the calculator, XLIGHT); 4 significant digits shown.
enum { PT_ = Y0 + 30, PB_ = Y0 + 120, PL_ = 12, PR_ = UI_W - 12, PCOLS = (PR_ - PL_) / 2 };
struct pfield { const char * lab; float * v; char kind; }; // kind: 0 any, 1 > 0, 2 whole >= 1, 3 in [0, 1], 4 whole, 5 in (0, 1)
static pfield PF[6];
static int pn, psel;
static float pq[PD_N][2], pa, pb, par, tv[4][4]; // the parameters, bounds, area; the tests' values
static int pd, pinv, ptail, pinit, ptest = -1, talt; // the distribution, inverse, region (below), a test (-1: none), its Ha
// the region: area 0 a <= X <= b, 1 X <= x, 2 X >= x, 3 X = k; inverse 0 left, 1 right, 2 centered
static const char * const PNAME[PD_N] = {"Normal", "t", "Chi-square", "Binomial", "Poisson"};
static float SL[2], SR[2], MK[2], px0, px1; // the shaded intervals, the marks, the x range
static int NS, NM;
static short PH[PCOLS + 1];                 // the curve's heights (static: no big locals)
static char pres[64], pnote[80];            // the result (math), a line of text above it

static NOINL char * fixd(char * p, float v, int d) { // v >= 0 with d decimals, trailing zeros dropped
  long m = 1;
  for (int i = 0; i < d; ++i) m *= 10;
  const long n = (long)(v * m + 0.5f), w = n / m;
  long f = n % m;
  char t[12];
  itos((int)w, t); strcpy(p, t); p += strlen(p);
  if (f) {
    *p++ = '.';
    for (long k = m / 10; k && f; k /= 10) { *p++ = (char)('0' + f / k); f %= k; }
  }
  *p = 0;
  return p;
}
static NOINL void sig4(char * b, float v) { // 4 significant digits: 0.6827, 2.064, 100, 1.35e-7
  if (!fin_(v)) { strcpy(b, "-"); return; }
  if (v < 0) { *b++ = '-'; v = -v; }
  int e = 0, d = 3;
  if (v != 0 && (v < 1e-4f || v >= 1e6f)) { // 1.35e-7
    while (v >= 10) { v /= 10; ++e; }
    while (v < 1) { v *= 10; --e; }
    if (v >= 9.9995f) { v /= 10; ++e; }
    char * p = fixd(b, v, 3);
    *p++ = 'e';
    if (e < 0) { *p++ = '-'; e = -e; }
    itos(e, p);
    return;
  }
  for (float t = v; t >= 10 && d > 0; t /= 10) --d; // (the decimals for 4 digits)
  if (v > 0) for (float t = v; t < 1 && d < 8; t *= 10) ++d;
  fixd(b, v, d);
}
static NOINL void padd(const char * l, float * v, char k) { PF[pn].lab = l; PF[pn].v = v; PF[pn++].kind = k; }
static NOINL void pfields() {
  pn = 0;
  if (ptest >= 0) { // x n C | p0 x n | xbar s n C | mu0 xbar s n ("\x01x": x with a bar)
    static const char * const L[4][4] = {{"x", "n", "C", 0}, {"p0", "x", "n", 0}, {"\x01x", "s", "n", "C"}, {("\xce\xbc" "0"), "\x01x", "s", "n"}};
    static const char K[4][4] = {{4, 2, 5, 0}, {5, 4, 2, 0}, {0, 1, 2, 5}, {0, 0, 1, 2}};
    for (int i = 0; i < 4 && L[ptest][i]; ++i) padd(L[ptest][i], tv[ptest] + i, K[ptest][i]);
    return;
  }
  float * q = pq[pd];
  if (pd == PD_NORM) { padd("\xce\xbc", q, 0); padd("\xcf\x83", q + 1, 1); }
  else if (pd == PD_BIN) { padd("n", q, 2); padd("p", q + 1, 3); }
  else padd(pd == PD_POIS ? "\xce\xbb" : "df", q, 1);
  if (pinv) padd("area", &par, 5);
  else if (ptail == 0) { padd("lower", &pa, 0); padd("upper", &pb, 0); }
  else if (ptail == 3) padd("k", &pa, 4);
  else padd("x", ptail == 1 ? &pb : &pa, 0);
}
static NOINL bool pvalid(float v, int k) {
  if (!fin_(v)) return false;
  if (k == 1) return v > 0;
  if (k == 2 || k == 4) return v == floorf(v) && v >= (k == 2);
  if (k == 3) return v >= 0 && v <= 1;
  if (k == 5) return v > 0 && v < 1;
  return true;
}
// the curve (continuous: its shape, highest = 1) or the bars over [px0, px1]
static NOINL float pshape(int d, const float * q, float x) {
  if (d == PD_NORM) { const float z = (x - q[0]) / q[1]; return expf(-z * z / 2); }
  if (d == PD_T) return expf(-(q[0] + 1) / 2 * logf(1 + x * x / q[0]));
  if (d == PD_CHI) return x <= 0 ? 0 : expf((q[0] / 2 - 1) * logf(x) - x / 2);
  return p_pdf(d, q, floorf(x + 0.5f));
}
static NOINL void pcurve(int d, const float * q) {
  float h[PCOLS + 1], m = 0; // (floats: 4 bytes each, 600 on the stack)
  for (int c = 0; c <= PCOLS; ++c) { h[c] = pshape(d, q, px0 + (px1 - px0) * c / PCOLS); if (c > 1 && fin_(h[c]) && h[c] > m) m = h[c]; }
  for (int c = 0; c <= PCOLS; ++c) { const float y = fin_(h[c]) && m > 0 ? h[c] / m : 0; PH[c] = (short)((y > 1.05f ? 1.05f : y) * (PB_ - PT_ - 4)); }
}
static NOINL int pxof(float x) { return PL_ + (int)((x - px0) * (PR_ - PL_) / (px1 - px0)); }
static NOINL bool pshaded(float x) { for (int i = 0; i < NS; ++i) if (x >= SL[i] - 1e-6f && x <= SR[i] + 1e-6f) return true; return false; }
// a nice step for about 6 labels over the range
static NOINL float pstep(float r) {
  float s = 1;
  while (s * 6 < r) s *= 10;
  while (s * 6 > r * 10) s /= 10; // (s about r/6 .. r/0.6)
  return s * 5 < r ? s * 2 < r / 3 ? s * 5 : s * 2 : s;
}
static NOINL void paint_plot(int b, int e) {
  const bool disc = p_discrete(ptest >= 0 ? (ptest < 2 ? PD_NORM : PD_T) : pd);
  if (disc) { // bars: one per whole number in the range
    const int k0 = (int)ceilf(px0), k1 = (int)floorf(px1);
    const int bw = (PR_ - PL_) / (k1 - k0 + 1);
    float m = 0;
    for (int k = k0; k <= k1; ++k) { const float v = p_pdf(pd, pq[pd], k); if (v > m) m = v; }
    for (int k = k0; k <= k1; ++k) {
      const int h = m > 0 ? (int)(p_pdf(pd, pq[pd], k) / m * (PB_ - PT_ - 4)) : 0, x = pxof(k) - bw / 2;
      if (x + bw < 0 || h <= 0) continue;
      ui_fill(x, PB_ - h, bw > 2 ? bw - 1 : 1, h, col(pshaded(k) ? UC_ACC : UC_LINE));
    }
  } else {
    for (int c = 0; c < PCOLS; ++c) // the area under the curve where shaded
      if (pshaded(px0 + (px1 - px0) * (c + 0.5f) / PCOLS) && PH[c] > 0) ui_fill(PL_ + 2 * c, PB_ - PH[c], 2, PH[c], col(UC_ACCSOFT));
    for (int c = 0; c < PCOLS; ++c)
      ui_seg16((PL_ + 2 * c) * 16, (PB_ - PH[c]) * 16, (PL_ + 2 * c + 2) * 16, (PB_ - PH[c + 1]) * 16, 24, ramp(UC_ACC, UC_BG));
  }
  ui_fill(PL_ - 4, PB_, PR_ - PL_ + 8, 1, col(UC_INK));
  int mx[2];
  for (int i = 0; i < NM; ++i) { // the marks: a line up to the curve, the value under the axis
    mx[i] = pxof(MK[i]);
    if (mx[i] < PL_ - 2 || mx[i] > PR_ + 2) continue;
    const int c = (mx[i] - PL_) / 2, h = disc ? 0 : PH[c < 0 ? 0 : c > PCOLS ? PCOLS : c];
    ui_fill(mx[i], PB_ - h - 4, 1, h + 8, col(UC_ACC));
    char v[16]; sig4(v, MK[i]);
    text(&ui_tb10, v, mx[i], PB_ + 14, UC_ACC, UC_BG, 1);
  }
  const float st = pstep(px1 - px0); // the labels between them
  for (float t = ceilf(px0 / st) * st; t <= px1 + st * 1e-3f; t += st) {
    const int x = pxof(t);
    bool near = false;
    for (int i = 0; i < NM; ++i) near = near || (x > mx[i] - 34 && x < mx[i] + 34);
    ui_fill(x, PB_ + 1, 1, 3, col(UC_SUB));
    if (near) continue;
    char v[16]; sig4(v, fabsf(t) < st * 1e-3f ? 0 : t);
    text(&ui_tr10, v, x, PB_ + 14, UC_SUB, UC_BG, 1);
  }
  (void)b; (void)e;
}
static NOINL int pchip(int i, int x, int y) { // field i, its box's top at y: its width
  const bool on = i == psel;
  const char * l = PF[i].lab;
  int w = text(&ui_tr10, l + (*l == 1), x, y + 13, UC_SUB, UC_BG);
  if (*l == 1) ui_fill(x, y + 2, w, 1, col(UC_SUB)); // (x with a bar)
  w += 4;
  char v[24];
  if (on && editing) strcpy(v, edb); else sig4(v, *PF[i].v);
  const int tw = ui_text_width(&ui_tb12, v, -1), bw = tw + 14 > 30 ? tw + 14 : 30;
  ui_rrect(0, x + w, y, bw, 19, 5, on ? UC_ACCSOFT : UC_BG, UC_BG);
  ui_rframe(0, x + w, y, bw, 19, 5, on ? UC_ACC : UC_LINE, UC_BG);
  text(&ui_tb12, v, x + w + bw / 2, y + 14, on ? UC_ACC : UC_INK, on ? UC_ACCSOFT : UC_BG, 1);
  if (on && editing) ui_fill(x + w + bw / 2 + tw / 2 + 1, y + 4, 2, 11, col(UC_ACC));
  return w + bw + 10;
}
static NOINL void paint_prob() {
  pfields();
  const int rows = ui_band_open(48);
  for (int b = Y0; b < SBOT; b += rows ? rows : SBOT - Y0) {
    const int e = rows && b + rows < SBOT ? b + rows : SBOT;
    if (rows) ui_band_begin(b, e); else ui_clip(0, b, UI_W, e);
    ui_fill(0, Y0, UI_W, SBOT - Y0, col(UC_BG));
    int x = 12, i = 0;
    if (ptest < 0) x += text(&ui_tb12, PNAME[pd], x, Y0 + 18, UC_ACC, UC_BG) + 12;
    const int np = ptest >= 0 || pd == PD_NORM || pd == PD_BIN ? 2 : 1; // (the parameters on top, the region below)
    for (; i < (ptest >= 0 ? pn : np); ++i) x += pchip(i, x, Y0 + 4);
    paint_plot(b, e);
    ui_fill(12, Y0 + 148, UI_W - 24, 1, col(UC_LINE));
    x = 12;
    for (; i < pn; ++i) x += pchip(i, x, Y0 + 154);
    if (*pnote) { // (\x01x, \x02p: x-bar, p-hat)
      const int nx = ptest >= 0 ? 12 : x + 4, m = *pnote < 3, w = text(&ui_tr10, pnote + m, nx, Y0 + 167, UC_SUB, UC_BG);
      if (m) { const int lw = ui_text_width(&ui_tr10, pnote + 1, 1); ui_fill(nx + (*pnote == 2 ? lw / 2 - 1 : 0), Y0 + 156 - (*pnote == 2), *pnote == 2 ? 3 : lw, 1, col(UC_SUB)); if (*pnote == 2) { ui_fill(nx + lw / 2 - 2, Y0 + 156, 1, 1, col(UC_SUB)); ui_fill(nx + lw / 2 + 2, Y0 + 156, 1, 1, col(UC_SUB)); } }
      (void)w;
    }
    if (emsg) text(&ui_tb10, emsg, UI_W - 12, Y0 + 167, UC_RED, UC_BG, 2);
    math(pres, UI_W / 2, SBOT - 8, 4, UI_W - 24, UC_INK, UC_BG, 1);
    if (rows) ui_band_end();
  }
  ui_band_close();
  ui_noclip();
}
static NOINL void pcat(char * d, const char * a, float v) { char b[16]; sig4(b, v); strcat(d, a); strcat(d, b); }
// a test's p-value for the statistic s (normal or t with df), its tails shaded
static NOINL float ptail_p(int d, const float * q, float s) {
  const float l = p_cdf(d, q, -fabsf(s)); // (the small tail, without 1 - its complement)
  NS = talt ? 1 : 2; NM = talt ? 1 : 2; MK[0] = s; MK[1] = -s; // (two-sided: -s marked too)
  SL[0] = -1e30f; SR[0] = talt == 2 ? 1e30f : talt == 1 ? s : -fabsf(s);
  if (talt == 2) SL[0] = s;
  SL[1] = fabsf(s); SR[1] = 1e30f;
  return talt == 0 ? 2 * l : (talt == 1) == (s < 0) ? l : 1 - l;
}
static NOINL void pcompute() {
  *pres = *pnote = 0;
  NS = 1; NM = 0;
  for (int i = 0; i < pn; ++i) if (!pvalid(*PF[i].v, PF[i].kind)) { strcpy(pres, "?"); emsg = "Check the values"; px0 = -4; px1 = 4; NS = 0; pcurve(PD_NORM, pq[PD_NORM]); return; }
  if (ptest >= 0) { // the standard normal or t(n-1), the statistic's tails or the critical values' middle
    const float * v = tv[ptest];
    static float q[2];
    const bool t = ptest >= 2;
    const int d = t ? PD_T : PD_NORM;
    const float n = ptest == 0 ? v[1] : ptest == 1 ? v[2] : t && ptest == 2 ? v[2] : v[3];
    q[0] = t ? n - 1 : 0; q[1] = 1;
    if (t && n < 2) { strcpy(pres, "?"); emsg = "n must be 2 or more"; NS = 0; return; }
    if (ptest == 0 && v[0] > n) { strcpy(pres, "?"); emsg = "x is at most n"; NS = 0; return; }
    if (ptest == 1 && v[1] > n) { strcpy(pres, "?"); emsg = "x is at most n"; NS = 0; return; }
    px0 = -4; px1 = 4;
    if (ptest == 0 || ptest == 2) { // an interval: estimate +- critical value * SE
      const float C = v[ptest == 0 ? 2 : 3], c = p_inv(d, q, (1 + C) / 2, -1000, 1000);
      const float est = ptest == 0 ? v[0] / n : v[0], se = ptest == 0 ? sqrtf(est * (1 - est) / n) : v[1] / sqrtf(n);
      SL[0] = -c; SR[0] = c; NM = 2; MK[0] = -c; MK[1] = c;
      strcpy(pres, "("); pcat(pres, "", est - c * se); pcat(pres, ",", est + c * se); strcat(pres, ")");
      pcat(pnote, ptest == 0 ? "\x02p = " : "\x01x = ", est); // (p-hat, x-bar: the mark drawn over the letter)
      pcat(pnote, "   ME = ", c * se); pcat(pnote, t ? "   t* = " : "   z* = ", c);
      if (t) pcat(pnote, "   df = ", n - 1);
    } else { // a test: the statistic, its p-value
      const float s = ptest == 1 ? (v[1] / n - v[0]) / sqrtf(v[0] * (1 - v[0]) / n) : (v[1] - v[0]) / (v[2] / sqrtf(n));
      const float p = ptail_p(d, q, s);
      if (fabsf(s) > 3.6f) { px0 = -fabsf(s) - 0.6f; px1 = -px0; }
      strcpy(pres, t ? "t=" : "z="); pcat(pres, "", s); pcat(pres, ",  p=", p);
      static const char * const ha[3] = {" \xe2\x89\xa0 ", " < ", " > "};
      strcpy(pnote, t ? "H0: \xce\xbc = " : "H0: p = "); pcat(pnote, "", v[0]);
      strcat(pnote, t ? "    Ha: \xce\xbc" : "    Ha: p"); pcat(pnote, ha[talt], v[0]);
      strcat(pnote, p < 0.05f ? "    p < 0.05: significant" : "    p \xe2\x89\xa5 0.05: not significant");
      if (t) pcat(pnote, "    df = ", n - 1);
    }
    pcurve(d, q);
    return;
  }
  const float * q = pq[pd];
  const bool disc = p_discrete(pd);
  float m = 0, sd = 1; // the range: the mean +- about 4 SDs (chi-square, Poisson: from 0)
  switch (pd) {
  case PD_NORM: m = q[0]; sd = q[1]; break;
  case PD_T: sd = q[0] < 3 ? 1.6f : 1; break;
  case PD_CHI: m = q[0]; sd = sqrtf(2 * q[0]); break;
  case PD_BIN: m = q[0] * q[1]; sd = sqrtf(q[0] * q[1] * (1 - q[1])); break;
  default: m = q[0]; sd = sqrtf(q[0]);
  }
  px0 = m - 4 * sd; px1 = m + 4 * sd;
  if (pd >= PD_CHI && px0 < 0) px0 = 0;
  if (pd == PD_BIN && (px1 > q[0] || q[0] <= 40)) px1 = q[0];
  if (pd == PD_BIN && q[0] <= 40) px0 = 0;
  if (disc) { px0 = floorf(px0); px1 = ceilf(px1); if (px1 - px0 > 140) { px0 = floorf(m - 70); px1 = px0 + 140; } if (px1 - px0 < 4) px1 = px0 + 4; }
  if (pd == PD_CHI && px1 < 8) px1 = 8;
  if (!pinv) {
    float lo = ptail == 1 ? -1e30f : pa, hi = ptail == 2 ? 1e30f : ptail == 3 ? pa : pb;
    if (disc) { lo = ceilf(lo - 1e-4f); hi = floorf(hi + 1e-4f); }
    float v;
    if (hi < lo) v = 0;
    else if (ptail == 2 && pd <= PD_T) v = p_cdf(pd, q, pd == PD_T ? -lo : 2 * q[0] - lo); // (the right tail by symmetry: small ones exact)
    else v = p_cdf(pd, q, hi) - (ptail == 1 ? 0 : p_cdf(pd, q, disc ? lo - 1 : lo));
    SL[0] = lo; SR[0] = hi;
    if (ptail != 1) MK[NM++] = lo;
    if (ptail == 0 || ptail == 1) MK[NM++] = hi;
    if (ptail == 3) NM = 1;
    strcpy(pres, "P(");
    if (ptail == 0) { pcat(pres, "", lo); pcat(pres, "<=X<=", hi); }
    else if (ptail == 3) pcat(pres, "X=", lo);
    else pcat(pres, ptail == 1 ? "X<=" : "X>=", ptail == 1 ? hi : lo);
    pcat(pres, ")=", v);
  } else { // the x for the area
    const float lo = pd == PD_NORM ? q[0] - 40 * q[1] : pd == PD_T ? -1000 : 0, hi = pd == PD_NORM ? q[0] + 40 * q[1] : pd == PD_T ? 1000 : 50 * q[0] + 200;
    const float a = ptail == 0 ? par : ptail == 1 ? 1 - par : (1 + par) / 2, x = p_inv(pd, q, a, lo, hi);
    NM = 1; MK[0] = x;
    if (ptail == 0) { SL[0] = -1e30f; SR[0] = x; strcpy(pnote, "P(X \xe2\x89\xa4 x) = area"); pcat(pres, "x=", x); }
    else if (ptail == 1) { SL[0] = x; SR[0] = 1e30f; strcpy(pnote, "P(X \xe2\x89\xa5 x) = area"); pcat(pres, "x=", x); }
    else {
      const float y = pd == PD_NORM ? 2 * q[0] - x : -x;
      SL[0] = y; SR[0] = x; MK[NM++] = y;
      strcpy(pnote, "the middle area");
      pcat(pres, "", y); pcat(pres, "<=X<=", x);
    }
  }
  if (!disc) pcurve(pd, q);
}
static NOINL void ptabs() {
  static const char * const REG[4] = {"a\xe2\x89\xa4X\xe2\x89\xa4" "b", "X\xe2\x89\xa4x", "X\xe2\x89\xa5x", "X=k"}, * const INV[3] = {"left", "right", "middle"};
  static const char * tabs[5];
  if (ptest >= 0) { tabs[0] = ptest & 1 ? (talt == 0 ? "Ha: \xe2\x89\xa0" : talt == 1 ? "Ha: <" : "Ha: >") : ""; tabs[1] = ptest >= 2 ? "from L1" : ""; tabs[2] = tabs[3] = ""; tabs[4] = "back"; }
  else { tabs[0] = "dist"; tabs[1] = pinv ? INV[ptail] : REG[ptail]; tabs[2] = p_discrete(pd) ? "" : pinv ? "area" : "inverse"; tabs[3] = "tests"; tabs[4] = "back"; }
  focus_bar_tabs(tabs, -1);
}
static NOINL bool pcommit() { // the value typed into the field selected
  giac::ctrl_c = giac::interrupted = false;
  if (!edn) { editing = 0; return true; }
  edb[edn] = 0;
  const float v = fv(eval(gen(edb, contextptr), 1, contextptr));
  if (!pvalid(v, PF[psel].kind)) { emsg = PF[psel].kind == 2 || PF[psel].kind == 4 ? "A whole number" : PF[psel].kind == 5 || PF[psel].kind == 3 ? "Between 0 and 1" : PF[psel].kind == 1 ? "More than 0" : "Not a number"; return false; }
  *PF[psel].v = v;
  editing = 0; edn = 0;
  return true;
}
static NOINL void ploop() { // the distribution screen or a test's: keys until back
  for (;;) {
    pfields();
    if (psel >= pn) psel = pn - 1;
    if (!editing) pcompute();
    focus_status_label(ptest >= 0 ? (ptest < 2 ? "TEST  1-proportion z" : "TEST  1-sample t") : "PROBABILITY");
    ptabs();
    paint_prob();
    const int key = key_();
    emsg = 0;
    if (const int ch = typed(key)) {
      if (!editing) { editing = 1; edn = 0; }
      if (edn < (int)sizeof(edb) - 2) { edb[edn++] = (char)ch; edb[edn] = 0; }
      continue;
    }
    if (key == KEY_CTRL_DEL && editing) { if (edn) edb[--edn] = 0; else editing = 0; continue; }
    if (key == KEY_CTRL_EXIT || key == KEY_CTRL_AC || key == KEY_CTRL_F5) { if (editing) { editing = 0; continue; } break; }
    if (key == KEY_CTRL_EXE || key == KEY_CTRL_UP || key == KEY_CTRL_DOWN || key == KEY_CTRL_LEFT || key == KEY_CTRL_RIGHT) {
      if (editing && !pcommit()) continue;
      if (key == KEY_CTRL_LEFT || key == KEY_CTRL_UP) psel = (psel + pn - 1) % pn;
      else psel = (psel + 1) % pn;
      continue;
    }
    if (ptest >= 0) {
      if (key == KEY_CTRL_F1 && (ptest & 1)) talt = (talt + 1) % 3;
      if (key == KEY_CTRL_F2 && ptest >= 2) { // the mean, SD and size of L1
        load();
        onevar o;
        if (!stats1(D_[0], o) || o.n < 2) { emsg = "Type 2 or more values in L1 (Statistics)"; continue; }
        float * v = tv[ptest] + (ptest == 3);
        v[0] = fv(o.mean); v[1] = fv(o.sx); v[2] = o.n;
      }
      continue;
    }
    if (key == KEY_CTRL_F1) {
      static const char * const hint[PD_N] = {"\xce\xbc, \xcf\x83", "df", "df", "n, p", "\xce\xbb"};
      const int c = focus_card_list(4, "dist", 0, PNAME, hint, PD_N, pd);
      if (c >= 0 && c != pd) { pd = c; ptail = 0; psel = 0; if (p_discrete(pd)) pinv = 0; pq[PD_BIN][0] = 10; pa = pd >= PD_BIN ? 2 : -1; pb = pd >= PD_BIN ? 5 : 1; }
    }
    if (key == KEY_CTRL_F2) ptail = (ptail + 1) % (pinv ? (pd == PD_CHI ? 2 : 3) : p_discrete(pd) ? 4 : 3);
    if (key == KEY_CTRL_F3 && !p_discrete(pd)) { pinv = !pinv; ptail = 0; }
    if (key == KEY_CTRL_F4) {
      static const char * const TN[4] = {"1-proportion z interval", "1-proportion z test", "1-sample t interval", "1-sample t test"};
      static const char * const TH[4] = {"x, n, C", "p0, x, n", "mean, s, n, C", ("\xce\xbc" "0, mean, s, n")};
      focus_bar_tabs(0, -1); paint_prob(); // (the card over a repainted view)
      const int c = focus_card_list(4, "tests", 0, TN, TH, 4, 0);
      if (c >= 0) { ptest = c; talt = 0; psel = 0; ploop(); ptest = -1; psel = 0; }
    }
  }
}
void focus_prob() {
  giac::ctrl_c = giac::interrupted = false;
  if (!pinit) { // the TI's defaults: N(0, 1), t(10), chi-square(5), B(10, 0.5), Poisson(3); 95%
    pinit = 1;
    pq[PD_NORM][1] = 1; pq[PD_T][0] = 10; pq[PD_CHI][0] = 5; pq[PD_BIN][0] = 10; pq[PD_BIN][1] = 0.5f; pq[PD_POIS][0] = 3;
    pa = -1; pb = 1; par = 0.95f;
    static const float T0[4][4] = {{58, 100, 0.95f, 0}, {0.5f, 58, 100, 0}, {5, 1.2f, 25, 0.95f}, {5, 5.4f, 1.2f, 25}};
    memcpy(tv, T0, sizeof tv);
  }
  editing = edn = 0; emsg = 0; psel = 0; ptest = -1;
  const int view0 = focus_view;
  focus_view = 1;
  ploop();
  focus_view = view0;
  focus_status_label(0);
  focus_invalidate();
}
#endif
