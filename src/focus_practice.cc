// focus_practice.cc - DeltaCE's practice mode (more > Practice): problems made up by topic (linear
// and quadratic equations, systems, exponential and log equations, derivatives, integrals,
// factoring, trig equations), the answer typed and checked (numerically: any equivalent form is
// right; an antiderivative through its derivative, a factored form must be a product), the right
// answer and its steps (the steps screen) when it is not. The problems come from patterns with
// random whole numbers chosen so that the answers are clean; giac computes what the screen shows.
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
using namespace giac;
extern giac::context * contextptr;
extern "C" void GetKey(int * key);
extern "C" volatile unsigned getkey_polls;
#define NOINL __attribute__((noinline))
int focus_prompt(const char * title, const char * label, std::string & s, bool numeric); // focus_menu.cc
static NOINL int key_() { int k; GetKey(&k); giac::ctrl_c = giac::interrupted = false; return k; }

enum { SB = 16, MB = 22, SBOT = UI_H - MB, Y0 = SB };
static unsigned char col(int c) { return ui_col(0, c); }
static NOINL int text(const ui_face * f, const char * s, int x, int base, int fg, int align = 0) {
  const int w = ui_text_width(f, s, -1);
  ui_draw_text(f, s, -1, align == 2 ? x - w : align == 1 ? x - w / 2 : x, base, ui_ramp(0, fg, UC_BG), 0);
  return w;
}
static NOINL void math(const char * s, int x, int base, int maxlv, int w, int fg) { // centered at x
  mi_layout L;
  const int lv = ui_math_fit(s, strlen(s), -1, maxlv, w, 60, MI_F_IMPLDOT, L);
  ui_math_draw(L, s, lv, x - L.width / 2, base, 0, fg, UC_BG, UC_ACC);
}

// ---------------------------------------------------------------- the problems
enum { K_VALUES, K_PAIR, K_DERIV, K_ANTI, K_FACTOR }; // how an answer is checked
static const char * const TOPIC[8] = {"Linear equations", "Quadratics", "Systems", "Exponents and logs",
                                      "Derivatives", "Integrals", "Factoring", "Trig equations"};
static const char * const TPREV[8] = {"3x+5=11", "x^2-5x+6=0", "[2x+y=5,x-y=1]", "2^(x+1)=16",
                                      "diff(x^3*sin(x),x)", "integrate(2x*cos(x^2),x)", "x^2+5x+6", "2*sin(x)=1"};
static const char * const ASK[8] = {"Solve for x", "Solve for x: every solution", "Solve the system", "Solve for x",
                                    "The derivative", "An antiderivative", "Factor", "Solve for x in [0, 2\xcf\x80)"}; // (π: math font only)
static int topic, kind, state, nright, ntotal, nprob; // state: 0 asking, 1 right, 2 wrong, 3 the steps seen
static unsigned long seed;
static std::string * cmd_, * show_, * yours_, * right_; // the command (steps), the problem shown, the answers
static vecteur * want_;                                   // the values (equations), or [f] (the others)

