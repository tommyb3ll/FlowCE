// test_mathinput.cc - host unit tests for src/mathinput.cc (2D display of the input line).
// Build and run with tests/host/run_mathinput_tests.sh. mathinput.cc must be compiled with
// -DMI_TEST, which exports mi_test_errors (caret position emission order errors and node
// pool overflows; both must stay 0).
#include "mathinput.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

extern int mi_test_errors;

// ---------------------------------------------------------------- mini framework
static int g_checks, g_fails, g_case_fails, g_cases, g_cases_failed;

static void check_(bool ok, const char * what, int line) {
  ++g_checks;
  if (!ok) { ++g_fails; ++g_case_fails; printf("    check failed (line %d): %s\n", line, what); }
}
static void checkeq_(long a, long b, const char * ea, const char * eb, int line) {
  ++g_checks;
  if (a != b) {
    ++g_fails; ++g_case_fails;
    printf("    check failed (line %d): %s == %s  (%ld != %ld)\n", line, ea, eb, a, b);
  }
}
#define CHECK(c) check_((c), #c, __LINE__)
#define CHECKEQ(a, b) checkeq_((long)(a), (long)(b), #a, #b, __LINE__)

static void run_case(const char * name, void (*fn)()) {
  g_case_fails = 0;
  int e0 = mi_test_errors;
  fn();
  if (mi_test_errors != e0) { ++g_fails; ++g_case_fails; printf("    module self-check errors: %d\n", mi_test_errors - e0); }
  ++g_cases;
  if (g_case_fails) ++g_cases_failed;
  printf("%s %s\n", g_case_fails ? "FAIL" : "PASS", name);
}

// ---------------------------------------------------------------- helpers
static const mi_metrics & MM = mi_default_metrics;
enum { LEFT, RIGHT, UP, DOWN };
static int slen(const char * s) { return (int)strlen(s); }
static mi_layout lay(const char * s, int caret = -1) {
  mi_layout L;
  mi_build(s, slen(s), caret, MM, L);
  return L;
}
struct Loc { int x, y, h; };
static Loc loc(const char * s, int p) { mi_layout L = lay(s, p); Loc l = {L.cx, L.cy, L.ch}; return l; }
static bool same(Loc a, Loc b) { return a.x == b.x && a.y == b.y && a.h == b.h; }
static bool isloc(Loc a, int x, int y, int h) { return a.x == x && a.y == y && a.h == h; }
static int mv(const char * s, int p, int dir) { return mi_move(s, slen(s), p, dir, MM); }
// apply a key script (L R U D) from p; stops at -1
static int keys(const char * s, int p, const char * k) {
  for (; *k && p >= 0; ++k) p = mv(s, p, *k == 'L' ? LEFT : *k == 'R' ? RIGHT : *k == 'U' ? UP : DOWN);
  return p;
}
// positions visited by repeating one key n times
static std::string walk(const char * s, int p, int dir, int n) {
  std::string r;
  char b[16];
  for (int i = 0; i < n; ++i) { p = mv(s, p, dir); snprintf(b, sizeof b, "%s%d", i ? "," : "", p); r += b; }
  return r;
}
static int cnt(const mi_layout & L, int code) {
  int n = 0;
  for (size_t i = 0; i < L.ops.size(); ++i) n += L.ops[i].code == code;
  return n;
}
static int parens(const mi_layout & L) {
  int n = 0;
  for (size_t i = 0; i < L.ops.size(); ++i) n += L.ops[i].code >= MI_LPAREN && L.ops[i].code <= MI_RBRACE;
  return n;
}
static bool optext(const mi_op & o, const char * s, const char * str) {
  if (o.code != MI_TEXT) return false;
  if (o.lit) return !strcmp(o.lit, str);
  return o.len == slen(str) && !memcmp(s + o.pos, str, o.len);
}
static const mi_op * text(const mi_layout & L, const char * s, const char * str, int nth = 0) {
  for (size_t i = 0; i < L.ops.size(); ++i)
    if (optext(L.ops[i], s, str) && !nth--) return &L.ops[i];
  return 0;
}
static const mi_op * textsm(const mi_layout & L, const char * s, const char * str, int sm) {
  for (size_t i = 0; i < L.ops.size(); ++i)
    if (optext(L.ops[i], s, str) && L.ops[i].small == sm) return &L.ops[i];
  return 0;
}
static const mi_op * opc(const mi_layout & L, int code, int nth = 0) {
  for (size_t i = 0; i < L.ops.size(); ++i)
    if (L.ops[i].code == code && !nth--) return &L.ops[i];
  return 0;
}
static mi_op none() { mi_op o; memset(&o, 0, sizeof o); o.code = 255; return o; }
static mi_op g_none = none();
#define OP(e) (*((e) ? (e) : &g_none)) // dereference without crashing when an op is missing
static bool inb(const mi_layout & L) { // caret inside or on the layout bounds
  return L.cx >= 0 && L.cx <= L.width && L.cy >= -L.asc && L.cy + L.ch <= L.desc && L.ch > 0;
}
static bool caret_in_box(const mi_layout & L) {
  for (size_t i = 0; i < L.ops.size(); ++i) {
    const mi_op & o = L.ops[i];
    if (o.code == MI_BOX && L.cx >= o.x - 1 && L.cx <= o.x + o.w && L.cy < o.y + o.h && L.cy + L.ch > o.y)
      return true;
  }
  return false;
}
static bool allsmall(const mi_layout & L, int sm) {
  for (size_t i = 0; i < L.ops.size(); ++i) if (L.ops[i].small != sm) return false;
  return true;
}

// ---------------------------------------------------------------- structure
static void t_fraction() {
  const char * s = "(x+1)/(x-1)";
  mi_layout L = lay(s);
  const mi_op & bar = OP(opc(L, MI_HLINE)), & nx = OP(text(L, s, "x", 0)), & dx = OP(text(L, s, "x", 1));
  const mi_op & n1 = OP(text(L, s, "1", 0));
  CHECKEQ(cnt(L, MI_HLINE), 1);
  CHECKEQ(parens(L), 0);               // hidden parentheses
  CHECKEQ(cnt(L, MI_TEXT), 6);
  CHECKEQ(bar.y, -(14 / 3));           // bar at the math axis
  CHECKEQ(bar.h, 1);
  CHECKEQ(bar.w, 3 * 8 + 2 * 2 + 4);   // max(num, den) + 4
  CHECKEQ(nx.y + 4, bar.y - 2);        // numerator bottom 2 px above the bar
  CHECKEQ(dx.y - 14, bar.y + 1 + 2);   // denominator top 2 px below the bar
  CHECKEQ(nx.x + n1.x + n1.w, 2 * bar.x + bar.w); // numerator centered
  CHECK(allsmall(L, 0));
  CHECKEQ(L.width, 32); CHECKEQ(L.asc, 24); CHECKEQ(L.desc, 17);
  L = lay("x/2");
  CHECKEQ(parens(L), 0);
  CHECK(OP(text(L, "x/2", "x")).y < OP(opc(L, MI_HLINE)).y);
  CHECK(OP(text(L, "x/2", "2")).y > OP(opc(L, MI_HLINE)).y);
  CHECKEQ(L.width, 12);
  CHECKEQ(OP(text(L, "x/2", "x")).x, 2);
  L = lay("z(x+1)/(x-1)");              // operand not exactly a group: visible parens
  CHECKEQ(parens(L), 2);
  CHECKEQ(cnt(L, MI_LPAREN), 1);
  L = lay("2(x+1)/3");
  CHECKEQ(parens(L), 2);
  L = lay("a/b/c");                     // left associative: (a/b)/c
  CHECKEQ(cnt(L, MI_HLINE), 2);
  CHECK(OP(opc(L, MI_HLINE, 1)).y < OP(opc(L, MI_HLINE, 0)).y);
}

