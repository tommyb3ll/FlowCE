// steps.cc - step-by-step solutions (steps.h).
// Built for size: a step stores a rule number and its parts. Each rule's math line is a template
// (giac syntax; A, B, C the parts, V the variable, dV and dA their differentials) filled only when
// the step is shown. The methods share helpers: a rewritten integrand (via), a substitution (sub).
#if defined FLOWCE_STEPS || defined HOSTGIAC
#include "steps.h"
#ifdef TICE
extern "C" volatile unsigned char focus_phase; // focus.cc: timing probe (tools/emu/phases.py)
#endif
#include <string.h>
using namespace giac;
#define NOINL __attribute__((noinline))
#ifdef TICE
#define TOL 1e-4 // the calculator's doubles are 32-bit floats
#else
#define TOL 1e-8
#endif

// ---------------------------------------------------------------- rules
// title | formula (giac syntax, drawn small; 0: none) | math line
enum {
  R_EQD, R_SUMD, R_CONSTD, R_PROD, R_QUOT, R_POWD, R_CHAIN, R_BASICD, R_EXPD, R_LOGD,
  R_EQI, R_SUMI, R_CONSTI, R_POWI, R_CONSTANT, R_LNI, R_TABLE, R_ATAN, R_ASIN, R_LNSQ, R_SUB, R_SUBEQ, R_BACK,
  R_PARTS, R_PARTSV, R_PARTS2, R_CYCLE, R_PFRAC, R_TRIGID, R_PYTH, R_EXPAND, R_SQUARE, R_SIMP, R_TSUB,
  R_TSUBR, R_TSUBEQ, R_TRI, R_SPLIT, R_CONJ, R_WEIER, R_ABS, R_IMPROPER, R_NOELEM,
  R_FTC, R_EQV, R_PART, R_ABSI, R_FTC1, R_LSUB, R_LLAW, R_LALG, R_LCONJ, R_LHOP, R_LDIV, R_LQUOT,
  R_LEXP, R_LINFV, R_LDNE, R_LSQZ, R_LCOMP, R_LFRAC, R_LDOM, R_TDER, R_TVAL, R_TAYLOR,
  R_SDIV, R_SRAT, R_SGEOM, R_PSUM, R_PSUMT, R_SLIM, R_PSER, R_LCMP, R_LFORM, R_AST, R_RATIO, R_ROOT, R_LNQ, R_ITEST, R_COSPI, R_LOSC, R_LDIVQ,
  R_AREA, R_DISK, R_WASH, R_SHELL, R_ARC, R_ARCP, R_SURF, R_AVG, R_MEET, R_SQ, R_LINE, R_SGEOMD, R_SCONV, R_LSIDE,
  R_ADIST, R_ALCD, R_ASWAP, R_ASUB, R_AADD, R_ADIV, R_ACONT, R_ATRUE, R_AFALSE,
  R_QSTD, R_QABC, R_QFORM, R_QDISC, R_QSIMP, R_ALINE, R_QTWO, R_QTWO2, R_QONE, R_QNONE,
  R_FGCF, R_FDSQ, R_FSUMC, R_FDIFC, R_FPSQ, R_FTWO, R_FSPLIT, R_FGROUP, R_FQUAD, R_FROOT, R_FZERO, R_FSOLVE, R_FNOREAL, R_FPRIME, R_FPRIME2,
  R_REXCL, R_RLCD, R_RKEEP, R_RDROP, R_RSQ, R_ROK, R_RBAD, R_RNONE,
  R_CSGROUP, R_CSFACT, R_CSHALF, R_CSADD, R_CSSQ, R_CSDIST, R_CSCOMB, R_CSVERT, R_CSIN, R_CSBACK,
  R_DLEAD, R_DSUB, R_DREM, R_DCHECK, R_DANS, R_SYSET, R_SYTAB, R_SYDOWN, R_SYMUL, R_SYREAD, R_SYDIVA, R_KFAC, R_KCAN,
  R_PFDIV, R_PFFACT, R_PFSET, R_PFMULT, R_PFROOT, R_PFCOEF, R_PFSOLVE, R_PFRES,
  R_YSTD, R_YFRAC, R_YMUL, R_YADD, R_YSUBT, R_YBACK, R_YSOL, R_YNONE, R_YINF, R_YPAIR, R_YTWO, R_YNONE3, R_YINF3,
  R_GSAME, R_GEXPEQ, R_GLN, R_GPOW, R_GEXPF, R_GPRODR, R_GQUOTR, R_GPOWR, R_GARG, R_GOK, R_GBAD, R_GNONE, R_GSUBU, R_GBACK, R_GXSIDE, R_GFACX, R_GMOVE,
  R_TREF, R_TQS1, R_TQS2, R_TQC1, R_TQC2, R_TQT1, R_TQT2, R_TAXIS, R_TGEN, R_TSOLVX, R_TINT, R_TRANGE, R_TDIVC, R_TPYTH, R_TEACH,
  R_IDIFF, R_ICHAIN, R_ISIDE, R_IFACT,
  R_NFLIP, R_NFAC, R_NCRIT, R_NTEST, R_NLINE, R_NKEEP1, R_NKEEP2, R_NKEEP3, R_NKEEP4, R_NNONE, R_NALL,
  R_BNEG, R_BZERO, R_BTWO, R_BCASE1, R_BCASE2, R_BBETW, R_B3, R_B3DIV, R_B3FLIP, R_BOUT, R_BALL, R_BNOT0, R_BSOL,
  R_NRULES
};
static const char * const RULES[]={
  "",0,"diff(A,V)=B",
  "Sum rule","diff(u+v,x)=diff(u,x)+diff(v,x)","diff(A,V)=B",
  "Constant multiple","diff(c*u,x)=c*diff(u,x)","diff(A,V)=B*diff(C,V)",
  "Product rule","diff(u*v,x)=diff(u,x)*v+u*diff(v,x)","diff(A,V)=diff(B,V)*C+B*diff(C,V)",
  "Quotient rule","diff(u/v,x)=(diff(u,x)*v-u*diff(v,x))/v^2","diff(A,V)=(diff(B,V)*C-B*diff(C,V))/C^2",
  "Power rule","diff(x^n,x)=n*x^(n-1)","diff(A,V)=B",
  "Chain rule","diff(f(u),x)=diff(f,u)*diff(u,x)","diff(A,V)=B*diff(C,V)",
  "Known derivative",0,"diff(A,V)=B",
  "Exponential rule","diff(a^x,x)=a^x*ln(a)","diff(A,V)=B",
  "Write the power with e",0,"A=B",
  "",0,"integrate(A,V)=B",
  "Sum rule","integrate(u+v,x)=integrate(u,x)+integrate(v,x)","integrate(A,V)=B",
  "Constant multiple","integrate(c*u,x)=c*integrate(u,x)","integrate(A,V)=B*integrate(C,V)",
  "Power rule","integrate(x^n,x)=x^(n+1)/(n+1)","integrate(A,V)=B",
  "Integral of a constant","integrate(c,x)=c*x","integrate(A,V)=B",
  "Integral of 1/x","integrate(1/x,x)=ln(abs(x))","integrate(A,V)=B",
  "Known integral",0,"integrate(A,V)=B",
  "Arctangent formula","integrate(1/(a^2+x^2),x)=atan(x/a)/a","integrate(A,V)=B",
  "Arcsine formula","integrate(1/sqrt(a^2-x^2),x)=asin(x/a)","integrate(A,V)=B",
  "Logarithm formula","integrate(1/sqrt(x^2+c),x)=ln(abs(x+sqrt(x^2+c)))","integrate(A,V)=B",
  "Substitution",0,"A=B,dA=C*dV",
  "",0,"integrate(A,V)=integrate(B,C)",
  "Substitute back",0,"integrate(A,V)=B",
  "Integration by parts","integrate(u,v)=u*v-integrate(v,u)","U=A,dW=B*dV",
  "",0,"dU=A*dV,W=B",
  "",0,"integrate(A,V)=B",
  "The integral comes back: solve for it",0,"integrate(A,V)=B",
  "Partial fractions",0,"A=B",
  "Trig identity",0,"A=B",
  "Pythagorean identity","tan(x)^2=sec(x)^2-1","A=B",
  "Expand",0,"A=B",
  "Complete the square",0,"A=B",
  "Simplify",0,"A=B",
  "Trig substitution",0,"V=B,dV=C*dA",
  "",0,"A=B",
  "",0,"integrate(A,V)=integrate(B,C)",
  "Back to x with a right triangle",0,"sin(V)=A,cos(V)=B",
  "Split the numerator",0,"A=B",
  "Multiply by the conjugate",0,"A=B",
  "Substitution t = tan(x/2)","[sin(x)=2*t/(1+t^2),cos(x)=(1-t^2)/(1+t^2),dx=2/(1+t^2)*dt]","integrate(A,V)=integrate(B,C)",
  "Split where the inside changes sign",0,"A=B",
  "Improper integral: a limit",0,"A=B",
  "No elementary antiderivative: the decimal value",0,"A=B",
  "Evaluate from a to b","integrate(f,x,a,b)=F(b)-F(a)","A=B",
  "",0,"A=B",
  "Compute",0,"A",
  "Sign on the interval",0,"A=B",
  "Fundamental theorem of calculus","diff(integrate(f(t),t,a,u),x)=f(u)*diff(u,x)","diff(A,V)=B",
  "Direct substitution",0,"A=B",
  "Limit laws",0,"A=B",
  "Factor and cancel",0,"A=B",
  "Multiply by the conjugate",0,"A=B",
  "L'Hopital's rule","limit(f/g,x,a)=limit(diff(f,x)/diff(g,x),x,a)","A=B",
  "Divide by the highest power",0,"A=B",
  "Write the product as a quotient",0,"A=B",
  "Write the power with e","u^v=e^(v*ln(u))","A=B",
  "The denominator goes to 0, the numerator does not",0,"A=B",
  "The one-sided limits differ: no limit",0,"A,B",
  "Squeeze theorem: bounded times going to 0",0,"A=B",
  "Limit of a continuous function","limit(f(u),x,a)=f(limit(u,x,a))","A=B",
  "One fraction",0,"A=B",
  "Factor out the fastest term",0,"A=B",
  "Derivatives",0,"A=B",
  "Their values at the center",0,"A=B",
  "Taylor polynomial",0,"A=B",
  "Divergence test: the terms do not go to 0",0,"A=B",
  "Ratio of consecutive terms",0,"A=B",
  "Geometric series with |r| < 1","sum(a*r^k,k,0,inf)=a/(1-r)","A=B",
  "Partial sum",0,"A=B",
  "Partial sum: the middle terms cancel",0,"A=B",
  "Its limit",0,"A=B",
  "p-series: converges for p > 1, diverges for p <= 1","sum(1/n^p,n,1,inf)","A=B",
  "Limit comparison with the p-series","limit(a(n)/b(n),n,inf)=L","A=B",
  "Indeterminate form",0,"A,B",
  "Alternating series test: the size decreases to 0",0,"A=B",
  "Ratio test: below 1, the series converges",0,"A=B",
  "Root test: below 1, the series converges",0,"A=B",
  "Log of a quotient","ln(a/b)=ln(a)-ln(b)","A=B",
  "Integral test: same behavior as the integral",0,"A",
  "cos(n pi) is (-1)^n",0,"A=B",
  "Oscillates between -1 and 1: no limit",0,"A=B",
  "Long division",0,"A=B",
  "Area between curves","A=integrate(abs(f(x)-g(x)),x,a,b)","A=B",
  "Disk method","V=pi*integrate(R(x)^2,x,a,b)","R(V)=A",
  "Washer method","V=pi*integrate(R(x)^2-r(x)^2,x,a,b)","R(V)=A,r(V)=B",
  "Shell method","V=2*pi*integrate(r(x)*h(x),x,a,b)","r(V)=A,h(V)=B",
  "Arc length","L=integrate(sqrt(1+diff(f(x),x)^2),x,a,b)","diff(f(V),V)=A",
  "Arc length of a parametric curve","L=integrate(sqrt(diff(x,t)^2+diff(y,t)^2),t,a,b)","diff(x,V)=A,diff(y,V)=B",
  "Surface area","S=2*pi*integrate(r(x)*sqrt(1+diff(f(x),x)^2),x,a,b)","r(V)=A,diff(f(V),V)=B",
  "Average value","integrate(f(x),x,a,b)/(b-a)","A=B",
  "Where the curves meet",0,"A=B",
  "A perfect square",0,"sqrt(A)=B",
  "",0,"A",
  "Geometric series with |r| >= 1: diverges",0,"abs(A)>=1",
  "Converges when |r| < 1",0,"A",
  "Sign on that side",0,"A=B",
  // equations (DeltaCE): A, B the sides, C the common denominator
  "Distribute and combine like terms",0,"A=B",
  "Multiply both sides by the common denominator",0,"C*(A)=C*(B)",
  "Swap the sides",0,"A=B",
  "Subtract from both sides",0,"A=B",
  "Add to both sides",0,"A=B",
  "Divide both sides",0,"(A)/C=(B)/C",
  "",0,"A=B",
  "Always true: every number is a solution",0,"A=B",
  "Never true: no solution",0,"A=B",
  "Move every term to one side",0,"A=0",
  "Identify a, b and c",0,"a=A,b=B,c=C",
  "Quadratic formula","x=(-b\xc2\xb1sqrt(b^2-4ac))/(2a)","A",
  "The discriminant","b^2-4ac","A",
  "Simplify",0,"A",
  "",0,"A",
  "Two solutions",0,"V=A,V=B",
  "",0,"V=A,V=B",
  "The discriminant is 0: one solution",0,"V=A",
  "The discriminant is negative: no real solution",0,"A<0",
  // factoring (DeltaCE): A a line written as text, or an equation
  "Factor out the greatest common factor",0,"A",
  "Difference of squares","a^2-b^2=(a-b)(a+b)","A",
  "Sum of cubes","a^3+b^3=(a+b)(a^2-ab+b^2)","A",
  "Difference of cubes","a^3-b^3=(a-b)(a^2+ab+b^2)","A",
  "Perfect square trinomial","a^2+2ab+b^2=(a+b)^2","A",
  "Two numbers: product c, sum b",0,"A",
  "Split the middle term (ac method)",0,"A",
  "Factor by grouping",0,"A",
  "No whole numbers: roots by the formula","x=(-b\xc2\xb1sqrt(b^2-4ac))/(2a)","A",
  "A rational root: divide by its factor",0,"A",
  "Zero product property: each factor is 0",0,"A",
  "Solve each",0,"A",
  "No real solution from this factor",0,"A=0",
  "No real roots: does not factor",0,"A",
  "Does not factor further",0,"A",
  // rational and radical equations (DeltaCE)
  "Excluded values: no denominator can be 0",0,"A",
  "Multiply both sides by the LCD",0,"A",
  "Check: not an excluded value",0,"A",
  "An excluded value: not a solution",0,"A",
  "Square both sides",0,"A",
  "Check in the equation: both sides agree",0,"A",
  "Check: the sides differ, extraneous",0,"A",
  "Every candidate is extraneous: no solution",0,"A",
  // F4's forms (DeltaCE): completing the square, long division; A a line written as text
  "Group the x terms",0,"A",
  "Factor a out of the x terms",0,"A",
  "Half the x coefficient, squared","(b/2)^2","A",
  "Add it and subtract it",0,"A",
  "Write the perfect square",0,"A",
  "Distribute",0,"A",
  "Combine the numbers",0,"A",
  "The vertex","a*(x-h)^2+k: (h,k)","A",
  "Complete the square in",0,"A",
  "Put it back",0,"A",
  "Divide the leading terms",0,"A",
  "Multiply back and subtract",0,"A",
  "The remainder's degree is lower: done",0,"A",
  "Check: divisor times quotient, plus remainder",0,"A",
  "",0,"A", // (a division typed: quotient plus remainder over the divisor, the answer box)
  // synthetic division, factor and cancel (DeltaCE)
  "Write c, where the divisor is 0",0,"A",
  "",0,"A", // (the table)
  "Bring down the first coefficient",0,"A",
  "Multiply by c, add down the column",0,"A",
  "Bottom row: the quotient, then the remainder",0,"A",
  "Divide the quotient by the x coefficient",0,"A",
  "Factor the top and the bottom",0,"A",
  "Cancel the common factors",0,"A",
  // partial fractions (DeltaCE)
  "The top's degree is not lower: divide first",0,"A",
  "Factor the denominator",0,"A",
  "A fraction for each factor",0,"A",
  "Multiply both sides by the denominator",0,"A",
  "Put in a root of the denominator",0,"A",
  "Match the coefficients of each power of x",0,"A",
  "Solve for the unknowns",0,"A",
  "Put the values in",0,"A",
  // systems of linear equations (DeltaCE): elimination
  "Write each equation in standard form",0,"A",
  "Clear the fractions: multiply by the LCD",0,"A",
  "Multiply so that a variable drops out",0,"A",
  "Add the equations",0,"A",
  "Subtract the equations",0,"A",
  "Put it into an equation",0,"A",
  "The solution",0,"A",
  "Never true: no solution (the lines are parallel)",0,"A",
  "Always true: infinitely many (the same line)",0,"A",
  "Take a pair of equations",0,"A",
  "Two equations in two unknowns",0,"A",
  "Never true: no solution",0,"A",
  "Always true: infinitely many solutions",0,"A",
  "Write both sides as powers of the same base",0,"A",
  "Same base: the exponents are equal",0,"A",
  "Take ln of both sides",0,"A",
  "Power rule of logs","ln(a^u)=u*ln(a)","A",
  "Exponential form: log_b(u) = c means u = b^c",0,"A",
  "Product rule of logs","log(a)+log(b)=log(a*b)","A=B",
  "Quotient rule of logs","log(a)-log(b)=log(a/b)","A=B",
  "Power rule of logs","k*log(a)=log(a^k)","A=B",
  "Same base: the arguments are equal",0,"A",
  "Check: every log's argument is positive",0,"A",
  "A log of a number <= 0: not a solution",0,"A",
  "A power of a positive base is never <= 0: no solution",0,"A",
  "A quadratic in the power: substitute",0,"A",
  "Back to x",0,"A",
  "Unknowns to one side, the rest to the other",0,"A=B",
  "Factor out x",0,"A=B",
  "Move the logs to one side",0,"A=B",
  "The reference angle",0,"A=B",
  "sin > 0: quadrants I and II",0,"A",
  "sin < 0: quadrants III and IV",0,"A",
  "cos > 0: quadrants I and IV",0,"A",
  "cos < 0: quadrants II and III",0,"A",
  "tan > 0: quadrants I and III",0,"A",
  "tan < 0: quadrants II and IV",0,"A",
  "On an axis of the unit circle",0,"A",
  "All solutions: add n periods (n an integer)",0,"A",
  "Solve for x",0,"A",
  "The solutions in [0, 2pi)",0,"A",
  "sin and cos are between -1 and 1: no solution",0,"A=B",
  "Divide both sides by cos: sin/cos is tan",0,"A=B",
  "Pythagorean identity","sin(x)^2+cos(x)^2=1","A=B",
  "The angles where",0,"A=B",
  "Differentiate both sides: y is a function of x",0,"diff(A,V)=diff(B,V)",
  "Chain rule: each y term's derivative times dy/dx",0,"A=B",
  "The dy/dx terms to one side, the rest to the other",0,"A=B",
  "Factor out dy/dx",0,"A=B",
  // inequalities, absolute values (DeltaCE): a step's = is its relation (step_t rel)
  "Divide by a negative: the sign flips",0,"(A)/C=(B)/C",
  "Factor",0,"A",
  "Where it is 0 or undefined",0,"A",
  "Test a number in each interval",0,"A",
  "",0,"A",
  "Keep where it is negative",0,"A",
  "Keep where it is negative or 0",0,"A",
  "Keep where it is positive",0,"A",
  "Keep where it is positive or 0",0,"A",
  "No interval works: no solution",0,"A",
  "Every number works",0,"A",
  "An absolute value is never negative",0,"A",
  "Only 0 has absolute value 0",0,"A",
  "Two cases: the inside is k or -k",0,"A",
  "First case",0,"A=B",
  "Second case",0,"A=B",
  "Less than k: between -k and k",0,"A",
  "The same to all three parts",0,"A",
  "Divide all three parts",0,"A",
  "Divide by a negative: the signs flip",0,"A",
  "More than k: below -k or above k",0,"A",
  "Every number works",0,"A",
  "Every number but where the inside is 0",0,"A",
  "The solutions",0,"A",
};
// (DeltaCE) an inequality's relation: 0 an equation, 1 <, 2 <=, 3 >, 4 >=
static const unary_function_ptr * relop(int r){ return r==1?at_inferieur_strict:r==2?at_inferieur_egal:r==3?at_superieur_strict:r==4?at_superieur_egal:at_equal; }
static int rel_of(const gen & e){ for (int r=1;r<=4;++r) if (e.is_symb_of_sommet(relop(r))) return r; return 0; }
static int flip(int r){ return r?(r+1)%4+1:0; } // (< and >, <= and >=)
static const char * const RELT[5]={"=","<","<=",">",">="};

const char * steps_title(const step_t & s){ return RULES[3*s.rule]; }
int steps_method,steps_methods; // (DeltaCE, steps.h)
const char * steps_method_name="";
bool steps_answer(const std::vector<step_t> & v,gen & r){ // (DeltaCE)
  if (v.empty() || v.back().rule!=R_DANS) return false;
  r=v.back().a;
  return true;
}
const char * steps_formula(const step_t & s){ return RULES[3*s.rule+1]; }
// (DeltaCE) a synthetic division's table: rows of cells, a tab between cells, a newline between
// rows, \1 before a cell new in this step; "" for another step
std::string steps_table(const step_t & s,GIAC_CONTEXT){
  std::string t;
  if ((s.rule!=R_SYTAB && s.rule!=R_SYDOWN) || s.a.type!=_VECT) return t;
  const vecteur & rows=*s.a._VECTptr;
  for (unsigned i=0;i<rows.size();++i){
    if (i) t+='\n';
    if (rows[i].type!=_VECT) return std::string();
    const vecteur & r=*rows[i]._VECTptr;
    for (unsigned j=0;j<r.size();++j){
      if (j) t+='\t';
      if (is_undef(r[j])) continue;
      if (s.b.type==_VECT) for (unsigned k=0;k<s.b._VECTptr->size();++k) if ((*s.b._VECTptr)[k]==gen(int(32*i+j))){ t+='\1'; break; }
      t+=r[j].print(contextptr);
    }
  }
  return t;
}

// d followed by a name (du, dx): identifiers keep their char pointer, the string one copies it
static gen dname(const gen & v){ return gen(identificateur(std::string("d")+v.print(context0))); }

static gen mergek(const gen & g);
static NOINL gen radicand(const gen & g);
static vecteur * phm_;
gen steps_math(const step_t & s,GIAC_CONTEXT){
  if (!phm_){ // (giac's results are read as vectors only after a check: ON leaves error values)
    gen l("[A,B,C,V,dV,dA,U,W,dU,dW]",contextptr);
    if (l.type!=_VECT) return undef;
    phm_=new vecteur(*l._VECTptr);
  }
  // by parts: u and v, unless the variable is one of them (after a substitution u): p and q
  bool uv=s.v.type==_IDNT && (!strcmp(s.v._IDNTptr->id_name,"u") || !strcmp(s.v._IDNTptr->id_name,"v"));
  gen U(identificateur(uv?"p":"u")),W(identificateur(uv?"q":"v"));
#ifdef TICE
  focus_phase=135;
#endif
  const gen tpl(RULES[3*s.rule+2],contextptr);
#ifdef TICE
  focus_phase=136;
#endif
  // the values of the template's own names only (subst is simultaneous: the others did nothing, and
  // dA printed the step's whole expression, ~40 ms a step on the calculator with the rest)
  const char * T=RULES[3*s.rule+2];
  vecteur from,to;
  for (int k=0;k<10;++k){
    const gen & p=(*phm_)[k];
    if (p.type!=_IDNT || !strstr(T,p._IDNTptr->id_name)) continue;
    from.push_back(p);
    to.push_back(k==0?s.a:k==1?s.b:k==2?s.c:k==3?s.v:k==4?dname(s.v):k==5?dname(s.a):k==6?U:k==7?W:k==8?dname(U):dname(W));
  }
  gen r=subst(tpl,from,to,true,contextptr);
  if (s.rel && r.is_symb_of_sommet(at_equal)) r=symbolic(*relop(s.rel),r._SYMBptr->feuille); // (DeltaCE: x < 4)
#ifdef TICE
  focus_phase=137;
#endif
  return s.rule>=R_ADIST?r:mergek(r); // (an equation's line: 4*1*6 as substituted, not 24)
}

// ---------------------------------------------------------------- state and helpers
static std::vector<step_t> * out_;
static unsigned char depth_;
static const gen * in_; // the problem as typed: its limit is the console's, the steps' reference
static int budget_;
static const context * ctx_;
static gen * var_; // the variable (no global gen: the app has no static constructors)
#define CTX ctx_
#define X (*var_)

static unsigned char rel_; // (DeltaCE) the inequality being solved (relop), 0 for an equation
static NOINL void S(int r,const gen & a,const gen & b,const gen & c){
  step_t s; s.rule=r; s.depth=depth_; s.rel=rel_; s.a=a; s.b=b; s.c=c; s.v=X;
  out_->push_back(s);
}
static NOINL void S(int r,const gen & a,const gen & b){ S(r,a,b,zero); }
static NOINL size_t mark(){ return out_->size(); }
static NOINL void cut(size_t m){ out_->resize(m); }
static NOINL gen ev(const gen & g){ return eval(g,1,CTX); }
// a power of a sum over 40 ((2x+1)^2026): normal and ratnormal would expand it (8x (2x+1)^2026's last
// line held 1.25 MB of heap on the PC: the calculator ran out of memory and quit); kept as written
// (any: a power over 40 of anything but a variable, ln(n)^80: giac's limit expanded it, 93 MB)
static NOINL bool bigp(const gen & g,bool any=false){
  if (g.type!=_SYMB) return false;
  const gen & a=g._SYMBptr->feuille;
  if (g.is_symb_of_sommet(at_pow) && a.type==_VECT && a._VECTptr->size()==2 && (any?a[0].type!=_IDNT:a[0].is_symb_of_sommet(at_plus)) && a[1].type==_INT_ && (a[1].val>40 || a[1].val<-40)) return true;
  if (a.type==_VECT){ for (const_iterateur it=a._VECTptr->begin();it!=a._VECTptr->end();++it) if (bigp(*it,any)) return true; return false; }
  return bigp(a,any);
}
static NOINL gen rn(const gen & g){ return bigp(g)?g:ratnormal(g,CTX); }
// the radicands of g's fractional powers (normal over two of them, giac's algebraic extensions, took
// 4.7 MB and 20 s on the PC for sqrt(sin x)/(sqrt(sin x)+sqrt(cos x)); ratnormal over one leaves
// sqrt(3)^2 or x+2x: normal for it)
static NOINL int nrad(const gen & g){
  vecteur rs,ps=lop(g,at_pow);
  for (const_iterateur it=ps.begin();it!=ps.end();++it){ const gen & a=it->_SYMBptr->feuille; if (a.type==_VECT && a[1].type==_FRAC && !equalposcomp(rs,a[0])) rs.push_back(a[0]); }
  return rs.size();
}
// ratnormal, then normal if shorter over one radicand: 3/(8x^2 sqrt(x)), not (x+2x)/(8x^2 sqrt(x) x)
static NOINL gen rn1(const gen & g){
  gen r=rn(g);
  if (nrad(r)==1 && !bigp(r)){ gen s=normal(r,CTX); if (taille(s,200)<taille(r,200)) r=s; }
  return r;
}
static NOINL gen D(const gen & g,const gen & x){ return ev(derive(g,x,CTX)); }
static NOINL bool dep(const gen & g,const gen & x){
  return g.type==_IDNT ? g==x : (g.type==_SYMB || g.type==_VECT) && depend(g,*x._IDNTptr);
}
static bool dep(const gen & g){ return dep(g,X); }
static NOINL gen sub(const gen & g,const gen & a,const gen & b){ return subst(g,a,b,false,CTX); }
static NOINL gen SYM(const unary_function_ptr * u,const gen & a){ return symbolic(*u,a); }
static NOINL gen SYM2(const unary_function_ptr * u,const gen & a,const gen & b){ return symbolic(*u,makesequence(a,b)); }
static gen Dx(const gen & f){ return SYM2(at_derive,f,X); } // d/dx f, not computed
static gen Ix(const gen & f){ return SYM2(at_integrate,f,X); }
static bool is(const gen & g,const unary_function_ptr * u){ return g.is_symb_of_sommet(u); }
static const gen & arg(const gen & g){ return g._SYMBptr->feuille; }
static gen arg(const gen & g,int i){ return g._SYMBptr->feuille[i]; }
// loops by iterator: an index costs a multiplication by sizeof(gen) on the eZ80 (a helper call)
#define EACH(it,v) for (const_iterateur it=(v).begin(),it##_=(v).end();it!=it##_;++it)
#define EACHW(it,v) for (iterateur it=(v).begin(),it##_=(v).end();it!=it##_;++it)
// f applied to g's arguments in turn: the first nonzero result (0: none)
static NOINL gen first_of(const gen & g,gen (*f)(const gen &)){
  if (g.type!=_SYMB) return 0;
  const gen & a=arg(g);
  if (a.type!=_VECT) return f(a);
  EACH(it,*a._VECTptr){ gen r=f(*it); if (!is_zero(r)) return r; }
  return 0;
}
// g with f applied to each argument
static NOINL gen map_of(const gen & g,gen (*f)(const gen &)){
  gen a=arg(g);
  if (a.type!=_VECT) return symbolic(g._SYMBptr->sommet,f(a));
  vecteur v=*a._VECTptr;
  EACHW(it,v) *it=f(*it);
  return symbolic(g._SYMBptr->sommet,gen(v,a.subtype));
}

// the factors of a product (-g: -1 and g) or the terms of a sum, flattened
static NOINL void flat(const unary_function_ptr * u,const gen & g,vecteur & v){
  if (is(g,u) && arg(g).type==_VECT){
    EACH(it,*arg(g)._VECTptr) flat(u,*it,v);
  }
  else if (u==at_prod && g.is_neg()){ v.push_back(-1); flat(u,arg(g),v); }
  else v.push_back(g);
}
// for display: the numbers of each product multiplied together (2*1/3 is 2/3, 1*dx is dx)
static bool isnum(const gen & g){ return g.type<_IDNT || g.type==_FRAC; }
static gen prodof(const vecteur & v){ return v.empty()?gen(1):v.size()==1?v[0]:symb_prod(v); }
// a polynomial (names, sums, products, positive integer powers: x, x^2+1), written before the
// other factors of a product (mergek)
static NOINL bool polyf(const gen & g){
  if (g.type<=_IDNT) return true;
  if (g.type!=_SYMB) return false;
  if (g.is_pow()) return arg(g,1).type==_INT_ && arg(g,1).val>0 && polyf(arg(g,0));
  if (!g.is_plus() && !g.is_prod() && !g.is_neg()) return false;
  if (arg(g).type!=_VECT) return polyf(arg(g));
  EACH(it,*arg(g)._VECTptr) if (!polyf(*it)) return false;
  return true;
}
// b^e of g (e = 1 when g is no power)
static NOINL gen pexp(const gen & g,gen & b){
  if (!g.is_pow()){ b=g; return 1; }
  b=arg(g,0);
  return arg(g,1);
}
static bool raw_; // the problem as typed: no sec for 1/cos, no powers merged (x sqrt(x))
static NOINL gen mergek(const gen & g){
  if (g.type==_VECT){ vecteur v=*g._VECTptr; EACHW(it,v) *it=mergek(*it); return gen(v,g.subtype); } // (u=..., du=...)
  if (g.type!=_SYMB) return g;
  gen h=map_of(g,mergek),b,b2;
  if (!raw_ && h.is_inv() && (is(arg(h),at_cos) || is(arg(h),at_sin))) return SYM(is(arg(h),at_cos)?at_sec:at_csc,arg(arg(h))); // sec x, not 1/cos x
  // textbook notation: (x^(1/9))^-2 as x^(-2/9) ...
  if (!raw_ && h.is_pow() && arg(h,0).is_pow() && isnum(arg(h,1)) && isnum(arg(arg(h,0),1))){ // ... (sqrt(x+1))^2 as x+1
    const gen e=arg(arg(h,0),1)*arg(h,1),b=arg(arg(h,0),0);
    h=is_one(e)?b:symb_pow(b,e);
  }
  if (!h.is_prod() || arg(h).type!=_VECT) return h;
  vecteur v,w,den; gen k=1;
  flat(at_prod,h,v);
  EACH(it,v){
    gen e=pexp(*it,b);
    if (isnum(*it)) k=k*(*it);
    else if (it->is_inv()){ if (isnum(arg(*it))) k=k/arg(*it); else den.push_back(arg(*it)); } // ... one denominator,
    else if (isnum(e) && is_strictly_positive(-e,CTX)) den.push_back(is_minus_one(e)?b:symb_pow(b,-e));
    else { // ... x*x^(1/3) as x^(4/3)
      bool m=false;
      EACHW(jt,w){ gen e2=pexp(*jt,b2); if (!m && !raw_ && b2==b && isnum(e) && isnum(e2)){ e=e+e2; *jt=is_one(e)?b:symb_pow(b,e); m=true; } }
      if (!m) w.push_back(*it);
    }
  }
  if (k.type==_FRAC && !den.empty()){ den.insert(den.begin(),k._FRACptr->den); k=k._FRACptr->num; } // dx/(2 sqrt(x))
  // cos u csc^k u is cot u csc^(k-1) u, sin u sec^k u is tan u sec^(k-1) u (giac evaluates cot and
  // tan with sin and cos: -2cos(x)csc(x)^3 for -2cot(x)csc(x)^2), cos u sec u and sin u csc u are 1
  if (!raw_) EACHW(it,w) EACHW(jt,w){
    gen e=pexp(*jt,b2);
    if (jt==it || !(is(*it,at_cos) || is(*it,at_sin)) || !(is(b2,at_csc) || is(b2,at_sec)) || arg(*it)!=arg(b2) || e.type!=_INT_ || e.val<1) continue;
    const bool c=is(*it,at_cos);
    *it=c==is(b2,at_sec)?gen(1):SYM(c?at_cot:at_tan,arg(*it));
    *jt=e.val==1?gen(1):e.val==2?b2:symb_pow(b2,e-1);
  }
  for (unsigned i=0;i<w.size();) if (is_one(w[i])) w.erase(w.begin()+i); else ++i;
  // the polynomial factors first, as the textbooks write them (2x e^(x^2), not 2 e^(x^2) x), the
  // differential last (e^x dx)
  vecteur u;
  if (!is_one(k) || w.empty()) u.push_back(k);
  for (int pass=0;pass<3;++pass)
    EACH(it,w){
      const bool d=it->type==_IDNT && it->_IDNTptr->id_name[0]=='d' && it->_IDNTptr->id_name[1] && !it->_IDNTptr->id_name[2];
      if (pass==(d?2:polyf(*it)?0:1)) u.push_back(*it);
    }
  if (!den.empty()) u.push_back(symb_inv(prodof(den)));
  if (is_minus_one(k) && u.size()>1){ u.erase(u.begin()); return symb_neg(prodof(u)); } // -x^3, not -1*x^3
  return prodof(u);
}
// constant factors in c, the others in v
static NOINL void factors(const gen & f,vecteur & c,vecteur & v){
  vecteur w; flat(at_prod,f,w);
  EACH(it,w) (dep(*it)?v:c).push_back(*it);
}

