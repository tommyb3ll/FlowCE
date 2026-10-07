// focus.cc - the Focus interface of the console (focus.h; the design and its prototype are in
// notes/ui). The console's lines are read as entries (an input line and its output lines); the
// edit line is drawn large and centered, entries are stacked above it, the last result is shown
// large right after an evaluation. Cursor moves into the history select an entry (its input or
// its result). All text is drawn with ui_math (STIX Two, 4 shades) and ui_font (Atkinson).
#include <string.h>
#include <stdlib.h>
#ifdef TICE
#include <time.h>              // clock (before console.h: giac's first.h defines a clock macro)
#endif
#include "console.h"           // Line[], Last_Line, Cursor, menus; maps std to ustl
#include "focus.h"
#include "ui_gfx.h"
#include "ui_font.h"
#include "ui_fontdata.h"
#include "ui_math.h"
#ifdef TICE
#include <sys/power.h>
#include <sys/rtc.h>
#endif

extern "C" int os_get_angle_unit();
extern "C" int os_key_flags();      // k_csdk.c: 1 2nd, 2 alpha, 4 alpha lock, 8 lowercase
bool console_edit2d();              // console.cc: the edit line is math (not Python)
int console_caret();                // console.cc: caret index in the edit line, -1 if in the history
extern const char * console_form_name; // main.cc: the form F4 last gave console_form_line()
const char * console_form_line();
extern const char * console_approx_for; // main.cc: the decimal value of that printed result
const char * console_approx();

// what a result line shows: giac prints the lists of solve as list[a,b], shown [a,b]
static const char * shown(const char * s) { return s && !strncmp(s, "list[", 5) ? s + 4 : s; }
const char * console_fkey_label(int layer, int k); // console.cc: F-key label (layer 0, 2nd, alpha)

// timing probe for tools/emu (sampled with peek): the step of the drawing in progress, 0 when done
extern "C" { volatile unsigned char focus_phase; }
static const ui_theme * theme = &ui_theme_paper; // Paper or Night (focus_toggle_theme)

enum { SB = 16, MB = 22, ST = SB, SBOT = UI_H - MB, SH = SBOT - ST, PEEK = 30, MAXE = 64,
       IN_LV = 5, OUT_LV = 3, HERO_LV = 0, W_IN = 286, W_OUT = 292 };

static int hero_last;      // the last result is shown large (until edited or cleared)
static int hero_lv = HERO_LV;
static int hscroll;        // history mode: top of the view in the entry column
static char smsg[40];      // status bar message

static unsigned char col(int c) { return ui_col(0, c); }
static const unsigned char * ramp(int fg, int bg) { return ui_ramp(0, fg, bg); }

// ------------------------------------------------------------------ fitted layouts (cached)
struct fitc { const char * p; unsigned hash; signed char maxlv, lv; short maxw, maxh, flags, w, a, d; };
enum { NFC = 72 }; // history lines + the hero: a miss costs 1-3 mi_build
// heap, allocated on first use: KhiCAS's static data must fit the 8400 bytes of pixelShadow
static fitc * FC;
static int fcn;
static unsigned shash(const char * s) { unsigned h = 5381; while (*s) h = h * 33 + (unsigned char)*s++; return h; }
static fitc fitted(const char * s, int maxlv, int maxw, int maxh, int flags) {
  unsigned h = shash(s);
  if (!FC) FC = (fitc *)calloc(NFC, sizeof(fitc));
  for (int i = 0; i < NFC; ++i) {
    const fitc & c = FC[i];
    if (c.p == s && c.hash == h && c.maxlv == maxlv && c.maxw == maxw && c.maxh == maxh && c.flags == flags)
      return c;
  }
  fitc & c = FC[fcn];
  fcn = (fcn + 1) % NFC;
  mi_layout L;
  c.lv = (signed char)ui_math_fit(s, strlen(s), -1, maxlv, maxw, maxh, flags, L);
  c.p = s; c.hash = h; c.maxlv = (signed char)maxlv; c.maxw = (short)maxw; c.maxh = (short)maxh;
  c.flags = (short)flags; c.w = L.width; c.a = L.asc; c.d = L.desc;
  return c;
}
// built layouts of the last strings drawn: a banded repaint draws an entry once per band
enum { NLC = 16 }; // a screen of entries (input and result) plus the hero
struct lckey { const char * p; unsigned hash; signed char lv; char flags; };
static lckey LCK[NLC];
static mi_layout * LCL; // heap (new[]: no static constructors in KhiCAS)
static int lcn;
static const mi_layout & layout_of(const char * s, int lv, int flags) {
  unsigned h = shash(s);
  if (!LCL) LCL = new mi_layout[NLC];
  for (int i = 0; i < NLC; ++i)
    if (LCK[i].p == s && LCK[i].hash == h && LCK[i].lv == lv && LCK[i].flags == flags) return LCL[i];
  int i = lcn;
  lcn = (lcn + 1) % NLC;
  mi_build(s, strlen(s), -1, ui_math_metrics(lv, flags), LCL[i]);
  LCK[i].p = s; LCK[i].hash = h; LCK[i].lv = (signed char)lv; LCK[i].flags = (char)flags;
  return LCL[i];
}
static void draw_math(const char * s, const fitc & c, int x, int base, int ink, int bg, int flags) {
  ui_math_draw(layout_of(s, c.lv, flags), s, c.lv, x, base, 0, ink, bg, UC_ACC);
}

// messages ("Done", "f(x) defined", warnings) rather than math
static bool is_text(const char * s) {
  if (!*s) return true;
  if (s[0] == '"' && s[1] == '/' && s[2] == '/') return true; // an error: "// Error: ..."
  if (*s == '"') return false;
  for (const char * p = s; *p; ++p)
    if (*p == ' ' && ((p[1] | 32) >= 'a' && (p[1] | 32) <= 'z') && p > s && p[-1] != ',') return true;
  if (*s >= 'A' && *s <= 'Z') {
    const char * p = s + 1;
    while (*p >= 'a' && *p <= 'z') ++p;
    if (!*p && p - s > 2) return true;
  }
  return false;
}
// an antiderivative: integrate(f,x) or int(f,x) with 2 arguments (or 1: in x): its result gets "+ C"
static bool is_antideriv(const char * s) {
  while (*s == ' ') ++s;
  const char * p = !strncmp(s, "integrate(", 10) ? s + 10 : !strncmp(s, "int(", 4) ? s + 4 : 0;
  if (!p) return false;
  int depth = 0, commas = 0;
  for (; *p; ++p) {
    if (*p == '(' || *p == '[') ++depth;
    else if (*p == ')' || *p == ']') { if (!depth--) break; }
    else if (*p == ',' && !depth) ++commas;
  }
  return commas <= 1;
}