static void t_fraction_nested() {
  const char * s = "1/(1+1/x)";
  mi_layout L = lay(s);
  const mi_op & outer = OP(opc(L, MI_HLINE, 0)), & inner = OP(opc(L, MI_HLINE, 1));
  CHECKEQ(cnt(L, MI_HLINE), 2);
  CHECKEQ(parens(L), 0);
  CHECK(inner.y > outer.y);             // inner fraction in the denominator
  CHECK(inner.x > outer.x && inner.x + inner.w <= outer.x + outer.w);
  CHECK(allsmall(L, 0));                // nested fractions keep the font size
  CHECKEQ(OP(text(L, s, "x")).y - 14, inner.y + 3);
}

static void t_power() {
  const char * s = "x^2";
  mi_layout L = lay(s);
  const mi_op & e = OP(text(L, s, "2"));
  CHECKEQ(e.small, 1);
  CHECKEQ(e.y + 3, -(14 * 45 / 100));  // exponent bottom at 45% of the base ascent
  CHECKEQ(e.x, 8);
  CHECKEQ(OP(text(L, s, "x")).small, 0);
  CHECKEQ(L.width, 14); CHECKEQ(L.asc, 18);
  s = "x^(2n)";
  L = lay(s);
  CHECKEQ(parens(L), 0);                // hidden exponent parens
  CHECKEQ(OP(text(L, s, "2")).small, 1);
  CHECKEQ(OP(text(L, s, "n")).small, 1);
  CHECKEQ(L.width, 8 + 6 + 1 + 6);     // 2n: implicit multiplication gap
  s = "(x+1)^2";
  L = lay(s);
  CHECKEQ(parens(L), 2);                // the base keeps its parentheses
  CHECKEQ(OP(text(L, s, "2")).x, 5 + 28 + 5);
  CHECKEQ(OP(text(L, s, "2")).y + 3, -(14 * 45 / 100));
  s = "x^y^z";
  L = lay(s);
  CHECKEQ(OP(text(L, s, "z")).small, 1); // exponents of exponents stay small
  CHECK(OP(text(L, s, "z")).y < OP(text(L, s, "y")).y);
  CHECK(OP(text(L, s, "z")).x > OP(text(L, s, "y")).x);
  s = "2^-3";
  L = lay(s);
  CHECKEQ(OP(text(L, s, "-")).small, 1);
  CHECKEQ(OP(text(L, s, "3")).small, 1);
  s = "x**2";
  L = lay(s);
  CHECKEQ(OP(text(L, s, "2")).small, 1);
  CHECKEQ(L.width, 14);
  s = "e^(x)";
  L = lay(s);
  CHECKEQ(parens(L), 0);
  CHECKEQ(OP(text(L, s, "x")).small, 1);
}

static void t_radicals() {
  const char * s = "sqrt(x)";
  mi_layout L = lay(s);
  const mi_op & r = OP(opc(L, MI_SQRT)), & x = OP(text(L, s, "x"));
  CHECKEQ(cnt(L, MI_SQRT), 1);
  CHECKEQ(parens(L), 0);
  CHECK(!text(L, s, "sqrt"));
  CHECKEQ(x.x, r.x + 7);                // after the 6 px sign
  CHECKEQ(r.y, -14 - 2);                // overbar 1 px above the content
  CHECKEQ(r.y + r.h, 4);
  CHECKEQ(L.width, 16);
  s = "surd(x,3)";
  L = lay(s);
  const mi_op & r2 = OP(opc(L, MI_SQRT)), & n = OP(text(L, s, "3"));
  CHECKEQ(cnt(L, MI_SQRT), 1);
  CHECKEQ(n.small, 1);
  CHECK(n.x + n.w <= r2.x + 6);         // index over the left part of the sign
  CHECK(n.y + 3 < 0 && n.y + 3 > r2.y); // raised, but lower than the overbar
  CHECKEQ(OP(text(L, s, "x")).x, r2.x + 7);
  CHECKEQ(parens(L), 0);
}

static void t_abs() {
  const char * s = "abs(x-1)";
  mi_layout L = lay(s);
  const mi_op & b1 = OP(opc(L, MI_BAR, 0)), & b2 = OP(opc(L, MI_BAR, 1));
  CHECKEQ(cnt(L, MI_BAR), 2);
  CHECKEQ(parens(L), 0);
  CHECK(b1.x + b1.w < OP(text(L, s, "x")).x);
  CHECK(OP(text(L, s, "1")).x + 8 <= b2.x);
  CHECKEQ(b1.h, 18);
  CHECKEQ(b2.h, 18);
  L = lay("abs(1/x)");
  CHECKEQ(OP(opc(L, MI_BAR)).h, L.asc + L.desc); // as tall as the content
}

static void t_integral() {
  const char * s = "integrate(x*sin(x),x)";
  mi_layout L = lay(s);
  const mi_op & sg = OP(opc(L, MI_INTEGRAL)), & d = OP(text(L, s, "d"));
  const mi_op & var = OP(text(L, s, "x", 2));
  CHECKEQ(cnt(L, MI_INTEGRAL), 1);
  CHECKEQ(sg.h, 18 + 4);                // content height + 4
  CHECKEQ(sg.w, 6);
  CHECK(d.lit != 0);
  CHECKEQ(var.x, d.x + 8);              // d then the variable from the buffer
  CHECKEQ(var.pos, 19);
  CHECKEQ(cnt(L, MI_DOT), 1);
  CHECKEQ(parens(L), 2);                // only sin( )
  CHECK(!text(L, s, "integrate"));
  CHECK(OP(text(L, s, "sin")).x > sg.x + 6 && OP(text(L, s, "sin")).x < d.x);
  CHECKEQ(d.x, OP(opc(L, MI_RPAREN)).x + 5 + 2); // 2 px gap after f
  CHECKEQ(lay("int(x,x)").ops.size(), lay("integrate(x,x)").ops.size());
}

