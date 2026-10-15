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
#include "steps.h"
#include "answer.h"

extern giac::context * contextptr;
namespace giac { extern bool (*control_c_hook)(); }
extern "C" { extern volatile char oom_hit; } // allocator_custom.c: memory ran out
void oom_rearm(); // main.cc: the memory reserve, taken again

static bool starts(const char * s, const char * p) { return !strncmp(s, p, strlen(p)); }
// a calculation the steps screen can show: a derivative, an integral, a limit, a sum, a Taylor
// polynomial, also inside another command (subst(diff(f,x,3),x=0), solve(diff(f,x)=0,x): the
// steps take the calculus part first, then the rest); an application (area, volume...)
int focus_steps_able(const char * s) {
  if (strstr(s, "desolve(")) return 0; // (its diff(y,x) is the equation, not a derivative to work out)
  static const char * const ops[] = {"diff(", "integrate(", "int(", "derive(", "limit(", "series(", "taylor(", "sum(",
                                     "area(", "volume(", "arclength(", "surfacearea(", "average("};
  for (const char * p = s; *p; ++p)
    if (p == s || !isalnum((unsigned char)p[-1]))
      for (unsigned k = 0; k < sizeof(ops) / sizeof(ops[0]); ++k)
        if (starts(p, ops[k])) return 1;
  return 0;
}

