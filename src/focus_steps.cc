// focus_steps.cc - the steps screen (the steps build, FLOWCE_STEPS): a calculation's steps
// (steps.cc) as a worked example, down the screen with room around each step: the problem, then
// for each step its number, its name and the rule it uses (gray) on the left, its working
// (u = x   dv = sin(x) dx) and what it gives in the middle; a line going on with an equation has
// its = under the = above. A part worked out inside a step (an integral within) is a step of its
// own, then "Back to step n" goes on with the step it was for. The answer last, in a box. The
// step being read is highlighted: up/down (left/right) go to the step before/after. EXIT, CLEAR,
// EXE: back. Enter after a derivative or an integral opens it (console.cc). The PC tools draw it
// too (tools/hostgiac -V).
#if defined FLOWCE_STEPS || defined HOSTGIAC
#include <string.h>
#include <stdlib.h>
#include <string>
#include <giac/giacPCH.h> // first: its "#define std ustl", as in main.cc
#include "k_csdk.h"       // GetKey, the key codes
#include "focus.h"
#include "ui_gfx.h"
#include "ui_font.h"
#include "ui_fontdata.h"
#include "ui_math.h"
extern "C" volatile unsigned char focus_phase; // focus.cc: timing probe (tools/emu/phases.py: 130 the steps, 131 their layout, 132 laid out)
#include "steps.h"
#include "answer.h"
#include "analyze.h"

extern giac::context * contextptr;
namespace giac { extern bool (*control_c_hook)(); }
extern "C" { extern volatile char oom_hit; } // allocator_custom.c: memory ran out
void oom_rearm(); // main.cc: the memory reserve, taken again
#ifdef ANDEBUG // (a test build: #define ANDEBUG at the top of this file)
#include <stdio.h>
#define ADBG(...) sprintf((char *)0xFB0000, __VA_ARGS__)
#endif
#ifndef HOSTGIAC
extern int dconsole_mode; // console.cc: 0, giac's notes ("Unable to isolate x...") go nowhere
static void quiet(int on) { dconsole_mode = !on; }
#else
static void quiet(int) {}
#endif