static void t_defint() {
  const char * s = "integrate(x^2,x,0,1)";
  mi_layout L = lay(s);
  const mi_op & sg = OP(opc(L, MI_INTEGRAL)), & lo = OP(text(L, s, "0")), & hi = OP(text(L, s, "1"));
  CHECKEQ(lo.small, 1); CHECKEQ(hi.small, 1);
  CHECK(hi.y < lo.y);
  CHECK(hi.x >= sg.x + 6 && lo.x >= sg.x + 6);
  CHECK(hi.y - 9 <= sg.y + 1);          // upper bound at the top right
  CHECK(lo.y + 3 >= sg.y + sg.h - 1);   // lower bound at the bottom right
  CHECKEQ(sg.h, 18 + 4 + 4);            // x^2 is 22 tall
  CHECK(OP(text(L, s, "x", 0)).x >= hi.x + hi.w);
  CHECKEQ(cnt(L, MI_BOX), 0);
}

static void t_diff() {
  const char * s = "diff(x^3,x)";
  mi_layout L = lay(s);
  const mi_op & bar = OP(opc(L, MI_HLINE)), & d1 = OP(text(L, s, "d", 0)), & d2 = OP(text(L, s, "d", 1));
  const mi_op & var = OP(text(L, s, "x", 1));
  CHECKEQ(cnt(L, MI_HLINE), 1);
  CHECKEQ(parens(L), 0);                // x^3 is not a sum
  CHECK(d1.y < bar.y && d2.y > bar.y);  // d over d x
  CHECKEQ(var.x, d2.x + 8);
  CHECKEQ(var.y, d2.y);
  CHECKEQ(d1.small + d2.small + var.small, 0); // big font
  CHECK(OP(text(L, s, "x", 0)).x > bar.x + bar.w);
  s = "diff(x^2+1,x)";
  L = lay(s);
  CHECKEQ(cnt(L, MI_LPAREN), 1);
  CHECKEQ(cnt(L, MI_RPAREN), 1);
  CHECKEQ(OP(opc(L, MI_LPAREN)).pos, -1); // belongs to no buffer chars
  CHECK(OP(opc(L, MI_LPAREN)).x < OP(text(L, s, "x")).x);
  CHECKEQ(parens(lay("diff(sin(x),x)")), 2);
}

static void t_sum() {
  const char * s = "sum(k^2,k,1,n)";
  mi_layout L = lay(s);
  const mi_op & sg = OP(opc(L, MI_SIGMA)), & eq = OP(text(L, s, "="));
  const mi_op & hi = OP(text(L, s, "n")), & lo = OP(text(L, s, "1")), & kv = OP(textsm(L, s, "k", 1));
  const mi_op & kf = OP(textsm(L, s, "k", 0));
  CHECKEQ(cnt(L, MI_SIGMA), 1);
  CHECKEQ(sg.h, 2 * 14);
  CHECK(eq.lit != 0);
  CHECKEQ(eq.small + hi.small + lo.small + kv.small, 4);
  CHECK(hi.y + 3 <= sg.y);              // b above the sigma
  CHECK(lo.y - 9 >= sg.y + sg.h && kv.y == lo.y); // k=a below
  CHECK(kv.x < eq.x && eq.x < lo.x);
  CHECK(kf.x >= sg.x + sg.w);           // then f
  CHECKEQ(OP(text(L, s, "2")).small, 1);
}

static void t_limit() {
  const char * s = "limit(sin(x)/x,x,0)";
  mi_layout L = lay(s);
  const mi_op & lim = OP(text(L, s, "lim")), & ar = OP(text(L, s, "\x1e"));
  const mi_op & v = OP(textsm(L, s, "x", 1)), & a = OP(text(L, s, "0"));
  CHECK(lim.lit != 0 && lim.small == 0);
  CHECKEQ(lim.w, 24);
  CHECK(ar.lit != 0 && ar.small == 1);
  CHECKEQ(ar.w, 6);                     // arrow glyph adv wide
  CHECK(v.y - 9 >= lim.y + 4);          // below lim
  CHECK(v.x < ar.x && ar.x < a.x && a.small == 1);
  CHECK(OP(opc(L, MI_HLINE)).x > lim.x + lim.w);
}

static void t_exp_pi() {
  const char * s = "exp(x)";
  mi_layout L = lay(s);
  CHECK(text(L, s, "e") && OP(text(L, s, "e")).lit);
  CHECKEQ(OP(text(L, s, "x")).small, 1);
  CHECKEQ(OP(text(L, s, "x")).y + 3, -(14 * 45 / 100));
  CHECKEQ(parens(L), 0);
  s = "pi*r^2";
  L = lay(s);
  CHECK(text(L, s, "pi") && OP(text(L, s, "pi")).lit);
  CHECKEQ(OP(text(L, s, "pi")).w, 8);   // one special glyph
  CHECKEQ(cnt(L, MI_DOT), 1);
  CHECKEQ(OP(text(L, s, "2")).small, 1);
  L = lay("infinity");
  CHECK(text(L, "infinity", "oo"));
  CHECKEQ(L.width, 8);
  L = lay("pi2");
  CHECK(text(L, "pi2", "pi2") && !OP(text(L, "pi2", "pi2")).lit);
}

static void t_operators() {
  mi_layout L = lay("a+b");
  CHECKEQ(OP(text(L, "a+b", "+")).x, 8 + 2);
  CHECKEQ(OP(text(L, "a+b", "b")).x, 8 + 2 + 8 + 2);
  L = lay("a<=b");
  CHECKEQ(OP(text(L, "a<=b", "<=")).w, 16);
  CHECKEQ(OP(text(L, "a<=b", "b")).x, 8 + 2 + 16 + 2);
  const char * rel[] = {"a=b", "a<b", "a>b", "a>=b", "a==b", "a!=b", "a:=b", "a=>b", "a->b", "a-b"};
  for (int i = 0; i < 10; ++i) {
    L = lay(rel[i]);
    int w = (slen(rel[i]) - 2) * 8;
    CHECKEQ(OP(text(L, rel[i], "b")).x, 8 + 2 + w + 2);
  }
  L = lay("a,b");
  CHECKEQ(OP(text(L, "a,b", "b")).x, 8 + 8 + 4); // comma followed by adv/2
  L = lay("2x");
  CHECKEQ(OP(text(L, "2x", "x")).x, 9);  // implicit multiplication: 1 px
  L = lay("-x");
  CHECKEQ(OP(text(L, "-x", "x")).x, 8);  // unary minus: no gap
  L = lay("a*b");
  const mi_op & dot = OP(opc(L, MI_DOT));
  CHECKEQ(dot.x, 8); CHECKEQ(dot.w, 8 / 2 + 2);
  CHECKEQ(2 * dot.y + dot.h, 2 * -(14 / 4)); // centered at mid x-height
  CHECKEQ(OP(text(L, "a*b", "b")).x, 14);
  L = lay("x!+n'");
  CHECKEQ(OP(text(L, "x!+n'", "!")).x, 8);
  CHECKEQ(OP(text(L, "x!+n'", "'")).x, 16 + 2 + 8 + 2 + 8);
}

