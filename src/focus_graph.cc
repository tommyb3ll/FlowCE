// focus_graph.cc - DeltaCE's graph screen: y = f(x) (one to four functions), equations in x and y,
// parametric and polar curves (plotparam, plotpolar), slope fields and their solutions (plotfield,
// plotode); KhiCAS's own plots and graph view are not built. Made to be easier than the
// calculator's own grapher:
// - a bar that says what the keys do: pan/trace, zoom, points, table, more;
// - pan: the arrows move the view while held, + and - zoom while held (on the middle of the
//   screen); trace: the arrows move a cursor along a curve (up/down: the next curve);
// - the grid and its numbers follow the zoom (steps of 1, 2, 5 x 10^k); zoomed in, dots on the
//   curves at the grid's x, and their coordinates when that setting is on;
// - the key points (zeros, extrema, inflection points from the analysis, analyze.cc; where the
//   curves cross), exact; the cursor jumps to them (F3, 2nd left/right);
// - a table of values, the tangent line, the area under the curve, f' too, the analysis.
// Fast: each function is compiled to a small program over floats (giac's evalf took 0.4-0.8 s a
// redraw on the calculator). A frame is about 0.5 s; while a key is held, 0.1 to 0.2 s (every 3rd
// column, lines without anti-aliasing; a view moved by whole pixels reuses the rows it had).
#ifdef FLOWCE_STEPS
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <giac/giacPCH.h> // first: its "#define std ustl"
#include "k_csdk.h"
#include "focus.h"
#include "ui_gfx.h"
#include "ui_font.h"
#include "ui_fontdata.h"
#include "ui_math.h"
#include "analyze.h"
using namespace giac;
extern giac::context * contextptr;
extern int dconsole_mode; // console.cc: 0, giac's notes go nowhere
#define NOINL __attribute__((noinline))

int focus_prompt(const char * title, const char * label, std::string & s, bool numeric); // focus_menu.cc

// ---------------------------------------------------------------- the functions, compiled
enum { O_X, O_C, O_ADD, O_MUL, O_NEG, O_INV, O_IPOW, O_POW, O_ROOT, O_F1, O_Y };
enum { F_SIN, F_COS, F_TAN, F_EXP, F_LN, F_SQRT, F_ABS, F_ASIN, F_ACOS, F_ATAN, F_SINH, F_COSH, F_TANH, F_LOG10, F_FLOOR, F_CEIL };
enum { NMAX = 6, NGRP = 4, PMAX = 80, SMAX = 20 };
struct prog { unsigned char op[PMAX], fn[PMAX]; float c[PMAX]; unsigned char n, depth, ok; };
static prog * P_;          // the functions' programs
static gen * F_;           // the functions (giac), F_[i]
static int NF;             // how many
static gen * X_;           // the variable
static gen * Yv_;          // y, in equations (x^2+y^2=25); 0: none
// What is drawn: the curves y = f(x) (F_, P_; an equation solved for y gives one per solution:
// y = sqrt(25-x^2), y = -sqrt(25-x^2)) and the vertical lines (x = 3: VX), each of an equation typed
// (GRP: its color); the equations that are not solved for y as such (F(x, y) = 0: IM_, IP_, IG_)
static unsigned char GRP[NMAX];
static float VX[NMAX];     // a vertical line's x, NAN for a curve
static gen * IM_;          // F(x, y) of an equation drawn as such (x^3+y^3=6xy)
static prog * IP_;
static unsigned char IG_[NGRP];
static int NI, NGR;        // how many such equations; how many typed
static gen * EQ_;          // each typed one as F(x, y) = 0 (y-f(x) for a function): where two cross
static float * DAT_;       // (DeltaCE) data points drawn with the curves (focus_stats.cc: the points
static int DATN_;          // a curve was fitted to): their x, then their y (by x); how many
static int tpi;            // the point traced (tf == NF: the points)
static const unsigned char COLS[NGRP] = {UC_ACC, UC_RED, UC_PURPLE, UC_ORANGE};
static inline unsigned char col(int i) { return COLS[GRP[i] & 3]; }

static NOINL bool emit(prog & p, int op, float c, int fn, int d) {
  if (p.n >= PMAX) return false;
  p.op[p.n] = (unsigned char)op; p.c[p.n] = c; p.fn[p.n] = (unsigned char)fn; ++p.n;
  if (d > p.depth) p.depth = (unsigned char)d;
  return d < SMAX;
}
// g as a float (NAN: not a real number)
static NOINL float fl(const gen & g) {
  const gen z = evalf(g, 1, contextptr);
  return z.type == _INT_ ? (float)z.val : z.type == _DOUBLE_ ? (float)z._DOUBLE_val : NAN;
}
static bool fin(float v);
static NOINL bool cnum(const gen & g, float & v) { // a number (pi, 2/3, sqrt(2) too)
  if (g.type == _IDNT && (g == *X_ || (Yv_ && g == *Yv_))) return false;
  v = fl(g);
  return fin(v);
}
// g compiled into p, its value at stack depth d (+1)
static bool comp(prog & p, const gen & g, int d) {
  float v;
  if (g.type == _IDNT && g == *X_) return emit(p, O_X, 0, 0, d + 1);
  if (g.type == _IDNT && Yv_ && g == *Yv_) return emit(p, O_Y, 0, 0, d + 1);
  if (g.type != _SYMB) return cnum(g, v) && emit(p, O_C, v, 0, d + 1);
  if (!depend(g, *X_->_IDNTptr) && !(Yv_ && depend(g, *Yv_->_IDNTptr))) return cnum(g, v) && emit(p, O_C, v, 0, d + 1);
  const unary_function_ptr & s = g._SYMBptr->sommet;
  const gen & a = g._SYMBptr->feuille;
  if (s == at_plus || s == at_prod) {
    if (a.type != _VECT || a._VECTptr->empty()) return false;
    const vecteur & v2 = *a._VECTptr;
    if (!comp(p, v2[0], d)) return false;
    for (unsigned i = 1; i < v2.size(); ++i)
      if (!comp(p, v2[i], d + 1) || !emit(p, s == at_plus ? O_ADD : O_MUL, 0, 0, d + 1)) return false;
    return true;
  }
  if (s == at_neg) return comp(p, a, d) && emit(p, O_NEG, 0, 0, d + 1);
  if (s == at_inv) return comp(p, a, d) && emit(p, O_INV, 0, 0, d + 1);
  if (s == at_pow && a.type == _VECT && a._VECTptr->size() == 2) {
    const gen & b = a[0], & e = a[1];
    if (e.type == _INT_ && e.val >= -64 && e.val <= 64) return comp(p, b, d) && emit(p, O_IPOW, 0, e.val + 64, d + 1); // (the exponent + 64)
    if (e.type == _FRAC && e._FRACptr->num.type == _INT_ && e._FRACptr->den.type == _INT_) {
      const int q = e._FRACptr->den.val, n = e._FRACptr->num.val;
      if (q == 2 && n == 1) return comp(p, b, d) && emit(p, O_F1, 0, F_SQRT, d + 1);
      if (q & 1) // an odd root, real for negative bases too (the textbooks' cube root of -8: -2)
        return comp(p, b, d) && emit(p, O_ROOT, 1.0f / q, 0, d + 1) && (n == 1 || emit(p, O_IPOW, 0, n + 64, d + 1)); // (1/q: no division at each x)
    }
    return comp(p, b, d) && comp(p, e, d + 1) && emit(p, O_POW, 0, 0, d + 2);
  }
  if (s == at_surd && a.type == _VECT && a._VECTptr->size() == 2 && a[1].type == _INT_)
    return comp(p, a[0], d) && emit(p, O_ROOT, 1.0f / a[1].val, 0, d + 1);
  static const unary_function_ptr * const * const fs[] = {&at_sin, &at_cos, &at_tan, &at_exp, &at_ln, &at_sqrt, &at_abs,
    &at_asin, &at_acos, &at_atan, &at_sinh, &at_cosh, &at_tanh, &at_log10, &at_floor, &at_ceil};
  for (int k = 0; k < (int)(sizeof(fs) / sizeof(fs[0])); ++k)
    if (s == **fs[k]) return comp(p, a, d) && emit(p, O_F1, 0, k, d + 1);
  return false;
}
// sin and cos for the curves' pixels (quick_): a table, 3e-5; sinf takes 20 float operations, each a
// call to the OS's routines (1000 cycles), this 3. A period is 2^22, the phase the low bits of u 2^22/2pi.
static const unsigned short QS[129] = {
  0, 804, 1608, 2412, 3216, 4019, 4821, 5623, 6424, 7223, 8022, 8820, 9616, 10411, 11204, 11996,
  12785, 13573, 14359, 15142, 15924, 16703, 17479, 18253, 19024, 19792, 20557, 21319, 22078, 22834, 23586, 24334,
  25079, 25820, 26557, 27291, 28020, 28745, 29465, 30181, 30893, 31600, 32302, 32999, 33692, 34379, 35061, 35738,
  36409, 37075, 37736, 38390, 39039, 39682, 40319, 40950, 41575, 42194, 42806, 43411, 44011, 44603, 45189, 45768,
  46340, 46905, 47464, 48014, 48558, 49095, 49624, 50145, 50659, 51166, 51664, 52155, 52638, 53113, 53580, 54039,
  54490, 54933, 55367, 55794, 56211, 56620, 57021, 57413, 57797, 58171, 58537, 58895, 59243, 59582, 59913, 60234,
  60546, 60850, 61144, 61429, 61704, 61970, 62227, 62475, 62713, 62942, 63161, 63371, 63571, 63762, 63943, 64114,
  64276, 64428, 64570, 64703, 64826, 64939, 65042, 65136, 65219, 65293, 65357, 65412, 65456, 65491, 65515, 65530,
  65535};
static bool quick_;
static float qsin(float u, unsigned long shift) {
  const float p = u * 667544.214f;
  if (!(p > -2.1e9f && p < 2.1e9f)) return shift ? cosf(u) : sinf(u);
  const unsigned long t = (unsigned long)(long)p + shift;
  const int q = (int)(t >> 20) & 3;
  int r = (int)(t & 0xFFFFF);
  if (q & 1) r = 0x100000 - r;
  const int i = r >> 13, f = r & 0x1FFF; // (128 steps a quarter)
  const int v = i == 128 ? QS[128] : QS[i] + (((int)QS[i + 1] - (int)QS[i]) * f >> 13);
  const float s = v * (1.0f / 65535);
  return q & 2 ? -s : s;
}
static float fun1(int k, float u) {
  if (quick_ && k <= F_TAN) {
    if (k == F_SIN) return qsin(u, 0);
    if (k == F_COS) return qsin(u, 0x100000);
    return qsin(u, 0) / qsin(u, 0x100000);
  }
  switch (k) {
  case F_SIN: return sinf(u); case F_COS: return cosf(u); case F_TAN: return tanf(u);
  case F_EXP: return expf(u); case F_LN: return u > 0 ? logf(u) : NAN;
  case F_SQRT: return u >= 0 ? sqrtf(u) : NAN; case F_ABS: return fabsf(u);
  case F_ASIN: return u >= -1 && u <= 1 ? asinf(u) : NAN; case F_ACOS: return u >= -1 && u <= 1 ? acosf(u) : NAN;
  case F_ATAN: return atanf(u); case F_SINH: return sinhf(u); case F_COSH: return coshf(u); case F_TANH: return tanhf(u);
  case F_LOG10: return u > 0 ? log10f(u) : NAN; case F_FLOOR: return floorf(u); case F_CEIL: return ceilf(u);
  }
  return NAN;
}
// v a number (not NaN, not infinite), by its bits: the CE's float library compares NaN equal to itself
static NOINL bool fin(float v) {
  union { float f; unsigned long u; } q;
  q.f = v;
  return (q.u & 0x7f800000UL) != 0x7f800000UL;
}
// g at x: its program p, else giac (a function the programs cannot do: slow)
static NOINL float run_prog(const prog & p, const gen & g, float x, float yv = 0) {
  if (!p.ok) return fl(subst(Yv_ ? subst(g, *Yv_, gen((double)yv), false, contextptr) : g, *X_, gen((double)x), false, contextptr));
  // (pointers, no index: an index is a multiplication, a helper call on the eZ80)
  float st[SMAX], * sp = st - 1;
  const unsigned char * op = p.op, * fn = p.fn;
  const float * cc = p.c;
  for (int j = p.n; j > 0; --j, ++op, ++fn, ++cc) {
    switch (*op) {
    case O_X: *++sp = x; break;
    case O_Y: *++sp = yv; break;
    case O_C: *++sp = *cc; break;
    case O_ADD: sp[-1] += *sp; --sp; break;
    case O_MUL: sp[-1] *= *sp; --sp; break;
    case O_NEG: *sp = -*sp; break;
    case O_INV: *sp = 1 / *sp; if (!fin(*sp)) return NAN; break;
    case O_IPOW: { // (x^2: one multiplication)
      int e = *fn - 64;
      float b = e < 0 ? 1 / *sp : *sp, r = 1;
      bool one = true;
      for (e = e < 0 ? -e : e;;) {
        if (e & 1) { r = one ? b : r * b; one = false; }
        if (!(e >>= 1)) break;
        b *= b;
      }
      *sp = r;
    } break;
    case O_POW: sp[-1] = powf(sp[-1], *sp); --sp; if (!fin(*sp)) return NAN; break;
    case O_ROOT: { const float u = *sp; *sp = u < 0 ? -powf(-u, *cc) : powf(u, *cc); } break;
    case O_F1: *sp = fun1(*fn, *sp); if (!fin(*sp)) return NAN; break; // (the OS's float routines do not carry a NaN on: 1/2*sqrt(-1) was a number)
    }
  }
  return sp == st ? *st : NAN;
}
static NOINL float val(int i, float x) { return fin(VX[i]) ? NAN : run_prog(P_[i], F_[i], x); } // f_i at x (a vertical line: none)

// ---------------------------------------------------------------- the view
enum { GT = 16, GB = 218, GH = GB - GT, BAR_H = UI_H - GB };
static float vx0, vx1, vy0, vy1;          // the window
static float w0;                          // its width when the graph opened: zoomed in from it, dots
// (the frame: frame below)
static int mode;                          // 0 pan, 1 trace
static int tf;                            // trace: the curve
static float tx;                          // trace: x
static bool coords, grid = true;          // dots' coordinates shown; the grid
static bool klabels;                      // the key points' coordinates on the graph
static bool tang;                         // the tangent line at the trace point
static float area_a, area_b;              // the area under the curve shaded (a < b), else a = b
static float fan_a, fan_b;                // a polar curve's area shaded: θ from a to b (a < b)
static char note[64];                     // the readout's own line (a key point, the area, the tangent)
static int note_x;                        // ... for the trace x it was made for (pixel column), -1: any
static float note_t;                      // ... a curve's: for its t (note_x >= 0)

static NOINL float col_x(float c) { return vx0 + (vx1 - vx0) * c / (UI_W - 1); }
static NOINL float row_of(float y) { return GT + (vy1 - y) * (GH - 1) / (vy1 - vy0); }
static NOINL int clampi(float v) { return v < -1000 ? -1000 : v > 2000 ? 2000 : v >= 0 ? (int)(v + 0.5f) : -(int)(0.5f - v); } // (rounded; no floorf)
static NOINL int px_of(float x) { return clampi((x - vx0) * (UI_W - 1) / (vx1 - vx0)); }
static NOINL int py_of(float y) { return clampi(row_of(y)); }