static bool starts(const char * s, const char * p) { return !strncmp(s, p, strlen(p)); }
// a calculation the steps screen can show: a derivative, an integral, a limit, a sum, a Taylor
// polynomial, also inside another command (subst(diff(f,x,3),x=0), solve(diff(f,x)=0,x): the
// steps take the calculus part first, then the rest); an application (area, volume...)
// (DeltaCE) solve( and factor( typed as such: steps for a polynomial in one letter only (steps.cc's
// equations and factoring), told from the text: the chip and Enter promised steps for every solve,
// and "No steps" came back for inequalities, systems, |x|, roots, logs (the user, in class)
static int alg_able(const char * s) {
  // (solve: square roots and x in a denominator too, the radical and rational equations; one root a side)
  const bool eq = starts(s, "solve(");
  static const char * const no[] = {"<", ">", "[", "abs(", "surd(", "log", "ln(", "exp(", "sin(", "cos(", "tan(", "^(", "e^", "!", "%", "|", "sqrt(", "/("};
  for (unsigned k = 0; k < sizeof(no) / sizeof(no[0]) - (eq ? 2 : 0); ++k) if (strstr(s, no[k])) return 0;
  if (eq) { int n = 0; for (const char * p = s; (p = strstr(p, "sqrt(")); ++p) ++n; if (n > 2) return 0; }
  const char * v = 0;
  int vn = 0;
  for (const char * p = s; *p;) { // one name besides the calls (3x+2=11 and its x: x)
    if (!isalpha((unsigned char)*p) && *p != '_') { ++p; continue; }
    const char * q = p;
    while (isalnum((unsigned char)*q) || *q == '_') ++q;
    if (*q != '(') {
      if (!v) { v = p; vn = q - p; }
      else if (q - p != vn || strncmp(v, p, vn)) return 0;
    }
    p = q;
  }
  for (const char * p = s; (p = strpbrk(p, "/^")); ++p) // (factor(1/x): x in a denominator; 2^x: in an exponent)
    if (isalpha((unsigned char)p[1]) && (*p == '^' || !eq)) return 0;
  if (v && starts(s, "solve(") && !strncmp(s + 6, v, vn) && s[6 + vn] == '=') { // x=5: already solved
    const char * r = s + 7 + vn, * e = strrchr(r, ','); // (its right side: up to ",x)")
    bool again = false;
    for (const char * p = r; *p && (!e || p < e); ++p)
      if (!strncmp(p, v, vn) && !isalnum((unsigned char)p[vn]) && (p == r || (!isalpha((unsigned char)p[-1]) && p[-1] != '_'))) again = true; // (2b: b)
    if (!again) return 0;
  }
  return v != 0;
}
// (DeltaCE) one name besides the calls, no function of it (canonical_form(x^2+6x+5), propfrac((x^3+1)/(x-2)))
static int one_name(const char * s) {
  static const char * const no[] = {"sin(", "cos(", "tan(", "log", "ln(", "exp(", "e^", "abs(", "sqrt(", "^("};
  for (unsigned k = 0; k < sizeof(no) / sizeof(no[0]); ++k) if (strstr(s, no[k])) return 0;
  const char * v = 0;
  int vn = 0;
  for (const char * p = s; *p;) {
    if (!isalpha((unsigned char)*p) && *p != '_') { ++p; continue; }
    const char * q = p;
    while (isalnum((unsigned char)*q) || *q == '_') ++q;
    if (*q != '(') {
      if (!v) { v = p; vn = q - p; }
      else if (q - p != vn || strncmp(v, p, vn)) return 0;
    }
    p = q;
  }
  return v != 0;
}
// (DeltaCE) the steps' systems, exponential, log and trig equations, told from the text too. v: the
// one name besides the calls, pi and e (e^x); its every place inside the argument of a call of fs
// (log(x-3), sin(2x)) or in an exponent (2^(x+1), e^x): x+e^x and x*sin(x) are not theirs
static bool has(const char * s, const char * const * w, unsigned n) {
  for (unsigned k = 0; k < n; ++k) if (strstr(s, w[k])) return true;
  return false;
}
static int inside(const char * s, const char * const * fs, unsigned nf, bool expo) {
  const char * v = 0;
  int vn = 0, allow = 0, d = 0;
  unsigned char st[24];
  const char * end = s + strlen(s); // (solve(...,x): up to its x)
  for (const char * p = s; *p; ++p) {
    if (*p == '(') ++d; else if (*p == ')') --d; else if (*p == ',' && d == 1) end = p;
  }
  d = 0;
  for (const char * p = s; p < end; ++p) {
    if (*p == '(') {
      if (d == 24) return 0;
      bool a = expo && p > s && p[-1] == '^';
      for (unsigned k = 0; k < nf && !a; ++k) { const int n = strlen(fs[k]); a = p - s >= n && !strncmp(p - n, fs[k], n) && (p - n == s || !(isalpha((unsigned char)p[-n - 1]) || p[-n - 1] == '_')); } // (2sin(x))
      st[d++] = a; allow += a;
      continue;
    }
    if (*p == ')') { if (d) allow -= st[--d]; continue; }
    if (!isalpha((unsigned char)*p) || (p > s && (isalpha((unsigned char)p[-1]) || p[-1] == '_'))) continue; // (2x: x)
    const char * q = p;
    while (isalnum((unsigned char)*q) || *q == '_') ++q;
    const int n = q - p;
    if (*q != '(' && !(n == 2 && !strncmp(p, "pi", 2)) && !(n == 1 && *p == 'e' && *q == '^')) {
      if (!v) { v = p; vn = n; }
      else if (n != vn || strncmp(v, p, n)) return 0; // (two names)
      if (!allow && !(expo && p > s && p[-1] == '^')) return 0; // (x outside: x+e^x)
    }
    p = q - 1;
  }
  return v != 0;
}
// -1: none of these (alg_able's then)
static int more_able(const char * s) {
  static const char * const bad[] = {"<", ">", "!", "%", "|", "abs(", "sqrt(", "surd(", "asin(", "acos(", "atan(", "sec(", "csc(", "cot("};
  static const char * const trig[] = {"sin(", "cos(", "tan("};
  static const char * const logs[] = {"log", "ln(", "exp(", "e^"};
  const unsigned nb = sizeof(bad) / sizeof(bad[0]);
  if (starts(s, "solve([")) { // a linear system: no powers, no calls, no x in a denominator, whole numbers and fractions
    if (strchr(s, '^') || strchr(s, '.') || has(s, bad, nb)) return 0;
    for (const char * p = s + 6; (p = strchr(p, '(')); ++p) if (isalpha((unsigned char)p[-1])) return 0; // (2(x+1): yes; a call: no)
    for (const char * p = s; (p = strchr(p, '/')); ++p) if (isalpha((unsigned char)p[1])) return 0;
    return 1;
  }
  // (sqrt(3) tan(x) = 1: a number; x in sqrt( or abs(: outside the calls, no)
  if (has(s, trig, 3)) {
    if (has(s, bad, 5) || has(s, bad + 7, nb - 7) || has(s, logs, 4) || strstr(s, "^(") || strchr(s, '[')) return 0;
    if (!giac::angle_radian(contextptr)) return 0; // (the steps' angles: radians)
    static const char * const tn[] = {"sin", "cos", "tan"};
    return inside(s, tn, 3, false);
  }
  bool e = has(s, logs, 4);
  for (const char * p = s; !e && (p = strchr(p, '^')); ++p) e = isalpha((unsigned char)p[1]) || p[1] == '(';
  if (!e) return -1;
  static const char * const fs[] = {"log", "log10", "logb", "ln", "exp"};
  if (has(s, bad, 5) || has(s, bad + 7, nb - 7) || strchr(s, '[')) return 0;
  return inside(s, fs, 5, true) ? 1 : -1;
}
// (DeltaCE) a division of polynomials typed, (x^3-8)/(x-2): its methods (steps.cc div_of: long
// division, synthetic division, factor and cancel). Each side's degree in the one name as written:
// sums, products (2x(x+1) too), whole powers, parentheses; -1 if not a polynomial ((x+1)/2, x^n)
static int tdeg(const char *& p, const char * e);
static int tprim(const char *& p, const char * e) {
  int d;
  if (p < e && *p == '(') { ++p; d = tdeg(p, e); if (d < 0 || p >= e || *p != ')') return -1; ++p; }
  else if (p < e && isalpha((unsigned char)*p)) { while (p < e && (isalnum((unsigned char)*p) || *p == '_')) ++p; d = 1; }
  else if (p < e && isdigit((unsigned char)*p)) { while (p < e && isdigit((unsigned char)*p)) ++p; d = 0; }
  else return -1;
  if (p < e && *p == '^') {
    if (++p >= e || !isdigit((unsigned char)*p)) return -1;
    int k = 0;
    while (p < e && isdigit((unsigned char)*p) && k < 1000) k = 10 * k + *p++ - '0';
    d *= k;
  }
  return d;
}
static int tdeg(const char *& p, const char * e) {
  int d = 0;
  if (p < e && (*p == '-' || *p == '+')) ++p;
  for (;;) {
    int t = 0;
    for (bool first = true;; first = false) { // a term: its factors' degrees added
      if (!first && p < e && *p == '*') ++p;
      else if (!first && !(p < e && (*p == '(' || isalnum((unsigned char)*p)))) break;
      const int f = tprim(p, e);
      if (f < 0) return -1;
      t += f;
    }
    if (t > d) d = t;
    if (p < e && (*p == '+' || *p == '-')) ++p; else break;
  }
  return d;
}
static int text_deg(const char * a, const char * b) { const char * p = a; const int d = tdeg(p, b); return p == b ? d : -1; }
// the one top-level / of s[0,n), 0 if none or several
static const char * top_slash(const char * s, const char * e) {
  const char * sl = 0;
  int d = 0;
  for (const char * p = s; p < e; ++p) {
    if (*p == '(') ++d;
    else if (*p == ')') --d;
    else if (*p == '/' && !d) { if (sl) return 0; sl = p; }
  }
  return sl;
}
// the top's degree not lower than the bottom's (long division), or a factor cancelled: the answer
// ans has a bottom of lower degree ((x+2)/(x^2-4) is 1/(x-2))
static int div_able(const char * s, const char * ans) {
  if (!one_name(s)) return 0;
  for (const char * p = s; *p; ++p)
    if (strchr("=,[<>!|.%", *p) || (*p == '(' && p > s && (isalnum((unsigned char)p[-1]) || p[-1] == '_'))) return 0; // (no calls, no decimals)
  const char * e = s + strlen(s), * sl = top_slash(s, e);
  if (!sl || sl == s || sl + 1 == e) return 0;
  const int dn = text_deg(s, sl), dd = text_deg(sl + 1, e);
  if (dn < 0 || dd < 1) return 0;
  if (dn >= dd) return 1;
  if (!ans) return 0;
  const char * ae = ans + strlen(ans), * as = top_slash(ans, ae);
  const int ad = !strchr(ans, '/') ? text_deg(ans, ae) >= 0 ? 0 : -1 : as ? text_deg(as + 1, ae) : -1;
  return ad >= 0 && ad < dd;
}
// (DeltaCE) inequalities and absolute values (steps.cc ineq_steps, abs_steps): one name, no
// function but abs; -1: neither (another check's)
static int ineq_able(const char * s) {
  if (!strchr(s, '<') && !strchr(s, '>') && !strstr(s, "abs(")) return -1;
  static const char * const no[] = {"[", "log", "ln(", "exp(", "sin(", "cos(", "tan(", "^(", "e^", "!", "%", "sqrt(", "surd(", "=>", ":="};
  for (unsigned k = 0; k < sizeof(no) / sizeof(no[0]); ++k) if (strstr(s, no[k])) return 0;
  if (const char * a = strstr(s, "abs(")) if (strstr(a + 4, "abs(") || strchr(s, '^')) return 0; // (one absolute value, linear)
  const char * v = 0; // one name besides solve( and abs(
  int vn = 0;
  for (const char * p = s; *p;) {
    if (!isalpha((unsigned char)*p)) { ++p; continue; }
    const char * q = p;
    while (isalnum((unsigned char)*q) || *q == '_') ++q;
    if (*q == '(') { if (!(q - p == 5 && !strncmp(p, "solve", 5)) && !(q - p == 3 && !strncmp(p, "abs", 3))) return 0; }
    else if (!v) { v = p; vn = q - p; }
    else if (q - p != vn || strncmp(v, p, vn)) return 0;
    p = q;
  }
  return v != 0;
}
// (DeltaCE) an equation or an inequality typed alone (the console solves it), = < > at the top
static bool relation(const char * s) {
  int d = 0;
  for (const char * p = s; *p; ++p) {
    if (*p == '(' || *p == '[') ++d;
    else if (*p == ')' || *p == ']') --d;
    else if (!d && (*p == '<' || *p == '>' || (*p == '=' && p > s && !strchr(":=!<>", p[-1]) && p[1] != '=' && p[1] != '>'))) return true;
  }
  return false;
}
int focus_steps_able(const char * s, const char * ans) {
  if (strstr(s, "desolve(")) return 0; // (its diff(y,x) is the equation, not a derivative to work out)
  if (starts(s, "implicitdiff(")) return 1;
  if (!starts(s, "solve(") && strlen(s) < 200 && relation(s)) { // as solve(...)
    char b[210];
    strcpy(b, "solve("); strcat(b, s); strcat(b, ")");
    return focus_steps_able(b, ans);
  }
  if (starts(s, "solve(")) { int k = ineq_able(s); if (k >= 0) return k; k = more_able(s); if (k >= 0) return k; }
  if (starts(s, "canonical_form(") || starts(s, "propfrac(") || starts(s, "partfrac(")) return one_name(s);
  static const char * const ops[] = {"diff(", "integrate(", "int(", "derive(", "limit(", "series(", "taylor(", "sum(",
                                     "area(", "volume(", "arclength(", "surfacearea(", "average("};
  for (const char * p = s; *p; ++p)
    if (p == s || !isalnum((unsigned char)p[-1]))
      for (unsigned k = 0; k < sizeof(ops) / sizeof(ops[0]); ++k)
        if (starts(p, ops[k])) return 1;
  if ((starts(s, "solve(") || (starts(s, "factor(") && !strchr(s, '.'))) && alg_able(s)) return 1; // (factor(0.5x^2-2): decimals)
  return div_able(s, ans);
}

