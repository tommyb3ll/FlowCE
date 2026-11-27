// Host test for the steps screen's gate (src/focus_steps.cc: focus_steps_able, and so alg_able).
// Linked against the host giac objects (focus_steps.cc needs giac and the UI, so it is not
// included here): the table cases.txt is "line<TAB>0|1", 1: the steps screen is offered.
// Usage: test_able cases.txt [-p]   (-p: print "line<TAB>result" for every line, the table itself)
#include <stdio.h>
#include <string.h>
namespace giac { class context; }
giac::context * contextptr = 0; // the globals the objects expect (driver.cc has them, and the main)
double hg_now_ms() { return 0; }
int focus_steps_able(const char * s, const char * ans = 0); // focus.h
int main(int argc, char ** argv) {
  FILE * f = argc > 1 ? fopen(argv[1], "r") : 0;
  if (!f) { fprintf(stderr, "usage: test_able cases.txt [-p]\n"); return 2; }
  const bool pr = argc > 2 && !strcmp(argv[2], "-p");
  char b[512];
  int n = 0, bad = 0;
  while (fgets(b, sizeof b, f)) {
    b[strcspn(b, "\r\n")] = 0;
    if (!*b || *b == '#') continue;
    char * t = strrchr(b, '\t');
    if (!t) { printf("BAD TABLE LINE: %s\n", b); return 2; }
    *t++ = 0;
    const int got = focus_steps_able(b) != 0;
    if (pr) { printf("%s\t%d\n", b, got); continue; }
    ++n;
    if (got == (*t == '1')) printf("PASS %s -> %d\n", b, got);
    else { ++bad; printf("FAIL %s -> %d (expected %c)\n", b, got, *t); }
  }
  if (!pr) printf("%d cases, %d passed, %d failed\n", n, n - bad, bad);
  return bad != 0;
}
