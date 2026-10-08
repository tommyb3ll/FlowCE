// focus_catalog.cc - the command search of the Focus interface (focus.h; the prototype's
// drawCatalog in notes/ui): KhiCAS's catalog (menufr.cc) as a full-screen list filtered while
// the query is typed (names starting with it, then names containing it, then descriptions
// containing it), F1-F5 choosing a category, the selected entry's signature and example below.
// A little larger than the prototype: rows of 19 px, names in bold 12 px.
// Speed: a key repaints only what changed (the field's text, the rows whose entry or selection
// changed, the footer when the selected entry changed), each region through its own band (no
// blank frame, no glyph drawn twice), and painting stops as soon as another key is pressed
// (key_waiting, k_csdk.c): the rest is painted after that key, so typing fast loses no key.
#include <string.h>
#include "console.h"           // maps std to ustl
#include "menuGUI.h"
#include "focus.h"
#include "k_defs.h"
#include "ui_gfx.h"
#include "ui_font.h"
#include "ui_fontdata.h"
#include "ui_math.h"

#define ELL "\xe2\x80\xa6" // the ellipsis, U+2026
// timing probe (focus.cc, tools/emu/phases.py): 60 filter, 61 example layout, 62 search field,
// 63 a row, 64 footer, 65 tabs, 66 updating; 0 waiting for a key
extern "C" volatile unsigned char focus_phase;

enum { ST = 16, SBOT = 218, CY = 18, CH = 22, LT = 43, RH = 19, ROWS = 7, LB = LT + ROWS * RH,
       FL = LB + 2, SY = FL + 13, EY = SY + 3, EH = SBOT - EY, DX = 104, QMAX = 15, SEL = 0x4000 };

// the search, on the heap while it is open (KhiCAS's static data is nearly full)
struct cat_state {
  const catalogFunc * C;   // the catalog
  short * idx;             // the matches in order (indices in C)
  unsigned char * cls;     // per entry: its match class 0-2, 3 = none
  int nc, ni, tab, sel, top, ql, exlv;
  char q[QMAX + 1], ex[96]; // the query (lowercase); the selected entry's example
  mi_layout L;             // ... laid out at level exlv (-1: none)
  // what the screen shows: the field's query and count (sn < 0: no card yet), the scrollbar
  // (sb_of), the footer's entry, the list position of the first row (-100: another list), each
  // row's entry (+ SEL when selected; -1 none, -2 unknown)
  char sq[QMAX + 1];
  int sn, sb, sf, stop;
  short shown[ROWS];
};
static cat_state * S;

static const char * const TABS[5] = {"all", "algebra", "calculus", "trig", "numbers"};
static const unsigned char TCAT[4][3] = { // the categories of tabs F2-F5 (menuGUI.h)
  {CAT_CATEGORY_ALGEBRA, CAT_CATEGORY_POLYNOMIAL, CAT_CATEGORY_SOLVE}, {CAT_CATEGORY_CALCULUS, 0, 0},
  {CAT_CATEGORY_TRIG, 0, 0}, {CAT_CATEGORY_ARIT, CAT_CATEGORY_REAL, CAT_CATEGORY_COMPLEXNUM}};