// The page: items laid out once (page coordinates: y from the top of the page), drawn in bands.
// Labels, names and rules at the margin X; the math in groups (an equation and the lines going
// on with it, the working of a step), each centered as a block.
enum { Y0 = 18, X = 14, XR = 306, W = XR - X, LVMIN = 6, LVWRAP = 5 };
enum { K_HEAD, K_LINE, K_LABEL, K_TITLE, K_FORM, K_WORK, K_EQ, K_BOX, K_ANS };
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
// of a list go
static __attribute__((noinline)) void unwrap(char * s) {
  for (char * a = s; *a;) {
    int n = top(a, ','), e = top(a, '=');
    if (n < 0) n = strlen(a);
    if (e >= 0 && e < n) n = e;
    char * b = a + n;
    if (n >= 2 && *a == '(' && b[-1] == ')') {
      int d = 0;
      char * q = a;
      for (; q < b; ++q) { if (*q == '(') ++d; else if (*q == ')' && !--d) break; }
      if (q == b - 1) { memmove(b - 1, b, strlen(b) + 1); memmove(a, a + 1, strlen(a)); b -= 2; }
    }
    a = *b ? b + 1 : b;
  }
}
// where a line of s[0,n) may break: before a + or - between terms, at top level, after the =
static int cuts(const char * s, int n, short * at, int max) {
  int depth = 0, k = 0, eq = top(s, '=');
  const int s0 = eq < 0 ? 0 : eq + 1; // (every parenthesis counted: from 1, ((u)-(v))/(1-w)^2 was cut
  for (int i = s0; i < n && k < max; ++i) { // inside (1-w), a garbled line; no cut at the start)
    const char c = s[i];
    if (c == '(' || c == '[') ++depth;
    else if (c == ')' || c == ']') --depth;
    else if (!depth && i > s0 + 1 && (c == '+' || c == '-') && !strchr("+-*/^(,=", s[i - 1]) &&
             !(s[i - 1] == 'e' && i > 1 && s[i - 2] >= '0' && s[i - 2] <= '9')) at[k++] = i;
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
  const int e = lead ? -1 : top(s, '=');
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
    int lv = LVWRAP;
    while (lay(s + a, best - a, lv, L) > wi && lv < UI_NSIZES - 1) ++lv;
    item * it = put(kind, dup(s + a, best - a), xi, y + h + 4, (a ? 0 : lr) + L.width, L.asc + L.desc, L.asc, lv);
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

// the steps of v as items (ans: the answer, as on the console; plusc: an antiderivative)
static __attribute__((noinline)) void lay_steps(const char * head, const std::vector<step_t> & v, const char * ans, bool plusc) {
  int y = 10, num = 0, prevd = 0, fe = -1, eqn = 0, k0 = 0; // fe: the part's last equation, its
  // left side s[0,eqn) (the lines going on with it have their = under its =)
  unsigned char last[8] = {0}; // the number of the last step at each depth
  SEC = 0; GRP = 1;
  y += add_math(head, strlen(head), X, y, W, 2, K_HEAD) + 16;
  for (unsigned i = 0; i < v.size(); ++i) {
    const int d = v[i].depth < 7 ? v[i].depth : 7;
    const char * title = steps_title(v[i]);
    std::string t = steps_math(v[i], contextptr).print(contextptr);
    if (oom_hit) break;
    char * s = dup(t.c_str(), t.size());
    if (!s) break;
    unwrap(s);
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
      y += 4;
    }
    free(s);
  }
  finish(k0);
  if (ans && *ans) { // the answer, in a box (instead of a last line "= the answer")
    if (NP && P[NP - 1].kind == K_EQ && P[NP - 1].num == 1 && !strcmp(P[NP - 1].s, ans)) { y = P[NP - 1].y; free(P[--NP].s); }
    k0 = NP;
    section("ANSWER", 0, y);
    const int b = NP;
    if (put(K_BOX, 0, X - 4, y, W + 8, 0, 0, 0)) {
      ++GRP;
      static const char DNE[] = "does not exist", DIV[] = "diverges"; // (a limit's, a series' text answer)
      const char * tx = starts(ans, DNE) || starts(ans, "no limit") ? DNE : starts(ans, DIV) ? DIV : 0; // (sin(1/x) at 0: "no limit (it oscillates)")
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
  case K_BOX: ui_rrect(0, x, y, it.w, it.h, 8, UC_ACCSOFT, bg); return;
  case K_ANS: bg = UC_ACCSOFT; break;
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
static void waiting(const char * head) {
  static const char T[] = "Working out the steps";
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
      waiting(head.c_str());
      giac::control_c_hook = focus_busy_tick;
      ok = steps_of(g, v, contextptr);
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
    std::string a2; // the answer box: the steps' value for a sum left as is, a series without O(...)
    if (ans && strstr(ans, "sum(") && !v.empty() && (!*steps_title(v.back()) || !strcmp(steps_title(v.back()), "Its limit")))
      ans = (a2 = v.back().b.print(contextptr)).c_str();
    else if (ans && (starts(p, "series(") || starts(p, "taylor(")) && !v.empty()) // (the steps' polynomial: 1-(x-1)+(x-1)^2,
      ans = (a2 = v.back().b.print(contextptr)).c_str(); // not giac's 1-x+1+(x-1)^2+O((x-1)^3))
    lay_steps(head.c_str(), v, ans, c == 1);
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

void focus_steps(const char * in, const char * ans) {
  focus_status_label("STEPS");
  const char * why = build(in, ans);
  focus_status_msg(0);
  int key;
  if (why) { // over the problem (the waiting line erased)
    clear_page();
    if (wait_y) ui_fill(0, wait_y - 14, UI_W, 32, ui_col(0, UC_BG));
    focus_note(why, why[0] == 'N' ? "No textbook method explains this answer" : 0);
    GetKey(&key);
  }
  else {
    const int vis = UI_H - Y0, maxtop = PH > vis ? PH - vis : 0;
    int top = 0, cur = 1;
    for (;;) {
      paint(top, cur);
      GetKey(&key);
      if (key == KEY_CTRL_EXIT || key == KEY_CTRL_AC || key == KEY_CTRL_EXE || key == KEY_CTRL_OK) break;
      if (key == KEY_CTRL_DOWN || key == KEY_CTRL_RIGHT) move(1, cur, top, maxtop);
      if (key == KEY_CTRL_UP || key == KEY_CTRL_LEFT) move(-1, cur, top, maxtop);
    }
  }
  clear_page();
  focus_status_label(0);
  focus_invalidate();
}

#ifdef HOSTGIAC
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