// g built with + * and positive integer powers from constants and x (giac's lvarx gives x for
// sqrt(1-x^2): no help). SH_FRAC: quotients too (a rational fraction); SH_ROOTX: any power of x
// itself (x^(1/3), x^-2); SH_TRIG: sin and cos of linear arguments instead of x.
enum { SH_FRAC=1, SH_ROOTX=2, SH_TRIG=4 };
static NOINL bool shape(const gen & g,int m){
  if (!dep(g)) return true;
  if (g==X) return !(m & SH_TRIG);
  if (g.type!=_SYMB) return false;
  const gen & a=arg(g);
  if (is(g,at_sin) || is(g,at_cos)){ gen p,q; return (m & SH_TRIG) && is_linear_wrt(a,X,p,q,CTX); }
  if (g.is_pow()){
    if (dep(arg(g,1))) return false;
    if ((m & SH_ROOTX) && arg(g,0)==X) return true;
    return arg(g,1).type==_INT_ && (arg(g,1).val>0 || (m & SH_FRAC)) && shape(arg(g,0),m);
  }
  if (g.is_inv()) return ((m & SH_FRAC) || ((m & SH_ROOTX) && (a==X || (a.is_pow() && arg(a,0)==X)))) && shape(a,m);
  if (g.is_neg()) return shape(a,m);
  if ((g.is_plus() || g.is_prod()) && a.type==_VECT){
    EACH(it,*a._VECTptr) if (!shape(*it,m)) return false;
    return true;
  }
  return false;
}

// The parsed text as sums and products: a-b is a+(-b), a/b a*inv(b), the inverse of a product the
// product of the inverses, nested sums and products flattened, numbers evaluated (1*1/3 is 1/3).
static bool numeric(const gen & g){ // no identifier (lidnt skips the bound ones)
  if (g.type==_IDNT) return false;
  if (g.type==_SYMB) return numeric(arg(g));
  if (g.type==_VECT){ EACH(it,*g._VECTptr) if (!numeric(*it)) return false; }
  return true;
}
static gen inv_of(const gen & g){
  if (g.type<_IDNT) return ev(symb_inv(g));
  if (g.is_prod() && arg(g).type==_VECT){
    vecteur v=*arg(g)._VECTptr;
    EACHW(it,v) *it=inv_of(*it);
    return symb_prod(v);
  }
  if (g.is_inv()) return arg(g);
  return symb_inv(g);
}
static NOINL gen norm(const gen & g){
  if (g.type!=_SYMB) return g;
  if (numeric(g)) return ev(g);
  const unary_function_ptr & u=g._SYMBptr->sommet;
  gen a=arg(map_of(g,norm));
  vecteur v;
  if (u==at_binary_minus && a.type==_VECT && a._VECTptr->size()==2){
    flat(at_plus,a[0],v); flat(at_plus,a[1].type<_IDNT?-a[1]:symb_neg(a[1]),v); return symb_plus(v);
  }
  if (u==at_division && a.type==_VECT && a._VECTptr->size()==2){
    if (!is_one(a[0])) flat(at_prod,a[0],v);
    flat(at_prod,inv_of(a[1]),v); return prodof(v);
  }
  if (u==at_inv) return inv_of(a);
  if ((u==at_plus || u==at_prod) && a.type==_VECT){
    flat(&u,SYM(&u,a),v); return SYM(&u,gen(v,_SEQ__VECT));
  }
  if (u==at_neg && (a.type<_IDNT || a==plus_inf)) return -a; // (-inf: not -(+infinity) at the top)
  if (u==at_sqrt) // sqrt(x^7) is x^(7/2); a power: giac prints x-sqrt(x+1) as x-(sqrt(x+1))
    return a.is_pow() && arg(a,0).type==_IDNT?symb_pow(arg(a,0),ev(arg(a,1)/2)):symb_pow(a,plus_one_half);
  return SYM(&u,a);
}

// a = f numerically (giac's simplify is slow on the calculator), the other variables given values.
// Points on both sides of 0 and far out, first inside [lo,hi] when given (only there: only): every point where f is
// real must agree, 5 of them at most, at least 3 (else complex values agreeing make up the 3; a
// complex one differing can be a branch of sqrt, not an error).
static NOINL bool same_num(const gen & a0,const gen & f0,const gen & x,const gen * lo=0,const gen * hi=0,bool only=false){
  gen d=a0-f0,f=f0;
  vecteur ids=lidnt(makevecteur(d,f));
  double c=0.71;
  EACH(it,ids){ if (*it!=x && evalf(*it,1,CTX).type!=_DOUBLE_){ d=sub(d,*it,c); f=sub(f,*it,c); } c+=0.17; } // (pi keeps its value)
  // a power over 40 of a sum: in the calculator's 32-bit floats its values overflow, or cancel
  // (8x(2x+1)^2026's antiderivative differentiated is 4(2x+1)^2027-4(2x+1)^2026, 2% of each term at
  // x=0.011: the steps said "No steps"); its exact values at integer points instead ((2x+1)^2027 at
  // 1 is 3^2027, a 3213-bit integer)
  if (bigp(d)){
    // (the calculator's giac refuses powers over 1000, FACTORIAL_SIZE_LIMIT: "calculation size limit
    // exceeded" at each point; up to 4096 here, put back at once)
    const int lim=FACTORIAL_SIZE_LIMIT;
    if (lim<4096) FACTORIAL_SIZE_LIMIT=4096;
    // integer points (-1 first: tiny values); two that give exactly 0 are enough (at 2/3, the
    // fractions of 5000-bit numbers took minutes to reduce on the calculator)
    static const signed char ip[]={-1,1,2,3};
    int n=0;
    for (int i=0;i<4 && n>=0 && n<2;++i){
      const gen z=ev(sub(d,x,gen(ip[i])));
      if (z.type!=_INT_ && z.type!=_ZINT && z.type!=_FRAC) continue; // (not a number exactly)
      n=is_zero(z)?n+1:-1;
    }
    FACTORIAL_SIZE_LIMIT=lim;
    if (n<0) return false;
    if (n>=2) return true;
  }
  // (the last ones, next to 0, while fewer than 3 points gave a number: (2x+1)^2026 stays finite there
  // in the calculator's 32-bit floats)
  static const double pts[]={0.13,0.37,0.61,0.89,0.37,-0.61,1.29,-2.4,2.13,3.7,5.3,-7.9,11.3,0.011,-0.013,0.007};
  // the points: inside [lo,hi], the fixed ones, then next to the real ones while fewer than 3 are
  // (sqrt(sec(3x^2-7x)) is real only near some of them)
  vecteur ps;
  for (int i=lo?0:4;i<(only?4:16);++i) ps.push_back(i<4?ev(*lo+(*hi-*lo)*pts[i]):gen(pts[i]));
  unsigned nf=ps.size();
  int real=0,cplx=0;
  for (unsigned i=0;i<ps.size() && real<5 && (i<nf || real<3);++i){
    gen p=ps[i];
    gen z=evalf(sub(f,x,p),1,CTX),y;
    if (z.type!=_DOUBLE_ && z.type!=_CPLX) continue; // f not defined there
    if (z.type==_DOUBLE_ && !(z._DOUBLE_val-z._DOUBLE_val==0)) continue; // or too big ((2x+1)^2026 at 0.37)
    y=evalf(sub(d,x,p),1,CTX);
    if (y.type!=_DOUBLE_ && y.type!=_CPLX) continue;
    double ay=evalf_double(abs(y,CTX),1,CTX)._DOUBLE_val,az=evalf_double(abs(z,CTX),1,CTX)._DOUBLE_val;
    bool ok=ay<=TOL*(1+az);
#ifdef HOSTGIAC
    if (getenv("STEPDBG")) fprintf(stderr,"  same_num at %s: f=%s diff=%s %s\n",p.print(CTX).c_str(),z.print(CTX).c_str(),y.print(CTX).c_str(),ok?"ok":"DIFFERENT");
#endif
    if (z.type==_DOUBLE_){
      if (!ok) return false;
      ++real;
      if (ps.size()<nf+8){ ps.push_back(p+0.011); ps.push_back(p-0.013); }
    }
    else cplx+=ok;
  }
  return real+cplx>=3;
}

// ---------------------------------------------------------------- derivatives
static gen dsteps(const gen & f);

// f = outer(inner): the chain rule (inner x: a known derivative)
static NOINL gen chain(const gen & f,const gen & outer,const gen & inner){
  if (inner==X){ S(R_BASICD,f,outer); return outer; }
  S(R_CHAIN,f,outer,inner);
  ++depth_; gen d=dsteps(inner); --depth_;
  gen r=ev(outer*d);
  S(R_EQD,f,r);
  return r;
}
// the product rule, two factors at a time
static gen dprod(const vecteur & v){
  if (v.size()==1) return dsteps(v[0]);
  gen u=v[0],w=v.size()==2?v[1]:symb_prod(vecteur(v.begin()+1,v.end())),f=symb_prod(u,w);
  S(R_PROD,f,u,w);
  ++depth_;
  gen du=dsteps(u),dw=v.size()==2?dsteps(w):dprod(vecteur(v.begin()+1,v.end()));
  --depth_;
  gen r=ev(du*w+u*dw);
  S(R_EQD,f,r);
  return r;
}
// textbook derivatives of sec, csc, cot, tan (giac's are in sin and cos), in t
static gen trig_der(const unary_function_ptr & u,const gen & t){
  if (u==at_sec) return symb_prod(SYM(at_sec,t),symb_tan(t));
  if (u==at_csc) return symb_neg(symb_prod(SYM(at_csc,t),SYM(at_cot,t)));
  if (u==at_cot) return symb_neg(symb_pow(SYM(at_csc,t),2));
  if (u==at_tan) return symb_pow(SYM(at_sec,t),2);
  return 0;
}
static gen dsteps(const gen & f){
  // ON or clear while the steps are worked out: giac's results are then error values, and the
  // constant-multiple rule's ev(1) is not 1, so dsteps(rest) was dsteps(f) again, without end,
  // until the stack ran into the OS's RAM and the calculator reset (isteps and lsteps check too)
  if (ctrl_c || interrupted) return undef;
  if (!dep(f)) return 0;
  if (f==X) return 1;
  if (f.type!=_SYMB) return D(f,X);
  const unary_function_ptr & u=f._SYMBptr->sommet;
  const gen & a=arg(f);
  if (u==at_plus && a.type==_VECT){
    const vecteur & v=*a._VECTptr;
    vecteur dv,rv;
    EACH(it,v) if (dep(*it)) dv.push_back(it->is_neg()?symb_neg(Dx(arg(*it))):Dx(*it));
    bool shown=dv.size()>1;
    if (shown){ S(R_SUMD,f,symb_plus(dv)); ++depth_; }
    EACH(it,v) if (dep(*it)) rv.push_back(dsteps(*it));
    gen r=ev(symb_plus(rv));
    if (shown){ --depth_; S(R_EQD,f,r); }
    return r;
  }
  if (u==at_neg) return ev(-dsteps(a));
  if (u==at_prod && a.type==_VECT){
    vecteur c,num,den;
    const vecteur & v=*a._VECTptr;
    EACH(it,v){
      const gen & w=*it;
      if (!dep(w)) c.push_back(w);
      else if (w.is_inv() && arg(w)!=X && !(arg(w).is_pow() && arg(arg(w),0)==X)) den.push_back(arg(w));
      else num.push_back(w);
    }
    gen k=ev(prodof(c)),rest=prodof(num);
    if (!is_one(k)){ // c u
      EACH(it,den) rest=num.empty() && it==den.begin()?symb_inv(*it):symb_prod(rest,symb_inv(*it));
      S(R_CONSTD,f,k,rest);
      ++depth_; gen r=dsteps(rest); --depth_;
      r=ev(k*r);
      S(R_EQD,f,r);
      return r;
    }
    if (!den.empty()){ // u/v
      gen n=prodof(num),d=prodof(den);
      S(R_QUOT,f,n,d);
      ++depth_; gen dn=dsteps(n),dd=dsteps(d); --depth_;
      gen r=(dn*d-n*dd)/symb_pow(d,2);
      S(R_EQD,f,r);
      gen r2=rn(r); // (not over a root: giac's ratnormal left (-x^2+x^2+1)/(x^2+1)^(3/2))
      if (taille(r2,200)<taille(r,200) && is_zero(radicand(d))){ S(R_EQD,f,r2); r=r2; }
      return r;
    }
    return dprod(num);
  }
  if (u==at_inv){ // 1/g
    if (a==X || (a.is_pow() && arg(a,0)==X)){
      gen n=a==X?gen(-1):gen(ev(-arg(a,1))),r=ev(n*pow(X,n-1,CTX));
      S(R_POWD,f,r); return r;
    }
    return chain(f,-symb_inv(symb_pow(a,2)),a);
  }
  if (u==at_pow && a.type==_VECT){
    gen b=a[0],e=a[1];
    if (!dep(e)){
      gen o=ev(e*pow(b,e-1,CTX));
      if (b==X){ S(R_POWD,f,o); return o; }
      return chain(f,o,b);
    }
    if (!dep(b)){
      if (e==X){ gen r=ev(f*ln(b,CTX)); S(R_EXPD,f,r); return r; }
      return chain(f,f*ln(b,CTX),e);
    }
    gen w=symb_exp(e*symb_ln(b));
    S(R_LOGD,f,w);
    gen r=ev(sub(dsteps(w),w,f));
    S(R_EQD,f,r);
    return r;
  }
  if (a.type!=_VECT){ // f(g): a known derivative, or the chain rule
    gen t(identificateur(" t")),o=trig_der(u,t);
    if (is_zero(o)) o=D(SYM(&u,t),t);
    return chain(f,subst(o,t,a,true,CTX),a);
  }
  gen r=D(f,X);
  S(R_BASICD,f,r);
  return r;
}
// ---------------------------------------------------------------- integrals
static gen isteps(const gen & f,int lev);
static const char * const UNAMES[]={"u","w","z","s"};
static int nsub_,maxlev_;
static bool cutlev_; // a search stopped at maxlev_
static gen * lo_, * hi_; // the variable's interval in a definite integral (0: none)
static bool absi_; // |g| written g on it: F is checked there only
// g sampled inside [lo_,hi_] (n points): -1 or 1 if g keeps its sign (0 allowed), else 0; or the
// range of g (in a and b)
static NOINL int sampled(const gen & g,int n,gen * a=0,gen * b=0){
  double mn=1e30,mx=-1e30;
  for (int i=0;i<n;++i){
    gen y=evalf(sub(g,X,*lo_+(*hi_-*lo_)*((i+0.5)/n)),1,CTX);
    if (y.type!=_DOUBLE_) return 0;
    double d=y._DOUBLE_val;
    if (d<mn) mn=d;
    if (d>mx) mx=d;
  }
  if (a){ *a=mn; *b=mx; }
  return mn>=0 && mx>0?1:mx<=0 && mn<0?-1:0;
}

// f = g by rule r, then the integral of g; undef (and no step) if it fails
static NOINL gen via(int r,const gen & f,const gen & g,int lev){
  size_t m=mark();
  S(r,f,g);
  gen F=isteps(g,lev+1);
  if (is_undef(F)) cut(m);
  return F;
}
// the integral of g in the variable t = tx (a substitution's), its steps indented; on the interval
// tx takes there
static NOINL gen in_var(const gen & g,const gen & t,const gen & tx,int lev){
  gen * old=var_, * ol=lo_, * oh=hi_, a, b;
  if (lo_){
    sampled(tx,9,&a,&b);
    lo_=a.type==_DOUBLE_?&a:0; hi_=lo_?&b:0;
  }
  var_=(gen *)&t; ++depth_; ++nsub_;
  gen G=isteps(g,lev+1);
  --nsub_; --depth_; var_=old; lo_=ol; hi_=oh;
  return G;
}
// back from u: atan(tan(v)) is v, as in the textbooks (tan(x)-x, not giac's
// -x+pi*floor(x/pi+1/2)+tan(x)); asin(sin(t)) stays (t only where cos(t) > 0)
static gen invf(const gen & g){
  if (g.type!=_SYMB) return g;
  gen h=map_of(g,invf);
  return is(h,at_atan) && is(arg(h),at_tan)?arg(arg(h)):h;
}
static NOINL gen back(const gen & G,const gen & u,const gen & ux){ return ev(invf(subst(G,u,ux,true,CTX))); }
// the integral of f by the substitution uu = ux (duu = dux dx), g in uu
static NOINL gen subs(const gen & f,const gen & uu,const gen & ux,const gen & dux,const gen & g,int lev){
  size_t m=mark();
  S(R_SUB,uu,ux,dux);
  S(R_SUBEQ,f,g,uu);
  gen G=in_var(g,uu,ux,lev),F=is_undef(G)?G:back(G,uu,ux);
  // checked here: a substitution can be right only on a branch (another method then)
  if (is_undef(F) || !same_num(D(F,X),ev(f),X)){ cut(m); return undef; }
  S(R_BACK,f,F);
  return F;
}

// w = b^e or 1/b^e: w becomes b, e returned (1 for anything else)
static NOINL gen bexp(gen & w){
  gen e=1;
  if (w.is_inv()){ w=arg(w); e=-1; }
  while (w.is_pow()){ e=ev(e*arg(w,1)); w=arg(w,0); } // (giac's x^(2/5) is (x^(1/5))^2)
  return e;
}
// c x^a x^b...: the power rule (or 1/x). Read from the factors: giac's normal of several roots
// (sqrt(x) x^(1/3)) can take minutes.
static NOINL bool power(const gen & g,gen & F,int & r){
  vecteur c,v; factors(g,c,v);
  gen n=0;
  EACH(it,v){
    gen w=*it,e=bexp(w);
    if (w!=X || dep(e)) return false;
    n=n+e;
  }
  n=ev(n);
  if (v.empty() || is_zero(n)) return false;
  gen k=ev(prodof(c));
  r=is_minus_one(n)?R_LNI:R_POWI;
  F=r==R_LNI?k*symb_ln(symb_abs(X)):ev(k*pow(X,n+1,CTX)/(n+1));
  return true;
}

// A formula template (giac syntax) evaluated: A B C K Q W T the values given, in this order, X the
// variable. A formula as a string costs far less flash than the same formula built in C++.
static vecteur * ph_;
static NOINL gen fill(const char * s,const vecteur & v){
  if (!ph_){
    gen l("[A,B,C,K,Q,W,T]",CTX);
    if (l.type!=_VECT) return undef;
    ph_=new vecteur(*l._VECTptr);
  }
  vecteur from(ph_->begin(),ph_->begin()+v.size()),to(v);
  from.push_back(gen(identificateur("X"))); to.push_back(X);
  return subst(gen(s,CTX),from,to,false,CTX);
}
// k (a x+b)^n, a linear inside: (a x+b)^(n+1)/(a(n+1)), or ln|a x+b|/a
static NOINL bool linpow(const gen & g,gen & F,int & r){
  vecteur c,v; factors(g,c,v);
  if (v.size()!=1) return false;
  gen w=v[0],n=bexp(w),a,b;
  if (dep(n) || w==X || !is_linear_wrt(w,X,a,b,CTX) || is_zero(a)) return false;
  r=is_minus_one(n)?R_LNI:R_POWI;
  F=fill(r==R_LNI?"A/C*ln(abs(W))":"A*W^(B+1)/(C*(B+1))",makevecteur(prodof(c),n,a,0,0,w));
  return true;
}

// The table: f is recognized by its values at 0.37, 0.61 and -0.43 (a negative point: |sin x| is
// not sin x), those of the derivatives of these antiderivatives in TABV (computed on the PC:
// differentiating them on the calculator took 2.7 s the first time).
static const char TABLE[]="[exp(x),sin(x),-cos(x),tan(x),-cot(x),sec(x),-csc(x),ln(abs(sec(x))),ln(abs(sin(x))),ln(abs(sec(x)+tan(x))),-ln(abs(csc(x)+cot(x))),atan(x),asin(x),sinh(x),cosh(x),tanh(x),(sec(x)*tan(x)+ln(abs(sec(x)+tan(x))))/2]";
static const double TABV[][3]={
  {1.447734615,1.840431399,0.6505090947},{0.9323273456,0.8196480178,0.9089657497},
  {0.361615432,0.5728674601,-0.4168708024},{1.150437832,1.488487576,1.210333243},
  {7.647264093,3.047134971,5.754360199},{0.4160160736,0.8527060971,-0.5045525903},
  {7.129753433,2.497578139,5.230516333},{0.3878631617,0.6989188623,-0.4586210235},
  {2.578228867,1.430781245,-2.180449541},{1.07258465,1.220035891,1.100151464},
  {2.765368708,1.745604472,-2.398824754},{0.879584836,0.7288098535,0.8439530762},
  {1.076389473,1.261986643,1.107629601},{1.069234473,1.191891134,1.093883309},
  {0.378500142,0.6485402649,-0.4433742144},{0.8746898675,0.7039257003,0.8357146367},
  {1.23394196,1.816008267,1.331549889}};
static const double TABX[3]={0.37,0.61,-0.43};
static vecteur * tabF_;
static NOINL bool table(const gen & f,gen & F){
  if (!tabF_){
    gen l(TABLE,CTX);
    if (l.type!=_VECT) return false;
    tabF_=new vecteur(*l._VECTptr);
  }
  double y[3];
  for (int k=0;k<3;++k){
    gen v=evalf(sub(f,X,TABX[k]),1,CTX);
    if (v.type!=_DOUBLE_) return false;
    y[k]=v._DOUBLE_val;
  }
  for (unsigned i=0;i<tabF_->size();++i){
    int k=0;
    while (k<3 && fabs(y[k]-TABV[i][k])<TOL*(1+fabs(TABV[i][k]))) ++k;
    if (k==3){ F=sub((*tabF_)[i],gen(identificateur("x")),X); return true; }
  }
  return false;
}

// f = k q^e, q = A x^2+B x+C: false if f is not of this form
static NOINL bool quad(const gen & f,gen & k,gen & q,gen & e,gen & A,gen & B,gen & C){
  vecteur c,v; factors(f,c,v);
  if (v.empty()) return false;
  e=0;
  EACH(it,v){ // one power, or giac's q^-2 q^(-1/2) for q^(-5/2)
    gen b=*it,n=bexp(b);
    if (it!=v.begin() && b!=q) return false;
    q=b; e=e+n;
  }
  e=ev(e);
  if (!is_quadratic_wrt(q,X,A,B,C,CTX) || is_zero(A) || dep(A) || dep(B) || dep(C)) return false;
  k=ev(prodof(c)); A=ev(A); B=ev(B); C=ev(C);
  return true;
}
// k (A x^2+C)^e, e = -1 or -1/2: the arctangent, arcsine or logarithm formula
static const char * const QF[]={"A/sqrt(B*C)*atan(X*sqrt(B/C))","A/sqrt(-B)*asin(X*sqrt(-B/C))","A/sqrt(B)*ln(abs(X*sqrt(B)+sqrt(Q)))"};
static NOINL bool quadform(const gen & f,gen & F,int & r){
  gen k,q,e,A,B,C;
  if (!quad(f,k,q,e,A,B,C) || !is_zero(B)) return false;
  bool pa=is_positive(A,CTX),pc=is_positive(C,CTX);
  int i=is_minus_one(e)?(pa && pc?0:-1):e!=gen(-1)/2?-1:!pa && pc?1:pa?2:-1;
  if (i<0) return false;
  r=R_ATAN+i;
  F=fill(QF[i],makevecteur(k,A,C,0,q));
  return true;
}

// a basic integral: power, 1/x, a^x, the table, the quadratic formulas
// the argument of g's first function of x (the exponent of a^(...)), 0 if none
static NOINL gen firstarg(const gen & g){
  if (g.type!=_SYMB) return 0;
  const gen & a=arg(g);
  if (g.is_pow() && !dep(arg(g,0))) return arg(g,1);
  if (a.type==_VECT || g.is_inv() || g.is_neg()) return first_of(g,firstarg);
  return dep(a)?a:gen(0);
}
static bool basic(const gen & f,gen & F,int & r);
// h(a x+b) with h(x) a basic integral: H(a x+b)/a, in one step (no written u = 4x)
static NOINL bool linin(const gen & f,gen & F,int & r){
  gen w=firstarg(f),a,b,t(identificateur(" l"));
  if (is_zero(w) || w==X || !is_linear_wrt(w,X,a,b,CTX)) return false;
  gen h=sub(f,w,t),H;
  if (dep(h)) return false; // x outside a x+b
  gen * old=var_; var_=&t;
  bool ok=basic(h,H,r);
  var_=old;
  if (ok){ F=ev(sub(H,t,w)/a); r=R_TABLE; }
  return ok;
}
static bool basic(const gen & f,gen & F,int & r){
  if (f==X){ F=symb_pow(X,2)/2; r=R_POWI; return true; }
  if (f.type!=_SYMB) return false;
  if (power(f,F,r) || linpow(f,F,r)) return true;
  if (f.is_pow() && !dep(arg(f,0)) && arg(f,1)==X){ F=f/ln(arg(f,0),CTX); r=R_TABLE; return true; }
  if (quadform(f,F,r)) return true;
  r=R_TABLE;
  return taille(f,40)<=32 && (table(f,F) || linin(f,F,r));
}

static gen sc(const gen & g){ return rn(tan2sincos(g,CTX)); } // tan, sec... in sin and cos

// u-substitution candidates: the arguments of functions, the bases of powers, the functions
// themselves; sin proposes cos (odd powers), cos proposes tan and sec
static void addc(vecteur & v,const gen & g0){ gen g=ev(g0); if (dep(g) && g!=X && !equalposcomp(v,g)) v.push_back(g); }
static NOINL void cands(const gen & g,vecteur & v){
  if (g.type!=_SYMB) return;
  const unary_function_ptr & u=g._SYMBptr->sommet;
  const gen & a=arg(g);
  if (a.type==_VECT){
    if (u==at_pow && dep(a[0])){
      if (a[0]!=X) addc(v,a[0]);
      else if (a[1].type!=_INT_) addc(v,g); // sqrt(x)
    }
    EACH(it,*a._VECTptr) cands(*it,v);
    return;
  }
  if (u!=at_neg) addc(v,a);
  if (u==at_inv || u==at_neg){ cands(a,v); return; }
  addc(v,g);
  if (u==at_sin) addc(v,symb_cos(a));
  if (u==at_cos){ addc(v,symb_sin(a)); addc(v,symb_tan(a)); addc(v,symb_inv(symb_cos(a))); }
  if (u==at_tan) addc(v,symb_inv(symb_cos(a)));
  cands(a,v);
}
// u = a k + b with a and b constants: k, t becomes (t - b)/a
static NOINL gen kern(const gen & u,gen & t){
  if (u.is_neg()){ t=-t; return kern(arg(u),t); }
  vecteur c,v,w;
  bool p=u.is_plus();
  if (p){ flat(at_plus,u,w); EACH(it,w) (dep(*it)?v:c).push_back(*it); }
  else factors(u,c,v);
  if (v.size()!=1 || c.empty()) return u;
  t=p?t-symb_plus(c):t/prodof(c);
  return kern(v[0],t);
}
// x as a function of t = u(x), u built from x with + and * constants, powers, exp, ln, sin, cos,
// tan (undef otherwise): u = sqrt(x) gives x = t^2
static NOINL gen xof(const gen & u0,const gen & t0){
  gen t=t0,u=kern(u0,t);
  if (u==X) return t;
  if (u.type!=_SYMB) return undef;
  if (u.is_inv()) return xof(arg(u),symb_inv(t));
  if (is(u,at_exp)) return xof(arg(u),symb_ln(t));
  if (is(u,at_ln)) return xof(arg(u),symb_exp(t));
  if (is(u,at_sin)) return xof(arg(u),SYM(at_asin,t)); // cos(asin(t)) is sqrt(1-t^2)
  if (is(u,at_cos)) return xof(arg(u),SYM(at_acos,t));
  if (is(u,at_tan)) return xof(arg(u),SYM(at_atan,t));
  if (u.is_pow() && !dep(arg(u,1))) return xof(arg(u,0),symb_pow(t,ev(symb_inv(arg(u,1)))));
  return undef;
}
static NOINL bool atrig(const gen & g){ return has_op(g,*at_asin) || has_op(g,*at_acos) || has_op(g,*at_atan); }
// a fractional power in g (skip 2: other than square roots)
static NOINL bool froot(const gen & g,int skip){
  if (g.type!=_SYMB) return false;
  if (g.is_pow() && arg(g,1).type==_FRAC && arg(g,1)._FRACptr->den!=skip) return true;
  const gen & a=arg(g);
  if (a.type==_VECT){ EACH(it,*a._VECTptr) if (froot(*it,skip)) return true; return false; }
  return froot(a,skip);
}
// f dx as g(t) dt with t = u (du = u' dx); undef if it can't. Tried in turn: u replaced by t; the
// trig functions written with u's (sin^2 = 1 - cos^2, sec^2 = 1 + tan^2) and u's function replaced;
// x as a function of t in what is left (u = sqrt(x): x = t^2).
static NOINL gen rewrite(const gen & fe,const gen & du,const gen & u,const gen & t){
  gen h=ev(fe/du),kt=t,k=kern(u,kt),g;
  // u linear: u replaced by t, then x by (t-b)/a in what is left, quoted (giac's evaluating subst
  // rebuilt (2x+1)^2026 by pow, which gives up on a sum to that power; x alone in x^2 sqrt(3x-1)
  // left sqrt(3 (1+t)/3-1))
  if (k==X) g=rn(ev(quotesubst(quotesubst(h,u,t,CTX),X,kt,CTX)));
  else g=rn(complex_subst(h,u,t,CTX));
  if (dep(g)){
    if (k.is_inv()){ k=arg(k); kt=symb_inv(kt); } // sec, csc
    gen s=is(k,at_sin)?_trigsin(h,CTX):is(k,at_cos)?_trigcos(h,CTX):is(k,at_tan)?_trigtan(h,CTX):sc(h);
    g=rn(complex_subst(s,is(k,at_tan)?k:sc(k),kt,CTX));
  }
  if (dep(g)){ // x = u^-1(t)
    gen xs=xof(u,t);
    if (!is_undef(xs)) g=rn(exp2pow(ev(sub(g,X,xs)),CTX)); // (e^(2 ln u) is u^2)
    // an inverse trig function, a log or an exponential left: x itself was there (x sin x with
    // u = cos x, x^2 e^(-2x) with u = e^(-2x), x^2 ln(x) with u = ln(x)), not a substitution (by
    // parts); x = e^t stays for sin(ln(x)) (then by parts twice, as textbooks do it)
    if ((atrig(g) && !atrig(fe)) || (has_op(g,*at_ln) && !has_op(fe,*at_ln))
        || (has_op(g,*at_exp) && !has_op(fe,*at_exp) && !has_op(fe,*at_sin) && !has_op(fe,*at_cos))) return undef;
  }
  if (dep(g) || is_undef(g) || has_op(g,*at_rootof) || taille(g,200)>2*taille(fe,200)+20) return undef;
  // only if simpler: it writes 1/sqrt(q) as sqrt(q)/q
  if (nrad(g)>1) return g;
  gen r=recursive_normal(g,CTX);
  return has_op(r,*at_rootof) || taille(r,200)>=taille(g,200)?g:r;
}
// u under a root or in a denominator (x sqrt(x+2), x/(x+1)^2): a linear u is the textbook way
static NOINL bool rooted(const gen & g,const gen & u){
  if (g.type!=_SYMB) return false;
  const gen & a=arg(g);
  if (g.is_pow() && a[0]==u && !(a[1].type==_INT_ && a[1].val>0)) return true;
  if (g.is_inv() && (a==u || (a.is_pow() && arg(a,0)==u))) return true;
  if (a.type==_VECT){ EACH(it,*a._VECTptr) if (rooted(*it,u)) return true; return false; }
  return rooted(a,u);
}
// a factor u^n of g, n >= 3 (u = 0: of any linear sum a x+b)
static NOINL bool upow(const gen & g,const gen & u){
  vecteur c,v; factors(g,c,v);
  EACH(it,v){ gen w=*it,e=bexp(w),a,b; if (e.type==_INT_ && e.val>=3 && (is_zero(u)?w.is_plus() && is_linear_wrt(w,X,a,b,CTX):w==u)) return true; }
  return false;
}
static NOINL gen trigsub(const gen & f,int lev);
// x in g only in powers x^j, j a multiple of k (g a function of x^k)
static NOINL bool ofpow(const gen & g,int k){
  if (g==X) return false;
  if (g.type!=_SYMB) return true;
  if (g.is_pow() && arg(g,0)==X) return arg(g,1).type==_INT_ && arg(g,1).val%k==0;
  const gen & a=arg(g);
  if (a.type==_VECT){ EACH(it,*a._VECTptr) if (!ofpow(*it,k)) return false; return true; }
  return ofpow(a,k);
}
static NOINL gen usub(const gen & f,int lev){
  if (nsub_>3) return undef;
  bool deep=nsub_>1; // two substitutions deep at most (u inside u), then only linear ones
  gen uu=gen(identificateur(UNAMES[nsub_])),fe=ev(f);
  vecteur cv; cands(fe,cv); // (evaluated: sqrt(x) is x^(1/2) there, as in fe)
  { // a factor x^m with a function of x^(m+1): u = x^(m+1), x/(x^4+1), x^4/(16+x^10)
    vecteur c,v; factors(fe,c,v);
    if (v.size()>1) EACH(it,v){
      gen b=*it,m=bexp(b);
      if (b!=X || m.type!=_INT_ || m.val<1) continue;
      bool ok=true;
      EACH(jt,v) if (jt!=it && !ofpow(*jt,m.val+1)) ok=false;
      if (ok) addc(cv,symb_pow(X,m+1));
    }
  }
  // every candidate first: a linear u giving a known integral, then the simplest new integral
  vecteur ok,gs; std::vector<int> sz;
  for (const_iterateur it=cv.begin();it!=cv.end() && budget_>0 && !interrupted;++it){
    gen u=*it,du=D(u,X),F,a,b; int r;
    budget_-=2;
    if (is_zero(du)) continue;
    bool lin=is_linear_wrt(u,X,a,b,CTX);
    if (deep && !lin) continue;
    gen g=rewrite(fe,du,u,uu);
#ifdef HOSTGIAC
    if (getenv("STEPDBG")) fprintf(stderr,"%*s  candidate u=%s: %s\n",2*lev,"",u.print(CTX).c_str(),g.print(CTX).c_str());
#endif
    if (is_undef(g) || is_inf(g) || is_zero(g)) continue;
    // the new integral in the table: taken at once (a textbook substitution lands there; the other
    // candidates can cost giac minutes, x = u^(1/4) in roots...). A linear u otherwise only under
    // roots (x sqrt(x+2)), else the simplest new integral; a root of something else than u left
    // (sqrt(u-4)) after the trig substitution
    const bool nr=froot(g,0) && !froot(fe,0); // a root that was not there: after the others
    if (nr && shape(fe,SH_FRAC)) continue; // (never for a rational fraction)
    vecteur c2,v2; gen * old=var_; var_=&uu; factors(g,c2,v2);
    bool known=!nr && v2.size()==1 && basic(ev(v2[0]),F,r);
    int s=known?-1:int(taille(g,1000))+(is_zero(radicand(g))?0:10000)+(nr?5000:0);
    var_=old;
    // a linear u: in a polynomial for u^n, n >= 3 (x (2x+1)^5, 8x (2x+1)^2026: expanding is long or
    // impossible); never in a rational fraction (partial fractions); else under a root or in a
    // denominator. Kept, it is the first one tried.
    const bool lu=lin && !known;
    if (lu && (shape(fe,0)?!upow(fe,u):shape(fe,SH_FRAC) || !rooted(fe,u))) continue;
    if (known){ // tried at once; right only on a branch (u = sqrt(q)): the next candidates
      gen G=subs(f,uu,u,du,g,lev);
      if (!is_undef(G)) return G;
      continue;
    }
    ok.push_back(u); gs.push_back(g); sz.push_back(lu?0:s);
  }
  for (bool ts=false;;){
    unsigned b=0;
    for (unsigned i=1;i<ok.size();++i) if (sz[i]<sz[b]) b=i;
    if (!ts && (ok.empty() || sz[b]>=5000)){ // (a root left by them all)
      ts=true; gen F=trigsub(f,lev);
      if (!is_undef(F)) return F;
    }
    if (ok.empty()) return undef;
    gen u=ok[b],g=gs[b]; int s=sz[b]%5000; // (the penalties off)
    ok.erase(ok.begin()+b); gs.erase(gs.begin()+b); sz.erase(sz.begin()+b);
    if (s>int(taille(fe,1000))+6) continue; // not simpler (taille is unsigned)
    gen F=subs(f,uu,u,D(u,X),g,lev);
    if (!is_undef(F)) return F;
  }
}