static int ui_text(const ui_face * f, const char * s, int x, int base, int fg, int bg, int align) {
  int w = ui_text_width(f, s, -1);
  if (align == 1) x -= w / 2;
  else if (align == 2) x -= w;
  ui_draw_text(f, s, -1, x, base, ramp(fg, bg), 0);
  return w;
}

// a message (an error, "Out of memory", "no limit...") centered on up to 4 lines of width w
// between top and bot, cut at spaces; giac's "// " prefix dropped
static void ui_text_wrapped(const char * s, int top, int bot, int w) {
  if (s[0] == '"') ++s;
  if (s[0] == '/' && s[1] == '/') for (s += 2; *s == ' '; ++s) ;
  const char * ln[4];
  int ll[4], n = 0;
  while (*s && !(s[0] == '"' && !s[1]) && n < 4) {
    int j = 0, last = -1;
    while (s[j] && !(s[j] == '"' && !s[j + 1]) && ui_text_width(&ui_tr12, s, j + 1) <= w) { if (s[j] == ' ') last = j; ++j; }
    if (s[j] && last > 0) j = last;
    if (!j) j = 1;
    ln[n] = s; ll[n++] = j;
    s += j;
    while (*s == ' ') ++s;
  }
  int y = (top + bot) / 2 - (n - 1) * 8 + 4;
  for (int k = 0; k < n; ++k, y += 16)
    ui_draw_text(&ui_tr12, ln[k], ll[k], (UI_W - ui_text_width(&ui_tr12, ln[k], ll[k])) / 2, y, ramp(UC_SUB, UC_BG), 0);
}

// ------------------------------------------------------------------ entries
struct fent { short in, out, m0; short h, inh, outh, msgs; };
static fent * E; // heap (see FC)
static int NE;
static void scan() {
  NE = 0;
  if (!E) E = (fent *)calloc(MAXE, sizeof(fent));
  for (int l = 0; l < Last_Line; ++l) {
    int t = Line[l].type;
    if (t == LINE_TYPE_CONT || !Line[l].str) continue;
    if (t == LINE_TYPE_INPUT || NE == 0) {
      if (NE == MAXE) break;
      fent & e = E[NE++];
      e.in = t == LINE_TYPE_INPUT ? l : -1; e.out = e.m0 = -1;
      if (t == LINE_TYPE_INPUT) continue;
    }
    fent & e = E[NE - 1];
    if (e.m0 < 0) e.m0 = l;
    e.out = l;
  }
  for (int k = 0; k < NE; ++k) { // geometry
    fent & e = E[k];
    e.inh = 0; e.outh = 0; e.msgs = 0;
    if (e.in >= 0) { fitc c = fitted(Line[e.in].str, IN_LV, W_IN, 34, MI_F_TIDY); e.inh = c.a + c.d; }
    for (int l = e.m0; l >= 0 && l < e.out; ++l)
      if (Line[l].type != LINE_TYPE_CONT && e.msgs < 2) ++e.msgs;
    if (e.out >= 0) {
      const char * s = shown(Line[e.out].str);
      if (is_text(s)) e.outh = 13;
      else { fitc c = fitted(s, OUT_LV, W_OUT, 64, MI_F_IMPLDOT); e.outh = c.a + c.d; }
    }
    e.h = 7 + e.inh + 5 + e.msgs * 11 + e.outh + 8;
  }
}
static int entry_of_line(int l) {
  while (l > 0 && Line[l].type == LINE_TYPE_CONT) --l;
  for (int k = NE - 1; k >= 0; --k)
    if (E[k].in == l || (E[k].m0 >= 0 && l >= E[k].m0 && l <= E[k].out)) return k;
  return NE - 1;
}

static void draw_result(const fent & e, int right, int base, int ink, int bg) {
  const char * s = shown(Line[e.out].str);
  if (is_text(s)) { // one line, right-aligned, cut with ... when longer than the row
    if (s[0] == '"') ++s;
    if (s[0] == '/' && s[1] == '/') for (s += 2; *s == ' '; ++s) ;
    int n = strlen(s), cut = 0;
    if (n && s[n - 1] == '"') --n;
    while (n > 1 && ui_text_width(&ui_tr10, s, n) > 280) { --n; cut = 1; }
    int w = ui_text_width(&ui_tr10, s, n) + (cut ? ui_text_width(&ui_tr10, "...", 3) : 0);
    int x = ui_draw_text(&ui_tr10, s, n, right - w, base, ramp(UC_SUB, bg), 0);
    if (cut) ui_draw_text(&ui_tr10, "...", 3, right - w + x, base, ramp(UC_SUB, bg), 0);
    return;
  }
  fitc c = fitted(s, OUT_LV, W_OUT, 64, MI_F_IMPLDOT);
  int cw = 0;
  if (e.in >= 0 && is_antideriv(Line[e.in].str)) { // "+ C" in the secondary color
    const ui_face * f = &ui_mu17;
    cw = ui_text_width(f, "+ ", -1) + ui_text_width(&ui_mi17, "C", -1) + 4;
    int x = right - cw + 4;
    x += ui_draw_text(f, "+ ", -1, x, base, ramp(UC_SUB, bg), 0);
    ui_draw_text(&ui_mi17, "C", -1, x, base, ramp(UC_SUB, bg), 0);
  }
  const int avail = W_OUT - cw;
  if (c.w > avail) { // too long even small: its end, clipped, "..." in front (it ran off the left)
    const int dw = ui_text_width(&ui_tr10, "...", 3) + 4;
    ui_draw_text(&ui_tr10, "...", 3, right - avail, base, ramp(UC_SUB, bg), 0);
    ui_clip(right - avail + dw, 0, right - cw, UI_H);
    draw_math(s, c, right - cw - c.w, base, ink, bg, MI_F_IMPLDOT);
    ui_noclip();
    return;
  }
  draw_math(s, c, right - cw - c.w, base, ink, bg, MI_F_IMPLDOT);
}

static void draw_entry(const fent & e, int y, int sel, int selout) {
  int bg = sel ? UC_ACCSOFT : UC_BG;
  if (sel) ui_rrect(0, 5, y + 2, UI_W - 10, e.h - 4, 7, UC_ACCSOFT, UC_BG);
  int b = y + 7;
  if (e.in >= 0) {
    fitc c = fitted(Line[e.in].str, IN_LV, W_IN, 34, MI_F_TIDY);
    draw_math(Line[e.in].str, c, 12, b + c.a, sel && !selout ? UC_ACC : UC_SUB, bg, MI_F_TIDY);
    b += e.inh;
  }
  b += 5;
  for (int l = e.m0, n = 0; l >= 0 && l < e.out && n < 2; ++l)
    if (Line[l].type != LINE_TYPE_CONT) {
      b += 11;
      ui_text(&ui_tr9, Line[l].str, UI_W - 12, b - 2, UC_SUB, bg, 2);
      ++n;
    }
  if (e.out >= 0) {
    const char * s = shown(Line[e.out].str);
    int a = is_text(s) ? 11 : fitted(s, OUT_LV, W_OUT, 64, MI_F_IMPLDOT).a;
    draw_result(e, UI_W - 12, b + a, sel && selout ? UC_ACC : UC_INK, bg);
  }
  if (sel) ui_rframe(0, 5, y + 2, UI_W - 10, e.h - 4, 7, UC_ACC, UC_BG);
  else ui_fill(12, y + e.h - 1, UI_W - 24, 1, col(UC_LINE));
}

