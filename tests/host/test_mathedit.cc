// test_mathedit.cc - host tests for mathedit.cc (structure-aware editing keys).
// Build and run with run_mathedit_tests.sh. Optional arguments: fuzz seed, fuzz steps.
//
// A case is a buffer with | for the caret, a key script and the expected buffer and caret.
// Script letters: S = ME_SQUARE, R = ME_RECIP, B = backspace (me_backspace); any other char
// is that key. When me_key returns 0 the test does what the caller does: it inserts the key's
// text at *caret ("^2" and "^-1" for S and R) and moves the caret after it. When
// me_backspace returns 0 the buffer is left as is (the caller's own deletion is not shown).
// rc is the expected return code of the last key (-1: not checked); cap the buffer capacity
// (0: 256).
#include "mathedit.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

struct tcase { const char * before; const char * keys; const char * after; int rc; int cap; };

static const tcase cases[] = {
  // ---- the work order's examples
  // rule 1: '/'
  {"x|", "/", "(x)/(|)", 1, 0},
  {"2x|", "/", "(2x)/(|)", 1, 0},
  {"a+b|", "/", "a+(b)/(|)", 1, 0},
  {"(a+b)|", "/", "(a+b)/(|)", 1, 0},
  {"|", "/", "(|)/()", 1, 0},
  {"a+|", "/", "a+(|)/()", 1, 0},
  {"x^(2)|", "/", "(x^(2))/(|)", 1, 0},
  {"2sin(x)|", "/", "(2sin(x))/(|)", 1, 0},
  {"x*y|", "/", "x*(y)/(|)", 1, 0},
  {"-x|", "/", "-(x)/(|)", 1, 0},
  {"integrate(x|,x)", "/", "integrate((x)/(|),x)", 1, 0},
  // rule 2: ',' '=' '<' '>' (rc 0: the caller inserted the key at the moved caret)
  {"(a)/(b|)", ",", "(a)/(b),|", 0, 0},
  {"x^(2|)", "=", "x^(2)=|", 0, 0},
  {"integrate((x)/(1+x|),x)", ",", "integrate((x)/(1+x),|x)", 1, 0},
  {"(1)/(x^(2|))", ",", "(1)/(x^(2)),|", 0, 0},
  // rule 3: '('
  {"2^(|)", "(", "2^(|)", 1, 0},
  {"x+|", "(", "x+(|)", 1, 0},
  {"|x+1", "(", "(|x+1)", 1, 0},
  {"(1)/(|x^(8))", "(", "(1)/((|x^(8)))", 1, 0},
  {"x^(|2)", "(", "x^((|2))", 1, 0},
  {"sin|x", "(", "sin(|x)", 1, 0},
  {"integrate(|x,x)", "(", "integrate((|x),x)", 1, 0},
  // rule 4: ')'
  {"(x+1|)", ")", "(x+1)|", 1, 0},
  {"integrate(x+1|,x)", ")", "integrate((x+1)|,x)", 1, 0},
  {"x+1|", ")", "(x+1)|", 1, 0},
  {"(|", ")", "(|", 1, 0},
  // rule 5: ME_SQUARE
  {"x|", "S", "x^(2)|", 1, 0},
  // rule 6: backspace
  {"integrate(|x^(2),x)", "B", "|x^(2)", 1, 0},
  {"sqrt(|)", "B", "|", 1, 0},
  {"f(a,|b)", "B", "f(a|,b)", 1, 0},
  {"(|a)/()", "B", "|a", 1, 0},
  {"(|)/()", "B", "|", 1, 0},
  {"(|a)/(b)", "B", "|(a)/(b)", 1, 0},
  {"(a)/(|)", "B", "a|", 1, 0},
  {"(a)/(|b)", "B", "(a|)/(b)", 1, 0},
  {"x^(|)", "B", "x|", 1, 0},
  {"x^(|2)", "B", "x|^(2)", 1, 0},
  {"(|a+b)", "B", "|a+b", 1, 0},

  // ---- more '/'
  {"integrate((1)/(x^(2|)),x)", "/", "integrate((1)/(x^((2)/(|))),x)", 1, 0}, // in an exponent in a denominator in an integral
  {"(a)/(b|)", "/", "(a)/((b)/(|))", 1, 0},
  {"x^2|", "/", "(x^2)/(|)", 1, 0},
  {"x^n|", "/", "(x^n)/(|)", 1, 0},
  {"x!|", "/", "(x!)/(|)", 1, 0},
  {"f'(x)|", "/", "(f'(x))/(|)", 1, 0},
  {"1.5|", "/", "(1.5)/(|)", 1, 0},
  {"sin(x)^(2)|", "/", "(sin(x)^(2))/(|)", 1, 0},
  {"2(x+1)|", "/", "(2(x+1))/(|)", 1, 0},
  {"(a)(b)|", "/", "((a)(b))/(|)", 1, 0},
  {"[1,2]|", "/", "([1,2])/(|)", 1, 0},
  {"{a}|", "/", "({a})/(|)", 1, 0},
  {"a=b|", "/", "a=(b)/(|)", 1, 0},
  {"a,b|", "/", "a,(b)/(|)", 1, 0},
  {"a;b|", "/", "a;(b)/(|)", 1, 0},
  {"a b|", "/", "a (b)/(|)", 1, 0},
  {"x|+1", "/", "(x)/(|)+1", 1, 0},
  {"|x", "/", "(|)/()x", 1, 0},
  {"x^|", "/", "x^(|)/()", 1, 0},
  {"f(|)", "/", "f((|)/())", 1, 0},
  {"\"ab\"+x|", "/", "\"ab\"+(x)/(|)", 1, 0},
  {"\"ab\"|", "/", "\"ab\"(|)/()", 1, 0},
  {"\"a|b\"", "/", "\"a/|b\"", 0, 0},                // in a string: the caller inserts it
  {"a)b|", "/", "a)(b)/(|)", 1, 0},                   // unbalanced: a stray )
  {"(x|", "/", "((x)/(|)", 1, 0},                     // unbalanced: an unclosed (
  {"x|", "/", "x|", 1, 6},                            // capacity: (x)/() needs 7 bytes
  {"x|", "/", "(x)/(|)", 1, 7},
  {"(a)|", "/", "(a)|", 1, 6},
  {"(a)|", "/", "(a)/(|)", 1, 7},
  {"|", "/", "|", 1, 5},
  {"|", "/", "(|)/()", 1, 6},

  // ---- more ',' '=' '<' '>'
  {"a|", ",", "a,|", 0, 0},
  {"|", ",", ",|", 0, 0},
  {"f(a|)", ",", "f(a,|)", 0, 0},
  {"(1)/(f(x|))", ",", "(1)/(f(x,|))", 0, 0},         // a call in a denominator keeps its commas
  {"(a)/((b|))", ",", "(a)/((b,|))", 0, 0},           // visible parentheses in a denominator
  {"x^(2|)", "<", "x^(2)<|", 0, 0},
  {"x^(2|)", ">", "x^(2)>|", 0, 0},
  {"(a)/(b|)=c", "=", "(a)/(b)=|c", 1, 0},            // steps over the =
  {"(a)/(b|)<c", "<", "(a)/(b)<|<c", 0, 0},           // < is not stepped over
  {"f(x^(2|),y)", ",", "f(x^(2),|y)", 1, 0},
  {"integrate((1)/(x^(2|)),x)", ",", "integrate((1)/(x^(2)),|x)", 1, 0},
  {"e^(x^(2|))", "=", "e^(x^(2))=|", 0, 0},
  {"(a)/(|b)", ">", "(a)/(b)>|", 0, 0},
  {"x/(b|)", ",", "x/(b),|", 0, 0},                   // a denominator without a numerator group
  {"x^(a|", "=", "x^(a=|", 0, 0},                     // unclosed exponent: stays
  {"[1|]", ",", "[1,|]", 0, 0},
  {"(\"a,b\"|)", ",", "(\"a,b\",|)", 0, 0},
  {"\"(a)/(b|)\"", ",", "\"(a)/(b,|)\"", 0, 0},       // in a string
  {"x|", "+", "x+|", 0, 0},                           // keys that are not handled
  {"x|", "x", "xx|", 0, 0},

  // ---- more '('
  {"|", "(", "(|)", 1, 0},
  {"x|", "(", "x(|)", 1, 0},
  {"f(a|,b)", "(", "f(a(|),b)", 1, 0},
  {"f(|a,b)", "(", "f((|a),b)", 1, 0},
  {"[|1,2]", "(", "[(|1),2]", 1, 0},
  {"{|a}", "(", "{(|a)}", 1, 0},
  {"|a;b", "(", "(|a);b", 1, 0},
  {"|x\"a)\"y", "(", "(|x\"a)\"y)", 1, 0},           // the ) in the string does not end the slot
  {"\"(|\"", "(", "\"((|\"", 0, 0},
  {"|a)b", "(", "(|a))b", 1, 0},                     // unbalanced: stops at the stray )
  {"|a\"b)", "(", "(|a)\"b)", 1, 0},                 // unterminated string: ) goes before it
  {"integrate((1)/(x^(|)),x)", "(", "integrate((1)/(x^(|)),x)", 1, 0},
  {"|x", "(", "(|x)", 1, 4},
  {"|x", "(", "|x", 1, 3},

  // ---- more ')'
  {"|", ")", "|", 1, 0},
  {"f(|)", ")", "f()|", 1, 0},
  {"[1,2|]", ")", "[1,2]|", 1, 0},
  {"{a|}", ")", "{a}|", 1, 0},
  {"a,b|", ")", "a,(b)|", 1, 0},
  {"f(a,|b)", ")", "f(a,|b)", 1, 0},                 // nothing left of the caret in the slot
  {"|x+1", ")", "|x+1", 1, 0},
  {"(1)/(x+1|)", ")", "(1)/(x+1)|", 1, 0},
  {"integrate((1)/(x^(2|)),x)", ")", "integrate((1)/(x^(2))|,x)", 1, 0}, // out of both boxes
  {"x^((1)/(2|))", ")", "x^((1)/(2))|", 1, 0},       // x^(1/2) typed: ) closes the ^(
  {"(1)/(1+(1)/(x|))", ")", "(1)/(1+(1)/(x))|", 1, 0}, // 1/(1+1/x) typed
  {"sin(x^(2|))", ")", "sin(x^(2))|", 1, 0},
  {"((a)/(b|))+1", ")", "((a)/(b))|+1", 1, 0},       // a group the user opened: closed, then stop
  {"(1)/(|)", "(", "(1)/((|))", 1, 0},               // /( empty: the user's group, inside the box
  {"x^(|)", "(", "x^(|)", 1, 0},                     // ^( empty: the exponent is the group
  {"(1)/((x+1|))", ")", "(1)/((x+1)|)", 1, 0},       // 1/(x+1) typed: still in the denominator
  {"(1)/((x+1)|)", ")", "(1)/((x+1))|", 1, 0},       // a second ): out of it
  {"(1)/((x+1)|)", "x", "(1)/((x+1)x|)", 0, 0},      // 1/(x+1)x: x in the denominator
  {"(1)/((x+1)|)", "+", "(1)/((x+1))+|", 0, 0},      // 1/(x+1)+2: + leaves the denominator
  {"(1)/((x+1)|)", "-", "(1)/((x+1))-|", 0, 0},
  {"integrate((1)/((x+1)|),x)", "+", "integrate((1)/((x+1))+|,x)", 0, 0},
  {"(1)/(x+1|)", "+", "(1)/(x+1+|)", 0, 0},          // + inside a denominator stays inside
  {"(1)/((x+1)(x|))", "+", "(1)/((x+1)(x+|))", 0, 0},
  {"(1)/((x+1)(x-1)|)", "+", "(1)/((x+1)(x-1)+|)", 0, 0}, // two groups: not one whole group
  {"x+(a|", ")", "x+(a)|", 1, 0},                    // unbalanced: closes the open group
  {"f(a,b|", ")", "f(a,b)|", 1, 0},
  {"f(|", ")", "f(|", 1, 0},
  {"a)b|", ")", "a)(b)|", 1, 0},
  {"\"(\",x|", ")", "\"(\",(x)|", 1, 0},            // the ( in the string opens nothing
  {"\"a|\"", ")", "\"a)|\"", 0, 0},
  {"x+1|", ")", "x+1|", 1, 5},                       // capacity: (x+1) needs 6 bytes
  {"x+1|", ")", "(x+1)|", 1, 6},
  {"(a|", ")", "(a|", 1, 3},
  {"(a|", ")", "(a)|", 1, 4},

  // ---- ME_SQUARE, ME_RECIP
  {"x|", "R", "x^(-1)|", 1, 0},
  {"|", "S", "^(2)|", 1, 0},
  {"(a)/(b|)", "S", "(a)/(b^(2)|)", 1, 0},
  {"\"x|\"", "S", "\"x^2|\"", 0, 0},
  {"\"x|\"", "R", "\"x^-1|\"", 0, 0},
  {"x|", "S", "x|", 1, 5},
  {"x|", "S", "x^(2)|", 1, 6},
  {"x|", "R", "x|", 1, 6},
  {"x|", "R", "x^(-1)|", 1, 7},

  // ---- more backspace
  {"|", "B", "|", 0, 0},
  {"|x", "B", "|x", 0, 0},
  {"x|", "B", "x|", 0, 0},
  {"(a)|", "B", "(a)|", 0, 0},
  {"a,|b", "B", "a,|b", 0, 0},                       // top-level comma: the caller deletes it
  {"f(|", "B", "f(|", 0, 0},                         // unclosed group: the caller's deletion
  {"\"(|\"", "B", "\"(|\"", 0, 0},
  {"\"a\\\"(|\"", "B", "\"a\\\"(|\"", 0, 0},         // after an escaped quote: still in the string
  {"[|1,2]", "B", "|1,2", 1, 0},
  {"{|}", "B", "|", 1, 0},
  {"[1,|2]", "B", "[1|,2]", 1, 0},
  {"[[1,2],[|3,4]]", "B", "[[1,2],|3,4]", 1, 0},
  {"f(\"(\",|x)", "B", "f(\"(\"|,x)", 1, 0},
  {"\"a\"(|x)", "B", "\"a\"|x", 1, 0},
  {"integrate(|x+1,x,0,1)", "B", "|x+1", 1, 0},
  {"sum(|k,k,1,n)", "B", "|k", 1, 0},
  {"abs(|)", "B", "|", 1, 0},
  {"x+abs(|)", "B", "x+|", 1, 0},
  {"y=sin(|x+1)", "B", "y=|x+1", 1, 0},
  {"2sqrt(|x+1)", "B", "2(|x+1)", 1, 0},             // brackets kept: 2x+1 would be wrong
  {"2sin(|x)", "B", "2(|x)", 1, 0},
  {"x^sqrt(|y)", "B", "x^(|y)", 1, 0},
  {"1/sqrt(|y)", "B", "1/(|y)", 1, 0},
  {"sqrt(|x)/(2)", "B", "|x/(2)", 1, 0},
  {"sqrt(|x+1)/(2)", "B", "(|x+1)/(2)", 1, 0},
  {"-(x)/(|)", "B", "-x|", 1, 0},
  {"-(a+b)/(|)", "B", "-(a+b)|", 1, 0},
  {"(a+b)/(|)*c", "B", "(a+b)|*c", 1, 0},
  {"(2x)/(|)^2", "B", "(2x)|^2", 1, 0},
  {"(x)/(|)^2", "B", "(x)|^2", 1, 0},                // before ^ ! ' brackets always stay
  {"x/(|)", "B", "x|", 1, 0},
  {"x/(|b)", "B", "x|/(b)", 1, 0},
  {"f(x)/(|)", "B", "f(x)|", 1, 0},
  {"f(x)/(|b)", "B", "f(x|)/(b)", 1, 0},
  {"x^(2)/(|)", "B", "x^(2)|", 1, 0},
  {"(a)/(b)/(|)", "B", "(a)/(b)|", 1, 0},
  {"(|a)/()+1", "B", "|a+1", 1, 0},
  {"(|a+b)/()*c", "B", "(|a+b)*c", 1, 0},
  {"2(|a)/()", "B", "2(|a)", 1, 0},
  {"(|a)/x", "B", "|(a)/x", 1, 0},
  {"(|)/(b)", "B", "|()/(b)", 1, 0},
  {"f(|a)/(b)", "B", "|a/(b)", 1, 0},
  {"(1)/(x^(|))", "B", "(1)/(x|)", 1, 0},
  {"integrate((1)/(x^(|2)),x)", "B", "integrate((1)/(x|^(2)),x)", 1, 0},
  {"integrate((1)/(|x),x)", "B", "integrate((1|)/(x),x)", 1, 0},
  {"f((|a,b))", "B", "f(|a,b)", 1, 0},

  // ---- key scripts: typing, and round trips
  {"|", "x/x+1", "(x)/(x+1|)", 0, 0},                // x/x+1: the + stays in the denominator
  {"|", "x/1+x,", "(x)/(1+x),|", 0, 0},
  {"integrate((1)/(x|),x)", "S,", "integrate((1)/(x^(2)),|x)", 1, 0},
  {"2x|", "/B", "2x|", 1, 0},
  {"a+b|", "/B", "a+b|", 1, 0},
  {"-x|", "/B", "-x|", 1, 0},
  {"a+|", "/B", "a+|", 1, 0},
  {"(a+b)|*c", "/B", "(a+b)|*c", 1, 0},
  {"(a+b)|", "/B", "a+b|", 1, 0},                    // a reused group's brackets go (top level)
  {"|x+1", "(B", "|x+1", 1, 0},
  {"x+1|", ")B", "(x+1)|", 0, 0},                    // right after ): left to the caller
  {"integrate(|x,x)", "B", "|x", 1, 0},
};