// LIATE rank of a factor (lower: the better u)
static NOINL bool linarg(const gen & g){ gen a,b; return g.type==_SYMB && is_linear_wrt(arg(g),X,a,b,CTX); }
static NOINL int liate(const gen & g0){
  gen g=g0;
  if (g.is_pow() && arg(g,1).type==_INT_ && arg(g,1).val>0) g=arg(g,0); // ln(x)^2
  if (is(g,at_ln)) return 0;
  if (is(g,at_atan) || is(g,at_asin) || is(g,at_acos)) return 1;
  if (shape(g0,0)) return 2; // a polynomial
  if ((is(g0,at_sin) || is(g0,at_cos) || is(g0,at_sinh) || is(g0,at_cosh)) && linarg(g0)) return 3;
  if (is(g0,at_exp) && linarg(g0)) return 4;
  if (g0.is_pow() && !dep(arg(g0,0)) && !dep(D(arg(g0,1),X))) return 4;
  return 9;
}
// by parts: u by LIATE, v the integral of dv (found quietly), rest = v du, k the constant
static NOINL bool parts1(const gen & f,int lev,gen & u,gen & dv,gen & du,gen & V,gen & k,gen & rest){
  gen fe=ev(f);
  if (shape(fe,SH_FRAC)) return false; // a rational fraction: partial fractions
  vecteur c,v,w; factors(fe,c,v);
  int ne=0; // 1/e^u is e^(-u) when no other factor has e^ (x^2/e^x: u = x^2, dv = e^(-x) dx)
  EACH(it,v) ne+=has_op(*it,*at_exp);
  if (ne==1) EACHW(it,v) if (it->is_inv() && is(arg(*it),at_exp)) *it=SYM(at_exp,ev(-arg(arg(*it))));
  const_iterateur bi=v.end();
  int br=9;
  EACH(it,v){ int r=liate(*it); if (r<br){ br=r; bi=it; } }
  if (br==9 || (v.size()==1 && br>1)) return false;
  EACH(it,v) if (it!=bi) w.push_back(*it); // dv: the other factors (x sec(x) tan(x))
  u=*bi; dv=prodof(w); du=D(u,X);
  int o=liate(dv);
  // trig times trig: identities; a polynomial times another function: x only (x sec(x)^2)
  if ((br==3 && o==3) || o<br || (o>4 && (br>2 || (br==2 && dep(du))))) return false;
  size_t m=mark(); unsigned char d=depth_; int b=budget_;
  V=isteps(dv,maxlev_-1); // (a substitution at most)
  cut(m); depth_=d; budget_=b-4;
  if (is_undef(V)) return false;
  V=ev(V); k=ev(prodof(c)); rest=rn(V*du);
  return true;
}
// h when g is -h (-cos(x), -2*x*e^x): u v - integral of (-h) is written u v + integral of h, as
// in the textbooks; else undef
static NOINL gen negpart(const gen & g){
  if (is(g,at_neg)) return arg(g);
  vecteur c,v; factors(g,c,v);
  return !c.empty() && is_strictly_positive(-ev(prodof(c)),CTX)?rn(-g):undef;
}
// -g written h for g = -h (F(b) - F(a) as F(b) + h: 1 + 1, not 1 - -1)
static NOINL gen negof(const gen & g){ gen h=negpart(g); return is_undef(h)?symb_neg(g):h; }
static NOINL void parts_steps(const gen & f,const gen & u,const gen & dv,const gen & du,const gen & V,const gen & k,const gen & rest){
  S(R_PARTS,u,dv); S(R_PARTSV,du,V);
  gen n=negpart(rest);
  S(R_PARTS2,f,k*(is_undef(n)?u*V-Ix(rest):u*V+Ix(n)));
}
static NOINL gen parts(const gen & f,int lev){
  gen u,dv,du,V,k,rest;
  if (!parts1(f,lev,u,dv,du,V,k,rest)) return undef;
  size_t m=mark();
  parts_steps(f,u,dv,du,V,k,rest);
  // e^x sin x: a second round brings the integral back, ratio times
  gen u2,dv2,du2,V2,k2,rest2;
  if (parts1(rest,lev+1,u2,dv2,du2,V2,k2,rest2)){
    gen ratio=rn(sc(k2*rest2)/sc(ev(f)));
    if (!dep(ratio) && !is_zero(ratio) && !is_zero(ev(1-k*ratio))){
      ++depth_; parts_steps(rest,u2,dv2,du2,V2,k2,rest2); --depth_;
      gen I=ev(k*(u*V-k2*u2*V2)/(1-k*ratio));
      S(R_CYCLE,f,I);
      return I;
    }
  }
  gen n=negpart(rest);
  ++depth_; gen R=isteps(is_undef(n)?rest:n,lev+1); --depth_;
  if (is_undef(R)){ cut(m); return R; }
  gen F=ev(k*(is_undef(n)?u*V-R:u*V+R));
  S(R_EQI,f,F);
  return F;
}

// 1 if g holds a sum with a fraction (x-1/2), else 0
static NOINL gen fracin(const gen & g){
  if (g.is_plus() && arg(g).type==_VECT) EACH(it,*arg(g)._VECTptr) if (it->type==_FRAC) return 1;
  return first_of(g,fracin);
}
// partial fractions, a term over x-1/2 as over 2x-1 (1/(4(x-1/2)): 1/(2(2x-1)))
static NOINL gen pfrac(const gen & g){
  gen p=_partfrac(makesequence(g,X),CTX);
  if (!p.is_plus() || arg(p).type!=_VECT) return p;
  vecteur t=*arg(p)._VECTptr;
  EACHW(it,t) if (!is_zero(fracin(*it))) *it=_factor(*it,CTX);
  return symb_plus(t);
}
// a rational fraction: partial fractions (and the polynomial part)
static NOINL gen ratfrac(const gen & fe,int lev){
  if (!shape(fe,SH_FRAC) || !dep(_denom(fe,CTX))) return undef;
  gen p=pfrac(fe);
  if (is_undef(p) || p==fe || !p.is_plus()) return undef;
  vecteur t; flat(at_plus,p,t); // a polynomial and one fraction (x^2/(x^2+1)): long division
  int k=0; EACH(it,t) if (dep(_denom(*it,CTX))) ++k;
  return via(k>1?R_PFRAC:R_LDIVQ,fe,p,lev);
}
// (a x+b)/q^n, q = A x^2+B x+C: a/(2A) q'/q^n plus the rest (a logarithm and an arctangent)
static NOINL gen splitnum(const gen & fe,int lev){
  gen n=_numer(fe,CTX),d=_denom(fe,CTX),a,b,q=d,A,B,C;
  if (q.is_pow()) q=arg(q,0);
  if (!is_linear_wrt(n,X,a,b,CTX) || is_zero(a) || dep(a) || dep(b)
      || !is_quadratic_wrt(q,X,A,B,C,CTX) || is_zero(A) || dep(A) || dep(B) || dep(C) || is_zero(ev(b-a*B/(2*A)))) return undef;
  return via(R_SPLIT,fe,fill("A/(2*B)*(2*B*X+C)/Q+(K-A*C/(2*B))/Q",makevecteur(a,A,B,b,d)),lev);
}
// sin^m cos^n with m and n even: the identities first (odd powers: a substitution)
static NOINL bool even_trig(const gen & fe){
  vecteur c,v; factors(fe,c,v);
  if (v.empty()) return false;
  EACH(it,v){
    gen b=*it; int n=1;
    if (b.is_pow() && arg(b,1).type==_INT_){ n=arg(b,1).val; b=arg(b,0); }
    if ((!is(b,at_sin) && !is(b,at_cos)) || n%2) return false;
  }
  return true;
}
// powers and products of sin and cos: power reduction, product to sum; times a polynomial
// (x sin(x)^2, x sin(x) cos(x)): then the terms one by one (by parts)
static NOINL gen trigid(const gen & f,int lev){
  gen fe=sc(ev(f)),p=1,t=1;
  vecteur c,v; factors(fe,c,v);
  EACH(it,v) if (shape(*it,0)) p=p**it; else t=t**it;
  if (is_one(p)) t=fe; // (as tlin writes it: 1/2-1/2*cos(2x))
  if (!shape(t,SH_TRIG)) return undef; // a polynomial in sin and cos
  gen l=_tlin(t,CTX);
  if (l==t || is_undef(l)) return undef;
  if (!is_one(p)) l=expand(ev(prodof(c)*p*l),CTX);
  return via(R_TRIGID,fe,l,lev);
}
// a fraction of sin x and cos x: 1/(1+-sin x) and 1/(1+-cos x) times the conjugate, else the
// substitution t = tan(x/2)
static NOINL gen trigfrac(const gen & f,int lev){
  gen fe=sc(ev(f)),d=_denom(fe,CTX);
  if (!shape(fe,SH_TRIG|SH_FRAC) || !dep(d) || nsub_>3) return undef; // (sin and cos of x: else t stays)
  gen w(identificateur(" w"));
  for (int k=0;k<2;++k){ // d = p + q s, p^2 = q^2: times (p - q s), 1 - s^2 is the other one squared
    gen s=k?symb_cos(X):symb_sin(X),p,q;
    if (!is_linear_wrt(sub(d,s,w),w,q,p,CTX) || dep(p) || dep(q) || is_zero(q) || !is_zero(ev(p*p-q*q))) continue;
    return via(R_CONJ,fe,fill("expand(A*(B-C*Q))/(B^2*W^2)",makevecteur(_numer(fe,CTX),p,q,0,s,k?symb_sin(X):symb_cos(X))),lev);
  }
  gen t(identificateur("t"));
  gen g=fill("ratnormal(subst(subst(A,sin(X),2*T/(1+T^2)),cos(X),(1-T^2)/(1+T^2))*2/(1+T^2))",makevecteur(fe,0,0,0,0,0,t));
  if (dep(g)) return undef;
  size_t m=mark();
  S(R_WEIER,f,g,t);
  gen G=in_var(g,t,symb_tan(X/2),lev);
  if (is_undef(G)){ cut(m); return G; }
  gen F=ev(sub(G,t,symb_tan(X/2)));
  S(R_BACK,f,F);
  return F;
}
// tan(a)^n, |n| >= 2: tan^2 = sec^2 - 1 (cot: csc^2 - 1); alone: the power by itself (tan(x)^4
// by the identity, as the textbooks; tan^m sec^n keeps u = tan x)
static NOINL gen pyth(const gen & fe,int lev,bool alone=false){
  vecteur c,v; factors(fe,c,v);
  if (alone && v.size()!=1) return undef;
  EACH(it,v){
    gen w=*it,e=bexp(w);
    if (is(w,at_cot)){ w=symb_tan(arg(w)); e=-e; } // cot^n is tan^-n
    if (!is(w,at_tan) || e.type!=_INT_) continue;
    int n=e.val;
    if (n<2 && n>-2) continue;
    vecteur r(c); EACH(jt,v) if (jt!=it) r.push_back(*jt); // the other factors
    return via(R_PYTH,fe,fill(n>0?"expand(A*tan(B)^C*(1/cos(B)^2-1))":"expand(A*tan(B)^C*(1/sin(B)^2-1))",makevecteur(prodof(r),arg(w),n>0?n-2:n+2)),lev);
  }
  return undef;
}
// k (A x^2+B x+C)^e with B != 0: complete the square (then a linear substitution)
// (then u = x+B/(2A), written: the textbook way, and a trig substitution can follow)
static NOINL gen square(const gen & fe,int lev){
  gen k,q,e,A,B,C;
  if (!quad(fe,k,q,e,A,B,C) || is_zero(B) || nsub_>3) return undef;
  gen sq=fill("A*(X+B/(2*A))^2+(C-B^2/(4*A))",makevecteur(A,B,C)),uu=gen(identificateur(UNAMES[nsub_]));
  size_t m=mark();
  S(R_SQUARE,q,sq);
  gen F=subs(k*symb_pow(sq,e),uu,ev(X+B/(2*A)),1,k*symb_pow(ev(fill("A*T^2+C-B^2/(4*A)",makevecteur(A,B,C,0,0,0,uu))),e),lev);
  if (is_undef(F)) cut(m);
  return F;
}
// a product of sums of powers of x (no rational fraction): expanding gives powers
static bool powers_only(const gen & fe){ return !fe.is_plus() && shape(fe,SH_ROOTX); }
static NOINL gen expandit(const gen & fe,int lev){
  if (fe.is_plus() || taille(fe,64)>48) return undef;
  gen t=expand(fe,CTX);
  if (!t.is_plus()) return undef;
  return via(R_EXPAND,fe,t,lev);
}
// (a+b)/c as a/c+b/c, c as it is (expand wrote (x^2+1)^2 as x^4+2x^2+1)
static NOINL gen splitfrac(const gen & fe,int lev){
  vecteur c,v,w,t; factors(fe,c,v);
  EACH(it,v) if (it->is_plus()){
    vecteur r(c); EACH(jt,v) if (jt!=it) r.push_back(*jt); // the other factors
    flat(at_plus,*it,w);
    EACH(kt,w){ // each term simplified when that is shorter (4e^(2x)/e^x is 4e^x)
      vecteur q(1,*kt); q.insert(q.end(),r.begin(),r.end()); // (the term first: sin(u)/x)
      gen a=prodof(q),b=expand(a,CTX);
      t.push_back(taille(b,64)<taille(a,64)?b:a);
    }
    return via(R_SPLIT,fe,symb_plus(t),lev);
  }
  return undef;
}
// a shorter integrand: tan(x)(cot(x)-cos(x)) is 1-sin(x) (f shown as typed: giac's evaluation
// of sqrt(1-cos(2x)) is already sqrt(2)abs(sin(x)), and the step read A = A)
static NOINL gen simp(const gen & f,const gen & fe,int lev){
  if (taille(fe,64)>48) return undef;
  gen t=_simplify(fe,CTX);
  if (t==fe || is_undef(t) || taille(t,64)>=taille(fe,64)) return undef;
  return via(R_SIMP,f,t,lev);
}
// the square roots and their powers q^(k/2) of g as powers of root (root^2 = q)
static NOINL gen radsub(const gen & g,const gen & q,const gen & root){
  if (g.type!=_SYMB) return g;
  if (g.is_pow() && arg(g,0)==q){
    gen e=arg(g,1);
    if (e.type==_INT_ || (e.type==_FRAC && e._FRACptr->den==2)) return pow(root,ev(2*e),CTX);
  }
  gen a=arg(g);
  if (a.type==_VECT){
    vecteur v=*a._VECTptr;
    EACHW(it,v) *it=radsub(*it,q,root);
    a=gen(v,a.subtype);
  }
  else a=radsub(a,q,root);
  return symbolic(g._SYMBptr->sommet,a);
}
// the first q of g under a square root (q^(k/2)), not x itself (0: none; lvarx gives x for
// sqrt(1-x^2))
static NOINL gen radicand(const gen & g){
  if (g.type!=_SYMB) return 0;
  if (g.is_pow() && arg(g,1).type==_FRAC && arg(g,1)._FRACptr->den==2 && dep(arg(g,0)) && arg(g,0)!=X) return arg(g,0);
  return first_of(g,radicand);
}
// sqrt(C-A x^2), sqrt(A x^2+C), sqrt(A x^2-C): x = a sin t, a tan t, a sec t; back to x with
// the right triangle (sin t and cos t as fractions of x). Each: x, sqrt(q), sin t, cos t, t.
static const char * const TSUB[]={"[A*sin(T),sqrt(C)*cos(T),X/A,sqrt(Q)/sqrt(C),asin(X/A)]",
  "[A*tan(T),sqrt(C)/cos(T),X*sqrt(B)/sqrt(Q),sqrt(C)/sqrt(Q),atan(X/A)]",
  "[A/cos(T),sqrt(-C)*tan(T),sqrt(Q)/(X*sqrt(B)),A/X,atan(sqrt(Q)/sqrt(-C))]"};
// g = k x^j q^-n, n >= 2: q (0 if not): (x^2+a^2)^-n by x = a tan t (a rational function with
// other factors: partial fractions first)
static NOINL gen qpow(const gen & g){
  vecteur c,v; factors(g,c,v);
  gen q=0;
  EACH(it,v){
    gen b=*it,n=bexp(b);
    if (b==X) continue;
    if (n.type!=_INT_ || n.val>-2 || (!is_zero(q) && b!=q)) return 0;
    q=b;
  }
  return q;
}
static NOINL gen trigsub(const gen & f,int lev){
  if (nsub_>3) return undef;
  gen fe=ev(f),q=radicand(fe),A,B,C;
  bool r=!is_zero(q);
  if (!r) q=qpow(fe);
  if (is_zero(q) || !is_quadratic_wrt(q,X,A,B,C,CTX) || !is_zero(B) || is_zero(A) || dep(A) || dep(C)) return undef;
  A=ev(A); C=ev(C);
  bool pa=is_positive(A,CTX),pc=is_positive(C,CTX);
  int k=pc?(pa?1:0):(pa?2:-1);
  if (k<0 || (!r && k!=1)) return undef; // (a^2-x^2)^-n: partial fractions
  gen t(identificateur("t"));
  gen W=fill(TSUB[k],makevecteur(sqrt(ev(k==1?C/A:-C/A),CTX),A,C,0,q,0,t));
  if (W.type!=_VECT || W._VECTptr->size()<5) return undef;
  const vecteur & w=*W._VECTptr;
  gen dx=D(w[0],t),g=sc(ev(sub(radsub(fe,q,w[1]),X,w[0])*dx)),g2=_trigcos(g,CTX);
  if (taille(g2,200)<taille(g,200)) g=g2; // sin^2 + cos^2 = 1
  if (dep(g)) return undef;
  size_t m=mark();
  S(R_TSUB,t,w[0],dx);
  S(R_TSUBR,SYM2(at_pow,q,gen(1)/2),w[1]);
  S(R_TSUBEQ,f,g,t);
  gen G=in_var(g,t,w[4],lev);
  if (is_undef(G)){ cut(m); return G; }
  { gen * old=var_; var_=&t; S(R_TRI,w[2],w[3]); var_=old; } // sin(2t) = 2 sin t cos t...
  // sec t, tan t... straight from the triangle: (x sqrt(x^2+1)+ln(x+sqrt(x^2+1)))/2, not one fraction
  gen F=ev(sub(sub(sub(sub(_texpand(G,CTX),symb_tan(t),rn(w[2]/w[3])),symb_sin(t),w[2]),symb_cos(t),w[3]),t,w[4])),F2=rn(F);
  if (taille(F2,200)<taille(F,200)) F=F2;
  S(R_EQI,f,F);
  return F;
}

// (a+b)/c split in fractions, unless a+b is c's derivative (times a constant): (2x+2)/sqrt(x^2+2x+5)
// A rational fraction only over one power of an irreducible quadratic (else partial fractions).
// (Read from f's factors: giac's denominator of the evaluated (x-2)/(x^2+1)^2 is x^4+2x^2+1.)
static NOINL bool splits(const gen & f,bool rat){
  vecteur c,v; factors(f,c,v);
  gen n=0,d=0,A,B,C;
  int nd=0;
  EACH(it,v){ gen b=*it,e=bexp(b); if (it->is_plus()) n=b; else if (is_positive(-e,CTX)){ d=b; ++nd; } }
  if (!n.is_plus() || !dep(d) || !dep(rn(n/D(d,X)))) return false;
  return !rat || (nd==1 && is_quadratic_wrt(d,X,A,B,C,CTX) && !is_zero(A) && is_strictly_positive(ev(4*A*C-B*B),CTX));
}
static gen absarg(const gen & f);
static gen isteps(const gen & f0,int lev){
  if (lev>maxlev_){ cutlev_=true; return undef; }
  if (--budget_<0 || ctrl_c || interrupted) return undef;
  gen f=norm(f0),F;
#ifdef HOSTGIAC
  if (getenv("STEPDBG")) fprintf(stderr,"%*sisteps %d budget %d in %s: %s\n",2*lev,"",lev,budget_,X.print(CTX).c_str(),f.print(CTX).c_str());
#endif
  if (lo_){ // |g| keeping its sign on the interval: g or -g (else no steps)
    gen g=absarg(f);
    if (!is_zero(g)){
      int s=sampled(g,17);
      if (!s) return undef;
      bool ab=absi_;
      absi_=true;
      F=via(R_ABSI,f,sub(f,symb_abs(g),s>0?g:-g),lev);
      if (is_undef(F)) absi_=ab;
      return F;
    }
  }
  if (!dep(f)){ F=f*X; S(R_CONSTANT,f,F); return F; }
  if (f.is_plus() && arg(f).type==_VECT){ // term by term
    const vecteur & v=*arg(f)._VECTptr;
    vecteur iv,rv;
    EACH(it,v) iv.push_back(it->is_neg()?symb_neg(Ix(arg(*it))):Ix(*it)); // (-h: its card is h's)
    size_t m=mark();
    S(R_SUMI,f,symb_plus(iv));
    ++depth_;
    EACH(it,v){
      gen G=it->is_neg()?ev(-isteps(arg(*it),lev+1)):isteps(*it,lev+1); // (-h: -integral of h, shown)
      if (is_undef(G)){ --depth_; cut(m); return G; }
      rv.push_back(G);
    }
    --depth_;
    F=ev(symb_plus(rv));
    S(R_EQI,f,F);
    return F;
  }
  if (f.is_neg()){ // -h: the minus out, shown (the card after integrates h)
    size_t m=mark();
    S(R_CONSTI,f,-1,arg(f));
    F=isteps(arg(f),lev);
    if (is_undef(F)){ cut(m); return F; }
    F=ev(-F); S(R_EQI,f,F);
    return F;
  }
  vecteur c,v; factors(f,c,v);
  gen k=ev(prodof(c));
  int r;
  if (!c.empty() && !v.empty()){
    gen g=prodof(v),G;
    if (is_minus_one(k)){ F=isteps(g,lev); return is_undef(F)?F:ev(-F); } // -g: as -(...), no step
    if (!is_one(k)){ // c times the integral of g
      if (basic(ev(g),G,r)){ F=ev(k*G); S(r,f,F); return F; } // 3x^2: one step
      size_t m=mark();
      S(R_CONSTI,f,k,g);
      G=isteps(g,lev);
      if (is_undef(G)){ cut(m); return G; }
      F=ev(k*G);
      S(R_EQI,f,F);
      return F;
    }
    f=g;
  }
  gen fe=ev(f);
  if (basic(fe,F,r)){ S(r,f,F); return F; }
  if (even_trig(fe) && !is_undef(F=trigid(f,lev))) return F;
  if (powers_only(fe)){ // expanded, unless that makes it long: x^2 (x^3+1)^5 is u = x^3+1, x (2x+1)^5
    // is u = 2x+1 (a power >= 3 of a linear sum, rooted); giac's degree expands the powers: not
    // for those (1.25 MB of heap for 8x (2x+1)^2026: the calculator ran out of memory and quit)
    const bool big=bigp(fe);
    gen d=big || upow(fe,0)?gen(99):_degree(makesequence(fe,X),CTX);
    if ((d.type!=_INT_ || d.val>6) && !is_undef(F=usub(f,lev))) return F;
    if (big) return undef;
    if (!is_undef(F=expandit(fe,lev))) return F;
  }
  if (splits(f,shape(fe,SH_FRAC)) && (!is_undef(F=splitnum(fe,lev)) || !is_undef(F=splitfrac(f,lev)))) return F;
  { // a power of tan alone: the identity, in a deeper round if needed (u = tan x is shorter)
    bool cl=cutlev_; cutlev_=false;
    F=pyth(f,lev,true); // (as typed: giac makes cot(x)^2 (cos(x)/sin(x))^2)
    if (!is_undef(F) || cutlev_) return F;
    cutlev_=cl;
  }
  if (!is_undef(F=usub(f,lev))) return F;
  if (!is_undef(F=ratfrac(fe,lev))) return F;
  if (!is_undef(F=splitnum(fe,lev))) return F;
  if (!is_undef(F=pyth(fe,lev))) return F;
  if (!is_undef(F=parts(f,lev))) return F;
  if (!is_undef(F=trigid(f,lev))) return F;
  if (!is_undef(F=square(fe,lev))) return F;
  if (!is_undef(F=expandit(fe,lev))) return F;
  if (!is_undef(F=trigfrac(f,lev))) return F;
  return simp(f,fe,lev);
}