static NOINL int R(int a, int b) { // a whole number in [a, b]
  seed = seed * 1103515245UL + 12345UL;
  return a + (int)((seed >> 8) % (unsigned long)(b - a + 1));
}
static NOINL int Rnz(int a, int b) { int v; do v = R(a, b); while (!v); return v; }
// pattern p with %0..%5 the numbers v (negative ones in parentheses)
static NOINL std::string pat(const char * p, const int * v) {
  std::string s;
  for (; *p; ++p) {
    if (*p == '%' && p[1] >= '0' && p[1] <= '5') {
      const int n = v[*++p - '0'];
      if (n < 0) s += '(';
      s += print_INT_(n);
      if (n < 0) s += ')';
    } else s += *p;
  }
  return s;
}
static NOINL gen rd(const std::string & s) { return gen(s, contextptr); }
static NOINL gen ev(const gen & g) { giac::ctrl_c = giac::interrupted = false; return eval(g, 1, contextptr); }
static NOINL std::string pr(const gen & g) { return g.print(contextptr); }
// an equation "L=R" with each side evaluated as giac writes it (3*x+(-5) is 3*x-5, 1*x is x)
static NOINL std::string eqn(const std::string & s) {
  const gen e = rd(s);
  if (!e.is_symb_of_sommet(at_equal)) return pr(ev(e));
  return pr(ev(e._SYMBptr->feuille[0])) + "=" + pr(ev(e._SYMBptr->feuille[1])); // (giac would write =(x+27))
}
static NOINL void make() {
  int v[6];
  std::string & c = *cmd_, & sh = *show_;
  vecteur & w = *want_;
  w.clear();
  kind = K_VALUES;
  switch (topic) {
  case 0: { // a x + b = c x + d, x whole
    v[4] = R(-9, 9); v[0] = R(2, 9); do v[2] = R(-4, 4); while (v[2] == v[0]);
    v[1] = R(-12, 12); v[3] = (v[0] - v[2]) * v[4] + v[1];
    sh = eqn(pat("%0*x+%1=%2*x+%3", v)); w.push_back(v[4]);
    break;
  }
  case 1: { // a (x - r) (x - s) = 0 expanded
    static const char A[5] = {1, 1, 1, 2, 3};
    v[0] = A[R(0, 4)]; v[1] = R(-9, 9); v[2] = R(-9, 9);
    sh = pr(ev(rd(pat("expand(%0*(x-%1)*(x-%2))", v)))) + "=0";
    w.push_back(v[1]); if (v[2] != v[1]) w.push_back(v[2]);
    break;
  }
  case 2: { // a x + b y = e, c x + d y = f
    const int x = R(-6, 6), y = R(-6, 6);
    do { v[0] = Rnz(-5, 5); v[1] = Rnz(-5, 5); v[2] = Rnz(-5, 5); v[3] = Rnz(-5, 5); } while (v[0] * v[3] == v[1] * v[2]);
    v[4] = v[0] * x + v[1] * y; v[5] = v[2] * x + v[3] * y;
    sh = "[" + eqn(pat("%0*x+%1*y=%4", v)) + "," + eqn(pat("%2*x+%3*y=%5", v)) + "]";
    c = "solve(" + sh + ",[x,y])";
    w.push_back(x); w.push_back(y);
    kind = K_PAIR;
    break;
  }
  case 3: { // b^(x+k) = b^n, or log(x+a) = n
    if (R(0, 1)) {
      static const char B[3] = {2, 3, 5}, N[3] = {6, 4, 3};
      const int i = R(0, 2);
      v[0] = B[i]; v[1] = R(-3, 3); const int n = R(1, N[i]);
      v[2] = 1; for (int j = 0; j < n; ++j) v[2] *= v[0];
      sh = eqn(pat("%0^(x+%1)=%2", v)); w.push_back(n - v[1]);
    } else {
      v[0] = R(-9, 9); v[1] = R(1, 2);
      sh = eqn(pat("log(x+%0)=%1", v)); w.push_back((v[1] == 1 ? 10 : 100) - v[0]);
    }
    break;
  }
  case 4: case 5: { // a pattern with small numbers
    static const char * const D[7] = {"%0*x^%1+%2*x^2-%3*x", "x^%1*sin(x)", "sin(%0*x^2)", "e^(%0*x)*x", "(%0*x+%2)^%1", "ln(x^2+%2)", "x/(x+%2)"};
    static const char * const I[7] = {"%0*x^%1+%2*x", "%0*cos(%1*x)", "e^(%1*x)", "%0/x", "x*e^x", "2*x*cos(x^2)", "x*sin(x)"};
    v[0] = R(2, 5); v[1] = R(2, 5); v[2] = R(1, 9); v[3] = R(1, 9);
    const std::string f = pr(ev(rd(pat(topic == 4 ? D[R(0, 6)] : I[R(0, 6)], v))));
    sh = (topic == 4 ? "diff(" : "integrate(") + f + ",x)";
    w.push_back(rd(f));
    kind = topic == 4 ? K_DERIV : K_ANTI;
    break;
  }
  case 6: { // (x + a)(x + b), a (x + b)(x + c), (p x + q)(x + s), x^2 - b^2: expanded
    static const char * const F[4] = {"(x+%1)*(x+%2)", "%0*(x+%1)*(x+%2)", "(%0*x+%1)*(x+%2)", "x^2-%3^2"};
    v[0] = R(2, 3); v[1] = Rnz(-9, 9); do v[2] = Rnz(-9, 9); while (v[2] == -v[1]); v[3] = R(2, 9);
    const gen p = ev(rd("expand(" + pat(F[R(0, 3)], v) + ")"));
    sh = pr(p); c = "factor(" + sh + ")";
    w.push_back(p);
    kind = K_FACTOR;
    break;
  }
  default: { // a table: the equation, its solutions in [0, 2pi)
    static const char * const T[10][2] = {
      {"2*sin(x)=1", "[pi/6,5*pi/6]"}, {"2*cos(x)=1", "[pi/3,5*pi/3]"}, {"tan(x)=1", "[pi/4,5*pi/4]"},
      {"2*sin(x)+sqrt(3)=0", "[4*pi/3,5*pi/3]"}, {"2*cos(x)+sqrt(2)=0", "[3*pi/4,5*pi/4]"}, {"sin(x)=0", "[0,pi]"},
      {"cos(x)=-1", "[pi]"}, {"sqrt(3)*tan(x)=1", "[pi/6,7*pi/6]"}, {"2*sin(x)^2=1", "[pi/4,3*pi/4,5*pi/4,7*pi/4]"},
      {"2*cos(x)^2-cos(x)-1=0", "[0,2*pi/3,4*pi/3]"}};
    const int i = R(0, 9);
    sh = T[i][0];
    w = *ev(rd(T[i][1]))._VECTptr;
  }
  }
  if (topic < 2 || topic == 3 || topic == 7) c = "solve(" + sh + ",x)";
  if (topic == 4 || topic == 5) c = sh;
  state = 0;
  ++nprob;
  *yours_ = *right_ = "";
}