static void t_blanks_utf8_strings() {
  mi_layout L = lay("x + 1");
  CHECKEQ(OP(text(L, "x + 1", "+")).x, 8 + 4 + 2);
  CHECKEQ(OP(text(L, "x + 1", "1")).x, 8 + 4 + 2 + 8 + 2 + 4);
  CHECKEQ(L.width, 36);
  CHECKEQ(lay(" x").width, 12);
  CHECKEQ(lay("x ").width, 12);
  const char * u = "\xce\xb8+1"; // theta, 2 bytes
  L = lay(u);
  CHECKEQ(OP(opc(L, MI_TEXT)).w, 8);
  CHECKEQ(OP(opc(L, MI_TEXT)).len, 2);
  CHECKEQ(OP(text(L, u, "+")).x, 10);
  CHECK(same(loc(u, 1), loc(u, 0)));    // inside the glyph
  const char * q = "\"a\\\"b\"+1";
  L = lay(q);
  CHECKEQ(OP(opc(L, MI_TEXT)).len, 6);
  CHECKEQ(OP(text(L, q, "+")).x, 48 + 2);
  L = lay("\"abc");
  CHECKEQ(cnt(L, MI_TEXT), 1);
}

static void t_unbalanced() {
  mi_layout L = lay("(x+1");
  CHECKEQ(cnt(L, MI_LPAREN), 1); CHECKEQ(cnt(L, MI_RPAREN), 0);
  L = lay("x)");
  CHECKEQ(parens(L), 0);
  CHECK(text(L, "x)", ")") != 0);
  CHECKEQ(cnt(L, MI_BOX), 0);
  L = lay(")");
  CHECKEQ(cnt(L, MI_BOX), 0); CHECKEQ(cnt(L, MI_TEXT), 1);
  L = lay("sqrt(x");                     // unclosed special call
  CHECKEQ(cnt(L, MI_SQRT), 1);
  L = lay("((x)");
  CHECKEQ(cnt(L, MI_LPAREN), 2); CHECKEQ(cnt(L, MI_RPAREN), 1);
  L = lay("[1,(2]");                     // ] closes the bracket and the inner (
  CHECKEQ(cnt(L, MI_LBRACKET), 1); CHECKEQ(cnt(L, MI_RBRACKET), 1);
  CHECKEQ(cnt(L, MI_LPAREN), 1); CHECKEQ(cnt(L, MI_RPAREN), 0);
  L = lay("(x]");
  CHECK(text(L, "(x]", "]") != 0);
  L = lay("(x+1)/(x-1");                 // unclosed denominator runs to the end
  CHECKEQ(parens(L), 0); CHECKEQ(cnt(L, MI_HLINE), 1);
}

static void t_boxes() {
  mi_layout L = lay("2+");
  CHECKEQ(cnt(L, MI_BOX), 1);
  CHECK(OP(opc(L, MI_BOX)).x >= 20);
  L = lay("x/");
  CHECKEQ(cnt(L, MI_BOX), 1);
  CHECK(OP(opc(L, MI_BOX)).y > OP(opc(L, MI_HLINE)).y);
  L = lay("x^");
  CHECKEQ(cnt(L, MI_BOX), 1); CHECKEQ(OP(opc(L, MI_BOX)).small, 1);
  CHECKEQ(cnt(lay("*2"), MI_BOX), 1);
  CHECKEQ(cnt(lay("/"), MI_BOX), 2);
  CHECKEQ(cnt(lay("sqrt()"), MI_BOX), 1);
  CHECKEQ(cnt(lay("f()"), MI_BOX), 0);
  CHECKEQ(cnt(lay("[]"), MI_BOX), 0);
  CHECKEQ(cnt(lay("{}"), MI_BOX), 0);
  CHECKEQ(cnt(lay("()"), MI_BOX), 1);
  CHECKEQ(cnt(lay("integrate(,x)"), MI_BOX), 1);
  CHECKEQ(cnt(lay("f(,)"), MI_BOX), 2);
  CHECKEQ(cnt(lay("2+;"), MI_BOX), 1);
  CHECKEQ(cnt(lay("x;"), MI_BOX), 0);
  CHECKEQ(cnt(lay("-"), MI_BOX), 1);
}

static void t_calls_brackets() {
  const char * s = "f(x,y)";
  mi_layout L = lay(s);
  CHECK(text(L, s, "f") && text(L, s, ","));
  CHECKEQ(cnt(L, MI_LPAREN), 1); CHECKEQ(cnt(L, MI_RPAREN), 1);
  CHECKEQ(OP(opc(L, MI_LPAREN)).w, 8 / 2 + 1);
  L = lay("sqrt(x,y)");                  // wrong arg count: normal call
  CHECK(text(L, "sqrt(x,y)", "sqrt")); CHECKEQ(cnt(L, MI_SQRT), 0); CHECKEQ(parens(L), 2);
  L = lay("integrate(x)");
  CHECK(text(L, "integrate(x)", "integrate")); CHECKEQ(cnt(L, MI_INTEGRAL), 0);
  L = lay("sin(x)^2");
  CHECKEQ(parens(L), 2); CHECKEQ(OP(text(L, "sin(x)^2", "2")).small, 1);
  L = lay("[1,2]");
  CHECKEQ(cnt(L, MI_LBRACKET), 1); CHECKEQ(cnt(L, MI_RBRACKET), 1);
  L = lay("{a}");
  CHECKEQ(cnt(L, MI_LBRACE), 1); CHECKEQ(cnt(L, MI_RBRACE), 1);
  L = lay("[(1+2)/3]");
  CHECKEQ(cnt(L, MI_LBRACKET), 1); CHECKEQ(cnt(L, MI_LPAREN), 0);
  L = lay("(1/2)");                      // parens as tall as the content
  CHECKEQ(OP(opc(L, MI_LPAREN)).h, L.asc + L.desc);
}

static void t_limits() {
  std::string s(24, '('); s += "x"; s += std::string(24, ')');
  mi_layout L = lay(s.c_str());
  CHECKEQ(parens(L), 48);                // 24 levels: still structured
  s = std::string(30, '('); s += "x"; s += std::string(30, ')');
  L = lay(s.c_str());
  CHECK(parens(L) < 60);                 // deeper: flat text from there on
  int longest = 0;
  for (size_t i = 0; i < L.ops.size(); ++i) if (L.ops[i].code == MI_TEXT && L.ops[i].len > longest) longest = L.ops[i].len;
  CHECK(longest >= 6);
  s = "";
  for (int i = 0; i < 300; ++i) s += "1+";
  s += "1";
  L = lay(s.c_str(), (int)s.size());
  CHECK(L.ops.size() <= 400);
  CHECK(inb(L));
  CHECKEQ(L.cx, L.width);
  s = "";
  for (int i = 0; i < 300; ++i) s += "1/";
  s += "2";
  L = lay(s.c_str(), 1);
  CHECK(L.ops.size() <= 400 && inb(L));
  s = std::string(2000, '-');
  L = lay(s.c_str(), 1000);
  CHECK(inb(L) && L.width == 16000);
  CHECKEQ(mv(s.c_str(), 1000, RIGHT), 1001);
}