// ---------------------------------------------------------------- entry
static bool calc_op(const gen & g){ return is(g,at_derive) || is(g,at_integrate) || is(g,at_limit) || is(g,at_series) || is(g,at_sum); }
// d/dx of an integral with x in its bounds: f(b) b' - f(a) a' (the fundamental theorem)
static bool ftc1able(const gen & g){
  if (!is(g,at_derive) || arg(g).type!=_VECT || arg(g)._VECTptr->size()!=2) return false;
  const vecteur & w=*arg(g)._VECTptr;
  if (!is(w[0],at_integrate) || arg(w[0]).type!=_VECT || arg(w[0])._VECTptr->size()!=4) return false;
  const vecteur & u=*arg(w[0])._VECTptr;
  return dep(u[2],w[1]) || dep(u[3],w[1]);
}
// the first calculus operation of g with none inside it (innermost first), or 0
static gen inner(const gen & g){
  if (g.type!=_SYMB) return 0;
  if (ftc1able(g)) return g; // the fundamental theorem at once
  gen r=first_of(g,inner);
  return !is_zero(r)?r:calc_op(g)?g:gen(0);
}
// no antiderivative in elementary functions: giac's has an integral or a special function left
static bool elementary(const gen & G){
  static const char * const special[]={"erf","Si","Ci","Ei","integrate","LambertW","gammad","Psi","Zeta","sum(",0};
  if (is_undef(G)) return false;
  std::string s=G.print(CTX);
  for (int i=0;special[i];++i) if (strstr(s.c_str(),special[i])) return false;
  return true;
}
// sqrt(p^2 r) = |p| sqrt(r) for the first square root of f with a square in its radicand, factored
// with cos^2 = 1 - sin^2, or sin^2 = 1 - cos^2, or as it is (f itself if none)
static NOINL gen sqout(const gen & f){
  gen q=radicand(f);
  if (is_zero(q)) return f;
  for (int k=0;k<3;++k){
    gen q2=_factor(k==2?q:k?_trigcos(q,CTX):_trigsin(q,CTX),CTX),out=1,in=1;
    vecteur c,v; factors(q2,c,v);
    EACH(it,v){
      gen b=*it; int e=1;
      if (b.is_pow() && arg(b,1).type==_INT_){ e=arg(b,1).val; b=arg(b,0); }
      if (e>=2) out=out*pow(symb_abs(b),e/2,CTX);
      if (e%2) in=in*b;
    }
    if (out!=1) return ev(radsub(f,q,out*sqrt(ev(prodof(c)*in),CTX)));
  }
  return f;
}
static gen one(const gen & op);
// the pieces' total is giac's integral (else a zero of g was missed: no steps rather than wrong ones)
static NOINL bool absame(const gen & tot,const gen & op){
  const gen d=evalf(tot-ev(op),1,CTX),r=evalf(tot,1,CTX);
  return d.type==_DOUBLE_ && r.type==_DOUBLE_ && fabs(d._DOUBLE_val)<=TOL*(1+fabs(r._DOUBLE_val));
}
// integrate(f,x,a,b) with |g| in f: one integral per interval where g keeps its sign. |g| alone
// (area between curves): g simplified, one antiderivative, each interval evaluated with it.
static NOINL gen absplit(const gen & op,const gen & g0,const gen & a,const gen & b){
  gen f=arg(op,0),g=rn(g0);
  if (taille(g,200)>=taille(g0,200)) g=g0;
  bool alone=f==symb_abs(g0);
  gen z=_solve(makesequence(symb_equal(g,0),X),CTX);
  vecteur pts(1,a);
  // giac's zeros, and for a periodic g the same ones a multiple of pi away where g vanishes too
  // (giac's solve gives cos(t)=0 at pi/2 only; on [0,3pi] the integral took one piece for five)
  const bool per=!is_inf(a) && !is_inf(b) && (has_op(g,*at_sin) || has_op(g,*at_cos) || has_op(g,*at_tan));
  if (z.type==_VECT)
    EACH(it,*z._VECTptr){
      const gen zi=ev(*it);
      if (evalf(zi,1,CTX).type!=_DOUBLE_) continue;
      const int k0=per?(int)floor(evalf_double((a-zi)/cst_pi,1,CTX)._DOUBLE_val):0;
      for (int k=k0;k<=k0+(per?60:0);++k){
        const gen c=per?rn(zi+k*cst_pi):zi; // (3pi/2, not pi/2+pi; -pi/2+pi is pi/2 again)
        if (!is_strictly_greater(b,c,CTX)) break;
        if (is_strictly_greater(c,a,CTX) && !equalposcomp(pts,c) && (!per || is_zero(c-zi) || fabs(evalf_double(sub(g,X,c),1,CTX)._DOUBLE_val)<1e-6)) pts.push_back(c);
      }
    }
  pts.push_back(b);
  gen ps=_sort(pts,CTX);
  if (ps.type!=_VECT) return undef;
  pts=*ps._VECTptr;
  vecteur pieces,sg;
  for (unsigned i=0;i+1<pts.size();++i){
    bool pos=is_positive(evalf(sub(g,X,(pts[i]+pts[i+1])/2),1,CTX),CTX);
    sg.push_back(pos?1:-1);
    gen I=symbolic(at_integrate,makesequence(alone?g:sub(f,symb_abs(g0),pos?g0:-g0),X,pts[i],pts[i+1]));
    pieces.push_back(alone && !pos?symb_neg(I):I);
  }
  S(pieces.size()>1?R_ABS:R_ABSI,op,symb_plus(pieces)); // (one piece: the sign on the interval)
  gen tot=0;
  if (alone){
    gen I=symbolic(at_integrate,makesequence(g,X));
    S(R_PART,I,0);
    ++depth_; gen G=one(I); --depth_;
    if (is_undef(G)) return G;
    vecteur t;
    for (unsigned i=0;i+1<pts.size();++i){
      gen Gb=sub(G,X,pts[i+1]),Ga=sub(G,X,pts[i]),d=SYM2(at_binary_minus,Gb,Ga);
      t.push_back(sg[i]==1?d:symb_neg(d));
      tot=tot+sg[i]*(Gb-Ga);
    }
    S(R_FTC,op,symb_plus(t));
    tot=_simplify(ev(tot),CTX);
    S(R_EQV,op,tot);
    return absame(tot,op)?tot:gen(undef);
  }
  EACH(it,pieces){
    S(R_PART,*it,0);
    ++depth_; gen vi=one(*it); --depth_;
    if (is_undef(vi)) return vi;
    tot=tot+vi;
  }
  tot=ev(tot);
  return absame(tot,op)?tot:gen(undef);
}
// the first |g| of f (0: none)
static gen absarg(const gen & f){
  if (f.type!=_SYMB) return 0;
  if (is(f,at_abs) && dep(arg(f))) return arg(f);
  return first_of(f,absarg);
}
// ---------------------------------------------------------------- limits
// lim f at la_ from ldir_ (1 the right, -1 the left, 0 both). The form is read from giac's limits
// of the parts (0/0, inf/inf, c/0, 0*inf, inf-inf, 1^inf) and rewritten the textbook way. A
// quotient is carried as its numerator and denominator: evaluated whole, giac undid the
// rewriting (x ln x written ln x/(1/x) came back as x ln x).
static int ldir_;
static gen * la_;
static NOINL gen Lx(const gen & f){ return SYM(at_limit,ldir_?makesequence(f,X,*la_,ldir_):makesequence(f,X,*la_)); }
static NOINL gen glim(const gen & g){ return bigp(g,true)?gen(undef):ev(Lx(g)); }
static NOINL bool finv(const gen & v){ return v.type!=_STRNG && !has_inf_or_undef(v) && !has_op(v,*at_bounded_function); }
static NOINL gen Ls(int r,const gen & f,const gen & v0){ gen v=v0.type==_FRAC?v0._FRACptr->num/v0._FRACptr->den:v0; S(r,Lx(f),v); return v; } // lim f = v by rule r (giac's limit gave the fraction 1/-7)
static gen lsteps(const gen & f,int lev);
static gen lq(const gen & f,const gen & n,const gen & d,int lev);
static NOINL gen quo(const gen & n,const gen & d){ return is_one(d)?n:symb_prod(n,symb_inv(d)); }
// lim f = lim n/d by rule r, then that limit (d = 1: n's)
static NOINL gen lvia(int r,const gen & f,const gen & n,const gen & d,int lev){
  size_t m=mark();
  gen g=quo(n,d);
  if (mergek(Lx(f)).print(CTX)!=mergek(Lx(g)).print(CTX)) S(r,Lx(f),Lx(g)); // (no step that changes nothing on the screen: e^x/(2x))
  gen L=is_one(d)?lsteps(n,lev+1):lq(g,n,d,lev+1);
  if (is_undef(L)) cut(m);
  return L;
}
// lim f = shown by rule r, then lim u (undef: the step taken back)
static NOINL gen linner(int r,const gen & f,const gen & shown,const gen & u,int lev){
  size_t m=mark();
  S(r,Lx(f),shown);
  gen L=lsteps(u,lev+1);
  if (is_undef(L)) cut(m);
  return L;
}
// the term of the sum g growing fastest: every other over it has a finite limit (0: none)
static NOINL gen fastest(const gen & g){
  vecteur w; flat(at_plus,g,w);
  EACH(it,w){
    bool ok=dep(*it);
    EACH(jt,w) if (ok && jt!=it && !finv(glim(ev(*jt / *it)))) ok=false; // (rn expanded (x+1)^1000)
    if (ok) return *it;
  }
  return 0;
}
// the conjugate of the sum g holding a square root: its first root term kept, the others' signs
// changed (sqrt(x+4)-1: sqrt(x+4)+1; sqrt(1+x)-sqrt(1-x): sqrt(1+x)+sqrt(1-x)); 0 if none
// (square roots only: a cube root is never cleared, (sqrt(x)-x^(1/3))/(x-1) went from conjugate to
// conjugate until the depth limit, 1.39 MB of heap on the PC)
static NOINL gen conjof(const gen & g){
  vecteur w,c; flat(at_plus,g,w);
  bool r=false;
  EACH(it,w){ gen e=ev(*it); if (froot(e,2)) return 0; bool q=!r && froot(e,0); c.push_back(q?*it:it->is_neg()?arg(*it):symb_neg(*it)); r=r||q; }
  return r && w.size()>1?symb_plus(c):gen(0);
}
// f's limits from each side, different: no limit (undef if they agree or one is missing)
static NOINL gen sides(const gen & f){
  gen L=SYM(at_limit,makesequence(f,X,*la_,-1)),R=SYM(at_limit,makesequence(f,X,*la_,1)),l=ev(L),r=ev(R);
  if (l==r || !(finv(l) || is_inf(l)) || !(finv(r) || is_inf(r))) return undef;
  S(R_LDNE,symb_equal(L,l),symb_equal(R,r));
  return gen(identificateur("DNE"));
}
// at +infinity, sqrt(q)/x^k in the terms of g as sqrt(q/x^(2k)): sqrt(1+1/x^2), not sqrt(x^2+1)/x
static NOINL gen rootin(const gen & g){
  if (*la_!=plus_inf) return g;
  vecteur t; flat(at_plus,g,t);
  EACHW(it,t){
    vecteur c,w; factors(*it,c,w);
    gen q=0,k=0;
    EACH(jt,w){ gen b=*jt,e=bexp(b); if (b==X && is_strictly_positive(-e,CTX)) k=-e; else if (e==gen(1)/2) q=b; }
    if (!is_zero(q) && !is_zero(k)) *it=symb_prod(prodof(c),symb_pow(expand(ev(q/pow(X,2*k,CTX)),CTX),gen(1)/2));
  }
  return t.size()==1?t[0]:symb_plus(t);
}
static gen lq(const gen & f,const gen & n,const gen & d,int lev){
  if (lev>12 || --budget_<0 || interrupted) return undef;
  gen ln=glim(n),ld=glim(d),a=*la_,v;
  const bool fn=finv(ln),fd=finv(ld),zn=fn && is_zero(ln),zd=fd && is_zero(ld),in=is_inf(ln),id=is_inf(ld);
  if (fd && !zd?fn || in:fn && id) return Ls(!is_inf(a) && fn && fd && finv(ev(sub(f,X,a)))?R_LSUB:R_LLAW,f,glim(f)); // c/d, inf/d, c/inf
  // (direct substitution only where f is defined: cos(x)/(sin(x)/(2sqrt(1-cos(x)))) at 0 is 1/(0/0))
  if (zd && (in || (fn && !zn))){ // c/0, inf/0: infinite on each side (both: no limit if they differ)
    v=glim(f);
    return ldir_ || is_inf(a) || (is_inf(v) && v!=unsigned_inf)?Ls(R_LINFV,f,v):sides(f);
  }
  if (id && has_op(ln,*at_bounded_function)) return Ls(R_LSQZ,f,0); // sin(x)/x at infinity
  if (!(zn && zd) && !(in && id)) return undef;
  // 0/0, inf/inf: the fraction simplified (factor and cancel), defined at a (at infinity: shorter)
  gen fe=ev(f),h=rn(fe);
  gen dh=_denom(h,CTX);
  if (h!=fe && (is_inf(a)?shape(d,0) && shape(dh,0)?is_strictly_greater(_degree(makesequence(d,X),CTX),_degree(makesequence(dh,X),CTX),CTX):taille(h,200)<taille(fe,200):finv(ev(sub(h,X,a)))) && !is_undef(v=lvia(R_LALG,f,h,1,lev))) return v;
  // (at infinity a denominator of lower degree: (x^4-4x^3+1)/(2-2x^2-7x^4) only changed its signs)
  // a finite point: a square root in a sum, times its conjugate over itself
  gen c=is_inf(a)?gen(0):conjof(n);
  const bool top=!is_zero(c);
  if (!top && !is_inf(a)) c=conjof(d);
  if (!is_zero(c) && !is_undef(v=lvia(R_LCONJ,f,top?rn(expand(ev(n*c),CTX)):symb_prod(n,c),top?symb_prod(d,c):rn(expand(ev(d*c),CTX)),lev))) return v;
  // at infinity, numerator and denominator over the highest power of x below, x^p, p its growth
  // (x/sqrt(x^2+1): 1/sqrt(1+1/x^2); L'Hopital went around in circles)
  gen p=is_inf(a)?glim(rn(ev(symb_ln(abs(d,CTX))/symb_ln(abs(X,CTX))))):gen(0);
  if (finv(p) && is_strictly_positive(p,CTX)){
    gen t=pow(X,p,CTX),n2=rootin(expand(ev(n/t),CTX)),d2=rootin(expand(ev(d/t),CTX));
    // (over a denominator x^p itself: only when the numerator comes apart, (x+cos(x))/x; e^x/x stays)
    if ((dep(d2) || n2.is_plus()) && !is_undef(v=lvia(R_LDIV,f,n2,d2,lev))) return v; // (e^x/(2x): no)
  }
  else if (is_inf(a) && d.is_plus()){ // no power of x (e^x below): over its fastest term
    gen t=fastest(d);
    if (!is_zero(t) && !is_undef(v=lvia(R_LDIV,f,expand(ev(n/t),CTX),expand(ev(d/t),CTX),lev))) return v;
  }
  // L'Hopital a few times at most, not on n! (no textbook derivative: (4n^n+n^3)/(ln(n)^80+2n!) took
  // 60 s and 93 MB on the PC), nor when the derivatives more than double the fraction
  if (lev>=8 || has_op(n,*at_factorial) || has_op(d,*at_factorial)) return undef;
  gen n1=D(n,X),d1=D(d,X);
  if (taille(n1,400)+taille(d1,400)>2*(taille(n,400)+taille(d,400))+20) return undef;
  size_t m=mark(); // the form (0/0 or inf/inf), f' and g', then the rule
  S(R_LFORM,symb_equal(Lx(n),ln),symb_equal(Lx(d),ld));
  S(R_EQD,n,n1); S(R_EQD,d,d1);
  if (is_undef(v=lvia(R_LHOP,f,n1,d1,lev))) cut(m);
  return v;
}
static gen lsteps(const gen & f0,int lev){
  if (lev>12 || --budget_<0 || ctrl_c || interrupted) return undef;
  gen f=norm(f0),a=*la_,v,L;
  if (!dep(f)) return Ls(R_LSUB,f,f);
  if (!is_inf(a) && !has_op(f,*at_floor) && !has_op(f,*at_sign) && finv(v=ev(sub(f,X,a)))) return Ls(R_LSUB,f,v); // continuous there
  if (ldir_ && !is_inf(a)){ // one side: |g| with g -> 0 there is g or -g (on the right of 0, |x| = x)
    const gen g=absarg(f);
    if (!is_zero(g) && is_zero(ev(sub(g,X,a)))){
      const gen s=is_strictly_positive(evalf(sub(g,X,a+gen(ldir_)/1000),1,CTX),CTX)?g:symb_neg(g);
      S(R_LSIDE,SYM(at_abs,g),s);
      return lsteps(sub(f,SYM(at_abs,g),s),lev+1);
    }
  }
  vecteur c,w,de; factors(f,c,w);
  vecteur nu(c);
  EACH(it,w){ gen b=*it,e=bexp(b); if (is_strictly_positive(-e,CTX)) de.push_back(is_minus_one(e)?b:symb_pow(b,ev(-e))); else nu.push_back(*it); }
  if (!de.empty()){ gen n=prodof(nu),d=prodof(de); return lq(quo(n,d),n,d,lev); }
  gen k=ev(prodof(c));
  if (w.size()==1 && w[0].type==_SYMB){
    const gen & g=w[0],& u=g.is_pow()?arg(g,0):arg(g);
    if (g.is_pow() && dep(arg(g,1))){ // u^v: e^(v ln u)
      gen e=symb_prod(arg(g,1),symb_ln(u));
      L=linner(R_LEXP,f,symb_prod(k,SYM(at_exp,Lx(e))),e,lev);
      return is_undef(L)?L:Ls(R_EQV,f,ev(k*exp(L,CTX)));
    }
    if (u.type!=_VECT){ // h(u), h continuous (a constant power too)
      if (u==X) return Ls(R_LLAW,f,glim(f)); // e^x at infinity: at once
      size_t m=mark();
      L=linner(R_LCOMP,f,symb_prod(k,g.is_pow()?symb_pow(Lx(u),arg(g,1)):SYM(&g._SYMBptr->sommet,Lx(u))),u,lev);
      if (is_undef(L)) return L;
      if (L.type==_IDNT && !ldir_){ cut(m); return sides(f); } // atan(1/x) at 0
      v=ev(k*sub(g,u,L)); // (giac's ln(0) is -inf, e^(-inf) 0)
      if (finv(v) || is_inf(v)) return Ls(R_EQV,f,v);
      cut(m); return undef;
    }
  }
  if (w.size()>1){ // a product: 0 times infinity as a quotient, a bounded factor times 0
    gen z=0,i=0;
    EACH(it,w){ gen l=glim(*it); if (is_zero(l)) z=*it; else if (is_inf(l)) i=*it; }
    if (!is_zero(z) && !is_zero(i)){
      vecteur r(c); EACH(it,w) if (*it!=z && *it!=i) r.push_back(*it);
      // ln x/(1/x), x^2/e^x (e^(-x) below as e^x), sin(1/x)/(1/x)
      const bool up=has_op(i,*at_ln) || has_op(z,*at_exp);
      if (!is_undef(v=lvia(R_LQUOT,f,prodof(r)*(up?i:z),ev(inv(up?z:i,CTX)),lev))) return v;
    }
    EACH(it,w) if ((is(*it,at_sin) || is(*it,at_cos)) && !finv(glim(*it)) && is_zero(glim(ev(f / *it)))) return Ls(R_LSQZ,f,0);
  }
  if (f.is_plus()){
    vecteur t; flat(at_plus,f,t);
    bool pi=false,mi=false;
    EACH(it,t){ gen l=glim(*it); if (l==plus_inf) pi=true; else if (l==minus_inf) mi=true; else if (!finv(l)) pi=mi=true; }
    if (pi && mi){ // infinity - infinity (or a term without a limit, csc(x)-1/x at 0): the
      gen c=conjof(f),h=rn(f),d=_denom(h,CTX); // conjugate, one fraction, the fastest term out
      if (!is_zero(c) && !is_undef(v=lvia(R_LCONJ,f,rn(expand(ev(f*c),CTX)),c,lev))) return v;
      if (dep(d) && !is_undef(v=lvia(R_LFRAC,f,_numer(h,CTX),d,lev))) return v;
      gen tt=is_inf(a)?fastest(f):gen(0);
      return is_zero(tt)?gen(undef):lvia(R_LDOM,f,symb_prod(tt,expand(ev(f/tt),CTX)),1,lev);
    }
  }
  if (has_op(f,*at_sin) || has_op(f,*at_cos)){ // limit laws need every part's limit: a term with a
    vecteur t,u; flat(at_plus,f,t); // sine or cosine factor without one, going to 0, by the squeeze
    EACH(it,t){ // theorem first (cos(x)/x at infinity; 2+sqrt(atan(x))*sqrt(sin(1/x)) at 0)
      u.clear(); flat(at_prod,*it,u);
      int k=0; // factors without a limit (each a sine or a cosine, or a positive power of one)
      EACH(jt,u){
        gen l=glim(*jt),b=jt->is_pow()?arg(*jt,0):*jt;
        if (finv(l) || is_inf(l)) continue;
        if ((!is(b,at_sin) && !is(b,at_cos)) || (jt->is_pow() && !is_strictly_positive(arg(*jt,1),CTX))) return undef;
        ++k;
      }
      if (k){ if (!is_zero(glim(*it))) return undef; S(R_LSQZ,Lx(*it),0); }
    }
  }
  v=glim(f); // every part has a limit: put together
  return finv(v) || is_inf(v)?Ls(R_LLAW,f,v):gen(undef);
}
// F without a jump in [a,b]: between 48 points F moves at most about h max|f| (F(b)-F(a) was 0
// for 1/(5+3cos x) on [0,2pi]: its tan(x/2) antiderivative jumps at pi). Undefined points: no check.
static NOINL bool nojump(const gen & F,const gen & f,const gen & x,const gen & a,const gen & b){
  gen A=evalf(a,1,CTX),B=evalf(b,1,CTX);
  if (A.type!=_DOUBLE_ || B.type!=_DOUBLE_) return true;
  double h=(B._DOUBLE_val-A._DOUBLE_val)/48,v[48],m=0; // (the largest |f| of all: cos(99x)sin(101x))
  for (int i=0;i<48;++i){
    gen t=A._DOUBLE_val+(i+0.37)*h,u=evalf(sub(F,x,t),1,CTX),y=evalf(sub(f,x,t),1,CTX);
    if (u.type!=_DOUBLE_ || y.type!=_DOUBLE_) return true;
    v[i]=u._DOUBLE_val;
    if (fabs(y._DOUBLE_val)>m) m=fabs(y._DOUBLE_val);
  }
  for (int i=1;i<48;++i) if (fabs(v[i]-v[i-1])>3*fabs(h)*m+1e-3) return false;
  return true;
}
static gen realroot(const gen & g){
  if (g.type!=_SYMB) return g;
  gen h=map_of(g,realroot);
  if (h.is_pow() && arg(h,1).type==_FRAC){
    const gen & q=arg(h,1)._FRACptr->den;
    if (q.type==_INT_ && q.val%2) return symb_pow(SYM2(at_surd,arg(h,0),q),arg(h,1)._FRACptr->num);
  }
  return h;
}
// ---------------------------------------------------------------- series and sums
static NOINL gen Linf(const gen & f,const gen & n){ return SYM(at_limit,makesequence(f,n,plus_inf)); }
static NOINL gen word(const char * w){ return gen(identificateur(w)); }
static NOINL bool same(const gen & a,const gen & b){ return is_zero(rn1(ev(a-b))); }
// giac's sum left as is, infinite or no value: a divergent one (or one giac cannot do)
static NOINL bool undone(const gen & G){ return is_inf(G) || is_undef(G) || G.type==_STRNG || has_op(G,*at_bounded_function) || is(G,at_sum); }
// the p-series conclusion, p = e: converges for p > 1 (giac's value; sum(1/n^2) left as is by giac,
// pi^2/6 is FlowCE's, in the answer), diverges for p <= 1
// a series that converges: its value when giac has one (a last line "= value"), else the word
static NOINL gen cvg(const gen & op,const gen & G){
  if (is(G,at_sum) || is_undef(G)) return word("converges");
  S(R_EQV,op,G);
  return G;
}
static NOINL gen pser(const gen & op,const gen & G,const gen & e){
  const bool cv=is_strictly_greater(e,1,CTX);
  if (cv?is_inf(G):!is_inf(G) && !is(G,at_sum)) return undef;
  S(R_PSER,word("p"),e);
  return cv?cvg(op,G):word("diverges");
}
// a(n+1)/a(n) as a(n+1) times a(n) upside down: one fraction, 2^(n+1) n!/((n+1)! 2^n), not a
// fraction over a fraction
static NOINL gen ratio_of(const gen & f,const gen & t,const gen & x){
  gen n=_numer(t,CTX),d=_denom(t,CTX);
  return symb_prod(subst(f,x,x+1,true,CTX),is_one(n)?d:symb_prod(d,symb_inv(n)));
}
// g at n as a decimal (0 if not real): compared as decimals (giac's is_strictly_positive is
// false for 4.5e-7, the term n e^(-n^2) at 4)
static NOINL double at_(const gen & g,const gen & n){ gen y=evalf(sub(g,X,n),1,CTX); return y.type==_DOUBLE_?y._DOUBLE_val:0; }
// 0 when g falls fast toward 0 at a+5, a+10, a+20 (giac's limit of n!/n^n took seconds on the
// calculator; of 1/n! it stays 1/((+infinity)!)), else g's limit
static NOINL gen lim_(const gen & g,const gen & a){
  const double y1=fabs(at_(g,a+5)),y2=fabs(at_(g,a+10)),y3=fabs(at_(g,a+20));
  return y1>0 && y2<y1 && y3<y2 && y3<1e-3*y1?gen(0):ev(Linf(g,X));
}
// every factor of g a constant to a power of n (or e^(...)), or a constant power of one (giac
// writes x^(2n) as (x^n)^2): maybe geometric (no simplify of the ratio of (n/(2n+1))^n: seconds on
// the calculator)
static NOINL bool geof(const gen & f0){
  const gen & f=f0.is_inv()?arg(f0):f0;
  return is(f,at_exp) || (f.is_pow() && (!dep(arg(f,0)) || (!dep(arg(f,1)) && geof(arg(f,0)))));
}
static NOINL bool geo(const vecteur & w){
  EACH(it,w) if (!geof(*it)) return false;
  return !w.empty();
}
// 1 if the index is in a power's base and exponent ((n/(2n+1))^n, n^n), else 0
static NOINL gen npow(const gen & g){ return g.is_pow() && dep(arg(g,0)) && dep(arg(g,1))?gen(1):first_of(g,npow); }
// 1 if the index is in an exponent of g (2^n, n^n), else 0
static NOINL gen nexp(const gen & g){ return g.is_pow() && dep(arg(g,1))?gen(1):first_of(g,nexp); }
// the sums linear in N (*nn_) written out: 2(N+1)-1 as 2N+1, N+1+1 as N+2
static const gen * nn_;
static NOINL gen linn(const gen & g){
  gen a,b;
  if (g.is_plus() && is_linear_wrt(g,*nn_,a,b,CTX)) return a*(*nn_)+b;
  return g.type==_SYMB?map_of(g,linn):g;
}
// sum(t,n,a,inf): the divergence test, telescoping, a p-series, a geometric series, the partial
// sums' limit, else a limit comparison with a p-series (convergence only)
static NOINL gen ssteps(const gen & op,const vecteur & v,const gen & x){
  if (v.size()!=4 || ev(v[3])!=plus_inf) return undef;
  const gen a=ev(v[2]),N=word(x==word("N")?"M":"N"),m1=pow(gen(-1),x,CTX);
  gen cp=ev(SYM(at_cos,x*cst_pi));
  gen t=ev(v[0]);
  // giac's sum as the reference, but not of n in a power's base and exponent, n!, ln, trig: it
  // simplifies them at length for nothing (43 s of (n/(2n+1))^n on the calculator; the console's
  // answer skips it too, answer.cc power_sum)
  const gen G=!is_zero(npow(t)) || has_op(t,*at_factorial) || has_op(t,*at_ln) || has_op(t,*at_sin) || has_op(t,*at_cos) || has_op(t,*at_tan)?op:ev(op);
  for (int i=0;i<2;++i,cp=SYM(at_cos,symb_prod(cst_pi,x))) if (contains(t,cp)){ S(R_COSPI,cp,m1); t=ev(subst(t,cp,m1,false,CTX)); } // (giac leaves cos(n pi), cos(pi n))
  gen L,val,SN;
  vecteur c,w; factors(t,c,w);
  // the shape first: giac's limit of (n/(2n+1))^n or (-1)^n/sqrt(n) took 10 to 20 s on the calculator
  gen r=0;
  if (geo(w)){ // geometric
    r=_simplify(ev(sub(t,x,x+1)/t),CTX); // (2^n/2^(n+1): ratnormal left it)
    const gen ar=evalf(abs(r,CTX),1,CTX);
    const bool in_x=ar.type!=_DOUBLE_; // r = x/3 (a power series): it converges for |x/3| < 1
    if (!dep(r) && (in_x || is_strictly_greater(1,ar,CTX))){
      gen t0=ev(sub(t,x,a));
      val=ev(t0/(1-r));
      S(R_SRAT,ratio_of(v[0],t,x),r);
      if (in_x) S(R_SCONV,SYM2(at_inferieur_strict,SYM(at_abs,r),1),0);
      S(R_SGEOM,op,symb_prod(t0,symb_inv(symb_plus(1,symb_neg(r)))));
      S(R_EQV,op,in_x?rn1(val):val); // (-3/(x-3), as the console writes it)
      return same(val,G) || (in_x && undone(G))?val:gen(undef); // (giac sums x^n/3^n only with |x| < 3 assumed: the console's is answer.cc's)
    }
    if (!dep(r) && ar.type==_DOUBLE_){ // (-pi/3)^n: |r| >= 1, the terms do not go to 0
      S(R_SRAT,ratio_of(v[0],t,x),r);
      S(R_SGEOMD,r,0);
      return undone(G)?word("diverges"):gen(undef);
    }
  }
  // b^n, b depending on n: the root test, lim (a_n)^(1/n) = lim b below 1 ((n/(2n+1))^n, 1/ln(n)^n)
  gen u=t.is_inv()?arg(t):t;
  if (!is_inf(G) && u.is_pow() && dep(arg(u,0)) && (arg(u,1)==x || is_zero(ev(arg(u,1)+x)))){
    gen b=(arg(u,1)==x)!=t.is_inv()?arg(u,0):symb_inv(arg(u,0));
    L=ev(Linf(b,x));
    if (finv(L) && is_strictly_greater(1,abs(L,CTX),CTX)){ S(R_ROOT,Linf(b,x),L); return cvg(op,G); }
  }
  // (-1)^(n+k) b, b > 0 decreasing to 0 (from a+3 on: ln(n)/n): the alternating series test
  if (!is_inf(G)) EACH(it,w) if (it->is_pow() && is_minus_one(arg(*it,0))){
    vecteur o(c);
    EACH(jt,w) if (jt!=it) o.push_back(*jt);
    gen b=ev(prodof(o));
    if (at_(b,a+3)<0) b=ev(-b);
    int k=3;
    while (k<12 && at_(b,a+k)>at_(b,a+k+1)) ++k;
    if (k<12 || !is_zero(lim_(b,a))) break; // (b not decreasing to 0: the terms' limit below)
    S(R_AST,Linf(b,x),0);
    return cvg(op,G);
  }
  L=lim_(t,a); // (0 at once when the terms fall fast: n!/n^n, 2^n/n!)
  if (!is_zero(L) && (finv(L) || L==plus_inf || L==minus_inf || has_op(L,*at_bounded_function))){ // the terms do not go to 0
    // (+-infinity itself: giac's limit of 1/n! is 1/((+infinity)!), and is_inf looks inside)
    S(R_SDIV,Linf(v[0],x),has_op(L,*at_bounded_function)?word("DNE"):L); // (no raw bounded_function)
    return undone(G)?word("diverges"):gen(undef);
  }
  // f(n)-f(n+k), k up to 3, as typed or in partial fractions: telescoping, S_N the first terms
  // less the last (1/(n(n+2)): 1+1/2-1/(N+1)-1/(N+2))
  gen pf=shape(t,SH_FRAC)?pfrac(t):t,s=t.is_plus()?t:pf; // (X: the index here)
  if (is(t,at_ln)) s=symb_plus(symb_ln(_numer(arg(t),CTX)),symb_neg(symb_ln(_denom(arg(t),CTX)))); // ln(n/(n+1))
  if (s.is_plus() && arg(s).type==_VECT && arg(s)._VECTptr->size()==2){
    gen u=arg(s,0),w=arg(s,1);
    for (int i=0;i<6;++i,std::swap(u,w)) if (same(sub(u,x,x+1+i/2),-w)){
      vecteur q;
      for (int j=0;j<=i/2;++j){ gen y=sub(u,x,a+j); if (!is_zero(y)) q.push_back(y); } // (ln(1))
      nn_=&N;
      for (int j=1;j<=1+i/2;++j){ gen y=linn(subst(u,x,N+j,true,CTX)); q.push_back(y.is_neg()?arg(y):symb_neg(y)); } // (atan: no --)
      SN=symb_plus(q);
      val=ev(Linf(SN,N));
      if (!same(val,G) && !is(G,at_sum) && !(is_inf(val) && is_inf(G))) return undef; // (S_N is exact)
      if (s!=t) S(is(t,at_ln)?R_LNQ:R_PFRAC,v[0],s);
      S(R_PSUMT,SYM(at_sum,makesequence(s!=t?s:v[0],x,a,N)),SN);
      S(R_SLIM,Linf(SN,N),val);
      return is_inf(val)?word("diverges"):val;
    }
  }
  gen e=0;
  bool ps=!w.empty();
  EACH(it,w){ gen b=*it,ei=bexp(b); ps=ps && b==x; e=e+ei; } // c/n^p (1/n^(3/2) is 1/(n*sqrt(n)))
  e=ev(-e);
  if (ps && is_strictly_positive(e,CTX)) return pser(op,G,e);
  // n! or the index in an exponent: the ratio test, lim |a(n+1)/a(n)| below 1 (2^n/n!, n!/n^n)
  if (!is_inf(G) && (has_op(t,*at_factorial) || !is_zero(nexp(t))) && dep(is_zero(r)?r=_simplify(ev(sub(t,x,x+1)/t),CTX):r)){
    gen q=_factor(r,CTX),qa=is_strictly_positive(evalf(sub(q,x,a+3),1,CTX),CTX)?q:SYM(at_abs,q);
    L=ev(Linf(qa,x));
    if (finv(L) && is_strictly_greater(1,L,CTX)){
      S(R_SRAT,ratio_of(v[0],t,x),q);
      S(R_RATIO,Linf(qa,x),L);
      return cvg(op,G);
    }
  }
  // the partial sums' closed form (giac's symbolic sum to N, slow: after the tests above)
  SN=ev(SYM(at_sum,makesequence(t,x,a,N)));
  if (elementary(SN) && SN.type!=_STRNG){
    SN=rn(SN);
    val=ev(Linf(SN,N));
    if (same(val,G) || (is_inf(val) && is_inf(G))){
      const bool sp=pf!=t && pf.is_plus();
      if (sp) S(R_PFRAC,v[0],pf);
      S(sp?R_PSUMT:R_PSUM,SYM(at_sum,makesequence(sp?pf:v[0],x,a,N)),SN);
      S(R_SLIM,Linf(SN,N),val);
      S(R_EQV,op,val);
      return val;
    }
  }
  // like 1/n^p: lim t n^p finite and not 0 (a limit comparison: converges or not)
  e=ev(-Linf(rn(ev(symb_ln(abs(t,CTX))/symb_ln(x))),x));
  L=finv(e) && is_strictly_positive(e,CTX)?ev(Linf(t*pow(x,e,CTX),x)):gen(0);
  size_t m=mark();
  if (finv(L) && is_strictly_positive(L,CTX)){
    S(R_LCMP,Linf(SYM2(at_division,v[0],symb_inv(pow(x,e,CTX))),x),L); // a_n over b_n = 1/n^p, as written (a quotient mergek leaves)
    val=pser(op,G,e);
    if (!is_undef(val)) return val;
    cut(m);
  }
  // positive and decreasing (from a+3 on): the integral test, the integral's steps below it
  // (1/(n ln(n)): u = ln(x), +infinity, diverges)
  for (int k=3;k<9;++k){ double y=at_(t,a+k); if (!(y>0 && y>at_(t,a+k+1))) return undef; }
  gen I=SYM(at_integrate,makesequence(v[0],x,a,plus_inf));
  S(R_ITEST,I,0);
  ++depth_; val=one(I); --depth_;
  if (is_undef(val) || (is_inf(val)?finv(G) && !is(G,at_sum):is_inf(G))){ cut(m); return undef; } // (as giac's sum)
  return is_inf(val)?word("diverges"):cvg(op,G);
}
// series(f,x,a,n): f's derivatives, their values at a, the Taylor polynomial (giac's without O)
static NOINL gen tsteps(const gen & op,const vecteur & v,const gen & x){
  if (v.size()<4 || v[3].type!=_INT_ || v[3].val<1 || v[3].val>8) return undef;
  const gen a=ev(v[2]),h=a==0?x:symb_plus(x,symb_neg(a)); // x - a
  const int n=v[3].val;
  vecteur ds(1,ev(v[0])),qs,gt,kp,ts;
  gen P=0;
  for (int k=1;k<=n && !interrupted;++k){ // (normal form only when shorter: -1/(x+1)^2)
    gen d=D(ds.back(),x),d2=rn1(d);
    if (taille(d2,200)<taille(d,200)) d=d2;
    S(k>1?R_EQV:R_TDER,Dx(ds.back()),d); ds.push_back(d);
  }
  for (int k=0;k<=n && !interrupted;++k){ // f(a) + f'(a) (x-a) + f''(a)/2! (x-a)^2 ...
    gen c=ev(sub(ds[k],x,a)),q=subst(ds[k],x,a,true,CTX);
    if (!k || q!=c) S(k?R_EQV:R_TVAL,q,c); // (e^x at 1: no line "e = e")
    if (is_zero(c)) continue;
    qs.push_back(k?symb_prod(k>1?symb_prod(c,symb_inv(SYM(at_factorial,k))):c,k>1?symb_pow(h,k):h):c);
    P=P+c/factorial(k)*pow(h,k,CTX);
    // the polynomial as textbooks write it: each coefficient a number, (x-a)^k kept whole (giac's
    // sum distributed it: 1-x+1+(x-1)^2 for 1-(x-1)+(x-1)^2)
    gen e=ev(c/factorial(k)),hk=k>1?symb_pow(h,k):h;
    ts.push_back(!k?e:is_one(e)?hk:is_strictly_positive(-e,CTX)?symb_neg(is_minus_one(e)?hk:symb_prod(-e,hk)):symb_prod(e,hk));
  }
  P=ev(P);
  flat(at_plus,ev(op),gt);
  EACH(it,gt) if (!has_op(*it,*at_order_size)) kp.push_back(*it);
  if (interrupted || !same(symb_plus(kp),P)) return undef;
  S(R_TAYLOR,qs.empty()?gen(0):symb_plus(qs),ts.empty()?gen(0):symb_plus(ts));
  return P;
}
// One calculus operation, with none inside: its value (undef: no steps)
static NOINL gen one_(const gen & op){
  const vecteur & v=*arg(op)._VECTptr;
  gen x=v[1]; var_=&x;
  if (is(op,at_limit)){ // the steps' limit must be giac's (no limit: giac says so too)
    if (v.size()<3) return undef;
    gen a=ev(v[2]),L,G;
    la_=&a; ldir_=v.size()>3?(is_positive(v[3],CTX)?1:-1):0;
    budget_=80;
    // giac's limit of the problem as typed (of sin(x)/sqrt(1-cos(x)) at 0: "limits are distinct";
    // of sin(x)*(1-cos(x))^(-1/2), the steps' form, sqrt(2)); no limit: the two sides first (|x|/x
    // at 0: L'Hopital was tried for 5 s on the calculator before), then the other ways
    G=ev(depth_?op:*in_);
    if (has_op(G,*at_bounded_function) && (is(v[0],at_sin) || is(v[0],at_cos))){ L=word("DNE"); S(R_LOSC,Lx(v[0]),L); return L; } // sin(1/x) at 0
    L=!ldir_ && (G.type==_STRNG || G==unsigned_inf)?sides(v[0]):gen(undef);
    if (is_undef(L)) L=lsteps(v[0],0);
    if (interrupted || is_undef(L)) return undef;
    if (L.type==_IDNT) return G.type==_STRNG || G==unsigned_inf || is_undef(G)?L:gen(undef);
    return L==G || (finv(L) && finv(G) && (is_zero(evalf(L-G,1,CTX)) || same_num(L,G,X)))?L:gen(undef); // (x^2/3, abs(x^2)/3)
  }
  if (is(op,at_sum)) return ssteps(op,v,x);
  if (is(op,at_series)) return tsteps(op,v,x);
  if (is(op,at_derive)){
    if (v.size()>2) return undef; // (of order 5 or more: nest)
    gen r;
    if (ftc1able(op)){
      const vecteur & w=*arg(v[0])._VECTptr; // integrate(f,t,a,b)
      gen b=symb_prod(sub(w[0],w[1],w[3]),Dx(w[3]));
      if (dep(w[2])) b=symb_plus(b,symb_neg(symb_prod(sub(w[0],w[1],w[2]),Dx(w[2]))));
      S(R_FTC1,v[0],b);
      r=ev(b);
      S(R_EQD,v[0],r);
    }
    else r=dsteps(v[0]);
#ifdef HOSTGIAC
    if (getenv("STEPDBG")) fprintf(stderr,"derivative of %s: steps %s, giac %s\n",v[0].print(CTX).c_str(),ev(r).print(CTX).c_str(),D(v[0],x).print(CTX).c_str());
#endif
    return !interrupted && same_num(ev(r),D(v[0],x),x)?r:gen(undef);
  }
  gen a,b,F,f=v[0],lo,hi;
  bool fin=v.size()==4;
  size_t m0=mark();
  if (fin){
    a=ev(v[2]); b=ev(v[3]);
    fin=!is_inf(a) && !is_inf(b);
    gen g=absarg(f);
    if (!is_zero(g) && fin) return absplit(op,g,a,b);
  }
  if (fin){ // the interval (|g| keeping its sign there is g or -g); a root hiding a square shows |g|
    bool sw=is_strictly_greater(a,b,CTX); lo=sw?b:a; hi=sw?a:b; lo_=&lo; hi_=&hi;
    gen fe=ev(f);
    gen f2=sqout(fe);
    if (f2==fe && !is_zero(radicand(fe)) && !froot(fe,2)){ // (x^(1/3): rootof)
      f2=_simplify(fe,CTX);
      if (!has_op(f2,*at_abs) || has_op(f2,*at_rootof) || taille(f2,200)>taille(fe,200)) f2=fe;
    }
    if (f2!=fe){ S(R_SIMP,v[0],f2); f=f2; }
  }
  // the short solutions first: the methods are tried a few levels deep, then deeper
  size_t m=mark();
  for (maxlev_=3;maxlev_<9;maxlev_+=2){
    budget_=150; nsub_=0; cutlev_=false;
    F=isteps(f,0);
    if (!is_undef(F) || interrupted || !cutlev_) break; // (nothing deeper to find)
    cut(m);
  }
  if (interrupted) return undef; // (ON, or too long: giac's state after an interruption is fragile)
  // F checked inside [a,b] too; only there if |g| was written g there
  if (is_undef(F) || !same_num(D(F,x),ev(v[0]),x,fin?&a:0,fin?&b:0,absi_)){
    cut(m0);
    if (v.size()!=4 || elementary(_integrate(makesequence(v[0],x),CTX))) return undef;
    gen val=evalf(op,1,CTX); // no elementary antiderivative: the decimal value
    S(R_NOELEM,op,val);
    return val;
  }
  if (v.size()==2) return F;
  gen Fb=sub(F,x,b),Fa=sub(F,x,a),val;
  if (has_i(Fa) || has_i(Fb)){ gen R=realroot(F); Fb=sub(R,x,b); Fa=sub(R,x,a); }
  // limits at an infinite bound, or where F is not defined (ln(x) at 0), from inside [a,b]
  bool ia=is_inf(a) || has_inf_or_undef(ev(Fa)),ib=is_inf(b) || has_inf_or_undef(ev(Fb)),up=is_greater(b,a,CTX);
  if (ia || ib){
    gen La=ia?SYM(at_limit,is_inf(a)?makesequence(F,x,a):makesequence(F,x,a,up?1:-1)):Fa;
    gen Lb=ib?SYM(at_limit,is_inf(b)?makesequence(F,x,b):makesequence(F,x,b,up?-1:1)):Fb;
    S(R_IMPROPER,op,symb_plus(Lb,negof(La)));
    val=ev(ev(Lb)-ev(La));
  }
  else { // F(b) - F(a) with the numbers put in, F(a) = 0 left out; an antiderivative jumping in
    if (!nojump(F,ev(v[0]),x,a,b)){ cut(m0); return undef; } // [a,b] gives no steps
    gen Qb=subst(F,x,b,true,CTX),Qa=subst(F,x,a,true,CTX);
    S(R_FTC,op,is_zero(Fa)?Qb:symb_plus(Qb,symb_neg(Qa)));
    val=ev(Fb-Fa);
  }
  val=_simplify(val,CTX);
  S(R_EQV,op,val);
  return val;
}
static gen one(const gen & op){
  gen * old=var_, * ol=lo_, * oh=hi_; bool ab=absi_;
  lo_=hi_=0; absi_=false;
  gen r=one_(op);
  var_=old; lo_=ol; hi_=oh; absi_=ab;
  return r;
}

