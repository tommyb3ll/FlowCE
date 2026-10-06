// mathedit.cc - structure-aware editing keys for the 2D input line (API: mathedit.h).
//
// The console's input line is giac text that mathinput.cc draws as 2D math: (A)/(B) is a
// stacked fraction, x^(2) a superscript, integrate(f,x) an integral (hidden parentheses and
// commas). The user sees boxes, the keys edit text; these functions make the keys follow the
// boxes. In place (memmove); no allocation, no static data. Scans only run forward (a string
// literal cannot be recognized backward), the context ones from the start of the buffer: a
// few O(n) passes per key.
//
// Words (| is the caret in the examples):
//  group: ( ) [ ] { }. Brackets match by depth whatever their kind (the same pairs as by
//    kind when the buffer is balanced). An unclosed group runs to the end of the buffer; a
//    stray closer at the top level is plain text that starts a new slot.
//  slot: a group's content (or the whole buffer) split at its top-level , and ;. The current
//    slot is the innermost one containing the caret.
//  string literal "..." (\ escapes): skipped as a whole by every scan. With the caret
//    inside one, nothing is handled (return 0); nothing is ever inserted into one.
//  call: a ( right after a name (letters, digits, _, UTF-8 bytes; leading digits are a
//    factor: 2sin( is 2 times sin().
//  denominator: a ( group right after /; exponent: a ( group right after ^; numerator: a
//    ( group right before /, when it is not a call, an exponent or a denominator.
//
// me_key:
//  /    the term left of the caret in the current slot becomes the numerator of a new
//       fraction, the caret goes in its empty denominator. Term: the longest run, ending at
//       the caret, of operands joined by implicit multiplication (names and numbers with
//       their dots, groups, calls), each with ^ exponents (^(..), ^2, ^n) and postfix ! '.
//       x| -> (x)/(|)   2sin(x)| -> (2sin(x))/(|)   a+b| -> a+(b)/(|)   -x| -> -(x)/(|)
//       A term that is one ( group is reused: (a+b)| -> (a+b)/(|). No term: a+| -> a+(|)/().
//  , = < >  with the caret in a denominator or an exponent: the caret moves after that
//       group, again while it is still in one; then a , or = equal to the key right there is
//       stepped over (1), else the caller inserts the key at the new caret (0):
//       (a)/(b|) , -> (a)/(b),|    integrate((x)/(1+x|),x) , -> integrate((x)/(1+x),|x)
//       Elsewhere 0 (the caret does not move).
//  (    right after an empty ^( ) or /( ): nothing (1), the box is the group: 1/(x+1) typed
//       is (1)/(x+1). Else ( at the caret and ) at the end of the current slot:
//       |x+1 -> (|x+1)   x+| -> x+(|)   x^(|2) -> x^((|2)).
//  )    a closer right after the caret is stepped over: (x+1|) -> (x+1)|; after the ) of a
//       denominator or an exponent, the next closer too, as the user sees no bracket there:
//       x^((1)/(2|)) -> x^((1)/(2))|   sin(x^(2|)) -> sin(x^(2))|. Else, when the
//       current slot has text before the caret: if the slot's group is not closed, ) is
//       inserted (it closes it); else that text is wrapped: x+1| -> (x+1)|,
//       integrate(x+1|,x) -> integrate((x+1)|,x). Else nothing (1).
//  ME_SQUARE, ME_RECIP: insert ^(2), ^(-1).
// me_backspace, right after an opening bracket of a closed group:
//  call       f(|a,b)  -> |a        (the name, the brackets and the other arguments go)
//  exponent   x^(|)    -> x|        x^(|2)   -> x|^(2)
//  denom.     (a)/(|)  -> a|        (a)/(|b) -> (a|)/(b)    x/(|b) -> x|/(b)
//  numerator  (|a)/()  -> |a        (|a)/(b) -> |(a)/(b)    (|)/() -> |
//  other      (|a+b)   -> |a+b      [|1,2]   -> |1,2
//  A call's or a numerator's brackets stay when removing them would change the meaning of
//  the text around them (unwrap): 2sqrt(|x+1) -> 2(|x+1), -(a+b)/(|) -> -(a+b)|.
//  Right after a comma of a group: the caret moves before it (f(a,|b) -> f(a|,b)).
//  Anywhere else (top level, unclosed group, string literal): 0.

#include "mathedit.h"
#include <string.h>

typedef unsigned char uc;
#define C(i) ((uc)s[i])

static int opn(uc c) { return c == '(' || c == '[' || c == '{'; }
static int cls(uc c) { return c == ')' || c == ']' || c == '}'; }
static int dig(uc c) { return (uc)(c - '0') < 10; }
static int idc(uc c) { return dig(c) || (uc)((c | 32) - 'a') < 26 || c == '_' || c > 127; }