// The page: items laid out once (page coordinates: y from the top of the page), drawn in bands.
// Labels, names and rules at the margin X; the math in groups (an equation and the lines going
// on with it, the working of a step), each centered as a block.
enum { Y0 = 18, X = 14, XR = 306, W = XR - X, LVMIN = 6, LVWRAP = 5 };
enum { K_HEAD, K_LINE, K_LABEL, K_TITLE, K_FORM, K_WORK, K_EQ, K_BOX, K_ANS, K_TEXT, K_MATH, K_TABLE, K_CHIP };
// (K_TEXT: a text of the analysis, num 0 gray (a label, " or "), 1 ink; K_MATH: its math beside a label;
// K_TABLE: a synthetic division's table, steps_table's text; K_CHIP: the method, F4 the next one)
struct item {
  char * s;      // math or label text (malloc), a title or formula (static), or 0
  int x, y, w, h, base, eqx; // box; base: the baseline below y; eqx: an equation's = after x, -1
  int kind, lv, num, grp, sec; // num: K_EQ 1, a line going on (= first); K_WORK 2, the second
};                             // item of its line; K_LABEL 1, "back to"; K_ANS: + C after it
static item * P;
static int NP, CAPN, PH, GRP, SEC; // items, capacity, page height, the group, the section

static item * put(int kind, char * s, int x, int y, int w, int h, int base, int lv) {
  if (NP == CAPN) {
    item * q = (item *)realloc(P, (CAPN + 16) * sizeof(item));
    if (!q) { if (kind != K_TITLE && kind != K_FORM) free(s); return 0; }
    P = q; CAPN += 16;
  }
  item & it = P[NP++];
  it.s = s; it.x = x; it.y = y; it.w = w; it.h = h; it.base = base; it.eqx = -1;
  it.kind = kind; it.lv = lv; it.num = 0; it.sec = SEC;
  it.grp = kind == K_HEAD || kind == K_WORK || kind == K_EQ || kind == K_ANS ? GRP : 0;
  return &it;
}
static void clear_page() {
  for (item * p = P; p < P + NP; ++p) if (p->kind != K_TITLE && p->kind != K_FORM) free(p->s);
  free(P); P = 0; NP = CAPN = PH = 0;
}
static char * dup(const char * s, int n) {
  char * d = (char *)malloc(n + 1);
  if (d) { memcpy(d, s, n); d[n] = 0; }
  return d;
}
static char * digits(char * q, int m) { // m written at q: the end
  char t[4];
  int n = 0;
  do { t[n++] = '0' + m % 10; m /= 10; } while (m && n < 3);
  while (n) *q++ = t[--n];
  *q = 0;
  return q;
}

// the index of the first top-level c (c '=': not in <= >= != ==) of s, -1 if none
static int top(const char * s, char c) {
  int depth = 0;
  for (int i = 0; s[i]; ++i) {
    const char d = s[i];
    if (d == '(' || d == '[') ++depth;
    else if (d == ')' || d == ']') --depth;
    else if (!depth && d == c && (c != '=' || (i && !strchr("<>!=", s[i - 1]) && s[i + 1] != '='))) return i;
  }
  return -1;
}
// giac prints u=(x^2), dv=(sin(x)*dx): the parentheses around a whole side of = or a whole item
// of a list go (DeltaCE: of < > <= >= too, (3x-5+5)<(7+5))
static __attribute__((noinline)) void unwrap(char * s) {
  for (char * a = s; *a;) {
    int n = top(a, ','), e = top(a, '=');
    const int lt = top(a, '<'), gt = top(a, '>');
    if (lt >= 0 && (e < 0 || lt < e)) e = lt;
    if (gt >= 0 && (e < 0 || gt < e)) e = gt;
    if (n < 0) n = strlen(a);
    if (e >= 0 && e < n) n = e;
    char * b = a + n;
    if (n >= 2 && *a == '(' && b[-1] == ')') {
      int d = 0;
      char * q = a;
      for (; q < b; ++q) { if (*q == '(') ++d; else if (*q == ')' && !--d) break; }
      if (q == b - 1) { memmove(b - 1, b, strlen(b) + 1); memmove(a, a + 1, strlen(a)); b -= 2; }
    }
    a = *b ? b + 1 + ((*b == '<' || *b == '>') && b[1] == '=') : b;
  }
}
// where a line of s[0,n) may break: before a + or - between terms, at top level, after the =; after
// a comma of a list (x=pi/12,x=pi/6,...: the answer ran off the box)
static int cuts(const char * s, int n, short * at, int max) {
  int depth = 0, k = 0, eq = top(s, '=');
  const int s0 = eq < 0 ? 0 : eq + 1; // (every parenthesis counted: from 1, ((u)-(v))/(1-w)^2 was cut
  for (int i = s0; i < n && k < max; ++i) { // inside (1-w), a garbled line; no cut at the start)
    const char c = s[i];
    if (c == '(' || c == '[') ++depth;
    else if (c == ')' || c == ']') --depth;
    else if (!depth && i > s0 + 1 && (c == '+' || c == '-') && !strchr("+-*/^(,=", s[i - 1]) &&
             !(s[i - 1] == 'e' && i > 1 && s[i - 2] >= '0' && s[i - 2] <= '9')) at[k++] = i;
    else if (!depth && c == ',' && i + 1 < n) at[k++] = i + 1;
  }
  return k;
}

// the math fonts by level (upright, italic): the = of a line going on, + C
static const ui_face * const FACE[2][UI_NSIZES] = {
  {&ui_mu34, &ui_mu28, &ui_mu24, &ui_mu20, &ui_mu17, &ui_mu14, &ui_mu12, &ui_mu10},
  {&ui_mi34, &ui_mi28, &ui_mi24, &ui_mi20, &ui_mi17, &ui_mi14, &ui_mi12, &ui_mi10}};
