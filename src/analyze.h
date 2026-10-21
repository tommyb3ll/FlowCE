// analyze.h - a function's analysis (DeltaCE, the steps build): domain, intercepts, symmetry,
// asymptotes and holes, where it increases and decreases, its extrema, concavity and inflection
// points, as a textbook's curve sketching does it: the points where something can change (the
// domain's edges, the zeros of f' and f''), then a test point between each two of them.
#ifndef ANALYZE_H
#define ANALYZE_H
#include <giac/giacPCH.h>

// A line of the report: a section (text: its title), or a row: its label (text) and its math, the
// items (giac syntax; text after \1) separated by '\n', drawn joined by sep (", ", " or ", " ");
// no math: the label alone, as a sentence.
enum { AN_SECTION, AN_ROW, AN_HEAD };
enum { AN_COMMA, AN_OR, AN_SPACE };
struct an_line { unsigned char kind, sep; const char * text; std::string math; };

// The analysis of f (parsed, not evaluated: as typed, its holes kept) in its variable. 0: done,
// else why not.
const char * analyze_of(const giac::gen & f, std::vector<an_line> & out, const giac::context * contextptr);

// The graph's key points (focus_graph.cc): the analysis' points, exact; kind 0 zero, 1 y-intercept,
// 2 local max, 3 local min, 4 inflection point. per: the period (0: none; the points are then the
// period's, from 0), else 0. 0: done, else why not.
struct an_pt { unsigned char kind; giac::gen x, y; };
const char * analyze_points(const giac::gen & f, std::vector<an_pt> & pts, giac::gen & per, const giac::context * contextptr);
// The real x where g is 0, exact when giac finds them (and with edges, the edges of g's domain:
// where a derivative is undefined); on [lo, hi] when lo < hi (sin, cos, tan: their families there,
// x mixed with them numerically), else all. Sorted, at most 24. For the graph's tools.
void analyze_zeros(const giac::gen & g, const giac::gen & x, double lo, double hi, bool edges, giac::vecteur & out, const giac::context * contextptr);
#endif
