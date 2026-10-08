// focus_edit.cc - the program editor in the Focus look. doTextArea (textGUI.cc) hands it the
// editable text areas (prgm: the session's program; files opened from the file menu): the code in
// the UI font on the stage's background, line numbers in a gutter, the theme's colors for keywords,
// commands, numbers, strings and comments, the caret and the selection in the accent color, dotted
// guides for the indentation; the Focus status bar and an F-key bar of its own (blocks, tests, edit,
// symbols, file) whose menus and prompts are Focus cards. Leaving saves the program and loads it
// (its functions are defined), or says on which line it fails. Editing itself (insertion,
// auto-indentation, undo, the clipboard, KhiCAS's 2nd and alpha menus) stays textGUI.cc's.
#include "calc.h"
#include <string>
#include <cstring>
#include <cstdlib>
#include "menuGUI.h"
#include "textGUI.h"
#include "console.h"
#include "file.h"
#include "main.h"
#include "focus.h"
#include "ui_gfx.h"
#include "ui_font.h"
#include "ui_fontdata.h"
#if !defined std
#define std ustl
#endif
using namespace std;

// textGUI.cc
int handle_key(textArea * text,int key,int keyflag);
void insert(textArea * text,const char * adds,bool indent);
bool match(textArea * text,int pos,int & line1,int & pos1,int & line2,int & pos2);
void set_undo(textArea * text);
void add_indented_line(std::vector<textElement> & v,int & textline,int & textpos);
int merged_size(const std::vector<textElement> & v);
// main.cc, focus_menu.cc
int find_color(const char * s);
int check_program(const std::vector<textElement> & v,int python,std::string & msg);
int focus_prompt(const char * title, const char * label, std::string & s, bool numeric);

enum { ST = 16, SBOT = 218, MAXRW = 24, ED_LEAVE = 1000 };
static const ui_face * F;         // the code's face: tr12, tr10 when small (mode key)
static int RH, BASE, IW, SPW, NV, GW, X0, XR;
static unsigned char WT[96];      // advances of ASCII 32-127 in F
static int top = 0;               // the first row shown (rows: the lines wrapped to the screen)
static textArea * shown;          // the text whose rows top counts
static int M1L, M1P, M2L, M2P;    // the brackets matched around the caret (line, byte; -1: none)
static int SL1, SP1, SL2, SP2;    // the selection, in order (SL1 < 0: none)
static const char * const TABS[5] = {"blocks", "tests", "edit", "symbols", "file"};

static unsigned char col(int c) { return ui_col(0, c); }
static const unsigned char * ramp(int c) { return ui_ramp(0, c, UC_BG); }

static void metrics(textArea * t) {
  const bool sm = t->minimini;
  F = sm ? &ui_tr10 : &ui_tr12;
  RH = sm ? 13 : 16; BASE = sm ? 10 : 12; IW = sm ? 5 : 6;
  NV = (SBOT - ST - 2) / RH;
  int n = t->elements.size(), d = 1;
  for (int m = 10; n >= m && d < 4; m *= 10) ++d;
  GW = 7 + 5 * d; X0 = GW + 6; XR = UI_W - 5;
  for (int c = 32; c < 128; ++c) { const ui_glyph * g = ui_glyph_of(F, c); WT[c - 32] = g ? g->adv : 0; }
  SPW = WT[0] < 4 ? 4 : WT[0];
}
// the width of byte c (a space between words is wider than the font's: code reads better)
static int wid(unsigned char c) {
  if (c == ' ') return SPW;
  if (c >= 32 && c < 128) return WT[c - 32];
  return (c & 0xc0) == 0x80 ? 0 : IW; // UTF-8: its first byte, then its continuation bytes
}
static int lead(const std::string & s) { int i = 0, n = s.size(); while (i < n && s[i] == ' ') ++i; return i; }
static int hang_of(int ind) { int h = (ind + 4) * IW, w = (XR - X0) / 2; return h < w ? h : w; }

// the rows of line s: the first byte of each in rs (rs[0] = 0); returns how many. A row breaks
// after its last space when it has one; the next rows hang 4 spaces in from the indentation.
static int wrap(const std::string & s, unsigned short * rs) {
  const int n = s.size(), ind = lead(s), W = XR - X0, hang = hang_of(ind);
  int r = 1, x = ind * IW, sp = -1;
  rs[0] = 0;
  for (int i = ind; i < n; ++i) {
    const unsigned char c = s[i];
    const int w = wid(c);
    if (x + w > W && i > rs[r - 1] && r < MAXRW) {
      const int b = sp >= rs[r - 1] && sp >= ind ? sp + 1 : i;
      rs[r++] = b;
      x = hang;
      for (int j = b; j < i; ++j) x += wid(s[j]);
      sp = -1;
    }
    if (c == ' ') sp = i;
    x += w;
  }
  return r;
}
static int rowof(const unsigned short * rs, int nr, int pos) { int k = nr - 1; while (k && rs[k] > pos) --k; return k; }
// x of byte pos in row k (leading spaces are indentation: IW each)
static int xof(const std::string & s, const unsigned short * rs, int k, int pos) {
  const int ind = lead(s);
  int x = X0 + (k ? hang_of(ind) : 0);
  for (int i = rs[k]; i < pos; ++i) x += i < ind ? IW : wid(s[i]);
  return x;
}