// ---------------------------------------------------------------- the check
static NOINL bool num(const gen & g, double & d) {
  const gen f = evalf(g, 1, contextptr);
  if (f.type == _DOUBLE_) { d = f._DOUBLE_val; return true; }
  if (f.type == _INT_) { d = f.val; return true; }
  return false;
}
static NOINL bool close(double a, double b) { return fabs(a - b) <= 1e-4 * (1 + fabs(a) + fabs(b)); }
static NOINL gen side(const gen & g) { return g.is_symb_of_sommet(at_equal) ? g._SYMBptr->feuille[1] : g; } // (x=3: 3)
// the values of an answer: x=2,x=3 / 2,3 / [2,3] / x=2 or x=3
static NOINL void vals(const gen & g, vecteur & out) {
  if (g.type == _VECT) { for (const_iterateur it = g._VECTptr->begin(); it != g._VECTptr->end(); ++it) vals(*it, out); return; }
  if (g.is_symb_of_sommet(at_ou) || g.is_symb_of_sommet(at_and)) { vals(g._SYMBptr->feuille, out); return; }
  out.push_back(side(g));
}
static NOINL bool same_values(const vecteur & a, const vecteur & b, bool ordered) {
  if (a.size() != b.size()) return false;
  for (unsigned i = 0; i < a.size(); ++i) {
    double x;
    if (!num(a[i], x)) return false;
    bool f = false;
    for (unsigned j = ordered ? i : 0; j < (ordered ? i + 1 : b.size()) && !f; ++j) { double y; f = num(b[j], y) && close(x, y); }
    if (!f) return false;
  }
  return true;
}
// g and h equal at a few points (h: f's derivative for an antiderivative)
static NOINL bool same_fn(const gen & g, const gen & h) {
  static const double P[3] = {0.7, 1.3, 2.1};
  const gen x(identificateur("x"));
  for (int i = 0; i < 3; ++i) {
    double a, b;
    if (!num(subst(g, x, P[i], false, contextptr), a) || !num(subst(h, x, P[i], false, contextptr), b) || !close(a, b)) return false;
  }
  return true;
}
static NOINL bool product(const gen & g) { // a factored form: a product or a power, maybe negated
  const gen h = g.is_symb_of_sommet(at_neg) ? g._SYMBptr->feuille : g;
  return h.is_symb_of_sommet(at_prod) || h.is_symb_of_sommet(at_pow);
}
static NOINL void the_answer() { // the right answer as shown
  std::string & r = *right_;
  const vecteur & w = *want_;
  r = "";
  if (kind == K_VALUES || kind == K_PAIR)
    for (unsigned i = 0; i < w.size(); ++i) { if (i) r += ","; r += kind == K_PAIR ? (i ? "y=" : "x=") : "x="; r += pr(w[i]); }
  else if (kind == K_DERIV) r = pr(normal(ev(rd("diff(" + pr(w[0]) + ",x)")), contextptr)); // (4x cos(2x^2), not 2*2*x*...)
  else if (kind == K_ANTI) r = pr(ev(rd("integrate(" + pr(w[0]) + ",x)"))) + "+C";
  else r = pr(ev(rd("factor(" + pr(w[0]) + ")")));
}
static NOINL bool right(const std::string & s) {
  const gen g = ev(rd(s));
  if (is_undef(g) || g.type == _STRNG) return false;
  const vecteur & w = *want_;
  if (kind == K_VALUES || kind == K_PAIR) {
    vecteur v;
    vals(g, v);
    if (kind == K_PAIR && g.type == _VECT && v.size() == 2 && g[0].is_symb_of_sommet(at_equal) && pr(g[0]._SYMBptr->feuille[0]) == "y") { const gen t = v[0]; v[0] = v[1]; v[1] = t; } // (y=2,x=1)
    return same_values(v, w, kind == K_PAIR);
  }
  const gen f = side(g), x(identificateur("x"));
  if (kind == K_DERIV) return same_fn(f, ev(rd("diff(" + pr(w[0]) + ",x)")));
  if (kind == K_ANTI) return same_fn(ev(symbolic(at_derive, makesequence(f, x))), w[0]);
  return product(rd(s)) && same_fn(f, w[0]); // (factored as typed: expand(...) would not count)
}

