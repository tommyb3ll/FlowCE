// mathinput.h - 2D (MathPrint style) display of a 1D giac input buffer.
// Pure functions of (buffer, caret, metrics); see mathinput.cc for the rules.
// Not reentrant (one static node pool); never fails, never reads outside s[0,len).
#ifndef MATHINPUT_H
#define MATHINPUT_H

#include <vector>
#if defined(TICE) && !defined(std)
#define std ustl
#endif

// fixed advance width; ascent above / descent below the baseline (pixels)
struct mi_font { short adv, asc, desc; };
// small: exponents, bounds, indices. opgap: space each side of binary + - = < > etc.
struct mi_metrics { mi_font big, small; short opgap; };

// display list op codes
enum { MI_TEXT, MI_DOT, MI_HLINE, MI_SQRT, MI_INTEGRAL, MI_SIGMA, MI_LPAREN, MI_RPAREN,
       MI_LBRACKET, MI_RBRACKET, MI_LBRACE, MI_RBRACE, MI_BAR, MI_BOX };

// Display list op; coordinates in pixels, y grows DOWN, y=0 is the baseline of the
// top-level line, x=0 its left edge.
//  MI_TEXT: x, y=baseline, w=text width, h=font height; chars [pos,pos+len) of the buffer,
//           or the static string lit when lit!=0 ("pi"/"oo" = one special glyph each,
//           "\x1e" = arrow glyph). Each glyph is adv wide.
//  others:  bounding box (x,y top-left, w, h).
//   MI_DOT: draw a dot centered in the box.     MI_HLINE: fill the box (fraction bar).
//   MI_SQRT: radical sign in the left 6 px (full height), overbar on the top row up to x+w.
//   MI_INTEGRAL / MI_SIGMA: the sign scaled to the box.  MI_BAR: vertical line, box center.
//   MI_LPAREN..MI_RBRACE: delimiter scaled to the box.   MI_BOX: empty-slot placeholder.
//  pos: buffer position the op belongs to (-1 for literals), informative except for MI_TEXT.
struct mi_op {
  unsigned char code, small; // small: 1 if in the small font
  short x, y, w, h;
  short pos, len;
  const char * lit;
};

struct mi_layout {
  std::vector<mi_op> ops;
  short width, asc, desc;    // extent of the whole layout around the baseline
  short cx, cy, ch;          // caret: x, top y, height (valid if the caret argument was >= 0)
};

// Lay out s[0,len) (need not be 0-terminated). caret -1: none.
void mi_build(const char * s, int len, int caret, const mi_metrics & m, mi_layout & out);
// dir 0 left, 1 right, 2 up, 3 down; returns the new caret, or -1 when up/down leaves the
// expression (the console then moves into the history), or the same caret when left/right
// hits an end.
int mi_move(const char * s, int len, int caret, int dir, const mi_metrics & m);

struct mi_template { const char * text; short caret; }; // caret offset inside text after insertion
enum { MI_T_FRAC, MI_T_DIV, MI_T_POW, MI_T_SQ, MI_T_SQRT, MI_T_NROOT, MI_T_ABS, MI_T_INT,
       MI_T_DEFINT, MI_T_DIFF, MI_T_SUM, MI_T_LIM, MI_T_EXP, MI_T_COUNT };
const mi_template & mi_get_template(int kind);

extern const mi_metrics mi_default_metrics; // big {8,14,4}, small {6,9,3}, opgap 2

// What Backspace should do: delete [from,to) (from==to: delete nothing) and return the new
// caret. m is only used when Backspace acts like LEFT (hidden structure char).
int mi_backspace(const char * s, int len, int caret, int & from, int & to,
                 const mi_metrics & m = mi_default_metrics);

#endif