// the step of the grid: 1, 2 or 5 x 10^k, at least minpx pixels apart (no log10f, powf: slow)
static NOINL float step_of(float range, int pixels, int minpx) {
  const float raw = range * minpx / pixels;
  float m = 1;
  for (int k = 0; m * 10 <= raw && k < 40; ++k) m *= 10;
  for (int k = 0; m > raw && k < 40; ++k) m /= 10;
  const float r = raw / m;
  return (r <= 1 ? 1 : r <= 2 ? 2 : r <= 5 ? 5 : 10) * m;
}
// v with dec decimals (at most 5); from a million: 1.23e7
static void fmtd(char * b, float v, int dec);
static NOINL void trim(char * b) { // 2.500: 2.5, 3.000: 3
  char * d = strchr(b, '.');
  if (!d) return;
  char * e = b + strlen(b) - 1;
  while (e > d && *e == '0') *e-- = 0;
  if (e == d) *e = 0;
}
static NOINL void expo(char * b, int e) { // e-5, e7
  b += strlen(b); *b++ = 'e';
  if (e < 0) { *b++ = '-'; e = -e; }
  char t[4]; int n = 0;
  do { t[n++] = '0' + e % 10; e /= 10; } while (e && n < 3);
  while (n) *b++ = t[--n];
  *b = 0;
}
static void fmtd(char * b, float v, int dec) {
  float a = fabsf(v), sc = 1;
  if (a >= 1e6f) {
    int e = 0;
    while (a >= 10 && e < 40) { a /= 10; ++e; }
    fmtd(b, v < 0 ? -a : a, 2); trim(b); expo(b, e);
    return;
  }
  for (int k = 0; k < dec; ++k) sc *= 10;
  while (dec > 0 && a * sc >= 2e9f) { --dec; sc /= 10; } // (a long)
  long n = (long)(a * sc + 0.5f), ip = n;
  if (v < 0 && n) *b++ = '-';
  for (int k = 0; k < dec; ++k) ip /= 10;
  char t[12]; int m = 0;
  do { t[m++] = '0' + ip % 10; ip /= 10; } while (ip && m < 11);
  while (m) *b++ = t[--m];
  if (dec) {
    long fr = n;
    char d[8];
    for (int k = dec - 1; k >= 0; --k) { d[k] = '0' + fr % 10; fr /= 10; }
    *b++ = '.';
    for (int k = 0; k < dec; ++k) *b++ = d[k];
  }
  *b = 0;
}
// v written with the decimals the step needs (step 0.05: 2), at most 5 (no log10f: slow)
static NOINL void fmt(char * b, float v, float step) {
  int dec = 0;
  for (float t = step; t < 0.999f && dec < 5; t *= 10) ++dec;
  fmtd(b, fabsf(v) < step * 1e-3f ? 0 : v, dec);
}
// v with 6 significant digits, no trailing zeros (the table, the tangent): 7940, 0.333333, 1.2e-5
static NOINL void fmtg(char * b, float v, int sig = 6) {
  float a = fabsf(v);
  if (a != 0 && a < 1e-4f) {
    int e = 0;
    while (a < 1 && e < 40) { a *= 10; ++e; }
    fmtd(b, v < 0 ? -a : a, 3); trim(b); expo(b, -e);
    return;
  }
  int d = sig;
  for (float t = 1; t <= a && d > 0; t *= 10) --d;
  fmtd(b, v, d > 5 ? 5 : d); trim(b);
}

// The frame: what is drawn, worked out once (floats, slow on the eZ80: the OS's routines), then
// drawn band by band in integers
enum { NONE = -0x7fff00, TOPC = (GT - 64 * GH) * 16, BOTC = (GB + 64 * GH) * 16, NG = 16, ND = 40, NKP = 40 };
struct gl { float v; int p; unsigned char w; char t[14]; }; // a grid line: its value, where, its number
struct dt { int x, y; unsigned char f, w; char t[30]; };  // a dot on a curve: where, its coordinates
struct frame {
  int y[NMAX * UI_W];            // the curves' rows at each column, 1/16 px (NONE: none)
  unsigned char j[NMAX * UI_W];  // joined to the column sampled before
  gl gx[NG], gy[NG];
  dt d[ND];
  int kx[NKP], ky[NKP];          // the key points
  short lx[NKP], ly[NKP];        // their coordinates' places (lx < 0: not shown, over another)
  char ln[64];                   // the readout's line
  int ngx, ngy, nd;
  short ec[16]; unsigned char ei[16]; int ex[16], ey[16]; int ne; // where a curve's domain ends between two columns (sqrt(25-x^2) at 5): to there from column ec
};
static frame * FR;
// y's row in 1/16 px, within 64 screens above and below (TOPC, BOTC); NONE: not a number
static int row16(float v) {
  if (!fin(v)) return NONE;
  const float h = vy1 - vy0;
  if (v > vy1 + 64 * h) return TOPC;
  if (v < vy0 - 64 * h) return BOTC;
  return (int)(GT * 16 + (vy1 - v) * (16 * (GH - 1)) / h);
}
// curve i continuous from column c0 (row a) to c1 (row z): a steep step is checked halfway (not
// joined across tan(x)'s asymptote, 1/x at 0: halfway it is not between them)
static NOINL bool joined(int i, int c0, int c1, int a, int z) {
  if ((a > z ? a - z : z - a) <= GH * 16 / 3) return true;
  const int m = row16(val(i, col_x((c0 + c1) * 0.5f)));
  if (m == NONE) return false;
  if ((a == TOPC && z == BOTC) || (a == BOTC && z == TOPC)) return m != TOPC && m != BOTC;
  return m >= (a < z ? a : z) - 32 && m <= (a < z ? z : a) + 32;
}
// the curves' rows (every 3 columns while a key is held); each float operation is a call to the
// OS (1000 cycles): the fewest, the row as A - v k
static float lx0, ly0, lw, lh; // the view the rows are for
static int lev;                // ... sampled every lev columns (0: none, the functions changed)
static int accy;               // rows moved up or down since all were evaluated (TOPC, BOTC: kept)
static int shx, shy;           // the last sample's shift (pixels; -1000: not a shift)
static int lnk = -1;           // the key points the frame's pixels are for (-2: new ones)
static NOINL void sample(int every) {
  const float w = vx1 - vx0, h = vy1 - vy0, pw = w / (UI_W - 1), dx = pw * every, k = 16 * (GH - 1) / h, A = GT * 16 + vy1 * k, top = vy1 + 64 * h, bot = vy0 - 64 * h;
  // The view moved by whole pixels (pan): the rows reused, shifted; only the columns that came
  // into view evaluated (left, right), or none (up, down: those beyond a screen only)
  int nx = 0, ny = 0;
  bool re = lev && (lev == 1 || lev == every) && fabsf(w - lw) <= w * 1e-5f && fabsf(h - lh) <= h * 1e-5f;
  if (re) {
    const float fx = (vx0 - lx0) / pw, fy = (vy0 - ly0) * (GH - 1) / h;
    nx = clampi(fx); ny = clampi(fy);
    re = fabsf(fx - nx) < 0.02f && fabsf(fy - ny) < 0.02f && nx > -UI_W / 2 && nx < UI_W / 2 && ny > -GH && ny < GH;
    accy = re ? accy + ny : 0;
    if (accy > 32 * GH || accy < -32 * GH) re = false, accy = 0; // (a point beyond 64 screens could be nearer now)
  }
  else accy = 0;
  const bool last = !re || (nx && lev != 1); // the last column: sampled always, not where its old one went
  quick_ = h > 0.05f; // (zoomed in further: 2e-5 shows)
  lx0 = vx0; ly0 = vy0; lw = w; lh = h; lev = every;
  const int n0 = re && nx < 0 ? -nx : 0, n1 = re && nx > 0 ? UI_W - nx : UI_W; // reused: n0 to n1
  shx = re ? nx : -1000; shy = ny;
  for (int i = 0; i < NF; ++i) {
    int * y = FR->y + i * UI_W;
    unsigned char * j = FR->j + i * UI_W;
    if (re && nx > 0) memmove(y, y + nx, (UI_W - nx) * sizeof(int));
    if (re && nx < 0) memmove(y - nx, y, (UI_W + nx) * sizeof(int));
    int pc = -1;
    float x = vx0;
    for (int c = 0, n = 0; c < UI_W; ++c, --n) { // (n: columns to the next sample; % is a helper call)
      j[c] = 0;
      if (n > 0 && c != UI_W - 1) { y[c] = NONE; continue; }
      n = every;
      int r = y[c];
      bool ev = !re || c < n0 || c >= n1 || (c == UI_W - 1 && last);
      if (!ev && ny && r != NONE && r != TOPC && r != BOTC) r += 16 * ny;
      if (ev) {
        const float v = val(i, c == UI_W - 1 ? vx1 : x);
        r = !fin(v) ? NONE : v > top ? TOPC : v < bot ? BOTC : (int)(A - v * k);
      }
      x += dx;
      y[c] = r;
      if (r == NONE) { pc = -1; continue; }
      if (pc >= 0) j[c] = joined(i, pc, c, y[pc], r);
      pc = c;
    }
  }
  quick_ = false;
  FR->ne = 0; // the domain's ends between two columns (full frames): found by halving, the curve drawn to them
  if (every == 1)
    for (int i = 0; i < NF; ++i) {
      const int * y = FR->y + i * UI_W;
      for (int c = 0; c + 1 < UI_W && FR->ne < 16; ++c) {
        const bool a = y[c] != NONE;
        if (a == (y[c + 1] != NONE)) continue;
        float xd = vx0 + (a ? c : c + 1) * pw, xu = vx0 + (a ? c + 1 : c) * pw;
        for (int t = 0; t < 12; ++t) { const float m = (xd + xu) / 2; if (fin(val(i, m))) xd = m; else xu = m; }
        const int r = row16(val(i, xd));
        if (r == NONE) continue;
        FR->ec[FR->ne] = (short)(a ? c : c + 1); FR->ei[FR->ne] = (unsigned char)i;
        FR->ex[FR->ne] = (int)((xd - vx0) / pw * 16) + 8; FR->ey[FR->ne++] = r;
      }
    }
}