// ------------------------------------------------------------------ hero (the edit line)
// a small up or down triangle centered at x, cy (the arrow keys)
static void arrow(int x, int cy, int up, int c) {
  for (int k = 0; k < 4; ++k) ui_fill(x - k, cy + (up ? k - 2 : 1 - k), 2 * k + 1, 1, col(c));
}
// a keycap (a word, or an arrow: "^" up, "v" down) then its label; returns the x after the label
static int key_hint(int x, int base, const char * k, const char * label) {
  int ar = (k[0] == '^' || k[0] == 'v') && !k[1], w = ar ? 15 : ui_text_width(&ui_tb9, k, -1) + 8;
  ui_rframe(0, x, base - 9, w, 12, 3, UC_SUB, UC_BG);
  if (ar) arrow(x + w / 2, base - 3, k[0] == '^', UC_SUB);
  else ui_text(&ui_tb9, k, x + w / 2, base, UC_SUB, UC_BG, 1);
  return x + w + 4 + ui_text(&ui_tr9, label, x + w + 4, base, UC_SUB, UC_BG, 0);
}

// The hero is prepared (what it shows, the edit line's layout and position) before it is painted:
// a banded repaint paints it once per band from these.
enum { HM_HINT, HM_MATH, HM_TEXT, HM_RESULT };
static int hm = HM_HINT, hy0, hh, hx, hbase, hcaret;
static mi_layout * HL; // HM_MATH: the edit line's layout (heap)
static int hb_y0, hb_y1; // HM_MATH: the rows it covers (ink overhang included)
static int help_e = -1;  // HM_MATH: the command the caret is in (catalog entry), for the help lines
enum { HELP_H = 32 };    // ... at the bottom of the hero

static void hero_prepare(int y0, int h) {
  hy0 = y0; hh = h;
  const char * s = (const char *)Console_GetEditLine();
  hcaret = console_caret();
  help_e = -1;
  if (s && *s && console_edit2d()) {
    if (!HL) HL = new mi_layout;
    mi_layout & L = *HL;
    focus_phase = 2;
    mi_arg_label = focus_arg_label; // empty arguments show their names (focus_catalog.cc)
    hero_lv = ui_math_refit(s, strlen(s), hcaret, HERO_LV, hero_lv, 296, h - 24 > 20 ? h - 24 : 20, MI_F_CALLBOX | MI_F_HINTS, L);
    focus_phase = 3;
    hx = (UI_W - L.width) / 2; hbase = y0 + (h - (L.asc + L.desc)) / 2 + L.asc;
    if (L.width > 296) { // caret kept in view
      hx = UI_W / 2 - L.cx;
      if (hx > 12) hx = 12;
      if (hx + L.width < UI_W - 12) hx = UI_W - 12 - L.width;
    }
    hb_y0 = hbase - L.asc - 4; hb_y1 = hbase + L.desc + 4;
    // in a command of several arguments: what it does and an example, under the line if it fits
    if (hcaret >= 0 && hb_y1 + 4 <= y0 + h - HELP_H && (help_e = focus_call_entry(s, hcaret)) >= 0) hb_y1 = y0 + h;
    hm = HM_MATH;
  } else if (s && *s) hm = HM_TEXT; // Python: plain text
  else if (hero_last && NE && E[NE - 1].out >= 0) hm = HM_RESULT;
  else hm = HM_HINT;
}

// A result too wide for one line even at the smallest size is wrapped at its top-level + and -
// (between terms) on up to WL lines of one level: each term is measured once, the lines are
// packed greedily, then measured. The lines are copies (0-terminated) in WB; wrapped() caches
// the last result.
enum { WL = 6 };
static char * WB;                    // the lines, one after the other
static const char * wl_src;          // the result wrapped (Line str), its hash
static unsigned wl_hash;
static int wl_n, wl_lv, wl_w, wl_h;  // lines, level, widest line, height of a line
static short wl_at[WL];              // line k: WB + wl_at[k]
static mi_layout * WLL;              // (one layout for all the measures: less code)
__attribute__((noinline)) static int mwidth(const char * s, int n, int lv, int & h) {
  if (!WLL) WLL = new mi_layout;
  mi_build(s, n, -1, ui_math_metrics(lv, MI_F_IMPLDOT), *WLL);
  if (WLL->asc + WLL->desc > h) h = WLL->asc + WLL->desc;
  return WLL->width;
}
__attribute__((noinline)) static int wrapped(const char * r, int maxw, int maxh, int lvmax) {
  unsigned hs = shash(r);
  if (r == wl_src && hs == wl_hash) return wl_n;
  wl_src = r; wl_hash = hs; wl_n = 0;
  int n = strlen(r), nc = 0, d = 0;
  short cut[48], tw[48];
  cut[nc++] = 0;
  for (int i = 0; i < n && nc < 47; ++i) { // the terms: r[cut[k], cut[k+1]) (from 0: a ( there counts,
    char c = r[i];                            // (13x^14+...)/182 was cut inside its parentheses)
    if (c == '(' || c == '[') ++d;
    else if (c == ')' || c == ']') --d;
    else if (i && !d && (c == '+' || c == '-') && !strchr("^*/(e=,", r[i - 1])) cut[nc++] = (short)i;
  }
  cut[nc] = (short)n;
  if (nc < 2 || n > 600) return 0;
  free(WB);
  if (!(WB = (char *)malloc(n + WL + 1))) return 0;
  for (int lv = 3; lv < lvmax; ++lv) { // larger than the single line's level
    int lh = 0, gap = 2 * ui_math_metrics(lv, MI_F_IMPLDOT).opgap + 2, lines = 0, k = 0, w = 0, p = 0;
    for (int j = 0; j < nc; ++j) // each term's width (its sign drawn as binary)
      tw[j] = (short)(mwidth(r + cut[j], cut[j + 1] - cut[j], lv, lh) + (j ? gap : 0));
    while (k < nc && lines < WL) {
      int j = k, lw = 0;
      while (j < nc && (j == k || lw + tw[j] <= maxw)) lw += tw[j++];
      wl_at[lines++] = (short)p;
      memcpy(WB + p, r + cut[k], cut[j] - cut[k]);
      p += cut[j] - cut[k];
      WB[p++] = 0;
      k = j;
    }
    if (k < nc || lines * (lh + 4) > maxh) continue;
    for (k = 0; k < lines; ++k) { // the real widths
      int x = mwidth(WB + wl_at[k], strlen(WB + wl_at[k]), lv, lh);
      if (x > w) w = x;
    }
    if (w <= maxw) { wl_n = lines; wl_lv = lv; wl_w = w; wl_h = lh + 4; return wl_n; }
  }
  return 0;
}