// ---------------------------------------------------------------- case runner
static int g_cases, g_case_fails;

static const char * keytext(int key) { return key == ME_SQUARE ? "^2" : key == ME_RECIP ? "^-1" : 0; }
static int keyof(char c) { return c == 'S' ? ME_SQUARE : c == 'R' ? ME_RECIP : c == 'B' ? -1 : c; }

// apply one key the way the console does; returns the function's return code
static int apply(char * s, int cap, int * caret, int key) {
  if (key < 0) return me_backspace(s, caret);
  int rc = me_key(s, cap, caret, key);
  if (!rc) {
    char one[2] = {(char)key, 0};
    const char * t = keytext(key) ? keytext(key) : one;
    int n = (int)strlen(s), k = (int)strlen(t);
    if (n + k < cap) {
      memmove(s + *caret + k, s + *caret, n - *caret + 1);
      memcpy(s + *caret, t, k);
      *caret += k;
    }
  }
  return rc;
}

static void run_case(const tcase & c) {
  std::string b = c.before;
  size_t bar = b.find('|');
  b.erase(bar, 1);
  int cap = c.cap ? c.cap : 256, caret = (int)bar, rc = -2;
  std::vector<char> mem(cap + 16, 'Z');
  memcpy(&mem[0], b.c_str(), b.size() + 1);
  for (const char * k = c.keys; *k; ++k) rc = apply(&mem[0], cap, &caret, keyof(*k));
  bool guard = true;
  for (int i = cap; i < cap + 16; ++i) guard = guard && mem[i] == 'Z';
  std::string got(&mem[0]);
  bool ok = guard && caret >= 0 && caret <= (int)got.size();
  if (ok) got.insert(caret, "|");
  ok = ok && got == c.after && (c.rc < 0 || rc == c.rc);
  ++g_cases;
  if (!ok) ++g_case_fails;
  printf("%s %-28s %-6s -> %s", ok ? "PASS" : "FAIL", c.before, c.keys, got.c_str());
  if (c.cap) printf("  (cap %d)", c.cap);
  if (!ok) printf("   expected %s rc %d, got rc %d%s", c.after, c.rc, rc, guard ? "" : ", WROTE PAST CAP");
  printf("\n");
}