// index after the item at i (s[i] != 0): a whole string literal, else one char
static int nx(const char * s, int i) {
  if (s[i++] != '"') return i;
  while (s[i] && s[i] != '"') i += (s[i] == '\\' && s[i + 1]) ? 2 : 1;
  return s[i] ? i + 1 : i;
}

// 1 if position p is inside a string literal (an unterminated one runs to the end)
static int instr(const char * s, int p) {
  int q = 0;
  for (int i = 0; i < p; ++i) {
    if (q && s[i] == '\\') ++i;
    else if (s[i] == '"') q = !q;
  }
  return q;
}

// closer matching the opener at i, -1 if the group is not closed
static int mt(const char * s, int i) {
  for (int d = 0; s[i]; i = nx(s, i)) {
    if (opn(C(i))) ++d;
    else if (cls(C(i)) && !--d) return i;
  }
  return -1;
}

// innermost group containing position p (p outside string literals): its opener, -1 at the
// top level. *a = start of the current slot.
static int grp(const char * s, int p, int * a) {
  int D = 0, d = 0, o = -1, i;
  for (i = 0; i < p; i = nx(s, i)) { // depth at p
    if (opn(C(i))) ++D;
    else if (cls(C(i)) && D) --D;
  }
  *a = 0;
  for (i = 0; i < p; i = nx(s, i)) { // the last opener at depth D-1 is the innermost one
    uc c = C(i);
    if (opn(c)) {
      if (d++ == D - 1) *a = (o = i) + 1;
    } else if (cls(c)) {
      if (d) --d;
      else *a = i + 1;               // stray closer (only at the top level)
    } else if (d == D && (c == ',' || c == ';')) *a = i + 1;
  }
  return o;
}

// end of the slot from p: its next top-level , ; or closer, the end of the buffer, or the
// start of an unterminated string literal
static int slend(const char * s, int p) {
  for (int d = 0; s[p]; p = nx(s, p)) {
    uc c = C(p);
    if (opn(c)) ++d;
    else if (cls(c) ? !d-- : !d && (c == ',' || c == ';')) break;
    else if (c == '"' && !s[nx(s, p)] && instr(s, nx(s, p))) break;
  }
  return p;
}

// start of the term that ends at p in the slot starting at a, -1 if none
static int term(const char * s, int a, int p) {
  int t = -1, st = 0, j; // st: 1 after an operand, 2 after its ^
  for (; a < p; a = j) {
    uc c = C(a);
    j = nx(s, a);
    if (opn(c)) {
      j = mt(s, a) + 1; // closed before p (the slot is the innermost one)
      if (j <= a) j = p;
    } else if (!idc(c) && c != '.') {
      st = st == 1 && c == '^' ? 2 : st == 1 && (c == '!' || c == '\'') ? 1 : 0;
      continue;
    }
    if (!st) t = a;
    st = 1;
  }
  return st == 1 ? t : -1;
}

// loosest operator at the top level of s[a,b): 0 none looser than * / ^ (operands, implicit
// multiplication, postfix ! '), 3 + -, 4 anything else (= < >, blanks, a leading sign), 5 , ;
static int lvl(const char * s, int a, int b) {
  int v = 0, w, i, j;
  for (i = a; i < b; i = j > i ? j : b) {
    uc c = C(i);
    j = opn(c) ? mt(s, i) + 1 : nx(s, i);
    w = c == ',' || c == ';' ? 5 : (c == '+' || c == '-') && i > a ? 3
      : idc(c) || opn(c) || strchr("\".*/^!'", c) ? 0 : 4;
    if (w > v) v = w;
  }
  return v;
}

// the loosest lvl() an operand may have without brackets right after the char c (r = 0) or
// right before it (r = 1); -1: keep them (a name or number would join the operand, it is an
// exponent or a denominator, ^ ! ' bind tighter...)
static int room(uc c, int r) {
  const char * k = r ? ",;)]}=<>+-*/" : ",;([{=<>+-*", * q = strchr(k, c); // c = 0: the end
  return q ? (r ? "4444433333224" : "444443333224")[q - k] - '0' : -1;
}

static void del(char * s, int i, int k) { memmove(s + i, s + i + k, strlen(s + i + k) + 1); }

static void ins(char * s, int i, const char * t) {
  int k = strlen(t);
  memmove(s + i + k, s + i, strlen(s + i) + 1);
  memcpy(s + i, t, k);
}