// ---------------------------------------------------------------- the screen
static NOINL void paint() {
  const int rows = ui_band_open(48);
  char sc[40];
  strcpy(sc, print_INT_(nright).c_str()); strcat(sc, " of "); strcat(sc, print_INT_(ntotal).c_str()); strcat(sc, " right");
  for (int b = Y0; b < SBOT; b += rows ? rows : SBOT - Y0) {
    const int e = rows && b + rows < SBOT ? b + rows : SBOT;
    if (rows) ui_band_begin(b, e); else ui_clip(0, b, UI_W, e);
    ui_fill(0, Y0, UI_W, SBOT - Y0, col(UC_BG));
    text(&ui_tb10, TOPIC[topic], 12, Y0 + 18, UC_ACC);
    if (ntotal) text(&ui_tr10, sc, UI_W - 12, Y0 + 18, UC_SUB, 2);
    text(&ui_tr10, ASK[topic], 12, Y0 + 40, UC_SUB);
    math(show_->c_str(), UI_W / 2, Y0 + 84, 1, UI_W - 24, UC_INK);
    ui_fill(12, Y0 + 112, UI_W - 24, 1, col(UC_LINE));
    if (state == 0) text(&ui_tr10, "enter: answer    F2: the steps    F3: another one", UI_W / 2, Y0 + 134, UC_SUB, 1);
    else {
      const bool ok = state == 1;
      ui_rrect(0, 10, Y0 + 120, UI_W - 20, SBOT - Y0 - 128, 8, ok ? UC_ACCSOFT : UC_CARD, UC_BG);
      ui_rrect(0, 10, Y0 + 128, 3, SBOT - Y0 - 144, 1, ok ? UC_ACC : UC_RED, ok ? UC_ACCSOFT : UC_CARD);
      ui_draw_text(&ui_tb12, ok ? "Right!" : state == 2 ? "Not quite" : "The answer", -1, 22, Y0 + 140, ui_ramp(0, ok ? UC_ACC : UC_RED, ok ? UC_ACCSOFT : UC_CARD), 0);
      const int bg = ok ? UC_ACCSOFT : UC_CARD;
      for (int k = 0; k < 2; ++k) { // yours, then the right one (labeled)
        const std::string & m = k ? *right_ : *yours_;
        if (m.empty() || (k && ok)) continue;
        const int yb = k ? SBOT - 16 : Y0 + 164;
        ui_draw_text(&ui_tr10, k ? "answer" : "yours", -1, 22, yb, ui_ramp(0, UC_SUB, bg), 0);
        mi_layout L;
        const int lv = ui_math_fit(m.c_str(), m.size(), -1, 3, UI_W - 100, 30, MI_F_IMPLDOT, L);
        ui_math_draw(L, m.c_str(), lv, 76, yb, 0, UC_INK, bg, UC_ACC);
      }
      if (ok) ui_draw_text(&ui_tr10, "enter: the next one", -1, 22, SBOT - 16, ui_ramp(0, UC_SUB, bg), 0);
    }
    if (rows) ui_band_end();
  }
  ui_band_close();
  ui_noclip();
}
static NOINL bool pick_topic() {
  focus_status_label("PRACTICE");
  static const char * const tabs[5] = {"answer", "steps", "another", "topic", "back"};
  focus_bar_tabs(tabs, -1);
  paint();
  const int t = focus_card_list(3, "topic", TPREV, TOPIC, 0, 8, topic);
  if (t < 0) return false;
  if (t != topic) { topic = t; nright = ntotal = 0; }
  make();
  return true;
}
void focus_practice() {
  giac::ctrl_c = giac::interrupted = false;
  if (!cmd_) { cmd_ = new std::string; show_ = new std::string; yours_ = new std::string; right_ = new std::string; want_ = new vecteur; }
  seed ^= getkey_polls * 2654435761UL + (unsigned long)RTC_GetTicks();
  const int view0 = focus_view;
  focus_view = 1;
  if (nprob ? (make(), true) : pick_topic())
    for (;;) {
      focus_status_label("PRACTICE");
      static const char * const tabs[5] = {"answer", "steps", "another", "topic", "back"};
      focus_bar_tabs(tabs, -1);
      paint();
      const int key = key_();
      if (key == KEY_CTRL_EXIT || key == KEY_CTRL_AC || key == KEY_CTRL_F5) break;
      if (key == KEY_CTRL_F3 || (state && key == KEY_CTRL_EXE)) { make(); continue; }
      if (key == KEY_CTRL_F4) { if (!pick_topic()) make(); continue; }
      if (key == KEY_CTRL_F2) { // the steps of the problem (no longer scored)
        if (!state) { the_answer(); state = 3; }
        std::string a = *right_;
        focus_steps(cmd_->c_str(), a.c_str());
        focus_view = 1;
        continue;
      }
      if ((key == KEY_CTRL_EXE || key == KEY_CTRL_F1) && !state) {
        std::string s;
        if (focus_prompt(ASK[topic], kind == K_PAIR ? "x, y =" : kind >= K_DERIV ? "" : "x =", s, false) != KEY_CTRL_EXE || s.empty()) continue;
        *yours_ = s;
        const bool ok = right(s);
        ++ntotal; nright += ok;
        state = ok ? 1 : 2;
        if (!ok) the_answer();
      }
    }
  focus_view = view0;
  focus_status_label(0);
  focus_invalidate();
}
#endif