// ---------------------------------------------------------------- parametric and polar curves
// x = x(t), y = y(t) (plotparam: the programs P_[2c], P_[2c+1]) and r = r(θ) (plotpolar: P_[c],
// x = r cos θ, y = r sin θ), t from PT0 to PT1 (X_ is t): sampled in t for the view, more points
// where the curve bends or leaves its domain; drawn from them by the view's mapping each frame,
// sampled again when the view is twice as wide or narrow
static int KIND_;               // 0: y = f(x), 1: parametric, 2: polar, 3: a slope field (its solutions: the curves)
static int NC_;                 // the parametric or polar curves
static float PT0[NGRP], PT1[NGRP];
enum { PSMAX = 400, SOLMAX = 240 };
static float * PS_;             // the curves' points (x, y; NAN: a break), PSS_ / 2 each
static int PSS_;
static int PSN[NGRP];
static float psw;               // the view's width they were sampled for (0: none)
static int * PM_;               // their places this frame (1/16 px)
static float sol_y(int c, float x);
static NOINL bool pt_at(int c, float t, float & x, float & y) { // curve c at t (false: none; a solution: at x = t)
  if (KIND_ == 3) { x = t; y = sol_y(c, t); }
  else if (KIND_ == 1) { x = run_prog(P_[2 * c], F_[2 * c], t); y = fin(x) ? run_prog(P_[2 * c + 1], F_[2 * c + 1], t) : NAN; }
  else { const float r = run_prog(P_[c], F_[c], t); x = r * cosf(t); y = r * sinf(t); }
  if (fin(x) && fin(y)) return true;
  x = y = NAN;
  return false;
}
static float * ps_; static int psn_, pscap_; static float pkx, pky; // (the sampling: its points, the last one this step may add, pixels a unit)
static NOINL void ps_put(float x, float y) { if (psn_ < PSMAX) { ps_[2 * psn_] = x; ps_[2 * psn_ + 1] = y; ++psn_; } }
static NOINL float pgap(float ax, float ay, float bx, float by) { return fabsf(bx - ax) * pkx + fabsf(by - ay) * pky; }
// the points after (ta: a) up to (tb: b), halved while two are more than 5 px apart (4 times at
// most, within this step's share of the points; still apart, and the middle far off the chord:
// a jump, tan's asymptote, not joined)
static void ps_seg(int c, float ta, float ax, float ay, float tb, float bx, float by, int d) {
  const bool fa = fin(ax), fb = fin(bx);
  const float g = fa && fb ? pgap(ax, ay, bx, by) : 0;
  if (fa != fb || g > 5) {
    const float tm = (ta + tb) * 0.5f;
    float mx, my;
    pt_at(c, tm, mx, my);
    if (d < 4 && psn_ < pscap_) {
      ps_seg(c, ta, ax, ay, tm, mx, my, d + 1);
      ps_seg(c, tm, mx, my, tb, bx, by, d + 1);
      return;
    }
    // (or the two beyond opposite sides of the view: tan(t)'s x from +1e6 to -1e6 at -pi/2)
    const bool across = (ax < vx0 && bx > vx1) || (ax > vx1 && bx < vx0) || (ay < vy0 && by > vy1) || (ay > vy1 && by < vy0);
    if (g > 40 && (across || !(fin(mx) && pgap(ax, ay, mx, my) + pgap(mx, my, bx, by) < 1.5f * g))) ps_put(NAN, NAN);
  }
  ps_put(bx, by);
}
static void solve_ode(int c);
static float sxl, sxh; // (a slope field: the x its solutions were worked out over)
static NOINL void psample() {
  pkx = (UI_W - 1) / (vx1 - vx0); pky = (GH - 1) / (vy1 - vy0);
  psw = vx1 - vx0;
  if (KIND_ == 3) { // (the solutions over the view and half a view each side)
    sxl = vx0 - psw / 2; sxh = vx1 + psw / 2;
    for (int c = 0; c < NC_; ++c) solve_ode(c);
    return;
  }
  for (int c = 0; c < NC_; ++c) {
    ps_ = PS_ + PSS_ * c; psn_ = 0;
    const float dt = (PT1[c] - PT0[c]) / 96;
    float t = PT0[c], ax, ay;
    pt_at(c, t, ax, ay);
    ps_put(ax, ay);
    for (int k = 1; k <= 96; ++k) {
      const float tb = k == 96 ? PT1[c] : t + dt;
      float bx, by;
      pt_at(c, tb, bx, by);
      pscap_ = psn_ + (PSMAX - 2 - psn_) / (97 - k); // (the rest shared by the steps left: the curve reaches its end)
      ps_seg(c, t, ax, ay, tb, bx, by, 0);
      t = tb; ax = bx; ay = by;
    }
    PSN[c] = psn_;
  }
}
// ---------------------------------------------------------------- slope fields
// y' = f(x, y) (plotfield, plotode: P_[0], x X_, y Yv_): short segments of slope f over the view,
// the solutions through points (SX0, SY0) by Runge-Kutta, from the point both ways (the curves)
static float SX0[NGRP], SY0[NGRP];
static float fy0_, fy1_;        // the y range given
enum { NFS = 320 };
static int * FS_;               // the segments this frame (1/16 px: x0, y0, x1, y1)
static int nfs_;
static NOINL float fxy(float x, float y) { return run_prog(P_[0], F_[0], x, y); }
// a Runge-Kutta step (NAN: a slope that is not a number; the OS's float routines do not carry one)
static NOINL float rk4(float x, float y, float h) {
  const float k1 = fxy(x, y);
  if (!fin(k1)) return NAN;
  const float k2 = fxy(x + h / 2, y + h / 2 * k1);
  if (!fin(k2)) return NAN;
  const float k3 = fxy(x + h / 2, y + h / 2 * k2);
  if (!fin(k3)) return NAN;
  const float k4 = fxy(x + h, y + h * k3);
  return fin(k4) ? y + h / 6 * (k1 + 2 * k2 + 2 * k3 + k4) : NAN;
}
// a step of a solution from (x, y) by h (shorter where it is steep: 6 px up or down at most and
// within 1/4 px of two half steps, h/32 at least); false where it ends: a slope not a number, beyond 64 views, vertical on the screen
// (400 px up a pixel across), a steep slope that turns over (sl: the last step's, in pixels):
// x^2+y^2=2 from y' = -x/y ends at x = sqrt(2), it does not go on
static NOINL bool sol_step(float & x, float & y, float h, float ym, float lim, float & sl) {
  const float m = fxy(x, y) * pky / pkx, hv = vy1 - vy0;
  if (!fin(m) || fabsf(m) > 400 || (fabsf(sl) > 1.5f && fabsf(m) > 1.5f && (m < 0) != (sl < 0))) return false;
  sl = m;
  for (int k = 0;; ++k, h /= 2) {
    const float u = rk4(x, y, h);
    if (!fin(u) || fabsf(u - ym) > lim) return false;
    // (off the screen by a view: no shorter steps, the points last to the ends of the x range)
    const bool off = (y > vy1 + hv && u > vy1 + hv) || (y < vy0 - hv && u < vy0 - hv);
    float err = 0; // (steep: the step against two half steps; they differ near a vertical tangent,
    if (fabsf(m) > 1 && !off) { // where a whole step jumped over its end: y' = -x/y went on past x = sqrt(2))
      const float u2 = rk4(x + h / 2, rk4(x, y, h / 2), h / 2);
      err = fin(u2) ? fabsf(u - u2) * pky : 1e9f;
    }
    if ((fabsf(u - y) * pky <= 6 && err <= 0.25f) || k == 5 || off) {
      if (err > 2) return false;
      const float m2 = fxy(x + h, u) * pky / pkx; // (the end turning over too: not kept)
      if (!fin(m2) || (fabsf(m) > 1.5f && fabsf(m2) > 1.5f && (m2 < 0) != (m < 0))) return false;
      x += h; y = u;
      return true;
    }
  }
}
// solution c from sxl to sxh (SOLMAX points at most, in x order) through (SX0, SY0): back from the
// point (then reversed), then on
static void solve_ode(int c) {
  float * p = PS_ + PSS_ * c;
  const float h = (sxh - sxl) / (SOLMAX - 4), lim = 64 * (vy1 - vy0), ym = (vy0 + vy1) / 2;
  float x = SX0[c], y = SY0[c];
  int nb = x > sxl ? (int)((x - sxl) / h) + 16 : 0, n = 0;
  if (nb > SOLMAX - 2) nb = SOLMAX - 2;
  float sl = 0;
  for (; n < nb && x > sxl && sol_step(x, y, -h, ym, lim, sl); ++n) { p[2 * n] = x; p[2 * n + 1] = y; }
  for (int i = 0, j = n - 1; i < j; ++i, --j)
    for (int k = 0; k < 2; ++k) { const float t = p[2 * i + k]; p[2 * i + k] = p[2 * j + k]; p[2 * j + k] = t; }
  x = SX0[c]; y = SY0[c];
  p[2 * n] = x; p[2 * n + 1] = y; ++n;
  for (sl = 0; n < SOLMAX && x < sxh && sol_step(x, y, h, ym, lim, sl); ++n) { p[2 * n] = x; p[2 * n + 1] = y; }
  PSN[c] = n; PT0[c] = p[0]; PT1[c] = p[2 * n - 2];
}
// solution c at x (a Runge-Kutta step from its point before x: the table's 6 digits), NAN outside
static float sol_y(int c, float x) {
  const float * p = PS_ + PSS_ * c;
  int lo = 0, hi = PSN[c] - 1;
  if (hi < 1 || !(x >= p[0] && x <= p[2 * hi])) return NAN;
  while (hi - lo > 1) { const int m = (lo + hi) / 2; if (p[2 * m] <= x) lo = m; else hi = m; }
  return x == p[2 * lo] ? p[2 * lo + 1] : rk4(p[2 * lo], p[2 * lo + 1], x - p[2 * lo]);
}
// the field's segments this frame: every 20 px (at multiples of that step: still while panning),
// 14 px long
static NOINL void fieldseg() {
  const float w = vx1 - vx0, h = vy1 - vy0, kx = (UI_W - 1) / w, ky = (GH - 1) / h, sx = 20 / kx, sy = 20 / ky;
  nfs_ = 0;
  for (float y = ceilf(vy0 / sy) * sy; y <= vy1 && nfs_ < NFS; y += sy)
    for (float x = ceilf(vx0 / sx) * sx; x <= vx1 && nfs_ < NFS; x += sx) {
      const float m = fxy(x, y);
      if (!fin(m)) continue;
      float dx = kx, dy = -m * ky; // (pixels, y down)
      if (fabsf(dy) > 1e4f * kx) { dx = 0; dy = 7 * 16; }
      else { const float s = 7 * 16 / sqrtf(dx * dx + dy * dy); dx *= s; dy *= s; }
      const int cx = (int)((x - vx0) * kx * 16) + 8, cy = (int)(GT * 16 + (vy1 - y) * ky * 16);
      int * q = FS_ + 4 * nfs_++;
      q[0] = cx - (int)dx; q[1] = cy - (int)dy; q[2] = cx + (int)dx; q[3] = cy + (int)dy;
    }
}
// the points' places this frame, 1/16 px (within 64 screens), NONE: a break
static NOINL void pmap() {
  const float w = vx1 - vx0, h = vy1 - vy0, kx = 16 * (UI_W - 1) / w, ky = 16 * (GH - 1) / h;
  const float xl = vx0 - 64 * w, xh = vx1 + 64 * w, yl = vy0 - 64 * h, yh = vy1 + 64 * h;
  for (int c = 0; c < NC_; ++c) {
    const float * p = PS_ + PSS_ * c;
    int * m = PM_ + PSS_ * c;
    for (int k = PSN[c]; k > 0; --k, p += 2, m += 2) {
      float x = p[0], y = p[1];
      if (!fin(x)) { m[0] = NONE; continue; }
      x = x < xl ? xl : x > xh ? xh : x;
      y = y < yl ? yl : y > yh ? yh : y;
      m[0] = (int)((x - vx0) * kx) + 8; m[1] = (int)(GT * 16 + (vy1 - y) * ky);
    }
  }
}
// one coordinate's range over the points v (n): all of it, or its middle 80% when the rest is
// more than 10 times wider (tan(t) near its asymptotes: x to 1e6, the curve a flat line)
static NOINL void prange(float * v, int n, float & lo, float & hi) {
  for (int i = 1; i < n; ++i) // (insertion sort: 97 at most)
    for (int j = i; j > 0 && v[j] < v[j - 1]; --j) { const float t = v[j]; v[j] = v[j - 1]; v[j - 1] = t; }
  lo = v[0]; hi = v[n - 1];
  const float a = v[n / 10], b = v[n - 1 - n / 10];
  if (hi - lo > 10 * (b - a) + 1e-6f) { lo = a - (b - a) / 4; hi = b + (b - a) / 4; }
}
// the extent of the curves (their 97 points in t), with equal units and a margin: the window
static NOINL void pfit() {
  float * xs = (float *)malloc(2 * 97 * NGRP * sizeof(float)), * ys = xs + 97 * NGRP, x0 = -10, x1 = 10, y0 = -10, y1 = 10;
  int n = 0;
  for (int c = 0; c < NC_ && xs; ++c)
    for (int k = 0; k <= 96; ++k) {
      float x, y;
      if (pt_at(c, PT0[c] + (PT1[c] - PT0[c]) * k / 96, x, y) && fabsf(x) < 1e6f && fabsf(y) < 1e6f) { xs[n] = x; ys[n++] = y; }
    }
  if (n) { prange(xs, n, x0, x1); prange(ys, n, y0, y1); }
  free(xs);
  const float w = 1.25f * (x1 - x0 > (y1 - y0) * UI_W / GH ? x1 - x0 : (y1 - y0) * UI_W / GH) + 1e-3f, h = w * (GH - 1) / (UI_W - 1);
  vx0 = (x0 + x1) / 2 - w / 2; vx1 = vx0 + w; vy0 = (y0 + y1) / 2 - h / 2; vy1 = vy0 + h;
  psw = 0;
}
// the trace point (false: y not a number there; a function's x is tx all the same)
static NOINL bool tpt(float & x, float & y) {
  if (KIND_) return pt_at(tf, tx, x, y);
  if (tf == NF && DATN_) { x = DAT_[tpi]; y = DAT_[DATN_ + tpi]; return true; } // (DeltaCE: a data point)
  x = tx; y = val(tf, tx);
  return fin(y);
}

// ---------------------------------------------------------------- the key points
struct kpt { float x, y, t; unsigned char kinds, f, lw; char * label; char lt[24]; }; // kinds: 1 << kind; label: math (giac syntax); lt: in decimals, lw wide; t: a parametric curve's
static kpt * K_;
static int NK = -1; // -1: not computed yet
static const char * const KIND[] = {"Zero", "y-intercept", "Local max", "Local min", "Inflection", "Crossing", "Horizontal tangent", "Vertical tangent"};
// a point's name: its main kind (x^2's (0, 0): local min, sin's zeros: zero, not inflection); a
// parametric curve's zero: an x-intercept
static NOINL const char * kname(int m) {
  static const unsigned char order[8] = {2, 3, 6, 7, 0, 4, 5, 1};
  for (int k = 0; k < 8; ++k) if (m >> order[k] & 1) return KIND_ && !order[k] ? "x-intercept" : KIND[order[k]];
  return "";
}
static inline unsigned char ccol(int c) { return KIND_ ? COLS[c & 3] : col(c); } // curve c's color (f: a function; else a curve)
// a key point of curve f (a function; a parametric curve at t)
static NOINL void kpt_add(int kind, int f, const gen & x, const gen & y, float t = NAN) {
  if (NK >= 40) return;
  const float xf = fl(x);
  if (!fin(xf)) return;
  float yf = fl(y);
  if (!fin(yf) && !KIND_) yf = val(f, xf);
  for (int j = 0; j < NK; ++j) // one point: its kinds (x^2's zero, minimum, y-intercept; a period's both ends; the circle's halves at (5, 0))
    if ((KIND_ ? K_[j].f == f : GRP[K_[j].f] == GRP[f]) && fabsf(K_[j].x - xf) <= 1e-3f * (1 + fabsf(xf)) && !(fabsf(K_[j].y - yf) > 1e-3f * (1 + fabsf(yf)))) { K_[j].kinds |= 1 << kind; return; }
  kpt & k = K_[NK];
  k.x = xf;
  k.y = yf;
  k.t = t;
  k.kinds = (unsigned char)(1 << kind); k.f = (unsigned char)f;
  { // its coordinates in decimals, for the graph: (1.73, 0)
    char u[16];
    strcpy(k.lt, "("); fmtg(u, k.x, 3); strcat(k.lt, u); strcat(k.lt, ", ");
    if (fin(k.y)) fmtg(u, k.y, 3); else strcpy(u, "?");
    strcat(k.lt, u); strcat(k.lt, ")");
    k.lw = (unsigned char)ui_text_width(&ui_tr9, k.lt, -1);
  }
  std::string s = "(";
  s += x.print(contextptr); s += ","; s += y.print(contextptr); s += ")";
  k.label = (char *)malloc(s.size() + 1);
  if (k.label) { strcpy(k.label, s.c_str()); ++NK; }
}
static void kpt_clear() {
  for (int i = 0; i < (NK > 0 ? NK : 0); ++i) free(K_[i].label);
  NK = -1;
}
// x and y of a parametric or polar curve (giac)
static NOINL void pxy(int c, gen & gx, gen & gy) {
  if (KIND_ == 1) { gx = F_[2 * c]; gy = F_[2 * c + 1]; return; }
  gx = F_[c] * symbolic(at_cos, *X_); gy = F_[c] * symbolic(at_sin, *X_);
}
// a parametric or polar curve's key points: where y = 0, x = 0, dy/dt = 0 (a horizontal tangent),
// dx/dt = 0 (vertical), t from PT0 to PT1, exact when giac finds them
static NOINL void pkpt_curve(int c) {
  const gen & t = *X_;
  gen gx, gy;
  pxy(c, gx, gy);
  static const unsigned char kd[4] = {0, 1, 6, 7};
  for (int q = 0; q < 4 && !giac::ctrl_c && !giac::interrupted; ++q) {
    const gen & u = q & 1 ? gx : gy;
    vecteur r;
    analyze_zeros(q < 2 ? u : eval(derive(u, t, contextptr), 1, contextptr), t, PT0[c], PT1[c], false, r, contextptr);
    for (unsigned j = 0; j < r.size(); ++j) {
      const float v = fl(r[j]);
      if (!(v >= PT0[c] - 1e-4f && v <= PT1[c] + 1e-4f)) continue;
      kpt_add(kd[q], c, normal(eval(subst(gx, t, r[j], false, contextptr), 1, contextptr), contextptr),
              normal(eval(subst(gy, t, r[j], false, contextptr), 1, contextptr), contextptr), v);
    }
  }
}
// The key points of every curve (the analysis', analyze.cc: its period repeated over the view) and
// where two curves cross; a few seconds on the calculator: once, when first needed
static NOINL void kpt_find() {
  if (NK >= 0) return;
  lnk = -2;
  if (!K_) K_ = (kpt *)malloc(40 * sizeof(kpt));
  if (!K_) return;
  NK = 0;
  focus_status_msg("Finding the key points");
  focus_busy(UI_H / 2);
  giac::control_c_hook = focus_busy_tick;
  dconsole_mode = 0;
  for (int c = 0; c < NC_ && (KIND_ == 1 || KIND_ == 2); ++c) pkpt_curve(c);
  for (int i = 0; i < NF && !KIND_ && !giac::ctrl_c && !giac::interrupted; ++i) {
    if (fin(VX[i])) continue; // (a vertical line)
    std::vector<an_pt> pts;
    gen per;
    analyze_points(F_[i], pts, per, contextptr);
    const float pf = is_zero(per) ? 0 : fl(per);
    for (unsigned j = 0; j < pts.size(); ++j) {
      if (pf <= 0 || pts[j].kind == 1) { kpt_add(pts[j].kind, i, pts[j].x, pts[j].y); continue; }
      const gen & x = pts[j].x; // periodic: each one over the view (and around it), as x + k period
      const float x0 = fl(x);
      const int k0 = (int)floorf((vx0 - x0) / pf) - 1, k1 = (int)ceilf((vx1 - x0) / pf) + 1;
      for (int k = k0; k <= k1 && k - k0 < 12; ++k)
        kpt_add(pts[j].kind, i, k ? ratnormal(x + gen(k) * per, contextptr) : x, pts[j].y);
    }
  }
  for (int g = 0; g < NGR; ++g) // two typed curves crossing: both equations, x and y solved for (y-f(x) for a function)
    for (int h = g + 1; h < NGR && !giac::ctrl_c; ++h) {
      gen sv = makevecteur(*X_, Yv_ ? *Yv_ : gen(identificateur("y")));
      gen s2 = _solve(makesequence(makevecteur(symbolic(at_equal, makesequence(EQ_[g], 0)), symbolic(at_equal, makesequence(EQ_[h], 0))), sv), contextptr);
      if (s2.type != _VECT) continue;
      for (unsigned k = 0; k < s2._VECTptr->size(); ++k) {
        const gen p = s2[k];
        if (p.type != _VECT || p._VECTptr->size() != 2) continue;
        const gen x = normal(p[0], contextptr), y = normal(p[1], contextptr);
        const float xf = fl(x), yf = fl(y);
        int f = -1; // the curve it is on (the circle's upper half, a line)
        for (int i = 0; i < NF && f < 0; ++i) if ((GRP[i] == g || GRP[i] == h) && !(fabsf(val(i, xf) - yf) > 1e-3f * (1 + fabsf(yf)))) f = i;
        if (f >= 0) kpt_add(5, f, x, y);
      }
    }
  for (int i = 1; i < NK; ++i) // left to right (parametric: by curve, t)
    for (int j = i; j > 0 && (KIND_ ? K_[j].f < K_[j - 1].f || (K_[j].f == K_[j - 1].f && K_[j].t < K_[j - 1].t) : K_[j].x < K_[j - 1].x); --j) { const kpt t = K_[j]; K_[j] = K_[j - 1]; K_[j - 1] = t; }
  giac::control_c_hook = 0;
  dconsole_mode = 1;
  focus_busy(0);
  giac::ctrl_c = giac::interrupted = false;
  focus_status_msg(0);
}