static int gap(int lv) { return ui_math_metrics(lv, MI_F_IMPLDOT).opgap; }
static int eqw(int lv) { return ui_text_width(FACE[0][lv], "=", -1) + 2 * gap(lv); } // " = "
static int lay(const char * s, int n, int lv, mi_layout & L) {
  mi_build(s, n, -1, ui_math_metrics(lv, MI_F_IMPLDOT), L);
  return L.width;
}
// s[0,n) at x, y, at most w wide (lead: after "=", a line going on with an equation): the largest
// level from lv0 down to LVMIN that fits; wider, lines broken between terms (one item each, the next
// ones further in). Returns the height used.
static int add_math(const char * s, int n, int x, int y, int w, int lv0, int kind, int lead = 0) {
  mi_layout L;
  int lv = ui_math_fit(s, n, -1, lv0, w - (lead ? eqw(lv0) : 0), 200, MI_F_IMPLDOT, L), a = 0, h = -4;
  if (L.width + (lead ? eqw(lv) : 0) <= w && lv <= LVMIN) {
    item * it = put(kind, dup(s, n), x, y, (lead ? eqw(lv) : 0) + L.width, L.asc + L.desc, L.asc, lv);
    if (it && lead) { it->num = 1; it->eqx = 0; }
    return L.asc + L.desc;
  }
  const int e = lead || top(s, ',') >= 0 ? -1 : top(s, '='); // (a list: cut after its commas, below)
  if (e > 0 && e < n) { // too wide: its left side, then "= the right side" below (a product had no
    const int h1 = add_math(s, e, x, y, w, lv0, kind); // place to break: clipped at both edges)
    return h1 + 4 + add_math(s + e + 1, n - e - 1, x, y + h1 + 4, w, lv0, kind, 1);
  }
  short at[24];
  const int nc = cuts(s, n, at, 24), lr = lead ? eqw(LVWRAP) : 0;
  while (a < n) {
    const int xi = x + (a ? (lead ? lr : 20) : 0), wi = w - (xi - x) - (a ? 0 : lr);
    int best = -1, k = 0;
    while (k < nc && at[k] <= a) ++k;
    for (int j = k; j <= nc; ++j) { // the longest line that fits
      const int e = j < nc ? at[j] : n;
      if (lay(s + a, e - a, LVWRAP, L) <= wi) best = e;
      else break;
    }
    if (best < 0) best = k < nc ? at[k] : n; // (too wide anyway: smaller, down to 10 px, then clipped)
    const int ln = best - a - (best - a > 1 && s[best - 1] == ','); // (a list's line: its comma dropped)
    int lv = LVWRAP;
    while (lay(s + a, ln, lv, L) > wi && lv < UI_NSIZES - 1) ++lv;
    item * it = put(kind, dup(s + a, ln), xi, y + h + 4, (a ? 0 : lr) + L.width, L.asc + L.desc, L.asc, lv);
    if (it && lead && !a) { it->num = 1; it->eqx = 0; }
    h += L.asc + L.desc + 4;
    a = best;
  }
  return h;
}
// the end of a part: the second items of the working's lines in one column when they all fit;
// each group of math centered as a block
static void finish(int k0) {
  item * const p0 = P + k0, * const end = P + NP, * p;
  int col = 0;
  for (p = p0; p + 1 < end; ++p)
    if (p[1].num == 2 && p->kind == K_WORK && p->num != 2 && p->x + p->w + 24 > col) col = p->x + p->w + 24;
  for (p = p0; p < end; ++p) if (p->num == 2 && p->kind == K_WORK && col + p->w > XR) col = 0;
  if (col) for (p = p0; p < end; ++p) if (p->num == 2 && p->kind == K_WORK) p->x = col;
  for (p = p0; p < end;) {
    item * q = p;
    int a = XR, b = X;
    for (; q < end && q->grp == p->grp; ++q) {
      if (q->x < a) a = q->x;
      if (q->x + q->w > b) b = q->x + q->w;
    }
    if (p->grp) for (; p < q; ++p) p->x += X + (b - a < W ? (W - (b - a)) / 2 : 0) - a; // (wider: at the left, cut at the right only)
    p = q;
  }
}
static void section(const char * label, int back, int & y) { // a line, then the label
  ++SEC;
  put(K_LINE, 0, X - 8, y, W + 16, 1, 0, 0);
  y += 14;
  item * it = put(K_LABEL, dup(label, strlen(label)), X, y, 0, 10, 8, 0);
  if (it) it->num = back;
  y += 16;
}

// (DeltaCE) a table's cells (steps_table's text) at level lv: the columns' widths cw, the rows'
// ascents ra and heights rh (16 columns, 4 rows at most); its width, 0 if larger. Column 0 is c,
// a bar after it; numbers at the right of their column
enum { TG = 12, TB = 7 }; // the gap between columns, each side of c's bar
static int table_geom(const char * s, int lv, short * cw, short * ra, short * rh, int & nc, int & nr) {
  mi_layout L;
  nc = nr = 0;
  for (int j = 0; j < 16; ++j) cw[j] = 0;
  for (const char * p = s;;) {
    if (nr == 4) return 0;
    int a = 0, d = 0, c = 0;
    for (;;) {
      const char * q = p;
      while (*q && *q != '\t' && *q != '\n') ++q;
      if (c == 16) return 0;
      const char * t = *p == '\1' ? p + 1 : p;
      if (q > t) {
        lay(t, q - t, lv, L);
        if (L.width > cw[c]) cw[c] = L.width;
        if (L.asc > a) a = L.asc;
        if (L.desc > d) d = L.desc;
      }
      ++c;
      p = q;
      if (*p != '\t') break;
      ++p;
    }
    if (c > nc) nc = c;
    if (!a) { a = nr ? ra[0] : 11; d = nr ? rh[0] - 8 - ra[0] : 3; } // (an empty row keeps its place)
    ra[nr] = a; rh[nr] = a + d + 8; ++nr;
    if (*p != '\n') break;
    ++p;
  }
  int w = cw[0] + 2 * TB + 1;
  for (int j = 1; j < nc; ++j) w += cw[j] + (j > 1 ? TG : 0);
  return w;
}
static void draw_table(const item & it, int y, int bg) {
  short cw[16], ra[4], rh[4], cx[16];
  int nc, nr;
  if (!table_geom(it.s, it.lv, cw, ra, rh, nc, nr)) return;
  cx[0] = it.x;
  for (int j = 1; j < nc; ++j) cx[j] = j == 1 ? it.x + cw[0] + 2 * TB + 1 : cx[j - 1] + cw[j - 1] + TG;
  const int ln = ui_col(0, UC_SUB);
  mi_layout L;
  const char * p = it.s;
  int ry = y;
  for (int i = 0; i < nr; ++i) {
    for (int c = 0;; ++c) {
      const char * q = p;
      while (*q && *q != '\t' && *q != '\n') ++q;
      const bool nw = *p == '\1';
      const char * t = nw ? p + 1 : p;
      if (q > t && c < nc) {
        lay(t, q - t, it.lv, L);
        ui_math_draw(L, t, it.lv, cx[c] + cw[c] - L.width, ry + 4 + ra[i], 0, nw ? UC_ACC : UC_INK, bg, UC_ACC);
      }
      p = q;
      if (*p != '\t') break;
      ++p;
    }
    if (*p == '\n') ++p;
    ry += rh[i];
  }
  if (nr < 3 || nc < 3) return;
  const int x1 = cx[1] - TB / 2, x2 = cx[nc - 1] + cw[nc - 1] + 2;
  ui_fill(it.x + cw[0] + TB, y + 2, 1, rh[0] + rh[1] - 2, ln); // c's bar
  ui_fill(x1, y + rh[0] + rh[1], x2 - x1, 1, ln);                // the line above the sums
  ui_fill(cx[nc - 1] - TG / 2, y + rh[0] + rh[1] + 2, 1, rh[2] - 4, ln); // the remainder apart
}

