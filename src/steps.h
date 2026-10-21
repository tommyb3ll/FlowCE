// steps.h - step-by-step solutions (the steps build, FlowCE.b84; tools/hostgiac -S): textbook
// rules for derivatives, integrals, limits, sums and Taylor series. giac computes every result;
// the walk decides which rule explains it, and a chain is shown only once checked.
#ifndef STEPS_H
#define STEPS_H
#include <giac/giacPCH.h>

// A step: its rule (steps_title, steps_math), its depth (a part of the step above it) and the
// parts its math line shows, in the variable v.
struct step_t { unsigned char rule, depth, rel; giac::gen a, b, c, v; }; // (rel: an inequality's relation, steps.cc)

// The steps of input (parsed, not evaluated): diff, integrate, limit, sum or series, or an
// expression containing them. false: none (the chain could not be explained, or did not check).
bool steps_of(const giac::gen & input, std::vector<step_t> & out, const giac::context * contextptr);

// The rule's title ("" for a step that continues the one above) and the formula it applies
// (0 for none); the step's math line (its rule's template filled with its parts).
const char * steps_title(const step_t & s);
const char * steps_formula(const step_t & s);
giac::gen steps_math(const step_t & s, const giac::context * contextptr);
// input as the steps write it (- and / as + and *: 1-x^2, not 1-(x^2) as parsed)
giac::gen steps_problem(const giac::gen & input, const giac::context * contextptr);
// (DeltaCE) the answer the steps end with when it is not the console's (a division typed with a
// remainder: quotient plus remainder over the divisor, not the fraction as typed)
bool steps_answer(const std::vector<step_t> & v, giac::gen & r);
// (DeltaCE) a problem with more than one method (a division: long division, synthetic division,
// factor and cancel): steps_of takes method steps_method (0: the usual one, counted modulo theirs)
// and sets steps_methods (their number, 0 or 1: one way) and steps_method_name (the one taken)
extern int steps_method, steps_methods;
extern const char * steps_method_name;
// (DeltaCE) a step drawn as a table (synthetic division): its cells, a tab between cells, a newline
// between rows, \1 before a cell new in this step; "" for a step drawn as math
std::string steps_table(const step_t & s, const giac::context * contextptr);
#endif