// ---------------------------------------------------------------- drawing
static NOINL void hline(int y, int c) { if (y >= GT && y < GB) ui_fill(0, y, UI_W, 1, ui_col(0, c)); }
static NOINL void vline(int x, int c) { if (x >= 0 && x < UI_W) ui_fill(x, GT, 1, GH, ui_col(0, c)); }
// the dots (radius 2, 4) and the key points' rings (3, the background inside) as ui_rrect and
// ui_rframe draw them, their pixels worked out once (tools: the same formula): 0 untouched, 1-3 the
// color's shade, 4 the background; the corners' arithmetic took 15% of a frame with 40 key points
static const unsigned char DOT2[25] = {2,3,3,3,2,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,2,3,3,3,2};
static const unsigned char RING3[49] = {0,2,3,3,3,2,0,2,1,4,4,4,1,2,3,4,4,4,4,4,3,3,4,4,4,4,4,3,3,4,4,4,4,4,3,2,1,4,4,4,1,2,0,2,3,3,3,2,0};
static const unsigned char DOT4[81] = {0,1,3,3,3,3,3,1,0,1,3,3,3,3,3,3,3,1,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,1,3,3,3,3,3,3,3,1,0,1,3,3,3,3,3,1,0};
static NOINL void disc(int X, int Y, int r, int c, bool fill) {
  if (X < -r || X >= UI_W + r || Y < ui_cy0 - r || Y >= ui_cy1 + r) return;
  const unsigned char * m = r == 2 ? DOT2 : r == 3 ? RING3 : DOT4, * ramp = ui_ramp(0, c, UC_BG), bg = ui_col(0, UC_BG);
  (void)fill;
  const int n = 2 * r + 1;
  unsigned char * row = ui_fb + (Y - r) * UI_W;
  for (int j = 0, y = Y - r; j < n; ++j, ++y, row += UI_W) {
    if (y < ui_cy0 || y >= ui_cy1) { m += n; continue; }
    for (int i = 0, x = X - r; i < n; ++i, ++x, ++m)
      if (*m && x >= ui_cx0 && x < ui_cx1) row[x] = *m == 4 ? bg : ramp[*m];
  }
}
// a segment (1/16 px) cut to the rows lo..hi (1/16 px): a curve going off the screen, its visible part
static NOINL void seg16(int x0, int y0, int x1, int y1, int lo, int hi, int th, const unsigned char * r) {
  if (y0 > y1) { int t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }
  if (y1 < lo || y0 > hi) return;
  if (y0 < lo) { x0 += (int)((long)(x1 - x0) * (lo - y0) / (y1 - y0)); y0 = lo; }
  if (y1 > hi) { x1 = x0 + (int)((long)(x1 - x0) * (hi - y0) / (y1 - y0)); y1 = hi; }
  ui_seg16(x0, y0, x1, y1, th, r);
}
// a polar area's region: the pole, then the curve from θ = fan_a to fan_b (1/16 px); its points (0: none)
static int FAN_[2 * 66];
static NOINL int fan_of(int * fan) {
  if (KIND_ != 2 || !(fan_a < fan_b)) return 0;
  const float kx = 16 * (UI_W - 1) / (vx1 - vx0), ky = 16 * (GH - 1) / (vy1 - vy0);
  fan[0] = (int)(-vx0 * kx) + 8; fan[1] = (int)(GT * 16 + vy1 * ky);
  int n = 1;
  for (int k = 0; k <= 64; ++k) {
    float x, y;
    if (!pt_at(tf, fan_a + (fan_b - fan_a) * k / 64, x, y) || fabsf(x - vx0) > 64 * (vx1 - vx0) || fabsf(y - vy0) > 64 * (vy1 - vy0)) continue;
    fan[2 * n] = (int)((x - vx0) * kx) + 8; fan[2 * n + 1] = (int)(GT * 16 + (vy1 - y) * ky); ++n;
  }
  return n < 3 ? 0 : n;
}
// the polygon xy (n points, 1/16 px) filled in the band's rows b..e (even-odd: a polar area's fan)
static NOINL void fillpoly(const int * xy, int n, int b, int e, unsigned char c) {
  if (n < 3) return;
  for (int y = b; y < e; ++y) {
    const int yc = y * 16 + 8;
    int xs[16], m = 0;
    for (int i = 0, j = n - 1; i < n && m < 16; j = i++) {
      const int y0 = xy[2 * j + 1], y1 = xy[2 * i + 1];
      if ((y0 <= yc) == (y1 <= yc)) continue;
      const int x = xy[2 * j] + (int)((long)(xy[2 * i] - xy[2 * j]) * (yc - y0) / (y1 - y0));
      int k = m++;
      for (; k > 0 && xs[k - 1] > x; --k) xs[k] = xs[k - 1];
      xs[k] = x;
    }
    for (int k = 0; k + 1 < m; k += 2) {
      int x0 = xs[k] >> 4, x1 = xs[k + 1] >> 4;
      if (x0 < 0) x0 = 0;
      if (x1 >= UI_W) x1 = UI_W - 1;
      if (x1 >= x0) ui_fill(x0, y, x1 - x0 + 1, 1, c);
    }
  }
}
// a curve's segment while a key is held: a run of pixels per column, 2 at least (no anti-aliasing:
// 15 thousand cycles a segment less); x in pixels, y in 1/16 px, the band's rows lo..hi
static NOINL void runs(int x0, int y0, int x1, int y1, int lo, int hi, unsigned char col) {
  const int n = x1 - x0, d = n > 1 ? (y1 - y0) / n : y1 - y0;
  for (int k = 0, y = y0; k < n; ++k, y += d) {
    int a = y >> 4, z = (k == n - 1 ? y1 : y + d) >> 4;
    if (a > z) { const int t = a; a = z; z = t; }
    if (z - a < 1) z = a + 1;
    if (a < lo) a = lo;
    if (z >= hi) z = hi - 1;
    if (z >= a) ui_fill(x0 + k, a, 1, z - a + 1, col);
  }
}
// a number on the grid: on the background (not crossed by its line), if in the band b..e
static NOINL int text_at(const char * s, int w, int x, int base, int c, int b, int e) {
  if (base + 3 < b || base - 8 >= e) return w;
  ui_fill(x - 1, base - 8, w + 2, 11, ui_col(0, UC_BG));
  ui_draw_text(&ui_tr9, s, -1, x, base, ui_ramp(0, c, UC_BG), 0);
  return w;
}
static bool tang_on() { return tang && mode && (KIND_ || tf < NF); } // (DeltaCE: not on data points)
static bool shown_note() { return tang_on() || (note[0] && (note_x < 0 || (KIND_ ? note_t == tx : note_x == px_of(tx)))); }
static char * tang_note();
static float slope(int i, float x);
static float tslope();
static NOINL void tangent_text(char * t, float m, float b);
// the readout card: x and y of the trace point (the middle: pan), the note (a key point's exact
// values, the tangent, the area) under them
static void readout(int b, int e) {
  const bool n = shown_note();
  const int w = 200, x0 = 4;
  // the note's line: its words, then its math (a fraction: the card taller)
  const char * nt = n ? (tang_on() ? tang_note() : note) : 0, * m = n ? strchr(nt, '\x01') : 0;
  const int tw = n ? ui_text_width(&ui_tb10, nt, m ? (int)(m - nt) : -1) : 0;
  mi_layout L;
  int lv = 0, asc = 10, desc = 2;
  if (m) {
    lv = ui_math_fit(m + 1, strlen(m + 1), -1, 6, w - tw - 22, 40, 0, L);
    if (L.asc > asc) asc = L.asc;
    if (L.desc > desc) desc = L.desc;
  }
  const int h = n ? 20 + asc + desc + 3 : 20, yy = GB - h - 4, base = yy + 18 + asc;
  if (yy + h <= b || yy >= e) return;
  ui_rrect(0, x0, yy, w, h, 6, UC_CARD, UC_BG);
  ui_rframe(0, x0, yy, w, h, 6, UC_LINE, UC_CARD);
  if (mode) ui_rrect(0, x0 + 6, yy + 7, 6, 6, 3, ccol(tf), UC_CARD);
  ui_draw_text(&ui_tb10, FR->ln, -1, x0 + (mode ? 16 : 8), yy + 14, ui_ramp(0, UC_INK, UC_CARD), 0);
  if (n) {
    ui_draw_text(&ui_tb10, nt, m ? (int)(m - nt) : -1, x0 + 8, base, ui_ramp(0, UC_ACC, UC_CARD), 0);
    if (m) ui_math_draw(L, m + 1, lv, x0 + 12 + tw, base, 0, UC_ACC, UC_CARD, UC_ACC);
  }
}
// The equations F(x, y) = 0 not solved for y, in the band b..e: F at the corners of G x G pixel
// cells, the curve across each cell between the edges where its sign changes (marching squares)
static NOINL void implicit(int b, int e, int G) {
  enum { NC = UI_W / 6 + 2 }; // (G >= 6)
  float A[NC], B[NC], XS[NC];
  signed char SA[NC], SB[NC]; // each value's sign (-1, 1), 0: none (computed once, not per cell: a float compare is a call)
  const int nx = (UI_W - 1) / G + 1, lo = (b - 2) * 16, hi = (e + 2) * 16;
  const float pw = (vx1 - vx0) / (UI_W - 1), ph = (vy1 - vy0) / (GH - 1);
  { float x = vx0; for (int i = 0; i < nx; ++i, x += G * pw) XS[i] = x; } // (the columns' x once)
  for (int m = 0; m < NI; ++m) {
    const unsigned char * rp = ui_ramp(0, COLS[IG_[m] & 3], UC_BG);
    int r = GT + (b - GT) / G * G;
    float y = vy1 - (r - GT) * ph;
    for (int i = 0; i < nx; ++i) { A[i] = run_prog(IP_[m], IM_[m], XS[i], y); SA[i] = !fin(A[i]) ? 0 : A[i] < 0 ? -1 : 1; }
    for (; r < e; r += G) {
      if (G < 14 && key_waiting()) return; // (a key: the frame stops, the key is read next)
      y -= G * ph;
      for (int i = 0; i < nx; ++i) { B[i] = run_prog(IP_[m], IM_[m], XS[i], y); SB[i] = !fin(B[i]) ? 0 : B[i] < 0 ? -1 : 1; }
      for (int i = 0; i + 1 < nx; ++i) {
        const signed char sg4[4] = {SA[i], SA[i + 1], SB[i + 1], SB[i]}; // the corners, clockwise from the top left
        if (!sg4[0] || !sg4[1] || !sg4[2] || !sg4[3] || (sg4[0] == sg4[1] && sg4[1] == sg4[2] && sg4[2] == sg4[3])) continue;
        const float v[4] = {A[i], A[i + 1], B[i + 1], B[i]};
        int px[4], py[4], n = 0;
        static const signed char cx[5] = {0, 1, 1, 0, 0}, cy[5] = {0, 0, 1, 1, 0};
        for (int k = 0; k < 4; ++k) { // the edges where the sign changes, the crossing by interpolation
          if (sg4[k] == sg4[(k + 1) & 3]) continue;
          const float t = v[k] / (v[k] - v[(k + 1) & 3]);
          px[n] = (int)(((i + cx[k] + (cx[k + 1] - cx[k]) * t) * G) * 16) + 8;
          py[n++] = (int)((r + (cy[k] + (cy[k + 1] - cy[k]) * t) * G) * 16);
        }
        for (int k = 0; k + 1 < n; k += 2) seg16(px[k], py[k], px[k + 1], py[k + 1], lo, hi, 26, rp);
      }
      for (int i = 0; i < nx; ++i) { A[i] = B[i]; SA[i] = SB[i]; }
    }
  }
}
static char tnote[48];
static char * tang_note() { return tnote; }
// the grid's lines from lo to hi at step st (k st, k an integer), their numbers
static NOINL int lines_of(gl * g, float lo, float hi, float st, bool x) {
  int n = 0;
  for (long k = (long)ceilf(lo / st), k1 = (long)floorf(hi / st); k <= k1 && n < NG; ++k) {
    const float v = k * st;
    g[n].v = v;
    g[n].p = x ? px_of(v) : py_of(v);
    g[n].t[0] = 0;
    if (k) fmt(g[n].t, v, st);
    g[n].w = (unsigned char)ui_text_width(&ui_tr9, g[n].t, -1);
    ++n;
  }
  return n;
}
extern "C" { volatile unsigned graph_frames; } // frames drawn (for tools/emu: frames a second)
static NOINL void paint(bool fast) {
  frame & F = *FR;
  ++graph_frames;
  if (!KIND_) sample(fast ? 3 : 1);
  else { // (curves: sampled again for a view twice as wide or narrow, not while a key is held)
    const float w = vx1 - vx0;
    if (!psw || (!fast && (w < psw / 2 || w > psw * 2 || (KIND_ == 3 && (vx0 < sxl || vx1 > sxh))))) psample();
    pmap();
    if (KIND_ == 3) fieldseg();
  }
  const float sx = step_of(vx1 - vx0, UI_W, 44), sy = step_of(vy1 - vy0, GH, 32);
  const int ax = px_of(0), ay = py_of(0);
  F.ngx = lines_of(F.gx, vx0, vx1, sx, true);
  F.ngy = lines_of(F.gy, vy0, vy1, sy, false);
  F.nd = 0; // zoomed in: the curves' points at the grid's x, their coordinates
  if (!KIND_ && (coords || vx1 - vx0 <= w0 / 2))
    for (int i = 0; i < NF; ++i) {
      int le = -99; // (where the last coordinates end)
      for (int k = 0; k < F.ngx && F.nd < ND; ++k) {
        const float xg = F.gx[k].v, y = val(i, xg);
        if (!fin(y)) continue;
        dt & d = F.d[F.nd];
        d.x = F.gx[k].p; d.y = py_of(y); d.f = (unsigned char)i; d.t[0] = 0; d.w = 0;
        if (d.y < GT - 3 || d.y > GB + 3) continue;
        if (coords && d.x + 4 > le + 4) {
          char u[16];
          strcpy(d.t, "("); fmt(u, xg, sx); trim(u); strcat(d.t, u); strcat(d.t, ", ");
          fmtg(u, y, 4); strcat(d.t, u); strcat(d.t, ")");
          d.w = (unsigned char)ui_text_width(&ui_tr9, d.t, -1);
          le = d.x + 4 + d.w;
        }
        ++F.nd;
      }
    }
  for (int k = 0; k < NK && k < NKP; ++k)
    if (shx != -1000 && lnk == NK) { F.kx[k] -= shx; if (F.ky[k] != NONE) F.ky[k] += shy; }
    else { F.kx[k] = px_of(K_[k].x); F.ky[k] = fin(K_[k].y) ? py_of(K_[k].y) : NONE; }
  lnk = NK;
  for (int k = 0; k < NK && k < NKP; ++k) { // the coordinates: off the curve (a maximum's above it, a
    // minimum's below, the others up and right), not over one already placed
    F.lx[k] = -1;
    const int X = F.kx[k], Y = F.ky[k], m = K_[k].kinds, w = K_[k].lw, y = m & 4 ? Y - 7 : m & 8 ? Y + 14 : Y - 5;
    int x = m & 12 ? X - w / 2 : X + 5;
    if (x + w > UI_W - 2) x = X - 5 - w; // (at the right edge: left of it)
    if (!klabels || Y == NONE || x < 2 || x + w > UI_W - 2 || y < GT + 10 || y > GB - 4) continue;
    bool free_ = true;
    for (int j = 0; j < k && free_; ++j)
      if (F.lx[j] >= 0 && x < F.lx[j] + K_[j].lw + 3 && F.lx[j] < x + w + 3 && y > F.ly[j] - 11 && y < F.ly[j] + 11) free_ = false;
    if (free_) { F.lx[k] = (short)x; F.ly[k] = (short)y; }
  }
  int tl[4]; // the tangent, cut to the plot (1/16 px)
  float tpxf, tpyf;
  const bool tok = mode && tpt(tpxf, tpyf);
  bool tg = tang && tok && (KIND_ || tf < NF); // (not on data points)
  if (tg) { // (its equation in the readout, for the trace point)
    const float x = tpxf, y = tpyf, m = tslope();
    if (!fin(m)) { // (vertical: x = c)
      char u[16];
      strcpy(tnote, "Tangent  x = "); fmtg(u, x); strcat(tnote, u);
      const int X = px_of(x) * 16 + 8;
      tl[0] = tl[2] = X; tl[1] = (GT - 2) * 16; tl[3] = (GB + 2) * 16;
    } else
    tangent_text(tnote, m, y - m * x);
    float xa = 0, xb = UI_W - 1, ra = row_of(y + m * (vx0 - x)), rb = row_of(y + m * (vx1 - x));
    if (ra > rb) { float t = ra; ra = rb; rb = t; t = xa; xa = xb; xb = t; }
    if (fin(m)) tg = rb >= GT - 2 && ra <= GB + 2;
    if (tg && fin(m)) {
      if (ra < GT - 2) { xa += (xb - xa) * (GT - 2 - ra) / (rb - ra); ra = GT - 2; }
      if (rb > GB + 2) { xb = xa + (xb - xa) * (GB + 2 - ra) / (rb - ra); rb = GB + 2; }
      tl[0] = (int)(xa * 16); tl[1] = (int)(ra * 16); tl[2] = (int)(xb * 16); tl[3] = (int)(rb * 16);
    }
  }
  const float ty = mode ? tpyf : 0;
  const int tpx = mode && (tok || !KIND_) ? px_of(KIND_ ? tpxf : tx) : -100, tpy = tok ? py_of(ty) : NONE;
  { // the readout's line (a curve: t first; polar: θ and r)
    char b1[24], b2[24];
    const float x = mode ? (KIND_ ? tpxf : tx) : (vx0 + vx1) / 2, y = mode ? ty : (vy0 + vy1) / 2;
    F.ln[0] = 0;
    if (KIND_ && KIND_ != 3 && mode) {
      fmtg(b1, tx, 4);
      strcpy(F.ln, KIND_ == 2 ? "\xce\xb8 = " : "t = "); strcat(F.ln, b1); strcat(F.ln, "   ");
      if (KIND_ == 2) { fmtg(b2, run_prog(P_[tf], F_[tf], tx), 4); strcat(F.ln, "r = "); strcat(F.ln, tok ? b2 : "none"); }
    }
    if (KIND_ != 2 || !mode) {
      if (KIND_ == 1 && mode) { fmtg(b1, x, 4); fmtg(b2, y, 4); } // (a curve's point: as t)
      else { fmt(b1, x, sx / 100); fmt(b2, y, sy / 100); }
      if (!KIND_ || !mode || tok) { strcat(F.ln, "x = "); strcat(F.ln, b1); strcat(F.ln, "   y = "); }
      strcat(F.ln, fin(y) && (!KIND_ || !mode || tok) ? b2 : "none");
    }
  }
  const int pa = area_a < area_b ? (px_of(area_a) < 0 ? 0 : px_of(area_a)) : 1, pb = area_a < area_b ? (px_of(area_b) >= UI_W ? UI_W - 1 : px_of(area_b)) : 0;
  const int nfan = fan_of(FAN_); // (static: a big array in paint's frame put its other locals beyond
  // the eZ80's 8-bit IX offsets, 9 KB more code)
  const int lb = ay + 11 < GB - 2 ? (ay < GT ? GT + 10 : ay + 11) : GB - 3; // the x numbers' baseline
  const int rows = ui_band_open(64), r0 = rows ? (GH % rows ? GH % rows : rows) : GH; // (the bands end at the bottom: the card in one)
  for (int b = GT, e = GT + r0; b < GB; b = e, e += rows) {
    if (NI && !fast && b > GT && key_waiting()) break; // (a slow frame, a key: stopped, the key read next)
    const int lo = (b - 2) * 16, hi = (e + 2) * 16;
    if (rows) ui_band_begin(b, e);
    ui_clip(0, b, UI_W, e);
    ui_fill(0, b, UI_W, e - b, ui_col(0, UC_BG));
    if (grid) { // the grid, the axes over it
      for (int k = 0; k < F.ngx; ++k) vline(F.gx[k].p, UC_LINE);
      for (int k = 0; k < F.ngy; ++k) hline(F.gy[k].p, UC_LINE);
    }
    for (int c = pa; c <= pb; ++c) { // the area under the curve: shaded from the x-axis
      int r = F.y[tf * UI_W + c];
      for (int k = 1; r == NONE && k < 3 && c >= k; ++k) r = F.y[tf * UI_W + c - k]; // (fast: every 3 columns)
      if (r == NONE) continue;
      int r0 = r / 16, r1 = ay;
      if (r0 > r1) { const int t = r0; r0 = r1; r1 = t; }
      if (r0 < b) r0 = b;
      if (r1 > e) r1 = e;
      if (r1 > r0) ui_fill(c, r0, 1, r1 - r0, ui_col(0, UC_ACCSOFT));
    }
    fillpoly(FAN_, nfan, b, e, ui_col(0, UC_ACCSOFT)); // (none: 0 points; called always: the band loop not duplicated)
    vline(ax, UC_SUB); hline(ay, UC_SUB);
    for (int k = 0; k < F.ngx; ++k) { // the numbers: under the x-axis (or at the bottom), left of the y-axis
      const gl & g = F.gx[k];
      const int x = g.p - g.w / 2;
      if (g.t[0] && x > 1 && x + g.w < UI_W - 1) text_at(g.t, g.w, x, lb, UC_SUB, b, e);
    }
    for (int k = 0; k < F.ngy; ++k) {
      const gl & g = F.gy[k];
      // left of the y-axis (right of it near the left edge); the axis off the screen: at that edge
      const int x = ax < 0 ? 3 : ax >= UI_W ? UI_W - 3 - g.w : ax - 4 - g.w >= 2 ? ax - 4 - g.w : ax + 4;
      if (g.t[0] && g.p + 4 > GT + 9 && g.p + 4 < GB - 2) text_at(g.t, g.w, x, g.p + 4, UC_SUB, b, e);
    }
    for (int k = 0; k < nfs_ && KIND_ == 3; ++k) { // a slope field's segments
      const int * q = FS_ + 4 * k;
      seg16(q[0], q[1], q[2], q[3], lo, hi, 16, ui_ramp(0, UC_SUB, UC_BG));
    }
    for (int c = 0; c < NC_ && KIND_; ++c) { // parametric, polar: from point to point (a field's solutions)
      const int * m = PM_ + PSS_ * c;
      const unsigned char * r = ui_ramp(0, ccol(c), UC_BG);
      for (int k = 1; k < PSN[c]; ++k, m += 2)
        if (m[0] != NONE && m[2] != NONE) seg16(m[0], m[1], m[2], m[3], lo, hi, 26, r);
    }
    for (int i = 0; i < NF && !KIND_; ++i) { // the curves
      const int * yr = F.y + i * UI_W;
      const unsigned char * jn = F.j + i * UI_W, * r = ui_ramp(0, col(i), UC_BG);
      int pc = -1;
      for (int c = 0; c < UI_W; c += !fast || c == UI_W - 1 ? 1 : c + 3 < UI_W ? 3 : UI_W - 1 - c) { // (the columns sampled)
        const int z = yr[c];
        if (z == NONE) { pc = -1; continue; }
        if (pc >= 0 && jn[c]) {
          const int a = yr[pc];
          if (!((a < lo && z < lo) || (a > hi && z > hi))) {
            if (fast) runs(pc, a, c, z, b, e, r[3]);
            else seg16(pc * 16 + 8, a, c * 16 + 8, z, lo, hi, 26, r);
          }
        }
        pc = c;
      }
      if (fin(VX[i])) { const int X = px_of(VX[i]); seg16(X * 16 + 8, lo, X * 16 + 8, hi, lo, hi, 26, r); } // (x = 3)
    }
    for (int k = 0; k < F.ne; ++k) // to where a domain ends
      seg16(F.ec[k] * 16 + 8, F.y[F.ei[k] * UI_W + F.ec[k]], F.ex[k], F.ey[k], lo, hi, 26, ui_ramp(0, col(F.ei[k]), UC_BG));
    if (NI) implicit(b, e, fast ? 14 : 6);
    for (int k = 0; k < F.nd; ++k) { // the dots, their coordinates
      const dt & d = F.d[k];
      if (d.y + 3 >= b && d.y - 3 < e) disc(d.x, d.y, 2, col(d.f), true);
      if (d.t[0] && d.y - 5 > GT + 9 && d.y - 5 < GB - 4) text_at(d.t, d.w, d.x + 4, d.y - 5, col(d.f), b, e);
    }
    for (int k = 0; k < DATN_; ++k) { // (DeltaCE) the data points, rings
      const int X = px_of(DAT_[k]), Y = py_of(DAT_[DATN_ + k]);
      if (Y + 4 >= b && Y - 4 < e) disc(X, Y, 3, UC_INK, false);
    }
    for (int k = 0; k < NK && k < NKP; ++k) { // the key points, their coordinates
      if (F.ky[k] == NONE) continue;
      if (F.ky[k] + 4 >= b && F.ky[k] - 4 < e) disc(F.kx[k], F.ky[k], 3, ccol(K_[k].f), false);
      if (F.lx[k] >= 0) text_at(K_[k].lt, K_[k].lw, F.lx[k], F.ly[k], ccol(K_[k].f), b, e);
    }
    if (tg) seg16(tl[0], tl[1], tl[2], tl[3], lo, hi, 18, ui_ramp(0, UC_SUB, UC_BG)); // the tangent
    if (mode) { // the trace point: a filled dot, faint lines through it
      vline(tpx, UC_ACCSOFT);
      if (tpy != NONE) { hline(tpy, UC_ACCSOFT); disc(tpx, tpy, 4, ccol(tf), true); }
    } else { // pan: a small cross in the middle
      const int cx = UI_W / 2, cy = GT + GH / 2;
      ui_fill(cx - 5, cy, 11, 1, ui_col(0, UC_INK)); ui_fill(cx, cy - 5, 1, 11, ui_col(0, UC_INK));
    }
    readout(b, e);
    ui_noclip();
    if (rows) ui_band_end();
  }
  ui_band_close();
}
static NOINL void bar() {
  static const char * const lab[2][5] = {{"pan", "zoom", "points", "table", "more"}, {"trace", "zoom", "points", "table", "more"}};
  focus_bar_tabs(lab[mode], 0);
  focus_status_label(mode ? "GRAPH  TRACE" : "GRAPH  PAN");
}