// diff(f,x,2) and diff(f,x,y) as nested derivatives (up to the 4th: the 23rd of x^2 sin(x^3) took
// 31050 steps, 168 MB and 64 s on the PC; one of order 5 or more has no steps, as the course reads
// it off the Taylor series)
static gen nest(const gen & g){
  if (g.type!=_SYMB) return g;
  gen a=arg(g);
  if (a.type==_VECT){
    vecteur w=*a._VECTptr;
    EACHW(it,w) *it=nest(*it);
    if (is(g,at_derive) && w.size()>2 && !(w.back().type==_INT_ && w.back().val>4)){
      gen r=w[0];
      for (unsigned i=1;i<w.size();++i){
        if (w[i].type==_INT_ && i>1){ for (int k=1;k<w[i].val;++k) r=SYM2(at_derive,r,w[i-1]); continue; }
        r=SYM2(at_derive,r,w[i]);
      }
      return r;
    }
    return symbolic(g._SYMBptr->sommet,gen(w,a.subtype));
  }
  return symbolic(g._SYMBptr->sommet,nest(a));
}

// taylor(f,x=a,n) and series(f,x=a,n) (giac's own syntax) and taylor(f,x,n,a) (the TI-89's) as the
// menu's series(f,x,a,n): every form has steps (the course's taylor(sqrt(x),x=4,3) had none)
static NOINL gen serfix(const gen & g){
  const bool t=is(g,at_taylor);
  if (!(t || is(g,at_series)) || arg(g).type!=_VECT || arg(g)._VECTptr->size()<2) return g;
  const vecteur & v=*arg(g)._VECTptr;
  gen x=v[1],a=0,n=v.size()>2?v[2]:gen(5);
  if (is(x,at_equal)){ a=arg(x,1); x=arg(x,0); }
  else if (!t){ if (v.size()>3) return g; n=5; if (v.size()==3) a=v[2]; } // series(f,x,a,n); series(f,x[,a]): order 5
  else if (v.size()>3) a=v[3];
  return x.type==_IDNT?SYM(at_series,makesequence(v[0],x,a,n)):g;
}
gen limit_simp(const gen & g); // answer.cc: a ratio with k in exponents simplified first
// the derivative of f by the steps' rules, written as they are (6x sec(3x^2)^2, -12 cot^2 csc^2):
// the console's answer when tan, cot, sec or csc is typed (giac's: 6x/cos(3x^2)^2,
// 12(sin^2-1)/sin^4); undef if it is not giac's
// e^(g ln b) back as b^g (x^cot(x): the steps write the power with e to differentiate it)
static gen unexp(const gen & g){
  if (g.type!=_SYMB) return g;
  if (is(g,at_exp)){
    vecteur c; flat(at_prod,arg(g),c);
    EACH(it,c) if (is(*it,at_ln)){ vecteur r(c); r.erase(r.begin()+(it-c.begin())); return symb_pow(arg(*it),prodof(r)); }
  }
  return map_of(g,unexp);
}
gen steps_derivative(const gen & f,const gen & x,GIAC_CONTEXT){
  std::vector<step_t> v;
  gen xx=x;
  ctx_=contextptr; out_=&v; depth_=0; nsub_=0; var_=&xx;
  gen r=dsteps(norm(f));
  return !is_undef(r) && !interrupted && same_num(ev(r),D(f,x),x)?mergek(unexp(r)):gen(undef);
}
// ---------------------------------------------------------------- applications
// The calculus menu's applications: area(f,g[,a,b]), volume(f,g[,a,b][,axis]), arclength(f,a,b),
// surfacearea(f,a,b[,axis]) and average(f,a,b), as the definite integral each comes to:
// answer_before (answer.cc) evaluates it, steps_of shows how it is set up, then its own steps.
// One curve and a, b: with the x-axis (area(x^2,0,2)). Curves in x, as y=f(x), or in y (x=g(y),
// or y alone): then dy. The axis: y=c or x=c (default the x-axis); washers when it runs along the
// variable's axis, shells across it. No a, b: where the curves meet.
static const char * const APPS[]={"area","volume","arclength","surfacearea","average"};
static NOINL bool named(const gen & g,const char * s){ return g.type==_IDNT && !strcmp(g._IDNTptr->id_name,s); }
// u-v as v-u, else -h: the positive way to write a distance. Differences as u+(-v), as norm writes
// them (giac prints a binary minus x-(x^2))
static NOINL gen flip(const gen & h){
  if (!h.is_plus() || arg(h).type!=_VECT || arg(h)._VECTptr->size()!=2) return symb_neg(h);
  const gen & w=arg(h,1);
  return symb_plus(w.is_neg()?arg(w):ev(-w),symb_neg(arg(h,0)));
}
static NOINL gen subt(const gen & u,const gen & v){ return is_zero(v)?u:symb_plus(u,isnum(v)?ev(-v):symb_neg(v)); }
// the sign of h on (a,b), read at 8 points: 1 or -1, 0 if it changes
static NOINL int sgn_on(const gen & h,const gen & a,const gen & b){
  int s=0;
  for (int i=1;i<9;++i){
    const gen t=is_inf(b)?(is_inf(a)?gen(i-5):a+i):is_inf(a)?b-i:a+(b-a)*i/9,d=evalf(sub(h,X,t),1,CTX);
    const int k=is_strictly_positive(d,CTX)?1:is_strictly_positive(-d,CTX)?-1:0;
    if (k && s && k!=s) return 0;
    if (k) s=k;
  }
  return s;
}
// h written positive on (a,b): as it is, flipped, or |h| when it changes sign
static NOINL gen pos(const gen & h,const gen & a,const gen & b){ const int s=sgn_on(h,a,b); return !s?symb_abs(h):s>0?h:flip(h); }
// g as k times powers b[i]^n[i]
static NOINL void powers(const gen & g,int e,gen & k,vecteur & b,std::vector<int> & n){
  if (g.is_prod() && arg(g).type==_VECT){ EACH(it,*arg(g)._VECTptr) powers(*it,e,k,b,n); return; }
  if (g.is_inv()){ powers(arg(g),-e,k,b,n); return; }
  if (!dep(g)){ k=e>0?k*g:k/g; return; }
  const bool p=g.is_pow() && arg(g,1).type==_INT_;
  b.push_back(p?arg(g,0):g); n.push_back(p?e*arg(g,1).val:e);
}
// sqrt(w) without its root when w is a square, positive on (a,b) (the textbook arc lengths: 1+f'^2
// = (x^4+1)^2/(4x^4), so sqrt(1+f'^2) = (x^4+1)/(2x^2)); 0 if w is no square. normal puts back
// what giac's factor splits over sqrt(2) (x^4+1); not sqrfree: of 8cos(t)^2sin(t)^2 it gave
// 8cos(t)^2
static NOINL gen root(const gen & w,const gen & a,const gen & b){
  gen k=1; vecteur bs; std::vector<int> n;
  powers(_factor(w,CTX),1,k,bs,n);
  gen r=sqrt(k,CTX);
  for (unsigned i=0;i<bs.size();++i){ if (n[i]%2) return 0; r=r*pow(bs[i],n[i]/2,CTX); }
  return pos(normal(ev(r),CTX),a,b);
}
// the real points where f and g meet, in order
static NOINL vecteur meet(const gen & f,const gen & g){
  const gen z=_solve(makesequence(symb_equal(f,g),X),CTX);
  vecteur r;
  if (z.type==_VECT) EACH(it,*z._VECTptr){ const gen d=evalf(*it,1,CTX); if (d.type==_DOUBLE_) r.push_back(*it); }
  for (unsigned i=1;i<r.size();++i)
    for (unsigned j=i;j && is_strictly_greater(r[j-1],r[j],CTX);--j){ gen s=r[j]; r[j]=r[j-1]; r[j-1]=s; }
  return r;
}
// the integral of |h| over (a,b) as integrals of h or -h, split where h vanishes (giac's |h| took 94 s
// on the calculator for x^3 against x^2+x, over 2 minutes for acos against asin); integrate(|h|) if a
// piece still changes sign. cut: the points inside
static NOINL gen pieces(const gen & h,const gen & a,const gen & b,vecteur & cut){
  const vecteur z=meet(h,0);
  vecteur pts(1,a),J;
  EACH(it,z) if (is_strictly_greater(*it,a,CTX) && is_strictly_greater(b,*it,CTX)) pts.push_back(*it);
  pts.push_back(b);
  for (unsigned i=0;i+1<pts.size();++i){
    const int s=sgn_on(h,pts[i],pts[i+1]);
    if (!s) return symbolic(at_integrate,makesequence(symb_abs(h),X,a,b));
    J.push_back(symbolic(at_integrate,makesequence(s>0?h:flip(h),X,pts[i],pts[i+1])));
  }
  cut=vecteur(pts.begin()+1,pts.end()-1);
  return J.size()==1?J.front():symb_plus(J);
}
// the call's integral (0: not an application; a user's own area(...) runs as written; 0 and
// appmsg_ for a call it cannot read); show: its setup as steps; lhs: what it computes (A, V, L, S,
// average)
static const char * appmsg_;
static NOINL gen app_(const gen & call,bool show,gen & lhs){
  appmsg_=0;
  if (!is(call,at_of) || arg(call).type!=_VECT || arg(call)._VECTptr->size()!=2) return 0;
  const gen & fn=arg(call,0);
  int k=4;
  while (k>=0 && !named(fn,APPS[k])) --k;
  if (k<0 || ev(fn)!=fn) return 0;
  const gen & as=arg(call,1);
  vecteur w=as.type==_VECT && as.subtype==_SEQ__VECT?*as._VECTptr:vecteur(1,as),m;
  const gen x(identificateur("x")),y(identificateur("y"));
  gen v=x,c=0,f,g=0,a,b,p=0,q=0,dv=0,w2=0,sq=0;
  var_=&v;
  bool horiz=true;
  if ((k&1) && !w.empty() && is(w.back(),at_equal) && arg(w.back()).type==_VECT){ // volume, surfacearea: the axis
    const gen & s=arg(w.back(),0),& t=arg(w.back(),1);
    if ((named(s,"x") || named(s,"y")) && !dep(t,x) && !dep(t,y)){ horiz=named(s,"y"); c=t; w.pop_back(); }
  }
  const int n=w.size(),nc=n==3?1:k<2 && (n==2 || n==4)?2:-1;
  for (int i=0;i<nc;++i){ // y=f(x): f; x=g(y): g, in y
    gen e=w[i];
    if (is(e,at_equal) && arg(e).type==_VECT && (named(arg(e,0),"x") || named(arg(e,0),"y"))){
      if (named(arg(e,0),"x")) v=y;
      e=arg(e,1);
    }
    (i?g:f)=norm(e);
  }
  const bool param=k==2 && f.type==_VECT && f._VECTptr->size()==2;
  if (param){ // [x(t),y(t)]: in t, or in its one variable
    const gen t(identificateur("t"));
    const vecteur ids=lidnt(f);
    v=dep(f,t) || ids.empty()?t:ids.front();
  }
  else if (v==x && !dep(makevecteur(f,g),x) && dep(makevecteur(f,g),y)) v=y;
  if (nc==n){ // no bounds: where the curves meet
    m=meet(f,g);
    if (m.size()<2){ appmsg_="The curves meet fewer than twice: give a and b"; return 0; }
    a=m.front(); b=m.back();
  }
  else if (nc>0){ a=w[n-2]; b=w[n-1]; }
  static const char * const USE[]={"Use area(f,g), area(f,g,a,b) or area(f,a,b)","Use volume(f,g,a,b,axis), volume(f,a,b,axis) or volume(f,g,axis)","Use arclength(f,a,b)","Use surfacearea(f,a,b,axis)","Use average(f,a,b)"};
  const gen ab=makevecteur(a,b);
  if (nc<0 || dep(ab,x) || dep(ab,y) || dep(ab,v)){ appmsg_=USE[k]; return 0; } // (volume(x,x^2,0): b left out)
  const bool along=horiz==(v==x); // the axis along the variable's: washers; a surface's radius f-c
  if (k==2 || k==3){ // ds = sqrt(1+f'^2) dx, or sqrt(x'^2+y'^2) dt
    dv=param?gen(makevecteur(rn1(D(f[0],v)),rn1(D(f[1],v)))):rn1(D(f,v));
    w2=rn(param?dv[0]*dv[0]+dv[1]*dv[1]:1+dv*dv);
    sq=root(w2,a,b);
  }
  if (k&1) p=pos(along?subt(f,c):subt(v,c),a,b); // washers: f's radius; shells, surfaces: the radius
  if (k==1){
    if (along){ // R: the far curve, r: the near one (none: disks)
      q=subt(g,c);
      q=is_zero(ev(q))?gen(0):pos(q,a,b);
      if (sgn_on(subt(p,q),a,b)<0){ gen s=p; p=q; q=s; }
    }
    else q=pos(subt(f,g),a,b); // the height
  }
  // R^2 with the square of a root taken (sqrt(4-x^2)^2 is 4-x^2: no steps for it). ds over one
  // denominator: sqrt(1+x^2/(4-x^2)) times sqrt(4-x^2) came out 4, not 8pi, in giac's integral
  vecteur cut;
  const gen ds=!is_zero(sq)?sq:SYM(at_sqrt,w2),
    I=!k?pieces(subt(f,g),a,b,cut):k==1?(!along?symb_prod(p,q):is_zero(q)?ev(symb_pow(p,2)):subt(ev(symb_pow(p,2)),ev(symb_pow(q,2)))):k==2?ds:k==3?symb_prod(p,ds):f,
    K=k==1?(along?cst_pi:2*cst_pi):k==3?2*cst_pi:k==4?ev(symb_inv(subt(b,a))):gen(1),
    J=!k?I:symbolic(at_integrate,makesequence(I,v,a,b)),r=is_one(K)?J:symb_prod(K,J); // (an area: its pieces' integrals)
  static const char * const LHS[]={"A","V","L","S","average"};
  lhs=identificateur(LHS[k]);
  if (show){
    if (m.empty()) m=cut; // (where they cross between a and b)
    if (!m.empty()){
      vecteur s;
      EACH(it,m) s.push_back(symb_equal(v,*it));
      S(R_MEET,f,g); S(R_LINE,gen(s,_SEQ__VECT),0);
    }
    if (!k) S(R_AREA,lhs,r);
    else if (k==4) S(R_AVG,lhs,r);
    else S(k==1?(along?(is_zero(q)?R_DISK:R_WASH):R_SHELL):k==3?R_SURF:param?R_ARCP:R_ARC,
           param?dv[0]:k==2?dv:p,param?dv[1]:k==3?dv:q);
    if (!is_zero(sq)) S(R_SQ,param?w2:symb_plus(1,symb_pow(dv,2)),sq);
    if (k && k!=4) S(R_EQV,lhs,r);
  }
  return r;
}
gen steps_app(const gen & g,const char ** msg,GIAC_CONTEXT){
  *msg=0;
  if (!is(g,at_of)) return 0;
  gen x(identificateur("x")),lhs,* old=var_;
  ctx_=contextptr; var_=&x;
  gen r=app_(g,false,lhs);
  var_=old;
  *msg=appmsg_;
  return r;
}