// a line of help under the edit line: pre (gray) then t in color fg, centered, cut with an
// ellipsis when wider than the screen (one pass over t)
static void help_line(const ui_face * f, const char * t, int base, int fg, const char * pre) {
  static const char ell[] = "\xe2\x80\xa6";
  const int maxw = UI_W - 20, pw = pre ? ui_text_width(f, pre, -1) : 0;
  int n = strlen(t), tw = ui_text_width(f, t, n), cut = tw + pw > maxw;
  if (cut) { // the chars that fit with the ellipsis after them
    const int ew = ui_text_width(f, ell, -1);
    int k = 0, w = 0;
    for (int cw; k < n && w + (cw = ui_text_width(f, t + k, 1)) <= maxw - pw - ew; ++k) w += cw;
    n = k; tw = w + ew;
  }
  int x = (UI_W - pw - tw) / 2;
  if (pre) x += ui_draw_text(f, pre, -1, x, base, ramp(UC_SUB, UC_BG), 0);
  x += ui_draw_text(f, t, n, x, base, ramp(fg, UC_BG), 0);
  if (cut) ui_draw_text(f, ell, -1, x, base, ramp(fg, UC_BG), 0);
}

__attribute__((noinline)) static void hero_paint() { // in the current clip
  const char * s = (const char *)Console_GetEditLine();
  int y0 = hy0, h = hh;
  if (hm == HM_MATH) {
    focus_phase = 4;
    ui_math_draw(*HL, s, hero_lv, hx, hbase, 0, UC_INK, UC_BG, UC_ACC);
    focus_phase = 5;
    if (hcaret >= 0) ui_math_caret(*HL, hx, hbase, 0, UC_ACC);
    if (help_e >= 0) { // the command's description (it names the arguments as the boxes do), an example
      char d[72], ex[72];
      focus_entry_help(help_e, d, sizeof(d), ex, sizeof(ex));
      int b = y0 + h - 22;
      if (*d) help_line(&ui_tr10, d, b, UC_SUB, 0);
      if (*ex) help_line(&ui_tr10, ex, b + 14, UC_INK, "e.g.  ");
    }
  } else if (hm == HM_TEXT) {
    ui_text(&ui_tr12, s, 12, y0 + h / 2 + 4, UC_INK, UC_BG, 0);
  } else if (hm == HM_RESULT) {
    const fent & e = E[NE - 1];
    int top = y0 + 10;
    if (e.in >= 0) {
      fitc c = fitted(Line[e.in].str, 4, 292, 36, MI_F_TIDY);
      draw_math(Line[e.in].str, c, (UI_W - c.w) / 2, top + c.a, UC_SUB, UC_BG, MI_F_TIDY);
      top += c.a + c.d + 6;
    }
    const char * r = shown(Line[e.out].str);
    int bot = y0 + h - (is_text(r) ? 14 : 25); // above the forms chip (tall fractions ran into it)
    if (is_text(r)) ui_text_wrapped(r, top, bot, 296);
    else {
      int anti = e.in >= 0 && is_antideriv(Line[e.in].str);
      fitc c = fitted(r, HERO_LV, anti ? 256 : 292, bot - top > 18 ? bot - top : 18, MI_F_IMPLDOT);
      int cw = anti ? ui_text_width(&ui_mu24, "+ ", -1) + ui_text_width(&ui_mi24, "C", -1) + 6 : 0;
      int x = (UI_W - c.w - cw) / 2, base = top + (bot - top - (c.a + c.d)) / 2 + c.a;
      if ((c.w > (anti ? 256 : 292) || c.lv >= 5) && wrapped(r, anti ? 256 : 292, bot - top, c.w > (anti ? 256 : 292) ? UI_NSIZES : c.lv)) { // on several lines
        int bx = (UI_W - wl_w) / 2, b0 = top + (bot - top - wl_n * wl_h) / 2 + wl_h - 6;
        for (int k = 0; k < wl_n; ++k) {
          const char * t = WB + wl_at[k];
          fitc lc = fitted(t, wl_lv, 600, 200, MI_F_IMPLDOT);
          draw_math(t, lc, bx + (k ? 10 : 0), b0 + k * wl_h, UC_INK, UC_BG, MI_F_IMPLDOT);
          if (k == wl_n - 1) { c = lc; x = bx + (k ? 10 : 0); base = b0 + k * wl_h; }
        }
      } else draw_math(r, c, x, base, UC_INK, UC_BG, MI_F_IMPLDOT);
      if (anti) {
        int cx = x + c.w + 6;
        cx += ui_draw_text(&ui_mu24, "+ ", -1, cx, base, ramp(UC_SUB, UC_BG), 0);
        ui_draw_text(&ui_mi24, "C", -1, cx, base, ramp(UC_SUB, UC_BG), 0);
      }
    }
    // the footer: the decimal value of an exact result, then the forms chip (the form shown;
    // F4 cycles them, main.cc console_cycle_form), centered together as in the prototype
    if (!is_text(r)) {
      const char * fn = (const char *)Line[e.out].str == console_form_line() ? console_form_name : "exact";
      if (fn[0] == 'e' && strchr(r, '.') && !r[strspn(r, "0123456789.-+e")]) fn = "decimal"; // 2.00927
      const char * ap = !strcmp((const char *)Line[e.out].str, console_approx_for) ? console_approx() : "";
      char abuf[40] = "";
      if (*ap) { strcpy(abuf, "\xe2\x89\x88 "); strncat(abuf, ap, sizeof(abuf) - 5); } // ≈ value
      int aw = *abuf ? ui_text_width(&ui_tr10, abuf, -1) + 12 : 0;
      int cw2 = ui_text_width(&ui_tb9, fn, -1) + ui_text_width(&ui_tb9, "F4", -1) + 20, cy = y0 + h - 13;
      int x0 = (UI_W - aw - cw2) / 2, cx = x0 + aw;
      if (*abuf) ui_draw_text(&ui_tr10, abuf, -1, x0, cy, ramp(UC_SUB, UC_BG), 0);
      ui_rrect(0, cx, cy - 10, cw2, 13, 6, UC_ACCSOFT, UC_BG);
      int w1 = ui_text(&ui_tb9, fn, cx + 6, cy, UC_ACC, UC_ACCSOFT, 0);
      ui_text(&ui_tb9, "F4", cx + 12 + w1, cy, UC_SUB, UC_ACCSOFT, 0);
    }
  } else {
    int m = y0 + h / 2 - 2, b = m + 20;
    ui_text(&ui_tr12, "Type a calculation", UI_W / 2, m, UC_SUB, UC_BG, 1);
    int x = UI_W / 2 - 110; // the prototype's three hints: math templates, up history, down search
    x = key_hint(x, b, "math", "templates") + 12;
    x = key_hint(x, b, "^", "history") + 12;
    key_hint(x, b, "v", "search");
  }
}

