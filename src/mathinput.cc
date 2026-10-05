// mathinput.cc - 2D (MathPrint style) display of the 1D console input buffer.
//
// The user edits a plain giac string with a caret index; this module draws that string
// as 2D math with the caret inside the layout, moves the caret in 2D and decides what
// Backspace deletes. Pure functions of (buffer, caret, metrics); output is a display list.
//
// Pipeline: lex+parse (recursive descent) into a static pool of 16-byte nodes, measure
// (bottom up), place (top down). The place pass emits the display ops and the caret
// location of every buffer position 0..len, in increasing position order.
//
// Input language (tolerant giac subset; never fails):
//  tokens: numbers 12 1.5 .5 2e-3, names [A-Za-z_\x80-\xff][A-Za-z0-9_\x80-\xff]*,
//    strings "..." (\x escapes), := => -> == != <= >= **, other single chars. Blanks
//    belong to the following token.
//  grammar, low to high: (1) ; and , (2) := => -> (3) = == != < > <= >= (4) binary + -
//    (5) * / implicit multiplication (6) prefix - + ! ' (7) ^ ** (right assoc)
//    (8) postfix ! ' (9) number name name(args) (..) [..] {..} string.
//  Levels 1-5 are kept as flat rows; a / takes everything of its level-5 term as the
//  numerator. Missing operands are boxes (not after a trailing ;, not in f() [] {}).
//  An unclosed group runs to the end; a closer of an enclosing group closes the inner
//  ones; unknown tokens and stray closers are plain text.
//  Limits: nesting deeper than 24, ~176 nodes or ~384 ops: the rest of the buffer is one
//  plain text op. Layout recursion is capped too (a/b/c/... chains nest without parser
//  recursion): a subtree deeper than RMAX is drawn as plain text.
//
// Layout (y grows down, baseline 0; f = current font; axis = -(f.asc/3)):
//  text: adv per glyph (UTF-8 sequence = 1 glyph); blank = adv/2; pi/infinity = 1 glyph.
//  binary + - = == != < > <= >= := => -> get opgap each side; , and ; are followed by adv/2;
//  * is a dot box adv/2+2 wide at mid-x-height; implicit multiplication is a 1 px gap.
//  a/b: stacked, centered, bar (1px) at the axis, width max+4, 2 px gaps, same font.
//  a^b: b in the small font, its bottom at 45% of the base's ascent above the baseline.
//  Hidden parentheses: an operand of / or the exponent of ^ that is exactly (..).
//  Visible ( [ {: boxes adv/2+1 wide, as tall as the content (at least the font).
//  name(args): name, parens, args separated by ", ". Special calls (exact arg count;
//  their parentheses and commas are hidden structure):
//   sqrt(a) radical; surd(a,n) radical with small index n; abs(a) bars;
//   integrate|int(f,x[,a,b]) sign (content+4 tall, 6 wide), bounds small at the top/bottom
//   right, f, 2 px, "d", x; diff(f,x) d/(d x) then f (in parens if f is a sum/relation);
//   sum(f,k,a,b) sigma (2*asc) with k=a below, b above, then f;
//   limit(f,x,a) "lim" over x ->a (small), then f;  exp(a) "e" with exponent a.
//   integrate/diff/sum/limit end with a 2 px gap so "after the construct" and "end of the
//   last slot" are different caret locations.
//
// Caret: position p gets the location (x, top, h=font height) of the deepest row (slot)
// containing it; positions inside hidden structure (function names, hidden ( ) , / ^)
// alias the previous position, so they share the location of the end of the slot they
// follow (or of the construct's left edge). Left/right go to the nearest position with a
// different location (left lands on the first index of the target's run, i.e. inside the
// slot, never between structure chars).
// Up/down: candidates are the positions inside the innermost 2D construct enclosing the
// caret that has any, then the next one outwards. A construct's left/right edge positions
// belong to the parent row. A power's base line counts as inside the power for candidates
// (down from the exponent lands on the base line) but not for the caret (up from the main
// line is -1). Among candidates: entirely below/above the caret's vertical range first,
// else partially lower/higher; then vertically nearest (quantized to half a small line),
// then smallest |dx|, then smallest index. No enclosing construct: -1 (console history).
//
// Backspace: a visible char deletes one glyph; a hidden structure char deletes its whole
// construct (name included; for a power from the ^) when all its slots are empty (the
// default variable x, k for sum, counts as empty), else it acts like LEFT.
//
// Not reentrant: one static node pool (MAXN nodes of 16 bytes) and static state.

#include "mathinput.h"

// node pool, parser nesting, display ops budget, layout recursion, construct chain
enum { MAXN = 192, MAXD = 24, MAXOPS = 400, RMAX = 2 * MAXD + 4, MAXC = 64 };

// token types: TK_* or the punctuation char itself
enum { TK_END, TK_NUM, TK_NAME, TK_STR, OP_DEF, OP_IMP, OP_ARR, OP_EQ, OP_NE, OP_LE, OP_GE,
       OP_POW, TK_OTHER = 31 };

