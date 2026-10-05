#ifndef TIINPUT_H
#define TIINPUT_H
/*
  TI-style implicit multiplication, applied to the console line before giac
  parses it. Only bare '*' characters are inserted, everything else (spaces
  included) is kept byte for byte:
    2x -> 2*x   pi(x+1) -> pi*(x+1)   (x+1)(y-1) -> (x+1)*(y-1)
    sin(x)cos(x) -> sin(x)*cos(x)     x y -> x* y
  With split_unknown, unknown all-letter names are cut into known names and
  single letters: xy -> x*y, 2pir -> 2*pi*r, xsin(x) -> x*sin(x).
  The exact rules are at the top of tiinput.cc.
*/
#include <string>
#if defined(TICE) && !defined(std)
#define std ustl
#endif

// name classes returned by the classifier
enum { TI_NAME_UNKNOWN = 0, TI_NAME_VALUE = 1, TI_NAME_FUNCTION = 2, TI_NAME_KEYWORD = 3 };
// classifies name[0..len); name is not 0-terminated at len (it may be a
// substring of a longer name)
typedef int (*ti_classify_fn)(const char * name, int len);
// Returns `line` with '*' inserted where TI-style implicit multiplication applies.
std::string ti_implicit_mult(const std::string & line, ti_classify_fn classify, bool split_unknown);

#endif