static void hero_peek_paint(int y) { // history mode: the edit line, small, at the bottom
  ui_fill(10, y + 2, UI_W - 20, 1, col(UC_LINE));
  const char * s = (const char *)Console_GetEditLine();
  int b = y + 20;
  if (s && *s && console_edit2d()) { fitc c = fitted(s, 5, 230, 22, MI_F_CALLBOX); draw_math(s, c, 14, b, UC_SUB, UC_BG, MI_F_CALLBOX); }
  else ui_text(&ui_tr10, "New calculation", 14, b, UC_SUB, UC_BG, 0);
  int w = ui_text(&ui_tb9, "back", UI_W - 12, b, UC_SUB, UC_BG, 2);
  arrow(UI_W - 12 - w - 9, b - 4, 0, UC_SUB);
}

// ------------------------------------------------------------------ status bar and F-key bar
// Both bars are redrawn only when what they show changes (each key would otherwise redraw them
// up to three times); full redraws pass force.
static int hist_sel = -1; // entry selected in the history, -1 in edit mode
// the status bar in 3 zones redrawn separately: mode label | message | 2nd/alpha chip, battery
enum { SZ_L = 112, SZ_R = 232 };
static char stat_l[24], stat_m[sizeof(smsg)];
static int stat_r = -1;
static const char * stat_view; // a full-screen view's label (focus_status_label), 0: the console's

// text with extra letter spacing (the status label)
static void spaced_text(const ui_face * f, const char * s, int x, int base, const unsigned char * rp, int sp) {
  for (; *s; ++s) x += ui_draw_glyph(f, (unsigned char)*s, x, base, rp, 0) + sp;
}

// battery level 0-4: boot_GetBatteryStatus takes about 150 ms, so it is read at startup and
// when the calculator has been idle for a few seconds (focus_idle), never while keys are handled
static int battery_lv = 4;
static void read_battery() {
#ifdef TICE
  battery_lv = boot_GetBatteryStatus();
#endif
}

static void status_draw(int force) {
  if (!focus_on) return;
  unsigned char ph = focus_phase;
  focus_phase = 12;
  char buf[sizeof(stat_l)];
  if (stat_view) { strncpy(buf, stat_view, sizeof(buf) - 1); buf[sizeof(buf) - 1] = 0; }
  else if (hist_sel >= 0) {
    strcpy(buf, "HISTORY  ");
    int n = hist_sel + 1, t = NE, k = strlen(buf);
    if (n >= 10) buf[k++] = '0' + n / 10;
    buf[k++] = '0' + n % 10; buf[k++] = ' '; buf[k++] = '/'; buf[k++] = ' ';
    if (t >= 10) buf[k++] = '0' + t / 10;
    buf[k++] = '0' + t % 10; buf[k] = 0;
  } else strcpy(buf, os_get_angle_unit() ? "RAD    EXACT" : "DEG    EXACT");
  // a view types letters directly (the command search locks alpha): no 2nd/alpha chip there
  int fl = stat_view ? 0 : os_key_flags() & 15, lv = battery_lv, r = fl | lv << 4;
  int dl = force || strcmp(buf, stat_l), dm = force || strcmp(smsg, stat_m), dr = force || r != stat_r;
  if (!dl && !dm && !dr) { focus_phase = ph; return; }
  focus_phase = 13;
  if (dl) {
    strcpy(stat_l, buf);
    ui_clip(0, 0, SZ_L, SB);
    ui_fill(0, 0, SZ_L, SB, col(UC_BG));
    spaced_text(&ui_tb9, buf, 8, 12, ramp(UC_SUB, UC_BG), 1);
  }
  if (dm) {
    strcpy(stat_m, smsg);
    ui_clip(SZ_L, 0, SZ_R, SB);
    ui_fill(SZ_L, 0, SZ_R - SZ_L, SB, col(UC_BG));
    if (smsg[0]) ui_text(&ui_tb9, smsg, (SZ_L + SZ_R) / 2, 12, UC_ACC, UC_BG, 1);
  }
  if (dr) {
    stat_r = r;
    ui_clip(SZ_R, 0, UI_W, SB);
    ui_fill(SZ_R, 0, UI_W - SZ_R, SB, col(UC_BG));
    if (fl & 3) { // 2nd / alpha chip
      const char * t = (fl & 1) ? "2nd" : (fl & 4) ? ((fl & 8) ? "a-lock" : "A-LOCK") : ((fl & 8) ? "a" : "A");
      int w = ui_text_width(&ui_tb9, t, -1) + 10, c = (fl & 1) ? UC_ACC : UC_GREEN, x = UI_W - 34 - w;
      ui_rrect(0, x, 2, w, 12, 4, c, UC_BG);
      ui_text(&ui_tb9, t, x + w / 2, 11, UC_WHITE, c, 1);
    }
    else { // the app's name by the battery (the 2nd/alpha chip takes its place)
      int x = UI_W - 30 - ui_text_width(&ui_tb9, "FlowCE", -1);
      x += ui_text(&ui_tb9, "Flow", x, 12, UC_SUB, UC_BG, 0);
      ui_text(&ui_tb9, "CE", x, 12, UC_ACC, UC_BG, 0);
    }
    int bx = UI_W - 24; // battery
    ui_rframe(0, bx, 4, 15, 8, 2, UC_SUB, UC_BG);
    ui_fill(bx + 15, 6, 1, 4, col(UC_SUB));
    ui_fill(bx + 2, 6, lv > 0 ? (lv * 11 + 3) / 4 : 1, 4, col(lv <= 1 ? UC_ACC : UC_SUB));
  }
  ui_noclip();
  focus_phase = ph;
}
extern "C" void focus_status(void) { status_draw(0); }
void focus_status_label(const char * s) { stat_view = s; status_draw(0); }

extern "C" void focus_status_msg(const char * msg) {
  char m[sizeof(smsg)];
  if (!msg || !strcmp(msg, session_filename)) m[0] = 0;
  else { strncpy(m, msg, sizeof(m) - 1); m[sizeof(m) - 1] = 0; }
  if (!strcmp(m, smsg)) return;
  strcpy(smsg, m);
  status_draw(0);
}