// ---------------------------------------------------------------- caret
static void t_caret() {
  const char * s = "(x+1)/(x-1)";
  CHECK(isloc(loc(s, 0), 0, -14, 18));   // before the fraction, main line
  CHECK(isloc(loc(s, 1), 2, -24, 18));   // just inside the hidden ( : numerator left edge
  CHECK(isloc(loc(s, 4), 30, -24, 18));  // before the hidden ) : numerator right edge
  CHECK(same(loc(s, 5), loc(s, 4)));     // between ) and / : end of the numerator
  CHECK(same(loc(s, 6), loc(s, 4)));
  CHECK(isloc(loc(s, 7), 2, -1, 18));
  CHECK(isloc(loc(s, 11), 32, -14, 18)); // after the fraction
  s = "x^2";
  CHECK(isloc(loc(s, 1), 8, -14, 18));
  CHECK(isloc(loc(s, 2), 8, -18, 12));   // small font height in the exponent
  CHECK(isloc(loc(s, 3), 14, -18, 12));
  s = "sqrt(x)";
  for (int p = 1; p <= 4; ++p) CHECK(same(loc(s, p), loc(s, 0)));
  CHECK(isloc(loc(s, 5), 7, -14, 18));
  CHECK(isloc(loc(s, 7), 16, -14, 18));
  mi_layout L = lay("", 0);
  CHECKEQ(L.width, 0); CHECKEQ(L.asc, 14); CHECKEQ(L.desc, 4);
  CHECK(L.cx == 0 && L.cy == -14 && L.ch == 18);
  L = lay("12", 99);                     // clamped
  CHECKEQ(L.cx, 16);
  L = lay("12", -1);
  CHECK(L.cx == 0 && L.cy == 0 && L.ch == 0);
  CHECK(same(loc("integrate(x,x)", 3), loc("integrate(x,x)", 0)));
  CHECK(!same(loc("integrate(x,t)", 14), loc("integrate(x,t)", 13))); // end of t vs after
}

// ---------------------------------------------------------------- navigation
static void t_nav_fraction() {
  const char * s = "(x+1)/(x-1)";
  CHECK(walk(s, 0, RIGHT, 10) == "1,2,3,4,7,8,9,10,11,11");
  CHECK(walk(s, 11, LEFT, 10) == "10,9,8,7,4,3,2,1,0,0");
  CHECKEQ(mv(s, 2, DOWN), 8);            // numerator -> denominator
  CHECKEQ(mv(s, 8, UP), 2);              // denominator -> numerator
  CHECKEQ(mv(s, 4, DOWN), 10);
  CHECKEQ(mv(s, 2, UP), -1);
  CHECKEQ(mv(s, 8, DOWN), -1);
  CHECKEQ(mv(s, 0, UP), -1);
  CHECKEQ(mv(s, 11, DOWN), -1);
  s = "1/(1+1/x)";
  CHECK(walk(s, 0, RIGHT, 8) == "1,3,4,5,6,7,8,9");
  CHECK(walk(s, 9, LEFT, 8) == "8,7,6,5,4,3,1,0");
  CHECKEQ(mv(s, 3, UP), 0);              // denominator base line -> numerator
  CHECKEQ(mv(s, 5, DOWN), 7);            // inner numerator -> inner denominator
  CHECKEQ(mv(s, 7, UP), 5);
  CHECK(mv(s, 5, UP) == 0 || mv(s, 5, UP) == 1); // then out to the outer numerator
  CHECKEQ(mv(s, 8, DOWN), -1);
  CHECK(mv(s, 0, DOWN) >= 3 && mv(s, 0, DOWN) <= 8);
}

static void t_nav_power() {
  const char * s = "x^2+1";
  CHECKEQ(mv(s, 3, DOWN), 1);            // exponent -> base line
  CHECKEQ(mv(s, 2, DOWN), 1);
  CHECKEQ(mv(s, 3, UP), -1);
  for (int p = 0; p <= 5; ++p)           // the main line, base line included, is top level
    if (p != 2 && p != 3) { CHECKEQ(mv(s, p, UP), -1); CHECKEQ(mv(s, p, DOWN), -1); }
  CHECK(walk(s, 0, RIGHT, 5) == "1,2,3,4,5");
  s = "x^(2)+1";
  CHECKEQ(mv(s, 4, DOWN), 5);            // after the power
  CHECKEQ(mv(s, 3, DOWN), 1);
  CHECKEQ(mv(s, 1, RIGHT), 3);           // skips the structure position between ^ and (
  CHECKEQ(mv(s, 3, LEFT), 1);
  CHECKEQ(mv(s, 4, RIGHT), 5);
  CHECKEQ(mv(s, 5, UP), -1);
  s = "(x^2+1)/(x-1)";                  // exponent inside a numerator
  CHECKEQ(mv(s, 4, DOWN), 2);            // -> base line of the numerator, not the denominator
  CHECK(mv(s, 2, DOWN) >= 9 && mv(s, 2, DOWN) <= 12); // numerator -> denominator
  s = "2^(1/x)";
  CHECKEQ(mv(s, 3, DOWN), 5);            // fraction inside the exponent
  CHECK(mv(s, 6, DOWN) == 7 || mv(s, 6, DOWN) == 1);
}

static void t_nav_sqrt_abs() {
  const char * s = "sqrt(x)+1";
  CHECK(walk(s, 0, RIGHT, 5) == "5,6,7,8,9");
  CHECKEQ(mv(s, 5, LEFT), 0);
  CHECKEQ(mv(s, 5, UP), -1); CHECKEQ(mv(s, 6, DOWN), -1);
  s = "surd(x,3)";
  int u = mv(s, 5, UP);
  CHECK(u == 7 || u == 8);               // radicand -> index
  CHECKEQ(mv(s, 7, DOWN), 5);
  CHECKEQ(mv(s, 7, UP), -1);
  s = "abs(x)";
  CHECK(walk(s, 0, RIGHT, 3) == "4,5,6");
}

