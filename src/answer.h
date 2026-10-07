// FlowCE's answers: what do_run (main.cc) does around giac's eval. The typed text's pre-pass, the
// parsed input before eval (real odd roots, an equation solved for its unknown, integrals by
// table or numerically, infinite sums of powers), the result after it (the sums and limits giac
// leaves, the normalization, solutions as x=...) and its text in textbook form.
// tools/hostgiac's -k option compiles this same file: the host tests see the calculator's answers.
#ifndef ANSWER_H
#define ANSWER_H
#include <giac/giacPCH.h>

struct answer_ctx {
  giac::gen gin;     // the input before ftc and the integral and sum tables (bounds, sums)
  giac::gen var;     // an equation's unknown (or solve(eq,x)'s x): its solutions show as x=...
  giac::gen vars;    // solve(eqs,[x,y]): x=2, y=1
  giac::gen ivar;    // integrate(f,x): x (an antiderivative: its constant terms go into + C), else 0
  bool real_in;      // no i typed (Focus): odd roots of negative numbers are real
  bool definite;     // integrate(f,x,a,b): undef means it diverges
  bool tabled;       // the answer was known without eval (sec, csc, power sums): g is it
  bool autosimp;     // the result gets auto_simplify (not factor(...), expand(...), programs)
  bool typed_sum, typed_limit; // the text has sum( or limit( (an oscillating result's message)
};

// the typed text, in place (capacity cap): paper notation and TI implicit multiplication; Focus:
// log( is log10(, a sum's index i is another letter
void answer_prepass(char * buf,int cap,bool focus);
// before eval: g parsed from buf (focus: Focus's rules). When a.tabled, g is the answer already.
void answer_before(giac::gen & g,answer_ctx & a,const char * buf,bool focus);
// after eval: msg gets a plain message instead of the result (or stays empty); graw: g before
// positive_first (the long-polynomial check of answer_print)
void answer_after(giac::gen & g,const answer_ctx & a,std::string & msg,giac::gen & graw);
// the text shown for g (not a graph, taille(g) < 256), in textbook form
std::string answer_print(const giac::gen & g,const giac::gen & graw,const answer_ctx & a);

// F4's forms of a result (console_cycle_form, main.cc, cycles them in the history). The order:
//  a number whose decimal value is shown under it: decimal first (at once; simplify of nested
//    trig can take a second), then simplified, one fraction, factored, expanded;
//  an expression with sin, cos or tan: the trig forms first: trig combined (tlin: 2sin(x)cos(x)
//    is sin(2x), sin(x)^2 is 1/2-cos(2x)/2), trig expanded (texpand: sin(2x) is 2sin(x)cos(x)),
//    in terms of sin, of cos (Pythagoras: 1-cos(x)^2 is sin(x)^2), of tan (only when tan alone
//    is left: 2sin(x)cos(x) is 2tan(x)/(tan(x)^2+1)); then simplified (only with trig below a
//    bar, and when shorter: sin(x)/(1+cos(x)) is tan(x/2); elsewhere giac's simplify gives the tan
//    form, slowly, or tan(x/2) messes), one fraction, factored, expanded, decimal;
//  else: simplified, one fraction, factored, expanded, decimal.
// A form is offered when it is new (not shown in the round: texts compared up to the order of
// their terms) and not much larger than the result. Each is computed once per result.
enum { FORM_SIMP, FORM_RAT, FORM_FACT, FORM_EXPA, FORM_TLIN, FORM_TEXP, FORM_TSIN, FORM_TCOS, FORM_TTAN, FORM_DEC };
extern const char * const answer_form_names[]; // "simplified", ... (by kind)
struct answer_forms {
  std::string orig;               // the result's text
  giac::gen g;                    // parsed and evaluated
  bool number, trig, below;
  int size, n;                    // taille(g); the number of forms tried
  const unsigned char * order;    // their kinds, in order
  std::vector<std::string> text;  // form k's text once computed ("": not offered)
  std::vector<giac::gen> form;    // form k
  std::vector<char> done;         // form k computed
  std::vector<std::string> seen;  // the texts shown in this round, the original first
};
// a new result to cycle (number: its decimal value is shown under it)
void answer_forms_start(answer_forms & F,const std::string & orig,bool number);
// the next form to show after position k (0: the original, k: form k-1 of the order) in this
// round, cur being shown: its position, 0 when back to the original (a new round starts). busy()
// is called before a form is computed (the status line: computing...).
int answer_forms_next(answer_forms & F,int k,const std::string & cur,void (*busy)());

// F4's forms are printed the same way
giac::gen positive_first(const giac::gen & g);
void textbook(std::string & str);
void textbook_constants(std::string & str);
// Focus: the decimal value of an exact numeric result, shown under it (console_approx), for the
// result printed as console_approx_for
void result_approx(const giac::gen & g);
void set_approx_for(const std::string & s);
const char * console_approx();
extern const char * console_approx_for;
#endif