static NOINL gen el_norm(const gen & g);
static NOINL bool poly_quo(const gen & e,gen & n,gen & d); // (DeltaCE, below: a division typed)
// the problem as typed (mergek without its rewrites: one denominator, 1/(n(n+2)), not 1/n/(n+2))
gen steps_problem(const gen & input,GIAC_CONTEXT){
  if (input.is_symb_of_sommet(at_implicitdiff) && arg(input).type==_VECT){ // (DeltaCE) the equation
    ctx_=contextptr;
    const gen e=arg(input,0);
    return e.is_symb_of_sommet(at_equal)?symbolic(at_equal,makesequence(ev(arg(e,0)),ev(arg(e,1)))):ev(e);
  }
  { // (DeltaCE) a division as typed (propfrac's or alone), its divisor not cancelled
    gen x(identificateur("x")),n,d;
    ctx_=contextptr; var_=&x;
    const gen & e=input.is_symb_of_sommet(at_propfrac)?(arg(input).type==_VECT?arg(input,0):arg(input)):input;
    if (poly_quo(e,n,d)) return SYM2(at_prod,n,SYM(at_inv,d));
  }
  if (input.is_symb_of_sommet(at_factor) || input.is_symb_of_sommet(at_canonical_form) || input.is_symb_of_sommet(at_propfrac) || input.is_symb_of_sommet(at_partfrac)){ ctx_=contextptr; return ev(arg(input).type==_VECT?arg(input,0):arg(input)); } // (the polynomial; F4's forms: the expression)
  if (input.is_symb_of_sommet(at_solve) && lop(input,at_derive).empty() && lop(input,at_integrate).empty() && lop(input,at_limit).empty() && lop(input,at_sum).empty()){ // the equation, its sides as giac writes them
    ctx_=contextptr;
    gen e=arg(input).type==_VECT?arg(input,0):arg(input);
    if (!lop(e,at_log10).empty() || !lop(e,at_logb).empty()) return el_norm(e); // (DeltaCE: log(x) as typed, not ln(x)/ln(10))
    return e.is_symb_of_sommet(at_equal) || rel_of(e)?symbolic(e._SYMBptr->sommet,makesequence(ev(arg(e,0)),ev(arg(e,1)))):ev(e); // (DeltaCE: 3x-5 < 7, not 7 > 3x-5)
  }
  if (input.is_symb_of_sommet(at_equal) || rel_of(input)){ ctx_=contextptr; return symbolic(input._SYMBptr->sommet,makesequence(ev(arg(input,0)),ev(arg(input,1)))); } // (DeltaCE: typed alone)
  ctx_=contextptr; raw_=true;
  gen in=input;
  if (is(in,at_of) && arg(in).type==_VECT && arg(in)._VECTptr->size()==2 && arg(in,1).type==_VECT){ // area(x-x^2,...): the arguments too
    vecteur w=*arg(in,1)._VECTptr;
    EACHW(it,w) *it=norm(*it);
    in=symbolic(at_of,makesequence(arg(in,0),gen(w,arg(in,1).subtype)));
  }
  gen r=mergek(nest(norm(serfix(in))));
  raw_=false;
  return r;
}
// ---------------------------------------------------------------- equations (DeltaCE)
// A linear or quadratic equation as a textbook solves it: both sides changed alike, one move a
// step; a quadratic always by the formula (the user's choice), its values put in as they are.
static NOINL gen co(const gen & p,int k){ // the coefficient of x^k in p, a polynomial of degree <= 2
  gen d=k?D(p,X):p;
  if (k==2) d=D(d,X)/2;
  return ev(sub(d,X,0));
}
static NOINL gen lcd(const gen & a,const gen & b,const gen & c,const gen & d){ // of the fractions
  gen v[4]={a,b,c,d},k=1;
  for (int i=0;i<4;++i) if (v[i].type==_FRAC) k=lcm(k,v[i]._FRACptr->den);
  return k;
}
static NOINL bool written(const gen & a,const gen & b){ return a.print(CTX)==b.print(CTX); } // (as written)
// both sides of A = B with c added (sign 1) or taken away (-1): 3x+2-2 = 11-2, no parentheses
static NOINL void both(int r,const gen & A,const gen & B,const gen & c,int sign){
  vecteur a,b;
  flat(at_plus,A,a); flat(at_plus,B,b);
  const gen t=sign>0?c:symbolic(at_neg,c);
  a.push_back(t); b.push_back(t);
  S(r,symbolic(at_plus,gen(a,_SEQ__VECT)),symbolic(at_plus,gen(b,_SEQ__VECT)));
}
static NOINL bool eq_linear(gen L,gen R){
  gen Lx=ev(expand(L,CTX)),Rx=ev(expand(R,CTX));
  gen k=lcd(co(Lx,1),co(Lx,0),co(Rx,1),co(Rx,0));
  if (!is_one(k)){ // fractions first
    S(R_ALCD,L,R,k);
    Lx=ev(expand(k*Lx,CTX)); Rx=ev(expand(k*Rx,CTX));
    S(R_ACONT,Lx,Rx);
  }
  else if (!written(Lx,L) || !written(Rx,R)) S(R_ADIST,Lx,Rx);
  gen q=ratnormal(Rx-co(Rx,1)*X-co(Rx,0),CTX);
  if (!is_zero(q)){ // x^2+2x = x^2+5: the x^2 terms cancel
    both(R_ASUB,Lx,Rx,q,-1);
    Lx=ev(expand(Lx-q,CTX)); Rx=ev(expand(Rx-q,CTX));
    S(R_ACONT,Lx,Rx);
  }
  gen aL=co(Lx,1),bL=co(Lx,0),aR=co(Rx,1),bR=co(Rx,0);
  if (is_zero(aL) && !is_zero(aR)){ // 5 = 3x-1: the x terms on the left
    rel_=flip(rel_); // (5 < 3x-1 is 3x-1 > 5)
    S(R_ASWAP,Rx,Lx);
    gen t=Lx; Lx=Rx; Rx=t; t=aL; aL=aR; aR=t; t=bL; bL=bR; bR=t;
  }
  if (!is_zero(aR)){ // the x terms to the left
    gen t=ev(aR*X);
    if (is_positive(aR,CTX)) both(R_ASUB,Lx,Rx,t,-1); else both(R_AADD,Lx,Rx,ev(-t),1);
    aL=ev(aL-aR);
    Lx=ev(aL*X+bL); Rx=bR;
    S(R_ACONT,Lx,Rx);
  }
  if (is_zero(aL)){ S((rel_?is_one(ev(symbolic(*relop(rel_),makesequence(bL,bR)))):is_zero(ev(bR-bL)))?R_ATRUE:R_AFALSE,bL,bR); return true; }
  if (!is_zero(bL)){ // the numbers to the right
    if (is_positive(bL,CTX)) both(R_ASUB,Lx,Rx,bL,-1); else both(R_AADD,Lx,Rx,ev(-bL),1);
    Lx=ev(aL*X); Rx=ev(bR-bL);
    S(R_ACONT,Lx,Rx);
  }
  if (!is_one(aL)){
    const bool neg=rel_ && is_strictly_positive(-aL,CTX); // (an inequality: by a negative, it flips)
    if (neg) rel_=flip(rel_);
    S(neg?R_NFLIP:R_ADIV,Lx,Rx,aL);
    S(R_ACONT,X,ratnormal(Rx/aL,CTX));
  }
  return true;
}
// The quadratic formula's lines are written as text (an identifier's name prints as is): giac
// prints (-5)^2-4*1*6 as ((-5)^2)-(4*1*6) and -(-5) as 5
static NOINL gen txt(const std::string & s){ return gen(identificateur(s)); }
static NOINL std::string pr(const gen & g){ return g.print(CTX); }
static NOINL std::string par(const gen & g){ // in parentheses when negative or not one term
  std::string s=pr(g);
  if (s[0]=='-' || strchr(s.c_str()+1,'+') || strchr(s.c_str()+1,'-') || strchr(s.c_str(),'/')){ std::string t="("; t+=s; t+=")"; return t; }
  return s;
}
static NOINL std::string base(const gen & g){ // a power's base: (x^2)^2, (2*x)^2
  std::string s=pr(g);
  if (s[0]=='-' || s.find_first_of("+-*/^",1)!=std::string::npos){ std::string t="("; t+=s; t+=")"; return t; }
  return s;
}
// A line written from a pattern: %k value k as giac prints it, #k in parentheses unless one term,
// $k as a power's base, ~ the plus-minus sign, @k the relation numbered k (DeltaCE: 1 < ... 4 >=). One function for every line: each built with
// strings at its place made the factoring 25 KB.
static NOINL gen line(const char * f,const gen & a,const gen & b,const gen & c,const gen & d,const gen & e,const gen & g){
  const gen * v[6]={&a,&b,&c,&d,&e,&g};
  std::string s;
  for (;*f;++f){
    const char k=*f;
    if ((k=='%' || k=='#' || k=='$') && f[1]>='0' && f[1]<='5'){
      const gen & x=*v[*++f-'0'];
      s+=k=='%'?pr(x):k=='#'?par(x):base(x);
    }
    else if (k=='~') s+="\xc2\xb1";
    else if (k=='@' && f[1]>='0' && f[1]<='5'){ const gen & x=*v[*++f-'0']; s+=RELT[x.type==_INT_ && x.val>=0 && x.val<=4?x.val:0]; } // (DeltaCE: @k relation k)
    else s+=k;
  }
  return txt(s);
}
static NOINL void PL(int r,const char * f,const gen & a,const gen & b=zero,const gen & c=zero,const gen & d=zero,const gen & e=zero,const gen & g=zero){
  S(r,line(f,a,b,c,d,e,g),0);
}
static NOINL bool eq_quadratic(const gen & L,const gen & R){
  gen p=ev(expand(ev(L-R),CTX));
  if (is_strictly_positive(-co(p,2),CTX)) p=ev(expand(ev(R-L),CTX)); // (to the side where x^2 is positive: x^2-3x-2 = 0)
  if (!is_zero(R) || !written(p,L)) S(R_QSTD,p,0);
  gen k=lcd(co(p,2),co(p,1),co(p,0),0);
  if (!is_one(k)){
    S(R_ALCD,p,0,k);
    p=ev(expand(k*p,CTX));
    S(R_ACONT,p,0);
  }
  const gen a=co(p,2),b=co(p,1),c=co(p,0),d=ev(b*b-4*a*c);
  S(R_QABC,a,b,c);
  // x = (-(b) +- sqrt(b^2-4ac))/(2a), the values as they are
  PL(R_QFORM,is_zero(b)?"%0=(~sqrt(0^2-4*#1*#2))/(2*#1)":"%0=(-#3~sqrt(#3^2-4*#1*#2))/(2*#1)",X,a,c,b);
  PL(R_QDISC,"#0^2-4*#1*#2=%3",b,a,c,d);
  if (is_strictly_positive(-d,CTX)){ S(R_QNONE,d,0); return true; }
  const gen n=ev(-b),e=ev(2*a),r=ev(symbolic(at_sqrt,d)); // r: sqrt(12) is 2 sqrt(3)
  const bool z=is_zero(n); // (b = 0: +-, not 0 +-)
  PL(R_QSIMP,z?"%0=(~sqrt(%1))/#2":"%0=(%3~sqrt(%1))/#2",X,d,e,n);
  if (is_zero(d)){ S(R_QONE,ratnormal(n/e,CTX),0); return true; }
  const bool lo=is_positive(e,CTX); // the smaller solution first, as the console lists them
  if (r.type==_INT_ || r.type==_FRAC){ // a perfect square: the two fractions, then their values
    PL(R_ALINE,z?"%0=(~%1)/#2":"%0=(%3~%1)/#2",X,r,e,n);
    const gen m=line(z?"(-%1)/#2":"(%0-%1)/#2",n,r,e,zero,zero,zero),pl=line(z?"(%1)/#2":"(%0+%1)/#2",n,r,e,zero,zero,zero);
    S(R_QTWO,lo?m:pl,lo?pl:m);
    S(R_QTWO2,ratnormal((n+(lo?-r:r))/e,CTX),ratnormal((n+(lo?r:-r))/e,CTX));
  }
  else { // the root simplified (sqrt(24) is 2 sqrt(6)), x = p +- q, then the two
    if (!written(r,symbolic(at_sqrt,d))) PL(R_ALINE,z?"%0=(~%1)/#2":"%0=(%3~%1)/#2",X,r,e,n);
    const gen P=ratnormal(n/e,CTX),Q=normal(ev(abs(r/e,CTX)),CTX);
    PL(R_ALINE,is_zero(P)?"%0=~%2":"%0=%1~%2",X,P,Q);
    S(R_QTWO,ev(P-Q),ev(P+Q));
  }
  return true;
}
// ---------------------------------------------------------------- factoring (DeltaCE)
// A polynomial in x factored as a textbook does it: the greatest common factor, then each factor by
// its shape (two terms: squares, cubes; three: a perfect square, two numbers, the formula's roots;
// four: grouping; else a rational root and a division), each step's result then the whole. Shown
// only when it ends where giac's factor does (the console's answer): no partial factoring.
static vecteur * F_;   // the factors so far, their product the polynomial
static gen * P0_;      // the polynomial
static NOINL gen prodq(const vecteur & v){ // the factors' product as written: x(x-3)^2, -(x-2)(x-3)
  vecteur w;
  bool neg=false;
  for (unsigned i=0;i<v.size();++i){ // equal factors together, where the first one is
    if (is_one(v[i])) continue;
    if (is_minus_one(v[i])){ neg=!neg; continue; }
    unsigned j=0,n=0;
    while (j<i && v[j]!=v[i]) ++j;
    if (j<i) continue;
    for (j=i;j<v.size();++j) n+=v[j]==v[i];
    w.push_back(n>1?symbolic(at_pow,makesequence(v[i],int(n))):v[i]);
  }
  gen r=w.empty()?gen(1):w.size()==1?w[0]:symbolic(at_prod,gen(w,_SEQ__VECT));
  return neg?symbolic(at_neg,r):r;
}
static NOINL void whole(){ S(R_ALINE,symbolic(at_equal,makesequence(*P0_,prodq(*F_))),0); }
static NOINL vecteur coeffs(const gen & q){ gen c=_coeff(makesequence(q,X),CTX); return c.type==_VECT?*c._VECTptr:vecteur(); }
static NOINL bool root_of(const gen & c,int k,gen & r){ // c = r^k, r a whole number or a fraction (k = 3: signed)
  if (c.type==_FRAC){
    gen a,b;
    if (!root_of(c._FRACptr->num,k,a) || !root_of(c._FRACptr->den,k,b)) return false;
    r=ev(a/b);
    return true;
  }
  if (c.type!=_INT_ || !c.val || (k==2 && c.val<0)) return false;
  const long s=c.val<0?-1:1,v=s*(long)c.val;
  const long t=(long)(k==2?sqrt((double)v):exp(log((double)v)/3))+0.5; // (then checked exactly)
  for (long u=t>0?t-1:0;u<=t+1;++u)
    if ((k==2?u*u:u*u*u)==v){ r=gen((int)(s*u)); return true; }
  return false;
}
static NOINL gen up(int k){ return k==1?X:symbolic(at_pow,makesequence(X,k)); }
// two terms: a^2 u^2 - b^2, a^3 u^3 +- b^3
static NOINL bool two_terms(const gen & q,const vecteur & c,vecteur & rep){
  const int n=c.size()-1;
  gen a,b;
  if (n%2==0 && root_of(c[0],2,a) && root_of(ev(-c[n]),2,b)){
    const gen A=ev(a*up(n/2)),f1=ev(A-b),f2=ev(A+b);
    PL(R_FDSQ,"%0=$1^2-#2^2=(%3)*(%4)",q,A,b,f1,f2);
    rep.push_back(f1); rep.push_back(f2);
    return true;
  }
  if (n%3==0 && root_of(c[0],3,a) && root_of(c[n],3,b)){
    const gen A=ev(a*up(n/3)),f1=ev(A+b),f2=ev(expand(A*A-A*b+b*b,CTX));
    const bool sum=is_positive(b,CTX);
    PL(sum?R_FSUMC:R_FDIFC,sum?"%0=$1^3+#2^3=(%3)*(%4)":"%0=$1^3-#2^3=(%3)*(%4)",q,A,ev(abs(b,CTX)),f1,f2);
    rep.push_back(f1); rep.push_back(f2);
    return true;
  }
  return false;
}
// two whole numbers m, n with m n = A C and m + n = B (|A C| up to 8 million: 24-bit ints)
static NOINL bool pair_for(const gen & A,const gen & B,const gen & C,int & m,int & n){
  if (A.type!=_INT_ || B.type!=_INT_ || C.type!=_INT_) return false;
  const long ac=(long)A.val*C.val,aa=ac<0?-ac:ac;
  if (aa>8000000L) return false;
  for (long d=1;d*d<=aa;++d){
    if (aa%d) continue;
    const long e=aa/d,cand[4][2]={{d,ac/d},{-d,-(ac/d)},{e,ac/e},{-e,-(ac/e)}};
    for (int j=0;j<4;++j) if (cand[j][0]+cand[j][1]==B.val){ m=(int)cand[j][0]; n=(int)cand[j][1]; return true; }
  }
  return false;
}
// A u^2 + B u + C: a perfect square, two numbers (the ac method when A is not 1), else the roots
static NOINL bool trinomial(const gen & q,const vecteur & c,vecteur & rep){
  const int n=c.size()-1,k=n/2;
  const gen A=c[0],B=c[k],C=c[n],u=up(k);
  gen ra,rc;
  if (root_of(A,2,ra) && root_of(C,2,rc) && (B==ev(2*ra*rc) || B==ev(-2*ra*rc))){ // (ra u +- rc)^2
    const gen f=ev(ra*u+(is_positive(B,CTX)?rc:ev(-rc)));
    PL(R_FPSQ,"%0=(%1)^2",q,f);
    rep.push_back(f); rep.push_back(f);
    return true;
  }
  int mi,ni;
  if (pair_for(A,B,C,mi,ni)){
    if (mi%A.val && !(ni%A.val)){ int t=mi; mi=ni; ni=t; } // (the first pair's factor has A's: 3x(x+2)+4(x+2))
    const gen M(mi),N(ni);
    if (is_one(A)){ // u^2 + B u + C = (u+m)(u+n)
      PL(R_FTWO,"#0*#1=%2,#0+#1=%3",M,N,C,B);
      rep.push_back(ev(u+M)); rep.push_back(ev(u+N));
      PL(R_ALINE,"%0=(%1)*(%2)",q,rep[0],rep[1]);
      return true;
    }
    // the ac method: A u^2 + m u + n u + C = g1 u (bin) + G2 (bin)
    gen g1=gcd(A,M);
    if (!is_positive(A,CTX)) g1=ev(-g1);
    const gen bin=ev(A/g1*u+M/g1),G2=ev(N/(A/g1)),gu=ev(g1*u);
    if (!is_zero(ratnormal(G2*bin-N*u-C,CTX))) return false;
    PL(R_FSPLIT,"#0*#1=%2,#3+#4=%5",A,C,ev(A*C),M,N,B);
    S(R_ALINE,symbolic(at_equal,makesequence(q,symbolic(at_plus,gen(makevecteur(ev(A*up(2*k)),ev(M*u),ev(N*u),C),_SEQ__VECT)))),0);
    PL(R_FGROUP,is_positive(G2,CTX)?"%0=%1*(%2)+%3*(%2)":"%0=%1*(%2)-%3*(%2)",q,gu,bin,ev(abs(G2,CTX)));
    rep.push_back(ev(gu+G2)); rep.push_back(bin);
    PL(R_ALINE,"%0=(%1)*(%2)",q,rep[0],rep[1]);
    return true;
  }
  if (k!=1) return false;
  const gen d=ev(B*B-4*A*C); // no whole numbers: the roots, if real (x^2+9: prime)
  if (!is_strictly_positive(d,CTX)) return false;
  const gen r=ev(symbolic(at_sqrt,d)),e=ev(2*A),n0=ev(-B),P=ratnormal(n0/e,CTX),Q=normal(ev(abs(r/e,CTX)),CTX);
  PL(R_FQUAD,is_zero(n0)?"%0=(~sqrt(%2))/#3":"%0=(%1~sqrt(%2))/#3",X,n0,d,e);
  if (is_zero(P)){ // +- q: (x+q)(x-q)
    PL(R_ALINE,"%0=-%1,%0=%1",X,Q);
    PL(R_ALINE,is_one(A)?"%0=(%1+%2)*(%1-%2)":"%0=#3(%1+%2)*(%1-%2)",q,X,Q,A);
  }
  else { // p -+ q as a textbook writes them: 2-sqrt(3), not -sqrt(3)+2
    PL(R_ALINE,"%0=%1-%2,%0=%1+%2",X,P,Q);
    PL(R_ALINE,is_one(A)?"%0=(%1-(%2-%3))*(%1-(%2+%3))":"%0=#4(%1-(%2-%3))*(%1-(%2+%3))",q,X,P,Q,A);
  }
  if (!is_one(A)) rep.push_back(A);
  rep.push_back(ev(X-normal(ratnormal((n0-r)/e,CTX),CTX))); rep.push_back(ev(X-normal(ratnormal((n0+r)/e,CTX),CTX)));
  return true;
}
// four terms: (a x^3 + b x^2) + (c x + d) = g1 x^2 (bin) + G2 (bin)
static NOINL bool grouping(const gen & q,const vecteur & c,vecteur & rep){
  if (!is_zero(ev(c[0]*c[3]-c[1]*c[2])) || is_zero(c[1])) return false;
  gen g1=gcd(c[0],c[1]);
  if (!is_positive(c[0],CTX)) g1=ev(-g1);
  const gen bin=ev(c[0]/g1*X+c[1]/g1),G2=ev(c[2]/(c[0]/g1)),x2=ev(g1*up(2));
  if (!is_zero(ratnormal(G2*bin-c[2]*X-c[3],CTX))) return false;
  PL(R_FGROUP,is_positive(G2,CTX)?"%0=(%1)+(%2)=%3*(%4)+%5*(%4)":"%0=(%1)+(%2)=%3*(%4)-%5*(%4)",q,ev(c[0]*up(3)+c[1]*up(2)),ev(c[2]*X+c[3]),x2,bin,ev(abs(G2,CTX)));
  rep.push_back(ev(x2+G2)); rep.push_back(bin);
  PL(R_ALINE,"%0=(%1)*(%2)",q,rep[0],rep[1]);
  return true;
}
// a rational root (giac's factor finds it): its factor, then the quotient
static NOINL bool rational_root(const gen & q,vecteur & rep){
  vecteur fs;
  flat(at_prod,_factor(q,CTX),fs);
  EACH(it,fs){
    const gen g=it->is_symb_of_sommet(at_pow)?it->_SYMBptr->feuille[0]:*it;
    const vecteur cg=coeffs(g);
    if (cg.size()!=2 || (cg[0].type!=_INT_ && cg[0].type!=_FRAC) || (cg[1].type!=_INT_ && cg[1].type!=_FRAC)) continue;
    const gen r=ratnormal(-cg[1]/cg[0],CTX),quo=ev(_quo(makesequence(q,g,X),CTX));
    PL(R_FROOT,"%0=0",subst(q,X,txt(par(r)),false,CTX));
    PL(R_ALINE,"%0=(%1)*(%2)",q,g,quo);
    rep.push_back(g); rep.push_back(quo);
    return true;
  }
  return false;
}
// the factor at i in F_ factored further; returns how many factors it became
static int ffac(unsigned i,int depth){
  const gen q=(*F_)[i];
  const vecteur c=coeffs(q);
  const int n=c.size()-1;
  if (n<2 || depth>6 || ctrl_c || interrupted) return 1;
  int terms=0;
  for (int j=0;j<=n;++j) terms+=!is_zero(c[j]);
  vecteur rep;
  if (!(terms==2 && two_terms(q,c,rep))
      && !(n%2==0 && (terms==3 || (terms==2 && n==2)) && (n==2 || !is_zero(c[n/2])) && trinomial(q,c,rep))
      && !(terms==4 && n==3 && grouping(q,c,rep)))
    rational_root(q,rep);
  if (rep.empty()) return 1;
  const bool others=F_->size()>1; // (the polynomial itself: its line was the whole)
  for (unsigned j=F_->size();j-->i+1;) // its copies ((x^2-1)^2) factored alike
    if ((*F_)[j]==q){ F_->erase(F_->begin()+j); F_->insert(F_->begin()+j,rep.begin(),rep.end()); }
  F_->erase(F_->begin()+i);
  F_->insert(F_->begin()+i,rep.begin(),rep.end());
  if (others) whole();
  int cnt=0;
  for (unsigned j=0;j<rep.size();++j) cnt+=ffac(i+cnt,depth+1);
  return cnt;
}
// the number of non-constant factors of g, giac's factor's form (a power counted by its exponent)
static NOINL int nfactors(const gen & g){
  vecteur fs;
  flat(at_prod,g,fs);
  int n=0;
  EACH(it,fs){
    gen b=*it,e=1;
    if (b.is_symb_of_sommet(at_neg)) b=b._SYMBptr->feuille;
    if (b.is_symb_of_sommet(at_pow)){ e=b._SYMBptr->feuille[1]; b=b._SYMBptr->feuille[0]; }
    if (dep(b)) n+=e.type==_INT_?e.val:1;
  }
  return n;
}
// p factored with steps (F_: its factors); false if they do not end where giac's factor does
static NOINL bool fac_steps(const gen & p){
  static vecteur * f;
  static gen * p0;
  if (!f){ f=new vecteur; p0=new gen; }
  F_=f; P0_=p0; *p0=p; f->clear();
  vecteur c=coeffs(p);
  if (c.size()<3){ if (c.size()==2) S(R_FPRIME2,p,0); return c.size()==2; } // (7x, x+1: already factored)
  gen num=0,den=1; // the content: the gcd of the numerators over the lcm of the denominators
  int low=c.size()-1;
  for (unsigned j=0;j<c.size();++j){
    if (is_zero(c[j])) continue;
    if (c[j].type!=_INT_ && c[j].type!=_ZINT && c[j].type!=_FRAC) return false; // (whole numbers, fractions)
    num=gcd(num,c[j].type==_FRAC?c[j]._FRACptr->num:c[j]);
    if (c[j].type==_FRAC) den=lcm(den,c[j]._FRACptr->den);
    low=c.size()-1-j;
  }
  gen g=ev(num/den);
  if (!is_positive(c[0],CTX)) g=ev(-g);
  gen r=ev(expand(ratnormal(low>0?p/(g*up(low)):p/g,CTX),CTX));
  if (!is_one(g)) f->push_back(g);
  if (low>0) f->push_back(up(low));
  f->push_back(r);
  if (f->size()>1) S(R_FGCF,symbolic(at_equal,makesequence(p,prodq(*f))),0);
  const size_t m=out_->size();
  ffac(f->size()-1,0);
  if (out_->size()==m && f->size()==1 && is_zero(normal(_factor(p,CTX)-p,CTX))) // prime: x^2+9 (no real roots), x^3+x+1
    S(c.size()==3 && is_strictly_positive(ev(4*c[0]*c[2]-c[1]*c[1]),CTX)?R_FPRIME:R_FPRIME2,p,0);
  gen gf=_factor(p,CTX);
  int n=0;
  EACH(it,*f) if (dep(*it)) n+=it->is_symb_of_sommet(at_pow)?it->_SYMBptr->feuille[1].val:1;
  return !ctrl_c && !interrupted && n==nfactors(gf) && is_zero(normal(symbolic(at_prod,gen(*f,_SEQ__VECT))-p,CTX)); // (normal: sqrt(2)^2 is 2)
}
static NOINL bool factor_steps(const gen & input){
  gen e=ev(arg(input).type==_VECT?arg(input,0):arg(input));
  vecteur ids=lidnt(e);
  gen v=0;
  EACH(it,ids) if (evalf(*it,1,CTX).type!=_DOUBLE_){ if (!is_zero(v)) return false; v=*it; }
  if (is_zero(v)) return false;
  X=v;
  e=ev(expand(e,CTX));
  const gen d=ratnormal(e,CTX);
  if (dep(_denom(d,CTX)) || !polyf(_numer(d,CTX))) return false; // (x^2/2-2: a polynomial too)
  return fac_steps(e) && !out_->empty();
}
// degree 3 and more: one side, factored, each factor 0, each solved (a quadratic factor left has
// no real root: the factoring goes as far as the real roots)
static NOINL bool eq_factor(const gen & L,const gen & R){
  gen p=ev(expand(ev(L-R),CTX));
  if (!is_zero(R) || !written(p,L)) S(R_QSTD,p,0);
  if (!fac_steps(p)) return false;
  std::string z,v;
  vecteur none;
  for (unsigned i=0;i<F_->size();++i){
    gen f=(*F_)[i];
    if (!dep(f) || (i && f==(*F_)[i-1])) continue;
    if (f.is_symb_of_sommet(at_pow)) f=f._SYMBptr->feuille[0]; // (x^2 = 0: x = 0)
    vecteur cf=coeffs(f);
    if (!z.empty()) z+=",";
    z+=pr(f); z+="=0";
    if (cf.size()==2){
      if (!v.empty()) v+=",";
      v+=pr(X); v+="="; v+=pr(normal(ratnormal(-cf[1]/cf[0],CTX),CTX));
    }
    else none.push_back(f);
  }
  S(R_FZERO,txt(z),0);
  if (!v.empty()) S(R_FSOLVE,txt(v),0);
  EACH(it,none) S(R_FNOREAL,*it,0);
  return true;
}
// a polynomial equation L = R in x (numeric coefficients): its steps by its degree
static NOINL bool eq_poly(const gen & L,const gen & R){
  const gen d=ratnormal(L-R,CTX);
  if (dep(_denom(d,CTX)) || dep(_denom(ratnormal(L,CTX),CTX)) || dep(_denom(ratnormal(R,CTX),CTX))) return false; // (x/(x-1) = 1/(x-1)+2: its sides)
  vecteur k=lvarx(_numer(d,CTX),X),ids=lidnt(d);
  if (k.size()>1 || (k.size()==1 && k[0]!=X) || !polyf(_numer(d,CTX))) return false; // (not a polynomial)
  EACH(it,ids) if (*it!=X && evalf(*it,1,CTX).type!=_DOUBLE_) return false; // (other letters: a x + b = 0; pi is a number)
  const gen deg=k.empty()?gen(0):_degree(makesequence(_numer(d,CTX),X),CTX);
  if (deg==2) return eq_quadratic(L,R);
  if (deg.type==_INT_ && deg.val>2) return eq_factor(L,R);
  if (deg.type==_INT_ && deg.val<2 && L.type!=_VECT) return eq_linear(L,R);
  return false;
}
// the real solutions of the polynomial equation L = R, exact
static NOINL vecteur roots_of(const gen & L,const gen & R){
  vecteur r;
  const gen s=_solve(makesequence(symbolic(at_equal,makesequence(ev(L-R),0)),X),CTX);
  double d;
  if (s.type==_VECT) EACH(it,*s._VECTptr){ gen z=evalf(*it,1,CTX); if (z.type==_DOUBLE_ || z.type==_INT_) r.push_back(normal(*it,CTX)); } // ((3-sqrt(17))/2, not -(-3+sqrt(17))/2)
  (void)d;
  return r;
}
static NOINL gen eqseq(const vecteur & v,const unary_function_ptr * op){ // x=a, x=b (op at_equal) or x!=a, ...
  vecteur w;
  EACH(it,v) w.push_back(symbolic(*op,makesequence(X,*it)));
  return w.size()==1?w.front():gen(w,_SEQ__VECT);
}
// each term of a side times the LCD, simplified: 2x(x+1)(1/x+1/(x+1)) is 2(x+1)+2x
static NOINL gen termwise(const gen & side,const gen & lcd){
  vecteur t,w;
  flat(at_plus,side,t);
  EACH(it,t){
    const gen r=ratnormal(lcd*(*it),CTX),f=_factor(r,CTX);
    w.push_back(taille(f,100)<=taille(r,100)?f:r);
  }
  return w.size()==1?w.front():symbolic(at_plus,gen(w,_SEQ__VECT));
}
// x in a denominator: the excluded values, both sides times the LCD, the polynomial equation, the
// solutions checked against the excluded values
static NOINL bool eq_rational(const gen & L,const gen & R){
  const gen lcd=ratnormal(_lcm(makesequence(_denom(ratnormal(L,CTX),CTX),_denom(ratnormal(R,CTX),CTX)),CTX),CTX);
  if (!dep(lcd)) return false;
  const vecteur ex=roots_of(lcd,0);
  if (!ex.empty()) S(R_REXCL,eqseq(ex,at_different),0);
  const gen lf=_factor(lcd,CTX);
  S(R_RLCD,symbolic(at_equal,makesequence(symbolic(at_prod,makesequence(lf,L)),symbolic(at_prod,makesequence(lf,R)))),0);
  const gen Ls=termwise(L,lcd),Rs=termwise(R,lcd);
  S(R_ALINE,symbolic(at_equal,makesequence(Ls,Rs)),0);
  if (!eq_poly(Ls,Rs)) return false;
  vecteur ok,bad;
  const vecteur c=roots_of(Ls,Rs);
  EACH(it,c) (is_zero(ratnormal(subst(lcd,X,*it,false,CTX),CTX))?bad:ok).push_back(*it);
  if (!ok.empty()) S(R_RKEEP,eqseq(ok,at_equal),0);
  if (!bad.empty()) S(R_RDROP,eqseq(bad,at_equal),0);
  return true;
}
// a square root of x in g: sqrt(u), u with x
static NOINL bool is_root(const gen & t){
  if (!t.is_symb_of_sommet(at_pow) || arg(t).type!=_VECT) return false;
  const gen e=arg(t,1);
  return e.type==_FRAC && is_one(e._FRACptr->num) && e._FRACptr->den==2 && dep(arg(t,0));
}
static NOINL int nroots(const gen & g){ int n=0; vecteur p=lop(g,at_pow); EACH(it,p) n+=is_root(*it); return n; }
// side = c sqrt(u) + o: its root term's root (c, o set), 0 if none
static NOINL gen root_term(const gen & side,gen & c,gen & o){
  vecteur t,rest;
  flat(at_plus,side,t);
  gen s=0;
  EACH(it,t){
    if (!is_zero(s) || !nroots(*it)){ rest.push_back(*it); continue; }
    if (is_root(*it)){ s=*it; c=1; continue; }
    vecteur f; // c sqrt(u): a number times the root
    flat(at_prod,*it,f);
    gen k=1,r=0;
    EACH(jt,f) if (is_root(*jt) && is_zero(r)) r=*jt; else k=k*(*jt);
    if (is_zero(r) || dep(k)) return 0;
    s=r; c=ev(k);
  }
  o=rest.empty()?gen(0):rest.size()==1?rest.front():symbolic(at_plus,gen(rest,_SEQ__VECT));
  return s;
}
// square roots: one alone on its side, both sides squared (twice when a root is left: sqrt(x) +
// sqrt(x+5) = 5), the polynomial equation, each candidate put back in the equation as given (Lc = Rc:
// squaring can add solutions)
static NOINL bool eq_radical(const gen & L0,const gen & R0,const gen & Lc,const gen & Rc,int depth){
  gen L=L0,R=R0;
  if (!nroots(L)){ S(R_ASWAP,R,L); gen t=L; L=R; R=t; }
  if (depth>1 || nroots(L)+nroots(R)>2) return false;
  gen c,o;
  const gen s=root_term(L,c,o);
  if (is_zero(s)) return false;
  if (!is_zero(o)){ // the root alone (the other terms, another root too, to the other side)
    const gen on=ev(-o);
    if (o.type<_IDNT && is_strictly_positive(-o,CTX)) both(R_AADD,L,R,on,1); else both(R_ASUB,L,R,o,-1);
    L=ev(c*s); R=ratnormal(R-o,CTX);
    S(R_ACONT,L,R);
  }
  if (!is_one(c)){
    S(R_ADIV,L,R,c);
    L=s; R=ratnormal(R/c,CTX);
    S(R_ACONT,L,R);
  }
  const gen u=arg(s,0),R2=ratnormal(ev(expand(R*R,CTX)),CTX);
  PL(R_RSQ,"(%0)^2=(%1)^2",L,R);
  S(R_ACONT,u,R2);
  if (nroots(u)+nroots(R2)) return eq_radical(u,R2,Lc,Rc,depth+1); // (a root left: again)
  if (!eq_poly(u,R2)) return false;
  const vecteur cand=roots_of(u,R2);
  int good=0;
  EACH(it,cand){ // both sides of the equation as given, at the candidate
    const gen p=txt(par(*it)),lq=subst(Lc,X,p,false,CTX),rq=subst(Rc,X,p,false,CTX);
    const gen lv=normal(ev(subst(Lc,X,*it,false,CTX)),CTX),rv=normal(ev(subst(Rc,X,*it,false,CTX)),CTX);
    const bool eq=is_zero(normal(lv-rv,CTX));
    good+=eq;
    PL(eq?R_ROK:R_RBAD,"%0=%1,%2=%3,%4=%5",X,*it,lq,lv,rq,rv);
  }
  if (!good && !cand.empty()) S(R_RNONE,eqseq(cand,at_different),0); // (x != 9: none left)
  return true;
}
// ---------------------------------------------------------------- systems (DeltaCE)
// Linear systems of 2 or 3 equations in as many unknowns, by elimination, as textbooks do: each
// equation as a row (coefficients, then the right side), fractions cleared, a variable eliminated
// from a pair by multiplying them so that its coefficients are opposite (the smallest multiple),
// the equations added or subtracted, the one left solved, the values put back.
typedef vecteur row; // a_0 .. a_{n-1}, c: a_0 v_0 + ... = c
static const vecteur * yv_; // the unknowns
static NOINL gen yeq(const row & r){ // the row as an equation (2x+3y=12)
  gen l=0;
  const int n=r.size()-1;
  for (int i=0;i<n;++i) if (!is_zero(r[i])) l=l+r[i]*(*yv_)[i];
  return symbolic(at_equal,makesequence(ev(l),r[n]));
}
static NOINL void yshow(int rule,const row & r){ // (a line going on: its sides, the template A=B)
  const gen e=yeq(r);
  if (rule==R_ACONT) S(rule,arg(e,0),arg(e,1)); else S(rule,e,0);
}
static NOINL gen yneg(const gen & g){ return pr(g)[0]=='-'?txt(par(g)):g; } // (3*(-1))
static NOINL void ycont(int rule,const gen & e){ if (rule==R_ACONT) S(rule,arg(e,0),arg(e,1)); else S(rule,e,0); }
static NOINL row yscale(const row & r,const gen & m){ row o(r); EACHW(it,o) *it=ev(*it*m); return o; }
static NOINL row ycomb(const row & a,const row & b,int sg){ row o(a); for (unsigned i=0;i<o.size();++i) o[i]=ev(a[i]+sg*b[i]); return o; }
// eliminates unknown k from rows a and b (both have it): the row without it, steps shown
static NOINL row yelim(const row & a,const row & b,int k){
  const gen p=a[k],q=b[k];
  const gen ap=abs(p,CTX),aq=abs(q,CTX),L=lcm(ap,aq);
  gen m1=ev(L/ap),m2=ev(L/aq);
  const bool same=is_positive(p,CTX)==is_positive(q,CTX);
  if (!is_one(m1) || !is_one(m2)){
    if (!is_one(m1)){ ycont(R_YMUL,symbolic(at_equal,makesequence(symbolic(at_prod,makesequence(m1,arg(yeq(a),0))),symbolic(at_prod,makesequence(m1,yneg(a.back())))))); yshow(R_ACONT,yscale(a,m1)); }
    if (!is_one(m2)){ ycont(is_one(m1)?R_YMUL:R_ACONT,symbolic(at_equal,makesequence(symbolic(at_prod,makesequence(m2,arg(yeq(b),0))),symbolic(at_prod,makesequence(m2,yneg(b.back())))))); yshow(R_ACONT,yscale(b,m2)); }
  }
  const row o=ycomb(yscale(a,m1),yscale(b,m2),same?-1:1);
  yshow(same?R_YSUBT:R_YADD,o);
  return o;
}
static NOINL int ynz(const row & r){ int n=0; for (unsigned i=0;i+1<r.size();++i) if (!is_zero(r[i])) ++n; return n; }
// the value of the unknown left in r (one coefficient): shown divided; false: 0 = c
static NOINL bool ysolve1(const row & r,gen & val,int & k){
  const int n=r.size()-1;
  k=-1;
  for (int i=0;i<n;++i) if (!is_zero(r[i])) k=i;
  if (k<0) return false;
  val=ev(r[n]/r[k]);
  if (is_one(r[k])) return true; // (shown already: x=2)
  if (!is_minus_one(r[k])) S(R_ADIV,arg(yeq(r),0),r[n],r[k]); // (-y=2: y=-2 at once)
  S(R_ACONT,(*yv_)[k],val);
  return true;
}
// row r with the values known (vals[i] not undef) put in, then solved for its one other unknown
static NOINL bool yback(const row & r,vecteur & vals){
  const int n=r.size()-1;
  gen l=0,shown_=0,rest=r[n];
  int k=-1;
  for (int i=0;i<n;++i){
    if (is_zero(r[i])) continue;
    if (!is_undef(vals[i])){ // (its value as is, negative in parentheses: x-2, 3*(-2), -(-1))
      const gen w=pr(vals[i])[0]=='-' && !(is_zero(shown_) && is_one(r[i]))?txt(par(vals[i])):vals[i];
      shown_=shown_+(is_one(r[i])?w:is_minus_one(r[i])?symbolic(at_neg,w):symbolic(at_prod,makesequence(r[i],w)));
      rest=rest-r[i]*vals[i];
    }
    else { if (k>=0) return false; k=i; shown_=shown_+r[i]*(*yv_)[i]; }
  }
  if (k<0) return false;
  S(R_YBACK,symbolic(at_equal,makesequence(shown_,r[n])),0);
  const gen v=ev(rest/r[k]);
  if (!is_one(r[k])) S(R_ACONT,ev(r[k]*(*yv_)[k]),ev(rest));
  S(R_ACONT,(*yv_)[k],v);
  vals[k]=v;
  return true;
}
// the unknown to eliminate between a and b: the smallest common multiple of its coefficients (the
// last one if equal: y before x, as textbooks do); -1: none in both
static NOINL int ypick(const row & a,const row & b){
  int k=-1; gen best=0;
  for (int i=0;i+1<(int)a.size();++i){
    if (is_zero(a[i]) || is_zero(b[i])) continue;
    const gen L=lcm(abs(a[i],CTX),abs(b[i],CTX));
    if (k<0 || !is_greater(L,best,CTX)){ k=i; best=L; }
  }
  return k;
}
// 2 or 3 rows: steps to the values (vals, undef: unknown); 0 solved, 1 no solution, 2 infinitely many
static NOINL int ysys(std::vector<row> & R,vecteur & vals){
  if (R.size()==2){
    const int k=ypick(R[0],R[1]);
    if (k<0) return -1;
    const row o=yelim(R[0],R[1],k);
    if (!ynz(o)){ // 0 = c: never true (0 != -2), or always (the equations one line: x+2y=4)
      const bool inf=is_zero(o.back()),three=yv_->size()>2;
      if (!inf) S(three?R_YNONE3:R_YNONE,symbolic(at_different,makesequence(0,o.back())),0);
      else { gen g=0; EACH(it,R[0]) g=gcd(g,*it); S(three?R_YINF3:R_YINF,yeq(yscale(R[0],ev(inv(g,CTX)))),0); }
      return inf?2:1;
    }
    gen v; int j;
    if (ynz(o)>1) return -1;
    ysolve1(o,v,j);
    vals[j]=v;
    int o_=-1; // the other unknown
    for (int i=0;i+1<(int)R[0].size();++i) if (i!=j && is_undef(vals[i]) && (!is_zero(R[0][i]) || !is_zero(R[1][i]))) o_=i;
    if (o_<0) return -1;
    // back into the equation where it has the smaller coefficient (1 rather than 3)
    const int q=is_zero(R[0][o_]) || (!is_zero(R[1][o_]) && is_greater(abs(R[0][o_],CTX),abs(R[1][o_],CTX),CTX))?1:0;
    if (!yback(R[q],vals) && !yback(R[1-q],vals)) return -1;
    return 0;
  }
  // three: eliminate one unknown from the pairs (1,2) and (1,3), then the two left; an equation
  // without an unknown already (x-y=0) is kept, the unknown eliminated from the other two
  int k=-1,i0=0,i1=1,i2=2;
  for (int j=0;j<3 && k<0;++j) for (int v=0;v<3;++v){
    if (!is_zero(R[j][v])) continue;
    const int a_=(j+1)%3,b_=(j+2)%3;
    if (is_zero(R[a_][v]) || is_zero(R[b_][v])) continue;
    k=v; i0=a_; i1=b_; i2=j; break;
  }
  std::vector<row> two;
  if (k>=0){
    yshow(R_YPAIR,R[i0]); yshow(R_ACONT,R[i1]);
    two.push_back(yelim(R[i0],R[i1],k)); two.push_back(R[i2]);
  }
  else {
    k=ypick(R[0],R[1]);
    if (k<0 || is_zero(R[2][k])) return -1;
    yshow(R_YPAIR,R[0]); yshow(R_ACONT,R[1]);
    two.push_back(yelim(R[0],R[1],k));
    yshow(R_YPAIR,R[0]); yshow(R_ACONT,R[2]);
    two.push_back(yelim(R[0],R[2],k));
  }
  for (int j=0;j<2;++j) if (!ynz(two[j])){ // (0 = c already)
    const bool inf=is_zero(two[j].back());
    if (!inf){ S(R_YNONE3,symbolic(at_different,makesequence(0,two[j].back())),0); return 1; }
    yshow(R_YINF3,R[i0]); yshow(R_ACONT,two[1-j]); // (an equation fewer: these two left)
    return 2;
  }
  const row & a=two[0], & b=two[1];
  // the pair without unknown k: two unknowns left (rows of the same length, k's coefficient 0)
  yshow(R_YTWO,a); yshow(R_ACONT,b);
  ++depth_;
  const int r=ysys(two,vals);
  --depth_;
  if (r) return r;
  for (int i=0;i<3;++i) if (yback(R[i],vals)) return 0;
  return -1;
}
static NOINL bool sys_steps(const vecteur & eqs,const vecteur & vars){
  const int n=vars.size();
  if (n<2 || n>3 || (int)eqs.size()!=n) return false;
  EACH(it,vars) if (it->type!=_IDNT) return false;
  yv_=&vars;
  std::vector<row> R;
  bool std_=true,frac=false;
  EACH(it,eqs){ // each equation's row: its coefficients (constants: linear) and its constant
    const gen e=*it;
    const gen d=ev(is(e,at_equal)?arg(e,0)-arg(e,1):e);
    row r(n+1);
    gen c=d;
    for (int i=0;i<n;++i){
      r[i]=ev(derive(d,vars[i],CTX));
      EACH(v,vars) if (dep(r[i],*v)) return false;
      c=sub(c,vars[i],0);
    }
    r[n]=ev(-c);
    EACH(v,r) if (v->type!=_INT_ && v->type!=_ZINT && v->type!=_FRAC) return false; // (numbers: 2/3, not decimals)
    EACH(v,r) frac=frac || v->type==_FRAC;
    if (!is(e,at_equal) || !written(ev(e),yeq(r))) std_=false;
    R.push_back(r);
  }
  if (!std_){ yshow(R_YSTD,R[0]); for (int i=1;i<n;++i) yshow(R_ACONT,R[i]); }
  if (frac){ // each row times the LCD of its coefficients
    bool any=false;
    for (int i=0;i<n;++i){
      gen k=1;
      EACH(v,R[i]) if (v->type==_FRAC) k=lcm(k,v->_FRACptr->den);
      if (!is_one(k)){ R[i]=yscale(R[i],k); yshow(any?R_ACONT:R_YFRAC,R[i]); any=true; }
    }
  }
  vecteur vals(n,undef);
  const int r=ysys(R,vals);
  if (r<0) return false;
  if (r==0){
    vecteur sol;
    for (int i=0;i<n;++i){ if (is_undef(vals[i])) return false; sol.push_back(symbolic(at_equal,makesequence(vars[i],vals[i]))); }
    S(R_YSOL,gen(sol,_SEQ__VECT),0);
  }
  return true;
}