// the steps of v as items (ans: the answer, as on the console; plusc: an antiderivative)
static __attribute__((noinline)) void lay_steps(const char * head, const std::vector<step_t> & v, const char * ans, bool plusc) {
  int y = 10, num = 0, prevd = 0, fe = -1, eqn = 0, k0 = 0; // fe: the part's last equation, its
  // left side s[0,eqn) (the lines going on with it have their = under its =)
  unsigned char last[8] = {0}; // the number of the last step at each depth
  SEC = 0; GRP = 1;
  y += add_math(head, strlen(head), X, y, W, 2, K_HEAD) + 16;
  if (steps_methods > 1 && *steps_method_name) { // (DeltaCE) the method, F4 the next: a chip under the problem
    const int w = ui_text_width(&ui_tb9, steps_method_name, -1) + ui_text_width(&ui_tb9, "F4", -1) + 20;
    put(K_CHIP, dup(steps_method_name, strlen(steps_method_name)), X + (W - w) / 2, y - 8, w, 13, 10, 0);
    y += 14;
  }
  giac::gen sa;
  const unsigned nv = ans && *ans && steps_answer(v, sa) ? v.size() - 1 : v.size(); // (DeltaCE: the steps' answer, in the box only)
  for (unsigned i = 0; i < nv; ++i) {
    const int d = v[i].depth < 7 ? v[i].depth : 7;
    const char * title = steps_title(v[i]);
    focus_phase = 133;
    const std::string tb = steps_table(v[i], contextptr); // (DeltaCE: a synthetic division's table)
    std::string t = tb.empty() ? steps_math(v[i], contextptr).print(contextptr) : tb;
    focus_phase = 134;
    if (oom_hit) break;
    char * s = dup(t.c_str(), t.size());
    if (!s) break;
    if (tb.empty()) unwrap(s);
    if (*title || (d < prevd && last[d])) { // a part: a step, or back to the one a part was for
      finish(k0);
      char lb[24];
      strcpy(lb, *title ? "STEP " : "BACK TO STEP ");
      digits(lb + strlen(lb), *title ? (last[d] = ++num) : last[d]);
      k0 = NP;
      section(lb, !*title, y);
      if (*title) {
        put(K_TITLE, (char *)title, X, y, 0, 14, 11, 0);
        y += 22;
        const char * f = steps_formula(v[i]);
        if (f) y += add_math(f, strlen(f), X, y, W, 5, K_FORM) + 14;
      }
      fe = -1;
    }
    prevd = d;
    if (!tb.empty()) { // the table centered, at the largest level that fits
      short cw[16], ra[4], rh[4];
      int nc, nr, lv = 3, w = 0;
      while (lv <= LVMIN && ((w = table_geom(s, lv, cw, ra, rh, nc, nr)) == 0 || w > W)) ++lv;
      if (w && w <= W) {
        int h = 0;
        for (int r = 0; r < nr; ++r) h += rh[r];
        put(K_TABLE, s, X + (W - w) / 2, y, w, h, 0, lv); // (put frees s if it cannot keep it)
        y += h + 12;
      }
      else free(s);
      continue;
    }
    const int eq = top(s, '=');
    if (eq > 0 && top(s, ',') < 0 && (starts(s, "integrate(") || starts(s, "diff(") || starts(s, "derive(") || starts(s, "int("))) {
      if (fe >= 0 && eq == eqn && !strncmp(s, P[fe].s, eq)) { // = under the = above
        const int ex = P[fe].x + P[fe].eqx, lv0 = P[fe].lv, n = strlen(s + eq + 1);
        mi_layout L;
        const int lv = ui_math_fit(s + eq + 1, n, -1, lv0, XR - ex - eqw(lv0), 200, MI_F_IMPLDOT, L);
        if (lv <= LVMIN && ex + eqw(lv) + L.width <= XR) {
          item * it = put(K_EQ, dup(s + eq + 1, n), ex, y, eqw(lv) + L.width, L.asc + L.desc, L.asc, lv);
          if (it) it->num = 1;
          y += L.asc + L.desc + 12;
        }
        else y += add_math(s + eq + 1, n, X, y, W, lv0, K_EQ, 1) + 12; // (too long there)
      }
      else { // a new equation: where its = is (its left side's width) when on one line
        const int k = NP;
        ++GRP;
        y += add_math(s, strlen(s), X, y, W, 3, K_EQ) + 12;
        fe = -1;
        if (NP == k + 1) { mi_layout L; P[k].eqx = lay(s, eq, P[k].lv, L); fe = k; eqn = eq; }
      }
    }
    else { // working (u = x, dv = ...; an identity): the items of a line side by side when they fit
      if (P[NP - 1].kind != K_WORK) ++GRP;
      const int k0 = NP;
      for (int a = 0; s[a];) {
        int n = top(s + a, ',');
        if (n < 0) n = strlen(s + a);
        mi_layout L;
        const int lv = ui_math_fit(s + a, n, -1, 4, W, 60, MI_F_IMPLDOT, L);
        const item * p = P + NP - 1;
        const int px = p->x + p->w + 24, pb = p->y + p->base;
        if (a && p->kind == K_WORK && lv <= LVMIN && px + L.width <= XR) { // beside the one before
          item * c = put(K_WORK, dup(s + a, n), px, pb - L.asc, L.width, L.asc + L.desc, L.asc, lv);
          if (c) c->num = 2;
          if (pb + L.desc + 10 > y) y = pb + L.desc + 10;
        }
        else y += add_math(s + a, n, X, y, W, 4, K_WORK) + 10;
        a += n + (s[a + n] == ',');
      }
      if (NP - k0 > 2) for (int k = k0 + 1; k < NP; ++k) if (P[k].num == 2) P[k].num = 3; // (a = 1, b = -5, c = 6)
      y += 4;
    }
    free(s);
  }
  finish(k0);
  if (ans && *ans) { // the answer, in a box (instead of a last line "= the answer")
    if (NP && ((P[NP - 1].kind == K_EQ && P[NP - 1].num == 1) || P[NP - 1].kind == K_WORK) && !strcmp(P[NP - 1].s, ans)) { y = P[NP - 1].y; free(P[--NP].s); }
    k0 = NP;
    section("ANSWER", 0, y);
    const int b = NP;
    if (put(K_BOX, 0, X - 4, y, W + 8, 0, 0, 0)) {
      ++GRP;
      static const char DNE[] = "does not exist", DIV[] = "diverges", NOS[] = "no solution"; // (a limit's, a series', an equation's text answer)
      const char * tx = starts(ans, DNE) || starts(ans, "no limit") ? DNE : starts(ans, DIV) ? DIV : starts(ans, NOS) ? NOS : 0; // (sin(1/x) at 0: "no limit (it oscillates)")
      const int h = tx ? (put(K_TITLE, (char *)tx, X + (W - ui_text_width(&ui_tb12, tx, -1)) / 2, y + 14, 0, 16, 13, 0), 16)
                       : add_math(ans, strlen(ans), X, y + 14, W - 12 - (plusc ? 44 : 0), 2, K_ANS);
      if (plusc && NP > b + 1) { // + C after the last line
        item & l = P[NP - 1];
        l.num = ui_text_width(FACE[0][l.lv], "+ ", -1) + ui_text_width(FACE[1][l.lv], "C", -1);
        l.w += 8 + l.num;
      }
      P[b].h = h + 28;
      y += h + 28;
    }
    finish(k0);
  }
  PH = y + 12;
}