// ---------------------------------------------------------------- moving
// the keypad's rows as the hardware sees them now (held keys): 6: + -, 7: the arrows
// (the OS leaves the keypad in its "any key" mode, where they are not valid: scanned once first,
// as keypadc's kb_Scan, then the OS's mode again)
static NOINL void kb_scan() {
  volatile unsigned char * const m = (volatile unsigned char *)0xF50000;
  __asm__ volatile("di");
  const unsigned char m0 = *m & 3;
  *m = 2;
  while (*m & 3) ;
  *m = m0;
  __asm__ volatile("ei");
}
static inline unsigned kb_row(int g) { return ((volatile unsigned short *)0xF50010)[g]; }
enum { KB_DOWN = 1, KB_LEFT = 2, KB_RIGHT = 4, KB_UP = 8, KB_ADD = 2, KB_SUB = 4 };
static NOINL void zoom_at(float cx, float cy, float f) {
  const float w = (vx1 - vx0) * f, m = fabsf(cx) > fabsf(cy) ? fabsf(cx) : fabsf(cy);
  if (f < 1 && w < 2e-4f * (1 + m)) return; // (floats: 7 digits; a pixel no finer)
  if (f > 1 && w > 1e7f) return;
  vx0 = cx + (vx0 - cx) * f; vx1 = cx + (vx1 - cx) * f;
  vy0 = cy + (vy0 - cy) * f; vy1 = cy + (vy1 - cy) * f;
}
static NOINL void follow() { // the trace point on the screen
  float x, y;
  const bool ok = tpt(x, y);
  if (KIND_ && !ok) return;
  const float w = vx1 - vx0, h = vy1 - vy0;
  if (x < vx0 + w / 20 || x > vx1 - w / 20) { vx0 = x - w / 2; vx1 = vx0 + w; }
  if (ok && (y < vy0 + h / 20 || y > vy1 - h / 20)) { vy0 = y - h / 2; vy1 = vy0 + h; }
}
// where zooming centers: the trace point, else the middle
static NOINL void zcenter(float & cx, float & cy) {
  cx = (vx0 + vx1) / 2; cy = (vy0 + vy1) / 2;
  if (!mode) return;
  float x, y;
  const bool ok = tpt(x, y);
  if (!KIND_) { cx = x; cy = y; } // (as before: y none, the view's y range reset)
  else if (ok) { cx = x; cy = y; }
}
// a curve's t nearest the middle of the view (trace starts there)
static NOINL float tnear(int c) {
  float best = 1e30f, bt = PT0[c];
  const float mx = (vx0 + vx1) / 2, my = (vy0 + vy1) / 2;
  for (int k = 0; k <= 96; ++k) {
    const float t = PT0[c] + (PT1[c] - PT0[c]) * k / 96;
    float x, y;
    if (!pt_at(c, t, x, y)) continue;
    const float d = fabsf(x - mx) * pkx + fabsf(y - my) * pky;
    if (d < best) { best = d; bt = t; }
  }
  return bt;
}
// a key that moves (arrows; + -): moved while it is held, a frame at a time, faster the longer
static void hold() {
  int fr = 0;
  for (;; ++fr) {
    kb_scan();
    const unsigned a = kb_row(7), z = kb_row(6);
    if (!(a & 15) && !(z & (KB_ADD | KB_SUB))) break;
    const float sp = fr < 4 ? 1 : fr < 12 ? 2 : 3, w = vx1 - vx0, h = vy1 - vy0;
    float cx, cy;
    if (z & (KB_ADD | KB_SUB)) zcenter(cx, cy);
    if (z & KB_ADD) zoom_at(cx, cy, 0.88f);
    if (z & KB_SUB) zoom_at(cx, cy, 1 / 0.88f);
    if (vy0 != vy0 || vy1 - vy0 <= 0) { vy0 = -10; vy1 = 10; }
    if (!mode) {
      const float dx = 9 * sp * w / (UI_W - 1), dy = 8 * sp * h / (GH - 1); // (whole pixels: the rows reused; 9: every 3)
      if (a & KB_LEFT) { vx0 -= dx; vx1 -= dx; }
      if (a & KB_RIGHT) { vx0 += dx; vx1 += dx; }
      if (a & KB_UP) { vy0 += dy; vy1 += dy; }
      if (a & KB_DOWN) { vy0 -= dy; vy1 -= dy; }
    } else if (a & (KB_LEFT | KB_RIGHT)) { // (a curve: t by 1/360 of its range)
      tx += (a & KB_RIGHT ? 1 : -1) * (KIND_ ? (PT1[tf] - PT0[tf]) / 360 : w / (UI_W - 1)) * (fr < 6 ? 1 : fr < 16 ? 3 : 6);
      if (KIND_) tx = tx < PT0[tf] ? PT0[tf] : tx > PT1[tf] ? PT1[tf] : tx;
      follow();
    }
    paint(true);
    if (fr == 0) os_wait_1ms(120); // (a tap moves once)
  }
  key_discard();
  while (key_waiting()) key_discard(); // (the presses the OS saw meanwhile)
  paint(false);
}