static void t_nav_integral() {
  const char * s = "integrate(f,x,a,b)"; // slots f [10,11] x [12,13] a [14,15] b [16,17]
  CHECK(walk(s, 0, RIGHT, 10) == "10,11,12,13,14,15,16,17,18,18");
  CHECKEQ(mv(s, 10, LEFT), 0);
  int u = mv(s, 10, UP), d = mv(s, 10, DOWN);
  CHECK(u == 16 || u == 17);             // f -> upper bound
  CHECK(d == 14 || d == 15);             // f -> lower bound
  u = mv(s, 14, UP); d = mv(s, 16, DOWN);
  CHECK(u == 16 || u == 17);             // lower bound -> upper bound
  CHECK(d == 14 || d == 15);
  CHECKEQ(mv(s, 16, UP), -1);
  CHECKEQ(mv(s, 14, DOWN), -1);
  CHECK(mv(s, 12, UP) >= 16 && mv(s, 12, DOWN) >= 14 && mv(s, 12, DOWN) <= 15);
  s = "integrate(sin(x),x,0,1)";        // f [10,16] 0 [19,20] 1 [21,22]
  CHECK(mv(s, 13, UP) >= 21);
  CHECK(mv(s, 13, DOWN) >= 19 && mv(s, 13, DOWN) <= 20);
  s = "integrate(x^2,x,0,1)";           // f [10,13] x [14,15] 0 [16,17] 1 [18,19]
  CHECKEQ(mv(s, 11, UP), 12);            // the base line of x^2 first goes into its exponent
  CHECK(keys(s, 11, "UU") >= 18);        // then to the upper bound
  CHECK(mv(s, 16, UP) >= 18);
  CHECK(mv(s, 10, DOWN) >= 16 && mv(s, 10, DOWN) <= 17);
}

static void t_nav_sum_limit() {
  const char * s = "sum(k,k,1,n)";     // f [4,5] k [6,7] 1 [8,9] n [10,11]
  int u = mv(s, 4, UP), d = mv(s, 4, DOWN);
  CHECK(u == 10 || u == 11);             // f -> upper limit
  CHECK(d >= 6 && d <= 9);               // f -> k=1 row
  CHECK(mv(s, 10, DOWN) >= 4 && mv(s, 10, DOWN) <= 5);
  s = "sum(k^2,k,1,n)";                 // f [4,7] k [8,9] 1 [10,11] n [12,13]
  u = mv(s, 5, UP);
  CHECK(u == 12 || u == 13);             // from the base line of k^2 to the upper limit
  d = mv(s, 5, DOWN);
  CHECK(d >= 8 && d <= 11);
  CHECK(walk(s, 0, RIGHT, 11) == "4,5,6,7,8,9,10,11,12,13,14");
  s = "limit(sin(x)/x,x,0)";            // f [6,14] x [15,16] 0 [17,18]
  CHECKEQ(mv(s, 6, DOWN), 13);
  d = mv(s, 13, DOWN);
  CHECK(d >= 15 && d <= 18);             // f's denominator -> x->0 below lim
  CHECKEQ(mv(s, 0, DOWN), -1);
  s = "limit(x,x,0)";                   // f [6,7] x [8,9] 0 [10,11]
  CHECK(mv(s, 8, UP) >= 6 && mv(s, 8, UP) <= 7);
  d = mv(s, 6, DOWN);
  CHECK(d >= 8 && d <= 11);
  CHECKEQ(mv(s, 6, UP), -1);
}

static void t_nav_flat() {
  const char * s = "1+2";
  for (int p = 0; p <= 3; ++p) { CHECKEQ(mv(s, p, UP), -1); CHECKEQ(mv(s, p, DOWN), -1); }
  s = "f(x)+[1,2]";
  bool ok = true;
  for (int p = 0; p <= slen(s); ++p) ok = ok && mv(s, p, UP) == -1 && mv(s, p, DOWN) == -1;
  CHECK(ok);
  CHECK(walk("1+2", 0, RIGHT, 4) == "1,2,3,3");
  CHECKEQ(mv("", 0, LEFT), 0); CHECKEQ(mv("", 0, RIGHT), 0); CHECKEQ(mv("", 0, UP), -1);
  CHECKEQ(mv("12", 1, 7), 1);           // unknown direction: no move
}

// ---------------------------------------------------------------- backspace
static int bs(const char * s, int c, int & f, int & t) { return mi_backspace(s, slen(s), c, f, t); }
#define BS(s, c, rf, rt, rr) do { int f_, t_, r_ = bs(s, c, f_, t_); CHECKEQ(f_, rf); CHECKEQ(t_, rt); CHECKEQ(r_, rr); } while (0)
static void t_backspace() {
  BS("12", 2, 1, 2, 1);                  // visible char
  BS("\xce\xb8", 2, 0, 2, 0);            // one UTF-8 glyph
  BS("2pi", 3, 1, 3, 1);                 // pi is one glyph
  BS("x y", 2, 1, 2, 1);                 // blank
  BS("(x)", 1, 0, 1, 0);                 // visible paren
  BS("[1,2]", 3, 2, 3, 2);               // visible comma
  BS("12", 0, 0, 0, 0);                  // position 0: nothing
  BS("(x+1)/(x-1)", 7, 7, 7, 4);         // hidden ( of a non-empty fraction: like LEFT
  BS("sqrt(x)", 5, 5, 5, 0);
  BS("x^(2)", 3, 3, 3, 1);
  BS("x/", 2, 2, 2, 1);
  BS("integrate(x,t)", 12, 12, 12, 11);
  BS("integrate(,t)", 10, 10, 10, 0);    // t is not the default variable
  BS("sqrt()", 5, 0, 6, 0);              // empty templates: delete them whole
  BS("()/()", 1, 0, 5, 0);
  BS("()/()", 4, 0, 5, 0);
  BS("x^()", 3, 1, 4, 1);
  BS("x^", 2, 1, 2, 1);
  BS("abs()", 4, 0, 5, 0);
  BS("integrate(,x)", 10, 0, 13, 0);
  BS("integrate(,x,,)", 13, 0, 15, 0);
  BS("diff(,x)", 5, 0, 8, 0);
  BS("sum(,k,,)", 4, 0, 9, 0);
  BS("limit(,x,)", 6, 0, 10, 0);
  BS("surd(,)", 5, 0, 7, 0);
  BS("e^()", 3, 1, 4, 1);
  BS("2+sqrt()", 7, 2, 8, 2);            // the empty template after 2+
  BS("2+()/()", 3, 2, 7, 2);
  BS("2+ abs()", 7, 3, 8, 3);
}

// ---------------------------------------------------------------- templates
static void t_templates() {
  const char * pre[] = {"", "2+"};
  for (int k = 0; k < MI_T_COUNT; ++k)
    for (int i = 0; i < 2; ++i) {
      const mi_template & t = mi_get_template(k);
      std::string s = std::string(pre[i]) + t.text;
      int c = slen(pre[i]) + t.caret;
      mi_layout L = lay(s.c_str(), c);
      bool ok = cnt(L, MI_BOX) >= 1 && inb(L);
      if (k == MI_T_SQ) ok = ok && L.ch == 12 && L.cy < -14; // caret in the exponent
      else ok = ok && caret_in_box(L);
      if (!ok) printf("    template %d \"%s\" caret %d: boxes %d caret (%d,%d,%d)\n", k, s.c_str(), c,
                      cnt(L, MI_BOX), L.cx, L.cy, L.ch);
      CHECK(ok);
    }
  CHECKEQ(mi_get_template(-1).caret, 1); // out of range: FRAC
  CHECKEQ(mi_get_template(MI_T_COUNT).caret, 1);
  CHECKEQ(cnt(lay("integrate(,x,,)"), MI_BOX), 3);
  CHECKEQ(cnt(lay("sum(,k,,)"), MI_BOX), 3);
  CHECKEQ(cnt(lay("limit(,x,)"), MI_BOX), 2);
}