// two calls with a caret outside [0, strlen]: nothing happens
static void run_bad_carets() {
  char s[16] = "x+1";
  int ok = 1;
  for (int k = 0; k < 2; ++k) {
    int caret = k ? 4 : -1;
    ok &= me_key(s, 16, &caret, '/') == 0 && me_backspace(s, &caret) == 0 && caret == (k ? 4 : -1);
  }
  ok &= !strcmp(s, "x+1");
  ++g_cases;
  if (!ok) ++g_case_fails;
  printf("%s caret outside the buffer: no change, rc 0\n", ok ? "PASS" : "FAIL");
}

// ---------------------------------------------------------------- property fuzz
struct rng {
  unsigned long long x;
  unsigned next(unsigned n) {
    x ^= x << 13; x ^= x >> 7; x ^= x << 17;
    return (unsigned)(x % n);
  }
};

static const char * atoms[] = {
  "x", "y", "2", "13", "ab", "1.5", "pi", "2x", "e", "sin", "+", "-", "*", "/", "=", "<", ">", ",",
  ";", "!", "^", "^2", "'", ".", " ", ":=", "\"s(t]r\"", "\"q\\\"(e\"", "\"a,b\"", "\"\"", "\"\\\\\"",
};
static const char * tpls[] = { // # = a random sub-buffer
  "(#)/(#)", "#^(#)", "^(#)", "^(2)", "^(-1)", "sqrt(#)", "integrate(#,x)", "integrate(#,x,#,#)",
  "sum(#,k,#,#)", "limit(#,x,#)", "diff(#,x)", "abs(#)", "surd(#,#)", "f(#,#)", "[#,#]", "{#}",
  "(#)", "[[#,#],[#,#]]", "#/(#)", "(#,#)", "e^(#)", "2sin(#)", "(#)/()", "()/(#)",
};
enum { NATOMS = sizeof atoms / sizeof *atoms, NTPLS = sizeof tpls / sizeof *tpls };