// ---------------------------------------------------------------- the menus
static NOINL void fit_y() { // the y range of the curves over the view
  float lo = 1e30f, hi = -1e30f;
  for (int i = 0; i < NF; ++i)
    for (int c = 0; c < UI_W; c += 2) {
      const float y = val(i, col_x(c));
      if (fin(y)) { if (y < lo) lo = y; if (y > hi) hi = y; }
    }
  if (lo > hi) { lo = -10; hi = 10; }
  if (hi - lo < 1e-6f) { lo -= 1; hi += 1; }
  const float m = (hi - lo) / 10;
  vy0 = lo - m; vy1 = hi + m;
}
static NOINL bool ask(const char * t, const char * sub, float & v, gen * exact = 0) {
  std::string s;
  if (focus_prompt(t, sub, s, false) != KEY_CTRL_EXE || s.empty()) return false; // (pi/3, sqrt(2), 1/2)
  const gen e(s, contextptr);
  if (exact) *exact = eval(e, 1, contextptr);
  v = fl(e);
  return fin(v);
}
static void bar();
static void redraw() { bar(); paint(false); } // (a menu closed: gone before a prompt)
static void zoom_menu() {
  static const char * const lab[] = {"Zoom in", "Zoom out", "Fit the curve", "Standard", "Trig", "Square", "Window..."};
  static const char * const hint[] = {"+", "-", 0, "-10 to 10", "x: -2pi to 2pi", "equal units", 0};
  const int c = focus_card_list(1, "zoom", 0, lab, hint, 7, 0);
  float cx, cy;
  zcenter(cx, cy);
  switch (c) {
  case 0: zoom_at(cx, cy, 0.5f); break;
  case 1: zoom_at(cx, cy, 2); break;
  case 2: if (KIND_ == 1 || KIND_ == 2) pfit(); else if (!KIND_) fit_y(); break;
  case 3: vx0 = vy0 = -10; vx1 = vy1 = 10; break;
  case 4: vx0 = -6.2832f; vx1 = 6.2832f; vy0 = -4; vy1 = 4; break;
  case 5: { const float m = (vy0 + vy1) / 2, h = (vx1 - vx0) * GH / UI_W; vy0 = m - h / 2; vy1 = m + h / 2; } break;
  case 6: {
    redraw();
    float a = vx0, b = vx1, p = vy0, q = vy1;
    if (ask("Window", "x min", a) && ask("Window", "x max", b) && ask("Window", "y min", p) && ask("Window", "y max", q) && a < b && p < q) { vx0 = a; vx1 = b; vy0 = p; vy1 = q; }
  } break;
  }
}
// giac at work: "Calculating" after a moment, ON stops it, its notes nowhere
static NOINL void working(bool on) {
  if (on) { focus_busy(UI_H / 2); giac::control_c_hook = focus_busy_tick; dconsole_mode = 0; return; }
  dconsole_mode = 1;
  giac::control_c_hook = 0;
  focus_busy(0);
  giac::ctrl_c = giac::interrupted = false;
}
// (x,y), or x=x without y, in giac syntax into b (n bytes); else in decimals
static NOINL void gtext(char * b, int n, const gen & x, const gen * y) {
  std::string m = y ? "(" : "x=";
  m += x.print(contextptr);
  if (y) { m += ","; m += y->print(contextptr); m += ")"; }
  if ((int)m.size() < n) { strcpy(b, m.c_str()); return; }
  char u[16];
  strcpy(b, y ? "(" : "x="); fmtg(u, fl(x), 4); strcat(b, u);
  if (y) { strcat(b, ","); fmtg(u, fl(*y), 4); strcat(b, u); strcat(b, ")"); }
}
// f_i at x, exact (sin(pi/3): sqrt(3)/2), its value v; false if not a real number there
static NOINL bool exact_at(int i, const gen & x, gen & y, float & v) {
  y = normal(eval(subst(F_[i], *X_, x, false, contextptr), 1, contextptr), contextptr);
  v = fl(y);
  return fin(v);
}
// the trace point on curve i at x (exact x, y); the readout's note: words, then (x, y)
static NOINL void point_at(int i, const gen & x, const gen & y, const char * words) {
  mode = 1; tf = i; tx = fl(x);
  follow();
  strcpy(note, words); strcat(note, "\x01");
  gtext(note + strlen(note), sizeof(note) - strlen(note), x, &y);
  note_x = px_of(tx);
}
// Value at x: an x typed (exact: pi/3), the point there (also enter in trace mode)
static NOINL void value_at() {
  float x, y;
  gen gx, gy;
  if (!ask("Value at x =", "x", x, &gx)) return;
  if (!exact_at(tf, gx, gy, y)) { focus_status_msg("Not defined there"); return; }
  point_at(tf, gx, gy, "Point");
}
// Solve f(x) = c: every x (sin, cos, tan: in the view and a screen each side), exact when giac
// finds it; several: a list to pick from
static NOINL void solve_for() {
  float c;
  gen gc;
  if (!ask("Solve  f(x) =", "y", c, &gc)) return;
  const std::string fs = F_[tf].print(contextptr);
  const bool t = fs.find("sin") != std::string::npos || fs.find("cos") != std::string::npos || fs.find("tan") != std::string::npos;
  const float w = vx1 - vx0;
  vecteur r;
  working(true);
  analyze_zeros(F_[tf] - gc, *X_, t ? vx0 - w : 0, t ? vx1 + w : 0, false, r, contextptr);
  working(false);
  const int n = r.size() < 24 ? r.size() : 24;
  paint(false); // (the prompt gone)
  if (!n) { focus_status_msg(t ? "No x near the view" : "No x gives that y"); return; }
  int k = 0;
  char * pool = n > 1 ? (char *)malloc(n * 48) : 0;
  if (pool) { // which one: x exact, in decimals
    const char * mth[24], * lab[24];
    const float mid = mode ? tx : (vx0 + vx1) / 2;
    int sel = 0;
    for (int j = 0; j < n; ++j) {
      const float v = fl(r[j]);
      gtext(pool + 48 * j, 32, r[j], 0);
      fmtg(pool + 48 * j + 32, v, 4);
      mth[j] = pool + 48 * j; lab[j] = pool + 48 * j + 32;
      if (fabsf(v - mid) < fabsf(fl(r[sel]) - mid)) sel = j; // (opens on the one nearest the middle)
    }
    k = focus_card_list(2, "points", mth, lab, 0, n, sel);
    free(pool);
    if (k < 0) return;
  }
  point_at(tf, r[k], gc, "Solution");
}
// Max and min on [a, b]: f at a, at b and where f' is 0 or undefined between: the largest, the
// smallest (exact); the two to go to
static NOINL void extrema_on() {
  float a, b;
  gen ga, gb;
  if (!ask("Max and min from x =", "a", a, &ga) || !ask("Max and min to x =", "b", b, &gb) || a == b) return;
  if (a > b) { const float t = a; a = b; b = t; }
  vecteur r;
  working(true);
  analyze_zeros(eval(derive(F_[tf], *X_, contextptr), 1, contextptr), *X_, a, b, true, r, contextptr);
  r.push_back(ga); r.push_back(gb);
  gen bx[2], by[2];
  float bv[2] = {0, 0};
  bool any = false;
  const float m = 1e-4f * (1 + fabsf(a) + fabsf(b));
  for (unsigned j = 0; j < r.size() && !giac::ctrl_c; ++j) {
    gen y;
    float v;
    const float x = fl(r[j]);
    if (!(x >= a - m && x <= b + m) || !exact_at(tf, r[j], y, v)) continue;
    for (int q = 0; q < 2; ++q)
      if (!any || (q ? v < bv[1] : v > bv[0])) { bx[q] = r[j]; by[q] = y; bv[q] = v; }
    any = true;
  }
  working(false);
  paint(false);
  if (!any) { focus_status_msg("Not defined there"); return; }
  char ms[2][48];
  const char * mth[2] = {ms[0], ms[1]}, * lab[2] = {"Absolute max", "Absolute min"};
  for (int q = 0; q < 2; ++q) gtext(ms[q], 48, bx[q], &by[q]);
  const int k = focus_card_list(2, "points", mth, lab, 0, 2, 0);
  if (k >= 0) point_at(tf, bx[k], by[k], lab[k]);
}
static NOINL void jump_to(int k) { // the trace cursor on key point k
  mode = 1; tf = K_[k].f; tx = KIND_ ? K_[k].t : K_[k].x;
  follow();
  strcpy(note, kname(K_[k].kinds)); strcat(note, "\x01");
  strncat(note, K_[k].label, sizeof(note) - strlen(note) - 1);
  note_x = px_of(tx); note_t = tx;
}
// a parametric or polar curve at a t typed (exact: pi/3): the trace point there
static NOINL void pvalue_at() {
  float t;
  gen gt, gx, gy;
  if (!ask(KIND_ == 2 ? "Value at \xce\xb8 =" : "Value at t =", KIND_ == 2 ? "\xce\xb8" : "t", t, &gt)) return;
  pxy(tf, gx, gy);
  const gen x = normal(eval(subst(gx, *X_, gt, false, contextptr), 1, contextptr), contextptr), y = normal(eval(subst(gy, *X_, gt, false, contextptr), 1, contextptr), contextptr);
  if (!fin(fl(x)) || !fin(fl(y))) { focus_status_msg("Not defined there"); return; }
  mode = 1; tx = t;
  follow();
  strcpy(note, "Point\x01");
  gtext(note + strlen(note), sizeof(note) - strlen(note), x, &y);
  note_x = 0; note_t = tx;
}
// F3: the tools on top, then the key points (zeros, extrema, inflection points, crossings)
// F3 for a slope field: the solution through a point typed (four), or none
static NOINL void fpoints_menu() {
  static const char * const lab[2] = {"Solution through a point", "Clear the solutions"};
  const int c = focus_card_list(2, "points", 0, lab, 0, NC_ ? 2 : 1, 0);
  if (c == 1) { NC_ = 0; mode = 0; tang = false; return; }
  if (c) return;
  if (NC_ >= NGRP) { focus_status_msg("Four solutions at most"); return; }
  redraw();
  float x, y;
  if (!ask("Solution through x =", "x0", x) || !ask("Solution through y =", "y0", y)) return;
  SX0[NC_] = x; SY0[NC_] = y;
  solve_ode(NC_);
  mode = 1; tf = NC_++; tx = x; // (traced from the point)
}
static void points_menu() {
  if (KIND_ == 3) { fpoints_menu(); return; }
  if (NK < 0) { kpt_find(); paint(false); } // (the busy dots gone)
  enum { NT = 4 };
  const char * lab[NT + 40], * mth[NT + 40], * hint[NT + 40];
  static const char * const tools[NT] = {"Value at x", "Solve  f(x) = ...", "Max and min on [a, b]", "Labels on the graph"};
  static const char * const ptools[2] = {"Value at t", "Labels on the graph"}; // (a curve: its t)
  const int nt = KIND_ ? 2 : NT, n = nt + (NK > 0 ? NK : 0);
  for (int k = 0; k < n; ++k) {
    lab[k] = k < nt ? (KIND_ ? ptools[k] : tools[k]) : kname(K_[k - nt].kinds);
    mth[k] = k < nt ? 0 : K_[k - nt].label;
    hint[k] = 0;
  }
  if (KIND_ == 2) lab[0] = "Value at \xce\xb8";
  hint[nt - 1] = klabels ? "on" : "off";
  const int c = focus_card_list(2, "points", mth, lab, hint, n, 0);
  if (c >= nt) { jump_to(c - nt); return; }
  if (c < 0 || c == nt - 1) { if (c == nt - 1) klabels = !klabels; return; }
  redraw();
  if (KIND_) { pvalue_at(); return; }
  if (c == 0) value_at();
  if (c == 1) solve_for();
  if (c == 2) extrema_on();
}
static void next_point(int dir) { // 2nd left/right: the key point before/after the trace point
  if (NK < 0) { kpt_find(); paint(false); }
  int best = -1;
  for (int k = 0; k < NK; ++k) { // (a curve: by t, on it)
    if (K_[k].f != tf && (mode || KIND_)) continue;
    const float d = ((KIND_ ? K_[k].t : K_[k].x) - tx) * dir;
    if (d > (KIND_ ? (PT1[tf] - PT0[tf]) / 4000 : (vx1 - vx0) / 1000) && (best < 0 || d < ((KIND_ ? K_[best].t : K_[best].x) - tx) * dir)) best = k;
  }
  if (best >= 0) jump_to(best);
}
static void table() { // x around the trace point (the middle) by steps of 1, 2 or 5 x 10^k; up/down: more
  // (a curve: t from its start, or around the trace point, each curve's x and y; polar: θ and r)
  const bool cv = KIND_ == 1 || KIND_ == 2; // (a field's solutions: by x)
  const float st = cv ? step_of(PT1[tf] - PT0[tf], 32, 1) : step_of(vx1 - vx0, UI_W, 14);
  float x0 = cv && !mode ? ceilf(PT0[tf] / st - 1e-3f) * st : floorf((mode ? tx : (vx0 + vx1) / 2) / st + 0.5f) * st - 4 * st;
  if (cv && x0 < PT0[tf]) x0 = ceilf(PT0[tf] / st - 1e-3f) * st; // (a curve: its t from its start)
  static char cells[9 * 5][16];
  const char * c[9 * 5];
  static const char * const hf[5] = {"x", "y1", "y2", "y3", "y4"}, * const hp[5] = {"t", "x1", "y1", "x2", "y2"}, * const hr[5] = {"\xce\xb8", "r1", "r2", "r3", "r4"};
  static const char * const hp1[3] = {"t", "x", "y"}, * const hr1[2] = {"\xce\xb8", "r"}, * const hf1[2] = {"x", "y"}; // (one curve)
  const char * const * head = KIND_ == 1 ? (NC_ > 1 ? hp : hp1) : KIND_ == 2 ? (NC_ > 1 ? hr : hr1) : (KIND_ == 3 ? NC_ : NF) > 1 ? hf : hf1;
  const int n = KIND_ == 3 ? NC_ : NF, nc = n + 1 > 5 ? 5 : n + 1;
  for (;;) {
    for (int r = 0; r < 9; ++r)
      for (int k = 0; k < nc; ++k) {
        const float x = x0 + r * st, y = !k ? x : KIND_ == 3 ? sol_y(k - 1, x) : val(k - 1, x);
        char * t = cells[r * nc + k];
        if (!k) { fmt(t, x, st); trim(t); }
        else if (fin(y)) fmtg(t, y);
        else strcpy(t, "undefined");
        c[r * nc + k] = t;
      }
    focus_table(head, nc, c, 9, "up/down: more   other keys: back");
    int key;
    GetKey(&key);
    if (key == KEY_CTRL_UP) x0 -= 4 * st;
    else if (key == KEY_CTRL_DOWN) x0 += 4 * st;
    else break;
  }
}
// The slope of curve i at x: its derivative compiled (exact: x^3-3x at 1 is 0, a difference
// quotient gave 0.002), else a difference quotient
static prog * DP_;
static gen * DF_;
static NOINL float slope(int i, float x) {
  if (!DP_) { DP_ = (prog *)calloc(NMAX, sizeof(prog)); DF_ = new gen[NMAX]; }
  if (DP_ && !DP_[i].depth && !DP_[i].ok) {
    DF_[i] = ratnormal(eval(derive(F_[i], *X_, contextptr), 1, contextptr), contextptr);
    DP_[i].depth = 1; // (tried)
    DP_[i].ok = comp(DP_[i], DF_[i], 0);
  }
  if (DP_ && DP_[i].ok) return run_prog(DP_[i], DF_[i], x);
  const float h = KIND_ ? (PT1[0] - PT0[0]) / 4000 : (vx1 - vx0) / 4000;
  return (val(i, x + h) - val(i, x - h)) / (2 * h);
}
// dy/dx at the trace point (a curve: dy/dt over dx/dt; NAN: a vertical tangent)
static NOINL float tslope() {
  if (!KIND_) return slope(tf, tx);
  if (KIND_ == 3) return fxy(tx, sol_y(tf, tx)); // (the field)
  float dx, dy;
  if (KIND_ == 1) { dx = slope(2 * tf, tx); dy = slope(2 * tf + 1, tx); }
  else {
    const float r = run_prog(P_[tf], F_[tf], tx), dr = slope(tf, tx), c = cosf(tx), s = sinf(tx);
    dx = dr * c - r * s; dy = dr * s + r * c;
  }
  return fabsf(dx) <= 1e-6f * (1 + fabsf(dy)) ? NAN : dy / dx;
}
// the tangent's equation: y = m x + b, its terms when not 0
static NOINL void tangent_text(char * t, float m, float b) {
  char u[20];
  strcpy(t, "Tangent  y = ");
  const bool mz = fabsf(m) < 1e-5f, bz = fabsf(b) < 1e-5f * (1 + fabsf(m));
  if (!mz) {
    if (fabsf(m - 1) < 1e-6f) strcat(t, "x");
    else if (fabsf(m + 1) < 1e-6f) strcat(t, "-x");
    else { fmtg(u, m); strcat(t, u); strcat(t, "x"); }
  }
  if (!bz || mz) {
    fmtg(u, mz ? b : fabsf(b));
    if (!mz) strcat(t, b < 0 ? " - " : " + ");
    strcat(t, u);
  }
}
static NOINL bool add_curve(const gen & f, int g, float vx);
// what = the integral of f from a to b, into the note: exact when giac finds it (short), else in decimals
static NOINL void int_note(const char * what, const gen & f, const gen & a, const gen & b) {
  working(true);
  gen I = _integrate(makesequence(f, *X_, a, b), contextptr);
  const float v = fl(I); // (not found exactly: numerically)
  working(false);
  std::string ex = I.print(contextptr);
  strcpy(note, what);
  if (!is_undef(I) && I.type != _DOUBLE_ && ex.size() < 30 && ex.find('.') == std::string::npos && ex.find("int") == std::string::npos) {
    strcat(note, " = \x01"); strcat(note, ex.c_str()); // exact: 8/3
  }
  else if (fin(v)) {
    char t[20];
    fmtg(t, v);
    strcat(note, " = "); strcat(note, t);
  }
  else strcat(note, ": not found");
  note_x = -1;
}
// F5 for a parametric or polar curve: the tangent, the arc length from t = a to b, a polar curve's area
static NOINL void pmore_menu() {
  static const char * const lab[4] = {"Tangent line here", "Arc length", "Area (polar)", "Grid"};
  const int pol = KIND_ == 2, fld = KIND_ == 3; // (a slope field: the tangent, the grid)
  const char * l[4] = {lab[0], fld ? lab[3] : lab[1], pol ? lab[2] : lab[3], lab[3]}, * hint[4] = {tang ? "on" : "off", fld ? (grid ? "on" : "off") : 0, pol ? "1/2 r^2" : grid ? "on" : "off", grid ? "on" : "off"};
  const int c = focus_card_list(4, "more", 0, l, hint, fld ? 2 : 3 + pol, 0);
  if (c == 0) {
    tang = !tang && (!fld || NC_);
    if (tang && !mode) { mode = 1; tx = tnear(tf); }
  }
  if (fld) { if (c == 1) grid = !grid; return; }
  if (c == 2 + pol) grid = !grid;
  if (c != 1 && !(pol && c == 2)) return;
  redraw();
  float a, b;
  gen ga, gb, gx, gy;
  const char * w = pol ? "\xce\xb8" : "t";
  char t1[24], t2[24];
  strcpy(t1, c == 1 ? "Arc length from " : "Area from "); strcat(t1, w); strcat(t1, " =");
  strcpy(t2, c == 1 ? "Arc length to " : "Area to "); strcat(t2, w); strcat(t2, " =");
  if (!ask(t1, w, a, &ga) || !ask(t2, w, b, &gb) || a == b) return;
  const gen & t = *X_;
  if (c == 2) { // (1/2 r^2 dθ), the region swept shaded
    int_note("Area", F_[tf] * F_[tf] / 2, ga, gb);
    fan_a = a < b ? a : b; fan_b = a < b ? b : a;
    return;
  }
  pxy(tf, gx, gy); // the arc: sqrt(x'^2 + y'^2) dt
  const gen dx = derive(gx, t, contextptr), dy = derive(gy, t, contextptr);
  int_note("Arc length", symbolic(at_sqrt, simplify(eval(dx * dx + dy * dy, 1, contextptr), contextptr)), ga, gb);
}
static NOINL void more_menu(int & nfun) {
  static const char * const lab[] = {"Tangent line here", "Area under the curve", "Graph f' too", "Analyze f(x)", "Point coordinates", "Grid"};
  const char * hint[6] = {tang ? "on" : "off", 0, 0, "the report", coords ? "on" : "off", grid ? "on" : "off"};
  const int c = focus_card_list(4, "more", 0, lab, hint, 6, 0);
  if (c == 0) {
    tang = !tang; // (drawn at the trace point, its equation in the readout: paint)
    if (tang && !mode) { mode = 1; tx = (vx0 + vx1) / 2; }
  }
  if (c == 1) { // from a to b: shaded, its value (giac: exact when it can)
    redraw();
    float a, b;
    gen ga, gb;
    if (ask("Area from x =", "a", a, &ga) && ask("Area to x =", "b", b, &gb) && a != b) {
      if (a > b) { float t = a; a = b; b = t; gen u = ga; ga = gb; gb = u; }
      area_a = a; area_b = b;
      // the integral from a to b (signed: below the x-axis counts less), exact when giac finds it
      int_note("Integral", F_[tf], ga, gb);
    }
  }
  if (c == 2 && nfun < NMAX) { // f' as another curve
    gen d = eval(derive(F_[tf], *X_, contextptr), 1, contextptr);
    const int g = NGR < NGRP ? NGR++ : GRP[tf]; // (its own color)
    EQ_[g] = gen(identificateur("y")) - d;
    add_curve(ratnormal(d, contextptr), g, NAN);
    nfun = NF;
    lev = 0;
    kpt_clear();
  }
  if (c == 3) { // the analysis' report (its screen), then the graph again
    std::string s = F_[tf].print(contextptr);
    focus_analyze(s.c_str());
  }
  if (c == 4) coords = !coords;
  if (c == 5) grid = !grid;
}