// The standard layer is the prototype's: an icon and a word per key (algebra, calculus, trig,
// symbols or forms, more), the menus being Focus popovers (focus_menu.cc); 2nd and alpha show
// KhiCAS's own menus, as text. (The IC_* icons are listed in focus.h.)
#define P16(a, b) X + (int)((a) * 16), Y + (int)((b) * 16)
// the prototype's icons, strokes in a 16 x 16 box centered at x, cy
void focus_icon(int ic, int x, int cy, int bank, int c, int bg) {
  const unsigned char * r = ui_ramp(bank, c, bg);
  int X = (x - 8) * 16, Y = (cy - 8) * 16, th = 22;
  switch (ic) {
  case IC_X2: {
    mi_layout L;
    mi_build("x^2", 3, -1, ui_math_metrics(6, 0), L);
    ui_math_draw(L, "x^2", 6, x - L.width / 2, cy + 4, bank, c, bg, c);
  } break;
  case IC_INT: {
    int q[] = {P16(10, 2.5), P16(8.6, 1.3), P16(7.1, 2.1), P16(6.8, 4), P16(6.2, 12), P16(5.9, 14.1), P16(4.4, 14.9), P16(3, 13.5)};
    ui_poly16(q, 8, th, r);
  } break;
  case IC_WAVE: { // a period of sine: 8 - 4.5 sin(2 pi i / 14), 1/16 px
    static const unsigned char sy[15] = {128, 97, 72, 58, 58, 72, 97, 128, 159, 184, 198, 198, 184, 159, 128};
    int q[30];
    for (int k = 0; k < 15; ++k) { q[2 * k] = X + (1 + k) * 16; q[2 * k + 1] = Y + sy[k]; }
    ui_poly16(q, 15, th, r);
  } break;
  case IC_PI: { // the math font's pi
    const ui_glyph * g = ui_glyph_of(&ui_mu17, 0x3c0);
    if (g) ui_draw_glyph(&ui_mu17, 0x3c0, x - g->adv / 2, cy + 5, r, 0);
  } break;
  case IC_FORMS: {
    int a[] = {P16(2, 5.5), P16(13, 5.5), P16(10.5, 3)}, b[] = {P16(14, 10.5), P16(3, 10.5), P16(5.5, 13)};
    ui_poly16(a, 3, th, r); ui_poly16(b, 3, th, r);
  } break;
  case IC_MORE:
    for (int k = 0; k < 3; ++k) ui_rrect(bank, x - 7 + 5 * k, cy - 1, 3, 3, 1, c, bg);
    break;
  case IC_SEARCH: { // a circle of radius 4.2 around (7, 7) and a handle
    static const signed char cx[13] = {67, 61, 45, 22, -2, -27, -48, -63, -67, -59, -42, -19, 67}, cy2[13] = {0, 25, 47, 63, 67, 62, 45, 23, -2, -31, -52, -64, 0};
    int q[26];
    for (int k = 0; k < 13; ++k) { q[2 * k] = X + 112 + cx[k]; q[2 * k + 1] = Y + 112 + cy2[k]; }
    ui_poly16(q, 13, th, r);
    int h[] = {P16(10.2, 10.2), P16(14, 14)};
    ui_poly16(h, 2, th + 4, r);
  } break;
  }
}
#undef P16

static int forms_key(); // F4 shows forms (a result is selected, or shown large)
static const char * const tab_word[5] = {"algebra", "calculus", "trig", "symbols", "more"};
static const unsigned char tab_icon[5] = {IC_X2, IC_INT, IC_WAVE, IC_PI, IC_MORE};

// the tab of F-key i in the standard layer; on: its popover is open (drawn bright, bank 1)
void focus_tab(int i, int on, int bank) {
  int cx = 32 + 64 * i, by = SBOT + 15, fm = i == 3 && forms_key();
  const char * w = fm ? "forms" : tab_word[i];
  ui_clip(cx - 32, SBOT + 1, cx + 32, UI_H);
  ui_fill(cx - 32, SBOT + 1, 64, MB - 1, ui_col(bank, UC_BAR));
  if (on) ui_rrect(bank, cx - 30, SBOT + 3, 60, MB - 5, 6, UC_ACCSOFT, UC_BAR);
  const ui_face * f = on ? &ui_tb10 : &ui_tr10;
  int bg = on ? UC_ACCSOFT : UC_BAR, lw = ui_text_width(f, w, -1), x0 = cx - (16 + lw) / 2;
  focus_icon(fm ? IC_FORMS : tab_icon[i], x0 + 7, by - 4, bank, UC_ACC, bg);
  ui_draw_text(f, w, -1, x0 + 16, by, ui_ramp(bank, on ? UC_ACC : UC_BARINK, bg), 0);
  ui_noclip();
}

static int bar_keyflag;
static char bar_sig[100];
static void bar_draw(int keyflag, int force) {
  unsigned char ph = focus_phase;
  focus_phase = 10;
  bar_keyflag = keyflag;
  int layer = keyflag == 1 ? 1 : (keyflag & 0xc) ? 2 : 0;
  char lab[5][20], sig[sizeof(bar_sig)];
  int k = 0;
  sig[k++] = '0' + layer;
  if (!layer) sig[k++] = '0' + forms_key();
  else for (int i = 0; i < 5; ++i) { // labels without the padding of the classic bar
    const char * p = console_fkey_label(layer, i);
    while (*p == ' ') ++p;
    int n = strlen(p);
    while (n && p[n - 1] == ' ') --n;
    if (n > 19) n = 19;
    memcpy(lab[i], p, n); lab[i][n] = 0;
    if (k + n + 2 < (int)sizeof(sig)) { memcpy(sig + k, lab[i], n); k += n; sig[k++] = '|'; }
  }
  sig[k] = 0;
  if (!force && !strcmp(sig, bar_sig)) { focus_phase = ph; return; }
  strcpy(bar_sig, sig);
  focus_phase = 11;
  int band = ui_band_open(MB) >= MB; // no blank frame when the labels change
  if (band) ui_band_begin(SBOT, UI_H);
  ui_clip(0, SBOT, UI_W, UI_H);
  ui_fill(0, SBOT, UI_W, MB, col(UC_BAR));
  ui_fill(0, SBOT, UI_W, 1, col(UC_LINE));
  if (!layer) for (int i = 0; i < 5; ++i) focus_tab(i, 0, 0);
  else {
    int fg = layer == 1 ? UC_ACC : UC_GREEN;
    for (int i = 0; i < 5; ++i) ui_text(&ui_tr10, lab[i], 32 + 64 * i, SBOT + 15, fg, UC_BAR, 1);
  }
  ui_noclip();
  if (band) ui_band_end();
  ui_band_close();
  focus_phase = ph;
}
void focus_bar(int keyflag) { bar_draw(keyflag, 0); }

// the bar as 5 text tabs (the command search's categories), tab on drawn as an open menu's tab
void focus_bar_tabs(const char * const * labels, int on) {
  int band = ui_band_open(MB) >= MB;
  if (band) ui_band_begin(SBOT, UI_H);
  ui_clip(0, SBOT, UI_W, UI_H);
  ui_fill(0, SBOT, UI_W, MB, col(UC_BAR));
  ui_fill(0, SBOT, UI_W, 1, col(UC_LINE));
  for (int i = 0; i < 5; ++i) {
    int cx = 32 + 64 * i, bg = i == on ? UC_ACCSOFT : UC_BAR;
    if (i == on) ui_rrect(0, cx - 30, SBOT + 3, 60, MB - 5, 6, UC_ACCSOFT, UC_BAR);
    ui_text(i == on ? &ui_tb10 : &ui_tr10, labels[i], cx, SBOT + 15, i == on ? UC_ACC : UC_BARINK, bg, 1);
  }
  ui_noclip();
  if (band) ui_band_end();
  ui_band_close();
  bar_sig[0] = 0; // the console's own bar is drawn again by the next bar_draw
}