// ---------------------------------------------------------------- robustness
static unsigned g_rng = 12345;
static unsigned rnd(unsigned n) { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng % n; }

// The fuzzed buffer lives right before (or right after) an inaccessible page on Windows,
// so reading s[len] or s[-1] faults; with ASan an exact-size heap block does the same.
static const char * hold(const std::string & s, int variant) {
#if defined(_WIN32) && !defined(__SANITIZE_ADDRESS__)
  static char * base = 0;
  static size_t page = 0;
  if (!base) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    page = si.dwPageSize;
    base = (char *)VirtualAlloc(0, 4 * page, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    DWORD old;
    VirtualProtect(base, page, PAGE_NOACCESS, &old);
    VirtualProtect(base + 3 * page, page, PAGE_NOACCESS, &old);
  }
  char * p = variant & 1 ? base + page : base + 3 * page - s.size();
  memcpy(p, s.data(), s.size());
  return p;
#else
  static char * held = 0; // exact-size block: ASan reports any read outside [0, len)
  (void)variant;
  delete[] held;
  held = s.size() ? new char[s.size()] : 0;
  if (held) memcpy(held, s.data(), s.size());
  return held;
#endif
}

static int g_bad, g_maxops, g_maxlen;
static const mi_metrics * FM = &mi_default_metrics; // metrics of the fuzz pass
static void fuzz_one(const std::string & str, int variant) {
  const mi_metrics & MM = *FM;
  const char * s = hold(str, variant);
  int n = (int)str.size(), c = (int)rnd(n + 3) - 1, p, q, r, f, t;
  mi_layout L, L2;
  mi_build(s, n, c, MM, L);
  if (L.ops.size() > 416 || L.width < 0 || (c >= 0 && !inb(L))) ++g_bad;
  if ((int)L.ops.size() > g_maxops) g_maxops = (int)L.ops.size();
  if (n > g_maxlen) g_maxlen = n;
  p = (int)rnd(n + 1);
  for (int d = 0; d < 4; ++d) {
    q = mi_move(s, n, p, d, MM);
    if (q < (d < 2 ? 0 : -1) || q > n) ++g_bad;
  }
  q = mi_move(s, n, p, LEFT, MM); // left then right returns to an equivalent location
  if (q != p) {
    r = mi_move(s, n, q, RIGHT, MM);
    mi_build(s, n, p, MM, L); mi_build(s, n, r, MM, L2);
    if (L.cx != L2.cx || L.cy != L2.cy || L.ch != L2.ch) ++g_bad;
  }
  q = mi_move(s, n, p, RIGHT, MM); // and right then left
  if (q != p) {
    r = mi_move(s, n, q, LEFT, MM);
    mi_build(s, n, p, MM, L); mi_build(s, n, r, MM, L2);
    if (L.cx != L2.cx || L.cy != L2.cy || L.ch != L2.ch) ++g_bad;
  }
  r = mi_backspace(s, n, (int)rnd(n + 2), f, t, MM);
  if (f < 0 || t < f || t > n || r < 0 || r > n) ++g_bad;
}

static void t_fuzz() {
  static const char * tok[] = {"x", "y", "1", "2", ".", "+", "-", "*", "/", "^", "(", ")", ",", "=",
                               "sqrt", "integrate", "diff", "sum", "limit", "abs", "[", "]", "\"", " "};
  static const char * ext[] = {"**", ":=", "!", "'", "pi", "e", "surd", "exp", "int", "{", "}", ";",
                               "\xce\xb8", "\x80", "\\", "\t", "1e5", "->", "<=", "#", "infinity"};
  int e0 = mi_test_errors;
  g_bad = 0;
  for (int i = 0; i < 20000; ++i) {
    std::string s;
    int m = (int)rnd(i % 50 == 0 ? 200 : 24);
    for (int j = 0; j < m; ++j) s += tok[rnd(24)];
    fuzz_one(s, i);
  }
  CHECKEQ(g_bad, 0);
  g_bad = 0;
  for (int i = 0; i < 5000; ++i) {     // wider alphabet
    std::string s;
    int m = (int)rnd(30);
    for (int j = 0; j < m; ++j) s += rnd(3) ? tok[rnd(24)] : ext[rnd(21)];
    fuzz_one(s, i);
  }
  CHECKEQ(g_bad, 0);
  g_bad = 0;
  for (int i = 0; i < 200; ++i) {      // long and deep buffers: depth / node / op budgets
    std::string s;
    int m = 200 + (int)rnd(1500);
    const char * deep[] = {"(", "x^", "sqrt(", "1/", "-", "[", "integrate(", "2+"};
    for (int j = 0; j < m; ++j) s += rnd(2) ? deep[rnd(8)] : tok[rnd(24)];
    fuzz_one(s, i);
  }
  CHECKEQ(g_bad, 0);
  CHECKEQ(mi_test_errors - e0, 0);
  CHECK(g_maxops <= 400);
  printf("    25200 buffers (longest %d bytes), max ops %d\n", g_maxlen, g_maxops);
  hold(std::string(), 0); // release the last buffer
}