// drawing: the last math layout is kept (an item cut by a band's edge is drawn twice)
static mi_layout * CL;
static const item * cl_p;
static void draw_item(const item & it, int y, int bg) { // y: its top on the screen; bg: the page's
  const int b = y + it.base, ink = it.kind == K_FORM ? UC_SUB : UC_INK;
  int x = it.x;
  switch (it.kind) {
  case K_LINE: ui_fill(x, y, it.w, 1, ui_col(0, UC_LINE)); return;
  case K_LABEL: ui_draw_text(&ui_tb9, it.s, -1, x, b, ui_ramp(0, it.num ? UC_SUB : UC_ACC, bg), 0); return;
  case K_TITLE: ui_draw_text(&ui_tb12, it.s, -1, x, b, ui_ramp(0, UC_INK, bg), 0); return;
  case K_TEXT: ui_draw_text(&ui_tr12, it.s, -1, x, b, ui_ramp(0, it.num ? UC_INK : UC_SUB, bg), 0); return;
  case K_BOX: ui_rrect(0, x, y, it.w, it.h, 8, UC_ACCSOFT, bg); return;
  case K_TABLE: draw_table(it, y, bg); return;
  case K_CHIP: { // (as the console's forms chip)
    ui_rrect(0, x, y, it.w, it.h, 6, UC_ACCSOFT, bg);
    const int w1 = ui_draw_text(&ui_tb9, it.s, -1, x + 6, y + it.base, ui_ramp(0, UC_ACC, UC_ACCSOFT), 0);
    ui_draw_text(&ui_tb9, "F4", -1, x + 12 + w1, y + it.base, ui_ramp(0, UC_SUB, UC_ACCSOFT), 0);
    return;
  }
  case K_ANS: bg = UC_ACCSOFT; break;
  case K_WORK: // (a working line broken at its =: the = too)
  case K_EQ:
    if (it.num == 1) { // a line going on: its = first
      ui_draw_text(FACE[0][it.lv], "=", -1, x + gap(it.lv), b, ui_ramp(0, UC_INK, bg), 0);
      x += eqw(it.lv);
    }
  }
  if (cl_p != &it) { lay(it.s, strlen(it.s), it.lv, *CL); cl_p = &it; }
  ui_math_draw(*CL, it.s, it.lv, x, b, 0, ink, bg, UC_ACC);
  if (it.kind == K_ANS && it.num) { // + C
    const unsigned char * r = ui_ramp(0, UC_SUB, bg);
    x = it.x + it.w - it.num;
    x += ui_draw_text(FACE[0][it.lv], "+ ", -1, x, b, r, 0);
    ui_draw_text(FACE[1][it.lv], "C", -1, x, b, r, 0);
  }
}
// the page part sec: its top and bottom (its line, the next part's), the page's end
static void part(int sec, int & y0, int & y1) {
  y0 = -1; y1 = PH;
  for (const item * p = P; p < P + NP; ++p)
    if (p->kind == K_LINE) { if (p->sec == sec) y0 = p->y; else if (p->sec == sec + 1) { y1 = p->y; break; } }
}
// the page from its row top, on the screen's rows Y0..; part cur highlighted (not the answer)
static void paint(int top, int cur) {
  if (!CL) CL = new mi_layout;
  cl_p = 0;
  int h0, h1;
  part(cur, h0, h1);
  if (cur == SEC && P[NP - 1].kind == K_ANS) h0 = -1;
  const int rows = ui_band_open(48), vis = UI_H - Y0;
  for (int b = Y0; b < UI_H; b += rows ? rows : UI_H) {
    const int e = rows && b + rows < UI_H ? b + rows : UI_H;
    if (rows) ui_band_begin(b, e);
    ui_fill(0, b, UI_W, e - b, ui_col(0, UC_BG));
    ui_clip(0, Y0, UI_W, UI_H);
    if (h0 >= 0) ui_rrect(0, X - 8, Y0 + h0 + 5 - top, W + 16, h1 - h0 - 9, 8, UC_ACCSOFT, UC_BG);
    for (const item * p = P; p < P + NP; ++p) {
      const int y = Y0 + p->y - top, on = h0 >= 0 && (p->sec == cur || (p->kind == K_LINE && p->sec == cur + 1));
      if (y < e && y + p->h + 4 > b && !(on && p->kind == K_LINE)) draw_item(*p, y, on ? UC_ACCSOFT : UC_BG);
    }
    ui_noclip();
    if (PH > vis) { // where we are
      const int th = vis - 8, bh = th * vis / PH > 8 ? th * vis / PH : 8;
      ui_fill(UI_W - 5, Y0 + 4, 2, th, ui_col(0, UC_LINE));
      ui_fill(UI_W - 5, Y0 + 4 + (th - bh) * top / (PH - vis), 2, bh, ui_col(0, UC_SUB));
    }
    if (rows) ui_band_end();
  }
  ui_band_close();
  delete CL; CL = 0;
}
// where the page starts when part sec is read: its line near the top (the problem above part 1)
static int stop(int sec, int maxtop) {
  int y0, y1;
  part(sec, y0, y1);
  y0 = sec <= 1 ? 0 : y0 - 8;
  return y0 < maxtop ? y0 : maxtop;
}

// While the steps are worked out (seconds on the CE; Enter showed nothing, the user said): at
// once the problem, as at the top of the page, over "Working out the steps" and the dots of
// "Calculating" (focus_busy_tick, from giac's control_c checks; ON stops, as for any calculation)
static int wait_y; // the baseline of that line, 0 if not shown
static void waiting(const char * head, const char * T = "Working out the steps") {
  SEC = 0; GRP = 1;
  wait_y = (Y0 + 10 + add_math(head, strlen(head), X, 10, W, 2, K_HEAD) + UI_H) / 2;
  finish(0);
  if (NP) paint(0, 0);
  clear_page();
  ui_draw_text(&ui_tr12, T, -1, (UI_W - ui_text_width(&ui_tr12, T, -1)) / 2, wait_y, ui_ramp(0, UC_SUB, UC_BG), 0);
  focus_busy(wait_y + 10);
  focus_status_msg("ON or clear: stop");
}