// node kinds; leaves first
enum { K_TEXT, K_OP, K_OPU, K_SEP, K_DOT, K_BLANK, K_FLAT, K_PI, K_INF,
       K_EMPTY, K_ROW, K_GROUP, K_CALL, K_FRAC, K_POW,
       K_SQRT, K_SURD, K_ABS, K_INT, K_DEFINT, K_DIFF, K_SUM, K_LIM, K_EXP };
enum { F_IMPL = 1, F_HID = 2, F_CLOSED = 4, F_NOBOX = 8, F_ARGSEP = 16 };

struct mi_node {
  unsigned char k, f;     // kind, flags
  short a, b, c;          // buffer span [a,b); c: core start after leading blanks
  unsigned char kid, nx;  // first child, next sibling (0: none)
  short w, as, ds;        // measured width, ascent, descent
};

static mi_node T[MAXN + 1];  // T[0] = nil, T[MAXN] = overflow sink (never used in practice)
static int nn, nops, dep, broken, rtail, ftail;
static unsigned char nopen[3];   // open ( [ { groups
static const char * S;
static int L;
static const mi_metrics * M;
static int ta, tc, tb, tt;       // current token [ta,tb), core at tc, type tt

#ifdef MI_TEST
int mi_test_errors;              // pool overflow or position emission order errors
#define MI_ERR() (++mi_test_errors)
#else
#define MI_ERR() ((void)0)
#endif

