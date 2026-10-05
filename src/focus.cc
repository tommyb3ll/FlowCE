// focus.cc - the Focus interface of the console (focus.h; the design and its prototype are in
// notes/ui). The console's lines are read as entries (an input line and its output lines); the
// edit line is drawn large and centered, entries are stacked above it, the last result is shown
// large right after an evaluation. Cursor moves into the history select an entry (its input or
// its result). All text is drawn with ui_math (STIX Two, 4 shades) and ui_font (Atkinson).
#include <string.h>
#include <stdlib.h>
#include "console.h"           // Line[], Last_Line, Cursor, menus; maps std to ustl
#include "focus.h"
#include "ui_gfx.h"
#include "ui_font.h"
#include "ui_fontdata.h"
#include "ui_math.h"
#ifdef TICE
#include <sys/power.h>
#endif

extern "C" int os_get_angle_unit();
extern "C" int os_key_flags();      // k_csdk.c: 1 2nd, 2 alpha, 4 alpha lock, 8 lowercase
bool console_edit2d();              // console.cc: the edit line is math (not Python)
int console_caret();                // console.cc: caret index in the edit line, -1 if in the history

int focus_on = 1;

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
static void draw_math(const char * s, const fitc & c, int x, int base, int ink, int bg, int flags) {
  mi_layout L;
  mi_build(s, strlen(s), -1, ui_math_metrics(c.lv, flags), L);
  ui_math_draw(L, s, c.lv, x, base, 0, ink, bg, UC_ACC);
}

// messages ("Done", "f(x) defined", warnings) rather than math
static bool is_text(const char * s) {
  if (!*s) return true;
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
// an antiderivative: integrate(f,x) or int(f,x) with 2 arguments: its result gets "+ C"
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
  return commas == 1;
}