// ------------------------------------------------------------------ the stage
// A model (what the stage shows) is computed first; painting rows draws whatever intersects them.
// Repaints go through the RAM strip (ui_band_*), so the screen never shows a blank frame:
// typing repaints the expression's rows, a move in the history the entries whose selection
// changed, and a scroll moves the pixels (memmove, gliding in steps) and paints the rows uncovered.
struct model { int hist, sel, selout, col_n, col_h, scroll, hero_y, ll; };
static model M, PM = {0, -1, 0, 0, 0, 0, 0, -1}; // the model, the one on screen (ll = -1: none)

static void stage_paint(int y0, int y1) { // rows [y0, y1) of the stage, in the current band
  ui_clip(0, y0, UI_W, y1);
  ui_fill(0, y0, UI_W, y1 - y0, col(UC_BG));
  int bot = M.hist ? SBOT : M.hero_y, yy = ST - M.scroll;
  for (int k = 0; k < M.col_n; ++k) {
    const fent & e = E[k];
    if (yy + e.h > y0 && yy < y1 && yy < bot) {
      ui_clip(0, y0 > ST ? y0 : ST, UI_W, y1 < bot ? y1 : bot);
      draw_entry(e, yy, k == M.sel, M.selout);
    }
    yy += e.h;
  }
  if (M.hist) {
    if (yy < y1) { ui_clip(0, y0 > yy ? y0 : yy, UI_W, y1); hero_peek_paint(yy); }
  } else if (M.hero_y < y1) {
    ui_clip(0, y0 > M.hero_y ? y0 : M.hero_y, UI_W, y1);
    hero_paint();
  }
  ui_noclip();
}

static void stage_rows(int y0, int y1) { // repaints rows [y0, y1) through the strip
  if (y0 < ST) y0 = ST;
  if (y1 > SBOT) y1 = SBOT;
  if (y0 >= y1) return;
  int rows = ui_band_open(64);
  if (!rows) { stage_paint(y0, y1); return; } // no memory: directly
  for (int b = y0; b < y1; b += rows) {
    int e = b + rows < y1 ? b + rows : y1;
    ui_band_begin(b, e);
    unsigned char ph = focus_phase;
    focus_phase = 30;
    stage_paint(b, e);
    focus_phase = 31;
    ui_band_end();
    focus_phase = ph;
  }
}

static int entry_top(int k) { int y = ST - M.scroll; for (int i = 0; i < k; ++i) y += E[i].h; return y; }

// The screen still shows the Focus stage: it only uses palette entries >= 128, every other
// KhiCAS screen (menus, graphs, the editor) only < 128.
static int screen_is_ours() { // a grid of 6 x 6 points: a message box over the stage hits some
  for (int y = 3; y < UI_H; y += 46)
    for (int x = 3; x < UI_W; x += 62)
      if (ui_fb[y * UI_W + x] < 128) return 0;
  return 1;
}

// Calculating: enter on a long calculation left the prompt ("Type a calculation") on screen for
// seconds, no sign of work (the user, 2026-10-07). While giac works, its control_c checks call
// focus_busy_tick (main.cc sets giac::control_c_hook): after 0.4 s the prompt becomes
// "Calculating" over three dots lit in turn, 4 a second. A quick answer shows nothing.
static long busy_t0 = -1; // clock() (32768 Hz) when the calculation started; -1: none
static signed char busy_fr;
void focus_busy(int on) {
#ifdef TICE
  busy_t0 = on ? (long)(clock)() : -1; // ((clock): giac's first.h makes clock() 0)
  busy_fr = -1;
#endif
}
bool focus_busy_tick() {
#ifdef TICE
  static unsigned char calls;
  if (busy_t0 < 0 || (++calls & 7)) return false; // the clock every 8 checks (it cost 3% of a sum)
  long t = (long)(clock)() - busy_t0;
  if (t < 13107) return false; // 0.4 s
  int fr = (int)((t >> 13) % 3);
  if (fr == busy_fr) return false;
  int m = hy0 + hh / 2 - 2, b = m + 20; // where hero_paint wrote the prompt
  if (busy_fr < 0) {
    if (hm != HM_HINT || M.hist || !screen_is_ours()) { busy_t0 = -1; return false; }
    ui_noclip();
    ui_fill(0, m - 18, UI_W, b + 10 - (m - 18), col(UC_BG)); // the prompt and its key hints
    ui_text(&ui_tr12, "Calculating", UI_W / 2, m, UC_SUB, UC_BG, 1);
  }
  for (int k = 0; k < 3; ++k) ui_rrect(0, UI_W / 2 - 15 + 12 * k, b - 9, 6, 6, 3, k == fr ? UC_ACC : UC_LINE, UC_BG);
  busy_fr = (signed char)fr;
#endif
  return false;
}

static void compute_model() {
  focus_phase = 20;
  scan();
  focus_phase = 6;
  int cl = console_caret() < 0 ? Start_Line + Cursor.y : -1; // a history line, or -1
  M.hist = cl >= 0 && cl < Last_Line && NE > 0;
  const char * s = (const char *)Console_GetEditLine();
  if (s && *s) hero_last = 0;
  int show_last = !M.hist && hero_last && NE && (!s || !*s);
  M.col_n = show_last ? NE - 1 : NE;
  int y = 0;
  for (int k = 0; k < M.col_n; ++k) y += E[k].h;
  M.col_h = y;
  M.sel = M.hist ? entry_of_line(cl) : -1; M.selout = 0;
  if (M.sel >= 0) M.selout = cl != E[M.sel].in;
  hist_sel = M.sel;
  if (M.hist) {
    int top = 0;
    for (int k = 0; k < M.sel; ++k) top += E[k].h;
    int bot = top + E[M.sel].h, view = SH - PEEK;
    if (!PM.hist) hscroll = M.col_h + PEEK - SH;
    if (top < hscroll + 4) hscroll = top - 4;
    if (bot > hscroll + view) hscroll = bot - view;
    int lo = M.col_h + PEEK - SH < -4 ? M.col_h + PEEK - SH : -4, hi = M.col_h + PEEK - SH;
    if (hscroll > hi) hscroll = hi;
    if (hscroll < lo) hscroll = lo;
    M.scroll = hscroll;
  } else {
    M.scroll = M.col_n ? M.col_h - (E[M.col_n - 1].outh + 12) : 0;
    if (M.col_n && E[M.col_n - 1].out < 0) M.scroll = M.col_h - (E[M.col_n - 1].inh + 12);
  }
  M.hero_y = ST + M.col_h - M.scroll;
  if (!M.hist && M.hero_y < ST) M.hero_y = ST;
  M.ll = Last_Line;
}