static bool namec(unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'; }
static const char * const XCAS_KW[] = {"then", "fi", "do", "od", "from", "to", "step", "local", "function", "ffunction", "end", "elif", 0};
static int word_color(const char * w, bool py) {
  if (!py)
    for (int k = 0; XCAS_KW[k]; ++k)
      if (!strcmp(w, XCAS_KW[k])) return UC_ACC;
  const int c = find_color(w);
  return c == _BLUE ? UC_ACC : c == 24844 ? UC_PURPLE : c == _GREEN ? UC_ORANGE : UC_INK;
}
// the color of each byte of s: keywords in the accent color, commands purple, numbers and
// constants orange, strings green, comments gray, the rest ink
static void colorize(const std::string & s, unsigned char * c, bool py) {
  const int n = s.size();
  const char * p = s.c_str();
  for (int i = 0; i < n;) {
    const unsigned char ch = p[i];
    int j = i + 1, k = UC_INK;
    if ((py && ch == '#') || (!py && ch == '/' && p[i + 1] == '/')) { j = n; k = UC_SUB; }
    else if (ch == '"' || (py && ch == '\'')) {
      while (j < n && (unsigned char)p[j] != ch) j += p[j] == '\\' ? 2 : 1;
      j = j < n ? j + 1 : n;
      k = UC_GREEN;
    }
    else if (ch >= '0' && ch <= '9') { while (j < n && (namec(p[j]) || p[j] == '.')) ++j; k = UC_ORANGE; }
    else if (namec(ch)) {
      while (j < n && namec(p[j])) ++j;
      char w[24];
      if (j - i < 24) { memcpy(w, p + i, j - i); w[j - i] = 0; k = word_color(w, py); }
    }
    memset(c + i, k, j - i);
    i = j;
  }
}

static bool marked(int li, int i) { return (li == M1L && i == M1P) || (li == M2L && i == M2P); }
// row k of line li at y: gutter (the line's number on its first row), background (the caret's
// line a shade lighter), indentation guides, selection, text, bracket marks, caret
static void paint_row(textArea * t, int li, const unsigned short * rs, int nr, int k, int y, const unsigned char * c) {
  const std::string & s = t->elements[li].s;
  const char * p = s.c_str();
  const int n = s.size(), ind = lead(s), cur = li == t->line, e = k + 1 < nr ? rs[k + 1] : n;
  ui_fill(0, y, GW, RH, col(UC_BG));
  ui_fill(GW, y, UI_W - GW, RH, col(cur ? UC_CARD : UC_BG));
  if (!k) {
    char b[8];
    int v = li + 1, m = 7;
    b[m] = 0;
    do { b[--m] = '0' + v % 10; v /= 10; } while (v && m);
    const ui_face * nf = cur ? &ui_tb9 : &ui_tr9;
    ui_draw_text(nf, b + m, -1, GW - 2 - ui_text_width(nf, b + m, -1), y + BASE, ramp(cur ? UC_ACC : UC_SUB), 0);
  }
  for (int g = 0; 2 * g < ind; ++g) // one dotted guide per enclosing block
    for (int yy = y + (y & 1); yy < y + RH; yy += 2) ui_fill(X0 + 2 * g * IW + 1, yy, 1, 1, col(UC_LINE));
  if (SL1 >= 0 && li >= SL1 && li <= SL2) {
    const int lim = k == nr - 1 ? n + 1 : e; // n + 1: the line break is selected too
    int a = li == SL1 ? SP1 : 0, b = li == SL2 ? SP2 : n + 1;
    if (a < rs[k]) a = rs[k];
    if (b > lim) b = lim;
    if (a < b) {
      const int xa = xof(s, rs, k, a), xb = b > n ? xof(s, rs, k, n) + 4 : xof(s, rs, k, b);
      ui_fill(xa, y + 1, xb - xa, RH - 2, col(UC_ACCSOFT));
    }
  }
  int x = X0 + (k ? hang_of(ind) : 0);
  for (int i = rs[k]; i < e;) {
    if (i < ind) { x += IW; ++i; continue; }
    if (p[i] == ' ') { x += SPW; ++i; continue; }
    int kc = c[i], j = i + 1;
    const bool hl = marked(li, i);
    if (hl) kc = M1L >= 0 && M2L >= 0 ? UC_GREEN : UC_RED; // a bracket and its match, or alone
    else while (j < e && p[j] != ' ' && c[j] == kc && !marked(li, j)) ++j;
    const int w = ui_draw_text(F, p + i, j - i, x, y + BASE, ramp(kc), 0);
    if (hl) ui_fill(x, y + BASE + 2, w, 1, col(kc));
    x += w;
    i = j;
  }
  if (cur && rowof(rs, nr, t->pos) == k)
    ui_fill(xof(s, rs, k, t->pos) - 1, y + 1, 2, RH - 2, col(UC_ACC));
}

// paints the rows of lines [l0, l1] that show, one row at a time through the strip (no blank
// frame); all: also the empty rows under the text
static void paint(textArea * t, int l0, int l1, bool all) {
  std::vector<textElement> & v = t->elements;
  unsigned short rs[MAXRW];
  const bool rows = ui_band_open(RH) >= RH; // (memory short: straight to the screen)
  const int nv = v.size();
  int g = 0, i = 0;
  for (; i < nv && g < top + NV; g += v[i].nlines, ++i) {
    if (g + v[i].nlines <= top || i < l0 || i > l1) continue;
    const int nr = wrap(v[i].s, rs);
    unsigned char * c = new unsigned char[v[i].s.size() + 1];
    if (!c) break;
    colorize(v[i].s, c, t->python);
    for (int k = 0; k < nr; ++k) {
      const int sr = g + k - top;
      if (sr < 0 || sr >= NV) continue;
      const int y = ST + 1 + sr * RH;
      if (rows) ui_band_begin(y, y + RH);
      paint_row(t, i, rs, nr, k, y, c);
      if (rows) ui_band_end();
    }
    delete[] c;
  }
  ui_band_close();
  if (all) {
    const int y = ST + 1 + (g - top < NV ? g - top : NV) * RH;
    ui_fill(0, ST, UI_W, 1, col(UC_BG));
    if (y < SBOT) ui_fill(0, y, UI_W, SBOT - y, col(UC_BG));
  }
}

// what the screen shows, to repaint only the lines whose look changed
static int p_top = -1, p_line, p_m1, p_m2, p_sel;
static unsigned p_sig;
static void refresh(textArea * t, bool full) {
  std::vector<textElement> & v = t->elements;
  metrics(t); // the gutter grows with the number of lines
  if (v.empty()) v.push_back(textElement());
  if (t->line < 0) t->line = 0;
  if (t->line >= (int)v.size()) t->line = v.size() - 1;
  const int len = v[t->line].s.size();
  if (t->pos < 0) t->pos = 0;
  if (t->pos > len) t->pos = len;
  unsigned short rs[MAXRW];
  int total = 0, crow = 0;
  unsigned sig = v.size();
  for (int i = 0; i < (int)v.size(); ++i) {
    const int nr = wrap(v[i].s, rs);
    if (i == t->line) crow = total + rowof(rs, nr, t->pos);
    v[i].nlines = nr;
    total += nr;
    sig = sig * 31 + nr;
  }
  if (crow < top) top = crow;
  if (crow >= top + NV) top = crow - NV + 1;
  if (top > total - NV) top = total > NV ? total - NV : 0;
  int l1, p1, l2, p2;
  if (!match(t, t->pos, l1, p1, l2, p2) && l1 == -1 && l2 == -1)
    match(t, t->pos - 1, l1, p1, l2, p2);
  M1L = l1; M1P = p1; M2L = l2; M2P = p2;
  SL1 = -1;
  if (t->clipline >= 0) {
    const bool before = t->clipline < t->line || (t->clipline == t->line && t->clippos < t->pos);
    SL1 = before ? t->clipline : t->line; SP1 = before ? t->clippos : t->pos;
    SL2 = before ? t->line : t->clipline; SP2 = before ? t->pos : t->clippos;
  }
  if (full || top != p_top || sig != p_sig || SL1 >= 0 || p_sel)
    paint(t, 0, v.size(), true);
  else {
    const int L[6] = {p_line, t->line, p_m1, p_m2, M1L, M2L};
    for (int a = 0; a < 6; ++a) {
      int b = 0;
      while (b < a && L[b] != L[a]) ++b;
      if (b == a && L[a] >= 0 && L[a] < (int)v.size()) paint(t, L[a], L[a], false);
    }
  }
  p_top = top; p_line = t->line; p_m1 = M1L; p_m2 = M2L; p_sel = SL1 >= 0; p_sig = sig;
}

// the caret on line l (1-based), at its end
static void goto_line(textArea * t, int l) {
  const int n = t->elements.size();
  t->line = l < 1 ? 0 : l > n ? n - 1 : l - 1;
  t->pos = t->elements[t->line].s.size();
  t->clipline = -1;
}

// a statement's template (def, if, for, return...) takes a line of its own
static bool statement(const char * s) {
  static const char * const kw[] = {"def ", "if ", "else", "elif ", "for ", "while ", "return", "print(", "break",
                                     "function ", "local ", 0};
  for (int k = 0; kw[k]; ++k)
    if (!strncmp(s, kw[k], strlen(kw[k]))) return true;
  return false;
}
// inserts template s: \1 marks where the caret goes (else after it). A statement goes on a line of
// its own: a new line under the caret's when text is before the caret, above it when text is only
// after; else and elif on an empty line step out of the block first. One undo takes it all back.
static void put(textArea * t, const char * s) {
  std::vector<textElement> & v = t->elements;
  std::vector<textElement> u(v);
  const int ul = t->line, up = t->pos;
  if (statement(s)) {
    const std::string & L0 = v[t->line].s;
    const int ind = lead(L0);
    if (t->pos > ind) { t->pos = L0.size(); add_indented_line(v, t->line, t->pos); }
    else if ((int)L0.size() > ind) {
      textElement e;
      e.s = std::string(ind, ' ');
      v.insert(v.begin() + t->line, e);
      t->pos = ind;
    }
  }
  std::string & L = v[t->line].s;
  if ((!strncmp(s, "else", 4) || !strncmp(s, "elif", 4)) && lead(L) == (int)L.size() && L.size() >= 2) {
    L.erase(L.begin(), L.begin() + 2);
    t->pos = L.size();
  }
  const char * m = strchr(s, 1);
  std::string a(s, m ? m - s : strlen(s));
  insert(t, a.c_str(), true);
  if (m) {
    const int l = t->line, p = t->pos;
    insert(t, m + 1, true);
    t->line = l; t->pos = p;
  }
  swap(t->undoelements, u); t->undoline = ul; t->undopos = up; t->undoclipline = -1;
  t->changed = true;
}

// the menus of F1 (blocks: a word, what it is for), F2 (tests) and F4 (symbols: the symbol, drawn
// large, and what it means); the template inserted
struct ed_item { const char * label, * hint, * text; };
static const ed_item PY_BLOCKS[] = {
  {"def f(x)", "function", "def f(x):\n\1\nreturn "},
  {"if", "condition", "if \1:"},
  {"else", "otherwise", "else:\n"},
  {"elif", "else if", "elif \1:"},
  {"for i in range", "repeat n times", "for i in range(\1):"},
  {"for x in", "each item of a list", "for x in \1:"},
  {"while", "loop while true", "while \1:"},
  {"return", "the result", "return "},
  {"print", "show a value", "print(\1)"},
  {"input", "ask for a value", "input(\1)"},
  {"break", "leave the loop", "break"},
};
static const ed_item XC_BLOCKS[] = {
  {"function f(x)", "function", "function f(x)\nlocal j;\n\1\nreturn ;\nffunction:;"},
  {"if then", "condition", "if \1 then\n\nfi;"},
  {"else", "otherwise", "else\n"},
  {"for from to", "repeat n times", "for \1 from  to  do\n\nod;"},
  {"for in", "each item of a list", "for \1 in  do\n\nod;"},
  {"while do", "loop while true", "while \1 do\n\nod;"},
  {"return", "the result", "return \1;"},
  {"print", "show a value", "print(\1);"},
  {"input", "ask for a value", "input(\1);"},
  {"local", "local variables", "local \1;"},
};
static const ed_item PY_TESTS[] = {
  {"==", "equal", "=="}, {"!=", "not equal", "!="}, {"<", "less than", "<"}, {">", "greater than", ">"},
  {"<=", "at most", "<="}, {">=", "at least", ">="}, {"and", "both", " and "}, {"or", "either", " or "},
  {"not", "the opposite", "not "}, {"in", "is in the list", " in "}, {"True", "true", "True"}, {"False", "false", "False"},
};
static const ed_item XC_TESTS[] = {
  {"==", "equal", "=="}, {"!=", "not equal", "!="}, {"<", "less than", "<"}, {">", "greater than", ">"},
  {"<=", "at most", "<="}, {">=", "at least", ">="}, {"and", "both", " and "}, {"or", "either", " or "},
  {"not", "the opposite", "not "}, {"true", "true", "true"}, {"false", "false", "false"},
};
static const ed_item PY_SYMBOLS[] = { // ((-) types a minus: _ is here)
  {"#", "comment", "# "}, {":", "colon", ":"}, {"\"\"", "text", "\"\1\""}, {"[ ]", "list", "[\1]"},
  {"_", "underscore", "_"}, {"%", "remainder", "%"}, {"//", "integer division", "//"}, {"**", "power", "**"},
  {"=", "store a value", "="}, {";", "two statements", ";"}, {"{ }", "braces", "{\1}"}, {"'", "quote", "'"},
  {"!", "factorial", "!"}, {"len", "length", "len(\1)"}, {"&", "and (bits)", "&"}, {"|", "or (bits)", "|"},
  {"\\", "backslash", "\\"}, {"~", "tilde", "~"},
};
static const ed_item XC_SYMBOLS[] = {
  {"//", "comment", "// "}, {":=", "store a value", ":="}, {"\"\"", "text", "\"\1\""}, {"[ ]", "list", "[\1]"},
  {"_", "underscore", "_"}, {"%", "remainder", "%"}, {"^", "power", "^"}, {";", "end of a statement", ";"},
  {":", "colon", ":"}, {"{ }", "braces", "{\1}"}, {"'", "quote", "'"}, {"!", "factorial", "!"},
  {"size", "length", "size(\1)"}, {"&", "and", "&"}, {"|", "or", "|"}, {"\\", "backslash", "\\"},
};
#define N(a) (int)(sizeof(a) / sizeof(a[0]))

// a menu of templates as a list card (sym: a column of symbols), tab i of the bar shown open;
// returns the template, 0
static const char * pick(int i, const ed_item * it, int n, bool sym) {
  const char * lab[20], * hint[20];
  for (int k = 0; k < n; ++k) { lab[k] = it[k].label; hint[k] = it[k].hint; }
  focus_tab_open(i, TABS[i]);
  const int r = sym ? focus_list(0, hint, n, 0, 0, lab) : focus_list(0, lab, n, 0, hint);
  return r < 0 ? 0 : it[r].text;
}

static const char ED_HELP[] =
  "Keys:\nenter  a new line, indented for its block\n2nd enter  check and load the program\n"
  "clear  leave: the program is saved and loaded\nalpha  letters; 2nd alpha: letters on\n"
  "math  math templates\n2nd 0  all commands\nvars  variables\nmode  text size\n"
  "Menus:\nblocks  def, if, for, while, return, print\ntests  == != < > and or not\n"
  "edit  select, copy, paste, undo, find, go to line\nsymbols  # : \" [ ] % // **\n"
  "file  check and load, save, insert a file, clear\n2nd F1-F5  the main screen's menus\n"
  "Loading:\nLeaving the editor saves the program and defines its functions: use them on the "
  "main screen, f(2) for instance. On an error, the editor goes to its line.";

// the names a program defines (def f, f(x):=, function f), for the status bar
static void names(textArea * t, char * out, int size) {
  int k = 0;
  out[0] = 0;
  for (size_t i = 0; i < t->elements.size(); ++i) {
    const std::string & s = t->elements[i].s;
    const char * p = s.c_str() + lead(s);
    if (!strncmp(p, "def ", 4)) p += 4;
    else if (!strncmp(p, "function ", 9)) p += 9;
    else if (!strstr(p, ":=") || lead(s)) continue;
    int n = 0;
    while (namec(p[n])) ++n;
    if (!n || p[n] != '(' || k + n + 5 >= size) continue;
    if (k) { out[k++] = ','; out[k++] = ' '; }
    memcpy(out + k, p, n); k += n;
    out[k++] = '('; out[k++] = ')'; out[k] = 0;
  }
}

static std::string num(int v) { char b[8]; int m = 7; b[m] = 0; do { b[--m] = '0' + v % 10; v /= 10; } while (v && m); return b + m; }
static std::string line_msg(int l, const char * s) { return "Line " + num(l) + s; }
// Python's common slips, found before giac (whose error line is often the def's): brackets that
// do not match, a header (def, if, for...) without its colon or with an empty block. Returns the
// line (from 1) with a message, 0 if none is found.
static int slips(textArea * t, std::string & msg) {
  const std::vector<textElement> & v = t->elements;
  const int n = v.size();
  char st[32];
  int sl[32], depth = 0;
  for (int i = 0; i < n; ++i) {
    const char * p = v[i].s.c_str();
    const int L = v[i].s.size();
    int colon = -1, last = -1; // the line's last colon outside brackets, its last character
    for (int j = 0; j < L; ++j) {
      const char c = p[j];
      if (c == '#') break;
      if (c != ' ') last = j;
      if (c == '"' || c == '\'') { // a string
        while (++j < L && p[j] != c) if (p[j] == '\\') ++j;
        last = j;
        continue;
      }
      if (c == '(' || c == '[' || c == '{') { if (depth < 32) { st[depth] = c; sl[depth] = i; } ++depth; }
      else if (c == ')' || c == ']' || c == '}') {
        if (!depth || (depth <= 32 && st[depth - 1] != (c == ')' ? '(' : c == ']' ? '[' : '{'))) {
          const char m[] = {':', ' ', 't', 'h', 'i', 's', ' ', c, 0};
          msg = line_msg(i + 1, m) + " closes nothing";
          return i + 1;
        }
        --depth;
      }
      else if (c == ':' && !depth) colon = j;
    }
    if (depth) continue; // the statement goes on, on the next line
    const int ind = lead(v[i].s);
    int w = 0;
    while (namec(p[ind + w])) ++w;
    static const char * const heads[] = {"def", "if", "elif", "else", "for", "while", 0};
    int h = 0;
    while (heads[h] && !(w == (int)strlen(heads[h]) && !strncmp(p + ind, heads[h], w))) ++h;
    if (!heads[h]) continue;
    if (colon < 0) { msg = line_msg(i + 1, ": a : is missing at the end"); return i + 1; }
    if (colon < last) continue; // its block on the same line
    int k = i + 1;
    while (k < n && lead(v[k].s) == (int)v[k].s.size()) ++k;
    if (k == n || lead(v[k].s) <= ind) {
      if (k == n) { msg = line_msg(i + 1, ": its block is empty"); return i + 1; }
      msg = line_msg(k + 1, ": indent it (block of line ") + num(i + 1) + ")";
      return k + 1;
    }
  }
  if (depth) {
    const char m[] = {':', ' ', 'a', ' ', st[0], 0};
    msg = line_msg(sl[0] + 1, m) + " is not closed";
    return sl[0] + 1;
  }
  return 0;
}
static int verify(textArea * t, std::string & msg) {
  const int l = t->python ? slips(t, msg) : 0;
  return l ? l : check_program(t->elements, t->python, msg);
}

// checks and loads the program (2nd enter, the file menu): its functions are defined; on an
// error, the caret goes to its line and a card says what is wrong
static int check(textArea * t) {
  std::string msg;
  const int l = verify(t, msg);
  if (l) {
    goto_line(t, l);
    focus_note(msg.c_str(), "Any key: back to the line");
    int k;
    ck_getkey(&k);
    return l;
  }
  char b[64];
  names(t, b, 40);
  std::string m(b[0] ? "Loaded: " : "Loaded");
  m += b;
  focus_status_msg(m.c_str());
  return 0;
}

// leaving: the program saved and loaded; on an error, the choice to fix it first. 1: leave
static int leave(textArea * t) {
  if (t->allowEXE || !t->changed || t->filename.empty()) return 1;
  save_script(t->filename.c_str(), merge_area(t->elements));
  t->changed = false; // saved (no question when the session is saved)
  std::string msg;
  const int l = verify(t, msg);
  if (!l) return 1;
  goto_line(t, l);
  static const char * const ch[] = {"Fix it", "Leave anyway"};
  if (focus_choose(msg.c_str(), ch, 2) == 1) return 1;
  t->changed = true; // still to load when leaving
  return 0;
}

// the next match of s at or after the caret, wrapping around: selected
static bool find_next(textArea * t, const std::string & s) {
  std::vector<textElement> & v = t->elements;
  const int n = v.size();
  for (int k = 0; k <= n; ++k) {
    const int li = (t->line + k) % n;
    const std::string & L = v[li].s;
    const int from = k ? 0 : t->pos;
    for (int p = from; p + (int)s.size() <= (int)L.size(); ++p)
      if (!strncmp(L.c_str() + p, s.c_str(), s.size())) {
        t->line = li; t->clipline = li; t->clippos = p; t->pos = p + s.size();
        return true;
      }
  }
  return false;
}

// F3: returns a key for handle_key (select, copy, cut, paste, undo, indent), 0 when done here
static int edit_menu(textArea * t, std::string & search, std::string & replace) {
  const bool sel = t->clipline >= 0;
  const char * lab[] = {sel ? "Copy" : "Select", sel ? "Cut" : "Cut the line", "Paste", "Undo",
                        "Find\xe2\x80\xa6", "Go to line\xe2\x80\xa6", "Indent"};
  const char * hint[] = {sel ? "the selection" : "then move", "to the clipboard", 0, "or redo",
                         "and replace", 0, "for its block"};
  focus_tab_open(2, TABS[2]);
  const int r = focus_list(0, lab, 7, 0, hint);
  if (r == 4 || r == 5) refresh(t, true); // a prompt next: over the text, not over the menu
  static const int keys[] = {KEY_CTRL_CLIP, 0, KEY_CTRL_PASTE, KEY_CTRL_UNDO, 0, 0, KEY_CHAR_FRAC};
  if (r == 1) return sel ? KEY_CTRL_DEL : KEY_CTRL_AC;
  if (r == 4) {
    std::string s, rp;
    lock_alpha(); // letters at once, as in KhiCAS's search
    const int ok = focus_prompt("Find", 0, s, false) == KEY_CTRL_EXE && !s.empty();
    if (ok) focus_prompt("Replace with (empty: find only)", 0, rp, false);
    reset_alpha();
    if (!ok) return 0;
    t->clipline = -1;
    if (find_next(t, s)) { search = s; replace = rp; }
    else focus_status_msg("Not found");
    return 0;
  }
  if (r == 5) {
    std::string s;
    if (focus_prompt("Go to line", 0, s, true) == KEY_CTRL_EXE && !s.empty())
      goto_line(t, strtol(s.c_str(), 0, 10));
    return 0;
  }
  return r < 0 ? 0 : keys[r];
}

// F5: the file menu; returns ED_LEAVE to leave
static int file_menu(textArea * t) {
  const char * lab[] = {"Check and load", "Save", "Save as\xe2\x80\xa6", "Insert a file\xe2\x80\xa6", "Clear all",
                        "Text size", "Shortcuts", "Leave"};
  const char * hint[] = {"2nd enter", t->filename.c_str(), 0, 0, "copied first", t->minimini ? "small" : "large",
                         0, "saves and loads"};
  focus_tab_open(4, TABS[4]);
  const int r = focus_list(0, lab, 8, 0, hint);
  if (r >= 2 && r <= 4) refresh(t, true); // a prompt or a list next: over the text
  char fn[MAX_FILENAME_SIZE + 1];
  switch (r) {
  case 0: check(t); break;
  case 2:
    if (!get_filename(fn, ".py")) break;
    t->filename = fn;
    // fall through: save under the new name
  case 1:
    save_script(t->filename.c_str(), merge_area(t->elements));
    focus_status_msg("Saved");
    break;
  case 3: {
    std::string ins;
    if (fileBrowser(fn, (char *)"*.py", (char *)"Scripts") && load_script(fn, ins))
      insert(t, ins.c_str(), false);
  } break;
  case 4: {
    static const char * const ch[] = {"Clear", "Cancel"};
    if (focus_choose("Clear the program?", ch, 2) != 0) break;
    if (merged_size(t->elements) < get_free_memory() / 8)
      copy_clipboard(merge_area(t->elements), false);
    set_undo(t);
    t->elements.clear();
    t->line = t->pos = 0;
    add(t, "");
  } break;
  case 5: t->minimini = !t->minimini; break;
  case 6: focus_text("Shortcuts", ED_HELP); break;
  case 7: return ED_LEAVE;
  }
  return 0;
}

// the command search (2nd 0), the word before the caret as the query: the command goes in as
// its template (irem(,): the caret in the first argument)
static void catalog(textArea * t) {
  std::string & L = t->elements[t->line].s;
  int w = t->pos;
  while (w > 0 && namec(L[w - 1])) --w;
  char q[24] = "", out[64], tp[64];
  if (t->pos - w >= 2 && t->pos - w < 24) { memcpy(q, L.c_str() + w, t->pos - w); q[t->pos - w] = 0; }
  if (!focus_catalog(q, out, sizeof(out) - 1) || !out[0]) return;
  if (q[0]) { L.erase(L.begin() + w, L.begin() + t->pos); t->pos = w; }
  const int l = strlen(out);
  int caret = l;
  if (l > 1 && out[l - 1] == '(') {
    if (!focus_call_template(out, tp, sizeof(tp), &caret)) { strcpy(tp, out); strcpy(tp + l, ")"); caret = l; }
  } else strcpy(tp, out);
  const int p0 = t->pos;
  insert(t, tp, false);
  t->pos = p0 + caret;
}

// the math key: the console's math templates (fraction, root, integral...), inserted as text
static void math_key(textArea * t) {
  const char * s = 0;
  int back = 0;
  if (focus_popover(KEY_CTRL_SYMB, &s, &back) != FA_INSERT || !s) return;
  insert(t, s, false);
  t->pos -= back;
}

// enter right before the colon ending a header (if x>0:, def f(x):): to the line's end first,
// so that the new line is the block's (a template leaves the caret before that colon)
static void before_colon(textArea * t) {
  const std::string & L = t->elements[t->line].s;
  int i = t->pos, n = L.size();
  while (n > i && L[n - 1] == ' ') --n;
  if (n == i + 1 && L[i] == ':') t->pos = L.size();
}

int focus_edit(textArea * t) {
  const int view0 = focus_view;
  focus_view = 1; // cards (menus, prompts, notes) draw over the editor, which repaints itself
  if (shown != t) { shown = t; top = 0; }
  std::string search, replace;
  int chrome = 1, full = 1, layer = -1, res = TEXTAREA_RETURN_EXIT;
  for (;;) {
    if (chrome) {
      focus_status_view(t->allowEXE ? "TEXT" : t->python ? "PYTHON" : "PROGRAM", 1);
      layer = -1;
    }
    refresh(t, full || chrome);
    chrome = full = 0;
    if (t->clipline >= 0 && search.empty()) focus_status_msg("F3: copy or cut");
    const int kf = (Char)Setup_GetEntry(0x14);
    if (kf != layer) { focus_bar_editor(TABS, kf); layer = kf; }
    focus_status();
    int key;
    ck_getkey(&key);
    while (key == KEY_CTRL_SHIFT || key == KEY_CTRL_ALPHA) { // 2nd, alpha: the chip, the bar's layer
      const int k2 = (Char)Setup_GetEntry(0x14);
      focus_status();
      focus_bar_editor(TABS, k2);
      layer = k2;
      ck_getkey(&key);
    }
    if (key != KEY_CTRL_PRGM && key != KEY_CHAR_FRAC && key != KEY_CTRL_MIXEDFRAC)
      key = translate_fkey(key);
    if (search.size()) { // finding: enter goes to the next match (replacing the current one first)
      if (key == KEY_CTRL_EXE) {
        if (replace.size() && t->clipline == t->line) {
          set_undo(t);
          std::string & L = t->elements[t->line].s;
          L = L.substr(0, t->clippos) + replace + L.substr(t->pos, L.size() - t->pos);
          t->pos = t->clippos + replace.size();
        }
        t->clipline = -1;
        if (!find_next(t, search)) { search = ""; focus_status_msg("No more"); }
        continue;
      }
      search = ""; replace = "";
      t->clipline = -1;
      focus_status_msg(0);
      if (key == KEY_CTRL_EXIT || key == KEY_CTRL_AC) continue;
    }
    else focus_status_msg(0);
    if (key == KEY_CTRL_F1 || key == KEY_CTRL_F2 || key == KEY_CTRL_F4) {
      const bool py = t->python;
      const char * s = key == KEY_CTRL_F2 ? (py ? pick(1, PY_TESTS, N(PY_TESTS), 1) : pick(1, XC_TESTS, N(XC_TESTS), 1))
        : key == KEY_CTRL_F1 ? (py ? pick(0, PY_BLOCKS, N(PY_BLOCKS), 0) : pick(0, XC_BLOCKS, N(XC_BLOCKS), 0))
        : (py ? pick(3, PY_SYMBOLS, N(PY_SYMBOLS), 1) : pick(3, XC_SYMBOLS, N(XC_SYMBOLS), 1));
      if (s) { t->clipline = -1; put(t, s); }
      chrome = 1;
      continue;
    }
    if (key == KEY_CTRL_F3) {
      key = edit_menu(t, search, replace);
      chrome = 1;
      if (search.size()) focus_status_msg(replace.size() ? "enter: replace" : "enter: next match");
      if (!key) continue;
    }
    if (key == KEY_CTRL_F5) {
      const int r = file_menu(t);
      chrome = 1;
      if (r == ED_LEAVE && leave(t)) break;
      continue;
    }
    if (key == KEY_CTRL_SYMB) { t->clipline = -1; math_key(t); chrome = 1; continue; }
    if (key == KEY_CTRL_CATALOG) { t->clipline = -1; catalog(t); chrome = 1; continue; }
    if (key == KEY_CHAR_CR) { check(t); chrome = 1; continue; }
    if (key == KEY_CTRL_EXIT && t->clipline < 0) key = KEY_CTRL_F16;
    if (key == KEY_CTRL_PRGM || key == KEY_CTRL_F16) {
      if (leave(t)) break;
      chrome = 1;
      continue;
    }
    if (key == KEY_CTRL_EXE && t->clipline < 0 && !t->allowEXE) before_colon(t);
    const int r = handle_key(t, key, kf);
    if (r >= 0) { res = r; break; }
    if (r == -1) { check(t); chrome = 1; }
    if (r == -2) full = 1;
    if (r == -5) chrome = 1; // a menu or a card covered the screen
  }
  focus_view = view0;
  focus_status_view(0, 0);
  focus_status_msg(0);
  focus_invalidate();
  p_top = -1;
  return res;
}