// ---------------------------------------------------------------- exponential and log equations (DeltaCE)
// x in exponents (2^(x+1) = 8, 3^x = 7, e^(2x)-3e^x+2 = 0) or in logs (log(x)+log(x-3) = 1): the
// power alone, then both sides as powers of one base (the exponents equal) or ln of both sides; the
// logs made one with the log rules, written as a power, the equation left solved, each solution
// checked in the logs (their arguments positive). The sides as typed: log(x) is log10(x) there,
// ln(x)/ln(10) once evaluated.
static NOINL bool el_num(const gen & g,double & d){
  const gen z=evalf(g,1,CTX);
  if (z.type==_DOUBLE_){ d=z._DOUBLE_val; return true; }
  if (z.type==_INT_){ d=z.val; return true; }
  return false;
}
static NOINL bool el_e(const gen & b){ return is(b,at_exp); } // (the base e is exp(1), not evaluated)
static NOINL gen el_pow(const gen & b,const gen & u){ return el_e(b)?SYM(at_exp,u):SYM2(at_pow,b,u); }
static NOINL gen el_log(const gen & b,const gen & u){ return el_e(b)?SYM(at_ln,u):b==10?SYM(at_log10,u):SYM2(at_logb,u,b); }
static NOINL gen el_mul(const gen & m,const gen & u){ return is_one(m)?u:is_minus_one(m)?SYM(at_neg,u):SYM2(at_prod,m,u); }
static NOINL gen el_sum(const vecteur & v){ return v.empty()?gen(0):v.size()==1?v.front():symbolic(at_plus,gen(v,_SEQ__VECT)); }
// F's kind: 1 a power b^u (b a positive number, not 1), 2 a log of u (base b), 0 neither
static NOINL int el_kind(const gen & F,gen & b,gen & u){
  double d;
  if (is(F,at_exp)){ b=SYM(at_exp,1); u=arg(F); return 1; }
  if (is(F,at_pow) && !dep(arg(F,0)) && dep(arg(F,1)) && el_num(arg(F,0),d) && d>0 && d!=1){ b=arg(F,0); u=arg(F,1); return 1; }
  if (is(F,at_ln)){ b=SYM(at_exp,1); u=arg(F); return 2; }
  if (is(F,at_log10)){ b=10; u=arg(F); return 2; }
  if (is(F,at_logb) && arg(F).type==_VECT && arg(F)._VECTptr->size()==2 && !dep(arg(F,1)) && el_num(arg(F,1),d) && d>0 && d!=1){ b=arg(F,1); u=arg(F,0); return 2; }
  return 0;
}
static NOINL bool el_has(const gen & g){ // a power or a log with x inside
  if (g.type!=_SYMB) return false;
  gen b,u;
  if (el_kind(g,b,u) && dep(u)) return true;
  const gen & a=arg(g);
  if (a.type!=_VECT) return el_has(a);
  EACH(it,*a._VECTptr) if (el_has(*it)) return true;
  return false;
}
// a side's terms: k*F (F a power or log with x, or a log of a number), c the numbers; false: other
// x terms (x+e^x)
static NOINL bool el_terms(const gen & s,vecteur & K,vecteur & Fs,gen & c){
  vecteur t;
  flat(at_plus,s,t);
  c=0;
  EACH(it,t){
    gen g=*it,k=1,F=0,b,u;
    const bool x=dep(g);
    if (is(g,at_neg)){ k=-1; g=arg(g); }
    vecteur f;
    if (is(g,at_prod)) flat(at_prod,g,f); else f.push_back(g);
    bool two=false;
    EACH(j,f){
      if (dep(*j) || (!x && el_kind(*j,b,u)==2)){ two=two || !is_zero(F); F=*j; }
      else k=k*(*j);
    }
    double d;
    if (two || is_zero(F) || !el_num(k,d)){ if (x) return false; c=c+*it; continue; }
    if (!el_kind(F,b,u)) return false;
    K.push_back(ev(k)); Fs.push_back(F);
  }
  c=ev(c);
  return true;
}
static NOINL gen el_term(const gen & k,const gen & F){ // k*F as written: -ln(x), 2*ln(x)
  if (is_one(k)) return F;
  if (is_minus_one(k)) return SYM(at_neg,F);
  double d;
  if (el_num(k,d) && d<0) return SYM(at_neg,SYM2(at_prod,ev(-k),F));
  return SYM2(at_prod,k,F);
}
static NOINL gen el_side(const vecteur & K,const vecteur & Fs){
  vecteur t;
  for (unsigned i=0;i<K.size();++i) t.push_back(el_term(K[i],Fs[i]));
  return el_sum(t);
}
// k*F + c = R: F alone (c taken away, both sides divided by k); the right side's value
static NOINL gen el_iso(const gen & L,const gen & k,const gen & F,const gen & c,const gen & R){
  gen l=L,r=ev(R);
  double d;
  if (!is_zero(c)){
    if (el_num(c,d) && d>0) both(R_ASUB,L,R,c,-1); else both(R_AADD,L,R,ev(-c),1);
    l=el_term(k,F); r=ev(r-c);
    S(R_ACONT,l,r);
  }
  if (!is_one(k)){
    if (!is_minus_one(k)) S(R_ADIV,l,r,k); // (-2^x = -8: 2^x = 8 at once)
    r=ev(r/k);
    S(R_ACONT,F,r);
  }
  return r;
}
// g = c^n, c the smallest whole number (g whole or 1/whole)
static NOINL bool el_root(const gen & g,gen & c,gen & n){
  gen a=g;
  int s=1;
  if (a.type==_FRAC && is_one(a._FRACptr->num)){ a=a._FRACptr->den; s=-1; }
  if (a.type!=_INT_ || a.val<2) return false;
  const long long v=a.val;
  for (int e=24;e>=1;--e){
    const long long r=(long long)(pow((double)v,1.0/e)+0.5);
    if (r<2) continue;
    long long p=1;
    int i=0;
    for (;i<e && p<=v;++i) p*=r;
    if (i==e && p==v){ c=int(r); n=s*e; return true; }
  }
  return false;
}
static NOINL bool el_lin(const gen & l,const gen & r);
static NOINL bool el_eq(const gen & l,const gen & r){ // (numbers with logs in it: 2x-1 = ln(5), x by hand)
  if (l==X && !dep(r)) return true;
  const size_t m=mark();
  if (eq_poly(l,r)) return true;
  cut(m);
  return el_lin(l,r);
}
// a side as typed with a-b as a+(-b) and a/b as a*b^-1, nothing evaluated (ln(9) stays)
static NOINL gen el_norm(const gen & g){
  if (g.type!=_SYMB) return g;
  const unary_function_ptr & u=g._SYMBptr->sommet;
  const gen a=arg(map_of(g,el_norm));
  vecteur v;
  if (u==at_binary_minus && a.type==_VECT && a._VECTptr->size()==2){
    flat(at_plus,a[0],v); flat(at_plus,a[1].type<_IDNT?-a[1]:symb_neg(a[1]),v); return symb_plus(v);
  }
  if (u==at_division && a.type==_VECT && a._VECTptr->size()==2) return is_one(a[0])?SYM(at_inv,a[1]):SYM2(at_prod,a[0],SYM(at_inv,a[1]));
  return SYM(&u,a);
}
// x terms on one side: a x + b = c x + d, the coefficients numbers with logs (x ln 3 = (x+1) ln 2)
static NOINL bool el_lin(const gen & l,const gen & r){
  if (dep(D(D(ev(l-r),X),X))) return false;
  gen Lx=ev(expand(l,CTX)),Rx=ev(expand(r,CTX));
  gen aL=D(Lx,X),aR=D(Rx,X),bL=ev(sub(Lx,X,0)),bR=ev(sub(Rx,X,0));
  const gen k=lcd(aL,bL,aR,bR);
  if (!is_one(k)){ // fractions first (x/2 = e: x = 2e)
    S(R_ALCD,l,r,k);
    Lx=ev(expand(k*Lx,CTX)); Rx=ev(expand(k*Rx,CTX));
    S(R_ACONT,Lx,Rx);
    if (Lx==X && !dep(Rx)) return true;
    aL=D(Lx,X); aR=D(Rx,X); bL=ev(sub(Lx,X,0)); bR=ev(sub(Rx,X,0));
  }
  else if (!written(Lx,l) || !written(Rx,r)) S(R_ADIST,Lx,Rx);
  const gen a=ev(aL-aR);
  if (is_zero(a)){ S(is_zero(ev(bR-bL))?R_ATRUE:R_AFALSE,bL,bR); return true; }
  gen lx=Lx,rx=Rx;
  if (!is_zero(aR) || !is_zero(bL)){
    vecteur lt,rt;
    lt.push_back(ev(aL*X));
    if (!is_zero(aR)) lt.push_back(ev(-aR*X));
    if (!is_zero(bR)) rt.push_back(bR);
    if (!is_zero(bL)) rt.push_back(ev(-bL));
    lx=el_sum(lt); rx=el_sum(rt);
    S(R_GXSIDE,lx,rx);
  }
  if (!is_zero(aR)){ lx=SYM2(at_prod,X,a); S(R_GFACX,lx,rx); }
  if (!is_one(a)) S(R_ADIV,lx,rx,a);
  S(R_ACONT,X,ev(rx/a));
  return true;
}
// b^u = r, r a number
static NOINL bool el_one(const gen & F,const gen & r){
  gen b,u,c,m,n,c2;
  el_kind(F,b,u);
  double d;
  if (!el_num(r,d)) return false;
  if (d<=0){ S(R_GNONE,SYM2(at_equal,F,r),0); return true; }
  if (!el_e(b) && el_root(b,c,m) && (is_one(r)?(n=0,true):el_root(r,c2,n) && c2==c)){ // 4^x = 8: 2^(2x) = 2^3
    const gen mu=el_mul(m,u);
    S(R_GSAME,SYM2(at_equal,el_pow(c,mu),el_pow(c,n)),0);
    S(R_GEXPEQ,SYM2(at_equal,mu,n),0);
    return el_eq(mu,n);
  }
  const gen lr=SYM(at_ln,r);
  S(R_GLN,SYM2(at_equal,SYM(at_ln,F),lr),0);
  if (el_e(b)){ // (ln(e^u) is u)
    const gen v=ev(lr);
    S(R_ACONT,u,lr);
    if (u==X && pr(v)!=pr(lr)) S(R_ACONT,X,v); // (ln(1) = 0)
    return el_eq(u,v);
  }
  const gen lb=SYM(at_ln,b),ul=SYM2(at_prod,u,lb);
  S(R_GPOW,SYM2(at_equal,ul,lr),0);
  S(R_ADIV,ul,lr,lb);
  const gen v=ev(SYM2(at_prod,lr,SYM(at_inv,lb)));
  S(R_ACONT,u,v);
  return el_eq(u,v);
}
// b1^u1 = b2^u2
static NOINL bool el_two(const gen & F1,const gen & F2){
  gen b1,u1,b2,u2,c1,m1,c2,m2;
  el_kind(F1,b1,u1); el_kind(F2,b2,u2);
  if (!el_e(b1) && !el_e(b2) && el_root(b1,c1,m1) && el_root(b2,c2,m2) && c1==c2){ // 4^x = 8^(x-1)
    const gen l=el_mul(m1,u1),r=el_mul(m2,u2);
    if (!is_one(m1) || !is_one(m2)) S(R_GSAME,SYM2(at_equal,el_pow(c1,l),el_pow(c1,r)),0);
    S(R_GEXPEQ,SYM2(at_equal,l,r),0);
    return el_eq(l,r);
  }
  S(R_GLN,SYM2(at_equal,SYM(at_ln,F1),SYM(at_ln,F2)),0);
  const gen l=el_e(b1)?u1:SYM2(at_prod,u1,SYM(at_ln,b1)),r=el_e(b2)?u2:SYM2(at_prod,u2,SYM(at_ln,b2));
  S(R_GPOW,SYM2(at_equal,l,r),0);
  return el_lin(l,r);
}
// every term on the left, k*b^(m x) with m 1 or 2, one base: u = b^x, a quadratic in u
static NOINL bool el_quad(const vecteur & K,const vecteur & Fs,const gen & c){
  const gen t(identificateur(X==gen(identificateur("u"))?"t":"u"));
  gen b0,Q=c;
  bool one=false,two=false;
  for (unsigned i=0;i<K.size();++i){
    gen b,u;
    el_kind(Fs[i],b,u);
    if (i && !(b==b0)) return false;
    b0=b;
    const gen m=D(u,X);
    if (!is_zero(ev(u-m*X))) return false;
    if (m==1){ one=true; Q=Q+K[i]*t; }
    else if (m==2){ two=true; Q=Q+K[i]*symb_pow(t,2); }
    else return false;
  }
  if (!one || !two) return false;
  Q=ev(expand(Q,CTX));
  const gen F=el_pow(b0,X);
  S(R_GSUBU,SYM2(at_equal,t,F),0);
  S(R_ACONT,Q,0);
  const gen x0=X;
  X=t;
  ++depth_;
  const bool ok=eq_poly(Q,0);
  const vecteur r=ok?roots_of(Q,0):vecteur();
  --depth_;
  X=x0;
  if (!ok) return false;
  EACH(it,r){
    S(R_GBACK,SYM2(at_equal,F,*it),0);
    ++depth_; const bool o=el_one(F,*it); --depth_;
    if (!o) return false;
  }
  return true;
}
// the logs k_i log(u_i) of a side: with the power rule (s1), as one log (s2) of a; q: a quotient
static NOINL bool el_comb(const vecteur & K,const vecteur & Fs,const gen & B,gen & s1,gen & s2,gen & a,bool & q){
  vecteur t,num,den;
  for (unsigned i=0;i<K.size();++i){
    gen b,u;
    el_kind(Fs[i],b,u);
    const gen k=K[i];
    if (k.type!=_INT_ && k.type!=_FRAC) return false;
    const bool neg=!is_positive(k,CTX);
    const gen ak=neg?ev(-k):k,w=is_one(ak)?u:SYM2(at_pow,u,ak),l=el_log(B,w);
    t.push_back(neg?SYM(at_neg,l):l);
    (neg?den:num).push_back(w);
  }
  q=!den.empty();
  const gen N=num.empty()?gen(1):num.size()==1?num[0]:symbolic(at_prod,gen(num,_SEQ__VECT));
  const gen M=den.size()==1?den[0]:symbolic(at_prod,gen(den,_SEQ__VECT));
  s1=el_sum(t);
  a=q?SYM2(at_prod,N,SYM(at_inv,M)):N;
  s2=el_log(B,a);
  return true;
}
// a = v (a the logs' argument): solved, each solution checked in the logs (us: their arguments)
static NOINL bool el_check(const gen & a,const gen & v,const vecteur & us){
  if (dep(_denom(ratnormal(a,CTX),CTX))?!eq_rational(a,v):!el_eq(a,v)) return false;
  const vecteur cand=roots_of(a,v);
  int good=0;
  EACH(it,cand){
    bool ok=true;
    double d;
    EACH(u,us) if (!el_num(sub(*u,X,*it),d) || d<=0) ok=false;
    good+=ok;
    S(ok?R_GOK:R_GBAD,SYM2(at_equal,X,*it),0);
  }
  if (!good && !cand.empty()) S(R_RNONE,eqseq(cand,at_different),0);
  return true;
}
// log_B(a) = c: a = B^c
static NOINL bool el_form(const gen & B,const gen & a,const gen & c,const vecteur & us){
  const gen p=el_pow(B,c),v=ev(p);
  S(R_GEXPF,SYM2(at_equal,a,p),0);
  if (pr(v)!=pr(p)) S(R_ACONT,a,v);
  return el_check(a,v,us);
}
static NOINL bool eq_el(gen L,gen R){
  vecteur KL,FL,KR,FR;
  gen cL,cR;
  if (!el_terms(L,KL,FL,cL) || !el_terms(R,KR,FR,cR)) return false;
  bool xl=false,xr=false;
  EACH(it,FL) xl=xl || dep(*it);
  EACH(it,FR) xr=xr || dep(*it);
  if (!xl && !xr) return false;
  if (!xl){ S(R_ASWAP,R,L); std::swap(L,R); std::swap(KL,KR); std::swap(FL,FR); std::swap(cL,cR); }
  int kind=0;
  gen B;
  vecteur us; // the logs' arguments with x
  for (int s=0;s<2;++s) EACH(it,s?FR:FL){
    gen b,u;
    const int k=el_kind(*it,b,u);
    if (kind && (k!=kind || (k==2 && !(b==B)))) return false; // (logs: one base)
    kind=k; B=b;
    if (k==2 && dep(u)) us.push_back(u);
  }
  if (kind==1){
    if (FL.size()==1 && FR.empty()) return el_one(FL[0],el_iso(L,KL[0],FL[0],cL,R));
    if (FL.size()==1 && FR.size()==1 && is_zero(cL) && is_zero(cR) && is_one(KL[0]) && is_one(KR[0])) return el_two(FL[0],FR[0]);
    vecteur t;
    flat(at_plus,L,t);
    if (!is_zero(R)){ // every term to the left
      vecteur r;
      flat(at_plus,R,r);
      EACH(it,r) t.push_back(SYM(at_neg,*it));
      S(R_QSTD,el_sum(t),0);
    }
    for (unsigned i=0;i<FR.size();++i){ KL.push_back(ev(-KR[i])); FL.push_back(FR[i]); }
    return el_quad(KL,FL,ev(cL-cR));
  }
  gen s1,s2,a,r1,r2,ar;
  bool q,qr;
  if (FR.empty() || !is_zero(cL) || !is_zero(cR)){ // log(x)+log(x-3) = 1: the logs one, then a power
    if (FR.empty() && FL.size()==1){ gen b,u; el_kind(FL[0],b,u); return el_form(B,u,el_iso(L,KL[0],FL[0],cL,R),us); }
    gen v=ev(cR-cL);
    if (!FR.empty()){ // the logs to the left, the numbers to the right
      for (unsigned i=0;i<FR.size();++i){ KL.push_back(ev(-KR[i])); FL.push_back(FR[i]); }
      S(R_GMOVE,el_side(KL,FL),v);
    }
    else if (!is_zero(cL)){
      double d;
      if (el_num(cL,d) && d>0) both(R_ASUB,L,R,cL,-1); else both(R_AADD,L,R,ev(-cL),1);
      S(R_ACONT,el_side(KL,FL),v);
    }
    if (!el_comb(KL,FL,B,s1,s2,a,q)) return false;
    if (pr(s1)!=pr(el_side(KL,FL))) S(R_GPOWR,s1,v);
    S(q?R_GQUOTR:R_GPRODR,s2,v);
    return el_form(B,a,v,us);
  }
  // logs on both sides: log(a) = log(b), a = b
  if (!el_comb(KL,FL,B,s1,s2,a,q) || !el_comb(KR,FR,B,r1,r2,ar,qr)) return false;
  if (pr(s1)!=pr(el_side(KL,FL)) || pr(r1)!=pr(el_side(KR,FR))) S(R_GPOWR,s1,r1);
  if (pr(s2)!=pr(s1) || pr(r2)!=pr(r1)) S(q || qr?R_GQUOTR:R_GPRODR,s2,r2);
  S(R_GARG,SYM2(at_equal,a,ar),0);
  return el_check(a,ar,us);
}

// ---------------------------------------------------------------- trig equations (DeltaCE)
// One trig function of one angle u = m x + c: the equation solved for it as for a variable (2 sin(x)
// = 1, 2 sin(x)^2 - sin(x) - 1 = 0: an identifier named sin(x)), then each value's angles: the
// reference angle in the quadrants of its sign (or an axis), every solution (a multiple of the
// period added, n an integer), solved for x, the solutions in [0, 2pi) (the console's answer too,
// answer.cc). sin(u) = cos(u): tan(u) = 1; sin(x)^2 with cos(x): the Pythagorean identity. Radians.
static NOINL bool tr_fn(const gen & g){ return is(g,at_sin) || is(g,at_cos) || is(g,at_tan); }
static NOINL void tr_find(const gen & g,vecteur & v){ // the trig functions of x in g
  if (g.type!=_SYMB) return;
  if (tr_fn(g) && dep(arg(g))){ if (!equalposcomp(v,g)) v.push_back(g); return; }
  const gen & a=arg(g);
  if (a.type!=_VECT){ tr_find(a,v); return; }
  EACH(it,*a._VECTptr) tr_find(*it,v);
}
static NOINL gen tr_eqs(const gen & v,const vecteur & w){ // v=w0, v=w1, ...
  vecteur e;
  EACH(it,w) e.push_back(SYM2(at_equal,v,*it));
  return e.size()==1?e.front():gen(e,_SEQ__VECT);
}
static NOINL gen tr_n(){ return gen(identificateur(X==gen(identificateur("n"))?"k":"n")); }
static NOINL gen tr_plus(const gen & b,const gen & s){ const gen t=ev(s*tr_n()); return is_zero(b)?t:SYM2(at_plus,b,t); } // b + s n
// b + s n in [0, 2pi), added to out in order (no copies)
static NOINL void tr_in(const gen & b,const gen & s,vecteur & out){
  double bd,sd;
  if (!el_num(b,bd) || !el_num(s,sd) || sd<=0) return;
  const double tp=6.283185307179586,e=1e-4; // (the calculator's double is a float)
  int k=(int)ceil(-bd/sd-e);
  for (int i=0;i<64;++i,++k){
    const double v=bd+k*sd;
    if (v>=tp-e) break;
    unsigned j=0;
    double w=0;
    for (;j<out.size();++j){ el_num(out[j],w); if (fabs(w-v)<e || w>v) break; }
    if (j<out.size() && fabs(w-v)<e) continue;
    out.insert(out.begin()+j,ratnormal(b+k*s,CTX));
  }
}
// T(u) = k: the angles, every solution, the ones in [0, 2pi) (added to sols)
static NOINL bool tr_basic(const gen & T,const gen & k,vecteur & sols){
  const bool sn=is(T,at_sin),cs=is(T,at_cos);
  const gen u=arg(T),m=D(u,X),c=ev(sub(u,X,0)),pi=cst_pi;
  double d;
  if (!el_num(k,d) || dep(m) || is_zero(m) || !is_zero(ratnormal(u-m*X-c,CTX))) return false; // (u = m x + c)
  if ((sn || cs) && (d>1+1e-12 || d<-1-1e-12)){ S(R_TRANGE,T,k); return true; }
  gen P=sn || cs?ev(2*pi):pi; // the period
  vecteur th,shown; // the angles of one period from 0 (tan: from 0 to pi)
  const bool zero=fabs(d)<1e-12,one=fabs(fabs(d)-1)<1e-12;
  if (zero || ((sn || cs) && one)){ // on an axis
    if (sn){ if (zero){ th.push_back(0); th.push_back(pi); } else th.push_back(ev(d>0?pi/2:3*pi/2)); }
    else if (cs){ if (zero){ th.push_back(ev(pi/2)); th.push_back(ev(3*pi/2)); } else th.push_back(d>0?gen(0):pi); }
    else th.push_back(0);
    S(R_TAXIS,tr_eqs(u,th),0);
    shown=th;
  }
  else {
    const unary_function_ptr * f=sn?at_asin:cs?at_acos:at_atan;
    const gen ak=d<0?ev(-k):k,a=ratnormal(ev(SYM(f,ak)),CTX); // (pi/3, not 1/3*pi)
    S(R_TREF,SYM(f,ak),a);
    gen t1,t2;
    int r;
    if (sn){ r=d>0?R_TQS1:R_TQS2; t1=d>0?a:SYM2(at_plus,pi,a); t2=SYM2(at_plus,d>0?pi:ev(2*pi),SYM(at_neg,a)); }
    else if (cs){ r=d>0?R_TQC1:R_TQC2; t1=d>0?a:SYM2(at_plus,pi,SYM(at_neg,a)); t2=d>0?SYM2(at_plus,ev(2*pi),SYM(at_neg,a)):SYM2(at_plus,pi,a); }
    else { r=d>0?R_TQT1:R_TQT2; t1=d>0?a:SYM2(at_plus,pi,SYM(at_neg,a)); t2=d>0?SYM2(at_plus,pi,a):SYM2(at_plus,ev(2*pi),SYM(at_neg,a)); }
    vecteur raw; raw.push_back(t1); raw.push_back(t2);
    shown.push_back(ratnormal(t1,CTX)); shown.push_back(ratnormal(t2,CTX));
    S(r,tr_eqs(u,raw),0);
    if (pr(tr_eqs(u,raw))!=pr(tr_eqs(u,shown))) S(R_ALINE,tr_eqs(u,shown),0);
    th.push_back(shown[0]);
    if (sn || cs) th.push_back(shown[1]);
  }
  if (th.size()==2 && is_zero(ratnormal(th[1]-th[0]-pi,CTX))){ th.pop_back(); P=pi; } // (0 and pi: pi n)
  vecteur g,in;
  EACH(it,th) g.push_back(tr_plus(*it,P));
  S(R_TGEN,tr_eqs(u,g),0);
  gen s=ratnormal(P/m,CTX);
  if (!el_num(s,d)) return false;
  if (d<0) s=ev(-s);
  vecteur xs;
  EACH(it,th){
    const gen b=ratnormal((*it-c)/m,CTX);
    xs.push_back(tr_plus(b,s));
    tr_in(b,s,in);
  }
  if (!(u==X)) S(R_TSOLVX,tr_eqs(X,xs),0);
  if (in.empty()) S(R_TINT,txt("none"),0);
  else if (!(u==X) || pr(tr_eqs(X,in))!=pr(tr_eqs(X,shown))) S(R_TINT,tr_eqs(X,in),0);
  EACH(it,in) tr_in(*it,ev(4*pi),sols); // (one each, in order: a step over 2pi)
  return true;
}
// P = 0 in one trig function T: solved for T (an identifier named T), each value's angles
static NOINL bool tr_one(const gen & T,const gen & L,const gen & R){
  const gen Y(identificateur(pr(T)));
  const gen Ls=ev(subst(L,T,Y,false,CTX)),Rs=ev(subst(R,T,Y,false,CTX));
  if (dep(Ls) || dep(Rs)) return false; // (x outside it: x+sin(x))
  const gen x0=X;
  vecteur ks;
  X=Y;
  bool ok=true;
  if (Ls==Y && !dep(Rs)) ks.push_back(Rs);
  else {
    const size_t m=mark();
    ok=eq_poly(Ls,Rs);
    if (!ok){ cut(m); ok=el_lin(Ls,Rs); } // (sqrt(3) tan(x) - 1 = 0)
    if (ok) ks=roots_of(Ls,Rs);
  }
  X=x0;
  if (!ok) return false;
  vecteur sols;
  EACH(it,ks){
    if (ks.size()>1){ S(R_TEACH,T,*it); ++depth_; }
    const bool o=tr_basic(T,*it,sols);
    if (ks.size()>1) --depth_;
    if (!o) return false;
  }
  if (ks.size()>1 && !sols.empty()) S(R_TINT,tr_eqs(X,sols),0);
  return true;
}
static NOINL bool eq_trig(const gen & L0,const gen & R0){
  if (!angle_radian(CTX)) return false;
  const gen L=ev(L0),R=ev(R0);
  vecteur fs;
  tr_find(L,fs); tr_find(R,fs);
  if (fs.size()==1) return tr_one(fs[0],L,R);
  if (fs.size()!=2 || !(arg(fs[0])==arg(fs[1]))) return false;
  const gen u=arg(fs[0]),s=SYM(at_sin,u),c=SYM(at_cos,u);
  if (!equalposcomp(fs,s) || !equalposcomp(fs,c)) return false;
  const gen Ys(identificateur("s_")),Yc(identificateur("c_"));
  const gen Q=ev(expand(subst(subst(ev(L-R),s,Ys,false,CTX),c,Yc,false,CTX),CTX));
  if (dep(Q)) return false;
  const gen a=D(Q,Ys),b=D(Q,Yc);
  if (!dep(a,Ys) && !dep(a,Yc) && !dep(b,Ys) && !dep(b,Yc) && is_zero(ratnormal(Q-a*Ys-b*Yc,CTX))){ // a sin(u) + b cos(u) = 0
    const gen t=SYM(at_tan,u),k=ratnormal(-b/a,CTX);
    S(R_TDIVC,t,k);
    vecteur sols;
    return tr_basic(t,k,sols);
  }
  for (int i=0;i<2;++i){ // sin(u)^2 with cos(u): 1 - cos(u)^2 (or the other way)
    const gen Ya=i?Yc:Ys,Yb=i?Ys:Yc,A=i?c:s,B=i?s:c;
    const gen q2=ev(D(D(Q,Ya),Ya)/2);
    if (dep(q2,Ya) || dep(q2,Yb) || is_zero(q2) || dep(ratnormal(Q-q2*Ya*Ya,CTX),Ya)) continue;
    const gen one_b=SYM2(at_plus,1,SYM(at_neg,SYM2(at_pow,B,2)));
    S(R_TPYTH,SYM2(at_pow,A,2),one_b);
    const gen Lb=subst(L,SYM2(at_pow,A,2),one_b,false,CTX),Rb=subst(R,SYM2(at_pow,A,2),one_b,false,CTX);
    vecteur f2;
    tr_find(Lb,f2); tr_find(Rb,f2);
    if (equalposcomp(f2,A)) return false; // (sin(x) left: sin(x)^3)
    S(R_ACONT,Lb,Rb);
    return tr_one(B,ev(expand(Lb,CTX)),ev(expand(Rb,CTX)));
  }
  return false;
}

// ---------------------------------------------------------------- inequalities (DeltaCE)
static NOINL gen fac_part(const gen & p); // (below: p factored, its factoring a part)
static NOINL std::string frac_txt(const gen & a,const gen & b);
static NOINL double num(const gen & g){ const gen e=evalf_double(g,1,CTX); return e.type==_DOUBLE_?e._DOUBLE_val:0; }
// one variable, numbers otherwise (pi is one)
static NOINL bool only_x(const gen & g){
  const vecteur ids=lidnt(g);
  EACH(it,ids) if (*it!=X && evalf(*it,1,CTX).type!=_DOUBLE_) return false;
  return true;
}
// a number in the interval (a, b) (lo, hi: a, b are there): 0 when it is in, else the integer
// nearest an end, else the middle
static NOINL gen test_pt(const gen & a,const gen & b,bool lo,bool hi){
  const double da=lo?num(a):0,db=hi?num(b):0;
  if ((!lo || da<0) && (!hi || db>0)) return 0;
  if (!lo) return gen(int(db)-(db<=int(db)?1:0)); // (the largest integer below b)
  const int k=int(da)+(da<0 && da!=int(da)?0:1); // (the smallest integer above a)
  if (!hi || k<db) return k;
  return ratnormal((a+b)/2,CTX);
}
// f rel 0 for f a polynomial or a quotient of polynomials: one side 0, factored, its critical
// numbers (zeros, and where the bottom is 0: never kept), a number tested in each interval, the
// intervals with the right sign kept (their zeros too with <= and >=)
static NOINL bool ineq_sign(const gen & L,const gen & R,int rl){
  const gen f=ratnormal(L-R,CTX),n=_numer(f,CTX),d=_denom(f,CTX);
  rel_=rl;
  if (!is_zero(R)) S(R_QSTD,f,0);
  rel_=0;
  const size_t m=mark();
  S(R_NFAC,0,0);
  const gen fn=fac_part(n),fd=dep(d)?fac_part(d):d;
  if (pr(fn)==pr(n) && pr(fd)==pr(d)) cut(m); else { std::string t=frac_txt(n,d); t+="="; t+=frac_txt(fn,fd); (*out_)[m].a=txt(t); }
  const vecteur z=roots_of(n,0),pz=dep(d)?roots_of(d,0):vecteur();
  vecteur c; // the critical numbers in order; pole[i]: the bottom's
  std::vector<char> pole;
  for (int j=0;j<2;++j) EACH(it,j?pz:z){
    if (it->type==_DOUBLE_ || it->type==_REAL) return false; // (approximate: no exact steps)
    unsigned i=0;
    while (i<c.size() && num(c[i])<num(*it)) ++i;
    if (i<c.size() && is_zero(ratnormal(c[i]-*it,CTX))){ if (j) pole[i]=1; continue; }
    c.insert(c.begin()+i,*it); pole.insert(pole.begin()+i,j);
  }
  const unsigned k=c.size();
  std::string t;
  for (unsigned i=0;i<k;++i){ if (i) t+=","; t+=pr(line(pole[i]?"%0!=%1":"%0=%1",X,c[i],0,0,0,0)); }
  if (k) S(R_NCRIT,txt(t),0);
  std::vector<char> in(2*k+1); // intervals at even places, the critical numbers at odd ones
  for (unsigned i=0;i<=k;++i){
    const gen tp=test_pt(i?c[i-1]:gen(0),i<k?c[i]:gen(0),i>0,i<k),v=ratnormal(subst(f,X,tp,false,CTX),CTX);
    const bool pos=is_strictly_positive(v,CTX);
    in[2*i]=(rl>=3)==pos;
    PL(i?R_NLINE:R_NTEST,!k?"%0=%1,%2@30":!i?"%0<%4,%0=%1,%2@30":i<k?"%5<%0<%4,%0=%1,%2@30":"%0>%5,%0=%1,%2@30",
       X,tp,v,gen(pos?3:1),i<k?c[i]:gen(0),i?c[i-1]:gen(0));
  }
  for (unsigned i=0;i<k;++i) in[2*i+1]=(rl==2 || rl==4) && !pole[i];
  t.clear(); // the runs kept, as giac writes them: x<-2, x>3; -2<=x<=2; x=1
  bool all=false;
  for (unsigned a=0;a<2*k+1;){
    if (!in[a]){ ++a; continue; }
    unsigned b=a;
    while (b+1<2*k+1 && in[b+1]) ++b;
    all=!a && b==2*k;
    if (!t.empty()) t+=",";
    const gen lo=a?c[(a-1)/2]:gen(0),hi=b<2*k?c[b/2]:gen(0);
    t+=pr(line(a==b && (a&1)?"%0=%2":!a?"%0@4%2":b==2*k?"%0@3%1":"%1@3%0@4%2",X,lo,hi,gen(!a || b==2*k?((a&1)?4:3):((a&1)?2:1)),gen((b&1)?2:1),0));
    a=b+1;
  }
  if (all) PL(R_NALL,"-oo<%0<oo",X);
  else if (t.empty()) S(R_NNONE,symbolic(*relop(rl),makesequence(f,0)),0);
  else S(R_NKEEP1+rl-1,txt(t),0);
  return !ctrl_c && !interrupted;
}
// an inequality: linear as an equation (the sign flipping by a negative), else ineq_sign
static NOINL bool ineq_steps(const gen & L,const gen & R,int rl){
  const gen f=ratnormal(L-R,CTX);
  if (!dep(f) || !only_x(f) || !polyf(_numer(f,CTX)) || !polyf(_denom(f,CTX))) return false;
  const gen dg=_degree(makesequence(_numer(f,CTX),X),CTX);
  if (!dep(_denom(f,CTX)) && dg.type==_INT_ && dg.val<=1 && !dep(_denom(ratnormal(L,CTX),CTX)) && !dep(_denom(ratnormal(R,CTX),CTX))){
    rel_=rl;
    const bool ok=eq_linear(L,R);
    rel_=0;
    return ok;
  }
  return ineq_sign(L,R,rl);
}
// |A| = k, < k, > k, A linear: the absolute value alone first (its moves as for a variable), then
// two cases, or one compound inequality; |A| = B with x in B too: two cases, each checked
static NOINL gen lin_root(const gen & e){ // e linear in x: where it is 0 (undef: nowhere or everywhere)
  const gen p=ev(expand(e,CTX)),c1=co(p,1);
  return is_zero(c1)?gen(undef):ratnormal(-co(p,0)/c1,CTX);
}
static NOINL void lin_part(int rule,const gen & a,const gen & b,int rl){ // a rl b solved, a part below its title
  rel_=rl; S(rule,a,b); ++depth_; eq_linear(a,b); --depth_; rel_=0;
}
static NOINL bool abs_steps(const gen & eq,int rl){
  if (!rl && !eq.is_symb_of_sommet(at_equal)) return false;
  const gen L=ev(arg(eq,0)),R=ev(arg(eq,1));
  const vecteur ab=lop(makevecteur(L,R),at_abs);
  if (ab.size()!=1) return false;
  const gen B=ab[0],A=arg(B),u(identificateur("u__")),Lu=sub(L,B,u),Ru=sub(R,B,u),fu=ev(expand(Lu-Ru,CTX));
  if (!polyf(A) || !only_x(A) || !is_one(_degree(makesequence(A,X),CTX))) return false;
  const bool xout=dep(fu);
  gen k;
  int r=rl;
  if (xout){ // |A| = B, B linear
    if (rl || !(Lu==u || Ru==u)) return false;
    k=Lu==u?R:L;
    if (!polyf(k) || !only_x(k) || is_strictly_greater(_degree(makesequence(k,X),CTX),1,CTX)) return false;
  }
  else {
    const gen sx=X;
    X=u;
    const gen c1=co(fu,1);
    if (is_zero(c1) || !is_one(_degree(makesequence(fu,u),CTX))){ X=sx; return false; }
    k=ratnormal(-co(fu,0)/c1,CTX);
    if (!(Lu==u && !dep(Ru))){ // the absolute value alone: the moves of a linear equation in it
      const size_t m=mark();
      rel_=rl;
      eq_linear(Lu,Ru);
      r=rel_; rel_=0;
      for (size_t i=m;i<out_->size();++i){ step_t & s=(*out_)[i]; s.a=sub(s.a,u,B); s.b=sub(s.b,u,B); s.c=sub(s.c,u,B); s.v=sx; }
    }
    X=sx;
  }
  const gen mk=ev(-k),G(r);
  const bool lt=r==1 || r==2;
  if (!xout && (is_zero(k) || is_strictly_positive(mk,CTX))){ // k <= 0
    if (is_zero(k) && (r==0 || r==2)){ PL(R_BZERO,"%0=0",A); ++depth_; eq_linear(A,0); --depth_; return true; } // (only 0)
    if (is_zero(k) && r==3){ PL(R_BNOT0,"%0!=%1",X,lin_root(A)); return true; }
    PL(R_BNEG,"%0@1%2",B,G,k); // (never negative: none below, every number above)
    rel_=r; S(!r || lt?R_AFALSE:R_ATRUE,B,k); rel_=0;
    return true;
  }
  if (!r){ // two cases
    PL(R_BTWO,"%0=%1,%0=%2",A,k,mk);
    lin_part(R_BCASE1,A,k,0);
    lin_part(R_BCASE2,A,mk,0);
    vecteur sol;
    for (int j=0;j<2;++j){
      const gen sj=lin_root(A-(j?mk:k));
      if (is_undef(sj) || (!sol.empty() && is_zero(ratnormal(sol[0]-sj,CTX)))) continue;
      if (xout){ // checked: |A| = B there
        const gen lv=ratnormal(subst(B,X,sj,false,CTX),CTX),rv=ratnormal(subst(k,X,sj,false,CTX),CTX);
        const bool ok=is_zero(ratnormal(lv-rv,CTX));
        PL(ok?R_ROK:R_RBAD,ok?"%0=%1,%2=%3":"%0=%1,%2!=%3",X,sj,lv,rv);
        if (!ok) continue;
      }
      sol.push_back(sj);
    }
    if (sol.empty()) PL(R_RNONE,"%0=%1",B,k);
    else PL(R_BSOL,sol.size()>1?"%0=%1,%0=%2":"%0=%1",X,sol[0],sol.size()>1?sol[1]:gen(0));
    return true;
  }
  const gen a1=co(A,1),a0=co(A,0);
  const bool neg=is_strictly_positive(-a1,CTX);
  if (lt){ // -k < A < k: the same to all three parts, then divide
    const char * const f=r==1?"%0<%1<%2":"%0<=%1<=%2";
    gen lo=mk,hi=k;
    PL(R_BBETW,f,lo,A,hi);
    if (!is_zero(a0)){ lo=ev(lo-a0); hi=ev(hi-a0); PL(R_B3,f,lo,ev(a1*X),hi); }
    if (!is_one(a1)){
      lo=ratnormal(lo/a1,CTX); hi=ratnormal(hi/a1,CTX);
      if (neg){ const gen w=lo; lo=hi; hi=w; }
      PL(neg?R_B3FLIP:R_B3DIV,f,lo,X,hi);
    }
    return true;
  }
  // A < -k or A > k: each solved
  const gen F(flip(r));
  PL(R_BOUT,"%0@1%2,%0@3%4",A,F,mk,G,k);
  lin_part(R_BCASE1,A,mk,flip(r));
  lin_part(R_BCASE2,A,k,r);
  gen s1=lin_root(A-mk),s2=lin_root(A-k);
  if (neg){ const gen w=s1; s1=s2; s2=w; } // (ascending: by a negative, x > s1 and x < s2)
  PL(R_BSOL,"%0@1%2,%0@3%4",X,F,s1,G,s2);
  return true;
}
static NOINL bool eq_steps(const gen & input){
  const gen & g=arg(input);
  if (g.type==_VECT && g._VECTptr->size()==2 && g[0].type==_VECT && g[1].type==_VECT) // (DeltaCE) a system
    return sys_steps(*g[0]._VECTptr,*g[1]._VECTptr);
  const gen eq=g.type==_VECT?g[0]:g;
  if (g.type==_VECT && g._VECTptr->size()>1){ if (g[1].type!=_IDNT) return false; X=g[1]; }
  { // (DeltaCE) absolute values, inequalities
    const int rl=rel_of(eq);
    if (!lop(eq,at_abs).empty()) return abs_steps(eq,rl);
    if (rl) return ineq_steps(ev(arg(eq,0)),ev(arg(eq,1)),rl);
  }
  if (el_has(eq)) return eq_el(el_norm(eq.is_symb_of_sommet(at_equal)?arg(eq,0):eq),el_norm(eq.is_symb_of_sommet(at_equal)?arg(eq,1):gen(0))); // (DeltaCE) 2^x = 8, log(x) = 2
  { vecteur fs; tr_find(eq,fs); if (!fs.empty()) return eq_trig(eq.is_symb_of_sommet(at_equal)?arg(eq,0):eq,eq.is_symb_of_sommet(at_equal)?arg(eq,1):gen(0)); } // (DeltaCE) 2 sin(x) = 1
  gen L=ev(eq.is_symb_of_sommet(at_equal)?arg(eq,0):eq),R=ev(eq.is_symb_of_sommet(at_equal)?arg(eq,1):gen(0));
  if (!dep(L) && !dep(R)) return false;
  if (nroots(L)+nroots(R)) return eq_radical(L,R,L,R,0); // (sqrt(x+3) = x-3)
  if (dep(_denom(ratnormal(L,CTX),CTX)) || dep(_denom(ratnormal(R,CTX),CTX))) return eq_rational(L,R); // (x/(x-2) = 3)
  return eq_poly(L,R);
}