// remove the brackets s[a] and s[e]: always if f, else if their content needs none where it
// is; 1 if removed
static int unwrap(char * s, int a, int e, int f) {
  int l = room(a ? C(a - 1) : 0, 0), r = room(C(e + 1), 1);
  if (!f && lvl(s, a + 1, e) > (l < r ? l : r)) return 0;
  del(s, e, 1);
  del(s, a, 1);
  return 1;
}

int me_key(char * s, int cap, int * caret, int key) {
  int n = strlen(s), p = *caret, a, o, e, t, i, j, d;
  const char * x = "", * y = ""; // the edit: insert x at i, then y at j <= i; the caret moves by d
  if ((unsigned)p > (unsigned)n || instr(s, p)) return 0; // also p < 0
  o = grp(s, p, &a);
  i = j = p;
  if (key == ',' || key == '=' || key == '<' || key == '>') {
    for (t = 0; o > 0 && s[o] == '(' && (s[o - 1] == '/' || s[o - 1] == '^') && (e = mt(s, o)) >= p; t = 1)
      o = grp(s, p = e + 1, &a); // leave the denominator / exponent (p only grows)
    if (!t) return 0;
    t = (key == ',' || key == '=') && C(p) == key; // step over it, else the caller inserts it
    *caret = p + t;
    return t;
  }
  if (key == '/') {
    t = term(s, a, p);
    if (t < 0) x = "()/()", d = 1;
    else if (C(t) == '(' && mt(s, t) == p - 1) x = "/()", d = 2; // one group: reused
    else x = ")/()", y = "(", j = t, d = 4;
  } else if (key == '(') {
    if (p > 1 && s[p - 1] == '(' && (s[p - 2] == '^' || s[p - 2] == '/') && s[p] == ')') return 1; // empty ^() or /(): reused
    i = slend(s, p), x = ")", y = "(", d = 1;
  } else if (key == ')') {
    if (cls(C(p))) { // stepped over; past the ) of a denominator or an exponent (no bracket on
      for (;;) {     // screen) the next closer is too: ) closes what the user opened
        o = grp(s, p, &a);
        ++p;
        if (!(o > 0 && (s[o - 1] == '^' || s[o - 1] == '/') && cls(C(p)))) break;
      }
      *caret = p;
      return 1;
    }
    if (a == p) return 1; // nothing to close or wrap
    x = ")", d = 1;       // closes an unclosed group (unbalanced text), else wraps:
    if (o < 0 || mt(s, o) >= 0) y = "(", j = a, d = 2;
  } else if (key == ME_SQUARE || key == ME_RECIP) {
    x = key == ME_SQUARE ? "^(2)" : "^(-1)";
    d = strlen(x);
  } else return 0;
  if (n + (int)(strlen(x) + strlen(y)) >= cap) return 1;
  ins(s, i, x);
  ins(s, j, y);
  *caret = p + d;
  return 1;
}

int me_backspace(char * s, int * caret) {
  int n = strlen(s), p = *caret, a, o, e, f, b, u = -1, w = 0; // finally unwrap the group at u
  if ((unsigned)p - 1 >= (unsigned)n || instr(s, p) || (o = grp(s, p, &a)) < 0) return 0; // p in [1,n]
  if (s[p - 1] == ',') { // a group's comma: f(a,|b) -> f(a|,b)
    *caret = p - 1;
    return 1;
  }
  if (o != p - 1 || (e = mt(s, o)) < 0) return 0; // only right after an opener of a closed group
  b = o ? s[o - 1] : 0;
  for (f = o; f > 0 && idc(C(f - 1)); --f) {}
  while (f < o && dig(C(f))) ++f; // [f,o): a call's name
  if (s[o] != '(') u = o, w = 1;  // [ { group: its brackets go
  else if (f < o) {               // call: integrate(|x^(2),x) -> |x^(2)
    a = slend(s, p);              // end of the first argument
    del(s, a, e - a);
    del(s, f, o - f);
    u = f, p = f + 1;
  } else if (b == '^' || b == '/') { // exponent, denominator
    if (e > p) p = b == '/' && o > 1 && s[o - 2] == ')' ? o - 2 : o - 1;
    else {
      del(s, p = o - 1, 3);
      if (b == '/' && p && s[p - 1] == ')' && (f = grp(s, p - 1, &a)) >= 0 && s[f] == '(' && mt(s, f) == p - 1)
        u = f; // the numerator
    }
  } else if (s[e + 1] == '/') { // numerator
    if (s[e + 2] == '(' && mt(s, e + 2) == e + 3) del(s, e + 1, 3), u = o;
    else p = o;
  } else u = o, w = 1; // any other group
  if (u >= 0 && unwrap(s, u, e = mt(s, u), w)) p -= (u < p) + (e < p);
  *caret = p;
  return 1;
}