// ---------------------------------------------------------------- proportional fonts (Focus UI)
static int pw(const char * s, int n, int sm, int st) { // a proportional test font
  if (st == MI_SYM) {
    std::string l(s, n);
    int w = l == "pi" ? 9 : l == "oo" ? 11 : l == "lim" ? 20 : l == "S" ? 14 : l == "" ? 12 : 8;
    return sm ? w * 2 / 3 : w;
  }
  int w = 0;
  for (int i = 0; i < n; ++i)
    if (((unsigned char)s[i] & 0xc0) != 0x80) w += (st == MI_IT ? 7 : 6) + ((unsigned char)s[i] % 4);
  return sm ? w * 2 / 3 : w;
}
static const mi_metrics PM = {{16, 22, 7}, {10, 14, 4}, 6, pw, 2, 4, 18, 16, MI_F_IMPLDOT};
static void t_proportional() {
  mi_layout L;
  const char * s = "x+sin(y)";
  mi_build(s, slen(s), -1, PM, L);
  const mi_op * x = text(L, s, "x"), * sn = text(L, s, "sin"), * y = text(L, s, "y");
  CHECK(x && sn && y);
  if (x && sn && y) {
    CHECKEQ(x->style, MI_IT); CHECKEQ(sn->style, MI_UP); CHECKEQ(y->style, MI_IT);
    CHECKEQ(x->w, pw("x", 1, 0, MI_IT)); CHECKEQ(sn->w, pw("sin", 3, 0, MI_UP));
  }
  s = "2*x"; mi_build(s, slen(s), -1, PM, L); CHECKEQ(cnt(L, MI_DOT), 0);  // implicit
  s = "2*3"; mi_build(s, slen(s), -1, PM, L); CHECKEQ(cnt(L, MI_DOT), 1);  // kept before a digit
  s = "x*2"; mi_build(s, slen(s), -1, MM, L); CHECKEQ(cnt(L, MI_DOT), 1);  // classic metrics: always
  s = "1/2"; mi_build(s, slen(s), -1, PM, L);
  const mi_op * bar = opc(L, MI_HLINE);
  CHECK(bar && bar->h == 2);
  s = "abc"; // caret positions inside a name follow the glyph widths
  int last = -1;
  bool mono = true;
  for (int p = 0; p <= 3; ++p) { mi_build(s, 3, p, PM, L); mono = mono && L.cx > last; last = L.cx; }
  CHECK(mono);
  s = "pi"; mi_build(s, 2, -1, PM, L);
  const mi_op * pi = opc(L, MI_TEXT);
  CHECK(pi && pi->style == MI_SYM && pi->w == 9);
  int e0 = mi_test_errors; // fuzz with the proportional metrics
  FM = &PM; g_bad = 0;
  static const char * tok[] = {"x", "y", "1", "2", ".", "+", "-", "*", "/", "^", "(", ")", ",", "=",
                               "sqrt", "integrate", "diff", "sum", "limit", "abs", "pi", "infinity", "Î¸", " "};
  for (int i = 0; i < 6000; ++i) {
    std::string b;
    int m = (int)rnd(i % 50 == 0 ? 120 : 24);
    for (int j = 0; j < m; ++j) b += tok[rnd(24)];
    fuzz_one(b, i);
  }
  FM = &mi_default_metrics;
  CHECKEQ(g_bad, 0);
  CHECKEQ(mi_test_errors - e0, 0);
  hold(std::string(), 0);
}

// ---------------------------------------------------------------- corpus
static void t_corpus() {
  const char * c[] = {"(x+1)/(x-1)", "x^2+3x+1", "2^(3/2)", "sqrt(x^2+1)", "abs(x-1)",
                      "integrate(x*sin(x),x)", "integrate(x^2,x,0,1)", "diff(x^3,x)",
                      "sum(k^2,k,1,n)", "limit(sin(x)/x,x,0)", "f(x):=x^2", "[1,2,3]", "e^(x)",
                      "pi*r^2", "1/(1+1/x)"};
  for (int i = 0; i < 15; ++i) {
    const char * s = c[i];
    int n = slen(s);
    mi_layout L = lay(s);
    // sensible: no boxes, width between 2 and 12 px per buffer char, at most 5 lines tall
    bool ok = cnt(L, MI_BOX) == 0 && L.width >= 2 * n && L.width <= 12 * n && L.asc + L.desc <= 5 * 18;
    for (int p = 0; p <= n; ++p) ok = ok && inb(lay(s, p));
    if (!ok) printf("    corpus \"%s\": width %d asc %d desc %d boxes %d\n", s, L.width, L.asc, L.desc, cnt(L, MI_BOX));
    CHECK(ok);
  }
  mi_layout A = lay(c[6]), B = lay(c[6]); // deterministic
  bool eq = A.ops.size() == B.ops.size();
  for (size_t i = 0; eq && i < A.ops.size(); ++i) { // field by field: padding bytes are not part of the value
    const mi_op & a = A.ops[i], & b = B.ops[i];
    eq = a.code == b.code && a.small == b.small && a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h &&
         a.pos == b.pos && a.len == b.len && a.lit == b.lit;
  }
  CHECK(eq);
}

static void t_perf() { // report only: cost of one mi_build / mi_move on ~100 chars
  const char * s = "integrate(x^2*sin(x)/(1+x^2),x,0,pi)+sum(k^2/(k+1),k,1,n)-limit((1+1/n)^n,n,infinity)+sqrt(2)";
  int n = slen(s), N = 2000;
  mi_layout L;
  std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
  for (int i = 0; i < N; ++i) mi_build(s, n, i % (n + 1), MM, L);
  std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
  for (int i = 0; i < N; ++i) mi_move(s, n, i % (n + 1), i & 3, MM);
  std::chrono::steady_clock::time_point t2 = std::chrono::steady_clock::now();
  double b = std::chrono::duration<double, std::micro>(t1 - t0).count() / N;
  double m = std::chrono::duration<double, std::micro>(t2 - t1).count() / N;
  printf("    %d chars, %d ops: mi_build %.1f us, mi_move %.1f us (host, sanitizers on)\n", n, (int)L.ops.size(), b, m);
  CHECK(L.ops.size() > 20);
}

int main() {
  run_case("fraction: stacked, centered, hidden parens", t_fraction);
  run_case("fraction: nested, same font", t_fraction_nested);
  run_case("power: small raised exponent, hidden parens", t_power);
  run_case("radicals: sqrt, surd", t_radicals);
  run_case("abs: bars", t_abs);
  run_case("integrate: sign, f, d, var", t_integral);
  run_case("integrate: bounds", t_defint);
  run_case("diff: d/dx and parentheses", t_diff);
  run_case("sum: sigma with limits", t_sum);
  run_case("limit: lim with x->a", t_limit);
  run_case("exp, pi, infinity", t_exp_pi);
  run_case("operators and spacing", t_operators);
  run_case("blanks, UTF-8, strings", t_blanks_utf8_strings);
  run_case("unbalanced groups", t_unbalanced);
  run_case("placeholder boxes", t_boxes);
  run_case("calls and brackets", t_calls_brackets);
  run_case("depth / op limits", t_limits);
  run_case("caret locations", t_caret);
  run_case("navigation: fractions", t_nav_fraction);
  run_case("navigation: powers", t_nav_power);
  run_case("navigation: sqrt, surd, abs", t_nav_sqrt_abs);
  run_case("navigation: definite integral", t_nav_integral);
  run_case("navigation: sum, limit", t_nav_sum_limit);
  run_case("navigation: flat expressions", t_nav_flat);
  run_case("backspace", t_backspace);
  run_case("templates in \"\" and \"2+\"", t_templates);
  run_case("robustness: random buffers", t_fuzz);
  run_case("proportional fonts, styles, implicit *", t_proportional);
  run_case("corpus", t_corpus);
  run_case("performance (report)", t_perf);
  printf("\n%d cases, %d failed; %d checks, %d failed\n", g_cases, g_cases_failed, g_checks, g_fails);
  printf("%s\n", g_fails ? "FAIL" : "PASS");
  return g_fails ? 1 : 0;
}