// the steps of the calculation typed as in (its answer ans): computed (the waiting screen, ON
// stops) and laid out; 0 if done, else why not
static __attribute__((noinline)) const char * build(const char * in, const char * ans) {
  const int S = strlen(in), cap = 2 * S + 32 > 256 ? 2 * S + 32 : 256;
  char * buf = (char *)malloc(cap);
  wait_y = 0;
  if (!buf) return "Out of memory";
  strcpy(buf, in);
  answer_prepass(buf, cap, true);
  std::vector<step_t> v;
  std::string head;
  bool ok = false;
  giac::ctrl_c = giac::kbd_interrupted = giac::interrupted = false;
  {
    giac::gen g(buf, contextptr);
    if (!giac::interrupted) {
      head = steps_problem(g, contextptr).print(contextptr);
      if (char * h = dup(head.c_str(), head.size())) { unwrap(h); head = h; free(h); } // (2(x+3)=4x-5, not =(4x-5))
      waiting(head.c_str());
      giac::control_c_hook = focus_busy_tick;
      quiet(1);
      focus_phase = 130;
      ok = steps_of(g, v, contextptr);
      focus_phase = 131;
      quiet(0);
    }
  }
  const bool stopped = giac::interrupted || giac::ctrl_c;
  giac::control_c_hook = 0;
  free(buf);
  const char * why = oom_hit ? "Out of memory" : giac::stack_hit ? "Too many recursion levels" : stopped ? "Stopped" :
    !ok || v.empty() ? "No steps for this one" : 0;
  giac::ctrl_c = giac::kbd_interrupted = giac::interrupted = giac::stack_hit = false;
  if (!why) {
    const char * p = in; // an antiderivative (integrate with 2 arguments): + C
    while (*p == ' ') ++p;
    int c = 0, depth = 0;
    if (starts(p, "integrate(") || starts(p, "int("))
      for (p = strchr(p, '(') + 1; *p && depth >= 0; ++p) {
        if (*p == '(' || *p == '[') ++depth;
        else if (*p == ')' || *p == ']') --depth;
        else if (*p == ',' && !depth) ++c;
      }
    giac::gen sa;
    std::string a2; // the answer box: the steps' value for a sum left as is, a series without O(...)
    if (ans && strstr(ans, "sum(") && !v.empty() && (!*steps_title(v.back()) || !strcmp(steps_title(v.back()), "Its limit")))
      ans = (a2 = v.back().b.print(contextptr)).c_str();
    else if (ans && (starts(p, "series(") || starts(p, "taylor(")) && !v.empty()) // (the steps' polynomial: 1-(x-1)+(x-1)^2,
      ans = (a2 = v.back().b.print(contextptr)).c_str(); // not giac's 1-x+1+(x-1)^2+O((x-1)^3))
    else if (ans && steps_answer(v, sa)) // (DeltaCE: a division typed, quotient plus remainder over the divisor)
      ans = (a2 = sa.print(contextptr)).c_str();
    lay_steps(head.c_str(), v, ans, c == 1);
    focus_phase = 132;
    if (oom_hit) { giac::ctrl_c = giac::interrupted = false; why = NP ? 0 : "Out of memory"; }
  }
  focus_busy(0);
  v.clear();
  if (oom_hit) oom_rearm();
  return why;
}

// ---------------------------------------------------------------- the analysis (DeltaCE)
// A row of the report: its label (gray) on the left, its items beside it, on as many lines as
// they need (a line's items on one baseline); no label: the items centered.
static int row(const an_line & l, int y) {
  const bool lab = l.text && *l.text;
  const int lw = lab ? ui_text_width(&ui_tr12, l.text, -1) : 0;
  if (lab && l.math.empty()) { // a sentence
    const int k = NP;
    if (put(K_TEXT, dup(l.text, strlen(l.text)), X, y, lw, 14, 11, 0)) P[k].num = 1;
    return y + 14;
  }
  if (!lab) ++GRP;
  const char * sep = l.sep == AN_OR ? " or " : l.sep == AN_SPACE ? " " : ", ";
  const int sw = ui_text_width(&ui_tr12, sep, -1), x0 = lab ? (lw + 10 < 120 ? X + lw + 10 : X + 16) : X;
  struct ent { const char * s; int n, x, w, asc, desc, lv, text; } e[16];
  int ne = 0, x = x0, top = y;
  if (lab) { // the label: the first line's first item
    e[ne].s = l.text; e[ne].n = strlen(l.text); e[ne].x = X; e[ne].w = lw; e[ne].asc = 11; e[ne].desc = 3; e[ne].lv = 0; e[ne].text = 2; ++ne;
    if (x0 == X + 16) x = XR + 1; // (a long label: its items on the next line)
  }
  const char * m = l.math.c_str();
  for (int first = 1;; first = 0) {
    const char * nl = strchr(m, '\n');
    const int n = nl ? nl - m : strlen(m);
    const bool tx = n && m[0] == '\1';
    mi_layout L;
    int w, asc = 11, desc = 3, lv = 0;
    if (tx) w = ui_text_width(&ui_tr12, m + 1, n - 1);
    else { lv = ui_math_fit(m, n, -1, 4, XR - x0, 60, MI_F_IMPLDOT, L); w = L.width; asc = L.asc; desc = L.desc; }
    const int need = (first ? 0 : sw) + w;
    if ((x + need > XR && x > x0) || ne > 13) { // the line is full: placed on its baseline (a comma
      if (!first && l.sep == AN_COMMA) { // ends it, "or" starts the next one)
        e[ne].s = ","; e[ne].n = 1; e[ne].x = x; e[ne].w = ui_text_width(&ui_tr12, ",", 1); e[ne].asc = 11; e[ne].desc = 3; e[ne].lv = 0; e[ne].text = 2; ++ne;
      }
      int a = 0, d = 0;
      for (int i = 0; i < ne; ++i) { if (e[i].asc > a) a = e[i].asc; if (e[i].desc > d) d = e[i].desc; }
      for (int i = 0; i < ne; ++i) {
        const int k = NP;
        const bool t = e[i].text;
        if (put(t ? K_TEXT : lab ? K_MATH : K_WORK, dup(e[i].s, e[i].n), e[i].x, top + a - e[i].asc, e[i].w, e[i].asc + e[i].desc, e[i].asc, e[i].lv)) {
          P[k].num = e[i].text == 1;
          if (!lab) P[k].grp = GRP;
        }
      }
      top += a + d + 6; ne = 0; x = x0;
      if (l.sep == AN_COMMA) first = 1;
    }
    if (!first) { e[ne].s = sep; e[ne].n = strlen(sep); e[ne].x = x; e[ne].w = sw; e[ne].asc = 11; e[ne].desc = 3; e[ne].lv = 0; e[ne].text = 2; ++ne; x += sw; }
    e[ne].s = tx ? m + 1 : m; e[ne].n = tx ? n - 1 : n; e[ne].x = x; e[ne].w = w; e[ne].asc = asc; e[ne].desc = desc; e[ne].lv = lv; e[ne].text = tx; ++ne;
    x += w;
    if (!nl) break;
    m = nl + 1;
  }
  int a = 0, d = 0;
  for (int i = 0; i < ne; ++i) { if (e[i].asc > a) a = e[i].asc; if (e[i].desc > d) d = e[i].desc; }
  for (int i = 0; i < ne; ++i) {
    const int k = NP;
    const bool t = e[i].text;
    if (put(t ? K_TEXT : lab ? K_MATH : K_WORK, dup(e[i].s, e[i].n), e[i].x, top + a - e[i].asc, e[i].w, e[i].asc + e[i].desc, e[i].asc, e[i].lv)) {
      P[k].num = e[i].text == 1;
      if (!lab) P[k].grp = GRP;
    }
  }
  return top + a + d;
}
static __attribute__((noinline)) void lay_report(const std::vector<an_line> & v) {
  int y = 10;
  SEC = 0; GRP = 1;
  for (unsigned i = 0; i < v.size() && !oom_hit; ++i) {
    const an_line & l = v[i];
    if (l.kind == AN_HEAD) y += add_math(l.math.c_str(), l.math.size(), X, y, W, 2, K_HEAD) + 14;
    else if (l.kind == AN_SECTION) section(l.text, 0, y);
    else y = row(l, y) + 9;
  }
  finish(0);
  PH = y + 8;
}
// the analysis of the function typed as in (a calculation's text): computed (the waiting screen,
// ON stops) and laid out; 0 if done, else why not
static __attribute__((noinline)) const char * build_report(const char * in) {
  const int S = strlen(in), cap = 2 * S + 32 > 256 ? 2 * S + 32 : 256;
  char * buf = (char *)malloc(cap);
  wait_y = 0;
  if (!buf) return "Out of memory";
  strcpy(buf, in);
  answer_prepass(buf, cap, true);
  std::vector<an_line> v;
  const char * why = 0;
  giac::ctrl_c = giac::kbd_interrupted = giac::interrupted = false;
  {
    giac::gen g(buf, contextptr);
    if (!giac::interrupted) {
      std::string head = giac::eval(g, 1, contextptr).print(contextptr); // (cos(x)^2-sin(x), not as typed)
      waiting(head.c_str(), "Analyzing");
      giac::control_c_hook = focus_busy_tick;
      quiet(1);
      why = analyze_of(g, v, contextptr);
      quiet(0);
    }
  }
  const bool stopped = giac::interrupted || giac::ctrl_c;
  giac::control_c_hook = 0;
  free(buf);
  if (oom_hit) why = "Out of memory";
  else if (giac::stack_hit) why = "Too many recursion levels";
  else if (stopped) why = "Stopped";
  giac::ctrl_c = giac::kbd_interrupted = giac::interrupted = giac::stack_hit = false;
#ifdef ANDEBUG // the report as text on CEmu's debug console (scripts/emu_analyze_corpus.py; -A's format)
  ADBG("== %s   [%s]\n", in, why ? why : "ok");
  for (unsigned i = 0; i < v.size(); ++i) {
    std::string m = v[i].math;
    for (unsigned k = 0; k < m.size(); ++k) { if (m[k] == '\n') m[k] = '|'; else if (m[k] == '\1') m[k] = '~'; }
    if (v[i].kind == AN_SECTION) ADBG("  %s\n", v[i].text);
    else ADBG("    %s%s%s\n", v[i].text ? v[i].text : "", v[i].text && *v[i].text && !m.empty() ? ":  " : "", m.c_str());
  }
#endif
  if (!why) {
    lay_report(v);
    if (oom_hit) { giac::ctrl_c = giac::interrupted = false; why = NP ? 0 : "Out of memory"; }
  }
  focus_busy(0);
  v.clear();
  if (oom_hit) oom_rearm();
  return why;
}