// the stage content moves up by d rows (down if d < 0): the pixels are moved, the rows uncovered
// painted; a long scroll glides in 2 steps
static void scroll_stage(int from, int to) {
  int d = to - from;
  int steps = d > 40 || d < -40 ? 2 : 1;
  static const unsigned char pct[2] = {60, 100};
  int cur = from;
  for (int i = 0; i < steps; ++i) {
    int nxt = steps == 1 ? to : from + d * pct[i] / 100, dd = nxt - cur;
    M.scroll = nxt;
    if (dd >= SH / 2 || dd <= -SH / 2) stage_rows(ST, SBOT);
    else if (dd > 0) {
      memmove(ui_fb + ST * UI_W, ui_fb + (ST + dd) * UI_W, (SH - dd) * UI_W);
      stage_rows(SBOT - dd, SBOT);
    } else if (dd < 0) {
      memmove(ui_fb + (ST - dd) * UI_W, ui_fb + ST * UI_W, (SH + dd) * UI_W);
      stage_rows(ST, ST - dd);
    }
    cur = nxt;
  }
}

bool focus_hold; // the start screen stays while the session loads (it painted half a console)
void focus_disp(int mode) {
  if (focus_hold) return;
  focus_phase = 1;
  const char * es = (const char *)Console_GetEditLine();
  int nonempty = es && *es;
  (void)mode; // KhiCAS asks for full redraws on most cursor moves: what changed is computed here
  ui_set_theme(theme); // cheap; repairs entries 128-255 if anything reset the palette
  int ours = PM.ll == Last_Line && screen_is_ours();
  if (ours && !PM.hist && hm != HM_RESULT && console_caret() >= 0 && nonempty && console_edit2d()) {
    // typing: only the expression changed (the entries above did not move; after a result shown
    // large, the first key takes the full path: that entry goes back into the column)
    int was_math = hm == HM_MATH, oy0 = hb_y0, oy1 = hb_y1;
    hero_prepare(M.hero_y, SBOT - M.hero_y);
    if (was_math) stage_rows(oy0 < hb_y0 ? oy0 : hb_y0, oy1 > hb_y1 ? oy1 : hb_y1);
    else stage_rows(M.hero_y, SBOT);
    ui_band_close();
    focus_phase = 0;
    return;
  }
  if (!nonempty) hero_lv = HERO_LV; // a new expression starts large
  compute_model();
  if (ours && M.hist && PM.hist && M.col_n == PM.col_n && M.col_h == PM.col_h) {
    // a move in the history: scroll if needed, then the entries whose selection changed
    focus_phase = 21;
    if (M.scroll != PM.scroll) {
      int to = M.scroll;
      scroll_stage(PM.scroll, to);
    }
    focus_phase = 22;
    if (M.sel != PM.sel || M.selout != PM.selout) {
      if (PM.sel >= 0 && PM.sel < M.col_n && PM.sel != M.sel) { int t = entry_top(PM.sel); stage_rows(t, t + E[PM.sel].h); }
      if (M.sel >= 0) { int t = entry_top(M.sel); stage_rows(t, t + E[M.sel].h); }
    }
    focus_phase = 23;
    ui_band_close();
    status_draw(0);
    PM = M;
    focus_phase = 0;
    return;
  }
  status_draw(1);
  if (!M.hist) hero_prepare(M.hero_y, SBOT - M.hero_y);
  focus_phase = 7;
  stage_rows(ST, SBOT);
  ui_band_close();
  focus_phase = 8;
  bar_draw(bar_keyflag, 1);
  PM = M;
  focus_phase = 0;
}

// F4 cycles the forms of a result: one selected in the history, or the last one shown large
int focus_result_line() {
  int cl = console_caret() < 0 ? Start_Line + Cursor.y : -1; // the history line of the cursor
  if (cl >= 0 && cl < Last_Line && Line[cl].type == LINE_TYPE_OUTPUT) return cl;
  return hm == HM_RESULT && NE ? E[NE - 1].out : -1;
}
static int forms_key() { return focus_result_line() >= 0; }
// History by calculations, as the prototype: the console line to select for dir 0 up, 1 down,
// 2 left (the input), 3 right (the result); Last_Line for the edit line. Up and down keep the
// part (input or result) that is selected.
int focus_hist_line(int dir) {
  focus_status_msg(0); // clears "Oldest calculation"
  scan();
  if (!NE) return Last_Line;
  int cl = console_caret() < 0 ? Start_Line + Cursor.y : Last_Line, k = cl < Last_Line ? entry_of_line(cl) : NE;
  int in = k < NE && cl == E[k].in;
  if (dir == 2 || dir == 3) {
    if (k >= NE) return cl;
    int l = dir == 2 ? E[k].in : E[k].out;
    return l >= 0 ? l : cl;
  }
  int k2 = dir == 0 ? (k > 0 ? k - 1 : -1) : k + 1;
  if (k2 < 0) { focus_status_msg("Oldest calculation"); return cl; }
  if (k2 >= NE) return Last_Line;
  int l = in ? E[k2].in : E[k2].out;
  return l >= 0 ? l : (E[k2].out >= 0 ? E[k2].out : E[k2].in);
}

void focus_repaint(int y0, int y1) { stage_rows(y0, y1); ui_band_close(); }
void focus_bar_redraw() { bar_draw(bar_keyflag, 1); }
void focus_bar_reset() { bar_draw(0, 1); }
int focus_screen() { return screen_is_ours(); }
int focus_view;
// a full-screen view (the command search) drew over everything with entries >= 128, so
// screen_is_ours() cannot tell: forget what is on screen
void focus_invalidate() { PM.ll = -1; }

void focus_init() {
  const char * t = read_file("FocusUI"); // the theme chosen last
  theme = t && t[0] == 'N' ? &ui_theme_night : &ui_theme_paper;
  ui_set_theme(theme);
  read_battery();
}
// every Focus pixel is a palette entry: the new theme shows at once, without a redraw
void focus_toggle_theme() {
  theme = theme == &ui_theme_night ? &ui_theme_paper : &ui_theme_night;
  ui_set_theme(theme);
  write_file("FocusUI", theme == &ui_theme_night ? "N" : "P", 2);
}
extern "C" void focus_idle(void) { // getkey, after a few idle seconds
  if (!focus_on) return;
  read_battery();
  status_draw(0);
}
void focus_evaluated() { hero_last = 1; }
int focus_clear_hero() {
  if (!hero_last) return 0;
  hero_last = 0;
  focus_disp(1);
  return 1;
}
const mi_metrics & focus_metrics() { return ui_math_metrics(hero_lv, MI_F_CALLBOX | MI_F_HINTS); } // as drawn