static std::string gen(rng & r, int depth) {
  std::string out;
  int k = (int)r.next(depth > 0 ? 5 : 3);
  for (int i = 0; i < k; ++i) {
    if (depth > 0 && r.next(3) == 0) {
      for (const char * t = tpls[r.next(NTPLS)]; *t; ++t)
        if (*t == '#') out += gen(r, depth - 1);
        else out += *t;
    } else out += atoms[r.next(NATOMS)];
  }
  return out;
}

// balanced outside string literals, brackets matched by kind, every string literal closed
static bool balanced(const char * s) {
  std::string st;
  for (int i = 0; s[i]; ++i) {
    char c = s[i];
    if (c == '"') {
      for (++i; s[i] && s[i] != '"'; ++i)
        if (s[i] == '\\' && s[i + 1]) ++i;
      if (!s[i]) return false;
    } else if (c == '(' || c == '[' || c == '{') st += c;
    else if (c == ')' || c == ']' || c == '}') {
      if (st.empty() || st[st.size() - 1] != (c == ')' ? '(' : c == ']' ? '[' : '{')) return false;
      st.erase(st.size() - 1);
    }
  }
  return st.empty();
}

// the string literals of s, in order (an unterminated one runs to the end)
static std::vector<std::string> literals(const char * s) {
  std::vector<std::string> v;
  for (int i = 0; s[i]; ++i)
    if (s[i] == '"') {
      int a = i;
      for (++i; s[i] && s[i] != '"'; ++i)
        if (s[i] == '\\' && s[i + 1]) ++i;
      v.push_back(std::string(s + a, s[i] ? i + 1 - a : i - a));
      if (!s[i]) break;
    }
  return v;
}