// up/down from part cur at top: within a part taller than the screen, else to the next part
static void move(int dir, int & cur, int & top, int maxtop) {
  const int vis = UI_H - Y0;
  int y0, y1;
  part(cur, y0, y1);
  if (dir > 0 && y1 > top + vis && top < maxtop) { // the rest of the part, a screen at most
    top = y1 - vis + 6 < top + vis - 40 ? y1 - vis + 6 : top + vis - 40;
    if (top > maxtop) top = maxtop;
  }
  else if (dir < 0 && top > stop(cur, maxtop)) top = top - (vis - 40) > stop(cur, maxtop) ? top - (vis - 40) : stop(cur, maxtop);
  else if (cur + dir >= 1 && cur + dir <= SEC) { cur += dir; top = stop(cur, maxtop); }
}

static int show(const char * why, const char * name, int methods);
void focus_steps(const char * in, const char * ans) {
  steps_method = 0;
  for (;;) { // (DeltaCE) F4: the problem's next method (a division: long, synthetic, factor and cancel)
    focus_status_label("STEPS");
    if (show(build(in, ans), "STEPS", steps_methods) != KEY_CTRL_F4) break;
    ++steps_method;
  }
  steps_method = 0;
}
// DeltaCE: a function's analysis (analyze.cc), on the steps' page
void focus_analyze(const char * in) {
  focus_status_label("ANALYSIS");
  show(build_report(in), "ANALYSIS", 0);
}
// (DeltaCE) the page's keys: up/down read on (a part taller than the screen a screen at a time),
// left/right a whole part, 2nd up/down the first part and the answer; the status bar says where.
// F4 with methods (more than one) leaves for the next. Returns the key that left
static int show(const char * why, const char * name, int methods) {
  focus_status_msg(0);
  int key;
  if (why && !strcmp(why, "No steps for this one")) { // (not an error: said in the status bar, the console back)
    clear_page();
    focus_status_label(0);
    focus_invalidate();
    focus_status_msg("No steps for this one");
    return 0;
  }
  if (why) { // over the problem (the waiting line erased)
    clear_page();
    if (wait_y) ui_fill(0, wait_y - 14, UI_W, 32, ui_col(0, UC_BG));
    focus_note(why, 0);
    GetKey(&key);
    key = 0; // (a key after an error leaves)
  }
  else {
    const int vis = UI_H - Y0, maxtop = PH > vis ? PH - vis : 0;
    int top = 0, cur = 1;
    static char lbl[24];
    for (;;) {
      if (SEC > 1) { // STEPS  3 / 8
        char * q = lbl;
        strcpy(q, name); q += strlen(q); *q++ = ' '; *q++ = ' ';
        q = digits(q, cur); strcpy(q, " / "); digits(q + 3, SEC);
        focus_status_label(lbl);
      }
      paint(top, cur);
      GetKey(&key);
      if (key == KEY_CTRL_EXIT || key == KEY_CTRL_AC || key == KEY_CTRL_EXE || key == KEY_CTRL_OK || (key == KEY_CTRL_F4 && methods > 1)) break;
      if (key == KEY_CTRL_DOWN) move(1, cur, top, maxtop);
      if (key == KEY_CTRL_UP) move(-1, cur, top, maxtop);
      if (key == KEY_CTRL_RIGHT && cur < SEC) top = stop(++cur, maxtop);
      if (key == KEY_CTRL_LEFT && cur > 1) top = stop(--cur, maxtop);
      if (key == KEY_CTRL_PAGEDOWN) top = stop(cur = SEC, maxtop);
      if (key == KEY_CTRL_PAGEUP) { cur = 1; top = 0; }
    }
  }
  clear_page();
  focus_status_label(0);
  focus_invalidate();
  return key;
}

#ifdef HOSTGIAC
// tools/hostgiac -W: the analysis screen of in after stop presses of down, as -V
int focus_analyze_preview(const char * in, int stop) {
  if (build_report(in)) { clear_page(); return -1; }
  const int vis = UI_H - Y0, maxtop = PH > vis ? PH - vis : 0;
  int top = 0, cur = 1;
  for (int i = 0; i < stop; ++i) move(1, cur, top, maxtop);
  paint(top, cur);
  const int n = SEC;
  clear_page();
  return n;
}
// tools/hostgiac -V: the screen of in (answer ans) after stop presses of down, drawn off screen;
// the number of parts, -1 if no steps
int focus_steps_preview(const char * in, const char * ans, int stop) {
  if (build(in, ans)) { clear_page(); return -1; }
  const int vis = UI_H - Y0, maxtop = PH > vis ? PH - vis : 0;
  int top = 0, cur = 1;
  for (int i = 0; i < stop; ++i) move(1, cur, top, maxtop);
  paint(top, cur);
  const int n = SEC;
  clear_page();
  return n;
}
#endif
#endif