static unsigned char col(int c) { return ui_col(0, c); }
static const unsigned char * ramp(int fg, int bg) { return ui_ramp(0, fg, bg); }
static char low(char c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
static int letter(char c) { return (c | 32) >= 'a' && (c | 32) <= 'z'; }

// the command's name: its signature up to the '(' of a call ("integrate(f,x,[a,b])"), else all
// of it ("a and b", "_(m/s)")
static int name_len(const char * s) {
  const char * p = strchr(s, '(');
  return p && p > s && (letter(p[-1]) || (p[-1] >= '0' && p[-1] <= '9')) ? p - s : strlen(s);
}
// the text an entry inserts: its insert field, else its name up to and including '('
static int insert_text(const catalogFunc & c, const char ** t) {
  if (c.insert) { *t = c.insert; return strlen(c.insert); }
  *t = c.name;
  const char * p = strchr(c.name, '(');
  return p ? p - c.name + 1 : strlen(c.name);
}

// an argument in brackets that may be left out: plain names ("[n]", "[a,b]"), not a list to fill
// in ("[x,y,..]", "[t=a..b]", "[[x1,y1],...]")
static int optional_arg(const char * a, int l) {
  if (l < 2 || a[0] != '[' || a[l - 1] != ']') return 0;
  for (int j = 1; j < l - 1; ++j)
    if (a[j] == '[' || a[j] == '=' || (a[j] == '.' && a[j + 1] == '.')) return 0;
  return 1;
}

// a command's call signature: the catalog entry whose signature is name[0,n) then "(", its
// arguments split at top-level commas (a[k], al[k] chars; at most 12) and how many are required
// (the template's boxes): not "..." (more of the same) nor, at the end, optional arguments in
// brackets ("diff(f,var,[n])": 2), unless every argument is in brackets ("ichinrem([a,m],[b,n])").
// -1 if there is none.
struct call_sig { const catalogFunc * c; const char * a[12]; int al[12], na, req; };
static int call_sig_of(const char * name, int n, call_sig & g) {
  int nc;
  const catalogFunc * C = catalog_entries(nc);
  for (int i = 0; i < nc; ++i) {
    const char * sig = C[i].name;
    if (strncmp(sig, name, n) || sig[n] != '(') continue;
    int depth = 0;
    g.c = C + i; g.na = 0;
    for (const char * p = sig + n + 1, * b = p; *p; ++p) {
      if (*p == '(' || *p == '[' || *p == '{') ++depth;
      else if ((*p == ')' || *p == ']' || *p == '}') && depth) --depth;
      else if ((*p == ',' || *p == ')') && !depth) {
        if (p > b && g.na < 12) { g.a[g.na] = b; g.al[g.na] = p - b; ++g.na; }
        if (*p == ')') break;
        b = p + 1;
      }
    }
    int lists = 1;
    for (g.req = 0; g.req < g.na && !(g.a[g.req][0] == '.' && g.a[g.req][1] == '.'); ++g.req)
      if (g.a[g.req][0] != '[') lists = 0;
    while (!lists && g.req > 0 && optional_arg(g.a[g.req - 1], g.al[g.req - 1])) --g.req;
    return i;
  }
  return -1;
}
static int name_at(const char * s) { // the length of the command name s starts with
  int n = 0;
  while (letter(s[n]) || s[n] == '_' || (n && s[n] >= '0' && s[n] <= '9')) ++n;
  return n;
}

// a call's template: the command s names ("irem(" or "irem"), "(", one empty argument per
// required argument of its signature in the catalog, ")": "irem(a,b)" gives irem(,), which the
// editor draws with a box per argument; a list comes with its brackets: "linsolve([eq1,..],
// [x,..])" gives linsolve([],[]). Returns its length (*caret: where the caret goes, in the first
// box), 0 if the command has no signature.
int focus_call_template(const char * s, char * out, int outsize, int * caret) {
  int n = name_at(s);
  call_sig g;
  if (!n || (s[n] && s[n] != '(') || call_sig_of(s, n, g) < 0 || n + 3 * g.req + 3 > outsize) return 0;
  memcpy(out, s, n);
  int k = n, c = 0;
  out[k++] = '(';
  for (int j = 0; j < g.req; ++j) {
    if (j) out[k++] = ',';
    if (g.a[j][0] == '[') out[k++] = '[';
    if (!j) c = k;
    if (g.a[j][0] == '[') out[k++] = ']';
  }
  if (!g.req) c = k;
  out[k++] = ')';
  out[k] = 0;
  if (caret) *caret = c;
  return k;
}

// the name of argument i of a call to name[0,n), from its signature ("powmod(a,n,p)": a, n, p),
// for the editor's empty boxes (mathinput.h, mi_arg_label): its length, *label pointing at it; 0
// for commands of one argument (a box says enough) and for "...". Optional ones lose their
// brackets ("[n]": n); lists keep theirs ("[x,y,..]").
int focus_arg_label(const char * name, int n, int i, const char ** label) {
  call_sig g;
  if (call_sig_of(name, n, g) < 0 || g.req < 2 || i >= g.na) return 0;
  const char * t = g.a[i];
  int l = g.al[i];
  if (t[0] == '.') return 0;
  if (optional_arg(t, l)) { ++t; l -= 2; }
  while (l > 0 && *t == ',') { ++t; --l; } // "[,a,b]" (the French catalog's integrate)
  *label = t;
  return l;
}

// the arguments of the call whose ( is s[i] (up to its ) or the end of s)
static int call_args(const char * s, int i) {
  int d = 0, n = 1;
  for (const char * p = s + i + 1; *p; ++p) {
    if (*p == '(' || *p == '[' || *p == '{') ++d;
    else if (*p == ')' || *p == ']' || *p == '}') { if (!d--) break; }
    else if (*p == ',' && !d) ++n;
  }
  return n;
}

// the command the caret is in: the innermost call around it (lists and groups inside it skipped)
// whose signature has 2 or more arguments; -1 if none, or if that call is drawn as a template
// (the integral sign, lim, sigma...: what it does is plain to see, the user's choice 2026-10-08).
// For the help line under the edit line.
int focus_call_entry(const char * s, int caret) {
  int d = 0;
  for (int i = caret - 1; i >= 0; --i) {
    char c = s[i];
    if (c == ')' || c == ']' || c == '}') ++d;
    else if (c == '(' || c == '[' || c == '{') {
      if (d) { --d; continue; }
      if (c != '(') continue;
      int b = i;
      while (b > 0 && (letter(s[b - 1]) || s[b - 1] == '_' || (s[b - 1] >= '0' && s[b - 1] <= '9'))) --b;
      while (b < i && !letter(s[b])) ++b; // 2irem(: the name starts at its first letter
      call_sig g;
      int e = b < i ? call_sig_of(s + b, i - b, g) : -1;
      if (e >= 0 && g.req >= 2) return mi_drawn_as_template(s + b, i - b, call_args(s, i)) ? -1 : e;
    }
  }
  return -1;
}

// ------------------------------------------------------------------ matching
// the query (lowercase, ql chars) at s, ignoring case
static int at(const char * s, const char * q, int ql) {
  for (int j = 0; j < ql; ++j)
    if (low(s[j]) != q[j]) return 0;
  return 1;
}
// the query in a description: the hot loop (about 10 KB of text for a new query). Its first char
// is found with memchr (the OS's: much faster per char than a compiled loop fetched from flash),
// lowercase then uppercase
static int in_text(const char * s, const char * q, int ql) {
  const char * e = s + strlen(s);
  for (int k = 0; k < 2; ++k) {
    char a = q[0];
    if (k) { if (a < 'a' || a > 'z') break; a -= 32; }
    for (const char * p = s; (p = (const char *)memchr(p, a, e - p)); ++p)
      if (at(p, q, ql)) return 1;
  }
  return 0;
}
// the entry's match class: 0 its name starts with the query, 1 its name contains it, 2 its
// description does, 3 none or not in the tab; with no query 0 names starting with a letter, 1
// the others (symbols, units). Entries starting with a space (programming constructs) are out.
static int classify(const catalogFunc * c) {
  cat_state & s = *S;
  const char * nm = c->name;
  if (nm[0] == ' ') return 3;
#ifndef WITH_TABVAR
  if (!strncmp(nm, "tabvar", 6)) return 3; // not in this build (makefile FEATURE_DEFS)
#endif
  if (s.tab) { // any of its 3 category bytes in the tab's categories
    const unsigned char * t = TCAT[s.tab - 1], * b = (const unsigned char *)&c->category;
    int k = 0;
    for (; k < 3; ++k)
      if (b[k] && (b[k] == t[0] || b[k] == t[1] || b[k] == t[2])) break;
    if (k == 3) return 3;
  }
  if (!s.ql) return letter(nm[0]) ? 0 : 1;
  int n = name_len(nm);
  if (n >= s.ql && at(nm, s.q, s.ql)) return 0;
  for (int i = 1; i + s.ql <= n; ++i)
    if (low(nm[i]) == s.q[0] && at(nm + i, s.q, s.ql)) return 1;
  return c->desc && in_text(c->desc, s.q, s.ql) ? 2 : 3;
}
// the matches in the prototype's catList order (class, then catalog order). narrow: the query
// only grew, so only the current matches can still match
static void filter(int narrow) {
  cat_state & s = *S;
  focus_phase = 60;
  if (narrow)
    for (int j = 0; j < s.ni; ++j) s.cls[s.idx[j]] = (unsigned char)classify(s.C + s.idx[j]);
  else {
    const catalogFunc * c = s.C;
    for (int i = 0; i < s.nc; ++i, ++c) s.cls[i] = (unsigned char)classify(c);
  }
  s.ni = 0;
  for (int k = 0; k < 3; ++k)
    for (int i = 0; i < s.nc; ++i)
      if (s.cls[i] == k) s.idx[s.ni++] = (short)i;
  s.sel = s.top = 0;
  s.stop = -100; // the rows on screen show another list: no scrolling of their pixels
}

// an entry's example, as menufr.cc's help builds it: "#..." is the whole expression, else the
// command's arguments (insert text + example + ")"); an example already starting with the
// command, or for an entry that does not insert a call, is taken whole too. In out (size m), its
// length; 0 if none.
static int example_text(const catalogFunc & c, char * out, int m) {
  const char * e = c.example, * t;
  if (!e) return 0;
  int n = insert_text(c, &t), k = 0;
  m -= 2;
  int whole = *e == '#' || !n || t[n - 1] != '(' || !strncmp(e, t, n - 1);
  if (*e == '#') ++e;
  if (!whole) { k = n < m ? n : m; memcpy(out, t, k); }
  while (*e && k < m) out[k++] = *e++;
  if (!whole) out[k++] = ')';
  out[k] = 0;
  return k;
}

// the help line of entry e (focus_call_entry): the first sentence of its description, which
// names the arguments as the editor's boxes do ("Returns a^n mod p."), and its example
void focus_entry_help(int e, char * desc, int dsize, char * ex, int esize) {
  int nc, k = 0;
  const catalogFunc & c = catalog_entries(nc)[e];
  for (const char * p = c.desc; p && *p && k < dsize - 1; ++p) {
    desc[k++] = *p;
    if (*p == '.' && (!p[1] || p[1] == ' ')) break;
  }
  desc[k] = 0;
  if (!example_text(c, ex, esize)) ex[0] = 0;
}

// the selected entry's example, laid out
static void example() {
  cat_state & s = *S;
  s.exlv = -1;
  if (!s.ni) return;
  int k = example_text(s.C[s.idx[s.sel]], s.ex, sizeof(s.ex));
  if (!k) return;
  focus_phase = 61;
  s.exlv = ui_math_fit(s.ex, k, -1, 5, 296, EH, 0, s.L);
}

// ------------------------------------------------------------------ painting
// draws s[0, n) (n < 0: all) from x; wider than maxw, it is cut with an ellipsis (spaces before
// it dropped). Each glyph is looked up and drawn once.
static int text_cut(const ui_face * f, const char * s, int n, int x, int base, int maxw, const unsigned char * r) {
  const char * e = n < 0 ? 0 : s + n, * p = s, * q;
  int w = 0, wt = 0, ew = ui_text_width(f, ELL, -1);
  unsigned cp;
  for (;; p = q) { // the glyphs that fit even with the ellipsis after them
    q = p;
    if (!(cp = ui_utf8(&q, e))) return w;
    const ui_glyph * g = ui_glyph_of(f, cp);
    int a = g ? g->adv : 0;
    if (w + a > maxw - ew) break;
    if (g) ui_draw_glyph(f, cp, x + w, base, r, 0);
    w += a;
    if (cp != ' ') wt = w;
  }
  int rw = w; // the rest: drawn if it ends within maxw, else the ellipsis
  for (q = p; rw <= maxw && (cp = ui_utf8(&q, e));) {
    const ui_glyph * g = ui_glyph_of(f, cp);
    rw += g ? g->adv : 0;
  }
  if (rw <= maxw) return w + ui_draw_text(f, p, q - p, x + w, base, r, 0);
  return wt + ui_draw_text(f, ELL, -1, x + wt, base, r, 0);
}

static void field_text() { // the query and caret (or the placeholder), the count
  cat_state & s = *S;
  ui_fill(30, CY + 1, 275, CH - 2, col(UC_CARD));
  int x = 32, base = CY + 15;
  if (s.ql) x += ui_draw_text(&ui_tb12, s.q, -1, x, base, ramp(UC_INK, UC_CARD), 0) + 1;
  else ui_draw_text(&ui_tr12, "Search commands", -1, x + 1, base, ramp(UC_SUB, UC_CARD), 0);
  ui_fill(x, CY + 5, 2, CH - 9, col(UC_ACC));
  char m[20], d[8];
  int k = 0, j = 0, n = s.ni;
  do d[k++] = (char)('0' + n % 10); while (n /= 10);
  while (k) m[j++] = d[--k];
  strcpy(m + j, s.ni == 1 ? " match" : " matches");
  ui_draw_text(&ui_tr10, m, -1, 304 - ui_text_width(&ui_tr10, m, -1), base, ramp(UC_SUB, UC_CARD), 0);
}

static void row_draw(int r) { // row r of the view: the name in bold, the description cut
  cat_state & s = *S;
  int i = s.top + r;
  if (i >= s.ni) return;
  const catalogFunc & c = s.C[s.idx[i]];
  int y = LT + r * RH, on = i == s.sel, bg = on ? UC_ACC : UC_BG;
  if (on) ui_rrect(0, 6, y, 306, RH, 5, UC_ACC, UC_BG);
  int w = text_cut(&ui_tb12, c.name, name_len(c.name), 13, y + 14, 200, ramp(on ? UC_ONACC : UC_INK, bg));
  int dx = 13 + w + 10 > DX ? 13 + w + 10 : DX;
  if (c.desc) text_cut(&ui_tr10, c.desc, -1, dx, y + 14, 306 - dx, ramp(on ? UC_ONACC : UC_SUB, bg));
}

static void footer_draw() { // the selected entry: signature, "enter insert", example
  cat_state & s = *S;
  ui_fill(8, FL, 304, 1, col(UC_LINE));
  if (!s.ni) return;
  const catalogFunc & c = s.C[s.idx[s.sel]];
  int hw = ui_text_width(&ui_tb9, "enter  insert", -1);
  ui_draw_text(&ui_tb9, "enter  insert", -1, 308 - hw, SY, ramp(UC_SUB, UC_BG), 0);
  text_cut(&ui_tb10, c.name, -1, 12, SY, 296 - hw - 10, ramp(UC_INK, UC_BG));
  if (s.exlv < 0) return;
  int y0 = ui_cy0, y1 = ui_cy1; // the band's rows; a tall example stays under the signature
  ui_clip(0, y0 > EY ? y0 : EY, 312, y1);
  ui_math_draw(s.L, s.ex, s.exlv, 12, EY + (EH - (s.L.asc + s.L.desc)) / 2 + s.L.asc, 0, UC_INK, UC_BG, UC_ACC);
  ui_clip(0, y0, UI_W, y1);
}

static void paint_band(int b, int e) { // everything in rows [b, e)
  cat_state & s = *S;
  focus_phase = b < LT ? 62 : b < LB ? 63 : 64;
  ui_clip(0, b, UI_W, e);
  ui_fill(0, b, UI_W, e - b, col(UC_BG));
  if (b < CY + CH) { // the field: a card, the magnifier
    ui_rrect(0, 8, CY, 304, CH, 8, UC_CARD, UC_BG);
    ui_rframe(0, 8, CY, 304, CH, 8, UC_LINE, UC_BG);
    focus_icon(IC_SEARCH, 21, CY + 11, 0, UC_SUB, UC_CARD);
    field_text();
  }
  for (int r = 0; r < ROWS; ++r)
    if (LT + r * RH < e && LT + (r + 1) * RH > b) row_draw(r);
  if (b < LB && e > LT) {
    if (s.sb > 0) { // a thin scrollbar
      ui_fill(315, LT, 2, ROWS * RH, col(UC_LINE));
      ui_fill(315, s.sb >> 8, 2, s.sb & 0xff, col(UC_ACC));
    }
    if (s.sb < 0) {
      char m[QMAX + 24];
      strcpy(m, "No command matches \"");
      strcat(m, s.q);
      strcat(m, "\"");
      ui_draw_text(&ui_tr12, m, -1, (UI_W - ui_text_width(&ui_tr12, m, -1)) / 2, LT + 50, ramp(UC_SUB, UC_BG), 0);
    }
  }
  if (e > FL) footer_draw();
  ui_noclip();
}

static void paint(int y0, int y1) { // rows [y0, y1) through the strip
  int rows = ui_band_open(48);
  for (int b = y0; b < y1;) {
    int e = rows && b + rows < y1 ? b + rows : y1;
    if (rows) { ui_band_begin(b, e); paint_band(b, e); ui_band_end(); }
    else paint_band(b, e); // no memory: directly
    b = e;
  }
}

// the scrollbar as the rows draw it (thumb top << 8 | length), 0 none, -1 the no-match message
static int sb_of(const cat_state & s) {
  if (!s.ni) return -1;
  if (s.ni <= ROWS) return 0;
  int tr = ROWS * RH, tl = tr * ROWS / s.ni;
  if (tl < 12) tl = 12;
  return (LT + (tr - tl) * s.top / (s.ni - ROWS)) << 8 | tl;
}

// the list moved by one row (d = 1: down) as the selection crossed an edge: the 5 rows that stay
// move as pixels, and the 2 rows at that end (the new entry, the one losing the selection) are
// composed in the strip first, copied right after the move; the scrollbar is redrawn in place
static void scroll(int d) {
  cat_state & s = *S;
  unsigned char * scr = ui_fb; // the screen (no band open)
  int r0 = d > 0 ? ROWS - 2 : 0, y0 = LT + r0 * RH, band = ui_band_open(48) >= 2 * RH;
  if (d > 0) { memmove(s.shown, s.shown + 1, (ROWS - 1) * sizeof(short)); s.shown[ROWS - 1] = -2; }
  else { memmove(s.shown + 1, s.shown, (ROWS - 1) * sizeof(short)); s.shown[0] = -2; }
  s.sb = sb_of(s);
  s.stop = s.top;
  if (band) { ui_band_begin(y0, y0 + 2 * RH); paint_band(y0, y0 + 2 * RH); }
  memmove(scr + (LT + RH - d * RH) * UI_W, scr + (LT + RH) * UI_W, (ROWS - 2) * RH * UI_W);
  if (band) {
    ui_band_end();
    for (int r = r0; r < r0 + 2; ++r) {
      int i = s.top + r;
      s.shown[r] = (short)(s.idx[i] | (i == s.sel ? SEL : 0));
    }
  }
  else s.shown[r0] = s.shown[r0 + 1] = -2; // painted by rows_update
  int ty = s.sb >> 8, tl = s.sb & 0xff;
  ui_noclip();
  ui_fill(315, LT, 2, ty - LT, col(UC_LINE));
  ui_fill(315, ty, 2, tl, col(UC_ACC));
  ui_fill(315, ty + tl, 2, LB - ty - tl, col(UC_LINE));
}

// the rows that changed, the selected one first; 0 if a key stopped it
static int rows_update() {
  cat_state & s = *S;
  int d = s.top - s.stop;
  if ((d == 1 || d == -1) && s.sb > 0 && !key_waiting()) scroll(d);
  s.stop = s.top;
  int sb = sb_of(s);
  if (sb != s.sb) { // every row draws a slice of the scrollbar (or of the message)
    s.sb = sb;
    for (int r = 0; r < ROWS; ++r) s.shown[r] = -2;
  }
  for (int pass = 0; pass < 2; ++pass)
    for (int r = 0; r < ROWS; ++r) {
      int i = s.top + r, v = i < s.ni ? s.idx[i] | (i == s.sel ? SEL : 0) : -1;
      if (v == s.shown[r] || (pass == 0) != (i == s.sel)) continue;
      if (key_waiting()) return 0;
      paint(LT + r * RH, LT + (r + 1) * RH);
      s.shown[r] = (short)v;
    }
  return 1;
}

// brings the screen up to date: the field, the rows, the footer; stops when a key is waiting
// (the next GetKey takes it, and what is left is painted after it)
static void update() {
  cat_state & s = *S;
  focus_phase = 66;
  if (s.sn != s.ni || strcmp(s.sq, s.q)) {
    if (!s.ni) s.sb = -2; // the no-match message quotes the query: its rows are redrawn
    if (s.sn < 0) paint(ST, LT); // the whole card
    else { // its text only, over the screen's pixels
      int band = ui_band_open(48) >= CH;
      focus_phase = 62;
      if (band) { ui_band_begin(CY + 1, CY + CH - 1); ui_band_load(); }
      ui_clip(30, CY + 1, 305, CY + CH - 1);
      field_text();
      ui_noclip();
      if (band) ui_band_end();
    }
    s.sn = s.ni;
    strcpy(s.sq, s.q);
  }
  int f = s.ni ? s.idx[s.sel] : -1;
  if (rows_update() && f != s.sf && !key_waiting()) {
    example();
    paint(LB, SBOT);
    s.sf = f;
  }
  ui_band_close();
}

// ------------------------------------------------------------------ the search
int focus_catalog(const char * query, char * out, int outsize) {
  S = new cat_state;
  cat_state & s = *S;
  s.C = catalog_entries(s.nc);
  s.idx = new short[s.nc];
  s.cls = new unsigned char[s.nc];
  s.tab = s.ql = 0;
  for (; query && *query && s.ql < QMAX; ++query) s.q[s.ql++] = low(*query);
  s.q[s.ql] = 0;
  s.sn = -1; s.sb = -2; s.sf = -2; // nothing on screen yet
  focus_status_label("COMMANDS");
  lock_alpha(); // plain keys type letters
  focus_phase = 65;
  focus_bar_tabs(TABS, 0);
  filter(0);
  int res = 0, plain = 0, typed = 0; // typed: the query was edited here (not just the word before the caret)
  for (;;) {
    update();
    int k;
    focus_phase = 0;
    GetKey(&k);
    if (k == KEY_CTRL_ALPHA) { lock_alpha(); continue; } // still letters (ALPHA before a letter by habit)
    if (k == KEY_CTRL_SHIFT) { reset_kbd(); plain = 1; continue; } // 2nd: the next key unshifted (digits)
    if (plain) { plain = 0; lock_alpha(); }
    int ch = (k >= 'a' && k <= 'z') || (k >= '0' && k <= '9') ? k : k >= 'A' && k <= 'Z' ? k + 32 : k == KEY_CHAR_PMINUS ? '_' : 0;
    // F1-F5: the tabs (with alpha locked, the keys read as F11-F15)
    int t = k >= KEY_CTRL_F1 && k <= KEY_CTRL_F5 ? k - KEY_CTRL_F1 : k >= KEY_CTRL_F11 && k <= KEY_CTRL_F15 ? k - KEY_CTRL_F11 : -1;
    if (k == KEY_CTRL_EXIT || k == KEY_CTRL_AC || (k == KEY_CTRL_DEL && !s.ql)) break;
    if (k == KEY_CTRL_UP && !typed && s.sel == 0) break; // nothing searched, at the top: up closes (down opened it)
    if (k == KEY_CTRL_EXE || k == KEY_CTRL_OK) {
      if (s.ni) {
        const char * txt;
        int n = insert_text(s.C[s.idx[s.sel]], &txt);
        if (n > outsize - 1) n = outsize - 1;
        memcpy(out, txt, n);
        out[n] = 0;
        res = 1;
      }
      break;
    }
    if (ch && s.ql < QMAX) { s.q[s.ql++] = (char)ch; s.q[s.ql] = 0; filter(1); typed = 1; }
    else if (k == KEY_CTRL_DEL) { s.q[--s.ql] = 0; filter(0); typed = 1; }
    else if (t >= 0 && t != s.tab) {
      s.tab = t;
      focus_phase = 65;
      focus_bar_tabs(TABS, t);
      filter(0);
    }
    else { // the selection moves; the list scrolls to keep it in view
      int ns = s.sel;
      if (k == KEY_CTRL_UP && ns > 0) --ns;
      if (k == KEY_CTRL_DOWN && ns < s.ni - 1) ++ns;
      if (k == KEY_CTRL_LEFT) ns = ns > ROWS ? ns - ROWS : 0;
      if (k == KEY_CTRL_RIGHT) ns = ns + ROWS < s.ni ? ns + ROWS : s.ni - 1;
      if (ns < 0) continue;
      s.sel = ns;
      if (ns < s.top) s.top = ns;
      if (ns >= s.top + ROWS) s.top = ns - ROWS + 1;
    }
  }
  delete[] s.idx;
  delete[] s.cls;
  delete S;
  S = 0;
  reset_kbd();
  focus_status_label(0);
  focus_invalidate(); // the console repaints everything
  return res;
}