static int ui_text(const ui_face * f, const char * s, int x, int base, int fg, int bg, int align) {
  int w = ui_text_width(f, s, -1);
  if (align == 1) x -= w / 2;
  else if (align == 2) x -= w;
  ui_draw_text(f, s, -1, x, base, ramp(fg, bg), 0);
  return w;
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
    if (e.in >= 0) { fitc c = fitted(Line[e.in].str, IN_LV, W_IN, 34, 0); e.inh = c.a + c.d; }
    for (int l = e.m0; l >= 0 && l < e.out; ++l)
      if (Line[l].type != LINE_TYPE_CONT && e.msgs < 2) ++e.msgs;
    if (e.out >= 0) {
      const char * s = Line[e.out].str;
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
  const char * s = Line[e.out].str;
  if (is_text(s)) { ui_text(&ui_tr10, s, right, base, UC_SUB, bg, 2); return; }
  fitc c = fitted(s, OUT_LV, W_OUT, 64, MI_F_IMPLDOT);
  int cw = 0;
  if (e.in >= 0 && is_antideriv(Line[e.in].str)) { // "+ C" in the secondary color
    const ui_face * f = &ui_mu17;
    cw = ui_text_width(f, "+ ", -1) + ui_text_width(&ui_mi17, "C", -1) + 4;
    int x = right - cw + 4;
    x += ui_draw_text(f, "+ ", -1, x, base, ramp(UC_SUB, bg), 0);
    ui_draw_text(&ui_mi17, "C", -1, x, base, ramp(UC_SUB, bg), 0);
  }
  draw_math(s, c, right - cw - c.w, base, ink, bg, MI_F_IMPLDOT);
}

static void draw_entry(const fent & e, int y, int sel, int selout) {
  int bg = sel ? UC_ACCSOFT : UC_BG;
  if (sel) ui_rrect(0, 5, y + 2, UI_W - 10, e.h - 4, 7, UC_ACCSOFT, UC_BG);
  int b = y + 7;
  if (e.in >= 0) {
    fitc c = fitted(Line[e.in].str, IN_LV, W_IN, 34, 0);
    draw_math(Line[e.in].str, c, 12, b + c.a, sel && !selout ? UC_ACC : UC_SUB, bg, 0);
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
    const char * s = Line[e.out].str;
    int a = is_text(s) ? 11 : fitted(s, OUT_LV, W_OUT, 64, MI_F_IMPLDOT).a;
    draw_result(e, UI_W - 12, b + a, sel && selout ? UC_ACC : UC_INK, bg);
  }
  if (sel) ui_rframe(0, 5, y + 2, UI_W - 10, e.h - 4, 7, UC_ACC, UC_BG);
  else ui_fill(12, y + e.h - 1, UI_W - 24, 1, col(UC_LINE));
}

// ------------------------------------------------------------------ hero (the edit line)
static void key_hint(int x, int base, const char * k, const char * label) {
  int w = ui_text_width(&ui_tb9, k, -1) + 8;
  ui_rframe(0, x, base - 9, w, 12, 3, UC_SUB, UC_BG);
  ui_text(&ui_tb9, k, x + w / 2, base, UC_SUB, UC_BG, 1);
  ui_text(&ui_tr9, label, x + w + 4, base, UC_SUB, UC_BG, 0);
}

static void draw_hero(int y0, int h) {
  const char * s = (const char *)Console_GetEditLine();
  int caret = console_caret();
  if (s && *s && console_edit2d()) { // the layout first, then clear and draw at once (no blank frame)
    mi_layout L;
    hero_lv = ui_math_refit(s, strlen(s), caret, HERO_LV, hero_lv, 296, h - 24 > 20 ? h - 24 : 20, 0, L);
    ui_clip(0, y0 < ST ? ST : y0, UI_W, SBOT);
    ui_fill(0, y0, UI_W, h, col(UC_BG));
    {
      int x = (UI_W - L.width) / 2, base = y0 + (h - (L.asc + L.desc)) / 2 + L.asc;
      if (L.width > 296) { // caret kept in view
        x = UI_W / 2 - L.cx;
        if (x > 12) x = 12;
        if (x + L.width < UI_W - 12) x = UI_W - 12 - L.width;
      }
      ui_math_draw(L, s, hero_lv, x, base, 0, UC_INK, UC_BG, UC_ACC);
      if (caret >= 0) ui_math_caret(L, x, base, 0, UC_ACC);
    }
    ui_noclip();
    return;
  }
  ui_clip(0, y0 < ST ? ST : y0, UI_W, SBOT);
  ui_fill(0, y0, UI_W, h, col(UC_BG));
  if (s && *s) { // Python: plain text
    ui_text(&ui_tr12, s, 12, y0 + h / 2 + 4, UC_INK, UC_BG, 0);
  } else if (hero_last && NE && E[NE - 1].out >= 0) {
    const fent & e = E[NE - 1];
    int top = y0 + 10;
    if (e.in >= 0) {
      fitc c = fitted(Line[e.in].str, 4, 292, 36, 0);
      draw_math(Line[e.in].str, c, (UI_W - c.w) / 2, top + c.a, UC_SUB, UC_BG, 0);
      top += c.a + c.d + 6;
    }
    int bot = y0 + h - 14;
    const char * r = Line[e.out].str;
    if (is_text(r)) ui_text(&ui_tr12, r, UI_W / 2, (top + bot) / 2 + 4, UC_SUB, UC_BG, 1);
    else {
      int anti = e.in >= 0 && is_antideriv(Line[e.in].str);
      fitc c = fitted(r, HERO_LV, anti ? 256 : 292, bot - top > 18 ? bot - top : 18, MI_F_IMPLDOT);
      int cw = anti ? ui_text_width(&ui_mu24, "+ ", -1) + ui_text_width(&ui_mi24, "C", -1) + 6 : 0;
      int x = (UI_W - c.w - cw) / 2, base = top + (bot - top - (c.a + c.d)) / 2 + c.a;
      draw_math(r, c, x, base, UC_INK, UC_BG, MI_F_IMPLDOT);
      if (anti) {
        int cx = x + c.w + 6;
        cx += ui_draw_text(&ui_mu24, "+ ", -1, cx, base, ramp(UC_SUB, UC_BG), 0);
        ui_draw_text(&ui_mi24, "C", -1, cx, base, ramp(UC_SUB, UC_BG), 0);
      }
    }
    // the forms key
    int cw2 = ui_text_width(&ui_tb9, "forms", -1) + ui_text_width(&ui_tb9, "F4", -1) + 20, cx = (UI_W - cw2) / 2, cy = y0 + h - 13;
    ui_rrect(0, cx, cy - 10, cw2, 13, 6, UC_ACCSOFT, UC_BG);
    int w1 = ui_text(&ui_tb9, "forms", cx + 6, cy, UC_ACC, UC_ACCSOFT, 0);
    ui_text(&ui_tb9, "F4", cx + 12 + w1, cy, UC_SUB, UC_ACCSOFT, 0);
  } else {
    int m = y0 + h / 2;
    ui_text(&ui_tr12, "Type a calculation", UI_W / 2, m, UC_SUB, UC_BG, 1);
    key_hint(62, m + 20, "math", "templates");
    key_hint(170, m + 20, "up", "history");
  }
  ui_noclip();
}

static void draw_hero_peek(int y) { // history mode: the edit line, small, at the bottom
  if (y >= SBOT) return;
  ui_clip(0, y, UI_W, SBOT);
  ui_fill(0, y, UI_W, SBOT - y, col(UC_BG));
  ui_fill(10, y + 2, UI_W - 20, 1, col(UC_LINE));
  const char * s = (const char *)Console_GetEditLine();
  int b = y + 20;
  if (s && *s && console_edit2d()) { fitc c = fitted(s, 5, 230, 22, 0); draw_math(s, c, 14, b, UC_SUB, UC_BG, 0); }
  else ui_text(&ui_tr10, "New calculation", 14, b, UC_SUB, UC_BG, 0);
  ui_text(&ui_tb9, "down: back", UI_W - 12, b, UC_SUB, UC_BG, 2);
  ui_noclip();
}

// ------------------------------------------------------------------ status bar and F-key bar
extern "C" void focus_status_msg(const char * msg) {

  if (!msg || !strcmp(msg, session_filename)) smsg[0] = 0;
  else { strncpy(smsg, msg, sizeof(smsg) - 1); smsg[sizeof(smsg) - 1] = 0; }
  focus_status();
}

static int hist_sel = -1; // entry selected in the history, -1 in edit mode

extern "C" void focus_status(void) {
  if (!focus_on) return;
  ui_clip(0, 0, UI_W, SB);
  ui_fill(0, 0, UI_W, SB, col(UC_BG));
  char buf[24];
  if (hist_sel >= 0) {
    strcpy(buf, "HISTORY  ");
    int n = hist_sel + 1, t = NE, k = strlen(buf);
    if (n >= 10) buf[k++] = '0' + n / 10;
    buf[k++] = '0' + n % 10; buf[k++] = ' '; buf[k++] = '/'; buf[k++] = ' ';
    if (t >= 10) buf[k++] = '0' + t / 10;
    buf[k++] = '0' + t % 10; buf[k] = 0;
  } else strcpy(buf, os_get_angle_unit() ? "RAD    CAS" : "DEG    CAS");
  ui_text(&ui_tb9, buf, 8, 12, UC_SUB, UC_BG, 0);
  int fl = os_key_flags(), x = UI_W - 30;
  if (fl & 3) { // 2nd / alpha chip
    const char * t = (fl & 1) ? "2nd" : (fl & 4) ? ((fl & 8) ? "a-lock" : "A-LOCK") : ((fl & 8) ? "a" : "A");
    int w = ui_text_width(&ui_tb9, t, -1) + 10, c = (fl & 1) ? UC_ACC : UC_GREEN;
    x -= w + 4;
    ui_rrect(0, x, 2, w, 12, 4, c, UC_BG);
    ui_text(&ui_tb9, t, x + w / 2, 11, UC_WHITE, c, 1);
  }
  if (smsg[0]) ui_text(&ui_tb9, smsg, (x + 70) / 2, 12, UC_ACC, UC_BG, 1);
  // battery
  int bx = UI_W - 24, lv = 4;
#ifdef TICE
  lv = boot_GetBatteryStatus();
#endif
  ui_rframe(0, bx, 4, 15, 8, 2, UC_SUB, UC_BG);
  ui_fill(bx + 15, 6, 1, 4, col(UC_SUB));
  ui_fill(bx + 2, 6, lv > 0 ? (lv * 11 + 3) / 4 : 1, 4, col(lv <= 1 ? UC_ACC : UC_SUB));
  ui_noclip();
}

static int bar_keyflag;
void focus_bar(int keyflag) {
  bar_keyflag = keyflag;
  std::string menu(" "), shiftmenu = menu, alphamenu;
  int bgc = 0;
  get_current_console_menu(menu, shiftmenu, alphamenu, bgc, 0);
  const std::string & m = keyflag == 1 ? shiftmenu : (keyflag & 0xc) ? alphamenu : menu;
  ui_clip(0, SBOT, UI_W, UI_H);
  ui_fill(0, SBOT, UI_W, MB, col(UC_BAR));
  ui_fill(0, SBOT, UI_W, 1, col(UC_LINE));
  int fg = keyflag == 1 ? UC_ACC : (keyflag & 0xc) ? UC_GREEN : UC_BARINK;
  const char * p = m.c_str();
  for (int i = 0; i < 5 && *p; ++i) {
    while (*p == ' ') ++p;
    const char * e = p;
    while (*e && *e != '|') ++e;
    const char * t = e;
    while (t > p && t[-1] == ' ') --t;
    char lab[20];
    int n = t - p < 19 ? (int)(t - p) : 19;
    memcpy(lab, p, n); lab[n] = 0;
    ui_text(&ui_tr10, lab, 32 + 64 * i, SBOT + 15, fg, UC_BAR, 1);
    p = *e ? e + 1 : e;
  }
  ui_noclip();
}

// ------------------------------------------------------------------ the stage
static int col_n, col_h, last_full_col_n = -1, last_hero_y = -1, last_mode = -1;

static int fast_ok, last_LL = -1;
void focus_disp(int mode) {
  const char * es = (const char *)Console_GetEditLine();
  int nonempty = es && *es;
  if (!(mode & 1) && fast_ok && nonempty && console_caret() >= 0 && Last_Line == last_LL) {
    draw_hero(last_hero_y, SBOT - last_hero_y); // typing: nothing above the edit line moved
    return;
  }
  if (!nonempty) hero_lv = HERO_LV; // a new expression starts large
  scan();
  int cl = console_caret() < 0 ? Start_Line + Cursor.y : -1; // a history line, or -1
  int hist = cl >= 0 && cl < Last_Line && NE > 0;
  const char * s = (const char *)Console_GetEditLine();
  if (s && *s) hero_last = 0;
  int show_last = !hist && hero_last && NE && (!s || !*s);
  col_n = show_last ? NE - 1 : NE;
  int y = 0;
  for (int k = 0; k < col_n; ++k) y += E[k].h;
  col_h = y;
  int sel = hist ? entry_of_line(cl) : -1, selout = 0;
  if (sel >= 0) selout = cl != E[sel].in;
  hist_sel = sel;
  int scroll;
  if (hist) {
    int top = 0;
    for (int k = 0; k < sel; ++k) top += E[k].h;
    int bot = top + E[sel].h, view = SH - PEEK;
    if (last_mode != 1) hscroll = col_h + PEEK - SH;
    if (top < hscroll + 4) hscroll = top - 4;
    if (bot > hscroll + view) hscroll = bot - view;
    int lo = col_h + PEEK - SH < -4 ? col_h + PEEK - SH : -4, hi = col_h + PEEK - SH;
    if (hscroll > hi) hscroll = hi;
    if (hscroll < lo) hscroll = lo;
    scroll = hscroll;
  } else {
    scroll = col_n ? col_h - (E[col_n - 1].outh + 12) : 0;
    if (col_n && E[col_n - 1].out < 0) scroll = col_h - (E[col_n - 1].inh + 12);
  }
  int hero_y = ST + col_h - scroll;
  if (!hist && hero_y < ST) hero_y = ST;
  last_mode = hist; last_full_col_n = col_n; last_hero_y = hero_y;
  fast_ok = !hist && nonempty; last_LL = Last_Line;
  ui_set_theme(&ui_theme_paper); // cheap; repairs entries 128-255 if anything reset the palette
  focus_status();
  // entries, top to bottom (each draws its own background: no full clear, no flash)
  int clip_bot = hist ? SBOT : hero_y;
  ui_clip(0, ST, UI_W, clip_bot);
  int yy = ST - scroll;
  if (yy > ST) ui_fill(0, ST, UI_W, yy - ST, col(UC_BG));
  for (int k = 0; k < col_n; ++k) {
    const fent & e = E[k];
    if (yy + e.h > ST && yy < clip_bot) {
      ui_clip(0, ST, UI_W, clip_bot);
      ui_fill(0, yy, UI_W, e.h, col(UC_BG));
      draw_entry(e, yy, k == sel, selout);
    }
    yy += e.h;
  }
  ui_noclip();
  if (hist) {
    if (yy < SBOT) draw_hero_peek(yy);
  } else draw_hero(hero_y, SBOT - hero_y);
  focus_bar(bar_keyflag);
}

void focus_init() { ui_set_theme(&ui_theme_paper); }
void focus_evaluated() { hero_last = 1; }
int focus_clear_hero() {
  if (!hero_last) return 0;
  hero_last = 0;
  focus_disp(1);
  return 1;
}
const mi_metrics & focus_metrics() { return ui_math_metrics(hero_lv, 0); }