// (DeltaCE) data points drawn with the curves (DAT_, DATN_ above): a copy; the y range is theirs
void focus_graph_points(const float * x, const float * y, int n) {
  free(DAT_); DAT_ = 0; DATN_ = 0;
  if (n <= 0 || !(DAT_ = (float *)malloc(2 * n * sizeof(float)))) return;
  memcpy(DAT_, x, n * sizeof(float)); memcpy(DAT_ + n, y, n * sizeof(float)); DATN_ = n;
  for (int i = 1; i < n; ++i) // by x (the trace steps from one to the next)
    for (int j = i; j > 0 && DAT_[j - 1] > DAT_[j]; --j) {
      float t = DAT_[j]; DAT_[j] = DAT_[j - 1]; DAT_[j - 1] = t;
      t = DAT_[n + j]; DAT_[n + j] = DAT_[n + j - 1]; DAT_[n + j - 1] = t;
    }
}
static NOINL int point_near(float x) { // the point nearest to x
  int k = 0;
  for (int i = 1; i < DATN_; ++i) if (fabsf(DAT_[i] - x) < fabsf(DAT_[k] - x)) k = i;
  return k;
}
static NOINL void fit_points() { // the view's y range: the points' (an eighth more above and below)
  float lo = 1e30f, hi = -1e30f;
  for (int k = 0; k < DATN_; ++k) { const float y = DAT_[DATN_ + k]; if (fin(y)) { if (y < lo) lo = y; if (y > hi) hi = y; } }
  if (lo > hi) return;
  if (hi - lo < 1e-6f) { lo -= 1; hi += 1; }
  const float m = (hi - lo) / 8;
  vy0 = lo - m; vy1 = hi + m;
}

// ---------------------------------------------------------------- the screen
static NOINL void run(float a, float b, bool eqs) {
  vx0 = a; vx1 = b; vy0 = -10; vy1 = 10;
  if (KIND_ == 3) { vy0 = fy0_; vy1 = fy1_; } // (a field: the ranges given, else equal units)
  else if (KIND_) pfit(); // (parametric, polar: the curves' extent, equal units)
  else if (eqs) { // equations: equal units (a circle round), the curves' extent in view
    float x0 = 1e30f, x1 = -1e30f, y0 = 1e30f, y1 = -1e30f;
    // the curves with ends (a circle: defined at neither -20 nor 20) and the vertical lines; none: all
    for (int pass = 0, i = 0; pass < 2 && x0 > x1; ++pass, i = 0)
     for (; i < NF; ++i)
      if (pass || fin(VX[i]) || (!fin(val(i, -20)) && !fin(val(i, 20))))
      for (int c = 0; c <= 80; ++c) {
        const float x = fin(VX[i]) ? VX[i] : -20 + c * 0.5f, y = fin(VX[i]) ? 0 : val(i, x);
        if (fin(y) && fabsf(y) < 1e4f) { if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y; }
      }
    if (x0 > x1) { x0 = -10; x1 = 10; y0 = y1 = 0; }
    if (x1 - x0 < 1e-4f && y1 - y0 < 1e-4f) { x0 -= 10; x1 += 10; } // (x = 3 alone: 20 wide around it)
    const float w = 1.3f * (x1 - x0 > (y1 - y0) * UI_W / GH ? x1 - x0 : (y1 - y0) * UI_W / GH) + 1e-3f, h = w * (GH - 1) / (UI_W - 1);
    vx0 = (x0 + x1) / 2 - w / 2; vx1 = vx0 + w; vy0 = (y0 + y1) / 2 - h / 2; vy1 = vy0 + h;
  }
  w0 = vx1 - vx0;
  int in = 0, all = 0; // y from -10 to 10, unless the curves are mostly outside: fitted to them
  for (int i = 0; i < NF && !eqs && !KIND_; ++i)
    for (int c = 0; c < UI_W; c += 4) {
      const float y = val(i, col_x(c));
      if (fin(y)) { ++all; if (y >= -10 && y <= 10) ++in; }
    }
  if (4 * in < all) fit_y();
  if (vy1 - vy0 > 1e4f && !KIND_) { vy0 = -10; vy1 = 10; } // (1/x: not its 1e8 near 0)
  if (DATN_ && !KIND_) fit_points();
  mode = 0; tf = 0; tx = 0; tang = false; area_a = area_b = fan_a = fan_b = 0; note[0] = 0;
  while (tf < NF - 1 && fin(VX[tf])) ++tf; // (trace: a curve y = f(x))
  focus_view = 1;
  int nfun = NF;
  bar();
  paint(false);
  focus_status_msg("arrows move, + - zoom"); // (until a key)
  for (;;) {
    int key;
    GetKey(&key);
    focus_status_msg(0);
    if (key == KEY_CTRL_EXIT || key == KEY_CTRL_AC || key == KEY_CTRL_QUIT) break;
    if (key == KEY_CTRL_LEFT || key == KEY_CTRL_RIGHT || key == KEY_CTRL_UP || key == KEY_CTRL_DOWN || key == KEY_CHAR_PLUS || key == KEY_CHAR_MINUS) {
      if (mode && (key == KEY_CTRL_UP || key == KEY_CTRL_DOWN)) { // trace: the next curve (not a vertical line; DeltaCE: the points after the curves)
        const int n = KIND_ ? NC_ : NF + (DATN_ ? 1 : 0);
        for (int k = 0; k < n; ++k) { tf = (tf + (key == KEY_CTRL_DOWN ? 1 : n - 1)) % n; if (KIND_ || tf == NF || !fin(VX[tf])) break; }
        if (!KIND_ && tf == NF) { tpi = point_near(tx); tx = DAT_[tpi]; }
        if (KIND_) tx = tx < PT0[tf] ? PT0[tf] : tx > PT1[tf] ? PT1[tf] : tx;
        follow(); paint(false);
        continue;
      }
      if (mode && !KIND_ && tf == NF) { // (DeltaCE) trace on the points: the next one
        if (key == KEY_CTRL_LEFT && tpi > 0) --tpi;
        if (key == KEY_CTRL_RIGHT && tpi < DATN_ - 1) ++tpi;
        if (key == KEY_CTRL_LEFT || key == KEY_CTRL_RIGHT) { tx = DAT_[tpi]; follow(); paint(false); continue; }
      }
      hold();
      continue;
    }
    if (key == KEY_SHIFT_LEFT || key == KEY_SHIFT_RIGHT || key == KEY_CTRL_PAGEUP || key == KEY_CTRL_PAGEDOWN) {
      next_point(key == KEY_SHIFT_RIGHT || key == KEY_CTRL_PAGEDOWN ? 1 : -1);
      bar(); paint(false);
      continue;
    }
    if (key == KEY_CTRL_F1) { // pan <-> trace
      if (!mode && !DATN_ && (!NF || fin(VX[tf]))) { focus_status_msg("Trace follows curves y = f(x)"); continue; }
      if (!mode && KIND_ == 3 && !NC_) { focus_status_msg("Points: a solution through a point first"); continue; }
      mode = !mode;
      if (mode) { tx = KIND_ ? tnear(tf) : (vx0 + vx1) / 2; if (!KIND_ && tf == NF) { tpi = point_near(tx); tx = DAT_[tpi]; } follow(); }
      bar(); paint(false);
      if (mode) focus_status_msg("2nd left right: key points");
      continue;
    }
    if (!KIND_ && (!NF || (mode && tf == NF)) && (key == KEY_CTRL_EXE || key == KEY_CTRL_F3 || key == KEY_CTRL_F5 || (!NF && key == KEY_CTRL_F4))) {
      focus_status_msg(NF ? "Up or down: the curve" : "Fit a curve first"); // (DeltaCE: the points alone; Statistics fits one)
      continue;
    }
    if (key == KEY_CTRL_EXE && mode) { // trace: an x to go to, its point exact
      redraw();
      if (KIND_ == 3) { float x; if (ask("Value at x =", "x", x) && fin(sol_y(tf, x))) { tx = x; follow(); } }
      else if (KIND_) pvalue_at(); else value_at();
      bar(); paint(false);
      continue;
    }
    if (key == KEY_CTRL_F2) zoom_menu();
    else if (key == KEY_CTRL_F3) points_menu();
    else if (key == KEY_CTRL_F4) table();
    else if (key == KEY_CTRL_F5) { if (KIND_) pmore_menu(); else more_menu(nfun); }
    else continue;
    bar(); paint(false);
  }
  focus_view = 0;
  focus_status_label(0);
  focus_invalidate();
}