static int mx(int a, int b) { return a > b ? a : b; }
static int mn(int a, int b) { return a < b ? a : b; }
static int ch(int i) { return (unsigned char)S[i]; }
static int blank(int c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
static int digit(int c) { return c >= '0' && c <= '9'; }
static int namec(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c >= 0x80; }
static int gstart(int i) { return (ch(i) & 0xC0) != 0x80; }  // not a UTF-8 continuation byte
static int digits(int i) { while (i < L && digit(ch(i))) ++i; return i; }
static const mi_font & fnt(int sm) { return sm ? M->small : M->big; }

// ---------------------------------------------------------------- lexer
static void lex(int i) {
  ta = i;
  while (i < L && blank(ch(i))) ++i;
  tc = i;
  if (i >= L) { tt = TK_END; tb = L; return; }
  int c = ch(i), d = i + 1 < L ? ch(i + 1) : 0;
  if (digit(c) || (c == '.' && digit(d))) {
    i = digits(i);
    if (i < L && S[i] == '.') i = digits(i + 1);
    if (i < L && (S[i] | 32) == 'e') { // exponent only if digits follow
      int j = i + 1;
      if (j < L && (S[j] == '+' || S[j] == '-')) ++j;
      if (j < L && digit(ch(j))) i = digits(j);
    }
    tt = TK_NUM;
  } else if (namec(c)) {
    while (i < L && (namec(ch(i)) || digit(ch(i)))) ++i;
    tt = TK_NAME;
  } else if (c == '"') {
    for (++i; i < L && S[i] != '"'; ++i)
      if (S[i] == '\\' && i + 1 < L) ++i;
    if (i < L) ++i;
    tt = TK_STR;
  } else {
    static const char two[] = ":==>->==!=<=>=**";
    tt = c < 32 ? TK_OTHER : c;
    ++i;
    for (int j = 0; j < 16; j += 2)
      if (c == two[j] && d == two[j + 1]) { tt = OP_DEF + j / 2; ++i; break; }
  }
  tb = i;
}

static int binlev(int t) { // binary operator precedence level, 0: none
  switch (t) {
  case ';': case ',': return 1;
  case OP_DEF: case OP_IMP: case OP_ARR: return 2;
  case '=': case OP_EQ: case OP_NE: case '<': case '>': case OP_LE: case OP_GE: return 3;
  case '+': case '-': return 4;
  case '*': case '/': return 5;
  }
  return 0;
}
static int prim(int t) { return t == TK_NUM || t == TK_NAME || t == TK_STR || t == '(' || t == '[' || t == '{'; }
static int prefix(int t) { return t == '-' || t == '+' || t == '!' || t == '\''; }
static int powop(int t) { return t == '^' || t == OP_POW; }
static int otype(int t) { return t == '(' ? 0 : t == '[' ? 1 : t == '{' ? 2 : -1; }
static int ctype(int t) { return t == ')' ? 0 : t == ']' ? 1 : t == '}' ? 2 : -1; }

// ---------------------------------------------------------------- parser
static int newnode(int k, int a, int b, int ops) {
  int n = nn;
  if (n < MAXN) ++nn; else { n = MAXN; broken = 1; MI_ERR(); }
  mi_node & d = T[n];
  d.k = k; d.f = 0; d.a = a; d.b = b; d.c = a; d.kid = d.nx = 0; d.w = d.as = d.ds = 0;
  nops += ops; // upper bound of the display ops this node will produce
  return n;
}
static int leaf(int k) { int n = newnode(k, ta, tb, 1); T[n].c = tc; return n; }
static int box(int p) { return newnode(K_EMPTY, p, p, 1); }
// budget exhausted: keep a reserve for the nodes created while unwinding (<= 2 per level)
static int over() { return nops > MAXOPS - 16 || nn + 2 * dep + 16 > MAXN; }
static int flat() { // the rest of the buffer as plain text
  if (tt == TK_END) return 0;
  int n = newnode(K_FLAT, ta, L, 1);
  ta = tc = tb = L; tt = TK_END;
  return n;
}
static void add(int & h, int & t, int h2, int t2) { if (h) T[t].nx = h2; else h = h2; t = t2; }
static int wrap(int h, int t) { // list -> single node
  if (h == t) return h;
  int r = newnode(K_ROW, T[h].a, T[t].b, 0);
  T[r].kid = h;
  return r;
}
static void hide(int n) { if (T[n].k == K_GROUP && S[T[n].c] == '(') T[n].f |= F_HID; }
static int namei(int n, const char * w) { // name core == w
  int i = T[n].c;
  for (; *w; ++w, ++i)
    if (i >= T[n].b || S[i] != *w) return 0;
  return i == T[n].b;
}

static int pseq(int stopc);
static int pfactor();

static int pgroup() { // ( [ { : content to the matching closer or the end
  int ty = otype(tt), g = newnode(K_GROUP, ta, 0, 2), c;
  T[g].c = tc;
  lex(tb); ++nopen[ty];
  c = pseq(0);
  if (!c) { c = box(ta); if (ty) T[c].f |= F_NOBOX; } // () shows a box, [] {} nothing
  --nopen[ty];
  T[g].kid = c;
  if (ctype(tt) == ty) { T[g].f |= F_CLOSED; T[g].b = tb; lex(tb); }
  else T[g].b = T[c].b;
  return g;
}

static const struct { const char * nm; unsigned char na, k; } spec[] = {
  {"sqrt", 1, K_SQRT}, {"surd", 2, K_SURD}, {"abs", 1, K_ABS}, {"integrate", 2, K_INT},
  {"integrate", 4, K_DEFINT}, {"int", 2, K_INT}, {"int", 4, K_DEFINT}, {"diff", 2, K_DIFF},
  {"sum", 4, K_SUM}, {"limit", 3, K_LIM}, {"exp", 1, K_EXP}};

static int pcall(int nm) { // name( args ): normal or special call
  int cl = newnode(K_CALL, T[nm].a, 0, 4), t = nm, na = 0, a, k = 0;
  lex(tb); ++nopen[0];
  for (;;) {
    a = pseq(1);
    if (!a) a = box(ta);
    T[t].nx = a; t = a; ++na;
    if (tt != ',') break;
    a = leaf(K_SEP); T[a].f |= F_ARGSEP;
    T[t].nx = a; t = a; lex(tb);
  }
  --nopen[0];
  if (tt == ')') { T[cl].f |= F_CLOSED; T[cl].b = tb; lex(tb); }
  else T[cl].b = T[t].b;
  for (unsigned i = 0; i < sizeof(spec) / sizeof(spec[0]); ++i)
    if (spec[i].na == na && namei(nm, spec[i].nm)) k = spec[i].k;
  if (k) { T[cl].k = k; T[cl].kid = T[nm].nx; T[cl].c = T[nm].c; } // name is structure
  else {
    T[cl].kid = nm;
    if (na == 1 && T[t].k == K_EMPTY) T[t].f |= F_NOBOX; // f(): no box
  }
  return cl;
}

static int primary() {
  int k = tt, n;
  if (k == TK_NUM || k == TK_STR) { n = leaf(K_TEXT); lex(tb); return n; }
  if (k == TK_NAME) {
    n = leaf(K_TEXT); lex(tb);
    if (namei(n, "pi")) T[n].k = K_PI;
    else if (namei(n, "infinity")) T[n].k = K_INF;
    return tt == '(' ? pcall(n) : n;
  }
  return otype(k) >= 0 ? pgroup() : 0;
}

// levels 6-9: prefix ops, primary, postfix ops, ^. Returns a list head (tail in ftail).
static int pfactor() {
  if (tt == TK_END) return 0;
  if (dep > MAXD || over()) return ftail = flat(); // nesting deeper than 24: plain text
  ++dep;
  int h = 0, t = 0, b, bt, n;
  while (prefix(tt) && !over()) { n = leaf(K_OPU); add(h, t, n, n); lex(tb); }
  b = over() ? flat() : primary();
  if (!b && (h || powop(tt))) b = box(ta); // missing operand
  if (b) {
    bt = b;
    while ((tt == '!' || tt == '\'') && !over()) { n = leaf(K_OPU); T[bt].nx = n; bt = n; lex(tb); }
    if (powop(tt) && !over()) {
      int base = wrap(b, bt), p = newnode(K_POW, T[base].a, 0, 0), e;
      lex(tb);
      e = pfactor(); // right associative, may start with - (level 6)
      e = e ? wrap(e, ftail) : box(ta);
      hide(e);
      T[p].kid = base; T[base].nx = e; T[p].b = T[e].b;
      b = bt = p;
    }
    add(h, t, b, bt);
  }
  --dep;
  ftail = t;
  return h;
}

// levels 1-5 as one flat row. Returns a list head (tail in rtail).
static int prow(int stopc) {
  int h = 0, t = 0, tp = 0, need = 1, nb = 0, n; // tp: item before the current level-5 term
  for (;;) {
    int k = tt, lv = binlev(k);
    if (stopc && k == ',') lv = 0;
    if (over()) { if ((n = flat())) { add(h, t, n, n); need = 0; } break; }
    if (need ? prim(k) || prefix(k) || powop(k) : prim(k)) { // operand, or implicit *
      if (!(n = pfactor())) break;
      if (!need) T[n].f |= F_IMPL;
      add(h, t, n, ftail); need = 0;
      continue;
    }
    if (!lv) break;
    if (need && (nb || k != ';')) { n = box(ta); add(h, t, n, n); } // missing left operand
    if (k == '/') { // numerator = current term
      int first = tp ? T[tp].nx : h, num, fr, den;
      if (tp) T[tp].nx = 0; else h = 0;
      num = wrap(first, t);
      t = tp;
      fr = newnode(K_FRAC, T[num].a, 0, 1);
      lex(tb);
      den = pfactor();
      den = den ? wrap(den, ftail) : box(ta);
      hide(num); hide(den);
      T[fr].kid = num; T[num].nx = den; T[fr].b = T[den].b;
      add(h, t, fr, fr); need = nb = 0;
      continue;
    }
    n = leaf(lv == 1 ? K_SEP : k == '*' ? K_DOT : K_OP);
    add(h, t, n, n); lex(tb);
    if (lv < 5) tp = n;
    need = 1; nb = k != ';';
  }
  if (need && nb) { n = box(ta); add(h, t, n, n); } // missing right operand
  rtail = t;
  return h;
}

// rows plus stray tokens up to the end, a closer of an open group or (stopc) a comma
static int pseq(int stopc) {
  int h = 0, t = 0, n;
  for (;;) {
    if ((n = prow(stopc))) add(h, t, n, rtail);
    if (tt == TK_END) {
      if (ta < L) { n = newnode(K_BLANK, ta, L, 0); T[n].c = L; add(h, t, n, n); ta = tc = L; }
      break;
    }
    if (stopc && tt == ',') break;
    int ty = ctype(tt);
    if (ty >= 0 && nopen[ty]) break;
    if (over()) n = flat();
    else { n = leaf(K_TEXT); lex(tb); } // stray token: plain text
    add(h, t, n, n);
  }
  return h ? wrap(h, t) : 0;
}

static int parse() {
  nn = 1; nops = dep = broken = 0;
  nopen[0] = nopen[1] = nopen[2] = 0;
  lex(0);
  int r = pseq(0);
  if (broken) { nn = 1; nops = 0; r = L ? newnode(K_FLAT, 0, L, 1) : 0; } // cannot happen
  return r;
}

// ---------------------------------------------------------------- measure / geometry
static int slot(int n, int i) { // i-th slot content of a 2D construct (hidden parens removed)
  int c = T[n].kid;
  if (T[n].k == K_POW) c = T[c].nx;
  for (; c; c = T[c].nx)
    if (!(T[c].f & F_ARGSEP) && !i--) return (T[c].k == K_GROUP && (T[c].f & F_HID)) ? T[c].kid : c;
  return 0;
}
static int sfont(int k, int i, int sm) { // font of slot i (must match the sl() calls in cons)
  if (k == K_POW || k == K_EXP || (k == K_SURD && i == 1) || (k == K_DEFINT && i >= 2) ||
      ((k == K_SUM || k == K_LIM) && i >= 1)) return 1;
  return sm;
}
static int needp(int n) { // diff(f,x): f needs parentheses (top level + - = , ...)
  if (T[n].k == K_ROW)
    for (int c = T[n].kid; c; c = T[c].nx)
      if (T[c].k == K_OP || T[c].k == K_SEP) return 1;
  return 0;
}

// display ops (only when building)
static mi_layout * OUT;
static void op(int code, int sm, int x, int y, int w, int h, int pos, int len, const char * lit) {
  if (!OUT) return;
  mi_op o;
  o.code = code; o.small = sm; o.x = x; o.y = y; o.w = w; o.h = h; o.pos = pos; o.len = len; o.lit = lit;
  OUT->ops.push_back(o);
}

// cons() describes the geometry of a 2D construct once, for three uses (gm):
// GM_MEASURE: union of the vertical extents (by0,by1) and width cw; GM_DECO: emit its
// decoration ops; i >= 0: origin (gx,gy) and font gs of slot i.
// Coordinates passed to sl/dec/dtext are offsets from the construct origin (ox, oy).
enum { GM_MEASURE = -2, GM_DECO = -1 };
static int gm, gx, gy, gs, cw, by0, by1, ox, oy, sn[4];
static void sl(int i, int x, int y, int sm) { // slot i (node sn[i]) at (x, y)
  if (gm == GM_MEASURE) { by0 = mn(by0, y - T[sn[i]].as); by1 = mx(by1, y + T[sn[i]].ds); }
  else if (gm == i) { gx = ox + x; gy = oy + y; gs = sm; }
}
static void dec(int code, int sm, int x, int y, int w, int h) { // decoration box
  if (gm == GM_MEASURE) { by0 = mn(by0, y); by1 = mx(by1, y + h); }
  else if (gm == GM_DECO) op(code, sm, ox + x, oy + y, w, h, -1, 0, 0);
}
static void dtext(const char * lit, int ng, int sm, int x, int y) { // literal text, ng glyphs
  const mi_font & f = fnt(sm);
  if (gm == GM_MEASURE) { by0 = mn(by0, y - f.asc); by1 = mx(by1, y + f.desc); }
  else if (gm == GM_DECO) op(MI_TEXT, sm, ox + x, oy + y, ng * f.adv, f.asc + f.desc, -1, ng, lit);
}

static void cons(int n, int x, int y, int sm) {
  const mi_font & f = fnt(sm), & sf = M->small;
  int k = T[n].k, a = f.adv, h = a / 2, ax = f.asc / 3, t, u, v, i;
  for (i = 0; i < 4; ++i) sn[i] = slot(n, i);
  const mi_node & A = T[sn[0]], & B = T[sn[1]], & C = T[sn[2]], & D = T[sn[3]]; // T[0]: none
  int ca = mx(f.asc, A.as), cd = mx(f.desc, A.ds); // content extent, at least the font
  ox = x; oy = y;
  switch (k) {
  case K_FRAC: // bar at the axis, parts centered, 2 px gaps
    cw = mx(A.w, B.w) + 4;
    dec(MI_HLINE, sm, 0, -ax, cw, 1);
    sl(0, (cw - A.w) / 2, -ax - 2 - A.ds, sm);
    sl(1, (cw - B.w) / 2, -ax + 3 + B.as, sm);
    break;
  case K_POW: case K_EXP: { // exponent bottom at 45% of the base ascent
    int b = k == K_POW ? T[n].kid : 0, bw = b ? T[b].w : a, ba = b ? T[b].as : f.asc;
    if (b) { if (gm == GM_MEASURE) { by0 = mn(by0, -ba); by1 = mx(by1, T[b].ds); } }
    else dtext("e", 1, sm, 0, 0);
    sl(0, bw, -(ba * 45 / 100) - A.ds, 1);
    cw = bw + A.w;
  } break;
  case K_SQRT: case K_SURD: // sign 6 px + 1, overbar 1 px above the content, index overhang t
    t = k == K_SURD ? mx(0, B.w - 3) : 0;
    cw = t + A.w + 8;
    dec(MI_SQRT, sm, t, -ca - 2, A.w + 8, ca + 2 + cd);
    sl(0, t + 7, 0, sm);
    if (k == K_SURD) sl(1, t + 3 - B.w, (cd - ca - 2) / 2 - 1 - B.ds, 1);
    break;
  case K_ABS:
    t = mx(1, h - 1);
    dec(MI_BAR, sm, 0, -ca, t, ca + cd);
    sl(0, t + 1, 0, sm);
    dec(MI_BAR, sm, t + 2 + A.w, -ca, t, ca + cd);
    cw = 2 * t + 2 + A.w;
    break;
  case K_INT: case K_DEFINT: // sign, [bounds], f, 2 px, d, x
    ca = mx(ca, B.as); cd = mx(cd, B.ds);
    dec(MI_INTEGRAL, sm, 0, -ca - 2, 6, ca + cd + 4);
    t = 7;
    if (k == K_DEFINT) { // a at the bottom right, b at the top right, never overlapping
      u = (cd - ca) / 2;
      sl(2, 7, mx(cd + 2 - C.ds, u + 1 + C.as), 1);
      sl(3, 7, mn(-ca - 2 + D.as, u - 1 - D.ds), 1);
      t += mx(C.w, D.w) + 1;
    }
    sl(0, t, 0, sm);
    t += A.w + 2;
    dtext("d", 1, sm, t, 0);
    sl(1, t + a, 0, sm);
    cw = t + a + B.w + 2; // trailing gap: "after the construct" differs from "end of x"
    break;
  case K_DIFF: // d/(d x) then f, in parens if it is a sum
    i = needp(sn[0]);
    u = mx(f.asc, B.as); v = a + B.w + 4;
    dec(MI_HLINE, sm, 0, -ax, v, 1);
    dtext("d", 1, sm, (v - a) / 2, -ax - 2 - f.desc);
    dtext("d", 1, sm, 2, -ax + 3 + u);
    sl(1, 2 + a, -ax + 3 + u, sm);
    t = v + 2;
    if (i) { dec(MI_LPAREN, sm, t, -ca, h + 1, ca + cd); t += h + 1; }
    sl(0, t, 0, sm);
    if (i) dec(MI_RPAREN, sm, t + A.w, -ca, h + 1, ca + cd);
    cw = t + A.w + (i ? h + 1 : 2);
    break;
  case K_SUM: { // column: b, sigma, k=a; then f
    int sh = 2 * f.asc, sw = a + 4, st = -ax - sh / 2, bx, bb;
    u = B.w + sf.adv + C.w; v = mx(sf.asc, mx(B.as, C.as));
    t = mx(sw, mx(u, D.w));
    dec(MI_SIGMA, sm, (t - sw) / 2, st, sw, sh);
    bx = (t - u) / 2; bb = st + sh + 1 + v;
    sl(1, bx, bb, 1);
    dtext("=", 1, 1, bx + B.w, bb);
    sl(2, bx + B.w + sf.adv, bb, 1);
    sl(3, (t - D.w) / 2, st - 1 - D.ds, 1);
    sl(0, t + 2, 0, sm);
    cw = t + 4 + A.w;
  } break;
  case K_LIM: { // column: lim over x->a; then f
    int bx, bb;
    u = B.w + sf.adv + C.w; v = mx(sf.asc, mx(B.as, C.as));
    t = mx(3 * a, u);
    dtext("lim", 3, sm, (t - 3 * a) / 2, 0);
    bx = (t - u) / 2; bb = f.desc + 1 + v;
    sl(1, bx, bb, 1);
    dtext("\x1e", 1, 1, bx + B.w, bb);
    sl(2, bx + B.w + sf.adv, bb, 1);
    sl(0, t + h, 0, sm);
    cw = t + h + A.w + 2;
  } break;
  }
}

// Layout recursion depth (measure and place recurse identically). A subtree deeper than
// RMAX (e.g. a long a/b/c/d/... chain, which nests without parser recursion) is drawn as
// plain text, like K_FLAT.
static int rd;
static int kindof(int n) { return rd > RMAX ? (int)K_FLAT : (int)T[n].k; }
static int corec(int n, int k) { return k == K_FLAT ? T[n].a : T[n].c; }

static void measure(int n, int sm) {
  mi_node & d = T[n];
  const mi_font & f = fnt(sm);
  int k, h = f.adv / 2, w, c, first;
  ++rd;
  k = kindof(n); c = corec(n, k); w = (c - d.a) * h;
  d.as = f.asc; d.ds = f.desc;
  if (k < K_EMPTY) { // leaf: leading blanks, glyphs, gaps
    if (k == K_DOT) w += h + 2;
    else if (k == K_PI || k == K_INF) w += f.adv;
    else if (k != K_BLANK) {
      for (int i = c; i < d.b; ++i)
        if (i == c || gstart(i)) w += f.adv;
      if (k == K_OP) w += 2 * M->opgap;
      if (k == K_SEP) w += h;
    }
    d.w = w;
  } else if (k == K_EMPTY) d.w = (d.f & F_NOBOX) ? 0 : f.adv;
  else if (k <= K_CALL) { // row, group, call: items side by side, delimiters as tall as the content
    c = d.kid; w = 0;
    if (k == K_CALL) { measure(c, sm); w = T[c].w; c = T[c].nx; }
    for (first = c; c; c = T[c].nx) {
      measure(c, sm);
      w += T[c].w + (T[c].f & F_IMPL ? 1 : 0);
      d.as = mx(d.as, T[c].as); d.ds = mx(d.ds, T[c].ds);
      if (k != K_ROW && !(d.f & F_HID)) {
        if (c == first) w += (T[c].a - 1 - (k == K_CALL ? T[d.kid].b : d.a)) * h + h + 1;
        if (!T[c].nx && (d.f & F_CLOSED)) w += (d.b - 1 - T[c].b) * h + h + 1;
      }
    }
    d.w = w;
  } else { // 2D construct
    if (k == K_POW) measure(d.kid, sm);
    for (int i = 0, s; (s = slot(n, i)); ++i) measure(s, sfont(k, i, sm));
    gm = GM_MEASURE; by0 = -f.asc; by1 = f.desc;
    cons(n, 0, 0, sm);
    d.w = cw; d.as = -by0; d.ds = by1;
  }
  --rd;
}

// ---------------------------------------------------------------- caret positions
// emit() receives every position 0..len in increasing order; for the same position the
// deepest claim wins (a slot edge beats the row boundary at the same index). The previous
// position is kept pending and handed to deliver() once it is final.
// Each position also records which 2D constructs it was emitted inside (ck: stack of
// construct nodes being placed). A construct's own edges belong to the parent row. A
// power's base line (edges included) is "weakly" inside the power: as a candidate it is
// inside (down from the exponent reaches it), as the caret it is not (up from the main
// line still returns -1). Structure positions inherit everything from the position they
// alias.
enum { MD_LOC, MD_LEFT, MD_RIGHT, MD_UP, MD_DOWN };
static int md, tgt, pp, pd, pc, px, py, ph, lx, ly, lh, rs, rx, ry, rh, res, ckn, tkn, cp, bl, bk[5];
static unsigned char ck[MAXC], tk[MAXC]; // constructs enclosing the current / target position
// cp: length of the common prefix of ck and tk (target chain), maintained by push/pop
// bl: stack depth while emitting the base line of the innermost power, else -1

static void push(int n) {
  if (ckn < MAXC) ck[ckn] = n;
  if (cp == ckn && ckn < tkn && tk[ckn] == n) cp = ckn + 1;
  ++ckn;
}
static void pop() { if (cp >= ckn) cp = ckn - 1; --ckn; }

static void cand(int p, int x, int y, int h, int c) { // up/down candidate
  int t = y, b = y + h, ct = ly, cb = ly + lh, q = mx(1, (M->small.asc + M->small.desc) / 2), key[5], i;
  if (!c) return; // only positions inside the target's 2D constructs (top level: -1)
  if (md == MD_UP) { t = -b; b = -y; ct = -cb; cb = -ly; } // mirror: up becomes down
  // tier 0: entirely below (gap); tier 1: partially lower (center distance, doubled).
  // Distances are quantized (half a small line) so that rows a pixel or two apart tie
  // and the smallest |dx| decides.
  if (t >= cb) { key[1] = 0; key[2] = (t - cb) / q; }
  else if ((t >= ct && b > cb) || (t > ct && b >= cb)) { key[1] = 1; key[2] = (t + b - ct - cb) / (2 * q); }
  else return;
  key[0] = tkn - c; // innermost shared construct first
  key[3] = x > lx ? x - lx : lx - x;
  key[4] = p;
  for (i = 0; i < 5 && key[i] == bk[i]; ++i) {}
  if (bk[4] < 0 || (i < 5 && key[i] < bk[i]))
    for (i = 0; i < 5; ++i) bk[i] = key[i];
}

static void deliver(int p, int x, int y, int h, int c) {
  int same = x == lx && y == ly && h == lh;
  if (md == MD_LOC) { if (p == tgt) { lx = x; ly = y; lh = h; } }
  else if (md == MD_LEFT) {
    if (p >= tgt) return;
    if (x != rx || y != ry || h != rh) { rs = p; rx = x; ry = y; rh = h; } // run of equal locations
    if (!same) res = rs;
  } else if (md == MD_RIGHT) { if (p > tgt && res < 0 && !same) res = p; }
  else if (p != tgt) cand(p, x, y, h, c);
}

static void emit(int p, int x, int y, int h, int d, int c) {
  if (p == pp) { if (d < pd) return; }
  else {
    if (p < pp || (pp >= 0 && p != pp + 1)) MI_ERR();
    if (p < pp) return;
    if (pp >= 0) deliver(pp, px, py, ph, pc);
  }
  pp = p; px = x; py = y; ph = h; pd = d; pc = c;
  if (p == tgt && md == MD_LOC) {
    tkn = mn(ckn - (ckn == bl), MAXC); // on a power's base line: not inside that power
    for (int i = 0; i < tkn; ++i) tk[i] = ck[i];
  }
}
static void alias(int p, int e) { for (; p < e; ++p) emit(p, px, py, ph, pd, pc); } // structure
static void P(int p, int x, int y, int sm, int d) {
  const mi_font & f = fnt(sm);
  emit(p, x, y - f.asc, f.asc + f.desc, d, cp);
}

// ---------------------------------------------------------------- place
// Draws node n at (x, baseline y); emits positions (a,b] (a itself belongs to the caller,
// a construct may re-claim it from a deeper slot). dp: slot depth.
static void place(int n, int x, int y, int dp, int sm) {
  const mi_node & d = T[n];
  const mi_font & f = fnt(sm);
  int k, c, h = f.adv / 2, p, xx = x;
  ++rd;
  k = kindof(n); c = corec(n, k);
  if (k < K_EMPTY) { // leaf
    for (p = d.a + 1; p <= c; ++p) { xx += h; P(p, xx, y, sm, dp); } // leading blanks
    if (c < d.b) {
      int gx = xx + (k == K_OP ? M->opgap : 0), g = 1;
      if (k == K_DOT) op(MI_DOT, sm, gx, y - f.asc / 4 - 1, h + 2, 2, c, 1, 0);
      else {
        if (k < K_PI) for (p = c + 1; p < d.b; ++p) g += gstart(p);
        op(MI_TEXT, sm, gx, y, g * f.adv, f.asc + f.desc, c, d.b - c,
           k == K_PI ? "pi" : k == K_INF ? "oo" : 0);
        for (p = c + 1; p < d.b; ++p) // inside a glyph: alias
          if (k < K_PI && gstart(p)) { gx += f.adv; P(p, gx, y, sm, dp); }
          else alias(p, p + 1);
      }
    }
    P(d.b, x + d.w, y, sm, dp);
  } else if (k == K_EMPTY) {
    if (!(d.f & F_NOBOX)) op(MI_BOX, sm, x + 1, y - f.asc + 2, f.adv - 2, f.asc - 2, d.a, 0, 0);
  } else if (k <= K_CALL) { // row, visible group, call
    int vis = k != K_ROW && !(d.f & F_HID), code = MI_LPAREN, last;
    c = d.kid; p = d.a;
    if (k == K_CALL) { place(c, x, y, dp, sm); xx += T[c].w; p = T[c].b; c = T[c].nx; }
    if (vis) {
      code += 2 * otype(ch(T[c].a - 1));
      for (++p; p < T[c].a; ++p) { xx += h; P(p, xx, y, sm, dp); }
      op(code, sm, xx, y - d.as, h + 1, d.as + d.ds, T[c].a - 1, 1, 0);
      xx += h + 1;
      P(T[c].a, xx, y, sm, dp);
    }
    for (last = c; c; c = T[c].nx) {
      if (T[c].f & F_IMPL) ++xx;
      place(c, xx, y, dp, sm);
      xx += T[c].w; last = c;
    }
    if (vis && (d.f & F_CLOSED)) {
      for (p = T[last].b + 1; p < d.b; ++p) { xx += h; P(p, xx, y, sm, dp); }
      op(code + 1, sm, xx, y - d.as, h + 1, d.as + d.ds, d.b - 1, 1, 0);
      P(d.b, xx + h + 1, y, sm, dp);
    }
  } else { // 2D construct: decorations, [base], slots in buffer order, structure between
    int obl = bl;
    gm = GM_DECO; cons(n, x, y, sm);
    push(n);
    p = d.a;
    if (k == K_POW) { // the base line, edges included, is weakly inside the power
      bl = ckn;
      P(p, x, y, sm, dp);
      place(d.kid, x, y, dp, sm);
      p = T[d.kid].b;
    }
    for (int i = 0, s; (s = slot(n, i)); ++i) {
      gm = i; cons(n, x, y, sm);
      int sx = gx, sy = gy, ss = gs;
      alias(p + 1, T[s].a);
      bl = -1;
      P(T[s].a, sx, sy, ss, dp + 1);
      place(s, sx, sy, dp + 1, ss);
      p = T[s].b;
    }
    bl = k == K_POW ? ckn : obl;
    alias(p + 1, d.b);
    if (k != K_POW) pop();
    P(d.b, x + d.w, y, sm, dp);
    if (k == K_POW) pop();
    bl = obl;
  }
  --rd;
}

// ---------------------------------------------------------------- API
static int prep(const char * s, int len, const mi_metrics & m) {
  S = s; L = s && len > 0 ? mn(len, 30000) : 0; M = &m;
  int r = parse();
  rd = 0;
  if (r) measure(r, 0);
  return r;
}
static void run(int r, int mode, int target) { // one place pass
  md = mode; tgt = target; pp = -1; ckn = rd = cp = 0; bl = -1;
  P(0, 0, 0, 0, 0);
  if (r) place(r, 0, 0, 0, 0);
  if (pp >= 0) deliver(pp, px, py, ph, pc);
}
static int clampc(int c) { return c < 0 ? 0 : c > L ? L : c; }

void mi_build(const char * s, int len, int caret, const mi_metrics & m, mi_layout & out) {
  int r = prep(s, len, m);
  out.ops.clear();
  out.ops.reserve(nops + 1);
  OUT = &out; lx = ly = lh = 0;
  run(r, MD_LOC, caret < 0 ? -1 : clampc(caret));
  OUT = 0;
  out.width = r ? T[r].w : 0;
  out.asc = mx(m.big.asc, r ? T[r].as : 0);
  out.desc = mx(m.big.desc, r ? T[r].ds : 0);
  out.cx = lx; out.cy = ly; out.ch = lh;
}

int mi_move(const char * s, int len, int caret, int dir, const mi_metrics & m) {
  int r = prep(s, len, m), c = clampc(caret);
  lx = ly = lh = tkn = 0;
  run(r, MD_LOC, c); // location (and enclosing constructs) of the caret
  if (dir < 0 || dir > 3) return c;
  res = bk[4] = -1; rx = -30000;
  run(r, MD_LEFT + dir, c);
  if (dir < 2) return res < 0 ? c : res;
  return bk[4];
}

int mi_backspace(const char * s, int len, int caret, int & from, int & to, const mi_metrics & m) {
  int r = prep(s, len, m), c = clampc(caret), i = c - 1, n = r, k, nx, j, q, lv = 0;
  from = to = c;
  if (c <= 0 || !r) return c;
  for (;;) { // innermost node owning char i
    k = ++lv > RMAX ? (int)K_FLAT : (int)T[n].k; nx = 0; // same depth rule as measure/place
    if (k < K_EMPTY) break;
    if (k <= K_CALL) {
      for (q = T[n].kid; q; q = T[q].nx)
        if (T[q].a <= i && i < T[q].b) nx = q;
    } else {
      if (k == K_POW && i < T[T[n].kid].b) nx = T[n].kid;
      for (j = 0; (q = slot(n, j)); ++j)
        if (T[q].a <= i && i < T[q].b) nx = q;
      if (!nx) { // hidden structure char of construct n
        for (j = 0; (q = slot(n, j)); ++j) // the default variable x (k for sum) counts as empty
          if (T[q].k != K_EMPTY && !(j == 1 && k >= K_INT && k <= K_LIM && T[q].k == K_TEXT &&
                                       namei(q, k == K_SUM ? "k" : "x")))
            return mi_move(s, len, c, 0, m); // a slot is not empty: act like LEFT
        j = k == K_POW ? T[T[n].kid].b : T[n].a; // empty template: delete it whole
        while (j < L && blank(ch(j))) ++j;
        from = j; to = T[n].b;
        return j;
      }
    }
    if (!nx) break; // delimiter or blank of a visible group/call
    n = nx;
  }
  // visible char: delete one glyph
  from = i; to = c;
  if (k == K_PI || k == K_INF) { if (i >= T[n].c) { from = T[n].c; to = T[n].b; } }
  else {
    while (from > T[n].a && !gstart(from)) --from;
    while (to < T[n].b && !gstart(to)) ++to;
  }
  return from;
}

static const mi_template tpl[MI_T_COUNT] = {
  {"()/()", 1}, {"/()", 2}, {"^()", 2}, {"^2", 2}, {"sqrt()", 5}, {"surd(,)", 5}, {"abs()", 4},
  {"integrate(,x)", 10}, {"integrate(,x,,)", 10}, {"diff(,x)", 5}, {"sum(,k,,)", 4},
  {"limit(,x,)", 6}, {"e^()", 3}};

const mi_template & mi_get_template(int kind) {
  return tpl[kind >= 0 && kind < MI_T_COUNT ? kind : 0];
}

const mi_metrics mi_default_metrics = {{8, 14, 4}, {6, 9, 3}, 2};