// ---------------------------------------------------------------- F4's forms (DeltaCE)
gen positive_first(const gen & g); // answer.cc: a sum's positive term first (7-2x)
// (canonical_form(f): f's square completed; propfrac(f): its long division. The console's F4
// shows these forms; Enter on one shows its steps with these commands, focus.cc)
static NOINL std::string sg(const gen & c){ // c after other terms: +5, -9/4 ("" for 0)
  std::string s;
  if (is_zero(c)) return s;
  const std::string t=pr(c);
  if (t[0]!='-') s+="+";
  s+=t;
  return s;
}
// f's one variable (X), false if several or none
static NOINL bool one_var(const gen & f){
  vecteur ids=lidnt(f);
  gen v=0;
  EACH(it,ids) if (evalf(*it,1,CTX).type!=_DOUBLE_){ if (!is_zero(v) && *it!=v) return false; v=*it; }
  if (is_zero(v)) return false;
  X=v;
  return true;
}
// q a quadratic with an x term (rational coefficients): a, b, c
static NOINL bool quad(const gen & q,gen & a,gen & b,gen & c){
  if (!dep(q) || dep(_denom(ratnormal(q,CTX),CTX)) || _degree(makesequence(_numer(ratnormal(q,CTX),CTX),X),CTX)!=2) return false;
  const gen p=ev(expand(q,CTX));
  a=co(p,2); b=co(p,1); c=co(p,0);
  return !is_zero(b) && !dep(a) && !dep(b) && !dep(c);
}
// a x^2 + b x + c with its square completed as a textbook does it (a factored out of the x terms,
// half the x coefficient squared, added and subtracted, the perfect square, the numbers
// combined), then its vertex; the result written as F4 writes it (answer.cc): (x+3)^2-4, 5-(x-2)^2
static NOINL gen cs_quad(const gen & a,const gen & b,const gen & c,bool top){
  const gen B=ratnormal(b/a,CTX),h=ratnormal(B/2,CTX),q=ratnormal(h*h,CTX),k=ratnormal(c-a*q,CTX);
  const bool one=is_one(a);
  std::string in=pr(X); in+="^2"; in+=sg(ev(B*X)); // x^2+6*x
  std::string A; // a as a factor: "", "-", "2*"
  if (is_minus_one(a)) A="-";
  else if (!one){ A=par(a); A+="*"; }
  const std::string sq=pr(symb_pow(X+h,2)); // (x+3)^2
  std::string t;
  if (!one || !is_zero(c)){ t=A; t+="("; t+=in; t+=")"; t+=sg(c); S(one?R_CSGROUP:R_CSFACT,txt(t),0); }
  PL(R_CSHALF,B.type==_FRAC?"(#0/2)^2=%1":"(%0/2)^2=%1",B,q); // ((3/2)/2)^2, (-4/2)^2
  t=A; t+="("; t+=in; t+=sg(q); if (!one) t+=sg(ev(-q)); t+=")"; if (one) t+=sg(ev(-q)); t+=sg(c);
  S(R_CSADD,txt(t),0);
  t=A; if (!one) t+="("; t+=sq; t+=sg(ev(-q)); if (!one) t+=")"; t+=sg(c);
  S(R_CSSQ,txt(t),0);
  if (!one){ t=A; t+=sq; t+=sg(ratnormal(-a*q,CTX)); t+=sg(c); S(R_CSDIST,txt(t),0); }
  const gen s=symb_pow(X+h,2),u=one?s:is_minus_one(a)?symb_neg(s):symb_prod(a,s);
  const gen r=is_zero(k)?u:is_strictly_positive(-a,CTX) && is_strictly_positive(k,CTX)?SYM2(at_plus,k,u):SYM2(at_plus,u,k);
  if (pr(r)!=pr(txt(t))) S(R_CSCOMB,r,0);
  if (top) PL(R_CSVERT,"(%0,%1)",ratnormal(-h,CTX),k); // (inside sqrt(4x-x^2): no vertex)
  return r;
}
// every quadratic of g with an x term (sums only below the top, as F4: n(n+2) stays): completed,
// its steps first
static NOINL gen cs_in(const gen & g,bool top){
  gen a,b,c;
  if (g.type==_SYMB && (top || g.is_plus()) && quad(g,a,b,c)){
    if (!top) S(R_CSIN,g,0);
    return cs_quad(a,b,c,top);
  }
  if (g.type!=_SYMB) return g;
  if (arg(g).type!=_VECT) return symbolic(g._SYMBptr->sommet,cs_in(arg(g),false));
  vecteur v=*arg(g)._VECTptr;
  EACHW(it,v) *it=cs_in(*it,false);
  return symbolic(g._SYMBptr->sommet,gen(v,arg(g).subtype));
}
static NOINL bool cs_steps(const gen & input){
  const gen e=ev(arg(input).type==_VECT?arg(input,0):arg(input));
  if (!one_var(e)) return false;
  const size_t m=mark();
  const gen r=cs_in(e,true);
  if (mark()==m) return false;
  if (out_->front().rule==R_CSIN) S(R_CSBACK,r,0); // (inside: the whole again)
  return true;
}
// long division of a polynomial by one of lower or equal degree: the leading terms divided, the
// divisor times that subtracted, again until the remainder's degree is lower; then the check
// (top = divisor * quotient + remainder) and quotient plus remainder over the divisor
static NOINL gen lead(const gen & p,int & d){ // p's leading term (its degree d)
  const gen g=_degree(makesequence(p,X),CTX);
  d=g.type==_INT_?g.val:0;
  return ev(_coeff(makesequence(p,X,d),CTX)*symb_pow(X,d));
}
// g written as a quotient a/b (el_norm's a*inv(b)): its top and bottom as typed ((x^3-8)/(x-2)
// evaluated is x^2+2x+4: nothing left to divide by)
static NOINL bool written_quo(const gen & g,gen & n,gen & d){
  if (!g.is_prod() || arg(g).type!=_VECT) return false;
  vecteur up,down;
  EACH(it,*arg(g)._VECTptr){ if (is(*it,at_inv)) down.push_back(arg(*it)); else up.push_back(*it); }
  if (down.empty()) return false;
  n=prodof(up); d=prodof(down);
  return true;
}
// e (not evaluated) a division of polynomials in one variable: its top and bottom as typed, expanded
static NOINL bool poly_quo(const gen & e,gen & n,gen & d){
  gen a,b;
  if (!written_quo(el_norm(e),a,b) || !polyf(a) || !polyf(b) || !one_var(e)) return false; // (diff(x^3,x)/(x-1): the derivative's steps)
  n=ev(expand(ev(a),CTX)); d=ev(expand(ev(b),CTX));
  return dep(d) && polyf(n) && polyf(d);
}
// a fraction as text: (a)/(b), a alone over 1
static NOINL std::string frac_txt(const gen & a,const gen & b){
  std::string t,u=pr(a);
  if (is_one(b)) return u;
  const bool one=u.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_^")==std::string::npos; // (1/(x-2), x^2/(...))
  if (!one){ t="("; t+=u; t+=")"; } else t=u;
  t+="/("; t+=pr(b); t+=")";
  return t;
}
// quotient plus remainder over the divisor, the remainder in lowest terms ((x^2-1)/(x^2+x) is
// 1-1/x), as F4's long division writes it: a division typed's answer (the answer box)
static NOINL void div_answer(const gen & Q,const gen & N,const gen & d){
  vecteur w;
  flat(at_plus,Q,w);
  const gen rf=ratnormal(N/d,CTX),rd=_denom(rf,CTX);
  gen fd=_factor(rd,CTX);
  if (is_undef(fd) || fd.type==_STRNG) fd=rd;
  w.push_back(symb_prod(_numer(rf,CTX),symb_inv(fd)));
  S(R_DANS,positive_first(SYM(at_plus,gen(w,_SEQ__VECT))),0);
}
static NOINL bool long_steps(const gen & n,const gen & d,bool plain);
static NOINL bool synth_steps(const gen & n,const gen & d,bool plain);
static NOINL bool cancel_steps(const gen & n,const gen & d);
// (DeltaCE) a division typed, (x^3-8)/(x-2), or F4's long division (propfrac): its methods, by a
// linear divisor long division, synthetic division, factor and cancel when a factor is common; by a
// longer one factor and cancel first. plain (typed): the answer, quotient plus remainder over the
// divisor, a last step for the answer box
static NOINL bool div_of(const gen & e0,bool plain);
static NOINL bool div_steps(const gen & input){ return div_of(arg(input).type==_VECT?arg(input,0):arg(input),false); }
static NOINL bool div_of(const gen & e0,bool plain){
  gen n,d;
  const bool w=poly_quo(e0,n,d);
  if (!w){ // (not written as one: one fraction, x+1/(x-1))
    if (plain) return false;
    const gen e=ev(e0);
    if (!one_var(e)) return false;
    const gen r=ratnormal(e,CTX);
    n=ev(expand(_numer(r,CTX),CTX)); d=ev(expand(_denom(r,CTX),CTX));
    if (!dep(d) || !polyf(n) || !polyf(d)) return false;
  }
  int dn,dd;
  lead(n,dn); lead(d,dd);
  const bool common=w && dep(_gcd(makesequence(n,d),CTX));
  static const char * const names[]={"long division","synthetic division","factor and cancel"};
  unsigned char m[3];
  int k=0;
  if (dd==1){
    if (dn>=1){ m[k++]=0; m[k++]=1; }
    if (common) m[k++]=2;
  }
  else {
    if (common) m[k++]=2;
    if (dn>=dd) m[k++]=0;
  }
  if (!k) return false;
  steps_methods=k;
  const int mm=m[steps_method%k];
  steps_method_name=names[mm];
  return mm==2?cancel_steps(n,d):mm==1?synth_steps(n,d,plain):long_steps(n,d,plain);
}
static NOINL bool long_steps(const gen & n,const gen & d,bool plain){
  int dn,dd;
  gen ld=lead(d,dd),N=n,Q=0;
  lead(N,dn);
  if (dn<dd) return false;
  for (int guard=0;dn>=dd && !is_zero(N) && guard<12 && !ctrl_c && !interrupted;++guard){
    const gen ln=lead(N,dn);
    if (dn<dd) break;
    const gen t=ratnormal(ln/ld,CTX),N2=ev(expand(N-t*d,CTX));
    PL(R_DLEAD,"(%0)/(%1)=%2",ln,ld,t);
    PL(R_DSUB,"(%0)-#1*(%2)=%3",N,t,d,N2);
    Q=ev(Q+t); N=N2;
    if (is_zero(N)) break;
    lead(N,dn);
  }
  if (!is_zero(N)) S(R_DREM,N,0);
  PL(R_DCHECK,"%0=(%1)*(%2)%3",n,d,Q,txt(sg(N))); // (then the answer: quotient plus remainder over the divisor)
  if (plain && !is_zero(N)) div_answer(Q,N,d);
  return true;
}
// synthetic division by a x + b: c = -b/a where it is 0, the coefficients in a row (0 for a
// missing power), the first brought down, then each column: c times the last number below the
// line, added to the coefficient above. The bottom row is the quotient's coefficients and the
// remainder (by x - c: the quotient divided by a then). Each step's table shows the new numbers.
static NOINL void syn_table(int rule,const vecteur & r0,const vecteur & r1,const vecteur & r2,const vecteur & fresh){
  S(rule,gen(makevecteur(gen(r0,0),gen(r1,0),gen(r2,0)),0),gen(fresh,0));
}
static NOINL bool synth_steps(const gen & n,const gen & d,bool plain){
  const vecteur cd=coeffs(d),cn=coeffs(n);
  const int N=cn.size();
  if (cd.size()!=2 || N<2 || N>12) return false;
  const gen a=cd[0],c=ratnormal(-cd[1]/a,CTX);
  vecteur r0(N+1,undef),r1(N+1,undef),r2(N+1,undef),fresh;
  r0[0]=c;
  for (int i=0;i<N;++i){ r0[i+1]=cn[i]; fresh.push_back(i+1); }
  fresh.push_back(0);
  PL(R_SYSET,"%0=0,%1=%2",d,X,c);
  syn_table(R_SYTAB,r0,r1,r2,fresh);
  r2[1]=cn[0];
  syn_table(R_SYDOWN,r0,r1,r2,makevecteur(64+1));
  gen b=cn[0];
  for (int k=1;k<N && !ctrl_c && !interrupted;++k){
    const gen p=ev(c*b),s=ev(cn[k]+p);
    PL(R_SYMUL,"#0*#1=%2,%3+#2=%4",c,b,p,cn[k],s); // (2*4=8, -8+8=0)
    r1[k+1]=p; r2[k+1]=s;
    syn_table(R_SYTAB,r0,r1,r2,makevecteur(32+k+1,64+k+1));
    b=s;
  }
  gen Q=0;
  for (int i=1;i<N;++i) Q=Q+r2[i]*up(N-1-i);
  Q=ev(Q);
  const gen R=r2[N],dx=is_one(a)?d:ev(X-c); // (by x - c: (2x^3+...)/(x+1/2) = ...)
  vecteur w;
  flat(at_plus,Q,w);
  if (!is_zero(R)) w.push_back(symb_prod(R,symb_inv(dx)));
  std::string t=frac_txt(n,dx);
  t+="="; t+=pr(w.size()==1?w.front():positive_first(SYM(at_plus,gen(w,_SEQ__VECT))));
  S(R_SYREAD,txt(t),0);
  if (!is_one(a)){
    const gen Q2=ev(expand(Q/a,CTX));
    PL(R_SYDIVA,"(%0)/#1=%2",Q,a,Q2);
    Q=Q2;
  }
  PL(R_DCHECK,"%0=(%1)*(%2)%3",n,d,Q,txt(sg(R)));
  if (plain && !is_zero(R)) div_answer(Q,R,d);
  return true;
}
// p factored, its factoring steps a part below (none when it does not factor: x-3)
static NOINL gen fac_part(const gen & p){
  if (nfactors(_factor(p,CTX))<2 && coeffs(p).size()<=2) return p;
  const size_t m=mark();
  ++depth_;
  const bool ok=fac_steps(p);
  --depth_;
  if (ok && mark()>m) return prodq(*F_);
  cut(m);
  return _factor(p,CTX);
}
// factor and cancel: the top and the bottom factored (their steps), the values the bottom excludes,
// the common factors cancelled
static NOINL bool cancel_steps(const gen & n,const gen & d){
  if (!dep(_gcd(makesequence(n,d),CTX))) return false;
  const size_t m=mark();
  S(R_KFAC,0,0); // (its line once the factored forms are known)
  const gen fn=fac_part(n),fd=fac_part(d);
  std::string t=frac_txt(n,d);
  if (pr(fn)==pr(n) && pr(fd)==pr(d)) cut(m); // (neither factors: (x^2+1)/(x^2+1))
  else { t+="="; t+=frac_txt(fn,fd); (*out_)[m].a=txt(t); }
  const vecteur ex=roots_of(d,0);
  if (!ex.empty()) S(R_REXCL,eqseq(ex,at_different),0);
  const gen r=ratnormal(n/d,CTX),rn=_factor(_numer(r,CTX),CTX),rd=_factor(_denom(r,CTX),CTX);
  t=frac_txt(fn,fd); t+="="; t+=frac_txt(rn,rd);
  S(R_KCAN,txt(t),0);
  vecteur lost; // the excluded values the answer no longer shows (x+3 for (x^2-9)/(x-3): x != 3)
  EACH(it,ex) if (!is_zero(ratnormal(subst(rd,X,*it,false,CTX),CTX))) lost.push_back(*it);
  if (!lost.empty()){ t=frac_txt(rn,rd); t+=","; t+=pr(eqseq(lost,at_different)); S(R_DANS,txt(t),0); }
  return !ctrl_c && !interrupted;
}
// Partial fractions: the long division first when the top's degree is not lower, the denominator
// factored, a fraction for each factor (A/(x-1), (B x+C)/(x^2+1), each power of a repeated
// one), both sides times the denominator, the unknowns: a root put in for each factor when they
// are all linear and different, else the coefficients of each power of x matched and solved;
// then the fractions with their values. The unknowns are worked out as pf_0, pf_1... (A, B could
// be the user's variables) and shown as A, B, C
static vecteur * PU_, * PD_; // the unknowns worked out (pf_0, pf_1...: A, B could be the user's variables), shown (A, B)
static NOINL gen sh(const gen & g){ return subst(g,*PU_,*PD_,true,CTX); }
static NOINL gen evx(const gen & g){ return ev(expand(g,CTX)); }
static NOINL int deg(const gen & g){ const gen d=_degree(makesequence(g,X),CTX); return d.type==_INT_?d.val:0; }
static NOINL gen cof(const gen & g,int p){ return _coeff(makesequence(g,X,p),CTX); }
static NOINL gen sumof(const vecteur & v){ return v.empty()?gen(0):v.size()==1?v.front():symbolic(at_plus,gen(v,_SEQ__VECT)); }
static NOINL gen prodof2(const vecteur & v){ return v.size()==1?v.front():symbolic(at_prod,gen(v,_SEQ__VECT)); }
static NOINL gen unknown(){ // a new unknown: pf_k worked out, shown as the next capital
  std::string a("pf_0"),b("A");
  a[3]+=PU_->size(); b[0]+=PD_->size();
  PD_->push_back(gen(identificateur(b))); // (the string one: a char * is kept, not copied)
  PU_->push_back(gen(identificateur(a)));
  return PU_->back();
}
// e (linear in the unknowns) written with them in order: A+B-2*C
static NOINL std::string lin(const gen & e){
  std::string s;
  for (unsigned i=0;i<PU_->size();++i){
    const gen k=ev(derive(e,(*PU_)[i],CTX));
    if (is_zero(k)) continue;
    if (is_one(k)) s+="+";
    else if (is_minus_one(k)) s+="-";
    else { s+=sg(k); s+="*"; }
    s+=pr((*PD_)[i]);
  }
  s+=sg(ev(subst(e,*PU_,vecteur(PU_->size(),0),false,CTX)));
  if (s.empty()) s="0";
  if (s[0]=='+') s.erase(s.begin());
  return s;
}
// the fraction v/d written as an answer is: 1/(2*(x-1)), -x/(2*(x^2+1)) (0: none)
static NOINL gen fr(const gen & v,const gen & d){
  const gen n=normal(v,CTX);
  if (is_zero(n)) return 0;
  const gen q=_denom(n,CTX);
  return symb_prod(_numer(n,CTX),symb_inv(is_one(q)?d:symb_prod(q,d)));
}
// g's denominator as written (giac's denom simplifies first)
static NOINL gen typed_den(const gen & g){
  vecteur f;
  flat(at_prod,g,f);
  gen td=1;
  EACH(it,f){
    if (it->is_symb_of_sommet(at_inv)) td=td*arg(*it);
    else if (it->is_pow() && arg(*it,1).type==_INT_ && arg(*it,1).val<0) td=td*symb_pow(arg(*it,0),-arg(*it,1).val);
  }
  return td;
}
// x = the root of the linear factor b: every term of rhs but the i-th is 0; its unknown's value
static NOINL gen pf_root(const gen & n2,const gen & b,const gen & prod,int i){
  const gen x0=ratnormal(-co(b,0)/co(b,1),CTX),lv=ratnormal(subst(n2,X,x0,false,CTX),CTX);
  const gen c=ratnormal(subst(ratnormal(prod/(*PU_)[i],CTX),X,x0,false,CTX),CTX);
  const gen v=is_strictly_positive(-c,CTX)?ratnormal(-lv/ev(-c),CTX):ratnormal(lv/c,CTX); // (1/2 over -2: -1/4, not 1/-4)
  std::string t=pr(X); t+="="; t+=pr(x0); t+=","; t+=pr(lv); t+="="; // x=1,1=2*A,A=1/2 (no spaces: a name with one is quoted)
  if (is_minus_one(c)) t+="-";
  else if (!is_one(c)){ t+=c.type==_FRAC?par(c):pr(c); t+="*"; }
  t+=pr((*PD_)[i]); t+=","; t+=pr((*PD_)[i]); t+="="; t+=pr(v);
  S(R_PFROOT,txt(t),0);
  return v;
}
// the coefficients of each power of x matched, the system solved: the values (false: none)
static NOINL bool pf_coeffs(const gen & n2,const gen & rhs,vecteur & val){
  const gen ex=evx(rhs);
  std::string eqs,w;
  vecteur sys;
  for (int p=deg(ex);p>=0;--p){
    const gen a=cof(ex,p),b=cof(n2,p);
    if (!eqs.empty()) eqs+=",";
    eqs+=lin(a); eqs+="="; eqs+=pr(b);
    sys.push_back(SYM2(at_equal,a,b));
  }
  S(R_PFCOEF,txt(eqs),0);
  const gen sol=_linsolve(makesequence(gen(sys,_SEQ__VECT),gen(*PU_,_SEQ__VECT)),CTX);
  if (sol.type!=_VECT || sol._VECTptr->size()!=PU_->size()) return false;
  for (unsigned i=0;i<val.size();++i){ val[i]=normal(sol[i],CTX); if (i) w+=","; w+=pr((*PD_)[i]); w+="="; w+=pr(val[i]); }
  S(R_PFSOLVE,txt(w),0);
  return true;
}
// Partial fractions: a common factor cancelled, the long division first when the top's degree is
// not lower, the denominator factored, a fraction for each factor (A/(x-1), (B x+C)/(x^2+1),
// each power of a repeated one), both sides times the denominator, the unknowns (a root put in
// for each factor when they are all linear and different, else the coefficients of each power of
// x matched and solved), the fractions with their values
static NOINL bool pf_steps(const gen & input){
  const gen e=ev(arg(input).type==_VECT?arg(input,0):arg(input));
  if (!one_var(e)) return false;
  const gen r=ratnormal(e,CTX),d=evx(_denom(r,CTX));
  gen n=evx(_numer(r,CTX)),Q=0;
  if (!dep(d) || !polyf(n) || !polyf(d)) return false;
  if (deg(d)<deg(evx(typed_den(e)))) S(R_QSIMP,r,0); // ((x^3+1)/(x^2-1) is (x^2-x+1)/(x-1))
  if (deg(n)>=deg(d)){ // improper: the quotient, then the remainder's fractions
    Q=_quo(makesequence(n,d,X),CTX);
    n=_rem(makesequence(n,d,X),CTX);
    if (is_zero(n)) return false;
    S(R_PFDIV,SYM2(at_plus,Q,symb_prod(n,symb_inv(d))),0);
  }
  const gen fd=_factor(d,CTX);
  vecteur fs,base,fp;
  std::vector<int> pw;
  gen k=1;
  flat(at_prod,fd,fs);
  EACH(it,fs){
    gen b;
    const gen p=pexp(*it,b);
    if (!dep(b)){ k=k*(*it); continue; }
    if (p.type!=_INT_ || p.val<1 || deg(b)>2) return false;
    base.push_back(b); pw.push_back(p.val); fp.push_back(*it);
  }
  if (base.empty()) return false;
  if (base.size()==1 && pw[0]==1) return !is_zero(Q); // (one factor: nothing to split; a division gave it)
  if (!written(fd,d)) PL(R_PFFACT,"%0=%1",d,fd);
  if (!PU_){ PU_=new vecteur; PD_=new vecteur; }
  PU_->clear(); PD_->clear();
  vecteur terms,prods,dens; // each fraction; its unknown times the other factors; its denominator
  bool roots=true; // every factor linear and once
  for (unsigned i=0;i<base.size();++i){
    const bool l1=deg(base[i])==1;
    roots=roots && l1 && pw[i]==1;
    for (int j=1;j<=pw[i];++j){
      if (PU_->size()>8) return false;
      gen t=unknown();
      if (!l1) t=SYM2(at_plus,symb_prod(t,X),unknown()); // (B x+C)
      const gen dj=j==1?base[i]:symb_pow(base[i],j);
      dens.push_back(dj);
      terms.push_back(symb_prod(t,symb_inv(dj)));
      vecteur o(1,t); // the other factors: the denominator over this one's
      for (unsigned m=0;m<base.size();++m){
        const int q=m==i?pw[m]-j:pw[m];
        if (q) o.push_back(q==1?base[m]:symb_pow(base[m],q));
      }
      prods.push_back(prodof2(o));
    }
  }
  const gen n2=ratnormal(n/k,CTX),rhs=sumof(prods); // (2 x^2-2: 1/2 over (x-1)(x+1))
  PL(R_PFSET,"%0=%1",symb_prod(n2,symb_inv(prodof2(fp))),sh(sumof(terms)));
  PL(R_PFMULT,"%0=%1",n2,sh(rhs));
  vecteur val(PU_->size());
  if (roots) for (unsigned i=0;i<base.size();++i) val[i]=pf_root(n2,base[i],prods[i],i);
  else if (!pf_coeffs(n2,rhs,val)) return false;
  vecteur res(1,Q); // the fractions with their values, written as the answers are
  for (unsigned i=0;i<dens.size();++i) res.push_back(fr(subst(symb_prod(terms[i],dens[i]),*PU_,val,false,CTX),dens[i]));
  vecteur res2;
  EACH(it,res) if (!is_zero(*it)) res2.push_back(*it);
  S(R_PFRES,sumof(res2),0);
  return true;
}

// ---------------------------------------------------------------- implicit differentiation (DeltaCE)
// implicitdiff(eq,x,y): both sides differentiated in x, y a function of x (the chain rule: y^2 is
// 2y dy/dx), the dy/dx terms to one side, dy/dx factored out, both sides divided (giac's implicitdiff,
// kmisc.cc, gives -F_x/F_y, normal: the same)
static NOINL bool impl_steps(const gen & input){
  const gen & a=arg(input);
  if (a.type!=_VECT || a._VECTptr->size()<2) return false;
  const gen x=a[1],y=a._VECTptr->size()>2?a[2]:gen(identificateur("y"));
  if (x.type!=_IDNT || y.type!=_IDNT || x==y) return false;
  std::string nm="d"; nm+=pr(y); nm+="/d"; nm+=pr(x);
  const gen Yp=gen(identificateur(nm)),e=a[0];
  const gen L=ev(is(e,at_equal)?arg(e,0):e),R=ev(is(e,at_equal)?arg(e,1):gen(0));
  if (!dep(L,y) && !dep(R,y)) return false;
  X=x;
  S(R_IDIFF,L,R);
  const gen Ld=ev(D(L,x)+D(L,y)*Yp),Rd=ev(D(R,x)+D(R,y)*Yp);
  S(R_ICHAIN,Ld,Rd);
  const gen P=ev(expand(Ld-Rd,CTX)),A=ratnormal(D(P,Yp),CTX),B=ev(sub(P,Yp,0)),mB=ratnormal(-B,CTX);
  if (is_zero(A) || dep(A,Yp)) return false;
  vecteur t;
  flat(at_plus,ev(expand(A,CTX)),t);
  EACHW(it,t) *it=is_one(*it)?Yp:is_minus_one(*it)?SYM(at_neg,Yp):SYM2(at_prod,*it,Yp); // (2*y*dy/dx: dy/dx last)
  const gen side=t.size()==1?t[0]:symbolic(at_plus,gen(t,_SEQ__VECT));
  S(R_ISIDE,side,mB);
  gen l=side;
  if (t.size()>1){ l=SYM2(at_prod,A,Yp); S(R_IFACT,l,mB); }
  const gen r0=normal(mB/A,CTX),n=_numer(r0,CTX),d=_denom(r0,CTX);
  const gen r=pr(d)[0]=='-'?SYM2(at_prod,normal(-n,CTX),SYM(at_inv,normal(-d,CTX))):r0; // (as implicitdiff: no -(...) below)
  if (!is_one(A)){ S(R_ADIV,l,mB,A); S(R_ACONT,Yp,r); }
  return true;
}

bool steps_of(const gen & input,std::vector<step_t> & out,GIAC_CONTEXT){
  static gen * in0; // the input as the steps take it (in_ points to it; no static constructor in the app)
  if (!in0) in0=new gen;
  gen x(identificateur("x"));
  ctx_=contextptr; out_=&out; depth_=0; nsub_=0; var_=&x; in_=in0;
  steps_methods=0; steps_method_name=""; rel_=0;
  if (has_op(input,*at_desolve)) return false; // (diff(y,x) in it is the equation: "d/dx y = 0" was shown)
  if ((input.is_symb_of_sommet(at_equal) || rel_of(input)) && one_var(input)) // (DeltaCE) typed alone: solved, as the console does
    return eq_steps(symbolic(at_solve,makesequence(input,X))) && !out.empty();
  if (input.is_symb_of_sommet(at_solve) && lop(input,at_derive).empty() && lop(input,at_integrate).empty() && lop(input,at_limit).empty() && lop(input,at_sum).empty())
    return eq_steps(input) && !out.empty(); // (an equation; solve(diff(f,x)=0,x): its derivative's steps first, below)
  if (input.is_symb_of_sommet(at_factor)) return factor_steps(input);
  if (input.is_symb_of_sommet(at_implicitdiff)) return impl_steps(input) && !out.empty();
  if (input.is_symb_of_sommet(at_canonical_form)) return cs_steps(input) && !out.empty();
  if (input.is_symb_of_sommet(at_propfrac)) return div_steps(input) && !out.empty();
  if (input.is_symb_of_sommet(at_partfrac)) return pf_steps(input) && !out.empty();
  if (div_of(input,true)) return !out.empty(); // (DeltaCE) a division of polynomials typed: its long division
  gen lhs=0,ap=app_(input,true,lhs); // an application: its setup, then its integral's steps
  var_=&x;
  if (appmsg_) return false;
  *in0=is_zero(ap)?serfix(input):ap;
  gen ls=limit_simp(*in0);
  if (ls!=*in0){ S(R_SIMP,*in0,ls); *in0=ls; }
  const gen & input0=*in0;
  gen e=nest(norm(input0));
  for (int guard=0;guard<8;++guard){
    gen t=inner(e);
    if (is_zero(t)) break;
    if (arg(t).type!=_VECT) return false;
    const vecteur & v=*arg(t)._VECTptr;
    if (v.size()<2 || v[1].type!=_IDNT) return false;
    if (t==e) return !is_undef(one(e));
    S(R_PART,t,0);
    ++depth_; gen val=one(t); --depth_;
    if (is_undef(val)) return false;
    e=quotesubst(e,t,val,CTX);
  }
  if (out.empty()) return false;
  e=ev(e);
  S(R_EQV,is_zero(lhs)?input0:lhs,bigp(e)?e:normal(e,CTX));
  return true;
}
#endif