template <class T> static bool subseq(const T & a, const T & b) { // a is a subsequence of b
  size_t j = 0;
  for (size_t i = 0; i < b.size() && j < a.size(); ++i)
    if (a[j] == b[i]) ++j;
  return j == a.size();
}

static long g_fuzz_steps, g_fuzz_fails, g_handled, g_balance_checks;

#ifdef __SANITIZE_ADDRESS__
enum { GUARD = 0 };  // exact-size buffers: ASan reports any access past them
#else
enum { GUARD = 16 }; // no ASan: canary bytes after the buffer catch writes
#endif

static void fuzz_fail(const char * what, const std::string & before, int caret, int key, const char * after) {
  if (++g_fuzz_fails <= 20)
    printf("    fuzz: %s: \"%s\" caret %d key %d -> \"%s\"\n", what, before.c_str(), caret, key, after);
}

static void fuzz(unsigned long long seed, long steps) {
  static const int keys[] = {'/', '(', ')', ',', '=', '<', '>', ME_SQUARE, ME_RECIP, -1, -1, -1, 'x', '+', '1'};
  rng r = {seed * 2654435761ULL + 88172645463325252ULL};
  while (g_fuzz_steps < steps) {
    std::string init = gen(r, 3);
    if (init.size() > 300) continue;
    int cap = r.next(4) ? (int)init.size() + 1 + (int)r.next(14) : 512;
    std::vector<char> mem(cap + 16, 'Z');
    char * s = &mem[0];
    memcpy(s, init.c_str(), init.size() + 1);
    int caret = (int)r.next((unsigned)init.size() + 1);
    bool tidy = r.next(2); // the caller's own Backspace never deletes a bracket, quote or backslash
    for (int step = 0; step < 24 && g_fuzz_steps < steps; ++step, ++g_fuzz_steps) {
      int n = (int)strlen(s);
      if (!r.next(3)) caret = (int)r.next(n + 1);
      int key = keys[r.next(sizeof keys / sizeof *keys)];
      std::string before(s);
      std::vector<std::string> lit0 = literals(s);
      bool bal0 = balanced(s);
      int caret0 = caret, rc;
      // the call works on an exact-size heap copy, so ASan sees any access past the buffer
      // (past the terminator for Backspace, which never grows the text; past cap for keys)
      int size = key < 0 ? n + 1 : cap;
      char * w = (char *)malloc(size + GUARD);
      memcpy(w, s, n + 1);
      memset(w + size, 'Z', GUARD);
      if (key < 0) rc = me_backspace(w, &caret);
      else rc = me_key(w, cap, &caret, key);
      for (int g = 0; g < GUARD; ++g)
        if (w[size + g] != 'Z') { fuzz_fail("wrote past the buffer", before, caret0, key, ""); break; }
      if (!memchr(w, 0, size)) { fuzz_fail("not terminated in the buffer", before, caret0, key, ""); w[size - 1] = 0; }
      memcpy(s, w, strlen(w) + 1);
      free(w);
      g_handled += rc;
      if (rc && strcmp(s, before.c_str())) { // text changed: kept everything / only removed
        if (key < 0 ? !subseq(std::string(s), before) : !subseq(before, std::string(s)))
          fuzz_fail("not an insertion/removal", before, caret0, key, s);
        if (key < 0 ? !subseq(literals(s), lit0) : literals(s) != lit0)
          fuzz_fail("string literal changed", before, caret0, key, s);
      }
      if (!rc && strcmp(s, before.c_str())) fuzz_fail("rc 0 but text changed", before, caret0, key, s);
      if (!rc && key < 0 && caret != caret0) fuzz_fail("rc 0 but caret moved", before, caret0, key, s);
      if (caret < 0 || caret > (int)strlen(s)) { fuzz_fail("caret out of range", before, caret0, key, s); caret = 0; }
      if (bal0) { // checked before the caller's own insertion or deletion below
        ++g_balance_checks;
        if (!balanced(s)) fuzz_fail("unbalanced", before, caret0, key, s);
      }
      if (key >= 0 && !rc) { // the caller inserts the key
        const char * t = keytext(key);
        char one[2] = {(char)key, 0};
        if (!t) t = one;
        int m = (int)strlen(s), k = (int)strlen(t);
        if (m + k < cap) {
          memmove(s + caret + k, s + caret, m - caret + 1);
          memcpy(s + caret, t, k);
          caret += k;
        }
      }
      if (key < 0 && !rc && caret > 0) { // the caller's own deletion of one char
        int i = caret - 1;
        char c = s[i];
        if (tidy && (strchr("()[]{}\"\\", c) || (i > 0 && s[i - 1] == '\\'))) --caret;
        else {
          memmove(s + i, s + i + 1, strlen(s + i + 1) + 1);
          caret = i;
        }
      }
      for (int i = cap; i < cap + 16; ++i)
        if (mem[i] != 'Z') { fuzz_fail("wrote past cap", before, caret0, key, s); mem[i] = 'Z'; }
      if ((int)strlen(s) >= cap) fuzz_fail("text longer than cap", before, caret0, key, s);
    }
  }
}

int main(int argc, char ** argv) {
  unsigned long long seed = argc > 1 ? strtoull(argv[1], 0, 10) : 1;
  long steps = argc > 2 ? strtol(argv[2], 0, 10) : 200000;
  for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) run_case(cases[i]);
  run_bad_carets();
  fuzz(seed, steps);
  printf("%s fuzz: %ld steps (seed %llu), %ld handled, %ld balance checks, %ld failures\n",
         g_fuzz_fails ? "FAIL" : "PASS", g_fuzz_steps, seed, g_handled, g_balance_checks, g_fuzz_fails);
  printf("summary: %d cases, %d failed; fuzz %ld steps, %ld failures\n", g_cases, g_case_fails,
         g_fuzz_steps, g_fuzz_fails);
  return g_case_fails || g_fuzz_fails ? 1 : 0;
}