// one more curve y = f in equation g's color (false: no room)
static NOINL bool add_curve(const gen & f, int g, float vx) {
  if (NF >= NMAX) return false;
  F_[NF] = f; GRP[NF] = (unsigned char)g; VX[NF] = vx;
  memset(&P_[NF], 0, sizeof(prog));
  if (!fin(vx)) P_[NF].ok = comp(P_[NF], f, 0);
  ++NF;
  return true;
}
// equation L = R (the g-th typed): y = f(x) as is; without y, its vertical lines (x = 3); a
// polynomial of degree 1 or 2 in y, solved for it (a curve each: y = 3+sqrt(...), y = 3-sqrt(...));
// else drawn as F(x, y) = 0 (marching squares). false: not drawn (no room, other letters)
static NOINL bool add_eq(const gen & L, const gen & R, int g) {
  const gen & y = *Yv_, D = eval(L - R, 1, contextptr);
  EQ_[g] = D;
  if (L == y && !depend(R, *y._IDNTptr)) return add_curve(R, g, NAN);
  if (R == y && !depend(L, *y._IDNTptr)) return add_curve(L, g, NAN);
  if (!depend(D, *y._IDNTptr)) { // x = 3, x^2 = 4: vertical lines
    const gen r = _solve(makesequence(symbolic(at_equal, makesequence(D, 0)), *X_), contextptr);
    if (r.type != _VECT) return false;
    for (unsigned k = 0; k < r._VECTptr->size(); ++k) { const float c = fl(r[k]); if (fin(c) && !add_curve(r[k], g, c)) return false; }
    return true;
  }
  const vecteur ly = lvarx(D, y);
  const gen N = _numer(D, contextptr), cf = _coeff(makesequence(N, y), contextptr); // (a y^2 + b y + c: [a, b, c])
  if (ly.size() == 1 && ly[0] == y && cf.type == _VECT && (cf._VECTptr->size() == 2 || cf._VECTptr->size() == 3)) {
    // degree 1 or 2 in y: solved by the formula (giac's solve took 3 s on the calculator for a circle)
    if (cf._VECTptr->size() == 2) return add_curve(normal(-cf[1] / cf[0], contextptr), g, NAN);
    const gen a = cf[0], b = cf[1], c = cf[2], d = normal(b * b - 4 * a * c, contextptr);
    if (is_zero(d)) return add_curve(normal(-b / (2 * a), contextptr), g, NAN);
    const gen r = normal(symbolic(at_sqrt, d), contextptr); // (2*sqrt(25-x^2))
    return NF + 2 <= NMAX && add_curve(normal((-b + r) / (2 * a), contextptr), g, NAN) && add_curve(normal((-b - r) / (2 * a), contextptr), g, NAN);
  }
  if (NI >= NGRP) return false;
  IM_[NI] = D; IG_[NI] = (unsigned char)g;
  memset(&IP_[NI], 0, sizeof(prog));
  IP_[NI].ok = comp(IP_[NI], D, 0);
  ++NI;
  return true;
}
// in: name(...) (balanced: not plot(x)+plot(y)): its arguments parsed (a sequence for several)
static NOINL bool call_args(const char * in, const char * name, gen & a) {
  const int n = strlen(name);
  if (strncmp(in, name, n) || in[n] != '(') return false;
  const char * e = in + strlen(in);
  while (e > in + n && e[-1] == ' ') --e;
  if (e[-1] != ')') return false;
  int d = 0;
  for (const char * p = in + n; p < e - 1; ++p) if ((d += *p == '(' ? 1 : *p == ')' ? -1 : 0) <= 0) return false;
  const int len = e - in - n - 2;
  char * b = (char *)malloc(len + 1);
  if (!b) return false;
  int m = 0, st = 0; // (the template's empty arguments left out: plotpolar(r,,,): their defaults)
  d = 0;
  for (const char * p = in + n + 1, * q = p + len; p <= q; ++p) {
    const char c = p < q ? *p : ',';
    d += c == '(' || c == '[' || c == '{' ? 1 : c == ')' || c == ']' || c == '}' ? -1 : 0;
    if (c != ',' || d) { b[m++] = c; continue; }
    int k = st;
    while (k < m && b[k] == ' ') ++k;
    if (k == m) m = st; // (blank)
    else if (p < q) b[m++] = ',';
    st = m;
  }
  if (m && b[m - 1] == ',') --m;
  b[m] = 0;
  a = gen(b, contextptr);
  free(b);
  return true;
}
// a plot's arguments: what is drawn (e), its letter (v: 0 if none given), its range (lo, hi, if
// given): plot(e, x), plot(e, x=a..b), plot(e, x, a, b), plot(e, a..b); options (xstep=1) skipped
static NOINL void plot_args(const gen & a, gen & e, gen & v, float & lo, float & hi) {
  const vecteur w = a.type == _VECT && a.subtype == _SEQ__VECT ? *a._VECTptr : vecteur(1, a);
  e = w.empty() ? gen(0) : w[0];
  v = 0;
  for (unsigned i = 1; i < w.size(); ++i) {
    gen u = w[i];
    if (u.is_symb_of_sommet(at_equal) && u._SYMBptr->feuille[0].type == _IDNT) {
      if (!u._SYMBptr->feuille[1].is_symb_of_sommet(at_interval)) continue;
      v = u._SYMBptr->feuille[0]; u = u._SYMBptr->feuille[1];
    }
    if (u.type == _IDNT && is_zero(v)) { v = u; continue; }
    if (u.is_symb_of_sommet(at_interval)) { lo = fl(u._SYMBptr->feuille[0]); hi = fl(u._SYMBptr->feuille[1]); continue; }
    const float f = fl(u), g = i + 1 < w.size() ? fl(w[i + 1]) : NAN;
    if (fin(f) && fin(g)) { lo = f; hi = g; ++i; }
  }
}
// the one letter of the expressions in v (not a constant, not y in equations): into x if x is 0;
// false: another letter
static NOINL bool one_letter(const vecteur & v, gen & x, const gen * y) {
  for (unsigned i = 0; i < v.size(); ++i) {
    vecteur ids = lidnt(v[i]);
    for (unsigned j = 0; j < ids.size(); ++j) {
      if (evalf(ids[j], 1, contextptr).type == _DOUBLE_ || (y && ids[j] == *y)) continue;
      if (is_zero(x)) x = ids[j];
      else if (ids[j] != x) return false;
    }
  }
  return true;
}
// plotparam([x(t), y(t)], t, a, b) (or x(t)+i*y(t), or a list of [x, y]: three curves at most),
// plotpolar(r(θ), θ, a, b) (a list: four): the curves, t from 0 to 2 pi unless given
static NOINL bool curves_of(const gen & a, gen & t, float & lo, float & hi) {
  gen e;
  plot_args(a, e, t, lo, hi);
  e = eval(e, 1, contextptr);
  vecteur v;
  if (KIND_ == 2) v = e.type == _VECT ? *e._VECTptr : vecteur(1, e);
  else if (e.type == _VECT && e._VECTptr->size() == 2 && e[0].type != _VECT) v.push_back(e);
  else if (e.type == _VECT) v = *e._VECTptr;
  else v.push_back(makevecteur(re(e, contextptr), im(e, contextptr))); // (x + i y)
  if (v.empty() || (int)v.size() > (KIND_ == 1 ? NMAX / 2 : NGRP)) return false;
  if (!one_letter(v, t, 0)) return false;
  if (is_zero(t)) t = gen(identificateur(KIND_ == 2 ? "theta" : "t")); // (r = 2: a circle)
  if (!(lo < hi)) { lo = 0; hi = 6.2831853f; }
  *X_ = t;
  for (unsigned c = 0; c < v.size(); ++c, ++NC_) {
    PT0[c] = lo; PT1[c] = hi;
    if (KIND_ == 2) { if (!add_curve(v[c], c, NAN)) return false; continue; }
    if (v[c].type != _VECT || v[c]._VECTptr->size() != 2) return false;
    if (!add_curve(v[c][0], c, NAN) || !add_curve(v[c][1], c, NAN)) return false;
  }
  PS_ = (float *)malloc(NC_ * 2 * PSMAX * sizeof(float));
  PM_ = (int *)malloc(NC_ * 2 * PSMAX * sizeof(int));
  return PS_ && PM_;
}
// plotfield(f(x, y), [x=a..b, y=c..d]) and plotode(f, [x=a..b, y], [x0, y0]) (or plotode=[x0, y0]):
// y' = f, the solution through (x0, y0); x is the letter that is not y (t), x from -6 to 6 and y
// in equal units unless given
static NOINL bool field_of(const gen & a, float & lo, float & hi) {
  const vecteur w = a.type == _VECT && a.subtype == _SEQ__VECT ? *a._VECTptr : vecteur(1, a);
  if (w.empty()) return false;
  gen xv = 0, yv = 0;
  float y0 = 0, y1 = 0;
  for (unsigned i = 1; i < w.size(); ++i) {
    gen u = w[i];
    if (u.is_symb_of_sommet(at_equal)) u = u._SYMBptr->feuille[1]; // (plotode=[0,1])
    if (u.type != _VECT) continue;
    const vecteur & l = *u._VECTptr;
    if (l.size() == 2 && l[0].type != _IDNT && fin(fl(l[0])) && fin(fl(l[1]))) { // a point
      if (NC_ < NGRP) { SX0[NC_] = fl(l[0]); SY0[NC_] = fl(l[1]); ++NC_; }
      continue;
    }
    for (unsigned j = 0; j < l.size(); ++j) { // the letters, their ranges
      gen v = l[j];
      float p = 0, q = 0;
      if (v.is_symb_of_sommet(at_equal) && v._SYMBptr->feuille[1].is_symb_of_sommet(at_interval)) {
        const gen & r = v._SYMBptr->feuille[1]._SYMBptr->feuille;
        p = fl(r[0]); q = fl(r[1]); v = v._SYMBptr->feuille[0];
      }
      if (v.type != _IDNT) continue;
      if (is_zero(xv)) { xv = v; if (p < q) { lo = p; hi = q; } }
      else if (is_zero(yv)) { yv = v; y0 = p; y1 = q; }
    }
  }
  if (is_zero(yv)) yv = gen(identificateur("y"));
  *Yv_ = yv;
  const gen f = eval(w[0], 1, contextptr);
  if (!one_letter(vecteur(1, f), xv, Yv_)) return false;
  if (is_zero(xv)) xv = gen(identificateur("x"));
  *X_ = xv;
  if (!(lo < hi)) { lo = -6; hi = 6; }
  if (!(y0 < y1)) { const float h = (hi - lo) * (GH - 1) / (UI_W - 1); y0 = -h / 2; y1 = h / 2; }
  fy0_ = y0; fy1_ = y1;
  if (!add_curve(f, 0, NAN)) return false;
  PS_ = (float *)malloc(NGRP * PSS_ * sizeof(float));
  PM_ = (int *)malloc(NGRP * PSS_ * sizeof(int));
  FS_ = (int *)malloc(4 * NFS * sizeof(int));
  return PS_ && PM_ && FS_;
}
// The functions of plot(f), plot([f,g]), plot(f,x=a..b) typed (or the Graph actions' input), and
// equations in x and y (plot(x^2+y^2=25), plot([x^2+y^2=25, y=x+1])); parametric and polar
// curves (plotparam, plotpolar); slope fields (plotfield, plotode): shown here. 0: not a plot,
// -1: one it cannot draw (other letters, too many curves, no memory)
int focus_graph_input(const char * in) {
  gen a;
  const int kind = call_args(in, "plot", a) || call_args(in, "plotfunc", a) ? 0 : call_args(in, "plotparam", a) ? 1 : call_args(in, "plotpolar", a) ? 2 :
    call_args(in, "plotfield", a) || call_args(in, "plotode", a) ? 3 : -1;
  if (kind < 0) return 0;
  static gen * xs, * ys;
  if (!xs) { xs = new gen; ys = new gen(identificateur("y")); }
  KIND_ = kind; NC_ = 0; psw = 0; nfs_ = 0; PSS_ = kind == 3 ? 2 * SOLMAX : 2 * PSMAX;
  X_ = xs; Yv_ = 0;
  NF = NI = NGR = 0;
  F_ = new gen[NMAX]; EQ_ = new gen[NGRP]; IM_ = new gen[NGRP];
  P_ = (prog *)calloc(NMAX, sizeof(prog)); IP_ = (prog *)calloc(NGRP, sizeof(prog));
  FR = (frame *)calloc(1, sizeof(frame)); // (zeroed: a curve's domain ends (ne) are only found for functions; the last graph's were drawn again)
  bool ok = P_ && IP_ && FR, eqs = false;
  float lo = 0, hi = 0;
  if (ok && kind == 3) { Yv_ = ys; ok = field_of(a, lo, hi); }
  else if (ok && kind) { gen t; ok = curves_of(a, t, lo, hi); }
  else if (ok) {
    gen fs, x;
    lo = -10; hi = 10;
    plot_args(a, fs, x, lo, hi);
    vecteur v = fs.type == _VECT ? *fs._VECTptr : vecteur(1, fs);
    ok = (!v.empty() || DATN_) && v.size() <= NGRP; // (DeltaCE: plot([]) with data points: the points alone)
    for (unsigned i = 0; i < v.size() && ok; ++i) { // a function of one letter (the same), or an equation in x and y
      v[i] = eval(v[i], 1, contextptr);
      eqs = eqs || v[i].is_symb_of_sommet(at_equal);
      ok = v[i].type != _VECT && v[i].type != _STRNG;
    }
    ok = ok && one_letter(v, x, eqs ? ys : 0);
    if (is_zero(x)) x = gen(identificateur("x"));
    ok = ok && !(eqs && x == *ys);
    *xs = x; Yv_ = eqs ? ys : 0;
    if (ok && eqs) working(true); // (solving for y)
    for (unsigned i = 0; i < v.size() && ok; ++i, ++NGR) {
      if (v[i].is_symb_of_sommet(at_equal)) ok = add_eq(v[i]._SYMBptr->feuille[0], v[i]._SYMBptr->feuille[1], i);
      else { EQ_[i] = *ys - v[i]; ok = add_curve(v[i], i, NAN); }
    }
    if (eqs) working(false);
    if (!NF && !NI && !DATN_) ok = false; // (x^2 = -1: nothing to draw)
  }
  if (ok) {
    NK = -1; lev = 0;
    run(lo < hi ? lo : -10, lo < hi ? hi : 10, eqs);
  }
  kpt_clear();
  free(K_); K_ = 0;
  delete[] F_; F_ = 0; delete[] EQ_; EQ_ = 0; delete[] IM_; IM_ = 0;
  free(P_); P_ = 0; free(IP_); IP_ = 0;
  free(FR); FR = 0;
  free(DP_); DP_ = 0;
  delete[] DF_; DF_ = 0;
  free(PS_); PS_ = 0; free(PM_); PM_ = 0; free(FS_); FS_ = 0;
  if (Yv_) *ys = gen(identificateur("y")); // (a field's other letter)
  Yv_ = 0; KIND_ = 0;
  giac::ctrl_c = giac::interrupted = false; // (DeltaCE: clear, the key that left, read as a stop: what evaluates next would fail)
  return ok ? 1 : -1;
}
#endif
