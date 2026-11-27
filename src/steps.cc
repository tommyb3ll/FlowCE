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
  R_FGCF, R_FDSQ, R_FSUMC, R_FDIFC, R_FPSQ, R_FTWO, R_FSPLIT, R_FGROUP, R_FQUAD, R_FROOT, R_FZERO, R_FSOLVE, R_FNOREAL, R_FPRIME, R_FPRIME2, R_FREORD, R_FQFORM, R_FSOAP, R_FCHECK,
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
  R_PSLT, R_PSOPEN, R_PSALL, R_PSONE,
  R_KKNOWN, R_KDIFF, R_KDIFF2, R_KINTG, R_KINTG2, R_KSUBS, R_KMULT, R_KINT, R_KALL, R_KRAD, R_KTERM, R_KNONE, R_KCOEF, R_KDER, R_KPOLY, R_KADD,
  R_LGDOM, R_LGROW, R_LGEO, R_LRAT, R_PSUMW, R_RATIOD,
  R_TWA, R_TWS, R_TSN, R_TDEF,
  R_RAK, R_RLIM, R_RGRP, R_RCAN, R_RVAL, R_RCONV, R_RDIVG, R_PSRAD, R_PSRADA, R_PSRAD0, R_RLIM1,
  R_GFORM, R_GAR, R_GSUM, R_GDIV,
  R_KREPL, R_KSIMP, R_KINT2, R_KWOUT, R_KWADD, R_KWP, R_KFOR, R_LRATU, R_SDIV2, R_SCTR, R_SCTR2,
  R_ABSU, R_ICON, R_IPOW, R_SUBV, R_KPMUL, R_LSER, R_LSERL, R_LPOWB, R_LLAWS, R_KWD1, R_KWD2, R_KWLN, R_KWAT, R_KDTERM, R_LGEOI, R_TLET, R_LCMP0, R_LCMPI, R_RDEF, R_QISO, R_QROOT, R_QSQN, R_PCOMB, R_LE1, R_RPAIR, R_TCOEF, R_TADD, R_SWX, R_KSHIFT, R_KCZERO, R_KVALX, R_KVALS, R_GSPLIT, R_GADD, R_SIMPT, R_LALT, R_LALT0, R_LALTN, R_PSUMF, R_ZETA, R_KVALD, R_KCZERO2, R_LNSPLIT, R_SEQT, R_SEQL, R_QCPX,
#if defined FLOWCE_VECTORS || defined HOSTGIAC
  R_VGIVEN, R_VL, R_VDOT, R_VCROSS, R_VMAG, R_VTRIG, R_VADD, R_VSMUL, R_VPROJ, R_VSCAL, R_VANG, R_VORTH, R_VPAR, R_VALL, R_VPN, R_VCHK,
  R_VANGD, R_VUNDEF, R_VANS,
  R_CTIME, R_CNOT, R_CTCHK, R_CTBAD, R_CDIR, R_CPAR, R_CNPAR, R_CINT, R_CEQ, R_CSOLVE, R_CCHK, R_CCHKN, R_CSKEW, R_CMEET,
  R_CSAME, R_CPARD, R_CNEED, R_CDER, R_CEVAL, R_CLINE, R_CARC, R_CLIM, R_CSO, R_CAP1, R_CAP2, R_CAP3, R_CAPY, R_CAPN,
  R_CINTG, R_CIC, R_CSUBS, R_CPTS, R_CCOMP, R_CALL, R_CNONE, R_CRNG, R_CNOCUT, R_CPART, R_PLET, R_PPUT, R_PSOLV, R_PELL, R_PCS, R_PEVERY, R_PNOT, R_PSIGN, R_DCOND, R_DSOLV, R_DOUT, R_FEQ, R_FSOLV, R_FEX, R_VLN,
#endif
  R_NRULES
};
struct rule_t { const char * title; const char * formula; unsigned char tpl; }; // (a rule: its title, its formula or 0, its template)
// The titles and formulas packed as the catalog's descriptions (DeltaCE; tools/catpack.py src/steps.cc
// src/steps_text.h ST st SP, which tools/dev/wsl_build_v15.sh runs): ST of a literal is its packed bytes,
// found at compile time; st_unpack expands them (11.0 KB of text before, 2026-11-27). SP: a text left
// as it is (the vectors' rules, built with that pack only), its bytes never a pair's code.
#include "steps_text.h"
static constexpr unsigned long st_fnv(const char * s, unsigned long h) { return *s ? st_fnv(s + 1, ((h ^ (unsigned char)*s) * 16777619ul) & 0xFFFFFFFFul) : h; }
static constexpr int st_at(unsigned long h) { for (int i = 0; i < ST_N; ++i) if (st_keys[i] == h) return i; return -1; }
template <int I> struct st_index { static_assert(I >= 0, "a rule text is not in src/steps_text.h: run tools/catpack.py src/steps.cc src/steps_text.h ST st SP"); enum { v = I }; };
#define ST(s) (st_text + st_offs[st_index<st_at(st_fnv(s, 2166136261ul))>::v])
#define SP(s) s
static void st_put(unsigned char c, char *& o) {
  if (const unsigned char k = st_code[c]) { st_put(st_pair[k - 1][0], o); st_put(st_pair[k - 1][1], o); }
  else *o++ = c;
}
static __attribute__((noinline)) const char * st_unpack(const char * p, char * buf) { // (buf: ST_MAX + 1 bytes)
  if (!p) return p;
  char * o = buf;
  while (*p) st_put((unsigned char)*p++, o);
  *o = 0;
  return buf;
}
// the templates of the table below (a rule's third column): one byte each in the table, the strings once here
enum { T_A, T_A_is_B, T_integrate_A_and_V_is_B, T_diff_A_and_V_is_B, T_integrate_A_and_V_is_integrate_B_and_C, T_diff_A_and_V_is_B_diff_C_and_V, T_A_and_B, T_A_C_is_B_C, T_A_is_0, T_V_is_A_and_V_is_B, T_diff_A_and_V_is_diff_B_and_V_C_B_diff_C_and_V, T_diff_A_and_V_is_diff_B_and_V_C_B_diff_C_and_V_C_2, T_integrate_A_and_V_is_B_integrate_C_and_V, T_A_is_B_and_dA_is_C_dV, T_U_is_A_and_dW_is_B_dV, T_dU_is_A_dV_and_W_is_B, T_V_is_B_and_dV_is_C_dA, T_sin_V_is_A_and_cos_V_is_B, T_R_V_is_A, T_R_V_is_A_and_r_V_is_B, T_r_V_is_A_and_h_V_is_B, T_diff_f_V_and_V_is_A, T_diff_x_and_V_is_A_and_diff_y_and_V_is_B, T_r_V_is_A_and_diff_f_V_and_V_is_B, T_sqrt_A_is_B, T_abs_A_is_1, T_C_A_is_C_B, T_a_is_A_and_b_is_B_and_c_is_C, T_V_is_A, T_A_0, T_diff_A_and_V_is_diff_B_and_V, T_A_1, T_u_A };
static const char * const TPLS[]={
  "A", // T_A
  "A=B", // T_A_is_B
  "integrate(A,V)=B", // T_integrate_A_and_V_is_B
  "diff(A,V)=B", // T_diff_A_and_V_is_B
  "integrate(A,V)=integrate(B,C)", // T_integrate_A_and_V_is_integrate_B_and_C
  "diff(A,V)=B*diff(C,V)", // T_diff_A_and_V_is_B_diff_C_and_V
  "A,B", // T_A_and_B
  "(A)/C=(B)/C", // T_A_C_is_B_C
  "A=0", // T_A_is_0
  "V=A,V=B", // T_V_is_A_and_V_is_B
  "diff(A,V)=diff(B,V)*C+B*diff(C,V)", // T_diff_A_and_V_is_diff_B_and_V_C_B_diff_C_and_V
  "diff(A,V)=(diff(B,V)*C-B*diff(C,V))/C^2", // T_diff_A_and_V_is_diff_B_and_V_C_B_diff_C_and_V_C_2
  "integrate(A,V)=B*integrate(C,V)", // T_integrate_A_and_V_is_B_integrate_C_and_V
  "A=B,dA=C*dV", // T_A_is_B_and_dA_is_C_dV
  "U=A,dW=B*dV", // T_U_is_A_and_dW_is_B_dV
  "dU=A*dV,W=B", // T_dU_is_A_dV_and_W_is_B
  "V=B,dV=C*dA", // T_V_is_B_and_dV_is_C_dA
  "sin(V)=A,cos(V)=B", // T_sin_V_is_A_and_cos_V_is_B
  "R(V)=A", // T_R_V_is_A
  "R(V)=A,r(V)=B", // T_R_V_is_A_and_r_V_is_B
  "r(V)=A,h(V)=B", // T_r_V_is_A_and_h_V_is_B
  "diff(f(V),V)=A", // T_diff_f_V_and_V_is_A
  "diff(x,V)=A,diff(y,V)=B", // T_diff_x_and_V_is_A_and_diff_y_and_V_is_B
  "r(V)=A,diff(f(V),V)=B", // T_r_V_is_A_and_diff_f_V_and_V_is_B
  "sqrt(A)=B", // T_sqrt_A_is_B
  "abs(A)>=1", // T_abs_A_is_1
  "C*(A)=C*(B)", // T_C_A_is_C_B
  "a=A,b=B,c=C", // T_a_is_A_and_b_is_B_and_c_is_C
  "V=A", // T_V_is_A
  "A<0", // T_A_0
  "diff(A,V)=diff(B,V)", // T_diff_A_and_V_is_diff_B_and_V
  "A<1", // T_A_1
  "u=A", // T_u_A
};
static const rule_t RULES[]={
  {ST(""),0,T_diff_A_and_V_is_B},
  {ST("Sum rule"),ST("diff(u+v,x)=diff(u,x)+diff(v,x)"),T_diff_A_and_V_is_B},
  {ST("Constant multiple"),ST("diff(c*u,x)=c*diff(u,x)"),T_diff_A_and_V_is_B_diff_C_and_V},
  {ST("Product rule"),ST("diff(u*v,x)=diff(u,x)*v+u*diff(v,x)"),T_diff_A_and_V_is_diff_B_and_V_C_B_diff_C_and_V},
  {ST("Quotient rule"),ST("diff(u/v,x)=(diff(u,x)*v-u*diff(v,x))/v^2"),T_diff_A_and_V_is_diff_B_and_V_C_B_diff_C_and_V_C_2},
  {ST("Power rule"),ST("diff(x^n,x)=n*x^(n-1)"),T_diff_A_and_V_is_B},
  {ST("Chain rule"),ST("diff(f(u),x)=diff(f,u)*diff(u,x)"),T_diff_A_and_V_is_B_diff_C_and_V},
  {ST("Known derivative"),0,T_diff_A_and_V_is_B},
  {ST("Exponential rule"),ST("diff(a^x,x)=a^x*ln(a)"),T_diff_A_and_V_is_B},
  {ST("Write the power with e"),0,T_A_is_B},
  {ST(""),0,T_integrate_A_and_V_is_B},
  {ST("Sum rule"),ST("integrate(u+v,x)=integrate(u,x)+integrate(v,x)"),T_integrate_A_and_V_is_B},
  {ST("Constant multiple"),ST("integrate(c*u,x)=c*integrate(u,x)"),T_integrate_A_and_V_is_B_integrate_C_and_V},
  {ST("Power rule"),ST("integrate(x^n,x)=x^(n+1)/(n+1)"),T_integrate_A_and_V_is_B},
  {ST("Integral of a constant"),ST("integrate(c,x)=c*x"),T_integrate_A_and_V_is_B},
  {ST("Integral of 1/x"),ST("integrate(1/x,x)=ln(abs(x))"),T_integrate_A_and_V_is_B},
  {ST("Known integral"),0,T_integrate_A_and_V_is_B},
  {ST("Arctangent formula"),ST("integrate(1/(a^2+x^2),x)=atan(x/a)/a"),T_integrate_A_and_V_is_B},
  {ST("Arcsine formula"),ST("integrate(1/sqrt(a^2-x^2),x)=asin(x/a)"),T_integrate_A_and_V_is_B},
  {ST("Logarithm formula"),ST("integrate(1/sqrt(x^2+c),x)=ln(abs(x+sqrt(x^2+c)))"),T_integrate_A_and_V_is_B},
  {ST("Substitution"),0,T_A_is_B_and_dA_is_C_dV},
  {ST(""),0,T_integrate_A_and_V_is_integrate_B_and_C},
  {ST("Substitute back"),0,T_integrate_A_and_V_is_B},
  {ST("Integration by parts"),ST("integrate(u,v)=u*v-integrate(v,u)"),T_U_is_A_and_dW_is_B_dV},
  {ST(""),0,T_dU_is_A_dV_and_W_is_B},
  {ST(""),0,T_integrate_A_and_V_is_B},
  {ST("The integral comes back: solve for it"),0,T_integrate_A_and_V_is_B},
  {ST("Partial fractions"),0,T_A_is_B},
  {ST("Trig identity"),0,T_A_is_B},
  {ST("Pythagorean identity"),ST("tan(x)^2=sec(x)^2-1"),T_A_is_B},
  {ST("Expand"),0,T_A_is_B},
  {ST("Complete the square"),0,T_A_is_B},
  {ST("Simplify"),0,T_A_is_B},
  {ST("Trig substitution"),0,T_V_is_B_and_dV_is_C_dA},
  {ST(""),0,T_A_is_B},
  {ST(""),0,T_integrate_A_and_V_is_integrate_B_and_C},
  {ST("Back to x with a right triangle"),0,T_sin_V_is_A_and_cos_V_is_B},
  {ST("Split the numerator"),0,T_A_is_B},
  {ST("Multiply by the conjugate"),0,T_A_is_B},
  {ST("Substitution t = tan(x/2)"),ST("[sin(x)=2*t/(1+t^2),cos(x)=(1-t^2)/(1+t^2),dx=2/(1+t^2)*dt]"),T_integrate_A_and_V_is_integrate_B_and_C},
  {ST("Split where the inside changes sign"),0,T_A_is_B},
  {ST("Improper integral: a limit"),0,T_A_is_B},
  {ST("No elementary antiderivative: the decimal value"),0,T_A_is_B},
  {ST("Evaluate from a to b"),ST("integrate(f,x,a,b)=F(b)-F(a)"),T_A_is_B},
  {ST(""),0,T_A_is_B},
  {ST("Compute"),0,T_A},
  {ST("Sign on the interval"),0,T_A_is_B},
  {ST("Fundamental theorem of calculus"),ST("diff(integrate(f(t),t,a,u),x)=f(u)*diff(u,x)"),T_diff_A_and_V_is_B},
  {ST("Direct substitution"),0,T_A_is_B},
  {ST("Limit laws"),0,T_A_is_B},
  {ST("Factor and cancel"),0,T_A_is_B},
  {ST("Multiply by the conjugate"),0,T_A_is_B},
  {ST("L'Hopital's rule"),ST("limit(f/g,x,a)=limit(diff(f,x)/diff(g,x),x,a)"),T_A_is_B},
  {ST("Divide by the highest power"),0,T_A_is_B},
  {ST("Write the product as a quotient"),0,T_A_is_B},
  {ST("Write the power with e"),ST("u^v=e^(v*ln(u))"),T_A_is_B},
  {ST("The denominator goes to 0, the numerator does not"),0,T_A_is_B},
  {ST("The one-sided limits differ: no limit"),0,T_A_and_B},
  {ST("Squeeze theorem: bounded times going to 0"),0,T_A_is_B},
  {ST("Limit of a continuous function"),ST("limit(f(u),x,a)=f(limit(u,x,a))"),T_A_is_B},
  {ST("One fraction"),0,T_A_is_B},
  {ST("Factor out the fastest term"),0,T_A_is_B},
  {ST("Derivatives"),0,T_A_is_B},
  {ST("Their values at the center"),0,T_A_is_B},
  {ST("Taylor polynomial"),0,T_A_is_B},
  {ST("Divergence test: the terms do not go to 0"),0,T_A_is_B},
  {ST("Ratio of consecutive terms"),0,T_A_is_B},
  {ST("Geometric series with |r| < 1"),ST("sum(a*r^k,k,0,inf)=a/(1-r)"),T_A_is_B},
  {ST("Partial sum"),0,T_A_is_B},
  {ST("Partial sum: the middle terms cancel"),0,T_A_is_B},
  {ST("Its limit"),0,T_A_is_B},
  {ST("p-series: converges for p > 1, diverges for p <= 1"),ST("sum(1/n^p,n,1,inf)"),T_A_is_B},
  {ST("Limit comparison with the p-series"),ST("limit(a(n)/b(n),n,inf)=L"),T_A_is_B},
  {ST("Indeterminate form"),0,T_A_and_B},
  {ST("Alternating series test: the size decreases to 0"),0,T_A_is_B},
  {ST("Ratio test: below 1, the series converges"),0,T_A_is_B},
  {ST("Root test: below 1, the series converges"),0,T_A_is_B},
  {ST("Log of a quotient"),ST("ln(a/b)=ln(a)-ln(b)"),T_A_is_B},
  {ST("Integral test: same behavior as the integral"),0,T_A},
  {ST("cos(n pi) is (-1)^n"),0,T_A_is_B},
  {ST("Oscillates between -1 and 1: no limit"),0,T_A_is_B},
  {ST("Long division"),0,T_A_is_B},
  {ST("Area between curves"),ST("A=integrate(abs(f(x)-g(x)),x,a,b)"),T_A_is_B},
  {ST("Disk method"),ST("V=pi*integrate(R(x)^2,x,a,b)"),T_R_V_is_A},
  {ST("Washer method"),ST("V=pi*integrate(R(x)^2-r(x)^2,x,a,b)"),T_R_V_is_A_and_r_V_is_B},
  {ST("Shell method"),ST("V=2*pi*integrate(r(x)*h(x),x,a,b)"),T_r_V_is_A_and_h_V_is_B},
  {ST("Arc length"),ST("L=integrate(sqrt(1+diff(f(x),x)^2),x,a,b)"),T_diff_f_V_and_V_is_A},
  {ST("Arc length of a parametric curve"),ST("L=integrate(sqrt(diff(x,t)^2+diff(y,t)^2),t,a,b)"),T_diff_x_and_V_is_A_and_diff_y_and_V_is_B},
  {ST("Surface area"),ST("S=2*pi*integrate(r(x)*sqrt(1+diff(f(x),x)^2),x,a,b)"),T_r_V_is_A_and_diff_f_V_and_V_is_B},
  {ST("Average value"),ST("integrate(f(x),x,a,b)/(b-a)"),T_A_is_B},
  {ST("Where the curves meet"),0,T_A_is_B},
  {ST("A perfect square"),0,T_sqrt_A_is_B},
  {ST(""),0,T_A},
  {ST("Geometric series with |r| >= 1: diverges"),0,T_abs_A_is_1},
  {ST("Converges when |r| < 1"),0,T_A},
  {ST("Sign on that side"),0,T_A_is_B},
  // equations (DeltaCE): A, B the sides, C the common denominator
  {ST("Distribute and combine like terms"),0,T_A_is_B},
  {ST("Multiply both sides by the common denominator"),0,T_C_A_is_C_B},
  {ST("Swap the sides"),0,T_A_is_B},
  {ST("Subtract from both sides"),0,T_A_is_B},
  {ST("Add to both sides"),0,T_A_is_B},
  {ST("Divide both sides"),0,T_A_C_is_B_C},
  {ST(""),0,T_A_is_B},
  {ST("Always true: every number is a solution"),0,T_A_is_B},
  {ST("Never true: no solution"),0,T_A_is_B},
  {ST("Move every term to one side"),0,T_A_is_0},
  {ST("Identify a, b and c"),0,T_a_is_A_and_b_is_B_and_c_is_C},
  {ST("Quadratic formula"),ST("x=(-b\xc2\xb1sqrt(b^2-4ac))/(2a)"),T_A},
  {ST("The discriminant"),ST("b^2-4ac"),T_A},
  {ST("Simplify"),0,T_A},
  {ST(""),0,T_A},
  {ST("Two solutions"),0,T_V_is_A_and_V_is_B},
  {ST(""),0,T_V_is_A_and_V_is_B},
  {ST("The discriminant is 0: one solution"),0,T_V_is_A},
  {ST("The discriminant is negative: no real solution"),0,T_A_0},
  // factoring (DeltaCE): A a line written as text, or an equation
  {ST("Factor out the greatest common factor"),0,T_A},
  {ST("Difference of squares"),ST("a^2-b^2=(a-b)(a+b)"),T_A},
  {ST("Sum of cubes"),ST("a^3+b^3=(a+b)(a^2-ab+b^2)"),T_A},
  {ST("Difference of cubes"),ST("a^3-b^3=(a-b)(a^2+ab+b^2)"),T_A},
  {ST("Perfect square trinomial"),ST("a^2+2ab+b^2=(a+b)^2"),T_A},
  {ST("Two numbers: product c, sum b"),0,T_A},
  {ST("Split the middle term (ac method)"),0,T_A},
  {ST("Factor by grouping"),0,T_A},
  {ST("No whole numbers: roots by the formula"),ST("x=(-b\xc2\xb1sqrt(b^2-4ac))/(2a)"),T_A},
  {ST("A rational root: divide by its factor"),0,T_A},
  {ST("Zero product property: each factor is 0"),0,T_A},
  {ST("Solve each"),0,T_A},
  {ST("No real solution from this factor"),0,T_A_is_0},
  {ST("No real roots: does not factor"),0,T_A},
  {ST("Does not factor further"),0,T_A},
  {ST("Rearrange the terms"),0,T_A},
  {ST("Quadratic form: substitute u"),0,T_A},
  {ST("SOAP: Same, Opposite, Always Positive"),0,T_A},
  {ST("Check: multiply the factors"),0,T_A},
  // rational and radical equations (DeltaCE)
  {ST("Excluded values: no denominator can be 0"),0,T_A},
  {ST("Multiply both sides by the LCD"),0,T_A},
  {ST("Check: not an excluded value"),0,T_A},
  {ST("An excluded value: not a solution"),0,T_A},
  {ST("Square both sides"),0,T_A},
  {ST("Check in the equation: both sides agree"),0,T_A},
  {ST("Check: the sides differ, extraneous"),0,T_A},
  {ST("Every candidate is extraneous: no solution"),0,T_A},
  // F4's forms (DeltaCE): completing the square, long division; A a line written as text
  {ST("Group the x terms"),0,T_A},
  {ST("Factor a out of the x terms"),0,T_A},
  {ST("Half the x coefficient, squared"),ST("(b/2)^2"),T_A},
  {ST("Add it and subtract it"),0,T_A},
  {ST("Write the perfect square"),0,T_A},
  {ST("Distribute"),0,T_A},
  {ST("Combine the numbers"),0,T_A},
  {ST("The vertex"),ST("a*(x-h)^2+k: (h,k)"),T_A},
  {ST("Complete the square in"),0,T_A},
  {ST("Put it back"),0,T_A},
  {ST("Divide the leading terms"),0,T_A},
  {ST("Multiply back and subtract"),0,T_A},
  {ST("The remainder's degree is lower: done"),0,T_A},
  {ST("Check: divisor times quotient, plus remainder"),0,T_A},
  {ST(""),0,T_A}, // (a division typed: quotient plus remainder over the divisor, the answer box)
  // synthetic division, factor and cancel (DeltaCE)
  {ST("Write c, where the divisor is 0"),0,T_A},
  {ST(""),0,T_A}, // (the table)
  {ST("Bring down the first coefficient"),0,T_A},
  {ST("Multiply by c, add down the column"),0,T_A},
  {ST("Bottom row: the quotient, then the remainder"),0,T_A},
  {ST("Divide the quotient by the x coefficient"),0,T_A},
  {ST("Factor the top and the bottom"),0,T_A},
  {ST("Cancel the common factors"),0,T_A},
  // partial fractions (DeltaCE)
  {ST("The top's degree is not lower: divide first"),0,T_A},
  {ST("Factor the denominator"),0,T_A},
  {ST("A fraction for each factor"),0,T_A},
  {ST("Multiply both sides by the denominator"),0,T_A},
  {ST("Put in a root of the denominator"),0,T_A},
  {ST("Match the coefficients of each power of x"),0,T_A},
  {ST("Solve for the unknowns"),0,T_A},
  {ST("Put the values in"),0,T_A},
  // systems of linear equations (DeltaCE): elimination
  {ST("Write each equation in standard form"),0,T_A},
  {ST("Clear the fractions: multiply by the LCD"),0,T_A},
  {ST("Multiply so that a variable drops out"),0,T_A},
  {ST("Add the equations"),0,T_A},
  {ST("Subtract the equations"),0,T_A},
  {ST("Put it into an equation"),0,T_A},
  {ST("The solution"),0,T_A},
  {ST("Never true: no solution (the lines are parallel)"),0,T_A},
  {ST("Always true: infinitely many (the same line)"),0,T_A},
  {ST("Take a pair of equations"),0,T_A},
  {ST("Two equations in two unknowns"),0,T_A},
  {ST("Never true: no solution"),0,T_A},
  {ST("Always true: infinitely many solutions"),0,T_A},
  {ST("Write both sides as powers of the same base"),0,T_A},
  {ST("Same base: the exponents are equal"),0,T_A},
  {ST("Take ln of both sides"),0,T_A},
  {ST("Power rule of logs"),ST("ln(a^u)=u*ln(a)"),T_A},
  {ST("Exponential form: log_b(u) = c means u = b^c"),0,T_A},
  {ST("Product rule of logs"),ST("log(a)+log(b)=log(a*b)"),T_A_is_B},
  {ST("Quotient rule of logs"),ST("log(a)-log(b)=log(a/b)"),T_A_is_B},
  {ST("Power rule of logs"),ST("k*log(a)=log(a^k)"),T_A_is_B},
  {ST("Same base: the arguments are equal"),0,T_A},
  {ST("Check: every log's argument is positive"),0,T_A},
  {ST("A log of a number <= 0: not a solution"),0,T_A},
  {ST("A power of a positive base is never <= 0: no solution"),0,T_A},
  {ST("A quadratic in the power: substitute"),0,T_A},
  {ST("Back to x"),0,T_A},
  {ST("Unknowns to one side, the rest to the other"),0,T_A_is_B},
  {ST("Factor out x"),0,T_A_is_B},
  {ST("Move the logs to one side"),0,T_A_is_B},
  {ST("The reference angle"),0,T_A_is_B},
  {ST("sin > 0: quadrants I and II"),0,T_A},
  {ST("sin < 0: quadrants III and IV"),0,T_A},
  {ST("cos > 0: quadrants I and IV"),0,T_A},
  {ST("cos < 0: quadrants II and III"),0,T_A},
  {ST("tan > 0: quadrants I and III"),0,T_A},
  {ST("tan < 0: quadrants II and IV"),0,T_A},
  {ST("On an axis of the unit circle"),0,T_A},
  {ST("All solutions: add n periods (n an integer)"),0,T_A},
  {ST("Solve for x"),0,T_A},
  {ST("The solutions in [0, 2pi)"),0,T_A},
  {ST("sin and cos are between -1 and 1: no solution"),0,T_A_is_B},
  {ST("Divide both sides by cos: sin/cos is tan"),0,T_A_is_B},
  {ST("Pythagorean identity"),ST("sin(x)^2+cos(x)^2=1"),T_A_is_B},
  {ST("The angles where"),0,T_A_is_B},
  {ST("Differentiate both sides: y is a function of x"),0,T_diff_A_and_V_is_diff_B_and_V},
  {ST("Chain rule: each y term's derivative times dy/dx"),0,T_A_is_B},
  {ST("The dy/dx terms to one side, the rest to the other"),0,T_A_is_B},
  {ST("Factor out dy/dx"),0,T_A_is_B},
  // inequalities, absolute values (DeltaCE): a step's = is its relation (step_t rel)
  {ST("Divide by a negative: the sign flips"),0,T_A_C_is_B_C},
  {ST("Factor"),0,T_A},
  {ST("Where it is 0 or undefined"),0,T_A},
  {ST("Test a number in each interval"),0,T_A},
  {ST(""),0,T_A},
  {ST("Keep where it is negative"),0,T_A},
  {ST("Keep where it is negative or 0"),0,T_A},
  {ST("Keep where it is positive"),0,T_A},
  {ST("Keep where it is positive or 0"),0,T_A},
  {ST("No interval works: no solution"),0,T_A},
  {ST("Every number works"),0,T_A},
  {ST("An absolute value is never negative"),0,T_A},
  {ST("Only 0 has absolute value 0"),0,T_A},
  {ST("Two cases: the inside is k or -k"),0,T_A},
  {ST("First case"),0,T_A_is_B},
  {ST("Second case"),0,T_A_is_B},
  {ST("Less than k: between -k and k"),0,T_A},
  {ST("The same to all three parts"),0,T_A},
  {ST("Divide all three parts"),0,T_A},
  {ST("Divide by a negative: the signs flip"),0,T_A},
  {ST("More than k: below -k or above k"),0,T_A},
  {ST("Every number works"),0,T_A},
  {ST("Every number but where the inside is 0"),0,T_A},
  {ST("The solutions"),0,T_A},
  {ST("Ratio test: converges where the limit is below 1"),0,T_A_1},
  {ST("The open interval of convergence"),0,T_A},
  {ST("Limit 0 for every x: converges for all x"),0,T_A},
  {ST("Infinite unless x is the center"),0,T_A},
  {ST("Known series"),0,T_A_is_B},
  {ST("The geometric series, differentiated"),0,T_A_is_B},
  {ST("The geometric series, differentiated twice, halved"),0,T_A_is_B},
  {ST("The series of 1/(1+x), integrated"),0,T_A_is_B},
  {ST("The arctangent: a series integrated"),ST("atan(x)=integrate(1/(1+t^2),t,0,x)"),T_A_is_B},
  {ST("Substitute"),0,T_u_A},
  {ST("Multiply each term"),0,T_A},
  {ST("Integrate term by term"),0,T_A},
  {ST("Converges for every x"),0,T_A},
  {ST("Where it converges: |inside| < 1"),0,T_A},
  {ST("The term with that power"),0,T_A_is_B},
  {ST("No term with that power: coefficient 0"),0,T_A},
  {ST("Its coefficient"),0,T_A_is_B},
  {ST("The derivative: n! times the coefficient"),ST("f^((n))(0)=n!*a_n"),T_A_is_B},
  {ST("The terms up to that degree"),0,T_A},
  {ST("Add the polynomials"),0,T_A_is_B},
  {ST("The dominant terms (growth rates)"),0,T_A_is_B},
  {ST("Growth rates, the slowest first"),ST("ln(n)^p, n^q, b^n, n!, n^n"),T_A_is_B},
  {ST("A geometric sequence with |r| < 1"),ST("limit(r^n,n,inf)=0"),T_A_is_B},
  {ST("Ratio below 1: the terms go to 0"),0,T_A_is_B},
  {ST("Write out the terms"),0,T_A_is_B},
  {ST("Ratio test: above 1, the series diverges"),0,T_A_is_B},
  {ST("Write out some terms"),0,T_A},
  {ST("Write out some partial sums: cancel what cancels"),0,T_A},
  {ST("The pattern: a formula for the partial sums"),0,T_A},
  {ST("Definition of convergence"),0,T_A},
  {ST("Ratio test: the next term, the index + 1 put in"),0,T_A},
  {ST("Dividing by a term: times its reciprocal"),0,T_A},
  {ST("Group the like factors"),0,T_A},
  {ST("The constants out of the limit"),0,T_A},
  {ST("The limit L"),0,T_A},
  {ST("Ratio test: L < 1, the series converges"),0,T_A},
  {ST("Ratio test: L > 1, the series diverges"),0,T_A},
  {ST("The center and the radius of convergence"),0,T_A},
  {ST("Radius of convergence: infinite"),0,T_A},
  {ST("Radius of convergence: 0"),0,T_A},
  {ST("The ratio of the next term to the term"),0,T_A},
  {ST("Rules of exponents: the form"),ST("a*r^k"),T_A},
  {ST("A geometric series"),0,T_A},
  {ST("|r| < 1: it converges to"),ST("a*r^(k_0)/(1-r)"),T_A},
  {ST("|r| >= 1: the geometric series diverges"),0,T_A},
  {ST("Replace x by the inside"),0,T_A},
  {ST("Simplify each term: the rules of exponents"),0,T_A},
  {ST("The constants out of each integral"),0,T_A},
  {ST("Known series, written out"),0,T_A},
  {ST("Put the series together"),0,T_A},
  {ST("The Taylor polynomial: the terms up to that degree"),0,T_A},
  {ST("For this term"),0,T_A},
  {ST("Ratio above 1: the terms grow without bound"),0,T_A_is_B},
  {ST("Divergence test: the terms do not go to 0"),0,T_A},
  {ST("Put the center in"),ST("0^0=1"),T_A},
  {ST("Only the first term is not 0"),0,T_A},
  {ST("The absolute value alone, then |u| < R: -R < u < R"),0,T_A},
  {ST("The constant out of the integral"),0,T_A},
  {ST("Power rule"),ST("integrate(t^n,t)=t^(n+1)/(n+1)"),T_A},
  {ST("Put the number in for x"),0,T_A},
  {ST("Multiply the series, up to that degree"),0,T_A},
  {ST("Use the Taylor series of the top and the bottom"),0,T_A},
  {ST("The lowest powers decide the limit"),0,T_A_is_B},
  {ST("The base's limit, the power to infinity"),0,T_A_is_B},
  {ST("Each term's limit"),0,T_A},
  {ST("The geometric series, differentiated, written out"),0,T_A},
  {ST("The geometric series, differentiated twice, halved"),0,T_A},
  {ST("The series of 1/(1+x), integrated, written out"),0,T_A},
  {ST("The arctangent: a series integrated, written out"),ST("atan(x)=integrate(1/(1+t^2),t,0,x)"),T_A},
  {ST("Differentiate term by term"),0,T_A},
  {ST("A geometric sequence with r > 1"),ST("limit(r^n,n,inf)=inf"),T_A_is_B},
  {ST("Let g be the other factor: its polynomial first"),0,T_A},
  {ST("Limit comparison: 0, and the p-series converges"),ST("limit(a(n)/b(n),n,inf)=0"),T_A_is_B},
  {ST("Limit comparison: infinity, the p-series diverges"),ST("limit(a(n)/b(n),n,inf)=inf"),T_A_is_B},
  {ST("The ratio test uses the absolute value of the ratio"),0,T_A},
  {ST("Isolate the square"),0,T_A},
  {ST("Square root of both sides"),0,T_A},
  {ST("A square is never negative: no real solution"),0,T_A},
  {ST("The powers with n in the exponent, together"),0,T_A_is_B},
  {ST("1 to the infinity"),ST("limit(u^v,n,inf)=e^(limit(v*(u-1),n,inf))"),T_A_is_B},
  {ST("Cancel each pair"),0,T_A},
  {ST("The coefficients"),ST("a_k=f^((k))(c)/k!"),T_A},
  {ST("Add the fractions: a common denominator"),0,T_A},
  {ST("Write out some terms"),0,T_A},
  {ST("The k = 0 term is 0: the sum from k = 1"),0,T_A},
  {ST("The constant: both sides at x = 0"),0,T_A},
  {ST("A known series at a number"),0,T_A},
  {ST("Less the terms before the series starts"),0,T_A},
  {ST("A sum of geometric series: each one apart"),ST("sum(a_k+b_k)=sum(a_k)+sum(b_k)"),T_A},
  {ST("Add their values"),0,T_A},
  {ST("Simplify the term first"),0,T_A},
  {ST("Without the sign: the size of the terms"),0,T_A_is_B},
  {ST("The size goes to 0, so the terms do too"),0,T_A_is_B},
  {ST("The sign changes each term, the size does not go to 0: no limit"),0,T_A_is_B},
  {ST("The function it adds up to (a known series)"),0,T_A},
  {ST("Euler's value of the p-series"),0,T_A},
  {ST("The geometric series, differentiated, times x"),0,T_A_is_B},
  {ST("The first terms cancel: the sum starts later"),0,T_A},
  {ST("A log of a product: ln(a*b) = ln(a)+ln(b)"),0,T_A},
  {ST("Each term from the one before"),0,T_A},
  {ST("The terms approach L: L = the recurrence at L"),0,T_A},
  {ST("Complex solutions"),ST("sqrt(-1)=i"),T_A},
#if defined FLOWCE_VECTORS || defined HOSTGIAC
  {SP("The vectors"),0,T_A},
  {SP(""),0,T_A},
  {SP("Dot product: multiply matching parts, add"),SP("u·v=u_1 v_1+u_2 v_2+u_3 v_3"),T_A},
  {SP("Cross product: the determinant along i, j, k"),0,T_A},
  {SP("Magnitude: square root of the sum of squares"),SP("abs(u)=sqrt(u_1^2+u_2^2+u_3^2)"),T_A},
  {SP("Pythagorean identity"),SP("sin(t)^2+cos(t)^2=1"),T_A},
  {SP("Component by component"),0,T_A},
  {SP("Multiply each component by the number"),0,T_A},
  {SP("Vector projection"),SP("proj_v u=((u·v)/(v·v)) v"),T_A},
  {SP("Scalar projection"),SP("scal_v u=(u·v)/abs(v)"),T_A},
  {SP("Angle between the vectors"),SP("cos(θ)=(u·v)/(abs(u) abs(v))"),T_A},
  {SP("The dot product is 0: the vectors are orthogonal"),0,T_A},
  {SP("The cross product is 0: the vectors are parallel"),0,T_A},
  {SP("Put the values in"),0,T_A},
  {SP("N = u - P: the part of u orthogonal to v"),0,T_A},
  {SP("Check that N is orthogonal to v: N·v = 0"),0,T_A},
  {SP("The angle: in radians, then in degrees"),0,T_A},
  {SP("Not defined"),0,T_A},
  {SP("So"),0,T_A},
  {SP("The time t when r(t) is the point"),0,T_A},
  {SP("No time t: the point is not on the curve"),0,T_A},
  {SP("Check the other coordinates"),0,T_A},
  {SP("Check the other coordinates: not the point"),0,T_A},
  {SP("Direction vectors: the numbers times t"),0,T_A},
  {SP("Scalar multiples: the lines are parallel"),0,T_A},
  {SP("Not scalar multiples: not parallel"),0,T_A},
  {SP("Intersection (not at the same time: t and T)"),0,T_A},
  {SP("Set the components equal"),0,T_A},
  {SP("Find t and T from two, check the third"),0,T_A},
  {SP("The third checks out"),0,T_A},
  {SP("The third does not check out"),0,T_A},
  {SP("Not parallel, they do not meet: skew"),0,T_A},
  {SP("The lines intersect"),0,T_A},
  {SP("A point of one is on the other: same line"),0,T_A},
  {SP("Not on the other: parallel, different lines"),0,T_A},
  {SP("We need a point P0 and a direction vector v"),0,T_A},
  {SP("The direction: r'(t)"),0,T_A},
  {SP("Evaluate r'(t) at that time"),0,T_A},
  {SP("The line: l(t) = v t + P0"),0,T_A},
  {SP("Arc length"),SP("s=integrate(abs(r'(t)),t,a,b)"),T_A},
  {SP("Limits: the times at the endpoints"),0,T_A},
  {SP("So"),0,T_A},
  {SP("(1) s in terms of t"),SP("s(t)=integrate(abs(r'(T)),T,0,t)"),T_A},
  {SP("(2) t in terms of s"),0,T_A},
  {SP("(3) Replace t with t(s)"),0,T_A},
  {SP("|r'(t)| = 1: an arclength parameterization"),0,T_A},
  {SP("|r'(t)| is not 1: not arclength; find one"),0,T_A},
  {SP("Integrate each component, a constant each"),0,T_A},
  {SP("Use the initial condition"),0,T_A},
  {SP("Put the curve into the equation"),0,T_A},
  {SP("The points"),0,T_A},
  {SP("Component by component"),0,T_A},
  {SP("True for every t: the curve lies on it"),0,T_A},
  {SP("No solution: they do not meet"),0,T_A},
  {SP("Which part: the values each coordinate takes"),0,T_A},
  {SP("Not a limit: the surface has no points past it"),0,T_A},
  {SP("Only this part of it is traced"),0,T_A},
  {SP("Set one variable equal to t"),0,T_A},
  {SP("Put it in the equation"),0,T_A},
  {SP("Solve for the other"),0,T_A},
  {SP("Divide to get 1 on the right"),0,T_A},
  {SP("x = a cos t, y = b sin t: the whole ellipse"),0,T_A},
  {SP("For every t: the constant from one t, then checked"),0,T_A},
  {SP("Not true for every t: no such constant"),0,T_A},
  {SP("Where the constant drops out"),0,T_A},
  {SP("Where each part is defined"),0,T_A},
  {SP("Solve those together"),0,T_A},
  {SP("Take out where a denominator is 0"),0,T_A},
  {SP("The two sides, component by component"),0,T_A},
  {SP("Solve"),0,T_A},
  {SP("Pick values: an example"),0,T_A},
  {SP(""),0,T_A},
#endif
};
static_assert(sizeof(RULES)/sizeof(RULES[0])==R_NRULES,"RULES[] and the R_* enum must stay aligned");
// (DeltaCE) an inequality's relation: 0 an equation, 1 <, 2 <=, 3 >, 4 >=
static const unary_function_ptr * relop(int r){ return r==1?at_inferieur_strict:r==2?at_inferieur_egal:r==3?at_superieur_strict:r==4?at_superieur_egal:at_equal; }
static int rel_of(const gen & e){ for (int r=1;r<=4;++r) if (e.is_symb_of_sommet(relop(r))) return r; return 0; }
static int flip(int r){ return r?(r+1)%4+1:0; } // (< and >, <= and >=)
static const char * const RELT[5]={"=","<","<=",">",">="};

const char * steps_title(const step_t & s){ static char t[ST_MAX+1]; return st_unpack(RULES[s.rule].title,t); } // (till the next call: the steps page copies it)
int steps_method,steps_methods; // (DeltaCE, steps.h)
const char * steps_method_name="";
bool steps_answer(const std::vector<step_t> & v,gen & r){ // (DeltaCE)
  if (v.empty() || v.back().rule!=R_DANS) return false;
  r=v.back().a;
  return true;
}
const char * steps_formula(const step_t & s){ static char f[ST_MAX+1]; return st_unpack(RULES[s.rule].formula,f); } // (0: none)
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
// The steps' templates parsed (~30 ms each on the calculator; most steps share a few: diff(A,V)=B),
// the last NTPL kept; given back when the heap runs out (focus_menu_release, allocator_custom.c)
enum { NTPL = 12 };
static const char * tpl_s[NTPL];
static gen * tpl_g; // (heap: no static constructors in KhiCAS)
static unsigned char tpl_n;
static gen tpl_of(const char * s,GIAC_CONTEXT){
  if (tpl_g)
    for (int k=0;k<NTPL;++k)
      if (tpl_s[k] && (tpl_s[k]==s || !strcmp(tpl_s[k],s))) return tpl_g[k];
  const gen g(s,contextptr);
  if (g.type!=_SYMB || interrupted || ctrl_c) return g; // (ON during the parse: not kept)
  if (!tpl_g && !(tpl_g=new gen[NTPL])) return g;
  const int k=tpl_n;
  tpl_n=(unsigned char)(k+1==NTPL?0:k+1);
  tpl_s[k]=s; tpl_g[k]=g;
  return g;
}
int steps_release(){
  if (!tpl_g) return 0;
  delete[] tpl_g; tpl_g=0;
  for (int k=0;k<NTPL;++k) tpl_s[k]=0;
  return 1;
}
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
  const gen tpl=tpl_of(TPLS[RULES[s.rule].tpl],contextptr);
#ifdef TICE
  focus_phase=136;
#endif
  // the values of the template's own names only (subst is simultaneous: the others did nothing, and
  // dA printed the step's whole expression, ~40 ms a step on the calculator with the rest)
  const char * T=TPLS[RULES[s.rule].tpl];
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
static NOINL void S(int r,const gen & a){ S(r,a,zero); } // (a step with one part)
static NOINL size_t mark(){ return out_->size(); }
static NOINL void cut(size_t m){ out_->resize(m); }
static NOINL gen ev(const gen & g){ return eval(g,1,CTX); }
static NOINL gen evn(const gen & g){ return ev(-g); }
static NOINL gen evm(const gen & a,const gen & b){ return ev(a*b); } // (ev of a product, a sum, a difference, a quotient: one call each for its many sites)
static NOINL gen eva(const gen & a,const gen & b){ return ev(a+b); }
static NOINL gen evs(const gen & a,const gen & b){ return ev(a-b); }
static NOINL gen evq(const gen & a,const gen & b){ return ev(a/b); }
static NOINL gen degx(const gen & g){ return _degree(makesequence(g,X),CTX); }
static NOINL gen ratn(const gen & g){ return ratnormal(g,CTX); }
static NOINL gen nrm(const gen & g){ return normal(g,CTX); }
static NOINL bool zn(const gen & g){ return is_zero(nrm(g)); } // (normal says it is 0: sqrt(2)^2-2)
static NOINL bool zr(const gen & g){ return is_zero(ratn(g)); }
static NOINL bool spos(const gen & g){ return is_strictly_positive(g,CTX); }
static NOINL bool npos(const gen & g){ return is_positive(g,CTX); } // (not negative: 0 too)
static NOINL gen dnm(const gen & g){ return _denom(g,CTX); }
static NOINL gen nmr(const gen & g){ return _numer(g,CTX); }
static NOINL gen ab(const gen & g){ return abs(g,CTX); }
static NOINL gen evf(const gen & g){ return evalf(g,1,CTX); }
static NOINL gen sub(const gen & g,const gen & a,const gen & b){ return subst(g,a,b,false,CTX); }
static NOINL gen evx(const gen & g){ return ev(expand(g,CTX)); } // (ev(expand(g)): one call for its many sites)
static NOINL gen ratq(const gen & a,const gen & b){ return ratn(a/b); } // (ratnormal of a quotient, a difference; evx of a product, a difference)
static NOINL gen rats(const gen & a,const gen & b){ return ratn(a-b); }
static NOINL gen evxm(const gen & a,const gen & b){ return evx(a*b); }
static NOINL gen evxs(const gen & a,const gen & b){ return evx(a-b); }
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
// e's powers as powers of one name for ratnormal: e*e^2*e^3 is e^6, (2e^2-2)/(e^3-e^2) is (2e+2)/e^2
// (giac's ratnormal keeps each exp(k) a name of its own: s_2 of sum(2/e^k-2/e^(k+2)) was
// (-2e e^2 e^3-...)/(e e^2 e^3 e^4)); only with two or more, each of a number
static int ne_,no_; // (ne_ -1: an exp of no number; 1 one power seen, 2 two different ones; no_ how many)
static gen * e1_;
static NOINL gen map_of(const gen & g,gen (*f)(const gen &)); // (below)
static NOINL gen epw(const gen & g){
  if (g.type!=_SYMB || ne_<0) return g;
  if (g.is_symb_of_sommet(at_exp)){
    const gen & a=g._SYMBptr->feuille;
    if (a.type!=_INT_ && a.type!=_FRAC){ ne_=-1; return g; }
    ++no_;
    if (!ne_){ *e1_=a; ne_=1; } else if (a!=*e1_) ne_=2;
    return symb_pow(gen(identificateur("e_")),a);
  }
  return map_of(g,epw);
}
gen steps_epow(const gen & g,GIAC_CONTEXT){
  if (g.type!=_SYMB || !has_op(g,*at_exp)) return g;
  gen e1; e1_=&e1; ne_=0; no_=0;
  gen h=epw(g);
  if (ne_<1 || no_<2) return g;
  h=ratnormal(h,contextptr);
  if (!has_op(_denom(h,contextptr),*at_plus)) h=expand(h,contextptr); // (over a power of e: its terms, 2/e-2/e^3 as the course's a_1, not 2(e^2-1)/e^3)
  const gen r=eval(subst(h,gen(identificateur("e_")),symb_exp(1),false,contextptr),1,contextptr);
  return ne_==2 || r.print(contextptr).size()<=g.print(contextptr).size()?r:g; // (one power twice: when not longer, e/e is 1, e cos(e)+sin(e) kept as it was)
}
static NOINL gen rn(const gen & g){ return bigp(g)?g:steps_epow(ratnormal(g,CTX),CTX); }
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
  if (nrad(r)==1 && !bigp(r)){ gen s=nrm(r); if (taille(s,200)<taille(r,200)) r=s; }
  return r;
}
static NOINL gen D(const gen & g,const gen & x){ return ev(derive(g,x,CTX)); }
static NOINL bool dep(const gen & g,const gen & x){
  return g.type==_IDNT ? g==x : (g.type==_SYMB || g.type==_VECT) && depend(g,*x._IDNTptr);
}
static bool dep(const gen & g){ return dep(g,X); }
static NOINL gen SYM(const unary_function_ptr * u,const gen & a){ return symbolic(*u,a); }
static NOINL gen SYM2(const unary_function_ptr * u,const gen & a,const gen & b){ return symbolic(*u,makesequence(a,b)); }
static NOINL gen SYMV(const unary_function_ptr * u,const vecteur & v){ return symbolic(*u,gen(v,_SEQ__VECT)); } // (a sum or a product of the list: x+y+z)
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
static gen prodof(const vecteur & v){ return v.empty()?plus_one:v.size()==1?v[0]:symb_prod(v); }
// a polynomial (names, sums, products, positive integer powers: x, x^2+1), written before the
// other factors of a product (mergek); rat: its numbers are whole or fractions (2x+y/3, not 0.5x,
// sqrt(2)x or pi x: pi is a number, not a name)
static NOINL bool polyf(const gen & g,bool rat=false){
  if (rat){
    if (g.type==_INT_ || g.type==_ZINT || g.type==_FRAC) return true;
    if (g.type==_IDNT) return evf(g).type!=_DOUBLE_;
    if (g.is_symb_of_sommet(at_inv)) return arg(g).type==_INT_ || arg(g).type==_ZINT; // (1/4 written as a quotient)
  }
  else if (g.type<=_IDNT) return true;
  if (g.type!=_SYMB) return false;
  if (g.is_pow()) return arg(g,1).type==_INT_ && arg(g,1).val>0 && polyf(arg(g,0),rat);
  if (!g.is_plus() && !g.is_prod() && !g.is_neg()) return false;
  if (arg(g).type!=_VECT) return polyf(arg(g),rat);
  EACH(it,*arg(g)._VECTptr) if (!polyf(*it,rat)) return false;
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
    else if (isnum(e) && spos(-e)) den.push_back(is_minus_one(e)?b:symb_pow(b,-e));
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
    *it=c==is(b2,at_sec)?plus_one:SYM(c?at_cot:at_tan,arg(*it));
    *jt=e.val==1?plus_one:e.val==2?b2:symb_pow(b2,e-1);
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
    return a.is_pow() && arg(a,0).type==_IDNT?symb_pow(arg(a,0),evq(arg(a,1),plus_two)):symb_pow(a,plus_one_half);
  return SYM(&u,a);
}

// a sum's terms as typed, a binary minus's second negated (not norm's x*1/2 for x/2)
static NOINL void sterms(const gen & g,vecteur & v){
  if (g.is_plus() && arg(g).type==_VECT){ EACH(it,*arg(g)._VECTptr) sterms(*it,v); return; }
  if (is(g,at_binary_minus) && arg(g).type==_VECT && arg(g)._VECTptr->size()==2){ sterms(arg(g,0),v); v.push_back(symb_neg(arg(g,1))); return; }
  v.push_back(g);
}
// a = f numerically (giac's simplify is slow on the calculator), the other variables given values.
// Points on both sides of 0 and far out, first inside [lo,hi] when given (only there: only): every point where f is
// real must agree, 5 of them at most, at least 3 (else complex values agreeing make up the 3; a
// complex one differing can be a branch of sqrt, not an error).
static NOINL bool same_num(const gen & a0,const gen & f0,const gen & x,const gen * lo=0,const gen * hi=0,bool only=false){
  gen d=a0-f0,f=f0;
  vecteur ids=lidnt(makevecteur(d,f));
  double c=0.71;
  EACH(it,ids){ if (*it!=x && evf(*it).type!=_DOUBLE_){ d=sub(d,*it,c); f=sub(f,*it,c); } c+=0.17; } // (pi keeps its value)
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
    gen z=evf(sub(f,x,p)),y;
    if (z.type!=_DOUBLE_ && z.type!=_CPLX) continue; // f not defined there
    if (z.type==_DOUBLE_ && !(z._DOUBLE_val-z._DOUBLE_val==0)) continue; // or too big ((2x+1)^2026 at 0.37)
    y=evf(sub(d,x,p));
    if (y.type!=_DOUBLE_ && y.type!=_CPLX) continue;
    double ay=evalf_double(ab(y),1,CTX)._DOUBLE_val,az=evalf_double(ab(z),1,CTX)._DOUBLE_val;
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
  gen r=evm(outer,d);
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
  if (u==at_cot) return symb_neg(symb_pow(SYM(at_csc,t),plus_two));
  if (u==at_tan) return symb_pow(SYM(at_sec,t),plus_two);
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
  if (u==at_neg) return evn(dsteps(a));
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
      r=evm(k,r);
      S(R_EQD,f,r);
      return r;
    }
    if (!den.empty()){ // u/v
      gen n=prodof(num),d=prodof(den);
      S(R_QUOT,f,n,d);
      ++depth_; gen dn=dsteps(n),dd=dsteps(d); --depth_;
      gen r=(dn*d-n*dd)/symb_pow(d,plus_two);
      S(R_EQD,f,r);
      gen r2=rn(r); // (not over a root: giac's ratnormal left (-x^2+x^2+1)/(x^2+1)^(3/2))
      if (taille(r2,200)<taille(r,200) && is_zero(radicand(d))){ S(R_EQD,f,r2); r=r2; }
      return r;
    }
    return dprod(num);
  }
  if (u==at_inv){ // 1/g
    if (a==X || (a.is_pow() && arg(a,0)==X)){
      gen n=a==X?gen(-1):gen(evn(arg(a,1))),r=ev(n*pow(X,n-1,CTX));
      S(R_POWD,f,r); return r;
    }
    return chain(f,-symb_inv(symb_pow(a,plus_two)),a);
  }
  if (u==at_pow && a.type==_VECT){
    gen b=a[0],e=a[1];
    if (!dep(e)){
      gen o=ev(e*pow(b,e-1,CTX));
      if (b==X){ S(R_POWD,f,o); return o; }
      return chain(f,o,b);
    }
    if (!dep(b)){
      if (e==X){ gen r=evm(f,ln(b,CTX)); S(R_EXPD,f,r); return r; }
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
    gen y=evf(sub(g,X,*lo_+(*hi_-*lo_)*((i+0.5)/n)));
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
  while (w.is_pow()){ e=evm(e,arg(w,1)); w=arg(w,0); } // (giac's x^(2/5) is (x^(1/5))^2)
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
  return subst(gen(s,CTX),from,to,false,CTX); // (lists: giac's own overload, as before)
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
    gen v=evf(sub(f,X,TABX[k]));
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
  bool pa=npos(A),pc=npos(C);
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
  return dep(a)?a:zero;
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
  if (ok){ F=evq(sub(H,t,w),a); r=R_TABLE; }
  return ok;
}
static bool basic(const gen & f,gen & F,int & r){
  if (f==X){ F=symb_pow(X,plus_two)/2; r=R_POWI; return true; }
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
  gen h=evq(fe,du),kt=t,k=kern(u,kt),g;
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
  return !c.empty() && spos(-ev(prodof(c)))?rn(-g):undef;
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
  if (!shape(fe,SH_FRAC) || !dep(dnm(fe))) return undef;
  gen p=pfrac(fe);
  if (is_undef(p) || p==fe || !p.is_plus()) return undef;
  vecteur t; flat(at_plus,p,t); // a polynomial and one fraction (x^2/(x^2+1)): long division
  int k=0; EACH(it,t) if (dep(dnm(*it))) ++k;
  return via(k>1?R_PFRAC:R_LDIVQ,fe,p,lev);
}
// (a x+b)/q^n, q = A x^2+B x+C: a/(2A) q'/q^n plus the rest (a logarithm and an arctangent)
static NOINL gen splitnum(const gen & fe,int lev){
  gen n=nmr(fe),d=dnm(fe),a,b,q=d,A,B,C;
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
  gen fe=sc(ev(f)),d=dnm(fe);
  if (!shape(fe,SH_TRIG|SH_FRAC) || !dep(d) || nsub_>3) return undef; // (sin and cos of x: else t stays)
  gen w(identificateur(" w"));
  for (int k=0;k<2;++k){ // d = p + q s, p^2 = q^2: times (p - q s), 1 - s^2 is the other one squared
    gen s=k?symb_cos(X):symb_sin(X),p,q;
    if (!is_linear_wrt(sub(d,s,w),w,q,p,CTX) || dep(p) || dep(q) || is_zero(q) || !is_zero(ev(p*p-q*q))) continue;
    return via(R_CONJ,fe,fill("expand(A*(B-C*Q))/(B^2*W^2)",makevecteur(nmr(fe),p,q,0,s,k?symb_sin(X):symb_cos(X))),lev);
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
    if (e.type==_INT_ || (e.type==_FRAC && e._FRACptr->den==2)) return pow(root,evm(2,e),CTX);
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
  bool pa=npos(A),pc=npos(C);
  int k=pc?(pa?1:0):(pa?2:-1);
  if (k<0 || (!r && k!=1)) return undef; // (a^2-x^2)^-n: partial fractions
  gen t(identificateur("t"));
  gen W=fill(TSUB[k],makevecteur(sqrt(ev(k==1?C/A:-C/A),CTX),A,C,0,q,0,t));
  if (W.type!=_VECT || W._VECTptr->size()<5) return undef;
  const vecteur & w=*W._VECTptr;
  gen dx=D(w[0],t),g=sc(evm(sub(radsub(fe,q,w[1]),X,w[0]),dx)),g2=_trigcos(g,CTX);
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
  EACH(it,v){ gen b=*it,e=bexp(b); if (it->is_plus()) n=b; else if (npos(-e)){ d=b; ++nd; } }
  if (!n.is_plus() || !dep(d) || !dep(rn(n/D(d,X)))) return false;
  return !rat || (nd==1 && is_quadratic_wrt(d,X,A,B,C,CTX) && !is_zero(A) && spos(ev(4*A*C-B*B)));
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
    S(R_CONSTI,f,minus_one,arg(f));
    F=isteps(arg(f),lev);
    if (is_undef(F)){ cut(m); return F; }
    F=evn(F); S(R_EQI,f,F);
    return F;
  }
  vecteur c,v; factors(f,c,v);
  gen k=ev(prodof(c));
  int r;
  if (!c.empty() && !v.empty()){
    gen g=prodof(v),G;
    if (is_minus_one(k)){ F=isteps(g,lev); return is_undef(F)?F:evn(F); } // -g: as -(...), no step
    if (!is_one(k)){ // c times the integral of g
      if (basic(ev(g),G,r)){ F=evm(k,G); S(r,f,F); return F; } // 3x^2: one step
      size_t m=mark();
      S(R_CONSTI,f,k,g);
      G=isteps(g,lev);
      if (is_undef(G)){ cut(m); return G; }
      F=evm(k,G);
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
    gen d=big || upow(fe,0)?gen(99):degx(fe);
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
  return !is_zero(r)?r:calc_op(g)?g:zero;
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
    if (out!=1) return ev(radsub(f,q,out*sqrt(evm(prodof(c),in),CTX)));
  }
  return f;
}
static gen one(const gen & op);
// the pieces' total is giac's integral (else a zero of g was missed: no steps rather than wrong ones)
static NOINL bool absame(const gen & tot,const gen & op){
  const gen d=evf(tot-ev(op)),r=evf(tot);
  return d.type==_DOUBLE_ && r.type==_DOUBLE_ && fabs(d._DOUBLE_val)<=TOL*(1+fabs(r._DOUBLE_val));
}
// integrate(f,x,a,b) with |g| in f: one integral per interval where g keeps its sign. |g| alone
// (area between curves): g simplified, one antiderivative, each interval evaluated with it.
static NOINL gen absplit(const gen & op,const gen & g0,const gen & a,const gen & b){
  gen f=arg(op,0),g=rn(g0);
  if (taille(g,200)>=taille(g0,200)) g=g0;
  bool alone=f==symb_abs(g0);
  gen z=_solve(makesequence(symb_equal(g,zero),X),CTX);
  vecteur pts(1,a);
  // giac's zeros, and for a periodic g the same ones a multiple of pi away where g vanishes too
  // (giac's solve gives cos(t)=0 at pi/2 only; on [0,3pi] the integral took one piece for five)
  const bool per=!is_inf(a) && !is_inf(b) && (has_op(g,*at_sin) || has_op(g,*at_cos) || has_op(g,*at_tan));
  if (z.type==_VECT)
    EACH(it,*z._VECTptr){
      const gen zi=ev(*it);
      if (evf(zi).type!=_DOUBLE_) continue;
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
    bool pos=npos(evf(sub(g,X,(pts[i]+pts[i+1])/2)));
    sg.push_back(pos?1:-1);
    gen I=symbolic(at_integrate,makesequence(alone?g:sub(f,symb_abs(g0),pos?g0:-g0),X,pts[i],pts[i+1]));
    pieces.push_back(alone && !pos?symb_neg(I):I);
  }
  S(pieces.size()>1?R_ABS:R_ABSI,op,symb_plus(pieces)); // (one piece: the sign on the interval)
  gen tot=0;
  if (alone){
    gen I=SYM2(at_integrate,g,X);
    S(R_PART,I);
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
    S(R_PART,*it);
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
static bool lgratio_; // (lgrow decided by the ratio of consecutive terms, not the growth rates)
static bool nogiac_; // (growlim: the classes alone, no giac limit: ln(k+1)-ln(k+3) took 18 s on the calculator)
static bool trust_; // (a sequence's limit decided by lgrow at the top: by the growth classes, exactly; no giac limit to check it against)
static NOINL gen Lx(const gen & f){ return SYM(at_limit,ldir_?makesequence(f,X,*la_,ldir_):makesequence(f,X,*la_)); }
static NOINL gen glim(const gen & g){ return bigp(g,true)?gen(undef):ev(Lx(g)); }
static NOINL bool finv(const gen & v){ return v.type!=_STRNG && !has_inf_or_undef(v) && !has_op(v,*at_bounded_function); }
static NOINL gen Ls(int r,const gen & f,const gen & v0){ gen v=v0.type==_FRAC?v0._FRACptr->num/v0._FRACptr->den:v0; S(r,Lx(f),v); return v; } // lim f = v by rule r (giac's limit gave the fraction 1/-7)
static gen lsteps(const gen & f,int lev);
static gen lq(const gen & f,const gen & n,const gen & d,int lev);
static NOINL std::string tplus(const vecteur & w); // (below)
static NOINL std::string wsum(const vecteur & w,bool dots); // (below)
static NOINL gen growlim(const gen & g); // (below)
static NOINL gen line(const char * f,const gen & a,const gen & b,const gen & c,const gen & d,const gen & e,const gen & g); // (below)
static NOINL gen line3(const char * f,const gen & a,const gen & b=zero,const gen & c=zero){ return line(f,a,b,c,zero,zero,zero); }
static NOINL gen lser(const gen & f,const gen & n,const gen & d,int lev); // (below)
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
static NOINL bool gcls(const gen & t,double * c); // (below)
static NOINL gen fastest(const gen & g){
  vecteur w; flat(at_plus,g,w);
  if (*la_==plus_inf && w.size()<=6){ // (the growth classes first: n^50 << 2^n; giac's limit of n^50/2^n took 2 minutes on the calculator)
    double cs[30]; bool all=true;
    for (size_t i=0;i<w.size() && all;++i) all=gcls(w[i],cs+5*i);
    if (all) for (size_t i=0;i<w.size();++i){
      bool ok=dep(w[i]);
      for (size_t j=0;j<w.size() && ok;++j) if (j!=i){ int q=0; while (q<5 && cs[5*i+q]==cs[5*j+q]) ++q; if (q<5 && cs[5*i+q]<cs[5*j+q]) ok=false; }
      if (ok) return w[i];
    }
  }
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
  return r && w.size()>1?symb_plus(c):zero;
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
    EACH(jt,w){ gen b=*jt,e=bexp(b); if (b==X && spos(-e)) k=-e; else if (e==gen(1)/2) q=b; }
    if (!is_zero(q) && !is_zero(k)) *it=symb_prod(prodof(c),symb_pow(expand(ev(q/pow(X,2*k,CTX)),CTX),gen(1)/2));
  }
  return t.size()==1?t[0]:symb_plus(t);
}
static NOINL gen fastest(const gen & g);
static NOINL gen ratio_of(const gen & f,const gen & t,const gen & x); // (below)
static NOINL bool close(const gen & a,const gen & b);
static NOINL std::string pr(const gen & g); // (below)
static NOINL gen txt(const std::string & s); // (below)
static NOINL std::string par(const gen & g); // (below)
// a term's growth class at +infinity: (n^n, n!, ln b of b^n, q of n^q, p of ln(n)^p), compared in
// that order (the course's ln(n)^p << n^q << b^n << n! << n^n); false for any other factor
static NOINL gen dom(const gen & g);
static NOINL bool lpoly(const gen & u){ // a polynomial in the index of degree 1 or more
  const vecteur lv=lvar(u);
  if (lv.size()!=1 || lv[0]!=X) return false;
  const gen d=_degree(makesequence(u,X),CTX);
  return d.type==_INT_ && d.val>=1 && !dep(_denom(u,CTX));
}
static NOINL bool gcls(const gen & t,double * c){
  for (int i=0;i<5;++i) c[i]=0;
  gen al0,be0;
  vecteur w; flat(at_prod,t,w);
  EACH(it,w){
    if (!dep(*it)) continue;
    gen b=*it,e=bexp(b);
    if (dep(e)){ // b^n, n^n
      gen al,be;
      if (!is_linear_wrt(e,X,al,be,CTX) || !is_strictly_positive(al,CTX)) return false;
      const gen eb=evalf(b,1,CTX),ea=evalf(al,1,CTX);
      if (b==X){ c[0]+=ea._DOUBLE_val; continue; }
      if (dep(b) || eb.type!=_DOUBLE_ || eb._DOUBLE_val<=0) return false;
      c[2]+=ea._DOUBLE_val*std::log(eb._DOUBLE_val); continue;
    }
    const gen ee=evalf(e,1,CTX);
    if (ee.type!=_DOUBLE_) return false;
    if (b==X) c[3]+=ee._DOUBLE_val;
    else if (is(b,at_factorial) && arg(b)==X) c[1]+=ee._DOUBLE_val;
    else if (is(b,at_ln) && (arg(b)==X || is_linear_wrt(arg(b),X,al0,be0,CTX) || lpoly(arg(b)))) c[4]+=ee._DOUBLE_val; // (ln(k+3) as ln(k); ln(k^2+3) as 2 ln(k), the same class)
    else if (is(b,at_exp)){ gen al,be; if (!is_linear_wrt(arg(b),X,al,be,CTX)) return false; c[2]+=evalf(al,1,CTX)._DOUBLE_val*ee._DOUBLE_val; }
    else if (b.is_plus() || b.is_prod()){ // (a root of a sum: sqrt(4n^2+n) grows as n)
      const gen d=b.is_plus()?dom(b):b;
      double c2[5];
      if (d==b && b.is_plus()) return false;
      if (!gcls(d,c2)) return false;
      for (int i=0;i<5;++i) c[i]+=c2[i]*ee._DOUBLE_val;
    }
    else return false;
  }
  return true;
}
// the term of the sum g that grows fastest: by the classes when every term has one (ln(n)^74: giac's
// limits refuse its power), else by giac's limits (fastest)
static NOINL gen gfast(const gen & g){
  vecteur w; flat(at_plus,g,w);
  double best[5],c[5];
  int k=-1;
  bool tie=false;
  for (unsigned i=0;i<w.size();++i){
    if (!gcls(w[i],c)){ // (giac's limits: the others over it all go to 0)
      if (nogiac_) return 0;
      const gen t=fastest(g);
      if (is_zero(t)) return t;
      EACH(it,w) if (*it!=t && !is_zero(glim(ev(*it/t)))) return 0;
      return t;
    }
    int j=0;
    while (k>=0 && j<5 && c[j]==best[j]) ++j;
    if (k>=0 && j==5) tie=true; // (n^2 and -n^2: they may cancel)
    if (k<0 || (j<5 && c[j]>best[j])){ k=i; tie=false; for (j=0;j<5;++j) best[j]=c[j]; }
  }
  return k<0 || tie?gen(0):w[k];
}
// the dominant part of g at +infinity: a sum's fastest term, a product's or a power's of its parts',
// ln of one: q ln(n) for n^q inside (ln(n^2+1): 2 ln(n))
static bool dkeep_; // (dom for the screen: a root over its radicand's dominant term, sqrt(5n^2), the course's line before sqrt(5) n)
static NOINL gen dom(const gen & g){
  if (g.is_plus()){
    const gen t=gfast(g);
    if (!is_zero(t)) return dom(t);
    vecteur w,c; flat(at_plus,g,w); // (a tie: the terms of the top class, their dominant parts added)
    double b[5],q[5];
    int k=-1;
    EACH(it,w){ if (!gcls(*it,q)) return g; int j=0; while (k>=0 && j<5 && q[j]==b[j]) ++j; if (k<0 || (j<5 && q[j]>b[j])){ k=1; c.clear(); for (j=0;j<5;++j) b[j]=q[j]; } if (j==5 || c.empty()) c.push_back(*it); }
    if (c.size()<2) return g;
    vecteur d; EACH(it,c) d.push_back(dom(*it));
    const gen r=ev(symb_plus(d));
    return is_zero(r)?g:r;
  }
  if (is(g,at_ln)){
    const gen d=ev(dom(arg(g)));
    vecteur w; flat(at_prod,d,w);
    gen q=0;
    EACH(it,w){ gen b=*it,e=bexp(b); if (b==X && !dep(e)) q=q+e; else if (dep(*it)) return g; }
    return is_strictly_positive(q,CTX)?(is_one(q)?symb_ln(X):symb_prod(ev(q),symb_ln(X))):g;
  }
  if (g.is_prod() && arg(g).type==_VECT){ vecteur w=*arg(g)._VECTptr; EACHW(it,w) *it=dom(*it); return symbolic(at_prod,gen(w,_SEQ__VECT)); }
  if (g.is_inv()) return symb_inv(dom(arg(g)));
  if (g.is_pow() && !dep(arg(g,1))){ // (the power of each factor: sqrt(4n^2) is 2n, n > 0 at infinity)
    const gen e=arg(g,1),d=dom(arg(g,0));
    if ((d==arg(g,0) && !d.is_prod()) || (dkeep_ && (e.type==_FRAC || e.type==_INT_))) return symb_pow(d,e); // (dkeep_: (2n)^2 and sqrt(5n^2) as the course writes them first)
    vecteur w; flat(at_prod,ev(d),w);
    EACHW(it,w){ gen b=*it; const gen q=bexp(b); *it=dep(b)?(is_one(ev(q*e))?b:symb_pow(b,ev(q*e))):ev(pow(*it,e,CTX)); }
    return prodof(w);
  }
  return g;
}
// b^(m n) for a constant b (e^(m n), 1/b^n too): its b^m, else 0
static NOINL gen gbase(const gen & g){
  gen al,be;
  if (g.is_inv()){ const gen b=gbase(arg(g)); return is_zero(b)?b:ev(inv(b,CTX)); }
  if (is(g,at_exp)) return is_linear_wrt(arg(g),X,al,be,CTX) && is_zero(be)?exp(al,CTX):gen(0);
  if (g.is_pow() && !dep(arg(g,0)) && is_linear_wrt(arg(g,1),X,al,be,CTX) && is_zero(be) && !is_zero(al)) return ev(pow(arg(g,0),al,CTX));
  return 0;
}
// a quotient n/d of a sequence at +infinity the course's way (an index n or k, or n! in it): c r^n
// (3*5^n/6^n: |5/6| < 1), the dominant terms and their growth rates ((2^n+n!)/(2n^9+3n^n): n!/(3n^n),
// n! << n^n), n!'s by the ratio of consecutive terms (4(n!)^2/(2n)!: 1/4); undef if none of these
// a product as one fraction for the screen: (2n+1)/(3n), not norm's (2n+1)*1/3/n
static NOINL gen qshow(const gen & f){
  if (is(f,at_abs)) return SYM(at_abs,qshow(arg(f))); // (|(n+1)^2/(3n^2)|, not |(n+1)^2*1/3/n^2|)
  vecteur w,nu,de; flat(at_prod,f,w);
  bool q=false;
  EACH(it,w){
    if (it->type==_FRAC){ q=true; if (!is_one(it->_FRACptr->num)) nu.push_back(it->_FRACptr->num); de.push_back(it->_FRACptr->den); } // (1/3: 3 below)
    else (it->is_inv()?de:nu).push_back(it->is_inv()?arg(*it):*it);
  }
  return de.size()<2 && !q?f:symb_prod(nu.empty()?gen(1):prodof(nu),symb_inv(prodof(de)));
}
static NOINL gen pmerge(const gen & f,const gen & y); // (below)
// a product of powers b^(a n+c) with one |a| (n^n/(n+1)^(n+1) times n+1): the rest times U^(a n), U = n/(n+1);
// f itself if not that shape
static NOINL gen pcomb(const gen & f){
  vecteur w,cs,us; flat(at_prod,f,w);
  gen a0=0;
  EACH(it,w){
    gen b=*it,al,be; const gen e=bexp(b);
    if (!dep(e) || !is_linear_wrt(e,X,al,be,CTX) || is_zero(al)){ cs.push_back(*it); continue; }
    const gen aa=ev(abs(al,CTX));
    if (is_zero(a0)) a0=aa;
    if (aa!=a0) return f;
    us.push_back(is_strictly_positive(al,CTX)?b:symb_inv(b));
    if (!is_zero(be)) cs.push_back(symb_pow(b,be));
  }
  if (us.size()<2) return f;
  const gen C=normal(ev(prodof(cs)),CTX),U=qshow(prodof(us)),P=symb_pow(U,is_one(a0)?X:ev(a0*X)); // ((n+1)(n+1)^-1: 1)
  return is_one(C)?P:symb_prod(C,P);
}
static NOINL bool kroot(const gen & g){ // a root or a power of a product in g (sqrt(5n^2), (2n)^2)
  if (g.type!=_SYMB) return false;
  if ((g.is_pow() && (arg(g,0).is_prod() || arg(g,1).type==_FRAC)) || is(g,at_sqrt)) return true;
  const gen & a=arg(g);
  if (a.type==_VECT){ EACH(it,*a._VECTptr) if (kroot(*it)) return true; return false; }
  return kroot(a);
}
static NOINL void ldom(const gen & f,const gen & n,const gen & d,const gen & q){
  vecteur w,nu,de; flat(at_prod,norm(f),w); // (each typed factor's dominant term: (2n)(3n)/(2n)^2, the course's, before giac's 6n^2/(4n^2))
  EACH(it,w) (it->is_inv()?de:nu).push_back(it->is_inv()?arg(*it):*it);
  dkeep_=true;
  gen qk;
  if (w.size()>1 || !de.empty()){ // (the numbers as one coefficient, the rest one fraction: n^2/(2 n 2n), not n^2*1/2/(n*2*n))
    vecteur a,b; gen c=1;
    EACH(it,nu){ if (dep(*it)) a.push_back(dom(*it)); else c=c*(*it); }
    EACH(it,de){ if (dep(*it)) b.push_back(dom(*it)); else c=c/(*it); }
    c=ev(c);
    const gen cn=c.type==_FRAC?c._FRACptr->num:c,cd=c.type==_FRAC?c._FRACptr->den:gen(1);
    gen top=prodof(a),bot=prodof(b);
    if (is_minus_one(cn)) top=symb_neg(top); else if (!is_one(cn)) top=symb_prod(cn,top);
    if (!is_one(cd)) bot=b.empty()?cd:symb_prod(cd,bot);
    qk=b.empty() && is_one(cd)?top:SYM2(at_division,top,bot);
  }
  else qk=quo(dom(n),dom(d));
  dkeep_=false;
  // (shown only when it says more: a power or root of a product, (2n)^2, sqrt(5n^2), or two factors with n, (2n)(3n);
  // not (k!)/4^k for k!/4^k, the same on the screen)
  bool more=kroot(qk);
  { vecteur t; flat(at_prod,norm(qk),t); int nd=0;
    EACH(it,t){ vecteur t2; flat(at_prod,it->is_inv()?arg(*it):*it,t2); EACH(jt,t2) if (dep(*jt)) ++nd; }
    if (nd>2) more=true; }
  if (!more || pr(qk)==pr(q)){ S(R_LGDOM,Lx(qshow(f)),Lx(q)); return; }
  S(R_LGDOM,Lx(qshow(f)),Lx(qk));
  S(R_EQV,Lx(qk),Lx(q)); // (sqrt(5n^2) = sqrt(5) n: one operation a line)
}
// b^e with b = r^m an integer power (4 = 2^2, 27 = 3^3) as r^(m e): pmerge then sees one base (4^n/(2^n)^2 is 1;
// lgrow left it and the limit went the long way: 2 minutes on the calculator for 5*4^n/(2^n+n^50)^2)
static NOINL gen rootbase(const gen & f){
  vecteur w,o; flat(at_prod,f,w); bool ch=false;
  EACH(it,w){
    gen b=*it; const gen e=bexp(b);
    bool done=false;
    if (b.type==_INT_ && b.val>3 && b.val<100000 && dep(e))
      for (int r=2;r*r<=b.val && !done;++r){ long p=r; int m=1; while (p<b.val){ p*=r; ++m; } if (p==b.val){ o.push_back(symb_pow(gen(r),ev(m*e))); ch=done=true; } }
    if (!done) o.push_back(*it);
  }
  return ch?prodof(o):f;
}
static NOINL int vs1(const gen & L); // (below, with lim_)
static NOINL gen lgrow(const gen & f,const gen & n,const gen & d){
  const char * vn=X.type==_IDNT?X._IDNTptr->id_name:"";
  const bool fac=has_op(f,*at_factorial);
  if (!fac && strcmp(vn,"n") && strcmp(vn,"k")) return undef;
  { const vecteur ids=lidnt(f); EACH(it,ids) if (*it!=X && evalf(*it,1,CTX).type!=_DOUBLE_) return undef; } // (a ratio with x in it: lq's)
  vecteur w,c; flat(at_prod,ev(f),w);
  gen r=1,k=1;
  bool geo=true;
  EACH(it,w){ const gen g=gbase(*it); if (!dep(*it)) k=k*(*it); else if (!is_zero(g)) r=r*g; else geo=false; }
  if (geo && !is_one(r=ev(r))){ // c r^n
    const gen ar=evalf(abs(r,CTX),1,CTX);
    if (ar.type!=_DOUBLE_ || !vs1(ar)) return undef;
    const gen v=ar._DOUBLE_val<1?gen(0):is_one(r)?ev(k):is_strictly_positive(k,CTX)?plus_inf:minus_inf;
    if (ar._DOUBLE_val>=1 && is_strictly_positive(-r,CTX)) return undef; // ((-2)^n: no limit, giac's)
    const gen kr=is_one(k)?symb_pow(r,X):symb_prod(k,symb_pow(r,X)); // (no 1*(3/2)^k)
    if (pr(ev(f))==pr(ev(kr))) S(ar._DOUBLE_val>1?R_LGEOI:R_LGEO,Lx(kr),v); // ((3/2)^n as typed: one line)
    else { S(ar._DOUBLE_val>1?R_LGEOI:R_LGEO,Lx(f),Lx(kr)); S(R_EQV,Lx(kr),v); }
    return v;
  }
  const gen N=dom(n),Dd=dom(d),q=quo(N,Dd);
  const bool dropped=N!=n || Dd!=d;
  double c1[5],c2[5];
  int j=0;
  const bool cl=gcls(N,c1) && gcls(Dd,c2); // (single terms of the classes: the growth rates say it, no giac limit: 15 s on the calculator)
  while (cl && j<5 && c1[j]==c2[j]) ++j;
  if (dropped || cl){ // the dominant terms' quotient: a number, or 0 or infinity by the growth rates
    gen r1=rn(ev(pmerge(norm(q),X))); // (4^n/4^(n+1): 1/4; giac left 4^(n-n-1), n in it)
    if (dep(r1)){ const gen r2=rn(ev(pmerge(rootbase(norm(q)),X))); if (!dep(r2)) r1=r2; } // (4^n/(2^n)^2: 1, one base; only then: the rewrite sent hw's ratio limit the long way, 0.6 s on the PC)
    if (!dep(r1) && (dropped || cl)){ if (dropped) ldom(f,n,d,q); S(R_EQV,Lx(q),r1); return r1; }
    if (cl && j<5){
      if (dropped){ // (3^k/2^k left: (3/2)^k with 3/2 > 1, the course's way, not the growth classes)
        const gen qe=ev(q); vecteur w2; flat(at_prod,qe,w2);
        bool g2=true;
        EACH(it,w2) if (dep(*it) && is_zero(gbase(*it))) g2=false;
        if (g2){ const size_t m=mark(); ldom(f,n,d,q); const gen v=lgrow(q,_numer(qe,CTX),_denom(qe,CTX)); if (!is_undef(v)) return v; cut(m); }
      }
      const gen ys=evalf(sub(r1,X,50),1,CTX); // (the sign of an infinite one)
      const gen L=c1[j]<c2[j]?gen(0):ys.type==_DOUBLE_ && ys._DOUBLE_val<0?minus_inf:plus_inf;
      if (dropped) ldom(f,n,d,q);
      const bool cs=dep(r1) && pr(r1).size()<pr(q).size(); // (k*4^k/(4*4^k) cancelled: k/4)
      if (cs) S(R_EQV,Lx(q),Lx(r1));
      S(R_LGROW,Lx(cs?r1:q),L);
      return L;
    }
    if (!fac) return undef;
  }
  if (!fac) return undef;
  // n!: the ratio of consecutive terms (no limit giac finds quickly: Stirling)
  lgratio_=true; // (not the growth rates: growlim says none, the series' own ratio test is the course's)
  if (nogiac_) return undef;
  const gen fe=ev(f),qr=_simplify(ev(sub(fe,X,X+1)/fe),CTX),Lq=glim(qr);
  const int cq=Lq==plus_inf?1:finv(Lq) && !is_strictly_positive(-Lq,CTX)?vs1(Lq):0;
  if (!cq) return undef;
  const bool up=cq>0;
  if (up && is_strictly_positive(-evalf(sub(fe,X,5),1,CTX),CTX)) return undef; // (positive terms only)
  S(R_SRAT,ratio_of(f,fe,X),qr);
  S(up?R_LRATU:R_LRAT,Lx(qr),Lq);
  return up?plus_inf:gen(0);
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
  if (id && has_op(ln,*at_bounded_function)) return Ls(R_LSQZ,f,zero); // sin(x)/x at infinity
  if (!(zn && zd) && !(in && id)) return undef;
  if (in && id && a==plus_inf && !is_undef(v=lgrow(f,n,d))) return v; // (a sequence: n or k, or n! in it)
  if (zn && zd && is_zero(a) && !is_undef(v=lser(f,n,d,lev))) return v; // (the course's Taylor series way, before L'Hopital)
  // 0/0, inf/inf: the fraction simplified (factor and cancel), defined at a (at infinity: shorter)
  gen fe=ev(f),h=rn(fe);
  gen dh=dnm(h);
  if (h!=fe && (is_inf(a)?shape(d,0) && shape(dh,0)?is_strictly_greater(degx(d),degx(dh),CTX):taille(h,200)<taille(fe,200):finv(ev(sub(h,X,a)))) && !is_undef(v=lvia(R_LALG,f,h,1,lev))) return v;
  // (at infinity a denominator of lower degree: (x^4-4x^3+1)/(2-2x^2-7x^4) only changed its signs)
  // a finite point: a square root in a sum, times its conjugate over itself
  gen c=is_inf(a)?zero:conjof(n);
  const bool top=!is_zero(c);
  if (!top && !is_inf(a)) c=conjof(d);
  if (!is_zero(c) && !is_undef(v=lvia(R_LCONJ,f,top?rn(expand(evm(n,c),CTX)):symb_prod(n,c),top?symb_prod(d,c):rn(expand(evm(d,c),CTX)),lev))) return v;
  // at infinity, numerator and denominator over the highest power of x below, x^p, p its growth
  // (x/sqrt(x^2+1): 1/sqrt(1+1/x^2); L'Hopital went around in circles)
  gen p=is_inf(a)?glim(rn(evq(symb_ln(ab(d)),symb_ln(ab(X))))):zero;
  if (finv(p) && spos(p)){
    gen t=pow(X,p,CTX),n2=rootin(expand(evq(n,t),CTX)),d2=rootin(expand(evq(d,t),CTX));
    // (over a denominator x^p itself: only when the numerator comes apart, (x+cos(x))/x; e^x/x stays)
    if ((dep(d2) || n2.is_plus()) && !is_undef(v=lvia(R_LDIV,f,n2,d2,lev))) return v; // (e^x/(2x): no)
  }
  else if (is_inf(a) && d.is_plus()){ // no power of x (e^x below): over its fastest term
    gen t=fastest(d);
    if (!is_zero(t) && !is_undef(v=lvia(R_LDIV,f,expand(evq(n,t),CTX),expand(evq(d,t),CTX),lev))) return v;
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
  if (!dep(f)){ if (!lev) trust_=true; return Ls(R_LSUB,f,f); } // (a constant: its own limit, no giac limit to check: 3^(n+1)/3^n took 19 s on the calculator)
  if (f.is_plus() && a==plus_inf && !lev){ // (lim (13/14-3/(n+3)-3/(n+4)): 13/14-0-0, each term's own)
    vecteur t,r; flat(at_plus,f,t);
    bool ok=t.size()>1,pi=false,mi=false;
    EACH(it,t){ if (!ok) break; const bool ng=it->is_neg(); const gen l=dep(*it)?growlim(ng?arg(*it):*it):(ng?arg(*it):*it); if (is_undef(l) || !(finv(l) || is_inf(l))){ ok=false; break; } const gen sl=ng?ev(-l):l; if (sl==plus_inf) pi=true; if (sl==minus_inf) mi=true; r.push_back(ng && is_zero(l)?symb_neg(l):sl); }
    const vecteur lv=lvar(f);
    if (ok && !(pi && mi) && (!(pi || mi) || lv.size()!=1 || lv[0]!=X)){ // (ln(2)+ln(3)-inf-inf, as s_n's limit is written; a polynomial's by its dominant term: lim (k+1) is not "inf+1")
      const gen L=ev(symb_plus(r));
      S(R_LLAWS,line3("%0=%1",Lx(f),txt(tplus(r))),0);
      trust_=true; // (each by the growth classes: no giac limit to check)
      return Ls(R_EQV,f,L);
    }
  }
  if (a==plus_inf && !ldir_){ // a sequence's n^2/3^n, 3*5^n/6^n: growth rates (a product: no quotient for lq)
    const gen fe=ev(f),nn=_numer(fe,CTX),dd=_denom(fe,CTX);
    if (!is_undef(v=lgrow(f,nn,dd))){ if (!lev) trust_=true; return v; }
    const gen fc=pcomb(f); // ((n+1) n^n/(n+1)^(n+1): (n/(n+1))^n, then 1 to the infinity)
    if (fc!=f){ const size_t m=mark(); S(R_PCOMB,Lx(f),Lx(fc)); if (!is_undef(v=lsteps(fc,lev+1))){ if (!lev && finv(v)) trust_=true; return Ls(R_EQV,f,v); } cut(m); }
    { vecteur w0; flat(at_prod,fe,w0); gen b=1; bool m1=false; // ((-1)^n b_n, (-3)^n/2^n: the size of the terms first)
      EACH(it,w0){ gen u=*it; const gen q=bexp(u);
        if (dep(q) && !dep(u) && evalf(u,1,CTX).type==_DOUBLE_ && is_strictly_positive(-evalf(u,1,CTX),CTX)){ m1=true; const gen au=ev(-u); if (!is_one(au)) b=b*symb_pow(au,q); }
        else b=b*(*it); }
      if (m1 && dep(b)){
        const size_t m=mark();
        S(R_LALT,Lx(SYM(at_abs,f)),Lx(b));
        const gen Lb=lsteps(b,lev+1);
        if (is_zero(Lb)){ S(R_LALT0,Lx(f),gen(0)); if (!lev) trust_=true; return 0; }
        if (!is_undef(Lb) && (finv(Lb) || is_inf(Lb))){ const gen dne=identificateur("DNE"); S(R_LALTN,Lx(f),dne); if (!lev) trust_=true; return dne; }
        cut(m);
      } }
  }
  if (!is_inf(a) && !has_op(f,*at_floor) && !has_op(f,*at_sign) && finv(v=ev(sub(f,X,a)))) return Ls(R_LSUB,f,v); // continuous there
  if (ldir_ && !is_inf(a)){ // one side: |g| with g -> 0 there is g or -g (on the right of 0, |x| = x)
    const gen g=absarg(f);
    if (!is_zero(g) && is_zero(ev(sub(g,X,a)))){
      const gen s=spos(evf(sub(g,X,a+gen(ldir_)/1000)))?g:symb_neg(g);
      S(R_LSIDE,SYM(at_abs,g),s);
      return lsteps(sub(f,SYM(at_abs,g),s),lev+1);
    }
  }
  vecteur c,w,de; factors(f,c,w);
  vecteur nu(c);
  EACH(it,w){ gen b=*it,e=bexp(b); if (spos(-e)) de.push_back(is_minus_one(e)?b:symb_pow(b,evn(e))); else nu.push_back(*it); }
  if (!de.empty()){ gen n=prodof(nu),d=prodof(de); return lq(quo(n,d),n,d,lev); }
  gen k=ev(prodof(c));
  if (w.size()==1 && w[0].type==_SYMB){
    const gen & g=w[0],& u=g.is_pow()?arg(g,0):arg(g);
    if (g.is_pow() && dep(arg(g,1)) && a==plus_inf){ // (a base going to L < 1 or > 1, the power to infinity: no 1^inf)
      const gen Lu=growlim(u),Lv=growlim(arg(g,1));
      if (Lv==plus_inf && finv(Lu) && !is_strictly_positive(-Lu,CTX) && vs1(Lu)){
        const gen r=vs1(Lu)<0?gen(0):plus_inf;
        S(R_LPOWB,Lx(u),Lu);
        if (!lev) trust_=true; // (the growth classes' limits: ((5n^2+3n+4)/n^3)^n took 11 s more on the calculator with giac's)
        return Ls(R_EQV,f,ev(k*r));
      }
      if (Lv==plus_inf && is_one(Lu)){ // 1^inf, (n/(n+1))^n: e^(lim v(u-1)) (the course's e = lim (1+1/n)^n; giac's limit took 9 s on the calculator)
        const gen e=normal(ev(arg(g,1)*normal(ev(u-1),CTX)),CTX);
        const size_t m=mark();
        S(R_LE1,Lx(f),is_one(k)?SYM(at_exp,Lx(e)):symb_prod(k,SYM(at_exp,Lx(e))));
        const gen A=lsteps(e,lev+1);
        if (finv(A)){ if (!lev) trust_=true; return Ls(R_EQV,f,ev(k*exp(A,CTX))); }
        cut(m);
      }
    }
    if (g.is_pow() && dep(arg(g,1))){ // u^v: e^(v ln u)
      gen e=symb_prod(arg(g,1),symb_ln(u));
      L=linner(R_LEXP,f,symb_prod(k,SYM(at_exp,Lx(e))),e,lev);
      return is_undef(L)?L:Ls(R_EQV,f,evm(k,exp(L,CTX)));
    }
    if (u.type!=_VECT){ // h(u), h continuous (a constant power too)
      if (u==X) return Ls(R_LLAW,f,glim(f)); // e^x at infinity: at once
      size_t m=mark();
      L=linner(R_LCOMP,f,symb_prod(k,g.is_pow()?symb_pow(Lx(u),arg(g,1)):SYM(&g._SYMBptr->sommet,Lx(u))),u,lev);
      if (is_undef(L)) return L;
      if (L.type==_IDNT && !ldir_){ cut(m); return sides(f); } // atan(1/x) at 0
      v=evm(k,sub(g,u,L)); // (giac's ln(0) is -inf, e^(-inf) 0)
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
    EACH(it,w) if ((is(*it,at_sin) || is(*it,at_cos)) && !finv(glim(*it)) && is_zero(glim(ev(f / *it)))) return Ls(R_LSQZ,f,zero);
  }
  if (f.is_plus()){
    vecteur t; flat(at_plus,f,t);
    bool pi=false,mi=false;
    EACH(it,t){ gen l=glim(*it); if (l==plus_inf) pi=true; else if (l==minus_inf) mi=true; else if (!finv(l)) pi=mi=true; }
    if (pi && mi && has_op(f,*at_ln)){ // ln(k+1)-ln(k): one log, its inside's limit (continuity)
      const gen c=_lncollect(f,CTX);
      if (is(c,at_ln) && !is_undef(v=lvia(R_LNQ,f,c,1,lev))) return v;
    }
    if (pi && mi){ // infinity - infinity (or a term without a limit, csc(x)-1/x at 0): the
      gen c=conjof(f),h=rn(f),d=dnm(h); // conjugate, one fraction, the fastest term out
      if (!is_zero(c) && !is_undef(v=lvia(R_LCONJ,f,rn(expand(evm(f,c),CTX)),c,lev))) return v;
      if (dep(d) && !is_undef(v=lvia(R_LFRAC,f,nmr(h),d,lev))) return v;
      gen tt=is_inf(a)?fastest(f):zero;
      return is_zero(tt)?gen(undef):lvia(R_LDOM,f,symb_prod(tt,expand(evq(f,tt),CTX)),1,lev);
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
        if ((!is(b,at_sin) && !is(b,at_cos)) || (jt->is_pow() && !spos(arg(*jt,1)))) return undef;
        ++k;
      }
      if (k){ if (!is_zero(glim(*it))) return undef; S(R_LSQZ,Lx(*it)); }
    }
  }
  v=glim(f); // every part has a limit: put together
  return finv(v) || is_inf(v)?Ls(R_LLAW,f,v):gen(undef);
}
// F without a jump in [a,b]: between 48 points F moves at most about h max|f| (F(b)-F(a) was 0
// for 1/(5+3cos x) on [0,2pi]: its tan(x/2) antiderivative jumps at pi). Undefined points: no check.
static NOINL bool nojump(const gen & F,const gen & f,const gen & x,const gen & a,const gen & b){
  gen A=evf(a),B=evf(b);
  if (A.type!=_DOUBLE_ || B.type!=_DOUBLE_) return true;
  double h=(B._DOUBLE_val-A._DOUBLE_val)/48,v[48],m=0; // (the largest |f| of all: cos(99x)sin(101x))
  for (int i=0;i<48;++i){
    gen t=A._DOUBLE_val+(i+0.37)*h,u=evf(sub(F,x,t)),y=evf(sub(f,x,t));
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
static NOINL bool same(const gen & a,const gen & b){ return is_zero(rn1(evs(a,b))); }
// giac's sum left as is, infinite or no value: a divergent one (or one giac cannot do)
static NOINL bool undone(const gen & G){ return is_inf(G) || is_undef(G) || G.type==_STRNG || has_op(G,*at_bounded_function) || is(G,at_sum); }
// the p-series conclusion, p = e: converges for p > 1 (giac's value; sum(1/n^2) left as is by giac,
// pi^2/6 is FlowCE's, in the answer), diverges for p <= 1
static NOINL gen kval(const gen & op); // (below)
// a series that converges: its value when giac has one (a last line "= value"), or a known series'
// at a number (2^k/k!: e^2), else the word
static NOINL gen cvg(const gen & op,const gen & G){
  if (is(G,at_sum) || is_undef(G)){ const gen kv=kval(op); return is_undef(kv)?word("converges"):kv; }
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
// the sums linear in N (*nn_) written out: 2(N+1)-1 as 2N+1, N+1+1 as N+2
static const gen * nn_;
static NOINL gen linn(const gen & g){
  gen a,b;
  if (g.is_plus() && is_linear_wrt(g,*nn_,a,b,CTX)) return a*(*nn_)+b;
  return g.type==_SYMB?map_of(g,linn):g;
}
// the typed term f upside down: the factors under its bar on top, the others below
static NOINL gen flipf(const gen & f){
  vecteur w,n,d; flat(at_prod,f,w);
  EACH(it,w) (it->is_inv()?n:d).push_back(it->is_inv()?arg(*it):*it);
  return d.empty()?prodof(n):n.empty()?symb_inv(prodof(d)):symb_prod(prodof(n),symb_inv(prodof(d)));
}
static NOINL gen ratio_of(const gen & f,const gen &,const gen & x){
  nn_=&x;
  return symb_prod(linn(subst(f,x,x+1,true,CTX)),flipf(f)); // (the typed term: giac's wrote (x-1)^(2k) as ((x-1)^k)^2)
}
// g at n as a decimal (0 if not real): compared as decimals (giac's is_strictly_positive is
// false for 4.5e-7, the term n e^(-n^2) at 4)
static NOINL double at_(const gen & g,const gen & n){ gen y=evalf_double(sub(g,X,n),1,CTX); return y.type==_DOUBLE_?y._DOUBLE_val:0; } // (evalf_double: the calculator's evalf of 1/(2n)! at 20 was no double, the terms' limit went to giac)
// L against 1 for a test's verdict: -1 below, 1 above, 0 equal or too close to tell (the calculator's
// floats: e^2/e^2 was 0.9999999, a wrong "ratio below 1: the terms go to 0" for lim |(n+1)!/(n+1)^(n+1)| n^n/n!)
static NOINL int vs1(const gen & L){
  if (L==plus_inf) return 1;
  const gen d=ev(L-1);
  if (d.type==_INT_ || d.type==_ZINT || d.type==_FRAC) return is_zero(d)?0:is_strictly_positive(d,CTX)?1:-1;
  const gen e=evalf_double(d,1,CTX);
  return e.type!=_DOUBLE_ || fabs(e._DOUBLE_val)<TOL?0:e._DOUBLE_val>0?1:-1;
}
// 0 when g falls fast toward 0 at a+5, a+10, a+20 (giac's limit of n!/n^n took seconds on the
// calculator; of 1/n! it stays 1/((+infinity)!)), else g's limit
static NOINL gen growlim(const gen & g);
static NOINL gen lim_(const gen & g,const gen & a){
  const double y1=fabs(at_(g,a+4)),y2=fabs(at_(g,a+8)),y3=fabs(at_(g,a+14)); // (not past 30!: the calculator's floats end at 3e38, its 1/40! was 5e-10)
  if (y1>0 && y2<y1 && y3<=y2 && y3<1e-3*y1) return 0; // (<=: both past the floats, 0)
  const gen L=growlim(g); // (the growth rates first: giac's limit took seconds)
  if (!is_undef(L)) return L;
  const gen G=ev(Linf(g,X));
  return has_op(G,*at_factorial)?ev(subst(G,SYM(at_factorial,plus_inf),plus_inf,false,CTX)):G; // (giac's 1/((+infinity)!) is 0)
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
static NOINL gen npow(const gen & g){ return g.is_pow() && dep(arg(g,0)) && dep(arg(g,1))?plus_one:first_of(g,npow); }
// 1 if the index is in an exponent of g (2^n, n^n), else 0
static NOINL gen nexp(const gen & g){ return g.is_pow() && dep(arg(g,1))?gen(1):first_of(g,nexp); }
// 1 if g has a sine, cosine or tangent of the index going to infinity (sin(n pi/2)), else 0
static NOINL gen otrig(const gen & g){ return (is(g,at_sin) || is(g,at_cos) || is(g,at_tan)) && dep(arg(g)) && fabs(at_(arg(g),1000))>10?gen(1):first_of(g,otrig); }
// val and giac's sum G the same; G not evaluated when left as a sum (comparing evaluated it: 78 s on
// the calculator for sum((-9x^2)^k,k,0,inf), whose sum was not computed on purpose)
static NOINL bool sameG(const gen & val,const gen & G){ return !is(G,at_sum) && same(val,G); }
static bool quickG_; // (the console's series: no giac sum, 13-30 s on the calculator for the course's divergent ones)
const char * steps_divmsg; // (steps_series_answer: why a series diverges, shown as the console's answer)
static gen lsteps(const gen & f,int lev);
// g's limit at +infinity (the index X): by lgrow's growth rates when it decides at the top (no steps
// kept), else giac's
static NOINL gen fastlim(const gen & g){
  gen inf=plus_inf,* ola=la_;
  const int od=ldir_,ob=budget_;
  la_=&inf; ldir_=0; budget_=80;
  const size_t m=mark();
  trust_=false;
  gen L=lsteps(g,0);
  cut(m);
  if (!trust_) L=ev(Linf(g,X));
  la_=ola; ldir_=od; budget_=ob;
  return L;
}
// g's limit at +infinity (the index X) with its steps (the ratio test's rest): lsteps', giac's if none
static NOINL gen lkeep(const gen & g){
  gen inf=plus_inf,* ola=la_;
  const int od=ldir_,ob=budget_;
  la_=&inf; ldir_=0; budget_=80;
  const size_t m=mark();
  gen L=lsteps(g,0);
  if (is_undef(L)){ cut(m); L=ev(Linf(g,X)); if (finv(L) || is_inf(L)) S(R_EQV,Linf(g,X),L); }
  la_=ola; ldir_=od; budget_=ob;
  return L;
}
static NOINL gen growlim(const gen & g){
  gen inf=plus_inf,* ola=la_;
  const int od=ldir_,ob=budget_;
  la_=&inf; ldir_=0; budget_=40; lgratio_=false; nogiac_=true;
  const size_t m=mark();
  const gen fe=ev(g),L=lgrow(g,_numer(fe,CTX),_denom(fe,CTX));
  cut(m);
  la_=ola; ldir_=od; budget_=ob; nogiac_=false;
  return lgratio_?gen(undef):L;
}
// the free variable of a power series' terms besides the index x (0: none, or more than one)
static NOINL gen pvar(const gen & t,const gen & x){
  const vecteur ids=lidnt(t);
  gen y=0;
  EACH(it,ids) if (*it!=x && evalf(*it,1,CTX).type!=_DOUBLE_){ if (!is_zero(y)) return 0; y=*it; } // (pi: a number)
  return y;
}
// T as rest * u^e, u^e its only factor depending on y (nested powers, the console's ((x-1)^k)^2,
// put together); false if there is none or more than one
static NOINL bool ypart(const gen & T,const gen & y,gen & u,gen & e,gen & rest){
  vecteur w,o; flat(at_prod,T,w);
  u=0;
  EACH(it,w){
    if (!dep(*it,y)){ o.push_back(*it); continue; }
    if (!is_zero(u)) return false;
    u=*it; e=1;
    while (u.is_pow()){ e=arg(u,1)*e; u=arg(u,0); }
  }
  rest=prodof(o);
  return !is_zero(u);
}
// q^(1/p) simplified: 2 for 8^(1/3), 1/3 for (1/9)^(1/2), sqrt(2)/2 for (1/2)^(1/2)
static NOINL gen rootp(const gen & q,const gen & p){ return normal(is_one(p)?q:p==2?ev(SYM(at_sqrt,q)):ev(SYM2(at_surd,q,p)),CTX); }
// L|u|^p < 1 solved for y (u = al y + be, the course's way): |u|^p < 1/L, |u| < R, -R < u < R, then
// lo < y < hi; false if u is not linear in y
static NOINL bool pint(const gen & y,const gen & L,const gen & u,const gen & p,gen & lo,gen & hi){
  gen al,be;
  if (!is_linear_wrt(u,y,al,be,CTX) || is_zero(al)) return false;
  const gen au=SYM(at_abs,u),R=rootp(ev(inv(L,CTX)),p);
  if (!is_one(L) && !is_one(p)) S(R_NLINE,SYM2(at_inferieur_strict,symb_pow(au,p),ev(inv(L,CTX))),0);
  if (!is_one(L) || !is_one(p)) S(R_NLINE,SYM2(at_inferieur_strict,au,R),0);
  const bool fa=!is_one(abs(al,CTX)) && !is_zero(be);
  if (fa){ // (the inside factored: the center and radius read off)
    const gen aa=ev(abs(al,CTX)),xc=SYM(at_abs,ev(y+be/al));
    S(R_NLINE,SYM2(at_inferieur_strict,symb_prod(aa,xc),R),0);
    S(R_NLINE,SYM2(at_inferieur_strict,xc,ev(R/aa)),0);
  }
  lo=ev((-R-be)/al); hi=ev((R-be)/al);
  if (is_strictly_positive(-al,CTX)) std::swap(lo,hi);
  S(R_PSRAD,line3("c=%0,R=%1",ev(-be/al),normal(ev(R/abs(al,CTX)),CTX)),0); // (the course asks them too)
  if (fa){ S(R_PSOPEN,line3("-%1<%0<%1",symb_plus(y,ev(be/al)),normal(ev(R/abs(al,CTX)),CTX)),0); S(R_NLINE,line3("%1<%0<%2",y,lo,hi),0); } // (|x-1/2| < 1/2 unpacked, then 1/2 added: one operation a line)
  else if (u!=y){ S(R_PSOPEN,line3("-%1<%0<%1",u,R),0); S(R_NLINE,line3("%1<%0<%2",y,lo,hi),0); }
  else S(R_PSOPEN,line3("%1<%0<%2",y,lo,hi),0);
  return true;
}
static NOINL gen ssteps(const gen & op,const vecteur & v,const gen & x);
// a pair of the ratio as text: (top)/(bottom) (the parentheses hidden by the drawing)
static NOINL std::string frs(const gen & t,const gen & b){ return "("+pr(t)+")/("+pr(b)+")"; }
static NOINL gen lkeep(const gen & g);
static NOINL gen growlim(const gen & g);
// The course's ratio test written out: a_k and a_(k+1) (k+1 put in for k), L = lim |a_(k+1)/a_k| as
// a_(k+1) times a_k upside down (two fractions), the like factors paired, each pair cancelled with
// the constants and the x part out of the limit, the rest's limit by its own steps (dominant terms,
// growth rates), L. v0: the term as typed; y: a power series' variable (0: none), its part |u|^d in
// xp. Returns the constant L (times xp for a power series); undef (no steps) if a factor does not pair
static NOINL std::string pq(const gen & g); // (below)
static NOINL gen rtest(const gen & v0,const gen & x,const gen & y,gen & xp,bool quiet=false){
  vecteur w,nu,de; flat(at_prod,v0,w);
  EACH(it,w){ if (it->is_inv()){ vecteur d; flat(at_prod,arg(*it),d); de.insert(de.end(),d.begin(),d.end()); }
    else if (it->type==_FRAC){ if (!is_one(it->_FRACptr->num)) nu.push_back(it->_FRACptr->num); de.push_back(it->_FRACptr->den); } // ((k^2+k)/2: the 2 below, ../2 * 2/..)
    else nu.push_back(*it); }
  nn_=&x;
  std::string ps; // (the pairs, (top)/(bottom)*...)
  int np=0;
  gen c=1,rest=1;
  vecteur xs; // (the x part's factors, unevaluated: giac writes |x-1|^2 as (x-1)^2)
  std::string cp; // (the pairs that cancel: 2^(k+1)/2^k=2, (k+1)!/k!=k+1)
  for (int s=0;s<2;++s){
    const vecteur & f=s?de:nu;
    EACH(it,f){
      if (!dep(*it,x)) continue; // (a constant: it cancels)
      const gen f1=linn(subst(*it,x,x+1,true,CTX)),top=s?*it:f1,bot=s?f1:*it;
      if (np++) ps+="*";
      ps+=frs(top,bot);
      gen b=*it;
      const gen e=bexp(b);
      if (!is_zero(y) && dep(b,y)){ // the x part: |u|^d
        const gen d=ratnormal(ev(sub(e,x,x+1)-e),CTX);
        if (dep(b,x) || dep(d,x) || !is_strictly_positive(d,CTX)) return undef;
        xs.push_back(is_one(d)?SYM(at_abs,b):symb_pow(SYM(at_abs,b),d));
        if (!quiet){ if (!cp.empty()) cp+=","; cp+=frs(top,bot)+"="+pr(is_one(d)?b:symb_pow(b,d)); }
        continue;
      }
      if (!is_zero(y) && dep(*it,y)) return undef;
      if (is(b,at_exp) && !dep(e,x)){ // e^(f(k)): e^(f(k+1)-f(k))
        const gen d=ratnormal(ev(e*(arg(subst(b,x,x+1,false,CTX))-arg(b))),CTX),q=symb_exp(s?gen(ev(-d)):d);
        if (dep(d,x)) rest=rest*q; else { c=c*ev(q); if (!quiet){ if (!cp.empty()) cp+=","; cp+=frs(top,bot)+"="+pr(ev(q)); } }
      }
      else if (!dep(b,x)){ // b^e(k): b^(e(k+1)-e(k)), a constant or not
        const gen d=ratnormal(ev(sub(e,x,x+1)-e),CTX),q=symb_pow(b,s?gen(ev(-d)):d);
        if (dep(d,x)) rest=rest*q; else { c=c*ev(q); if (!quiet){ if (!cp.empty()) cp+=","; const gen dd=s?gen(ev(-d)):d,bd=is_one(abs(dd,CTX))?b:symb_pow(b,ev(abs(dd,CTX))),e0=s?top:bot; // (3^k/3^(k+1) = 3^k/(3*3^k) = 1/3: the power split, then cancelled)
          cp+=frs(top,bot)+"="+(is_minus_one(b)?std::string():(is_strictly_positive(dd,CTX)?frs(symb_prod(bd,e0),e0):frs(e0,symb_prod(bd,e0)))+"=")+pr(ev(q)); } } // ((-1)^(k+1)/(-1)^k = -1 at once)
      }
      else if (is(b,at_factorial)){ const gen q=_factor(_simplify(ev(top/bot),CTX),CTX); rest=rest*q; if (!quiet){ if (!cp.empty()) cp+=","; cp+=frs(top,bot)+"="+pq(q); } } // ((k+1)!/k!: k+1; (k+1)^2 and 1/(2(k+1)(2k+1)) factored, as the course leaves them)
      else rest=rest*SYM2(at_division,top,bot); // (a polynomial's pair as it is: (k+4)^3/(k+3)^3)
    }
  }
  if (!np) return undef;
  xp=xs.empty()?gen(1):prodof(xs);
  { vecteur cw,cn,cx; flat(at_prod,ev(c),cw); EACH(it,cw) (isnum(*it) || (it->is_inv() && isnum(arg(*it)))?cn:cx).push_back(*it); // (|x-3|/4, not |(x-3)*1/4|; (x-1)^4/20, not /5/4)
    const gen k=ev(abs(prodof(cn),CTX)),A=cx.empty()?gen(1):ev(abs(ev(prodof(cx)),CTX));
    if (cx.empty()) c=k;
    else if (k.type==_FRAC) c=SYM2(at_division,is_one(k._FRACptr->num)?A:symb_prod(k._FRACptr->num,A),k._FRACptr->den); // (as built: printed |x+1|/5)
    else c=is_one(k)?A:symb_prod(k,A); }
  if (quiet){ // (the console: L only; giac's simplify of the whole ratio took 4 s on the calculator)
    gen Lr=dep(rest,x)?growlim(rest):ev(abs(ev(rest),CTX));
    if (is_undef(Lr)){ const size_t m=mark(); Lr=lkeep(rest); cut(m); } // (the limit's rules, 1^inf too, before giac's: (n/(n+1))^n took 9 s on the calculator)
    return is_inf(Lr)?Lr:is_one(abs(Lr,CTX))?(lidnt(c).empty()?rn(c):c):steps_epow(ev(c*abs(Lr,CTX)),CTX); // (c as built when the rest's limit is 1, a number simplified: 1, not e/e)
  }
  const gen k1=linn(subst(v0,x,x+1,true,CTX));
  S(R_RAK,line3("a_%0=%1",x,qshow(v0)),0); // (one fraction: (k^2+k)/2, not (k^2+k)*1/2)
  S(R_LINE,line3("a_(%0+1)=%1",x,qshow(k1)),0);
  S(R_RDEF,line3("L=limit(abs(a_(%0+1)/a_%0),%0,+infinity)",x),0); // (where |...| comes from: the test's definition)
  bool dc=true; EACH(it,de) if (dep(*it,x)) dc=false;
  if (dc) S(R_RLIM1,line3("L=limit(abs((%1)/(%2)),%0,+infinity)",x,qshow(k1),qshow(v0)),0); // (k! x^k, a number below ((k^2+k)/2): one fraction over the other, narrow)
  else { // a_(k+1) as one fraction times a_k upside down
    vecteur n1,d1;
    EACH(it,nu) n1.push_back(linn(subst(*it,x,x+1,true,CTX)));
    EACH(it,de) d1.push_back(linn(subst(*it,x,x+1,true,CTX)));
    S(R_RLIM,line("L=limit(abs((%1)/(%2)*(%3)/(%4)),%0,+infinity)",x,prodof(n1),prodof(d1),prodof(de),prodof(nu),zero),0);
  }
  if (np>1) S(R_RGRP,line3("L=limit(abs(%1),%0,+infinity)",x,txt(ps)),0);
  if (!cp.empty()) S(R_RPAIR,txt(cp),0); // (each pair worked out before the constants come out)
  vecteur ov;
  if (!is_one(c)) ov.push_back(c);
  if (!is_one(xp)) ov.push_back(xp);
  gen Lr=1;
  if (dep(rest,x)){
    ov.push_back(Linf(rest,x));
    S(R_RCAN,line3("L=%0",prodof(ov)),0);
    ++depth_; Lr=lkeep(rest); --depth_;
    if (is_undef(Lr)) return undef;
  }
  else if (!is_one(rest)) c=ev(c*abs(ev(rest),CTX));
  gen L=is_inf(Lr)?Lr:ev(c*Lr);
  if (!is_inf(L) && lidnt(L).empty()) L=rn(L); // (a number: e/e is 1)
  S(R_RVAL,line3("L=%0",is_inf(L) || is_zero(L) || is_one(xp)?L:is_one(L)?xp:symb_prod(L,xp)),0);
  return L;
}
// a fraction or product as it is, a sum in parentheses (1-1/3, 1-(x-1)/2)
static NOINL std::string pq(const gen & g){ return g.is_plus()?par(g):pr(g); }
// the rules of exponents on a geometric term, between it and a r^k: 2^(2k)/3^(k+7) is
// (2^2)^k/(3^k*3^7), 3^(2k-1) is (3^2)^k/3 ((b^m)^k b^c); 0 if nothing changes
static NOINL gen gmid(const gen & v0,const gen & x){
  vecteur w,nu,de; flat(at_prod,v0,w);
  bool ch=false;
  EACH(it,w){
    gen b=*it,al,be;
    const bool in=b.is_inv();
    if (in) b=arg(b);
    if (b.is_pow() && !dep(arg(b,0),x) && is_linear_wrt(arg(b,1),x,al,be,CTX) && !is_zero(al) && (!is_one(al) || !is_zero(be))){
      const gen B=arg(b,0);
      (in?de:nu).push_back(symb_pow(is_one(al)?B:symb_pow(B,al),x));
      if (!is_zero(be)){ const bool ng=is_strictly_positive(-be,CTX),o=ng?!in:in; const gen c=ng?ev(-be):be; (o?de:nu).push_back(is_one(c)?B:symb_pow(B,c)); }
      ch=true;
    }
    else (in?de:nu).push_back(b);
  }
  if (!ch) return 0;
  const gen n=prodof(nu);
  return de.empty()?n:SYM2(at_division,n,prodof(de));
}
// a geometric series' term as the course writes it, a r^k (rules of exponents: 3^(2k-1)/4^k is
// (1/3)(9/4)^k), then a and r and (numbers) |r| < 1; its a
static NOINL gen gform(const gen & v0,const gen & t,const gen & x,const gen & r,bool in_x){
  gen a=_simplify(ev(t/pow(r,x,CTX)),CTX);
  if (!isnum(a)) a=normal(a,CTX);
  bool ar=true; // (typed a r^k already: no step, its a as typed, 2-2/e^2)
  { vecteur w,c; flat(at_prod,v0,w); int np=0; EACH(it,w) if (dep(*it,x)){ if (it->is_pow() && arg(*it,1)==x && !dep(arg(*it,0),x)) ++np; else ar=false; } else c.push_back(*it);
    ar=ar && np==1; if (ar) a=c.empty()?gen(1):prodof(c); }
  const gen f=is_one(a)?symb_pow(r,x):symb_prod(a,symb_pow(r,x));
  if (!ar && pr(v0)!=pr(f)){ const gen mid=gmid(v0,x); // (the exponents' rules shown: where 4/3 and 1/3^7 come from)
    if (!is_zero(mid) && pr(mid)!=pr(v0) && pr(mid)!=pr(f)) S(R_GFORM,line3("%0=%1=%2",v0,mid,f),0); else S(R_GFORM,line3("%0=%1",v0,f),0); }
  S(R_GAR,line3("a=%0,r=%1",a,r),0);
  if (!in_x){ const gen ar=ev(abs(r,CTX)); if (is_strictly_greater(1,ar,CTX)) S(R_LINE,line3("abs(r)=%0<1",ar),0); }
  return a;
}
// the ratio test of a series of numbers written out (rtest) with its conclusion, L < 1 or L > 1;
// false (nothing kept) if it does not pair or its L is not Lq
static NOINL bool rshow(const gen & v0,const gen & x,const gen & Lq){
  const size_t m=mark();
  gen xp;
  const gen L=rtest(v0,x,0,xp);
  if (is_undef(L) || (is_inf(L)?Lq!=plus_inf:!close(L,Lq))){ cut(m); return false; }
  const int c=vs1(L);
  if (!c){ cut(m); return false; }
  const bool lo=c<0;
  S(lo?R_RCONV:R_RDIVG,line3(lo?"L=%0<1":"L=%0>1",L),0);
  return true;
}
// a power series in y, sum(c(n)*u^e(n),n,a,inf) as typed, u linear in y, e(n+1)-e(n) = p > 0
// (geometric ones are ssteps'): the ratio test with y (the course's interval problems),
// |a(n+1)/a(n)| (written as a(n+1) times a(n) upside down, as the course does) = |c(n+1)/c(n)| |u|^p,
// its limit L|u|^p below 1: the open interval (the course's question: the ends are not tested);
// the interval (its line) is the answer, giac's value when it has one
gen steps_power_closed(const gen & f,const gen & n,int a); // (answer.cc: the known patterns' closed form)
static NOINL gen psteps(const gen & op,const vecteur & v,const gen & x,const gen & y,const gen & G){
  gen u,e,c0;
  if (!ypart(v[0],y,u,e,c0)) return undef;
  const gen p=ratnormal(sub(e,x,x+1)-e,CTX);
  if (dep(u,x) || p.type!=_INT_ || p.val<1) return undef;
  gen c=1,Lc=undef;
  if (quickG_){ gen xp; Lc=rtest(v[0],x,y,xp,true); } // (the console: its pairs, no simplify)
  if (is_undef(Lc)){
    c=_simplify(ev(sub(c0,x,x+1)/c0),CTX);
    { const gen f=_factor(c,CTX); if (!nrad(f)) c=f; } // ((k+4)^3/(9(k+3)^3), not over sqrt(10))
    if (is_strictly_positive(-evalf(sub(c,x,ev(v[2])+3),1,CTX),CTX)) c=ev(-c);
    Lc=growlim(c); // (the growth rates first: giac's limit of (k+4)^3/(9(k+3)^3) took seconds on the calculator)
    if (is_undef(Lc)) Lc=ev(Linf(c,x));
  }
  const gen up=is_one(p)?SYM(at_abs,u):symb_pow(SYM(at_abs,u),p),B=symb_prod(c,up),L=ev(abs(Lc,CTX)); // (3|x| < 1, not 3|x|^1)
  if (!finv(L) && L!=plus_inf) return undef;
  nn_=&x;
  if (!quickG_){ const size_t m=mark(); gen xp; const gen R=rtest(v[0],x,y,xp);
    if (is_undef(R) || (!is_inf(R) && !close(R,L))){ cut(m); nn_=&x;
      S(R_SRAT,SYM(at_abs,symb_prod(linn(subst(v[0],x,x+1,true,CTX)),flipf(v[0]))),B);
      S(R_SLIM,Linf(B,x),is_zero(L) || L==plus_inf?L:symb_prod(L,up)); } }
  gen I,lo,hi,al,be;
  if (is_zero(L)){ S(R_PSALL,I=line3("-oo<%0<oo",y),0); S(R_PSRADA,txt("R=+infinity"),0); }
  else if (L==plus_inf){ if (!is_linear_wrt(u,y,al,be,CTX)) return undef; S(R_PSONE,I=symb_equal(y,ev(-be/al)),0); S(R_PSRAD0,txt("R=0"),0); }
  else { S(R_PSLT,is_one(L)?up:symb_prod(L,up),0); if (!pint(y,L,u,p,lo,hi)) return undef; I=out_->back().a; } // (|2x-1| < 1, not 1*|2x-1|)
  if (!quickG_ && ev(v[2]).type==_INT_){ const gen cf=steps_power_closed(ev(v[0]),x,ev(v[2]).val); if (!is_zero(cf)) S(R_PSUMF,line3("%0=%1",op,cf),0); } // (the function it adds up to, the console's closed form: -ln((5-x)/2))
  return I;
}
// a term at a number as the course writes it out: its sums and products of integers worked out, its
// fractions as they come (3/(4+2) is 3/6, ln(1+1) ln(2), 1/1 is 1)
static NOINL gen ifold(const gen & g){
  if (g.type!=_SYMB) return g;
  if (lidnt(g).empty() && (g.is_plus() || g.is_prod()) && !has_op(g,*at_inv) && !has_op(g,*at_pow) && !has_op(g,*at_exp)) return ev(g);
  const gen h=map_of(g,ifold);
  if (h.is_pow() && arg(h,1).type==_INT_ && arg(h,1).val==0) return 1; // (0^0 = 1 too: a series' first term)
  if (h.is_pow() && arg(h,1).type==_INT_ && (is_minus_one(arg(h,0)) || arg(h,1).val==1)) return ev(h); // ((-1)^2, x^1)
  if (is(h,at_factorial) && arg(h).type==_INT_ && arg(h).val<2) return 1; // (0!, 1!; 2! kept, as written)
  if (h.is_inv() && is_one(arg(h))) return 1;
  if (is(h,at_division) && arg(h).type==_VECT && arg(h)._VECTptr->size()==2 && is_one(arg(h,1))) return arg(h,0); // (2*3^3/9^0: 2*3^3, not 2*3^3/1)
  if (h.is_prod() && arg(h).type==_VECT){ // (no 1s; -1 as a minus sign: -x^2/2!)
    vecteur w; bool ng=false;
    EACH(it,*arg(h)._VECTptr) if (is_minus_one(*it)) ng=!ng; else if (!is_one(*it)) w.push_back(*it);
    const gen r=w.empty()?gen(1):prodof(w);
    return ng?symb_neg(r):r;
  }
  return h;
}
static NOINL gen tq(const gen & g,const gen & x,const gen & j){ return ifold(subst(g,x,j,true,CTX)); }
// the terms of a sum as one line: the ones without a minus sign first (ln(5)-ln(3), as the course)
static NOINL std::string tplus(const vecteur & w){
  std::string s,m;
  EACH(it,w){
    const std::string p=pr(*it);
    const char * c=p.c_str();
    if (!*c) continue;
    if (*c=='-'){ m+=c; continue; }
    if (!s.empty()) s+="+";
    s+=*c=='+'?c+1:c;
  }
  return s.empty()?(m.empty()?std::string("0"):m):s+m;
}
// the course's telescoping work, t = u(k) - u(k+m) typed as p + q, w(k) = -u(k+m): some terms a_k,
// some partial sums s_n (what cancels cancelled, nothing else simplified), the pattern's s_n; the
// constant terms back (the limit's line)
static NOINL vecteur tlines(const gen & p,const gen & q,const gen & u,const gen & w,int m,const gen & x,const gen & a,const gen & n){
  if (a.type!=_INT_) return vecteur();
  const std::string sn=pr(n);
  for (int j=a.val;j<a.val+4;++j){ vecteur r; r.push_back(tq(p,x,j)); r.push_back(tq(q,x,j)); S(j==a.val?R_TWA:R_LINE,line3("a_%0=%1",j,txt(tplus(r))),0); }
  for (int j=a.val;j<a.val+4;++j){
    vecteur r;
    for (int i=a.val;i<=j && i<a.val+m;++i) r.push_back(tq(u,x,i));
    for (int i=j+1-m>a.val?j+1-m:a.val;i<=j;++i) r.push_back(tq(w,x,i));
    S(j==a.val?R_TWS:R_LINE,line3("s_%0=%1",j,txt(tplus(r))),0);
  }
  vecteur c,r;
  for (int i=a.val;i<a.val+m;++i) c.push_back(tq(u,x,i));
  r=c;
  nn_=&n;
  for (int i=1-m;i<=0;++i) r.push_back(linn(subst(w,x,n+i,true,CTX)));
  S(R_TSN,line3("s_%0=%1",n,txt(tplus(r))),0);
  return c;
}
// the definition of convergence: lim s_n with its terms' limits written in (3/6+3/7-0-0), the value
static NOINL gen tlim(const vecteur & c,const gen & SN,const gen & n,const gen & val){
  vecteur w,r(c); flat(at_plus,SN,w);
  EACH(it,w) if (dep(*it,n)){ const bool ng=it->is_neg(); const gen L=ev(Linf(ng?arg(*it):*it,n)); r.push_back(ng?is_zero(L)?symb_neg(L):ev(-L):L); } // (3/6+3/7-0-0)
  const gen tp=txt(tplus(r));
  vecteur pt(c); EACH(it,w) if (dep(*it,n)) pt.push_back(*it); // (s_n as the pattern wrote it, 3/6+3/7-3/(n+3)-3/(n+4), inside the limit first)
  S(R_TDEF,line(pr(val)==pr(tp)?"limit(s_%0,%0,+infinity)=%3=%1":"limit(s_%0,%0,+infinity)=%3=%1=%2",n,tp,val,Linf(txt(tplus(pt)),n),zero,zero),val); // (b: the value, the console says where it diverges)
  { vecteur q; bool ok=c.size()>1; gen D=1; // (3/6+3/7 = 1/2+3/7 = 7/14+6/14 = 13/14: where 13/14 comes from)
    EACH(it,c){ const gen v=ev(*it); if (v.type!=_INT_ && v.type!=_FRAC) ok=false; else if (v.type==_FRAC) D=ev(lcm(D,v._FRACptr->den)); q.push_back(v); }
    if (ok && !is_one(D) && D.type==_INT_){
      vecteur b; EACH(it,q){ const gen u=ev(*it*D); b.push_back(is_strictly_positive(-u,CTX)?symb_neg(SYM2(at_division,ev(-u),D)):SYM2(at_division,u,D)); }
      const std::string s0=tplus(c),s1=tplus(q),s2=tplus(b);
      std::string L=s0; if (s1!=s0) L+="="+s1; if (s2!=s1) L+="="+s2; L+="="+pr(val);
      S(R_TADD,txt(L),0);
    } }
  return val;
}
static NOINL bool pf_steps(const gen & input); // (below)
// the partial fractions of a term worked out (A and B found, as the course does), a part of its own;
// false (nothing kept) if they have no steps
static NOINL bool pfshow(const gen & t,const gen & x){
  const size_t m=mark();
  ++depth_; const bool ok=pf_steps(SYM(at_partfrac,makesequence(t,x))) && mark()>m; --depth_;
  if (!ok) cut(m);
  return ok;
}
// ---------------------------------------------------------------- a known series at a number
enum { KS_GEO, KS_EXP, KS_SIN, KS_COS, KS_GEO2, KS_GEO3, KS_LN, KS_ATAN };
static NOINL std::string wsum(const vecteur & w,bool dots); // (below)
// sum(t,k,a,inf) of numbers that is a known series at x = u (the course's sum of 2^k/k!: the series of
// e^x with x = 2; cos, sin, ln(1+x) (from k = 1 or 0), arctan), or P(k) u^k (the geometric series
// differentiated: k/2^k): the known series, u put in, the terms before a taken off, the value; undef if none
static const char * const KVS[]={"u^k/k!","(-1)^k*u^(2*k+1)/(2*k+1)!","(-1)^k*u^(2*k)/(2*k)!","(-1)^(k+1)*u^k/k","(-1)^k*u^(2*k+1)/(2*k+1)","(-1)^k*u^(k+1)/(k+1)"};
static const char * const KVF[]={"exp(x)","sin(x)","cos(x)","ln(1+x)","atan(x)","ln(1+x)"};
// q(k+1)/q(k) for q a product of constant bases to powers linear in k ((-1)^k (pi/2)^(2k)/4^k: -pi^2/16),
// from the exponents (giac's simplify of the quotient took 20 s on the calculator); else the simplified quotient
static NOINL gen gratio(const gen & q,const gen & k){
  vecteur w,o; flat(at_prod,q,w);
  EACH(it,w){ gen b=*it; const gen e=bexp(b); if (!dep(*it,k)) continue; gen al,be;
    if (is(b,at_exp) && is_one(e) && is_linear_wrt(arg(b),k,al,be,CTX)){ o.push_back(SYM(at_exp,al)); continue; } // (e^(-k): e^(-1))
    if (dep(b,k) || !is_linear_wrt(e,k,al,be,CTX)) return _simplify(ev(sub(q,k,k+1)/q),CTX);
    o.push_back(symb_pow(b,al)); }
  return ev(prodof(o));
}
static NOINL gen kvt(int j,const gen & u,const gen & k){ return subst(subst(gen(KVS[j],CTX),gen("u",CTX),u,true,CTX),gen("k",CTX),k,true,CTX); } // (as written, not worked out)
static NOINL gen kval(const gen & op){
  if (!is(op,at_sum) || arg(op).type!=_VECT || arg(op)._VECTptr->size()!=4 || arg(op,1).type!=_IDNT) return undef;
  const vecteur & v=*arg(op)._VECTptr;
  const gen k=v[1],a=ev(v[2]);
  if (a.type!=_INT_ || a.val<0 || a.val>6 || !is_inf(ev(v[3])) || k!=X) return undef;
  const gen t=ev(v[0]);
  { const vecteur ids=lidnt(t); EACH(it,ids) if (*it!=k && evalf(*it,1,CTX).type!=_DOUBLE_) return undef; } // (numbers: pi allowed, x not)
  vecteur w; flat(at_prod,t,w);
  int j=-1; gen Dn=1; // the known series' denominator in t: k!, (2k+1)!, (2k)!, k, 2k+1, k+1
  EACH(it,w){ gen b=*it; const gen e=bexp(b);
    if (!is_minus_one(e) || !dep(b,k)) continue;
    const bool fa=is(b,at_factorial);
    const gen A=fa?ev(arg(b)):b;
    j=A==k?(fa?0:3):same(A,2*k+1)?(fa?1:4):same(A,2*k)&&fa?2:same(A,k+1)&&!fa?5:-1;
    if (j>=0){ Dn=b; break; }
  }
  gen val,u;
  const gen xx=identificateur("x");
  if (j>=0){
    const int k0=j==3?1:0;
    const gen q=rn(ev(t*Dn)),rho=is_zero(q)?gen(0):gratio(q,k); // (c s^k)
    if (is_zero(rho) || dep(rho,k) || a.val<k0) return undef;
    if (!j) u=rho;
    else if (j==3 || j==5) u=ev(-rho);
    else { if (!is_strictly_positive(-evalf(rho,1,CTX),CTX)) return undef; u=_simplify(ev(sqrt(ev(-rho),CTX)),CTX); } // (rho = -u^2: sin, cos, atan)
    const gen ue=evalf(u,1,CTX);
    if (ue.type!=_DOUBLE_ || (j>=3 && (ue._DOUBLE_val>1 || ue._DOUBLE_val<(j==4?-1:-0.999999)))) return undef; // (where they converge)
    const gen c=_simplify(ev(sub(t,k,a)/ev(kvt(j,u,a))),CTX);
    { const gen e1=evalf(sub(t,k,a+1)-c*ev(kvt(j,u,a+1)),1,CTX),e0=evalf(sub(t,k,a+1),1,CTX); // (the shape checked one term on)
      if (dep(c,k) || is_zero(c) || e1.type!=_DOUBLE_ || e0.type!=_DOUBLE_ || fabs(e1._DOUBLE_val)>1e-9*(1+fabs(e0._DOUBLE_val))) return undef; }
    const gen Fu=subst(gen(KVF[j],CTX),xx,u,true,CTX),cF=is_one(c)?Fu:symb_prod(c,Fu),su=SYM(at_sum,makesequence(kvt(j,u,k),k,k0,plus_inf));
    S(R_KKNOWN,gen(KVF[j],CTX),SYM(at_sum,makesequence(kvt(j,xx,k),k,k0,plus_inf)));
    S(R_SUBV,line3("x=%0",u),0);
    val=ev(cF);
    S(R_KVALX,line3(pr(val)==pr(cF)?"%0=%1":"%0=%1=%2",is_one(c)?su:symb_prod(c,su),cF,val),0);
    if (a.val>k0){ // (from a: the known series less its terms before a)
      vecteur tb; gen less=0;
      for (int i=k0;i<a.val;++i){ tb.push_back(ifold(subst(v[0],k,i,true,CTX))); less=less+ev(sub(t,k,i)); }
      val=rn(ev(val-less));
      S(R_KVALS,line3(tb.size()==1?"%0=%1-%2":"%0=%1-(%2)",op,cF,tb.size()==1?tb[0]:txt(wsum(tb,false))),0);
      S(R_EQV,op,val);
    }
    return val;
  }
  // P(k) u^k, P a polynomial of degree 1 to 3 (k/2^k): the geometric series differentiated, times x
  vecteur gw,pw; EACH(it,w) if (dep(*it,k) && geof(*it)) gw.push_back(*it); else pw.push_back(*it);
  const gen P=ev(prodof(pw)),dg=_degree(makesequence(P,k),CTX);
  if (gw.empty() || !dep(P,k) || dg.type!=_INT_ || dg.val<1 || dg.val>3 || dep(_denom(P,CTX),k)) return undef;
  const gen Gm=ev(prodof(gw));
  u=_simplify(ev(sub(Gm,k,k+1)/Gm),CTX);
  const gen ue=evalf(abs(u,CTX),1,CTX);
  if (dep(u,k) || ue.type!=_DOUBLE_ || ue._DOUBLE_val>=1) return undef;
  static const char * const KM[]={"1/(1-x)","x/(1-x)^2","x*(1+x)/(1-x)^3","x*(1+4*x+x^2)/(1-x)^4"}; // (sum k^m x^k from k = 0: (x d/dx)^m 1/(1-x))
  vecteur at; for (int m=0;m<=dg.val;++m){ const gen pm=_coeff(makesequence(P,k,m),CTX); if (!is_zero(pm)) at.push_back(is_one(pm)?gen(KM[m],CTX):symb_prod(pm,gen(KM[m],CTX))); }
  const gen acc=at.size()==1?at[0]:symb_plus(at),c0=_simplify(ev(sub(Gm,k,0)),CTX);
  if (is_zero(ev(P-k))){ // (k u^k: the course's derivation)
    S(R_KKNOWN,gen(KM[0],CTX),SYM(at_sum,makesequence(symb_pow(xx,k),k,0,plus_inf)));
    S(R_KDIFF,gen("1/(1-x)^2",CTX),SYM(at_sum,makesequence(symb_prod(k,symb_pow(xx,symb_plus(k,-1))),k,1,plus_inf)));
  }
  S(R_KVALD,SYM(at_sum,makesequence(symb_prod(P,symb_pow(xx,k)),k,0,plus_inf)),acc);
  S(R_SUBV,line3("x=%0",u),0);
  val=rn(ev(c0*subst(acc,xx,u,false,CTX)));
  { gen less=0; for (int i=0;i<a.val;++i) less=less+ev(sub(t,k,i)); val=rn(ev(val-less)); }
  { const gen f=_factor(val,CTX); if (pr(f).size()<pr(val).size()) val=f; }
  const gen au=subst(acc,xx,u,true,CTX);
  S(R_KVALX,line3("%0=%1=%2",op,is_one(c0)?au:symb_prod(c0,au),val),0);
  return val;
}
// t = a sum of geometric terms ((2^k+3^k)/6^k, 3/4^k-2/5^k): each its own geometric series (|r| < 1),
// their values added; undef if not that
static NOINL gen gsplit(const gen & op,const vecteur & v,const gen & x){
  const gen t=ev(v[0]);
  { const vecteur ids=lidnt(t); EACH(it,ids) if (*it!=x && evalf(*it,1,CTX).type!=_DOUBLE_) return undef; } // (numbers: a power series has x)
  const gen e=t.is_plus()?t:ev(expand(t,CTX));
  if (!e.is_plus() || arg(e).type!=_VECT) return undef;
  const vecteur & ts=*arg(e)._VECTptr;
  if (ts.size()<2 || ts.size()>4) return undef;
  EACH(it,ts){
    vecteur c,w; factors(*it,c,w);
    if (!geo(w)) return undef;
    const gen r=gratio(*it,x);
    if (dep(r) || !is_strictly_greater(1,evalf(abs(r,CTX),1,CTX),CTX)) return undef;
  }
  vecteur ss,ds; EACH(it,ts) ss.push_back(SYM(at_sum,makesequence(*it,x,v[2],v[3])));
  { gen A=v[0],B=1; // (as typed: a sum over one denominator, each part over it)
    if (is(A,at_division) && arg(A).type==_VECT && arg(A)._VECTptr->size()==2){ B=arg(A,1); A=arg(A,0); }
    vecteur an; flat(at_plus,A,an);
    if (an.size()==ts.size()) EACH(it,an) ds.push_back(SYM(at_sum,makesequence(is_one(B)?*it:SYM2(at_division,*it,B),x,v[2],v[3]))); }
  if (ds.size()!=ss.size()) ds=ss;
  S(R_GSPLIT,line3("%0=%1",op,symb_plus(ds)),0);
  vecteur vals;
  ++depth_;
  EACH(it,ds){ const gen r=ssteps(*it,*arg(*it)._VECTptr,x); if (is_undef(r) || r.type==_IDNT || has_op(r,*at_sum)){ --depth_; return undef; } vals.push_back(r); }
  --depth_;
  const gen val=rn(ev(symb_plus(vals)));
  S(R_GADD,line3("%0=%1=%2",op,txt(tplus(vals)),val),0);
  return val;
}
// (-1)^a (-1)^b and like bases c^k/c^k put together, a negative base as (-1)^k |c|^k (an endpoint's term
// (-1)^k (-2)^k/(k 2^k) is 1/k, (-1)^k (-1)^(k+1) is -1); t if nothing goes together
static NOINL gen m1merge(const gen & t,const gen & x){
  vecteur parts[2]; flat(at_prod,_numer(t,CTX),parts[0]); flat(at_prod,_denom(t,CTX),parts[1]);
  vecteur bs,es,o; gen s=0; bool ch=false,ng=false; int nb=0;
  for (int i=0;i<2;++i) EACH(it,parts[i]){
    gen u=*it; gen q=bexp(u); if (i) q=ev(-q);
    if (!dep(q,x) || dep(u,x) || !(u.type==_INT_ || u.type==_FRAC)){ o.push_back(i?symb_inv(*it):*it); continue; }
    if (is_strictly_positive(-u,CTX)){ s=s+q; ++nb; u=ev(-u); if (is_one(u)) continue; }
    size_t j=0; while (j<bs.size() && bs[j]!=u) ++j;
    if (j<bs.size()){ es[j]=es[j]+q; ch=true; } else { bs.push_back(u); es.push_back(q); }
  }
  s=ev(s);
  { gen al,be; const bool even=nb==1 && is_linear_wrt(s,x,al,be,CTX) && al.type==_INT_ && !(al.val&1); // ((-1)^(2n) = 1: not alternating)
    if (nb<2 && !ch && !even) return t; } // (nothing to put together: (-1)^k/k as typed)
  if (!is_zero(s)){ gen al,be; if (!is_linear_wrt(s,x,al,be,CTX) || al.type!=_INT_ || be.type!=_INT_) return t;
    const int A=al.val&1,B=be.val&1; // ((-1)^(2k+1) = -1)
    if (A) o.insert(o.begin(),B?symb_pow(gen(-1),symb_plus(x,1)):symb_pow(gen(-1),x)); else if (B) ng=true; }
  for (size_t j=0;j<bs.size();++j){ const gen q=ev(es[j]); if (!is_zero(q)) o.push_back(is_one(q)?bs[j]:symb_pow(bs[j],q)); }
  EACHW(it,o){ gen g=*it; const bool in=g.is_inv(); if (in) g=arg(g); // (n^(2/3): giac keeps it as (n^(1/3))^2)
    if (g.is_pow() && arg(g,0).is_pow() && arg(arg(g,0),1).type==_FRAC){ g=symb_pow(arg(arg(g,0),0),ev(arg(g,1)*arg(arg(g,0),1))); *it=in?symb_inv(g):g; } }
  const gen r=o.empty()?gen(1):prodof(o);
  return ng?symb_neg(r):r; // (-1/(n+1)*(1/2)^(n+1), the sign in front)
}
// t typed as f(n)-f(n+k), k up to 3 (no partial fractions): telescoping before the ratio test
// (2^(1/n)-2^(1/(n+1)): the ratio test's limit took 18 s on the PC, minutes on the calculator)
static NOINL bool tpair(const gen & t,const gen & x){
  if (!t.is_plus() || arg(t).type!=_VECT || arg(t)._VECTptr->size()!=2) return false;
  const gen & u=arg(t,0),& w=arg(t,1);
  for (int j=1;j<=3;++j) if (is_zero(ev(u+sub(w,x,x-j))) || is_zero(ev(w+sub(u,x,x-j)))) return true;
  return false;
}
// f(n)-f(n+k), k up to 3, as typed or in partial fractions (a log of a quotient apart): telescoping;
// s the form whose middle terms cancel, SN its partial sum to N (the first terms less the last); u0,
// w0, m0: s as u(k) + w(k), w(k) = -u(k+m)
static NOINL bool telesc(const gen & t,const gen & x,const gen & a,const gen & N,gen & s,gen & SN,gen * u0=0,gen * w0=0,int * m0=0){
  const gen pf=shape(t,SH_FRAC)?pfrac(t):t;
  s=t.is_plus()?t:pf;
  if (is(t,at_ln)) s=symb_plus(symb_ln(_numer(arg(t),CTX)),symb_neg(symb_ln(_denom(arg(t),CTX)))); // ln(n/(n+1))
  if (!s.is_plus() || arg(s).type!=_VECT || arg(s)._VECTptr->size()!=2) return false;
  gen u=arg(s,0),w=arg(s,1);
  nn_=&x;
  for (int i=0;i<6;++i,std::swap(u,w)) if (same(linn(sub(u,x,x+1+i/2)),linn(-w))){ // (ln(k+1+1+1) is ln(k+3): giac leaves function arguments)
    if (u0){ *u0=u; *w0=w; *m0=1+i/2; }
    vecteur q;
    for (int j=0;j<=i/2;++j){ gen y=sub(u,x,a+j); if (!is_zero(y)) q.push_back(y); } // (ln(1))
    nn_=&N;
    for (int j=1;j<=1+i/2;++j){ gen y=linn(subst(u,x,N+j,true,CTX)); q.push_back(y.is_neg()?arg(y):symb_neg(y)); } // (atan: no --)
    gen k=0;
    vecteur q2;
    EACH(it,q) if (it->type<_IDNT || it->type==_FRAC) k=k+*it; else q2.push_back(*it); // (2+1: 3)
    if (!is_zero(k)) q2.insert(q2.begin(),k);
    SN=q2.size()==1?q2[0]:symb_plus(q2);
    return true;
  }
  return false;
}
// a and b equal as decimals (exp(3) and exp(1)^3: different to ratnormal)
static NOINL bool close(const gen & a,const gen & b){
  const gen d=evalf(a-b,1,CTX),m=evalf(abs(a,CTX),1,CTX);
  return d.type==_DOUBLE_ && m.type==_DOUBLE_ && fabs(d._DOUBLE_val)<=1e-7*(1+m._DOUBLE_val);
}
// sum(t,k,a,n), n a name (the course's s_n by the definition of convergence): telescoping, its
// explicit formula, checked against the sum's terms added at n = a+5
static NOINL gen sfin(const gen & op,const vecteur & v,const gen & x){
  const gen a=ev(v[2]),n=ev(v[3]),t=ev(v[0]);
  if (a.type==_INT_ && n.type==_INT_ && n.val>=a.val && n.val-a.val<12){ // a few terms: written out (the course's sum(3,k,3,6))
    vecteur w;
    for (int j=a.val;j<=n.val;++j){ gen y=ev(sub(t,x,j)); if (!has_op(y,*at_plus)) y=rn(y); if (y.type==_DOUBLE_?y._DOUBLE_val!=0:!is_zero(y)) w.push_back(y); } // ((x-1)^3 kept: the course's Taylor polynomials; (3x^3)^2 is 9x^6; 2e-7 is no 0)
    const gen S0=w.empty()?gen(0):w.size()==1?w[0]:symb_plus(w),val=lidnt(S0).empty()?rn(ev(S0)):S0;
    const int ni=lidnt(t).size();
    if (ni<=2 && n.val-a.val<(ni==1?6:3)){ const gen t1=qshow(v[0]),o1=SYM(at_sum,makesequence(t1,x,v[2],v[3])); vecteur q; for (int j=a.val;j<=n.val;++j) q.push_back(subst(t1,x,j<0?txt("("+pr(gen(j))+")"):gen(j),true,CTX)); S(R_PSUMW,o1,txt(wsum(q,false))); S(R_EQV,o1,S0); } // (each term one fraction: no (a/b)/c) // (36/1^2+36/2^2+36/3^2, then 36+9+4; (-1)^(1+1), not (-1)^((1)+1); with x: 3 terms)
    else S(R_PSUMW,op,S0);
    if (val!=S0) S(R_EQV,op,val);
    return val;
  }
  if (n.type!=_IDNT || n==x || !dep(n,n)) return undef;
  gen s,SN,tu,tw;
  int tm=0;
  if (!telesc(t,x,a,n,s,SN,&tu,&tw,&tm)) return undef;
  gen d=0;
  for (int j=0;j<=5;++j) d=d+sub(t,x,a+j);
  if (!close(sub(SN,n,a+5),d)) return undef;
  if (s!=t){ if (is(t,at_ln) || !pfshow(v[0],x)) S(is(t,at_ln)?R_LNQ:R_PFRAC,v[0],s); }
  const gen & src=s!=t?s:v[0];
  const bool two=src.is_plus() && arg(src).type==_VECT && arg(src)._VECTptr->size()==2;
  if (tlines(two?arg(src,0):tu,two?arg(src,1):tw,tu,tw,tm,x,a,n).empty()) S(R_PSUMT,SYM(at_sum,makesequence(src,x,a,n)),SN);
  return SN;
}
// sum(t,n,a,inf): the divergence test, telescoping, a p-series, a geometric series, the partial
// sums' limit, else a limit comparison with a p-series (convergence only)
static NOINL gen scenter(const gen & s);
static NOINL std::string wsum(const vecteur & w,bool dots);
// the divergence test's line: lim a_k = lim (its dominant terms) = L != 0, as the course writes it;
// k!/(k^2+4^k): the growth rates' steps first (lim k!/4^k = inf, 4^k << k!), then the test
static NOINL void divt(const gen & v0,const gen & x,const gen & L){
  const gen fe=ev(v0),nu=_numer(fe,CTX),de=_denom(fe,CTX);
  gen q=0;
  gen a=plus_inf; gen * la=la_; const int ld=ldir_; la_=&a; ldir_=0; // (dom's limits at +infinity: la_ pointed at a point gone with its limit, a crash for n^2(1-cos(1/n)))
  if (de.is_plus() || nu.is_plus()){
    q=quo(dom(nu),dom(de));
    if (pr(q)==pr(fe)) q=0;
    else if (dep(rn(ev(q)),x)){
      budget_=80;
      const size_t m=mark(); const gen r=lsteps(v0,0);
      if (r!=L) cut(m);
      q=0;
    }
  }
  la_=la; ldir_=ld;
  S(R_SDIV2,line3(is_zero(q)?"%0=%2!=0":"%0=%1=%2!=0",Linf(v0,x),Linf(q,x),L),L);
}
// a series with its center put in: some terms written out (0^0 = 1, 0^2 = 0), the first left
static NOINL gen sctr(const gen & op,const vecteur & v,const gen & x){
  const gen r=scenter(op),k0=ev(v[2]);
  if (is_undef(r) || k0.type!=_INT_) return undef;
  vecteur w,z;
  for (int j=k0.val;j<k0.val+3;++j){ w.push_back(ifold(subst(v[0],x,j,true,CTX))); z.push_back(j==k0.val?r:gen(0)); }
  S(R_SCTR,line3("%0=%1",op,txt(wsum(w,true))),0);
  S(R_SCTR2,line3("%0=%1",txt(wsum(z,true)),r),0);
  return r;
}
#ifdef HOSTGIAC_TT
double host_ms(); // (tools/hostgiac/khicas_post.cc: time.h clashes with giac's clock macro here)
static void tt(const char * w){ static double t0; const double m=host_ms(); if (getenv("STEPS_TT")) fprintf(stderr,"   {%s %.2f}\n",w,m-t0); t0=m; }
#else
#define tt(w)
#endif
// a factorial over another one of the index a whole number 1 to 6 apart cancelled: n!/(n+2)! is
// 1/((n+1)(n+2)) (72 s on the calculator as typed, its limit comparison through giac's series of n!)
static NOINL gen factc(const gen & t,const gen & x){
  vecteur w,o; flat(at_prod,t,w);
  std::vector<int> u(w.size(),0);
  bool ch=false;
  for (unsigned i=0;i<w.size();++i) if (!u[i] && is(w[i],at_factorial) && dep(w[i],x))
    for (unsigned j=0;j<w.size();++j) if (!u[j] && w[j].is_inv() && is(arg(w[j]),at_factorial)){
      const gen A=arg(w[i]),B=arg(arg(w[j])),k=rn(ev(A-B));
      if (k.type!=_INT_ || !k.val || k.val>6 || k.val<-6) continue;
      vecteur p; const int n=k.val>0?k.val:-k.val; const gen lo=k.val>0?B:A;
      for (int q=1;q<=n;++q) p.push_back(ev(lo+q));
      o.push_back(k.val>0?prodof(p):symb_inv(prodof(p))); u[i]=u[j]=1; ch=true; break;
    }
  if (!ch) return t;
  for (unsigned i=0;i<w.size();++i) if (!u[i]) o.push_back(w[i]);
  return prodof(o);
}
static NOINL gen ssteps(const gen & op,const vecteur & v,const gen & x){
  tt("ssteps");
  if (v.size()!=4 || ev(v[3])!=plus_inf) return undef;
  { const gen r=sctr(op,v,x); if (!is_undef(r)) return r; }
  const gen a=ev(v[2]),N=word(x==word("n")?"N":"n"),m1=pow(gen(-1),x,CTX); // (the course's s_n)
  gen cp=ev(SYM(at_cos,x*cst_pi));
  gen t=ev(v[0]);
  // giac's sum as the reference, but not of n in a power's base and exponent, n!, ln, trig: it
  // simplifies them at length for nothing (43 s of (n/(2n+1))^n on the calculator; the console's
  // answer skips it too, answer.cc power_sum)
  vecteur gw0,gw; flat(at_prod,t,gw0); EACH(it,gw0) if (dep(*it,x)) gw.push_back(*it);
  const bool rt=!is_zero(nexp(t)) && !geo(gw); // (the ratio test's: its value not asked, 15 s of giac's sum on the calculator)
  const gen G=!is_zero(npow(t)) || has_op(t,*at_factorial) || has_op(t,*at_ln) || has_op(t,*at_sin) || has_op(t,*at_cos) || has_op(t,*at_tan) || !is_zero(pvar(t,x)) || quickG_ || rt?op:ev(op); // (a power series: giac's sum took minutes on the calculator for (k^2+1)/8^k (x-1)^(3k), to no value)
  for (int i=0;i<2;++i,cp=SYM(at_cos,symb_prod(cst_pi,x))) if (contains(t,cp)){ S(R_COSPI,cp,m1); t=ev(subst(t,cp,m1,false,CTX)); } // (giac leaves cos(n pi), cos(pi n))
  if (lidnt(t).size()==1){ const gen tm=m1merge(t,x); // (an endpoint's term: (-1)^k (-2)^k/(k 2^k) is 1/k, then its own tests)
    if (tm!=t && pr(tm).size()<pr(v[0]).size()){ S(R_SIMPT,line3("%0=%1",v[0],tm),0); vecteur v2(v); v2[0]=tm; return ssteps(SYM(at_sum,gen(v2,_SEQ__VECT)),v2,x); } }
  if (has_op(t,*at_factorial)){ const gen tf=factc(t,x); // (n!/(n+2)! = 1/((n+1)(n+2)): then its own tests)
    if (tf!=t){ S(R_SIMPT,line3("%0=%1",v[0],tf),0); vecteur v2(v); v2[0]=tf; return ssteps(SYM(at_sum,gen(v2,_SEQ__VECT)),v2,x); } }
  gen L,val,SN;
  tt("G");
  vecteur c,w; factors(t,c,w);
  // the shape first: giac's limit of (n/(2n+1))^n or (-1)^n/sqrt(n) took 10 to 20 s on the calculator
  gen r=0;
  if (geo(w)){ // geometric
    r=gratio(t,x); // (2^n/2^(n+1): ratnormal left it; the exponents, not simplify: 9 s on the calculator for 2^k/6^k+3^k/6^k)
    const gen ar=evalf(abs(r,CTX),1,CTX);
    const bool in_x=ar.type!=_DOUBLE_; // r = x/3 (a power series): it converges for |x/3| < 1
    if (!dep(r) && (in_x || is_strictly_greater(1,ar,CTX))){
      gen t0=ev(sub(t,x,a));
      val=ev(t0/(1-r));
      if (!in_x && !isnum(val)){ // ((2-2/e^2)/e/(1-1/e): 2(e+1)/e^2, the shortest form)
        gen b=normal(val,CTX);
        const gen c[2]={_simplify(b,CTX),_factor(b,CTX)};
        for (int i=0;i<2;++i) if (pr(c[i]).size()<pr(b).size()) b=c[i];
        val=b;
      }
      const gen ga=quickG_?gen(0):gform(v[0],t,x,r,in_x);
      gen I0=undef;
      if (in_x){ // |r| < 1, r = c u^p: the interval (the course's 1/(1+9x^2): -1/3 < x < 1/3)
        S(R_SCONV,SYM2(at_inferieur_strict,SYM(at_abs,r),1),0);
        const gen y=pvar(r,x),q=_factor(r,CTX);
        vecteur rw; flat(at_prod,q,rw);
        gen rc=1,ru=1,rp=1,lo,hi;
        EACH(it,rw) if (dep(*it,y)) ru=ru*(*it); else rc=rc*(*it);
        if (ru.is_pow()){ rp=arg(ru,1); ru=arg(ru,0); }
        if (!is_zero(y) && rp.type==_INT_ && rp.val>0 && pint(y,ev(abs(rc,CTX)),ru,rp,lo,hi)) I0=out_->back().a;
      }
      if (quickG_ && in_x && !is_undef(I0)) return I0; // (the console: its interval, as the course asks; the function it adds up to: the steps' last lines)
      if (!quickG_){ const gen d=is_strictly_positive(-evalf(r,1,CTX),CTX)?symb_plus(1,ev(-r)):symb_plus(1,symb_neg(r)); // (1+3/4, not 1-(-3/4))
        S(R_GSUM,line(is_one(ga)?(is_zero(a)?"%0=1/(%3)":"%0=#2^%4/(%3)"):is_zero(a)?"%0=#1/(%3)":"%0=#1*#2^%4/(%3)",op,ga,r,d,a,zero),0); } // ((1/4)^1/(1-1/4), not 1*(1/4)^1)
      S(R_EQV,op,in_x?rn1(val):val); // (-3/(x-3), as the console writes it)
      return sameG(val,G) || (!is(G,at_sum) && !in_x && close(val,G)) || ((in_x || quickG_ || is(G,at_sum)) && undone(G))?(in_x?rn1(val):val):gen(undef); // (1/(x-1), not 1/(x*(1-1/x))) // (exp(1) and exp(2): ratnormal sees two names) // (giac sums x^n/3^n only with |x| < 3 assumed: the console's is answer.cc's)
    }
    if (!dep(r) && ar.type==_DOUBLE_){ // (-pi/3)^n: |r| >= 1, the terms do not go to 0
      if (!quickG_) gform(v[0],t,x,r,false);
      S(R_SGEOMD,r,0);
      return undone(G)?word("diverges"):gen(undef);
    }
  }
  tt("geo");
  { const gen sp=gsplit(op,v,x); if (!is_undef(sp)) return sp; } // ((2^k+3^k)/6^k: two geometric series, their values added)
  { const gen y=pvar(t,x); if (!is_zero(y)) return psteps(op,v,x,y,G); } // a power series: its interval
  tt("pvar");
  { const gen L0=growlim(t); // (the divergence test first when the growth rates give a limit not 0)
    if (!is_undef(L0) && !is_zero(L0) && (finv(L0) || is_inf(L0))){ if (quickG_) S(R_SDIV2,0,L0); else divt(v[0],x,L0); return undone(G)?word("diverges"):gen(undef); } }
  tt("growlim");
  // b^n, b depending on n: the root test, lim (a_n)^(1/n) = lim b below 1 ((n/(2n+1))^n, 1/ln(n)^n)
  gen u=t.is_inv()?arg(t):t;
  if (!is_inf(G) && u.is_pow() && dep(arg(u,0)) && (arg(u,1)==x || is_zero(eva(arg(u,1),x)))){
    gen b=(arg(u,1)==x)!=t.is_inv()?arg(u,0):symb_inv(arg(u,0));
    L=ev(Linf(b,x));
    if (finv(L) && vs1(abs(L,CTX))<0){ S(R_ROOT,Linf(b,x),L); return cvg(op,G); }
  }
  // (-1)^(n+k) b, b > 0 decreasing to 0 (from a+3 on: ln(n)/n): the alternating series test
  gen al0,be0;
  if (!is_inf(G)) EACH(it,w) if (it->is_pow() && is_minus_one(arg(*it,0)) && is_linear_wrt(arg(*it,1),x,al0,be0,CTX) && al0.type==_INT_ && (al0.val&1)){ // ((-1)^(2n) is 1: no alternating series)
    vecteur o(c);
    EACH(jt,w) if (jt!=it) o.push_back(*jt);
    gen b=ev(prodof(o));
    if (at_(b,a+3)<0) b=evn(b);
    int k=3;
    while (k<12 && at_(b,a+k)>at_(b,a+k+1)) ++k;
    const gen Lb=lim_(b,a);
    if (!is_undef(Lb) && !is_zero(Lb) && (finv(Lb) || is_inf(Lb))){ S(R_SDIV,Linf(b,x),Lb); return undone(G)?word("diverges"):gen(undef); } // (their size: (-1)^n (n^2+1))
    if (k<12 || !is_zero(Lb)) break; // (b not decreasing to 0: the terms' limit below)
    S(R_AST,Linf(b,x),0);
    return cvg(op,G);
  }
  // n! or the index in an exponent: the ratio test first (the course's), its limit by the growth rates
  // (4^(k^2)/(2k)!: giac's limit of the terms took 25 s on the calculator): below 1 it converges, above it diverges
  gen Lq0=undef;
  const bool tp=tpair(t,x) || (t.is_pow() && dep(arg(t,1)) && is_one(growlim(arg(t,0)))) || is(t,at_cos) || is(t,at_sin); // (and 1 to the infinity, (1+1/k)^k: its ratio is 1, 53 ms on the PC, the divergence test says e; cos((2^k+k^10)/3^k): 0.7 s on the PC)
  if (quickG_ && !tp && !is_inf(G) && (has_op(t,*at_factorial) || !is_zero(nexp(t)))){ gen xp; Lq0=rtest(v[0],x,0,xp,true); } // (the console: its pairs, no simplify)
  if (!is_undef(Lq0) && (finv(Lq0) || Lq0==plus_inf) && vs1(Lq0)){
    const bool lo=vs1(Lq0)<0;
    S(lo?R_RATIO:R_RATIOD,0,Lq0);
    if (lo) return cvg(op,G);
    return undone(G)?word("diverges"):gen(undef);
  }
  if (!tp && !is_inf(G) && (has_op(t,*at_factorial) || !is_zero(nexp(t))) && dep(is_zero(r)?r=_simplify(ev(sub(t,x,x+1)/t),CTX):r)){
    const gen q=_factor(r,CTX),qa=is_strictly_positive(evalf(sub(q,x,a+3),1,CTX),CTX)?q:SYM(at_abs,q),Lq=fastlim(qa);
    if ((finv(Lq) || Lq==plus_inf) && vs1(Lq)){
      const bool lo=vs1(Lq)<0;
      if (quickG_) S(lo?R_RATIO:R_RATIOD,0,Lq); else if (!rshow(v[0],x,Lq)){
        S(R_SRAT,ratio_of(v[0],t,x),q);
        S(lo?R_RATIO:R_RATIOD,Linf(qa,x),Lq);
      }
      if (lo) return cvg(op,G);
      return undone(G)?word("diverges"):gen(undef);
    }
  }
  tt("ratio");
  L=lim_(t,a); // (0 at once when the terms fall fast: n!/n^n, 2^n/n!)
  tt("lim_");
  if (!is_zero(L) && (finv(L) || L==plus_inf || L==minus_inf || has_op(L,*at_bounded_function))){ // the terms do not go to 0
    // (+-infinity itself: giac's limit of 1/n! is 1/((+infinity)!), and is_inf looks inside)
    if (has_op(L,*at_bounded_function)) S(R_SDIV,Linf(v[0],x),word("DNE")); // (no raw bounded_function)
    else if (quickG_) S(R_SDIV2,0,L);
    else divt(v[0],x,L);
    return undone(G)?word("diverges"):gen(undef);
  }
  // f(n)-f(n+k), k up to 3, as typed or in partial fractions: telescoping, S_N the first terms
  // less the last (1/(n(n+2)): 1+1/2-1/(N+1)-1/(N+2))
  gen ts,tu,tw;
  int tm=0;
  tt("div");
  bool hi=false; // (a rational term with a denominator of degree 4 or more, 2/(k^9+2k^7+8): no telescoping, no giac sum to N: 20 s on the calculator)
  { const vecteur lv=lvar(t);
    if (lv.size()==1 && lv[0]==x){
      const gen dg=_degree(makesequence(_denom(t,CTX),x),CTX);
      if (dg.type==_INT_ && dg.val>=4){ // (typed as linear factors, 1/(k(k+1)(k+2)(k+3)): it may telescope)
        vecteur w0; flat(at_prod,norm(v[0]),w0);
        hi=false;
        EACH(it,w0){ gen b=*it,al,be; const gen q=bexp(b); if (is_strictly_positive(-q,CTX) && dep(b,x) && !is_linear_wrt(b,x,al,be,CTX)) hi=true; }
      }
    }
  }
  if (!hi && telesc(t,x,a,N,ts,SN,&tu,&tw,&tm)){
    val=ev(Linf(SN,N));
    { vecteur w,q; flat(at_plus,SN,w); EACH(it,w) if (!dep(*it,N)) q.push_back(*it); // (the first terms: 2/e+2/e^2, not giac's one fraction)
      const gen s1=q.empty()?gen(0):ev(symb_plus(q)); if (finv(val) && close(s1,val)) val=s1; }
    if (!is(G,at_sum) && !sameG(val,G) && !close(val,G) && !(is_inf(val) && is_inf(G))) return undef; // (S_N is exact; giac's sum of logs not evaluated)
    if (ts!=t){ if (is(t,at_ln) || quickG_ || !pfshow(v[0],x)) S(is(t,at_ln)?R_LNQ:R_PFRAC,v[0],ts); }
    const gen & src=ts!=t?ts:v[0];
    const bool two=src.is_plus() && arg(src).type==_VECT && arg(src)._VECTptr->size()==2;
    const vecteur c=quickG_?vecteur(1,gen(0)):tlines(two?arg(src,0):tu,two?arg(src,1):tw,tu,tw,tm,x,a,N);
    if (c.empty()){ S(R_PSUMT,SYM(at_sum,makesequence(src,x,a,N)),SN); S(R_SLIM,Linf(SN,N),val); }
    else if (quickG_) S(R_TDEF,0,val); else tlim(c,SN,N,val);
    return is_inf(val)?word("diverges"):val;
  }
  tt("telesc");
  gen e=0;
  bool ps=!w.empty();
  EACH(it,w){ gen b=*it,ei=bexp(b); ps=ps && b==x; e=e+ei; } // c/n^p (1/n^(3/2) is 1/(n*sqrt(n)))
  e=evn(e);
  if (ps && spos(e)) return pser(op,G,e);
  // n! or the index in an exponent: the ratio test, lim |a(n+1)/a(n)| below 1 (2^n/n!, n!/n^n)
  if (!tp && !is_inf(G) && (has_op(t,*at_factorial) || !is_zero(nexp(t))) && dep(is_zero(r)?r=_simplify(ev(sub(t,x,x+1)/t),CTX):r)){
    gen q=_factor(r,CTX),qa=is_strictly_positive(evalf(sub(q,x,a+3),1,CTX),CTX)?q:SYM(at_abs,q);
    L=ev(Linf(qa,x));
    if (finv(L) && vs1(L)<0){
      if (quickG_) S(R_RATIO,0,L); else if (!rshow(v[0],x,L)){
        S(R_SRAT,ratio_of(v[0],t,x),q);
        S(R_RATIO,Linf(qa,x),L);
      }
      return cvg(op,G);
    }
  }
  // the partial sums' closed form (giac's symbolic sum to N, slow: after the tests above)
  const gen pf=hi?t:shape(t,SH_FRAC)?pfrac(t):t;
  const bool trg=!is_zero(otrig(t)); // (sin(n pi/2)/n: giac's sum to N and limits of it ran out the 90 s on the calculator)
  if (has_op(t,*at_ln) || trg) hi=true; // (ln(k^2+3)/k^3: giac's sum to N is no closed form, and slow)
  SN=hi?gen(0):ev(SYM(at_sum,makesequence(t,x,a,N)));
  if (!hi && elementary(SN) && SN.type!=_STRNG){
    SN=rn(SN);
    { gen s5=0; for (int j=0;j<5;++j) s5=s5+sub(t,x,a+j); // (giac's S_N against the first terms added: its sum(sqrt(n^2+1)-n) to N was -N(N+1)/2, the sqrt's part dropped, a wrong -infinity)
      const gen d=evalf_double(sub(SN,N,a+4)-s5,1,CTX),m=evalf_double(s5,1,CTX);
      if (d.type!=_DOUBLE_ || m.type!=_DOUBLE_ || fabs(d._DOUBLE_val)>TOL*(1+fabs(m._DOUBLE_val))) SN=undef; }
  }
  if (!hi && !is_undef(SN) && elementary(SN) && SN.type!=_STRNG){
    val=ev(Linf(SN,N));
    if (is(G,at_sum) || sameG(val,G) || (is_inf(val) && is_inf(G))){ // (S_N is giac's exact sum to N: G left as a sum on purpose)
      const bool sp=pf!=t && pf.is_plus();
      if (sp) S(R_PFRAC,v[0],pf);
      S(sp?R_PSUMT:R_PSUM,SYM(at_sum,makesequence(sp?pf:v[0],x,a,N)),SN);
      S(R_SLIM,Linf(SN,N),val);
      S(R_EQV,op,val);
      return val;
    }
  }
  tt("SN");
  // like 1/n^p: lim t n^p finite and not 0 (a limit comparison: converges or not)
  double gc[5];
  const bool gk=gcls(t,gc) && !gc[0] && !gc[1] && !gc[2]; // (a power of n and of ln(n): e from the classes, no giac limit of ln|t|/ln(n), 6 ms here)
  if (!gk && trg) return undef;
  // else t's first term at infinity, c/n^p, from its series in h = 1/n (1/n-sin(1/n): h^3/6;
  // (sqrt(n+1)-sqrt(n))/n: h^(3/2)/2): giac's limit of ln|t|/ln(n) took 5 s and gave undef, or ran out
  gen es=undef,lc=undef;
  if (!gk){
    const gen h(identificateur("h__")),sr=_series(makesequence(subst(t,x,symb_inv(h),false,CTX),h,0,6),CTX);
    vecteur w; flat(at_plus,sr,w);
    EACH(it,w){ // (each term c h^q: c and q at h = 1, no ratnormal over sqrt(h); checked at h = 1/4: ln(h) in it is no power)
      if (has_op(*it,*at_order_size) || is_zero(*it)) continue;
      const gen d=D(*it,h),c=ev(subst(*it,h,1,false,CTX)),q=ev(subst(d,h,1,false,CTX)/c),q4=evalf_double(subst(h*d/(*it),h,gen(0.25),false,CTX),1,CTX);
      if (!isnum(q) || q4.type!=_DOUBLE_ || fabs(q4._DOUBLE_val-evalf_double(q,1,CTX)._DOUBLE_val)>TOL){ es=undef; break; }
      if (is_undef(es) || is_strictly_greater(es,q,CTX)){ es=q; lc=c; }
    }
    if (!is_undef(lc) && !is_strictly_positive(lc,CTX)) es=undef;
  }
  e=gk?exact(gen(-gc[3]),CTX):!is_undef(es)?es:ev(-Linf(rn(ev(symb_ln(abs(t,CTX))/symb_ln(x))),x));
  tt("e");
  L=!finv(e) || !is_strictly_positive(e,CTX)?gen(0):gk && gc[4]>0?plus_inf:gk && gc[4]<0?gen(0):!is_undef(es)?lc:ev(Linf(t*pow(x,e,CTX),x));
  tt("L");
  size_t m=mark();
  if (finv(L) && spos(L)){
    S(R_LCMP,Linf(SYM2(at_division,v[0],symb_inv(pow(x,e,CTX))),x),L); // a_n over b_n = 1/n^p, as written (a quotient mergek leaves)
    val=pser(op,G,e);
    if (!is_undef(val)) return val;
    cut(m);
  }
  else if (finv(e) && is_strictly_positive(e,CTX) && (is_zero(L) || L==plus_inf)){ // (ln(k^2+3)/k^3: a power of ln between)
    // 1 < p < e with the limit 0: converges (1/k^2 for ln(k^2+3)/k^3); p = e <= 1 with the limit infinity: diverges
    const bool cv=is_strictly_greater(e,1,CTX);
    if (cv || L==plus_inf){
      const gen p=!cv?e:is_strictly_greater(e,2,CTX)?ev(e-1):ev((e+1)/2),L2=cv?ev(Linf(t*pow(x,p,CTX),x)):L;
      if (cv?is_zero(L2):L2==plus_inf){
        S(cv?R_LCMP0:R_LCMPI,Linf(SYM2(at_division,v[0],symb_inv(is_one(p)?x:symb_pow(x,p))),x),L2); // (1/k^(3/2) as written)
        val=pser(op,G,p);
        if (!is_undef(val)) return val;
        cut(m);
      }
    }
  }
  if (quickG_) return undef; // (the console: an integral's steps for "converges" alone took 45 s on the calculator)
  // positive and decreasing (from a+3 on): the integral test, the integral's steps below it
  // (1/(n ln(n)): u = ln(x), +infinity, diverges)
  for (int k=3;k<9;++k){ double y=at_(t,a+k); if (!(y>0 && y>at_(t,a+k+1))) return undef; }
  gen I=SYM(at_integrate,makesequence(v[0],x,a,plus_inf));
  S(R_ITEST,I);
  ++depth_; val=one(I); --depth_;
  if (is_undef(val) || (is_inf(val)?finv(G) && !is(G,at_sum):is_inf(G))){ cut(m); return undef; } // (as giac's sum)
  return is_inf(val)?word("diverges"):cvg(op,G);
}
static NOINL gen ufun(const gen & g); // (below)
static NOINL gen useries(const gen & g); // (below)
static NOINL gen psum(const gen & g); // (below)
// series(f,x,a,n): f's derivatives, their values at a, the Taylor polynomial (giac's without O)
static NOINL gen kpoly(const gen & f0,const gen & x,const gen & a,int n,const gen & G);
static NOINL std::string wsum(const vecteur & w,bool dots);
// a power series sum(c(k)(x-a)^e(k),k,k0,inf) to (x-a)^n: its terms written out (the course's p_n of a
// series f), undef if its terms are not that shape
static NOINL gen spoly(const gen & s,const gen & x,const gen & a,int n){
  if (!is(s,at_sum) || arg(s).type!=_VECT || arg(s)._VECTptr->size()!=4 || arg(s,1).type!=_IDNT) return undef;
  const gen K=arg(s,1),k0=ev(arg(s,2)),T=norm(arg(s,0)),hx=is_zero(a)?x:symb_plus(x,symb_neg(a));
  if (k0.type!=_INT_) return undef;
  vecteur w,p;
  for (int k=k0.val;k<k0.val+60;++k){
    const gen y=ev(sub(T,K,k)),d=ev(_degree(makesequence(sub(y,x,hx+a),x),CTX));
    if (d.type!=_INT_) return undef;
    w.push_back(y);
    if (d.val>n) break;
    if (!is_zero(y)) p.push_back(y);
  }
  S(R_KWOUT,line3("%0=%1",s,txt(wsum(w,true))),0);
  S(R_KWP,line3("p_%0(%1)=%2",gen(n),x,txt(wsum(p,false))),0);
  return p.empty()?gen(0):symb_plus(p);
}
static NOINL vecteur wpoly(const gen & P,const gen & h,const gen & hx,int n); // (below)
// series(integrate(g,t,0,c x),x,0,n): g's series integrated term by term, its terms up to x^n (giac
// integrated sin(t^2) to a Fresnel function first, then took its series: 9 s on the calculator); else
// giac's series
static NOINL gen qser(const gen & op){
  if (is(op,at_series) && arg(op).type==_VECT && arg(op)._VECTptr->size()==4){
    const vecteur & v=*arg(op)._VECTptr;
    const gen f=v[0],x=v[1],n=ev(v[3]);
    gen al,be;
    if (x.type==_IDNT && n.type==_INT_ && n.val>=0 && is_zero(ev(v[2])) && is(f,at_integrate) && arg(f).type==_VECT && arg(f)._VECTptr->size()==4 && arg(f,1).type==_IDNT && arg(f,1)!=x
        && is_zero(ev(arg(f,2))) && is_linear_wrt(arg(f,3),x,al,be,CTX) && is_zero(be) && !is_zero(al)){
      const gen t=arg(f,1),G=ev(SYM(at_series,makesequence(arg(f,0),t,0,n)));
      vecteur w,q; flat(at_plus,G,w);
      EACH(it,w) if (!has_op(*it,*at_order_size)) q.push_back(*it);
      if (q.empty()) return 0;
      const gen I=ev(SYM(at_integrate,makesequence(symb_plus(q),t,0,arg(f,3)))),P=ev(expand(I,CTX));
      const vecteur pn=wpoly(P,x,x,n.val);
      return pn.empty()?gen(0):pn.size()==1?pn[0]:symb_plus(pn);
    }
  }
  return ev(op);
}
static NOINL gen tsteps(const gen & op,const vecteur & v,const gen & x);
// sin(u)^2 and cos(u)^2 by their half angle (sin(x)^2 = 1/2-cos(2x)/2), b^u as e^(u ln b) (2^x): the
// known series' shapes (the book's "use the identity")
static NOINL gen kpre(const gen & g){
  if (g.is_pow() && arg(g,1)==2 && (is(arg(g,0),at_sin) || is(arg(g,0),at_cos))){ const gen c=SYM(at_cos,ev(2*arg(arg(g,0)))); return is(arg(g,0),at_sin)?symb_plus(fraction(1,2),symb_neg(symb_prod(fraction(1,2),c))):symb_plus(fraction(1,2),symb_prod(fraction(1,2),c)); }
  if (g.is_pow() && !dep(arg(g,0)) && dep(arg(g,1)) && arg(g,0)!=symb_exp(1) && is_strictly_positive(arg(g,0),CTX)) return symb_exp(symb_prod(arg(g,1),SYM(at_ln,arg(g,0))));
  return g.type==_SYMB?map_of(g,kpre):g;
}
static NOINL gen tsteps(const gen & op,const vecteur & v,const gen & x){
  if (v.size()<4 || v[3].type!=_INT_ || v[3].val<1) return undef;
  const gen a=ev(v[2]),h=a==0?x:symb_plus(x,symb_neg(a)); // x - a
  const int n=v[3].val;
  { const gen u=psum(v[0]); // (f defined as a sum, or the sum typed in: its terms up to (x-a)^n, written out, not the sum's interval differentiated)
    if (!is_zero(u)){ const gen r=spoly(u,x,a,n); if (!is_undef(r)) return r; } }
  const gen F0=ufun(v[0]);
  const gen G=qser(op); // (giac's terms, an integral's from its integrand's)
  if (n<=40){ const gen P=kpoly(F0,x,a,n,G); if (!is_undef(P)) return P; } // (the course's way: substituted in a known series)
  if (n>8) return undef;
  gen g=ev(v[0]),mono=1;
  int m=0;
  if (is_zero(a)){ // c x^m g(x): g's polynomial up to x^(n-m), then times c x^m (x^2 sqrt(1+2x): not the product's derivatives)
    vecteur w,o; flat(at_prod,norm(v[0]),w);
    EACH(it,w){ gen b=*it; const gen e=bexp(b); if (b==x && e.type==_INT_ && e.val>0) m+=e.val; else if (!dep(*it,x)) mono=mono*(*it); else o.push_back(*it); }
    if (m>0 && m<=n && !o.empty()){ g=ev(prodof(o)); mono=ev(mono*pow(x,m,CTX)); S(R_TLET,line3("g(%0)=%1",x,prodof(o)),0); }
    else { m=0; mono=1; }
  }
  const int n2=n-m;
  vecteur ds(1,g),qs,gt,kp,ts,cv;
  gen P=0;
  for (int k=1;k<=n2 && !interrupted;++k){ // (normal form only when shorter: -1/(x+1)^2)
    gen d=D(ds.back(),x),d2=rn1(d);
    if (taille(d2,200)<taille(d,200)) d=d2;
    S(k>1?R_EQV:R_TDER,Dx(ds.back()),d); ds.push_back(d);
  }
  for (int k=0;k<=n2 && !interrupted;++k){ // f(a) + f'(a) (x-a) + f''(a)/2! (x-a)^2 ...
    gen c=ev(sub(ds[k],x,a)),q=subst(ds[k],x,a,true,CTX);
    if (!k || pr(q)!=pr(c)) S(k?R_EQV:R_TVAL,q,c); // (e^x at 1: no line "e = e", nor "0 = 0")
    cv.push_back(c);
    if (is_zero(c)) continue;
    qs.push_back(k==1 && is_one(c)?h:k?symb_prod(k>1?symb_prod(c,symb_inv(SYM(at_factorial,k))):c,k>1?symb_pow(h,k):h):c); // (x, not 1*x)
    P=P+c/factorial(k)*pow(h,k,CTX);
    // the polynomial as textbooks write it: each coefficient a number, (x-a)^k kept whole (giac's
    // sum distributed it: 1-x+1+(x-1)^2 for 1-(x-1)+(x-1)^2)
    gen e=evq(c,factorial(k)),hk=k>1?symb_pow(h,k):h;
    ts.push_back(!k?e:is_one(e)?hk:spos(-e)?symb_neg(is_minus_one(e)?hk:symb_prod(-e,hk)):symb_prod(e,hk));
  }
  P=ev(P);
  if (cv.size()>1){ std::string cs; // (a_k = f^(k)(c)/k!, each: the course's coefficients, then p_n)
    for (int k=0;k<(int)cv.size();++k){ const std::string K=pr(gen(k)); if (!cs.empty()) cs+=","; cs+="a_"+K+"="; if (k>1 && !is_zero(cv[k])) cs+=(cv[k].type==_FRAC || is_strictly_positive(-cv[k],CTX)?"("+pr(cv[k])+")":pq(cv[k]))+"/("+K+"!)="; cs+=pr(rn(ev(cv[k]/factorial(k)))); } // ((2!), not 2!=: the parser read "2 !=")
    S(R_TCOEF,txt(cs),0); }
  const gen Pn=m?ev(expand(mono*P,CTX)):P;
  flat(at_plus,G,gt);
  EACH(it,gt) if (!has_op(*it,*at_order_size)) kp.push_back(*it);
  if (interrupted || !same(kp.empty()?gen(0):symb_plus(kp),Pn)) return undef;
  const gen QS=qs.size()==1?qs[0]:symb_plus(qs),TS=ts.size()==1?ts[0]:symb_plus(ts); // (one term: no "+14x^4/4!")
  if (m && !qs.empty()){ // g's, written out (g(x) = 1+x+...), then the factor multiplied in: x^2 (1+x+...)
    const gen gw=txt(wsum(ts,true));
    if (pr(QS)!=pr(TS)){ S(R_TAYLOR,QS,TS); S(R_LINE,line3("g(%0)=%1",x,gw),0); }
    else S(R_TAYLOR,line3("g(%0)",x),gw); // (not "1+x = 1+x")
    S(R_KMULT,line3("%0=%1*(%2)",v[0],mono,gw),0);
    S(R_KWP,line3("p_%0(%1)=%2",gen(n),x,txt(wsum(wpoly(Pn,x,x,n),false))),0);
  }
  else if (qs.empty()) S(R_KWP,line3("p_%0(%1)=%2",gen(n),x,0),0); // (every term 0 up to that degree: p_4(x) = 0, not "0 = 0")
  else S(R_TAYLOR,QS,TS);
  return Pn;
}
// ---------------------------------------------------------------- Taylor series from known ones
// The course's way (a calculus course's: the series of 1/(1-u), e^u, sin u and cos u are known, the others
// come from them): 1/(1-u)^2 and 1/(1-u)^3 by differentiating, ln(1+u) and atan(u) by integrating,
// then u = b h^p put in (h = x-a) and c h^m multiplied in; f's general term, where it converges,
// its h^n coefficient (f^(n)(a) = n! c_n: the 95th derivative of 3x^5 cos(x^3) at 0 is -3*95!/30!)
static const char * const KSF[]={"1/(1-u)","exp(u)","sin(u)","cos(u)","1/(1-u)^2","1/(1-u)^3","ln(1+u)","atan(u)"};
static const char * const KST[]={"u^k","u^k/k!","(-1)^k*u^(2*k+1)/(2*k+1)!","(-1)^k*u^(2*k)/(2*k)!",
  "k*u^(k-1)","k*(k+1)/2*u^(k-1)","(-1)^k*u^(k+1)/(k+1)","(-1)^k*u^(2*k+1)/(2*k+1)"}; // (ln(1+u) from k = 0, as the course's practice answers write it)
// f = c h^m F(b h^p) (F = KSF[s.F]); s.k0 the index's first value
struct kser_t { int F; gen c,m,b,p,k0,K,U,g0,ca; }; // (g0: the factor that is F, ca its constant: 1/(3+x) = 1/3 F(-x/3))
// u = b h^p, p > 0 a number
static NOINL bool kmono(const gen & u,const gen & h,gen & b,gen & p){
  if (!dep(u,h)) return false;
  p=rn(ev(h*D(u,h)/u)); // (b h^p: h u'/u is p)
  if (dep(p,h) || !is_strictly_positive(p,CTX)) return false;
  b=rn(ev(u/pow(h,p,CTX)));
  return !dep(b,h) && !is_zero(b);
}
static NOINL bool kmatch(const gen & f,const gen & h,kser_t & s){
  vecteur w; flat(at_prod,f,w);
  s.F=-1; s.c=1; s.m=0; s.ca=1; s.g0=0;
  gen u;
  EACH(it,w){
    gen g=*it,b=g,e=bexp(b);
    if (!dep(g,h)){ s.c=s.c*g; continue; }
    if (b==h && !dep(e,h)){ s.m=s.m+e; continue; } // (h^m, 1/h too: sin(x)/x)
    if (s.F>=0) return false; // (two functions: not one of these shapes)
    s.g0=g;
    if (is(g,at_exp)){ s.F=KS_EXP; u=arg(g); gen c0=ev(sub(u,h,0)); if (!is_zero(c0)){ s.c=s.c*exp(c0,CTX); s.ca=exp(c0,CTX); u=ev(u-c0); } } // (e^(a+u) = e^a e^u)
    else if (is(g,at_sin)){ s.F=KS_SIN; u=arg(g); }
    else if (is(g,at_cos)){ s.F=KS_COS; u=arg(g); }
    else if (is(g,at_atan)){ s.F=KS_ATAN; u=arg(g); }
    else if (is(g,at_ln)){ s.F=KS_LN; u=ev(arg(g)-1); }
    else if (e.type==_INT_ && e.val<0 && e.val>=-3){ // 1/(al+be h^p)^j = al^-j/(1-u)^j, u = -be/al h^p
      const gen al=ev(sub(b,h,0));
      if (is_zero(al)) return false;
      s.F=e.val==-1?KS_GEO:e.val==-2?KS_GEO2:KS_GEO3;
      s.c=s.c*pow(al,e,CTX); s.ca=ev(pow(al,e,CTX));
      u=rn(ev(1-b/al));
    }
    else return false;
  }
  if (s.F<0 || !kmono(u,h,s.b,s.p)) return false;
  s.c=ev(s.c); s.m=ev(s.m); s.k0=s.F==KS_GEO2 || s.F==KS_GEO3?1:0;
  return true;
}
// the known series' term K(k,u) as parsed: its u exponent e (linear in k), the rest q
static NOINL void kparts(const kser_t & s,gen & e,gen & q,const context * ctx){
  const gen T=norm(gen(KST[s.F],ctx)); // (a/b parsed as a division: a*inv(b))
  vecteur w,r; flat(at_prod,T,w);
  e=0;
  EACH(it,w) if (it->is_pow() && arg(*it,0)==s.U) e=arg(*it,1); else if (*it==s.U) e=1; else r.push_back(*it);
  q=prodof(r);
}
// (-1)^a with a reduced mod 2 (3k+1: k+1; 2k: 1): 0 if a is even, 1 for -1
static NOINL gen msign(const gen & a,const gen & k){
  gen al,be;
  if (!is_linear_wrt(a,k,al,be,CTX) || al.type!=_INT_ || be.type!=_INT_) return pow(gen(-1),a,CTX);
  const int A=al.val&1,B=be.val&1;
  if (!A) return gen(B?-1:1);
  const gen b=be.val>=-1 && be.val<=1?be:gen(B); // (k-1 as the power's k-1: the course's (-1)^(k-1))
  return symb_pow(gen(-1),is_zero(b)?k:symb_plus(k,b));
}
// the general term as the course writes it: c (-1)^... |b|^e q h^(p e + m), the signs of q and b^e put
// together; ex: its exponent of h
static NOINL gen kterm(const kser_t & s,const gen & h,gen & ex,const context * ctx){
  gen e,q; kparts(s,e,q,ctx);
  ex=rn(ev(s.p*e+s.m));
  vecteur w,o; flat(at_prod,q,w);
  gen sg=0; // the exponent of -1
  EACH(it,w) if (it->is_pow() && is_minus_one(arg(*it,0))) sg=sg+arg(*it,1); else o.push_back(*it);
  gen ab=s.b,cc=s.c;
  { vecteur o2; EACH(it,o) if (isnum(*it)) cc=ev(cc*(*it)); else o2.push_back(*it); o=o2; } // (k(k+1)/2's 1/2)
  if (is_strictly_positive(-s.b,CTX)){ sg=sg+e; ab=ev(-s.b); }
  gen m=msign(rn(ev(sg)),s.K);
  if (is_minus_one(m)) cc=ev(-cc); // (c (-1)^1)
  if (m.is_pow() && is_strictly_positive(-cc,CTX)){ m=msign(rn(ev(sg+1)),s.K); cc=ev(-cc); } // (-1*(-1)^k: (-1)^(k+1))
  vecteur t; // (the course's order: 5k(-1)^(k-1)4^(k-1)x^(2k-1), (-1)^k/(2k+1)! t^(4k+2))
  if (!is_one(cc)) t.push_back(cc);
  EACH(it,o) if (!is_one(*it) && !it->is_inv()) t.push_back(*it);
  if (m.is_pow()) t.push_back(m);
  if (!is_one(ab)) t.push_back(symb_pow(ab,e));
  EACH(it,o) if (it->is_inv()) t.push_back(*it);
  t.push_back(is_one(ex)?h:symb_pow(h,ex));
  return prodof(t);
}
// g with the numbers worked out but n! and b^e kept when big (95!, 4^1012): the coefficients the
// course writes, -3*95!/30! (giac's integer has 140 digits); a product: its number, the kept
// factors, the kept ones below the bar last
static NOINL gen held(const gen & g){
  if (g.type!=_SYMB) return g;
  if (is(g,at_factorial)){ const gen n=ev(held(arg(g))); return n.type==_INT_ && n.val>12?SYM(at_factorial,n):ev(SYM(at_factorial,n)); }
  if (g.is_pow()){
    const gen b=ev(held(arg(g,0))),e=ev(held(arg(g,1)));
    if (e.type==_INT_ && (b.type==_INT_ || b.type==_FRAC) && !is_one(abs(b,CTX)) && (e.val>12 || e.val<-12)) return symb_pow(b,e);
    return ev(symb_pow(b,e));
  }
  if (g.is_inv() || g.is_prod() || g.is_neg()){
    vecteur w,num,den; flat(at_prod,g,w);
    gen k=1;
    EACH(it,w){
      const bool iv=it->is_inv(),ng=it->is_neg();
      const gen h=held(iv || ng?arg(*it):*it);
      if (ng) k=-k;
      if (isnum(h)) k=iv?ev(k/h):ev(k*h);
      else (iv?den:num).push_back(h);
    }
    EACHW(it,num){ bool c=false; EACHW(jt,den) if (pr(*jt)==pr(*it)){ den.erase(jt); c=true; break; } if (c){ num.erase(it); --it; } } // (2025!/(1012! 2025!): 1/1012!)
    if (num.empty() && den.empty()) return k;
    if (!is_one(k)) num.insert(num.begin(),k);
    const gen n=prodof(num);
    return den.empty()?n:num.empty()?symb_inv(prodof(den)):symb_prod(n,symb_inv(prodof(den)));
  }
  const gen a=map_of(g,held);
  if (has_op(a,*at_factorial) || lop(a,at_pow).size()) return a; // (kept: a sum of them)
  return ev(a);
}
static NOINL gen hprod(const gen & a,const gen & b){ return held(symb_prod(a,b)); }
static NOINL gen ksum(const gen & t,const kser_t & s){ return SYM(at_sum,makesequence(t,s.K,s.k0,plus_inf)); }
// the steps from the known series to f = c h^m F(b h^p) (Fh: f in h, hx: h written in x): the known
// series (or how it comes from 1/(1-u)), u put in, c h^m multiplied in, where it converges; T the
// general term, T0 before c h^m
static NOINL void kshow(const kser_t & s,const gen & h,const gen & Fh,const gen & T,const gen & hx,bool conv,const context * ctx){
  static const unsigned short R0[]={R_KKNOWN,R_KKNOWN,R_KKNOWN,R_KKNOWN,R_KDIFF,R_KDIFF2,R_KINTG,R_KINTG2};
  const gen vk=h==hx?h:X; // (the known series in x, as the course writes them; t in an integral)
  const gen F=norm(gen(KSF[s.F],ctx)),K0=norm(gen(KST[s.F],ctx)); // (1-3x, not 1-(3*x) substituted)
  const gen Fk=subst(F,s.U,vk,true,ctx),Kk=subst(K0,s.U,vk,true,ctx);
  if (s.F==KS_GEO2){ // d/dx of 1/(1-x) = sum x^k
    S(R_KDIFF,line3("1/(1-%0)",vk),line3("sum(%0^%1,%1,0,+infinity)",vk,s.K));
    S(R_LINE,line3("diff(1/(1-%0),%0)=diff(sum(%0^%1,%1,0,+infinity),%0)",vk,s.K),0);
    S(R_LINE,line3("diff(1/(1-%0),%0)=1/(1-%0)^2",vk),0); // (one side a line: the left, then the sum term by term, then its index)
    S(R_KDTERM,line3("diff(sum(%0^%1,%1,0,+infinity),%0)=sum(%1*%0^(%1-1),%1,0,+infinity)",vk,s.K),0);
    S(R_KSHIFT,line3("sum(%1*%0^(%1-1),%1,0,+infinity)=sum(%1*%0^(%1-1),%1,1,+infinity)",vk,s.K),0);
    S(R_EQV,Fk,ksum(Kk,s));
  }
  else if (s.F==KS_LN || s.F==KS_ATAN){ // the integral of 1/(1+x) or 1/(1+x^2)
    const bool l=s.F==KS_LN;
    S(R0[s.F],line3(l?"1/(1+%0)":"1/(1+%0^2)",vk),line3(l?"sum((-1)^%1*%0^%1,%1,0,+infinity)":"sum((-1)^%1*%0^(2*%1),%1,0,+infinity)",vk,s.K));
    // (term by term with its constant, C = 0 from x = 0: the constant shown)
    S(R_LINE,line3(l?"%2=sum((-1)^%1*%0^(%1+1)/(%1+1),%1,0,+infinity)+C":"%2=sum((-1)^%1*%0^(2*%1+1)/(2*%1+1),%1,0,+infinity)+C",vk,s.K,Fk),0);
    S(R_KCZERO,line3(l?"ln(1)=0+C,C=0":"atan(0)=0+C,C=0",vk),0);
    S(R_EQV,Fk,ksum(Kk,s));
  }
  else S(R0[s.F],Fk,ksum(Kk,s));
  const gen hp=is_one(s.p)?hx:symb_pow(hx,s.p),ub=is_one(s.b)?hp:is_minus_one(s.b)?symb_neg(hp):symb_prod(s.b,hp); // (h shown as x-a)
  kser_t s0(s); s0.c=s.ca; s0.m=0;
  gen ex; const gen T0=kterm(s0,h,ex,ctx);
  const bool sh=h!=hx;
  const gen g0=T==T0?Fh:sh?subst(s.g0,h,hx,false,ctx):s.g0,T0x=sh?subst(T0,h,hx,true,ctx):T0; // (atan(2x-4) as typed)
  if (ub!=vk){ // x replaced: the factor in the known form (1/(1+4x^2)^2 = 1/(1-(-4x^2))^2), its series
    const gen Fu=subst(F,s.U,ub,true,ctx);
    S(R_KREPL,line3("%0->%1",vk,s.b.type==_FRAC?ev(ub):ub),0);
    if (!is_one(s.ca) || pr(g0)!=pr(Fu)) S(R_LINE,line3(is_one(s.ca)?"%0=%2":"%0=#1*%2",g0,s.ca,Fu),0); // ((1/3)*1/(1-(-x/3)))
    S(R_LINE,line3("%0=%1",g0,ksum(is_one(s.ca)?subst(K0,s.U,ub,true,ctx):symb_prod(s.ca,subst(K0,s.U,ub,true,ctx)),s)),0);
    S(R_KSIMP,line3("%0=%1",g0,ksum(T0x,s)),0);
  }
  if (T!=T0){ // c h^m multiplied in
    const gen Fx=sh?subst(Fh,h,hx,true,ctx):Fh,mu=sh?subst(ev(s.c/s.ca*pow(h,s.m,ctx)),h,hx,true,ctx):ev(s.c/s.ca*pow(h,s.m,ctx));
    S(R_KMULT,line3("%0=%1*%2",Fx,mu,ksum(T0x,s)),0);
    S(R_LINE,line3("%0=%1",Fx,ksum(sh?subst(T,h,hx,true,ctx):T,s)),0);
  }
  if (!conv) return;
  if (s.F==KS_EXP || s.F==KS_SIN || s.F==KS_COS){ S(R_KALL,line3("-oo<%0<oo",X),0); S(R_PSRADA,txt("R=+infinity"),0); }
  else {
    gen lo,hi;
    S(R_KRAD,SYM2(at_inferieur_strict,SYM(at_abs,ub),1),0);
    pint(X,ev(abs(s.b,CTX)),hx,s.p,lo,hi);
  }
}
// f in h = x-a: a sum of terms c h^m F(b h^p), integrate(g,t,0,x) of one: the known-series data of
// each (false if a term is none of these)
static NOINL bool ksplit(const gen & f,const gen & h,std::vector<kser_t> & v,vecteur & ints){
  vecteur w; flat(at_plus,f,w);
  EACH(it,w){
    kser_t s; s.K=gen("k",CTX); s.U=gen("u",CTX); // (the parser's k and u: the known series' are parsed)
    gen g=*it,in=0;
    gen up=0;
    if (is(g,at_integrate) && arg(g).type==_VECT && arg(g)._VECTptr->size()==4 && is_zero(arg(g,2)) && arg(g,1).type==_IDNT){
      gen c,p; // (term by term from 0 to c h: t^e becomes (c h)^(e+1)/(e+1))
      if (!kmono(arg(g,3),h,c,p) || !is_one(p)) return false;
      in=arg(g,1); up=arg(g,3); g=arg(g,0);
    }
    if (!kmatch(g,is_zero(in)?h:in,s)) return false;
    v.push_back(s); ints.push_back(is_zero(in)?gen(0):gen(makevecteur(in,up),_SEQ__VECT));
  }
  return !v.empty();
}
// a term c t^ex (c without t) integrated from 0 to U: c/(ex+1) U^(ex+1)
static NOINL gen kint(const gen & T,const gen & t,const gen & U,const gen & ex,gen & ex1){
  ex1=rn(ev(ex+1));
  vecteur w,o,d(1,ex1); flat(at_prod,T,w);
  EACH(it,w) if (!dep(*it,t)){ if (it->is_inv()) d.push_back(arg(*it)); else o.push_back(*it); }
  o.push_back(symb_inv(prodof(d)));
  o.push_back(symb_pow(U,ex1));
  return prodof(o);
}
// f0 (in x) about a: h for x-a (x itself at 0; a name no one can type: theirs may be h), hx it as
// shown, f f0 in h, its terms' known-series data
static NOINL bool kprep(const gen & f0,const gen & x,const gen & a,gen & h,gen & hx,gen & f,std::vector<kser_t> & ks,vecteur & in){
  h=is_zero(a)?x:gen(identificateur(" h")); hx=is_zero(a)?x:symb_plus(x,symb_neg(a));
  f=is_zero(a)?f0:sub(f0,x,h+a);
  return ksplit(f,h,ks,in);
}
// one term's series T (ex its power of h), its steps shown (sh: the term as typed): the known
// series, u put in, c h^m in; an integral's term by term
static NOINL gen kone(const kser_t & s,const gen & in,const gen & h,const gen & hx,const gen & sh,bool conv,gen & ex){
  const bool i0=is_zero(in);
  const gen t=i0?h:in[0];
  gen T=kterm(s,t,ex,CTX),ex1;
  kshow(s,t,i0?sh:arg(sh,0),T,i0?hx:t,conv && i0,CTX);
  if (i0) return T;
  { vecteur w,o; flat(at_prod,T,w); gen tp=1; EACH(it,w) if (dep(*it,t)) tp=*it; else o.push_back(*it); // (sum c integrate(t^e))
    S(R_KINT,line3("%0=%1",sh,line("sum(%0*integrate(%1,%2,0,%3),%4,%5,+infinity)",prodof(o),tp,t,in[1],s.K,s.k0)),0); }
  T=kint(T,t,in[1],ex,ex1); ex=ex1;
  S(R_LINE,line3("%0=%1",sh,ksum(T,s)),0);
  return T;
}
// series(f,x,a,inf): f's Taylor series in summation notation (the course's "find the Taylor series"),
// by the known ones; where it converges; undef if f is not one of those shapes
static NOINL gen ufun(const gen & g){ return is(g,at_of) && arg(g).type==_VECT && arg(g)._VECTptr->size()==2 && arg(g,0).type==_IDNT && is_zero(useries(g))?ev(g):g; } // (f(x) of f defined: its expression)
static NOINL gen ksigma(const vecteur & v,const gen & x){
  gen h,hx,f,ex; std::vector<kser_t> ks; vecteur in;
  if (is(v[0],at_derive) && arg(v[0]).type==_VECT && arg(v[0])._VECTptr->size()==2 && arg(v[0],1)==x && is_zero(ev(v[2]))){ // (diff(g,x): g's series, each term differentiated)
    const gen g=ufun(arg(v[0],0));
    if (!kprep(g,x,0,h,hx,f,ks,in) || ks.size()!=1 || !is_zero(in[0])) return undef;
    const gen T=kone(ks[0],0,h,hx,g,false,ex);
    gen c,e; vecteur w,o; flat(at_prod,T,w);
    e=0; EACH(it,w){ gen b=*it; const gen q=bexp(b); if (b==x) e=q; else o.push_back(*it); }
    c=prodof(o);
    const gen e1=rn(ev(e-1)),ee=rn(ev(e)); // (c x^e: c e x^(e-1))
    vecteur tf; if (!is_one(c)) tf.push_back(c); if (!is_one(ee)) tf.push_back(ee); if (!is_zero(e1)) tf.push_back(is_one(e1)?x:symb_pow(x,e1));
    const gen T1=tf.empty()?gen(1):prodof(tf);
    kser_t s1=ks[0];
    if (is_zero(ev(sub(e,s1.K,s1.k0)))) s1.k0=ev(s1.k0+1); // (the constant term's derivative is 0: from the next k)
    S(R_KDTERM,line3("diff(%0,%1)=%2",g,x,ksum(T1,s1)),0);
    return ksum(T1,s1);
  }
  const gen F=kpre(ufun(v[0]));
  if (!has_op(F,*at_integrate) && !has_op(F,*at_sum) && !has_op(F,*at_derive) && lvar(F)==vecteur(1,x)){ // (a polynomial: its own Taylor polynomial, in powers of x-a: 2-x at 1 is 1-(x-1); no expand of an integral: 8 s more on the calculator)
    const gen Fe=ev(F),d=_denom(Fe,CTX);
    if (!dep(d,x)){
      int k=0; for (gen q=Fe;k<30 && !is_zero(q=ev(D(q,x)));) ++k;
      vecteur v2(v); v2[3]=k; tsteps(symbolic(at_series,gen(v2,_SEQ__VECT)),v2,x);
      const gen a=ev(v[2]),hx=is_zero(a)?x:symb_plus(x,symb_neg(a));
      vecteur t; gen q=Fe,fc=1;
      for (int j=0;j<=k;++j){ const gen cj=rn(ev(sub(q,x,a)/fc)); if (!is_zero(cj)){ const gen pw=!j?gen(1):j==1?hx:symb_pow(hx,j); t.push_back(!j?cj:is_one(cj)?pw:is_minus_one(cj)?symb_neg(pw):symb_prod(cj,pw)); } q=ev(D(q,x)); fc=fc*(j+1); }
      return t.empty()?gen(0):t.size()==1?t[0]:symb_plus(t); // (2-x at 1: 1-(x-1), not giac's 1-x+1)
    } }
  if (is(F,at_ln) && dep(arg(F),x)){ // (ln(3+4x), ln(x) at 2: ln(a0) + ln(1 + ...), the known series of the second)
    const gen a=ev(v[2]),a0=ev(sub(arg(F),x,a));
    if (!is_one(a0) && is_strictly_positive(a0,CTX) && lidnt(a0).empty()){
      const gen L1=SYM(at_ln,symb_plus(1,symb_prod(symb_inv(a0),symb_plus(arg(F),symb_neg(a0)))));
      const gen L1n=SYM(at_ln,ev(1+ev(rn((arg(F)-a0)/a0)))); // (1+4x/3)
      vecteur v2(v); v2[0]=L1n;
      const size_t m=mark();
      S(R_LNSPLIT,line3("%0=ln(%1)+%2",F,a0,L1n),0);
      const gen r=ksigma(v2,x);
      if (!is_undef(r)) return symb_plus(SYM(at_ln,a0),r);
      cut(m);
      (void)L1;
    }
  }
  if (!kprep(F,x,ev(v[2]),h,hx,f,ks,in) || ks.size()!=1){
    if (!is_zero(ev(v[2]))) return undef;
    vecteur w,mo; flat(at_plus,ev(expand(F,CTX)),w); // ((1-cos(x))/x^2: 1/x^2-cos(x)/x^2, the monomials apart)
    gen kn=0; int nk=0;
    EACH(it,w){ if (!dep(*it,x) || !dep(rn(ev(x*D(*it,x)/(*it))),x)) mo.push_back(*it); else { kn=*it; ++nk; } } // (c x^p: x f'/f is p)
    if (nk!=1 || mo.empty() || mo.size()>3) return undef;
    ks.clear(); in.clear();
    if (!kprep(kn,x,0,h,hx,f,ks,in) || ks.size()!=1 || !is_zero(in[0])) return undef;
    const size_t m=mark();
    const gen T=kone(ks[0],0,h,hx,kn,false,ex);
    kser_t s1=ks[0];
    vecteur ms(mo);
    for (int i=0;i<3 && !ms.empty();++i){ // (the first terms of the series and the monomials: they cancel)
      const gen t0=rn(ev(sub(T,s1.K,s1.k0)));
      bool hit=false;
      EACHW(it,ms) if (is_zero(rn(ev(t0+*it)))){ ms.erase(it); hit=true; break; }
      if (!hit){ cut(m); return undef; }
      s1.k0=ev(s1.k0+1);
    }
    if (!ms.empty()){ cut(m); return undef; }
    const gen r=ksum(T,s1);
    S(R_KCZERO2,line3("%0=%1",v[0],r),0);
    return r;
  }
  const gen T=kone(ks[0],in[0],h,hx,F,true,ex);
  return ksum(h==x?T:subst(T,h,hx,true,CTX),ks[0]);
}
// f^(n)(a) = n! c_n, c_n the (x-a)^n coefficient of f's series (by the known ones, or f given as
// a series sum(c(k)*(x-a)^e(k),k,k0,inf)): which term has that power, its coefficient, n! times it
static NOINL gen kder(const gen & F,const gen & x,const gen & n,const gen & a,const gen & name){
  gen h,hx,f,T,ex,K,k0,C,u;
  std::vector<kser_t> ks; vecteur in;
  if (is(F,at_sum) && arg(F).type==_VECT && arg(F)._VECTptr->size()==4 && arg(F,1).type==_IDNT && is_inf(arg(F,3))){
    K=arg(F,1); k0=ev(arg(F,2)); hx=is_zero(a)?x:symb_plus(x,symb_neg(a)); T=norm(arg(F,0)); h=x; // (given)
  }
  else {
    if (!kprep(F,x,a,h,hx,f,ks,in) || ks.size()!=1 || !is_zero(in[0])) return undef;
    T=kone(ks[0],0,h,hx,F,false,ex);
    K=ks[0].K; k0=ks[0].k0;
  }
  if (!ypart(T,h,u,ex,C) || (h==x && !is_zero(rn(ev(u-hx))))) return undef; // (T = C (x-a)^ex)
  ex=rn(ev(ex));
  gen al,be;
  if (!is_linear_wrt(ex,K,al,be,CTX) || is_zero(al)) return undef;
  const gen ks0=rn(ev((n-be)/al));
  S(R_KTERM,symb_pow(hx,ex),symb_pow(hx,n)); // (the power n: e(k) = n)
  S(R_NLINE,symb_equal(ex,n),0);
  if (ks0.type!=_INT_ || is_strictly_greater(k0,ks0,CTX)){ S(R_KNONE,line3("%0^%1",hx,n),0); return 0; }
  S(R_NLINE,symb_equal(K,ks0),0);
  const gen Ck=subst(C,K,ks0,true,CTX),c=held(Ck),d=hprod(c,held(SYM(at_factorial,n)));
  S(R_KCOEF,Ck,c);
  S(R_KDER,line3("%2^((%0))(%1)",n,a,name),d); // (f^(2025)(0): the order in parentheses on the screen, not a power)
  return d;
}
// the known term K0 written out at k with u put in; T worked out at k (h shown as hx unless keep);
// a line g = the terms written out, + ... (the helpers keep kpoly small: 12 KB inline)
static NOINL gen kat(const gen & K0,const kser_t & s,int k,const gen & u){
  if (s.F==KS_GEO3) return ifold(subst(symb_prod(gen(k*(k+1)/2),symb_pow(s.U,gen(k-1))),s.U,u,true,CTX)); // (1+3x+6x^2+..., not 2*1/2+2*3*1/2*x)
  return ifold(subst(subst(K0,s.K,k,true,CTX),s.U,u,true,CTX));
}
static NOINL gen kev(const gen & T,const kser_t & s,int k,const gen & h,const gen & hx,bool keep){ const gen y=rn(ev(sub(T,s.K,k))); return keep?y:subst(y,h,hx,true,CTX); }
static NOINL void kln(int r,const char * f,const gen & g,const gen & c,const vecteur & w){ S(r,line3(f,g,c,txt(wsum(w,true))),0); }
static NOINL vecteur wpoly(const gen & P,const gen & h,const gen & hx,int n); // (below)
// a known series' terms up to x^n (and one more), worked out: its written-out line and polynomial
static NOINL gen kterms(const kser_t & s,const gen & h,int n,vecteur & w){
  gen ex; const gen T=kterm(s,h,ex,CTX);
  gen P=0;
  for (int k=s.k0.val;k<s.k0.val+60;++k){
    const bool more=is_strictly_greater(ev(sub(ex,s.K,k)),n,CTX);
    const gen y=rn(ev(sub(T,s.K,k)));
    w.push_back(y); if (!more) P=P+y;
    if (more) break;
  }
  return P;
}
// taylor(f,x=0,n) of a product of two known series (cos(x)/(1-x), the course's "write both out and
// multiply them"): each written out, the product up to x^n; undef if not that shape
static NOINL gen kpoly2(const gen & f0,const gen & x,int n,const gen & G){
  vecteur w,fs,rs; flat(at_prod,f0,w);
  EACH(it,w){ gen b=*it; const gen e=bexp(b); if (dep(*it,x) && !(b==x && !dep(e,x))) fs.push_back(*it); else rs.push_back(*it); }
  if (fs.size()!=2) return undef;
  kser_t s1,s2; s1.K=s2.K=gen("k",CTX); s1.U=s2.U=gen("u",CTX);
  if (!kmatch(symb_prod(prodof(rs),fs[0]),x,s1) || !kmatch(fs[1],x,s2)) return undef;
  const size_t m=mark();
  vecteur w1,w2;
  const gen P1=kterms(s1,x,n,w1),P2=kterms(s2,x,n,w2);
  S(R_KWOUT,line3("%0=%1",ev(symb_prod(prodof(rs),fs[0])),txt(wsum(w1,true))),0);
  S(R_LINE,line3("%0=%1",fs[1],txt(wsum(w2,true))),0);
  const vecteur pn=wpoly(ev(P1*P2),x,x,n);
  S(R_KPMUL,line3("%0=(%1)*(%2)",f0,txt(wsum(w1,true)),txt(wsum(w2,true))),0);
  S(R_LINE,line3("%0=%1",f0,txt(wsum(pn,true))),0);
  const gen P=pn.empty()?gen(0):symb_plus(pn);
  vecteur gt,kp; flat(at_plus,G,gt);
  EACH(it,gt) if (!has_op(*it,*at_order_size)) kp.push_back(*it);
  const gen Q=kp.empty()?gen(0):kp.size()==1?kp[0]:symb_plus(kp);
  if (interrupted || !same(Q,ev(P))){ cut(m); return undef; }
  S(R_KWP,line3("p_%0(%1)=%2",gen(n),x,txt(wsum(pn,false))),0);
  return Q;
}
// f's Taylor polynomial about 0 up to x^D when its terms are the known series' shapes or powers of x
// (e^(x^2)+2cos(x)-3, 4x^2/(1-3x)-4x^2); undef if not
static NOINL gen kpolyq(const gen & f,const gen & x,int D){
  vecteur w; flat(at_plus,ev(f),w);
  gen P=0;
  EACH(it,w){
    if (shape(*it,0)){ P=P+*it; continue; } // (a polynomial term: as it is)
    kser_t s; s.K=gen("k",CTX); s.U=gen("u",CTX);
    if (!kmatch(*it,x,s)) return undef;
    gen ex; const gen T=kterm(s,x,ex,CTX);
    for (int k=s.k0.val;k<s.k0.val+60;++k){ if (is_strictly_greater(ev(sub(ex,s.K,k)),D,CTX)) break; P=P+ev(sub(T,s.K,k)); }
  }
  return ev(expand(P,CTX));
}
// lim n/d at 0, both 0 there: their series' first terms (the course's way for e^(x^2)+2cos(x)-3), the
// limit of the lowest powers' quotient; undef if either is not made of the known series
static NOINL gen lser(const gen & f,const gen & n,const gen & d,int lev){
  const gen Pn=kpolyq(n,X,8),Pd=kpolyq(d,X,8);
  if (is_undef(Pn) || is_undef(Pd) || is_zero(Pn) || is_zero(Pd)) return undef;
  const vecteur wn=wpoly(Pn,X,X,8),wd=wpoly(Pd,X,X,8);
  if (wn.empty() || wd.empty()) return undef;
  const size_t m=mark();
  vecteur ls; // (each known series written out to the lowest power, e^(x^2) = 1+x^2+x^4/2+..., then the sum's: 7/12 x^4+...)
  for (int s=0;s<2;++s){
    const gen & g=s?d:n,& w0=s?wd[0]:wn[0],dg=_degree(makesequence(w0,X),CTX);
    const int D=dg.type==_INT_?dg.val:8;
    vecteur t; flat(at_plus,g,t);
    if (shape(ev(g),0)) continue; // (x^2 below: no series to write)
    if (t.size()>1) EACH(it,t){ const gen u=it->is_neg()?arg(*it):*it; if (shape(ev(u),0)) continue; const gen P=kpolyq(u,X,D); if (!is_undef(P)) ls.push_back(line3("%0=%1",u,txt(wsum(wpoly(P,X,X,D),true)))); } // (cos(x) = 1-x^2/2+..., not -cos(x))
    ls.push_back(line3("%0=%1",g,txt(wsum(vecteur(1,w0),true))));
  }
  for (unsigned i=0;i<ls.size();++i) S(i?R_LINE:R_LSER,ls[i],0);
  const gen q=quo(wn[0],wd[0]);
  S(R_LSERL,Lx(f),Lx(q));
  const gen L=lsteps(rn(ev(q)),lev+1);
  if (is_undef(L)) cut(m);
  return L;
}
// taylor(f,x=a,n) the course's way when each term of f is one of those shapes (4x cos(2x), 6x e^x -
// 4cos(x), integrate(sin(t^2),t,0,2x)): each one's series to (x-a)^n, then added; checked against
// giac's series (G: its terms); undef if not
// terms as one line in their order, "+..." after them (the course's series written out)
static NOINL std::string wsum(const vecteur & w,bool dots){
  std::string s;
  EACH(it,w){ const std::string p=it->is_plus()?"("+pr(*it)+")":pr(*it); const char * c=p.c_str(); if (!*c) continue; if (!s.empty() && *c!='-') s+="+"; s+=c; } // ((0^2+1)+..., a term that is a sum kept whole)
  if (s.empty()) s="0";
  if (dots) s+="+..."; // (the ellipsis: three dots, the math fonts have no glyph for it)
  return s;
}
// c h^j as the course writes it: the number, then (x-a)^j kept whole
static NOINL gen wmono(const gen & c,const gen & hx,int j){
  const gen hk=j==0?gen(1):j==1?hx:symb_pow(hx,j);
  if (!j) return c;
  return is_one(c)?hk:is_minus_one(c)?symb_neg(hk):symb_prod(c,hk);
}
// a polynomial in h as the course writes it, the lowest power first (h shown as hx), up to h^n
static NOINL vecteur wpoly(const gen & P,const gen & h,const gen & hx,int n){
  vecteur w,cs(n+1,gen(0)),t; // (each term c h^j: its c added at j; giac's coeff pulled in 50 KB)
  flat(at_plus,ev(expand(P,CTX)),t);
  EACH(it,t){
    vecteur f,c; flat(at_prod,*it,f);
    int j=0;
    EACH(jt,f){ gen b=*jt; const gen e=bexp(b); if (b==h && e.type==_INT_) j+=e.val; else c.push_back(*jt); }
    if (j>=0 && j<=n) cs[j]=cs[j]+prodof(c);
  }
  for (int j=0;j<=n;++j){ const gen c=rn(ev(cs[j])); if (!is_zero(c)) w.push_back(wmono(c,hx,j)); }
  return w;
}
// taylor(f,x=a,n) the course's way when each term of f is one of those shapes (4x cos(2x), 6x e^x -
// 4cos(x)): each known series written out (cos(x) = 1-x^2/2!+x^4/4!-...), the inside put in for x,
// the terms worked out, the multiplier in, the parts added, the terms up to (x-a)^n; checked
// against giac's series (G: its terms); undef if not (an integral's term: the summation notation)
static NOINL gen kpoly(const gen & f0,const gen & x,const gen & a,int n,const gen & G){
  gen h,hx,f; std::vector<kser_t> ks; vecteur in,ft,fx;
  if (!is_zero(a) && (has_op(f0,*at_sin) || has_op(f0,*at_cos))) return undef; // (sin about pi/2 is cos about 0 to giac: the derivatives, as the course)
  gen poly=0,f1=f0; // (e^(x^2)+2cos(x)-3, 4x^2/(1-3x)-4x^2: the polynomial terms added as they are)
  if (is_zero(a)){ vecteur t,o; flat(at_plus,f0,t); EACH(it,t) if (shape(ev(*it),0)) poly=poly+*it; else o.push_back(*it);
    if (!is_zero(poly) && !o.empty()) f1=o.size()==1?o[0]:symb_plus(o); else poly=0; }
  if (!kprep(f1,x,a,h,hx,f,ks,in)) return is_zero(a)?kpoly2(norm(f0),x,n,G):gen(undef); // (a product of two: both written out, multiplied)
  flat(at_plus,f,ft); flat(at_plus,f1,fx);
  if (fx.size()!=ft.size()) fx=ft; // (the terms shown as typed)
  const size_t m=mark();
  gen P=0,PA=0; // (P: up to h^n; PA: every term shown)
  int top=n;
  const bool many=ks.size()>1 || !is_zero(poly);
  for (unsigned i=0;i<ks.size();++i){
    const kser_t & s=ks[i];
    const bool ig=!is_zero(in[i]); // (integrate(g,t,0,c x): g's series, then term by term)
    const gen t=ig?in[i][0]:h,U=ig?in[i][1]:gen(0),hx_=ig?t:hx,vk=ig?t:h==hx?h:X;
    if (many){ S(R_KFOR,fx[i],0); ++depth_; }
    gen ex; const gen T=kterm(s,t,ex,CTX);
    kser_t s0(s); s0.c=s.ca; s0.m=0;
    gen ex0; const gen T0=kterm(s0,t,ex0,CTX);
    const gen F=norm(gen(KSF[s.F],CTX)),K0=norm(gen(KST[s.F],CTX));
    const gen hp=is_one(s.p)?hx_:symb_pow(hx_,s.p),ub=is_one(s.b)?hp:is_minus_one(s.b)?symb_neg(hp):symb_prod(s.b,hp);
    const gen ei=ig?gen(1):gen(0); // (integrated: one power more)
    int j1=s.k0.val; // (the terms with a power up to n, and one more: the pattern)
    while (j1<s.k0.val+60 && !is_strictly_greater(ev(sub(ex,s.K,j1)+ei),n,CTX)) ++j1;
    vecteur wa,wb,wc,wd,wi,wj;
    gen Pi=0;
    const bool kp=h==x || ig;
    for (int k=s.k0.val;k<=j1;++k){
      wa.push_back(kat(K0,s,k,vk)); wb.push_back(kat(K0,s,k,ub));
      wc.push_back(kev(T0,s,k,h,hx,kp)); wd.push_back(kev(T,s,k,h,hx,kp));
      const gen y=kev(T,s,k,h,hx,true),e=ev(sub(ex,s.K,k));
      if (ig){ // c t^e from 0 to U: c/(e+1) U^(e+1)
        const gen c=rn(ev(y/pow(t,e,CTX))),e1=ev(e+1);
        wi.push_back(symb_prod(ev(c/e1),symb_pow(U,e1)));
        const gen yi=rn(ev(c/e1*pow(U,e1,CTX)));
        wj.push_back(yi); Pi=Pi+(h==x?yi:sub(yi,x,h+a));
      }
      else Pi=Pi+y;
      const gen ee=ev(e+ei);
      if (ee.type==_INT_ && ee.val>top) top=ee.val;
    }
    { static const unsigned short RW[]={R_KWOUT,R_KWOUT,R_KWOUT,R_KWOUT,R_KWD1,R_KWD2,R_KWLN,R_KWAT}; // (ln and atan: from 1/(1+x), not known)
      kln(RW[s.F],"%0=%2",subst(F,s.U,vk,true,CTX),0,wa); }
    const gen gi=ig?arg(fx[i],0):fx[i]; // (the integrand, or the term)
    const gen g0=T==T0?gi:h!=hx && !ig?subst(s.g0,h,hx,false,CTX):s.g0;
    if (ub!=vk){
      S(R_KREPL,line3("%0->%1",vk,s.b.type==_FRAC?ev(ub):ub),0);
      kln(R_LINE,is_one(s.ca)?"%0=%2":"%0=#1*(%2)",g0,s.ca,wb);
    }
    if (ub!=vk || !is_one(s.ca)) kln(R_KSIMP,"%0=%2",g0,0,wc);
    if (T!=T0){
      const gen mu=h!=hx && !ig?subst(ev(s.c/s.ca*pow(h,s.m,CTX)),h,hx,true,CTX):ev(s.c/s.ca*pow(t,s.m,CTX));
      kln(R_KMULT,"%0=%1*(%2)",gi,mu,wc);
      kln(R_LINE,"%0=%2",gi,0,wd);
    }
    if (ig){ kln(R_KINT,"%0=%2",fx[i],0,wi); kln(R_LINE,"%0=%2",fx[i],0,wj); }
    if (many) --depth_;
    PA=PA+Pi;
  }
  PA=PA+poly;
  const vecteur pn=wpoly(PA,h,hx,n);
  if (many) kln(R_KWADD,"%0=%2",f0,0,wpoly(PA,h,hx,top));
  P=pn.empty()?gen(0):symb_plus(pn);
  vecteur gt,kp; flat(at_plus,G,gt);
  EACH(it,gt) if (!has_op(*it,*at_order_size)) kp.push_back(*it);
  const gen Q=kp.empty()?gen(0):kp.size()==1?kp[0]:symb_plus(kp); // (giac's: from the lowest power up)
  if (interrupted || !same(Q,ev(P))){ cut(m); return undef; }
  S(R_KWP,line3("p_%0(%1)=%2",gen(n),x,txt(wsum(pn,false))),0);
  return Q;
}
// One calculus operation, with none inside: its value (undef: no steps)
#ifndef FLOWCE_SERIES // (no series pack: 2.4's steps for sums and Taylor polynomials)
#include "steps_series24.inc"
#endif
static NOINL gen one_(const gen & op){
  const vecteur & v=*arg(op)._VECTptr;
  gen x=v[1]; var_=&x;
  if (is(op,at_limit)){ // the steps' limit must be giac's (no limit: giac says so too)
    if (v.size()<3) return undef;
    gen a=ev(v[2]),L,G;
    la_=&a; ldir_=v.size()>3?(npos(v[3])?1:-1):0;
    budget_=80;
    if (a==plus_inf && !ldir_){ // (a sequence: decided by lgrow, no giac limit: (2^n+n!)/(2n^9+3n^n) took 15 s on the calculator)
      const size_t m=mark();
      trust_=false; L=lsteps(v[0],0);
      if (trust_ && !is_undef(L) && !interrupted) return L;
      if (!is_undef(L) && !interrupted){ // (no giac limit either: the term at 10^6 and 10^7 agrees; ln(k+1)-ln(k) took 3 minutes)
        const gen y1=evalf(sub(v[0],x,1000000),1,CTX),y2=evalf(sub(v[0],x,10000000),1,CTX),Le=evalf(L,1,CTX);
        if (y1.type==_DOUBLE_ && y2.type==_DOUBLE_ && (Le.type==_DOUBLE_?fabs(y2._DOUBLE_val-Le._DOUBLE_val)<1e-3*(1+fabs(Le._DOUBLE_val)) && fabs(y2._DOUBLE_val-Le._DOUBLE_val)<=fabs(y1._DOUBLE_val-Le._DOUBLE_val)+1e-9:
            (L==plus_inf?y2._DOUBLE_val>1e3 && y2._DOUBLE_val>y1._DOUBLE_val:L==minus_inf && y2._DOUBLE_val<-1e3 && y2._DOUBLE_val<y1._DOUBLE_val))) return L;
      }
      cut(m); budget_=80;
    }
    // giac's limit of the problem as typed (of sin(x)/sqrt(1-cos(x)) at 0: "limits are distinct";
    // of sin(x)*(1-cos(x))^(-1/2), the steps' form, sqrt(2)); no limit: the two sides first (|x|/x
    // at 0: L'Hopital was tried for 5 s on the calculator before), then the other ways
    G=ev(depth_?op:*in_);
    if (has_op(G,*at_bounded_function) && (is(v[0],at_sin) || is(v[0],at_cos))){ L=word("DNE"); S(R_LOSC,Lx(v[0]),L); return L; } // sin(1/x) at 0
    L=!ldir_ && (G.type==_STRNG || G==unsigned_inf)?sides(v[0]):gen(undef);
    if (is_undef(L)) L=lsteps(v[0],0);
    if (interrupted || is_undef(L)) return undef;
    if (L.type==_IDNT) return G.type==_STRNG || G==unsigned_inf || is_undef(G)?L:gen(undef);
    return L==G || (finv(L) && finv(G) && (is_zero(evalf(L-G,1,CTX)) || close(L,G) || same_num(L,G,X)))?L:gen(undef); // (x^2/3, abs(x^2)/3)
  }
#ifndef FLOWCE_SERIES // (no series pack: 2.4's steps for sums and Taylor polynomials, steps_series24.inc)
  if (is(op,at_sum)) return s24::ssteps(op,v,x);
  if (is(op,at_series)) return s24::tsteps(op,v,x);
#else
  if (is(op,at_sum)) return v.size()==4 && !is_inf(ev(v[3]))?sfin(op,v,x):ssteps(op,v,x);
  if (is(op,at_series) && v.size()==4 && is_inf(ev(v[3]))){ const gen u=psum(v[0]); if (!is_zero(u)){ if (u!=v[0] && !is(v[0],at_derive) && !is(v[0],at_integrate)) S(R_KMULT,line3("%0=%1",v[0],u),0); return u; } } // (a given series: itself, a multiplier in each term)
  if (is(op,at_series)) return v.size()==4 && is_inf(ev(v[3]))?ksigma(v,x):tsteps(op,v,x);
#endif
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
  if (fin){ // c t^E from a to b, E with k in it: c (b^(E+1) - a^(E+1))/(E+1)
    vecteur w,c; flat(at_prod,norm(f),w);
    gen E=undef;
    EACH(it,w){ if (it->is_pow() && arg(*it,0)==x && !dep(arg(*it,1),x) && !lidnt(arg(*it,1)).empty() && is_undef(E)) E=arg(*it,1); else if (!dep(*it,x)) c.push_back(*it); else { E=undef; break; } }
    if (!is_undef(E) && is_zero(ev(v[2]))){
      const gen C=prodof(c),E1=ev(E+1),r=ev(C*pow(v[3],E1,CTX)/E1);
      if (!is_one(C)) S(R_ICON,line("%0=%1*integrate(%2,%3,0,%4)",op,C,symb_pow(x,E),x,v[3],zero),0);
      S(R_IPOW,line("%0*($1^(%2))/(%2)",C,v[3],E1,zero,zero,zero),0);
      S(R_EQV,op,r);
      return r;
    }
  }
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
    gen val=evf(op); // no elementary antiderivative: the decimal value
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
    val=evs(ev(Lb),ev(La));
  }
  else { // F(b) - F(a) with the numbers put in, F(a) = 0 left out; an antiderivative jumping in
    if (!nojump(F,ev(v[0]),x,a,b)){ cut(m0); return undef; } // [a,b] gives no steps
    gen Qb=subst(F,x,b,true,CTX),Qa=subst(F,x,a,true,CTX);
    S(R_FTC,op,is_zero(Fa)?Qb:symb_plus(Qb,symb_neg(Qa)));
    val=evs(Fb,Fa);
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
  ctx_=contextptr; out_=&v; depth_=0; nsub_=0; var_=&xx; gen la_home=plus_inf; la_=&la_home; ldir_=0; // (no limit point left from a call before: la_ pointed at a gone local)
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
  return symb_plus(w.is_neg()?arg(w):evn(w),symb_neg(arg(h,0)));
}
static NOINL gen subt(const gen & u,const gen & v){ return is_zero(v)?u:symb_plus(u,isnum(v)?evn(v):symb_neg(v)); }
// the sign of h on (a,b), read at 8 points: 1 or -1, 0 if it changes
static NOINL int sgn_on(const gen & h,const gen & a,const gen & b){
  int s=0;
  for (int i=1;i<9;++i){
    const gen t=is_inf(b)?(is_inf(a)?gen(i-5):a+i):is_inf(a)?b-i:a+(b-a)*i/9,d=evf(sub(h,X,t));
    const int k=spos(d)?1:spos(-d)?-1:0;
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
  return pos(nrm(ev(r)),a,b);
}
// the real points where f and g meet, in order
static NOINL vecteur meet(const gen & f,const gen & g){
  const gen z=_solve(makesequence(symb_equal(f,g),X),CTX);
  vecteur r;
  if (z.type==_VECT) EACH(it,*z._VECTptr){ const gen d=evf(*it); if (d.type==_DOUBLE_) r.push_back(*it); }
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
      q=is_zero(ev(q))?zero:pos(q,a,b);
      if (sgn_on(subt(p,q),a,b)<0){ gen s=p; p=q; q=s; }
    }
    else q=pos(subt(f,g),a,b); // the height
  }
  // R^2 with the square of a root taken (sqrt(4-x^2)^2 is 4-x^2: no steps for it). ds over one
  // denominator: sqrt(1+x^2/(4-x^2)) times sqrt(4-x^2) came out 4, not 8pi, in giac's integral
  vecteur cut;
  const gen ds=!is_zero(sq)?sq:SYM(at_sqrt,w2),
    I=!k?pieces(subt(f,g),a,b,cut):k==1?(!along?symb_prod(p,q):is_zero(q)?ev(symb_pow(p,plus_two)):subt(ev(symb_pow(p,plus_two)),ev(symb_pow(q,plus_two)))):k==2?ds:k==3?symb_prod(p,ds):f,
    K=k==1?(along?cst_pi:2*cst_pi):k==3?2*cst_pi:k==4?ev(symb_inv(subt(b,a))):plus_one,
    J=!k?I:symbolic(at_integrate,makesequence(I,v,a,b)),r=is_one(K)?J:symb_prod(K,J); // (an area: its pieces' integrals)
  static const char * const LHS[]={"A","V","L","S","average"};
  lhs=identificateur(LHS[k]);
  if (show){
    if (m.empty()) m=cut; // (where they cross between a and b)
    if (!m.empty()){
      vecteur s;
      EACH(it,m) s.push_back(symb_equal(v,*it));
      S(R_MEET,f,g); S(R_LINE,gen(s,_SEQ__VECT));
    }
    if (!k) S(R_AREA,lhs,r);
    else if (k==4) S(R_AVG,lhs,r);
    else S(k==1?(along?(is_zero(q)?R_DISK:R_WASH):R_SHELL):k==3?R_SURF:param?R_ARCP:R_ARC,
           param?dv[0]:k==2?dv:p,param?dv[1]:k==3?dv:q);
    if (!is_zero(sq)) S(R_SQ,param?w2:symb_plus(1,symb_pow(dv,plus_two)),sq);
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
static NOINL bool dsubst(const gen & g,gen & F,gen & x,gen & n,gen & a); // (below)
#if defined FLOWCE_VECTORS || defined HOSTGIAC
static NOINL gen m3_problem(const gen & input); // (steps_m3.inc)
#endif
gen steps_problem(const gen & input0,GIAC_CONTEXT){
  const gen input=steps_fixvar(input0,contextptr);
#if defined FLOWCE_VECTORS || defined HOSTGIAC
  { ctx_=contextptr; gen x(identificateur("x")); gen * ov=var_; var_=&x; const gen h=m3_problem(input); var_=ov; if (!is_zero(h)) return h; } // (u x v, proj_v u as the course writes them)
#endif
  { gen F,x,n,a; ctx_=contextptr; // f^(n)(a) as typed (deriv_notation made it subst(diff(f(x),x,n),x=a))
    if (dsubst(input,F,x,n,a)) return line3("%2^((%0))(%1)",n,a,is(F,at_of) && arg(F).type==_VECT && arg(F,0).type==_IDNT?arg(F,0):gen(identificateur("f"))); }
  if (input.is_symb_of_sommet(at_implicitdiff) && arg(input).type==_VECT){ // (DeltaCE) the equation
    ctx_=contextptr;
    const gen e=arg(input,0);
    return e.is_symb_of_sommet(at_equal)?SYM2(at_equal,ev(arg(e,0)),ev(arg(e,1))):ev(e);
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
  if (input.is_symb_of_sommet(at_equal) || rel_of(input)){ ctx_=contextptr; if (!lop(input,at_logb).empty()) return el_norm(input); return symbolic(input._SYMBptr->sommet,makesequence(ev(arg(input,0)),ev(arg(input,1)))); } // (DeltaCE: typed alone; a log with its base as typed, log_2(x+1) = 3, not ln(x+1)/ln(2) = 3)
  ctx_=contextptr; raw_=true;
  gen in=input;
  if (is(in,at_of) && arg(in).type==_VECT && arg(in)._VECTptr->size()==2 && arg(in,1).type==_VECT){ // area(x-x^2,...): the arguments too
    vecteur w=*arg(in,1)._VECTptr;
    EACHW(it,w) *it=norm(*it);
    in=SYM2(at_of,arg(in,0),gen(w,arg(in,1).subtype));
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
  return ev(sub(d,X,zero));
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
  const gen t=sign>0?c:SYM(at_neg,c);
  a.push_back(t); b.push_back(t);
  S(r,SYMV(at_plus,a),SYMV(at_plus,b));
}
static NOINL bool eq_linear(gen L,gen R){
  gen Lx=evx(L),Rx=evx(R);
  gen k=lcd(co(Lx,1),co(Lx,0),co(Rx,1),co(Rx,0));
  if (!is_one(k)){ // fractions first
    S(R_ALCD,L,R,k);
    Lx=evxm(k,Lx); Rx=evxm(k,Rx);
    S(R_ACONT,Lx,Rx);
  }
  else if (!written(Lx,L) || !written(Rx,R)) S(R_ADIST,Lx,Rx);
  gen q=ratn(Rx-co(Rx,1)*X-co(Rx,0));
  if (!is_zero(q)){ // x^2+2x = x^2+5: the x^2 terms cancel
    both(R_ASUB,Lx,Rx,q,-1);
    Lx=evxs(Lx,q); Rx=evxs(Rx,q);
    S(R_ACONT,Lx,Rx);
  }
  gen aL=co(Lx,1),bL=co(Lx,0),aR=co(Rx,1),bR=co(Rx,0);
  if (is_zero(aL) && !is_zero(aR)){ // 5 = 3x-1: the x terms on the left
    rel_=flip(rel_); // (5 < 3x-1 is 3x-1 > 5)
    S(R_ASWAP,Rx,Lx);
    gen t=Lx; Lx=Rx; Rx=t; t=aL; aL=aR; aR=t; t=bL; bL=bR; bR=t;
  }
  if (!is_zero(aR)){ // the x terms to the left
    gen t=evm(aR,X);
    if (npos(aR)) both(R_ASUB,Lx,Rx,t,-1); else both(R_AADD,Lx,Rx,evn(t),1);
    aL=evs(aL,aR);
    Lx=ev(aL*X+bL); Rx=bR;
    S(R_ACONT,Lx,Rx);
  }
  if (is_zero(aL)){ S((rel_?is_one(ev(symbolic(*relop(rel_),makesequence(bL,bR)))):is_zero(evs(bR,bL)))?R_ATRUE:R_AFALSE,bL,bR); return true; }
  if (!is_zero(bL)){ // the numbers to the right
    if (npos(bL)) both(R_ASUB,Lx,Rx,bL,-1); else both(R_AADD,Lx,Rx,evn(bL),1);
    Lx=evm(aL,X); Rx=evs(bR,bL);
    S(R_ACONT,Lx,Rx);
  }
  if (!is_one(aL)){
    const bool neg=rel_ && spos(-aL); // (an inequality: by a negative, it flips)
    if (neg) rel_=flip(rel_);
    S(neg?R_NFLIP:R_ADIV,Lx,Rx,aL);
    S(R_ACONT,X,ratq(Rx,aL));
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
  S(r,line(f,a,b,c,d,e,g));
}
static NOINL bool negp(const gen & g){ return pr(g)[0]=='-'; } // (printed with a minus sign: -2, -x+1)
static NOINL void qline(const gen & q,const gen & a,const gen & b){ PL(R_ALINE,"%0=(%1)*(%2)",q,a,b); } // (x^2-1=(x-1)*(x+1))
// x = (n +- sqrt(d))/e with d < 0: no real solution, the complex ones (the console's answer): sqrt(d) as
// i sqrt(-d), the real and the imaginary parts apart, the two solutions (x^2 = -9: x = +-sqrt(-9) = +-3i;
// x^2+2x+5 = 0: x = (-2 +- sqrt(-16))/2 = (-2 +- 4i)/2 = -1 +- 2i)
static NOINL void q_complex(const gen & n,const gen & e,const gen & d){
  const bool z=is_zero(n);
  PL(R_QCPX,z && is_one(e)?"%0=~sqrt(%1)":z?"%0=(~sqrt(%1))/#2":"%0=(%3~sqrt(%1))/#2",X,d,e,n);
  const gen r=nrm(ev(cst_i*SYM(at_sqrt,ev(-d)))); // (4i, 2i sqrt(2): as the answer writes it)
  if (!is_one(e)) PL(R_ALINE,z?"%0=(~%1)/#2":"%0=(%3~%1)/#2",X,r,e,n);
  const gen P=ratq(n,e),Q=ev(cst_i*nrm(ev(SYM(at_sqrt,ev(-d))/ab(e))));
  PL(R_ALINE,is_zero(P)?"%0=~%2":"%0=%1~%2",X,P,Q);
  S(R_QTWO,eva(P,Q),evs(P,Q)); // (as the console lists them: + first)
}
static NOINL bool eq_quadratic(const gen & L,const gen & R){
  gen p=ev(expand(ev(L-R),CTX));
  if (is_strictly_positive(-co(p,2),CTX)) p=ev(expand(ev(R-L),CTX)); // (to the side where x^2 is positive: x^2-3x-2 = 0)
  if (is_zero(co(p,1))){ // x^2 = q: the square root of both sides (1+x^2 = 1.25: x^2 = 0.25, x = +-0.5), not the formula
    const gen q=ev(-co(p,0)/co(p,2));
    if (!(R==q && L==SYM2(at_pow,X,2))) PL(R_QISO,"%0^2=%1",X,q); // (not as typed: sin(x)^2 = 1/4)
    if (is_strictly_positive(-q,CTX)){ PL(R_QSQN,"%0^2=%1<0",X,q); q_complex(0,1,q); return true; }
    if (is_zero(q)){ S(R_QONE,0,0); return true; }
    const gen r=ev(symbolic(at_sqrt,q));
    PL(R_QROOT,"%0=~sqrt(%1)",X,q);
    if (pr(r)!=pr(symbolic(at_sqrt,q))) PL(R_ALINE,"%0=~%1",X,r);
    S(R_QTWO,ev(-r),r);
    return true;
  }
  if (!is_zero(R) || !written(p,L)) S(R_QSTD,p,0);
  gen k=lcd(co(p,2),co(p,1),co(p,0),0);
  if (!is_one(k)){
    S(R_ALCD,p,zero,k);
    p=evxm(k,p);
    S(R_ACONT,p);
  }
  const gen a=co(p,2),b=co(p,1),c=co(p,0),d=ev(b*b-4*a*c);
  S(R_QABC,a,b,c);
  // x = (-(b) +- sqrt(b^2-4ac))/(2a), the values as they are
  PL(R_QFORM,is_zero(b)?"%0=(~sqrt(0^2-4*#1*#2))/(2*#1)":"%0=(-#3~sqrt(#3^2-4*#1*#2))/(2*#1)",X,a,c,b);
  PL(R_QDISC,"#0^2-4*#1*#2=%3",b,a,c,d);
  if (spos(-d)){ S(R_QNONE,d); q_complex(evn(b),evm(2,a),d); return true; }
  const gen n=evn(b),e=evm(2,a),r=ev(SYM(at_sqrt,d)); // r: sqrt(12) is 2 sqrt(3)
  const bool z=is_zero(n); // (b = 0: +-, not 0 +-)
  PL(R_QSIMP,z?"%0=(~sqrt(%1))/#2":"%0=(%3~sqrt(%1))/#2",X,d,e,n);
  if (is_zero(d)){ S(R_QONE,ratq(n,e)); return true; }
  const bool lo=npos(e); // the smaller solution first, as the console lists them
  if (r.type==_INT_ || r.type==_FRAC){ // a perfect square: the two fractions, then their values
    PL(R_ALINE,z?"%0=(~%1)/#2":"%0=(%3~%1)/#2",X,r,e,n);
    const gen m=line(z?"(-%1)/#2":"(%0-%1)/#2",n,r,e,zero,zero,zero),pl=line(z?"(%1)/#2":"(%0+%1)/#2",n,r,e,zero,zero,zero);
    S(R_QTWO,lo?m:pl,lo?pl:m);
    S(R_QTWO2,ratn((n+(lo?-r:r))/e),ratn((n+(lo?r:-r))/e));
  }
  else { // the root simplified (sqrt(24) is 2 sqrt(6)), x = p +- q, then the two
    if (!written(r,SYM(at_sqrt,d))) PL(R_ALINE,z?"%0=(~%1)/#2":"%0=(%3~%1)/#2",X,r,e,n);
    const gen P=ratq(n,e),Q=nrm(ev(ab(r/e)));
    PL(R_ALINE,is_zero(P)?"%0=~%2":"%0=%1~%2",X,P,Q);
    S(R_QTWO,evs(P,Q),eva(P,Q));
  }
  return true;
}
// ---------------------------------------------------------------- factoring (DeltaCE)
// A polynomial in one name or several, factored as a textbook does it: the greatest common factor (the
// number and the lowest power of every name), then each factor by its shape (two terms: squares, cubes;
// three: a perfect square, two numbers, the ac method, the formula's roots; four: grouping; else a
// rational root and a division), each step's result then the whole; the new factors again, until
// nothing factors. X is the main variable: with other names its coefficients are polynomials in them
// (x^2+5xy+6y^2: b = 5y, c = 6y^2). Shown only when it ends where giac's factor does (the console's
// answer): no partial factoring.
static vecteur * F_;   // the factors so far, their product the polynomial
static gen * P0_;      // the polynomial, as the lines show it
static bool alg2_;     // factor(...) itself (not a part of other steps): the SOAP, quadratic form and check lines
static vecteur * T0_;  // its terms as typed (monomials, several names), else none
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
    w.push_back(n>1?SYM2(at_pow,v[i],int(n)):v[i]);
  }
  gen r=w.empty()?plus_one:w.size()==1?w[0]:SYMV(at_prod,w);
  return neg?SYM(at_neg,r):r;
}
static NOINL void whole(){ S(R_ALINE,SYM2(at_equal,*P0_,prodq(*F_))); }
// the free names of g as giac orders them (pi and e are numbers, not names)
static NOINL vecteur fvars(const gen & g){
  vecteur r,ids=lidnt(g);
  EACH(it,ids) if (evf(*it).type!=_DOUBLE_) r.push_back(*it);
  return r;
}
// g has a name besides the main variable: its coefficients in X are polynomials (x^2+5xy+6y^2)
static NOINL bool others(const gen & g){
  const vecteur v=fvars(g);
  EACH(it,v) if (*it!=X) return true;
  return false;
}
static NOINL vecteur coeffs(const gen & q){ gen c=_coeff(makesequence(q,X),CTX); return c.type==_VECT?*c._VECTptr:vecteur(); }
static NOINL bool root_of(const gen & c,int k,gen & r){ // c = r^k: a whole number or fraction, or a monomial (4x^2y^2: 2xy) (k = 3: signed)
  if (c.type==_FRAC){
    gen a,b;
    if (!root_of(c._FRACptr->num,k,a) || !root_of(c._FRACptr->den,k,b)) return false;
    r=evq(a,b);
    return true;
  }
  if (c.type==_SYMB){ // its factors: numbers (multiplied together), names with a power divisible by k
    vecteur fs;
    flat(at_prod,c,fs);
    gen num=1,rt=1,a;
    EACH(it,fs){
      if (isnum(*it)) num=ev(num*(*it));
      else if (it->is_pow() && arg(*it,0).type==_IDNT && arg(*it,1).type==_INT_ && arg(*it,1).val>0 && arg(*it,1).val%k==0)
        rt=ev(rt*(arg(*it,1).val==k?arg(*it,0):SYM2(at_pow,arg(*it,0),arg(*it,1).val/k)));
      else return false;
    }
    if (!root_of(num,k,a)) return false;
    r=evm(a,rt);
    return true;
  }
  if (c.type!=_INT_ || !c.val || (k==2 && c.val<0)) return false;
  const long s=c.val<0?-1:1,v=s*(long)c.val;
  const long t=(long)(k==2?sqrt((double)v):exp(log((double)v)/3))+0.5; // (then checked exactly)
  for (long u=t>0?t-1:0;u<=t+1;++u)
    if ((k==2?u*u:u*u*u)==v){ r=gen((int)(s*u)); return true; }
  return false;
}
static NOINL gen up(int k){ return k==1?X:SYM2(at_pow,X,k); }
static NOINL std::string sg(const gen & c){ // c after other terms: +5, -9/4 ("" for 0)
  std::string s;
  if (is_zero(c)) return s;
  const std::string t=pr(c);
  if (t[0]!='-') s+="+";
  s+=t;
  return s;
}
// two terms: a^2 u^2 - b^2, a^3 u^3 +- b^3 (alone: SOAP, the sign rule of the cubes' second factor)
static NOINL bool two_terms(const gen & q,const vecteur & c,vecteur & rep){
  const int n=c.size()-1;
  gen a,b;
  if (n%2==0 && root_of(c[0],2,a) && root_of(evn(c[n]),2,b)){
    const gen A=ev(nrm(a*up(n/2))),f1=evs(A,b),f2=eva(A,b);
    PL(R_FDSQ,"%0=$1^2-$2^2=(%3)*(%4)",q,A,b,f1,f2);
    rep.push_back(f1); rep.push_back(f2);
    return true;
  }
  if (n%3==0 && root_of(c[0],3,a) && root_of(c[n],3,b)){
    const gen A=ev(nrm(a*up(n/3))),f1=eva(A,b),f2=evx(A*A-A*b+b*b);
    const bool sum=pr(b)[0]!='-'; // (b a monomial: its sign is its first character)
    PL(sum?R_FSUMC:R_FDIFC,sum?"%0=$1^3+$2^3=(%3)*(%4)":"%0=$1^3-$2^3=(%3)*(%4)",q,A,sum?b:evn(b),f1,f2);
    if (alg2_){ // Same sign as the problem in the binomial, Opposite in the middle term, Always Positive last (as f2 prints them)
      gen mid=evn(evm(A,b)),lst=evm(b,b);
      vecteur t;
      flat(at_plus,f2,t);
      EACH(it,t){ if (zn(*it-mid)) mid=*it; else if (zn(*it-lst)) lst=*it; }
      PL(R_FSOAP,"%0,%1,%2",txt(sg(b)),txt(sg(mid)),txt(sg(lst)));
    }
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
// B = b w and C = c w^2 for one monomial w in the names besides X (x^2+5xy+6y^2: w = y), b and c
// whole numbers; w is positive-leading (the sign of B is b's)
static NOINL bool homog(const gen & B,const gen & C,gen & w,gen & b,gen & c){
  vecteur t;
  const gen ic=_icontent(B,CTX);
  if (is_zero(ic)) return false;
  w=ratq(B,ic);
  if (negp(w)) w=ratn(-w);
  flat(at_plus,ev(nrm(w)),t);
  if (t.size()!=1 || is_one(w) || !is_one(_icontent(w,CTX))) return false;
  const vecteur ids=lidnt(w);
  EACH(it,ids) if (*it==X) return false;
  b=ev(ratq(B,w)); c=ev(ratn(C/(w*w)));
  return b.type==_INT_ && c.type==_INT_ && zr(c*w*w-C);
}
// x^4-18x^2-40 is a quadratic in u = x^2 (a name the problem does not use): its line with u
static NOINL void qform(const gen & q,const gen & A,const gen & B,const gen & C,int k){
  const gen U=identificateur("u");
  if (!alg2_ || k<2 || equalposcomp(lidnt(q),U) || ev(U)!=U) return; // (u assigned: not shown)
  PL(R_FQFORM,"%0=%1,%2=%3",q,SYMV(at_plus,makevecteur(evm(A,SYM2(at_pow,U,2)),evm(B,U),C)),U,up(k));
}
// A u^2 + B u + C: a perfect square, two numbers (the ac method when A is not 1), else the roots. With
// several names (x^2+5xy+6y^2) the coefficients are A, b w, c w^2: the same lines with w carried along
static NOINL bool trinomial(const gen & q,const vecteur & c,vecteur & rep){
  const int n=c.size()-1,k=n/2;
  const gen A=c[0],B=c[k],C=c[n],u=up(k);
  gen w=1,b=B,cc=C,ra,rc;
  if (root_of(A,2,ra) && root_of(C,2,rc)){ // (ra u +- rc)^2, rc a monomial in the other names
    const bool pl=zn(ev(B-2*ra*rc));
    if (pl || zn(ev(B+2*ra*rc))){
      const gen f=ev(ra*u+(pl?rc:evn(rc)));
      PL(R_FPSQ,"%0=(%1)^2",q,f);
      rep.push_back(f); rep.push_back(f);
      return true;
    }
  }
  if (others(q) && (A.type!=_INT_ || !homog(B,C,w,b,cc))) return false;
  int mi,ni;
  if (pair_for(A,b,cc,mi,ni)){
    if (mi%A.val && !(ni%A.val)){ int t=mi; mi=ni; ni=t; } // (the first pair's factor has A's: 3x(x+2)+4(x+2))
    const gen M(mi),N(ni);
    if (is_one(A)){ // u^2 + b w u + c w^2 = (u+m w)(u+n w)
      qform(q,A,B,C,k);
      PL(R_FTWO,"#0*#1=%2,#0+#1=%3",M,N,cc,b);
      rep.push_back(ev(u+M*w)); rep.push_back(ev(u+N*w));
      qline(q,rep[0],rep[1]);
      return true;
    }
    // the ac method: A u^2 + m w u + n w u + C = g1 u (bin) + G2 w (bin)
    gen g1=gcd(A,M);
    if (!npos(A)) g1=evn(g1);
    const gen bin=ev(A/g1*u+M/g1*w),G2=ev(N/(A/g1)),gu=evm(g1,u);
    if (!zr(G2*w*bin-N*u*w-C)) return false;
    qform(q,A,B,C,k);
    PL(R_FSPLIT,"#0*#1=%2,#3+#4=%5",A,cc,evm(A,cc),M,N,b);
    S(R_ALINE,SYM2(at_equal,q,SYMV(at_plus,makevecteur(ev(A*up(2*k)),ev(M*u*w),ev(N*u*w),C))));
    PL(R_FGROUP,npos(G2)?"%0=%1*(%2)+%3*(%2)":"%0=%1*(%2)-%3*(%2)",q,gu,bin,evm(ab(G2),w));
    rep.push_back(ev(gu+G2*w)); rep.push_back(bin);
    qline(q,rep[0],rep[1]);
    return true;
  }
  if (k!=1 || !is_one(w)) return false; // (no whole numbers in two names: no formula)
  const gen d=ev(B*B-4*A*C); // no whole numbers: the roots, if real (x^2+9: prime)
  if (!spos(d)) return false;
  const gen r=ev(SYM(at_sqrt,d)),e=evm(2,A),n0=evn(B),P=ratq(n0,e),Q=nrm(ev(ab(r/e)));
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
  rep.push_back(ev(X-nrm(ratn((n0-r)/e)))); rep.push_back(ev(X-nrm(ratn((n0+r)/e))));
  return true;
}
// four terms, any degrees and names: (t1+t2)+(t3+t4) = g1 (bin) + g2 (bin) for the pairings (1,2)(3,4),
// (1,3)(2,4), (1,4)(2,3) in turn, the terms in the order typed (the problem itself, T0_) else as giac
// writes them; g1 is the common factor of the first pair (the sign of its first term), g2 the
// second's, signed so that the two binomials are the same (x^2(x-2)-9(x-2)); a pairing other than the
// first is preceded by the terms rearranged. A common factor 1 is shown as 1*(bin).
static NOINL bool grouping(const gen & q,const vecteur & tm,vecteur & rep){
  vecteur t=tm;
  gen L=ev(nrm(q)); // (the sum as the lines begin it)
  if (T0_ && T0_->size()==4 && F_->size()==1 && q==*P0_){ t=*T0_; L=SYMV(at_plus,t); }
  static const char PR[3][4]={{0,1,2,3},{0,2,1,3},{0,3,1,2}};
  for (int k=0;k<3 && !ctrl_c && !interrupted;++k){
    const gen &a=t[PR[k][0]],&b=t[PR[k][1]],&c=t[PR[k][2]],&d=t[PR[k][3]];
    gen g1=gcd(a,b),h=gcd(c,d);
    if (is_zero(g1) || is_zero(h)) continue;
    if (negp(a)) g1=evn(g1);
    const gen p1=eva(a,b),p2=eva(c,d),bin=evx(ratq(p1,g1)),b2=evx(ratq(p2,h));
    gen g2=h;
    if (!zn(bin-b2)){
      if (!zn(bin+b2)) continue;
      g2=evn(h);
    }
    const bool neg=negp(g2);
    if (k) S(R_FREORD,SYM2(at_equal,L,SYMV(at_plus,makevecteur(a,b,c,d))));
    PL(R_FGROUP,neg?"%0=(%1)+(%2)=%3*(%4)-%5*(%4)":"%0=(%1)+(%2)=%3*(%4)+%5*(%4)",L,p1,p2,g1,bin,neg?evn(g2):g2);
    rep.push_back(eva(g1,g2)); rep.push_back(bin);
    qline(L,rep[0],rep[1]);
    return true;
  }
  return false;
}
// a rational root (giac's factor finds it): its factor, then the quotient
static NOINL bool rational_root(const gen & q,vecteur & rep){
  vecteur fs;
  flat(at_prod,_factor(q,CTX),fs);
  EACH(it,fs){
    const gen g=it->is_symb_of_sommet(at_pow)?it->_SYMBptr->feuille[0]:*it;
    const vecteur cg=coeffs(g);
    if (cg.size()!=2 || (cg[0].type!=_INT_ && cg[0].type!=_FRAC) || (cg[1].type!=_INT_ && cg[1].type!=_FRAC)) continue;
    const gen r=ratn(-cg[1]/cg[0]),quo=ev(_quo(makesequence(q,g,X),CTX));
    PL(R_FROOT,"%0=0",sub(q,X,txt(par(r))));
    qline(q,g,quo);
    rep.push_back(g); rep.push_back(quo);
    return true;
  }
  return false;
}
// the factor at i in F_ factored further; returns how many factors it became
static int ffac(unsigned i,int depth);
static int ffac1(unsigned i,int depth){
  const gen q=(*F_)[i];
  if (depth>6 || ctrl_c || interrupted) return 1;
  vecteur tm,rep; // (the terms of the expanded polynomial: counted here, once)
  flat(at_plus,ev(nrm(q)),tm); // (normal: the sum of its monomials; expand leaves 2x+(3+x)y+6)
  const vecteur c=coeffs(q);
  const int n=c.size()-1,terms=tm.size();
  const bool multi=others(q); // (a name in the coefficients: no rational root; x only there, or in one term: grouping alone)
  if (!((n>=2 && terms==2 && two_terms(q,c,rep))
      || (n>=2 && n%2==0 && !is_zero(c[n]) && ((terms==3 && !is_zero(c[n/2])) || (terms==2 && n==2 && !multi)) && trinomial(q,c,rep))
      || (terms==4 && grouping(q,tm,rep))
      || (!multi && n>=2 && rational_root(q,rep)))) return 1;
  const bool rest=F_->size()>1; // (the polynomial itself: its line was the whole)
  for (unsigned j=F_->size();j-->i+1;) // its copies ((x^2-1)^2) factored alike
    if ((*F_)[j]==q){ F_->erase(F_->begin()+j); F_->insert(F_->begin()+j,rep.begin(),rep.end()); }
  F_->erase(F_->begin()+i);
  F_->insert(F_->begin()+i,rep.begin(),rep.end());
  if (rest){ // (whole line, then the SOAP line of the cubes just factored: it is not part of the whole line's step)
    whole();
    const size_t k=out_->size()-1;
    if (k && (*out_)[k-1].rule==R_FSOAP){ const step_t t=(*out_)[k]; (*out_)[k]=(*out_)[k-1]; (*out_)[k-1]=t; }
  }
  int cnt=0;
  for (unsigned j=0;j<rep.size();++j) cnt+=ffac(i+cnt,depth+1);
  return cnt;
}
// a factor without the main variable (y^2-1 beside x) is an ordinary polynomial in its first name
static int ffac(unsigned i,int depth){
  const gen X0=X;
  const vecteur v=fvars((*F_)[i]);
  if (!v.empty() && !equalposcomp(v,X)) X=v[0];
  const int r=ffac1(i,depth);
  X=X0;
  return r;
}
// the number of non-constant factors of g, giac's factor's form (a power counted by its exponent; a
// factor with any name counts: y, y+2)
static NOINL int nfactors(const gen & g){
  vecteur fs;
  flat(at_prod,g,fs);
  int n=0;
  EACH(it,fs){
    gen b=*it,e=1;
    if (b.is_symb_of_sommet(at_neg)) b=b._SYMBptr->feuille;
    if (b.is_symb_of_sommet(at_pow)){ e=b._SYMBptr->feuille[1]; b=b._SYMBptr->feuille[0]; }
    if (!fvars(b).empty()) n+=e.type==_INT_?e.val:1;
  }
  return n;
}
// p factored with steps (F_: its factors); false if they do not end where giac's factor does. The
// greatest common factor: the number (the gcd of the numerators over the lcm of the denominators,
// negative when the leading coefficient is: with several names that coefficient in X is a polynomial
// and its sign is the sign of its first term as giac writes it expanded, but not when the problem was
// typed with a positive first term: 10-5y+2x-xy: 5(2-y)+x(2-y), not -(...)), times the lowest power of
// every name that is in all the terms (6x^2y+9xy^2: 3xy). The rest goes to ffac, which declines
// (nothing shown) when a coefficient in X is not a number: the steps then fail the check below.
// Already factored: x+1, 7x (one name), x+y+1, x^2+y^2 (several: nothing to take out, as one name's
// x^2+1): one line, the prime's.
static NOINL bool fac_steps(const gen & p,bool typed=false){
  static vecteur * f;
  static gen * p0;
  if (!f){ f=new vecteur; p0=new gen; }
  F_=f; P0_=p0; *p0=p; f->clear();
  const vecteur c=coeffs(p);
  if (c.size()<2) return false;
  const bool multi=others(p);
  int low=c.size()-1;
  bool rat=true; // (whole numbers, fractions, polynomials of them: not 0.5 or sqrt(2))
  for (unsigned j=0;j<c.size();++j){
    if (is_zero(c[j])) continue;
    rat=rat && (polyf(c[j],true) || polyf(evx(c[j]),true));
    low=c.size()-1-j;
  }
  const gen pe=multi?evx(p):p,dn=dnm(pe);
  rat=rat && (dn.type==_INT_ || dn.type==_ZINT);
  gen g=rat?ev(_icontent(evxm(pe,dn),CTX)/dn):plus_one;
  if (!multi && c.size()==2 && (!rat || is_zero(c[1]) || is_one(g))){ S(R_FPRIME2,p); return true; } // (x+1, 7x, 2.5x+5; 7y+14 has a number to take out)
  if (!rat) return false;
  gen L=c[0]; // (the sign rule: a number's, else its first term's)
  if (!isnum(L)){ vecteur t; flat(at_plus,evx(L),t); L=t[0]; }
  if ((isnum(L)?!npos(L):negp(L)) && !(typed && T0_ && pr((*T0_)[0])[0]!='-')) g=evn(g);
  if (!is_one(g)) f->push_back(g);
  if (low>0) f->push_back(up(low));
  if (multi){
    const vecteur ns=fvars(pe);
    EACH(it,ns){
      if (*it==X) continue;
      const gen k=_valuation(makesequence(pe,*it),CTX);
      if (k.type!=_INT_) return false;
      if (k.val>0) f->push_back(k.val>1?SYM2(at_pow,*it,k):*it);
    }
  }
  const gen r=f->empty()?p:multi?ev(nrm(ratn(p/prodof(*f)))):evx(ratn(p/prodof(*f))); // (as the lines show it)
  if (multi && is_one(r)) return false; // (3xy: a monomial)
  f->push_back(r);
  if (f->size()>1) S(R_FGCF,SYM2(at_equal,p,prodq(*f)));
  const size_t m=out_->size();
  ffac(f->size()-1,0);
  const gen gf=_factor(p,CTX);
  if (out_->size()==m && f->size()==1) // prime: x^2+9 (no real roots), x^3+x+1 (the count below says if giac's factor is not p)
    S(!multi && c.size()==3 && spos(ev(4*c[0]*c[2]-c[1]*c[1]))?R_FPRIME:R_FPRIME2,p);
  return !ctrl_c && !interrupted && nfactors(prodof(*f))==nfactors(gf) && zn(prodof(*f)-p); // (normal: sqrt(2)^2 is 2)
}
static NOINL bool factor_steps(const gen & input){
  gen e=ev(arg(input).type==_VECT?arg(input,0):arg(input));
  const gen e0=e;
  const vecteur ids=fvars(e);
  if (ids.empty()) return false;
  X=ids[0]; // (the main variable: the first name as giac orders them; the others are in the coefficients)
  e=evx(e);
  const gen d=ratn(e);
  if (!fvars(dnm(d)).empty() || !polyf(nmr(d))) return false; // (x^2/2-2: a polynomial too)
  vecteur nt,tt,typed; // (the terms of the polynomial, of the problem, and the problem's when each is a monomial)
  flat(at_plus,ev(nrm(e)),nt);
  if (nt.size()<2 && coeffs(e).size()>2) return false; // (6x^2: nothing to explain; 7x is "does not factor further")
  flat(at_plus,e0,tt);
  EACH(it,tt){
    vecteur u;
    flat(at_plus,ev(nrm(*it)),u);
    if (u.size()!=1){ typed.clear(); break; }
    typed.push_back(*it);
  }
  const bool multi=others(e);
  gen shown=e; // (several names: the problem as typed when it is a sum of monomials, else all expanded)
  if (multi) shown=typed.size()==nt.size()?SYMV(at_plus,typed):ev(nrm(e));
  T0_=typed.empty()?0:&typed;
  const size_t m=out_->size();
  alg2_=true;
  bool ok=fac_steps(shown,multi);
  if (!ok && multi && T0_){ cut(m); ok=fac_steps(shown); } // (the typed first term's sign did not lead to giac's answer)
  alg2_=false;
  T0_=0;
  if (ok && out_->size()>m && (*out_)[m].rule!=R_FPRIME && (*out_)[m].rule!=R_FPRIME2) S(R_FCHECK,SYM2(at_equal,prodq(*F_),*P0_)); // (the factors multiplied back)
  return ok && !out_->empty();
}
// degree 3 and more: one side, factored, each factor 0, each solved (a quadratic factor left has
// no real root: the factoring goes as far as the real roots)
static NOINL bool eq_factor(const gen & L,const gen & R){
  gen p=evx(evs(L,R));
  if (!is_zero(R) || !written(p,L)) S(R_QSTD,p);
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
      v+=pr(X); v+="="; v+=pr(nrm(ratn(-cf[1]/cf[0])));
    }
    else none.push_back(f);
  }
  S(R_FZERO,txt(z));
  if (!v.empty()) S(R_FSOLVE,txt(v));
  EACH(it,none) S(R_FNOREAL,*it);
  return true;
}
// a polynomial equation L = R in x (numeric coefficients): its steps by its degree
static NOINL bool eq_poly(const gen & L,const gen & R){
  const gen d=rats(L,R);
  if (dep(dnm(d)) || dep(dnm(ratn(L))) || dep(dnm(ratn(R)))) return false; // (x/(x-1) = 1/(x-1)+2: its sides)
  vecteur k=lvarx(nmr(d),X),ids=lidnt(d);
  if (k.size()>1 || (k.size()==1 && k[0]!=X) || !polyf(nmr(d))) return false; // (not a polynomial)
  EACH(it,ids) if (*it!=X && evf(*it).type!=_DOUBLE_) return false; // (other letters: a x + b = 0; pi is a number)
  const gen deg=k.empty()?zero:degx(nmr(d));
  if (deg==2) return eq_quadratic(L,R);
  if (deg.type==_INT_ && deg.val>2) return eq_factor(L,R);
  if (deg.type==_INT_ && deg.val<2 && L.type!=_VECT) return eq_linear(L,R);
  return false;
}
// the real solutions of the polynomial equation L = R, exact
static NOINL vecteur roots_of(const gen & L,const gen & R){
  vecteur r;
  const gen s=_solve(makesequence(SYM2(at_equal,evs(L,R),zero),X),CTX);
  double d;
  if (s.type==_VECT) EACH(it,*s._VECTptr){ gen z=evf(*it); if (z.type==_DOUBLE_ || z.type==_INT_) r.push_back(nrm(*it)); } // ((3-sqrt(17))/2, not -(-3+sqrt(17))/2)
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
    const gen r=ratn(lcd*(*it)),f=_factor(r,CTX);
    w.push_back(taille(f,100)<=taille(r,100)?f:r);
  }
  return w.size()==1?w.front():SYMV(at_plus,w);
}
// x in a denominator: the excluded values, both sides times the LCD, the polynomial equation, the
// solutions checked against the excluded values
static NOINL bool eq_rational(const gen & L,const gen & R){
  const gen lcd=ratn(_lcm(makesequence(dnm(ratn(L)),dnm(ratn(R))),CTX));
  if (!dep(lcd)) return false;
  const vecteur ex=roots_of(lcd,0);
  if (!ex.empty()) S(R_REXCL,eqseq(ex,at_different));
  const gen lf=_factor(lcd,CTX);
  S(R_RLCD,SYM2(at_equal,SYM2(at_prod,lf,L),SYM2(at_prod,lf,R)));
  const gen Ls=termwise(L,lcd),Rs=termwise(R,lcd);
  S(R_ALINE,SYM2(at_equal,Ls,Rs));
  if (!eq_poly(Ls,Rs)) return false;
  vecteur ok,bad;
  const vecteur c=roots_of(Ls,Rs);
  EACH(it,c) (zr(sub(lcd,X,*it))?bad:ok).push_back(*it);
  if (!ok.empty()) S(R_RKEEP,eqseq(ok,at_equal));
  if (!bad.empty()) S(R_RDROP,eqseq(bad,at_equal));
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
  o=rest.empty()?zero:rest.size()==1?rest.front():SYMV(at_plus,rest);
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
    const gen on=evn(o);
    if (o.type<_IDNT && spos(-o)) both(R_AADD,L,R,on,1); else both(R_ASUB,L,R,o,-1);
    L=evm(c,s); R=rats(R,o);
    S(R_ACONT,L,R);
  }
  if (!is_one(c)){
    S(R_ADIV,L,R,c);
    L=s; R=ratq(R,c);
    S(R_ACONT,L,R);
  }
  const gen u=arg(s,0),R2=ratn(evxm(R,R));
  PL(R_RSQ,"(%0)^2=(%1)^2",L,R);
  S(R_ACONT,u,R2);
  if (nroots(u)+nroots(R2)) return eq_radical(u,R2,Lc,Rc,depth+1); // (a root left: again)
  if (!eq_poly(u,R2)) return false;
  const vecteur cand=roots_of(u,R2);
  int good=0;
  EACH(it,cand){ // both sides of the equation as given, at the candidate
    const gen p=txt(par(*it)),lq=sub(Lc,X,p),rq=sub(Rc,X,p);
    const gen lv=nrm(ev(sub(Lc,X,*it))),rv=nrm(ev(sub(Rc,X,*it)));
    const bool eq=zn(lv-rv);
    good+=eq;
    PL(eq?R_ROK:R_RBAD,"%0=%1,%2=%3,%4=%5",X,*it,lq,lv,rq,rv);
  }
  if (!good && !cand.empty()) S(R_RNONE,eqseq(cand,at_different)); // (x != 9: none left)
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
  return SYM2(at_equal,ev(l),r[n]);
}
static NOINL void yshow(int rule,const row & r){ // (a line going on: its sides, the template A=B)
  const gen e=yeq(r);
  if (rule==R_ACONT) S(rule,arg(e,0),arg(e,1)); else S(rule,e);
}
static NOINL gen yneg(const gen & g){ return negp(g)?txt(par(g)):g; } // (3*(-1))
static NOINL void ycont(int rule,const gen & e){ if (rule==R_ACONT) S(rule,arg(e,0),arg(e,1)); else S(rule,e); }
static NOINL row yscale(const row & r,const gen & m){ row o(r); EACHW(it,o) *it=evm(*it,m); return o; }
static NOINL row ycomb(const row & a,const row & b,int sg){ row o(a); for (unsigned i=0;i<o.size();++i) o[i]=ev(a[i]+sg*b[i]); return o; }
// eliminates unknown k from rows a and b (both have it): the row without it, steps shown
static NOINL row yelim(const row & a,const row & b,int k){
  const gen p=a[k],q=b[k];
  const gen ap=ab(p),aq=ab(q),L=lcm(ap,aq);
  gen m1=evq(L,ap),m2=evq(L,aq);
  const bool same=npos(p)==npos(q);
  if (!is_one(m1) || !is_one(m2)){
    if (!is_one(m1)){ ycont(R_YMUL,SYM2(at_equal,SYM2(at_prod,m1,arg(yeq(a),0)),SYM2(at_prod,m1,yneg(a.back())))); yshow(R_ACONT,yscale(a,m1)); }
    if (!is_one(m2)){ ycont(is_one(m1)?R_YMUL:R_ACONT,SYM2(at_equal,SYM2(at_prod,m2,arg(yeq(b),0)),SYM2(at_prod,m2,yneg(b.back())))); yshow(R_ACONT,yscale(b,m2)); }
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
  val=evq(r[n],r[k]);
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
      const gen w=negp(vals[i]) && !(is_zero(shown_) && is_one(r[i]))?txt(par(vals[i])):vals[i];
      shown_=shown_+(is_one(r[i])?w:is_minus_one(r[i])?SYM(at_neg,w):SYM2(at_prod,r[i],w));
      rest=rest-r[i]*vals[i];
    }
    else { if (k>=0) return false; k=i; shown_=shown_+r[i]*(*yv_)[i]; }
  }
  if (k<0) return false;
  S(R_YBACK,SYM2(at_equal,shown_,r[n]));
  const gen v=evq(rest,r[k]);
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
    const gen L=lcm(ab(a[i]),ab(b[i]));
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
      if (!inf) S(three?R_YNONE3:R_YNONE,SYM2(at_different,zero,o.back()));
      else { gen g=0; EACH(it,R[0]) g=gcd(g,*it); S(three?R_YINF3:R_YINF,yeq(yscale(R[0],ev(inv(g,CTX))))); }
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
    const int q=is_zero(R[0][o_]) || (!is_zero(R[1][o_]) && is_greater(ab(R[0][o_]),ab(R[1][o_]),CTX))?1:0;
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
    if (!inf){ S(R_YNONE3,SYM2(at_different,zero,two[j].back())); return 1; }
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
      c=sub(c,vars[i],zero);
    }
    r[n]=evn(c);
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
    for (int i=0;i<n;++i){ if (is_undef(vals[i])) return false; sol.push_back(SYM2(at_equal,vars[i],vals[i])); }
    S(R_YSOL,gen(sol,_SEQ__VECT));
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
  const gen z=evf(g);
  if (z.type==_DOUBLE_){ d=z._DOUBLE_val; return true; }
  if (z.type==_INT_){ d=z.val; return true; }
  return false;
}
static NOINL bool el_e(const gen & b){ return is(b,at_exp); } // (the base e is exp(1), not evaluated)
static NOINL gen el_pow(const gen & b,const gen & u){ return el_e(b)?SYM(at_exp,u):SYM2(at_pow,b,u); }
static NOINL gen el_log(const gen & b,const gen & u){ return el_e(b)?SYM(at_ln,u):b==10?SYM(at_log10,u):SYM2(at_logb,u,b); }
static NOINL gen el_mul(const gen & m,const gen & u){ return is_one(m)?u:is_minus_one(m)?SYM(at_neg,u):SYM2(at_prod,m,u); }
static NOINL gen el_sum(const vecteur & v){ return v.empty()?zero:v.size()==1?v.front():SYMV(at_plus,v); }
// F's kind: 1 a power b^u (b a positive number, not 1), 2 a log of u (base b), 0 neither
static NOINL int el_kind(const gen & F,gen & b,gen & u){
  double d;
  if (is(F,at_exp)){ b=SYM(at_exp,plus_one); u=arg(F); return 1; }
  if (is(F,at_pow) && !dep(arg(F,0)) && dep(arg(F,1)) && el_num(arg(F,0),d) && d>0 && d!=1){ b=arg(F,0); u=arg(F,1); return 1; }
  if (is(F,at_ln)){ b=SYM(at_exp,plus_one); u=arg(F); return 2; }
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
  if (el_num(k,d) && d<0) return SYM(at_neg,SYM2(at_prod,evn(k),F));
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
    if (el_num(c,d) && d>0) both(R_ASUB,L,R,c,-1); else both(R_AADD,L,R,evn(c),1);
    l=el_term(k,F); r=evs(r,c);
    S(R_ACONT,l,r);
  }
  if (!is_one(k)){
    if (!is_minus_one(k)) S(R_ADIV,l,r,k); // (-2^x = -8: 2^x = 8 at once)
    r=evq(r,k);
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
  if (dep(D(D(evs(l,r),X),X))) return false;
  gen Lx=evx(l),Rx=evx(r);
  gen aL=D(Lx,X),aR=D(Rx,X),bL=ev(sub(Lx,X,zero)),bR=ev(sub(Rx,X,zero));
  const gen k=lcd(aL,bL,aR,bR);
  if (!is_one(k)){ // fractions first (x/2 = e: x = 2e)
    S(R_ALCD,l,r,k);
    Lx=evxm(k,Lx); Rx=evxm(k,Rx);
    S(R_ACONT,Lx,Rx);
    if (Lx==X && !dep(Rx)) return true;
    aL=D(Lx,X); aR=D(Rx,X); bL=ev(sub(Lx,X,zero)); bR=ev(sub(Rx,X,zero));
  }
  else if (!written(Lx,l) || !written(Rx,r)) S(R_ADIST,Lx,Rx);
  const gen a=evs(aL,aR);
  if (is_zero(a)){ S(is_zero(evs(bR,bL))?R_ATRUE:R_AFALSE,bL,bR); return true; }
  gen lx=Lx,rx=Rx;
  if (!is_zero(aR) || !is_zero(bL)){
    vecteur lt,rt;
    lt.push_back(evm(aL,X));
    if (!is_zero(aR)) lt.push_back(ev(-aR*X));
    if (!is_zero(bR)) rt.push_back(bR);
    if (!is_zero(bL)) rt.push_back(evn(bL));
    lx=el_sum(lt); rx=el_sum(rt);
    S(R_GXSIDE,lx,rx);
  }
  if (!is_zero(aR)){ lx=SYM2(at_prod,X,a); S(R_GFACX,lx,rx); }
  if (!is_one(a)) S(R_ADIV,lx,rx,a);
  S(R_ACONT,X,evq(rx,a));
  return true;
}
// b^u = r, r a number
static NOINL bool el_one(const gen & F,const gen & r){
  gen b,u,c,m,n,c2;
  el_kind(F,b,u);
  double d;
  if (!el_num(r,d)) return false;
  if (d<=0){ S(R_GNONE,SYM2(at_equal,F,r)); return true; }
  if (!el_e(b) && el_root(b,c,m) && (is_one(r)?(n=0,true):el_root(r,c2,n) && c2==c)){ // 4^x = 8: 2^(2x) = 2^3
    const gen mu=el_mul(m,u);
    S(R_GSAME,SYM2(at_equal,el_pow(c,mu),el_pow(c,n)));
    S(R_GEXPEQ,SYM2(at_equal,mu,n));
    return el_eq(mu,n);
  }
  const gen lr=SYM(at_ln,r);
  S(R_GLN,SYM2(at_equal,SYM(at_ln,F),lr));
  if (el_e(b)){ // (ln(e^u) is u)
    const gen v=ev(lr);
    S(R_ACONT,u,lr);
    if (u==X && pr(v)!=pr(lr)) S(R_ACONT,X,v); // (ln(1) = 0)
    return el_eq(u,v);
  }
  const gen lb=SYM(at_ln,b),ul=SYM2(at_prod,u,lb);
  S(R_GPOW,SYM2(at_equal,ul,lr));
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
    if (!is_one(m1) || !is_one(m2)) S(R_GSAME,SYM2(at_equal,el_pow(c1,l),el_pow(c1,r)));
    S(R_GEXPEQ,SYM2(at_equal,l,r));
    return el_eq(l,r);
  }
  S(R_GLN,SYM2(at_equal,SYM(at_ln,F1),SYM(at_ln,F2)));
  const gen l=el_e(b1)?u1:SYM2(at_prod,u1,SYM(at_ln,b1)),r=el_e(b2)?u2:SYM2(at_prod,u2,SYM(at_ln,b2));
  S(R_GPOW,SYM2(at_equal,l,r));
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
    else if (m==2){ two=true; Q=Q+K[i]*symb_pow(t,plus_two); }
    else return false;
  }
  if (!one || !two) return false;
  Q=evx(Q);
  const gen F=el_pow(b0,X);
  S(R_GSUBU,SYM2(at_equal,t,F));
  S(R_ACONT,Q);
  const gen x0=X;
  X=t;
  ++depth_;
  const bool ok=eq_poly(Q,0);
  const vecteur r=ok?roots_of(Q,0):vecteur();
  --depth_;
  X=x0;
  if (!ok) return false;
  EACH(it,r){
    S(R_GBACK,SYM2(at_equal,F,*it));
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
    const bool neg=!npos(k);
    const gen ak=neg?evn(k):k,w=is_one(ak)?u:SYM2(at_pow,u,ak),l=el_log(B,w);
    t.push_back(neg?SYM(at_neg,l):l);
    (neg?den:num).push_back(w);
  }
  q=!den.empty();
  const gen N=num.empty()?plus_one:num.size()==1?num[0]:SYMV(at_prod,num);
  const gen M=den.size()==1?den[0]:SYMV(at_prod,den);
  s1=el_sum(t);
  a=q?SYM2(at_prod,N,SYM(at_inv,M)):N;
  s2=el_log(B,a);
  return true;
}
// a = v (a the logs' argument): solved, each solution checked in the logs (us: their arguments)
static NOINL bool el_check(const gen & a,const gen & v,const vecteur & us){
  if (dep(dnm(ratn(a)))?!eq_rational(a,v):!el_eq(a,v)) return false;
  const vecteur cand=roots_of(a,v);
  int good=0;
  EACH(it,cand){
    bool ok=true;
    double d;
    EACH(u,us) if (!el_num(sub(*u,X,*it),d) || d<=0) ok=false;
    good+=ok;
    S(ok?R_GOK:R_GBAD,SYM2(at_equal,X,*it));
  }
  if (!good && !cand.empty()) S(R_RNONE,eqseq(cand,at_different));
  return true;
}
// log_B(a) = c: a = B^c
static NOINL bool el_form(const gen & B,const gen & a,const gen & c,const vecteur & us){
  const gen p=el_pow(B,c),v=ev(p);
  S(R_GEXPF,SYM2(at_equal,a,p));
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
      S(R_QSTD,el_sum(t));
    }
    for (unsigned i=0;i<FR.size();++i){ KL.push_back(evn(KR[i])); FL.push_back(FR[i]); }
    return el_quad(KL,FL,evs(cL,cR));
  }
  gen s1,s2,a,r1,r2,ar;
  bool q,qr;
  if (FR.empty() || !is_zero(cL) || !is_zero(cR)){ // log(x)+log(x-3) = 1: the logs one, then a power
    if (FR.empty() && FL.size()==1){ gen b,u; el_kind(FL[0],b,u); return el_form(B,u,el_iso(L,KL[0],FL[0],cL,R),us); }
    gen v=evs(cR,cL);
    if (!FR.empty()){ // the logs to the left, the numbers to the right
      for (unsigned i=0;i<FR.size();++i){ KL.push_back(evn(KR[i])); FL.push_back(FR[i]); }
      S(R_GMOVE,el_side(KL,FL),v);
    }
    else if (!is_zero(cL)){
      double d;
      if (el_num(cL,d) && d>0) both(R_ASUB,L,R,cL,-1); else both(R_AADD,L,R,evn(cL),1);
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
  S(R_GARG,SYM2(at_equal,a,ar));
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
static NOINL gen tr_plus(const gen & b,const gen & s){ const gen t=evm(s,tr_n()); return is_zero(b)?t:SYM2(at_plus,b,t); } // b + s n
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
    out.insert(out.begin()+j,ratn(b+k*s));
  }
}
// T(u) = k: the angles, every solution, the ones in [0, 2pi) (added to sols)
static NOINL bool tr_basic(const gen & T,const gen & k,vecteur & sols){
  const bool sn=is(T,at_sin),cs=is(T,at_cos);
  const gen u=arg(T),m=D(u,X),c=ev(sub(u,X,zero)),pi=cst_pi;
  double d;
  if (!el_num(k,d) || dep(m) || is_zero(m) || !zr(u-m*X-c)) return false; // (u = m x + c)
  if ((sn || cs) && (d>1+1e-12 || d<-1-1e-12)){ S(R_TRANGE,T,k); return true; }
  gen P=sn || cs?evm(2,pi):pi; // the period
  vecteur th,shown; // the angles of one period from 0 (tan: from 0 to pi)
  const bool zero=fabs(d)<1e-12,one=fabs(fabs(d)-1)<1e-12;
  if (zero || ((sn || cs) && one)){ // on an axis
    if (sn){ if (zero){ th.push_back(0); th.push_back(pi); } else th.push_back(ev(d>0?pi/2:3*pi/2)); }
    else if (cs){ if (zero){ th.push_back(evq(pi,plus_two)); th.push_back(ev(3*pi/2)); } else th.push_back(d>0?gen(0):pi); }
    else th.push_back(0);
    S(R_TAXIS,tr_eqs(u,th));
    shown=th;
  }
  else {
    const unary_function_ptr * f=sn?at_asin:cs?at_acos:at_atan;
    const gen ak=d<0?evn(k):k,a=ratn(ev(SYM(f,ak))); // (pi/3, not 1/3*pi)
    S(R_TREF,SYM(f,ak),a);
    gen t1,t2;
    int r;
    if (sn){ r=d>0?R_TQS1:R_TQS2; t1=d>0?a:SYM2(at_plus,pi,a); t2=SYM2(at_plus,d>0?pi:evm(2,pi),SYM(at_neg,a)); }
    else if (cs){ r=d>0?R_TQC1:R_TQC2; t1=d>0?a:SYM2(at_plus,pi,SYM(at_neg,a)); t2=d>0?SYM2(at_plus,evm(2,pi),SYM(at_neg,a)):SYM2(at_plus,pi,a); }
    else { r=d>0?R_TQT1:R_TQT2; t1=d>0?a:SYM2(at_plus,pi,SYM(at_neg,a)); t2=d>0?SYM2(at_plus,pi,a):SYM2(at_plus,evm(2,pi),SYM(at_neg,a)); }
    vecteur raw; raw.push_back(t1); raw.push_back(t2);
    shown.push_back(ratn(t1)); shown.push_back(ratn(t2));
    S(r,tr_eqs(u,raw));
    if (pr(tr_eqs(u,raw))!=pr(tr_eqs(u,shown))) S(R_ALINE,tr_eqs(u,shown));
    th.push_back(shown[0]);
    if (sn || cs) th.push_back(shown[1]);
  }
  if (th.size()==2 && zr(th[1]-th[0]-pi)){ th.pop_back(); P=pi; } // (0 and pi: pi n)
  vecteur g,in;
  EACH(it,th) g.push_back(tr_plus(*it,P));
  S(R_TGEN,tr_eqs(u,g));
  gen s=ratq(P,m);
  if (!el_num(s,d)) return false;
  if (d<0) s=evn(s);
  vecteur xs;
  EACH(it,th){
    const gen b=ratn((*it-c)/m);
    xs.push_back(tr_plus(b,s));
    tr_in(b,s,in);
  }
  if (!(u==X)) S(R_TSOLVX,tr_eqs(X,xs));
  if (in.empty()) S(R_TINT,txt("none"));
  else if (!(u==X) || pr(tr_eqs(X,in))!=pr(tr_eqs(X,shown))) S(R_TINT,tr_eqs(X,in));
  EACH(it,in) tr_in(*it,evm(4,pi),sols); // (one each, in order: a step over 2pi)
  return true;
}
// P = 0 in one trig function T: solved for T (an identifier named T), each value's angles
static NOINL bool tr_one(const gen & T,const gen & L,const gen & R){
  const gen Y(identificateur(pr(T)));
  const gen Ls=ev(sub(L,T,Y)),Rs=ev(sub(R,T,Y));
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
  if (ks.size()>1 && !sols.empty()) S(R_TINT,tr_eqs(X,sols));
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
  const gen Q=evx(sub(subst(evs(L,R),s,Ys,false,CTX),c,Yc));
  if (dep(Q)) return false;
  const gen a=D(Q,Ys),b=D(Q,Yc);
  if (!dep(a,Ys) && !dep(a,Yc) && !dep(b,Ys) && !dep(b,Yc) && zr(Q-a*Ys-b*Yc)){ // a sin(u) + b cos(u) = 0
    const gen t=SYM(at_tan,u),k=ratn(-b/a);
    S(R_TDIVC,t,k);
    vecteur sols;
    return tr_basic(t,k,sols);
  }
  for (int i=0;i<2;++i){ // sin(u)^2 with cos(u): 1 - cos(u)^2 (or the other way)
    const gen Ya=i?Yc:Ys,Yb=i?Ys:Yc,A=i?c:s,B=i?s:c;
    const gen q2=evq(D(D(Q,Ya),Ya),plus_two);
    if (dep(q2,Ya) || dep(q2,Yb) || is_zero(q2) || dep(ratn(Q-q2*Ya*Ya),Ya)) continue;
    const gen one_b=SYM2(at_plus,plus_one,SYM(at_neg,SYM2(at_pow,B,plus_two)));
    S(R_TPYTH,SYM2(at_pow,A,plus_two),one_b);
    const gen Lb=sub(L,SYM2(at_pow,A,plus_two),one_b),Rb=sub(R,SYM2(at_pow,A,plus_two),one_b);
    vecteur f2;
    tr_find(Lb,f2); tr_find(Rb,f2);
    if (equalposcomp(f2,A)) return false; // (sin(x) left: sin(x)^3)
    S(R_ACONT,Lb,Rb);
    return tr_one(B,evx(Lb),evx(Rb));
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
  EACH(it,ids) if (*it!=X && evf(*it).type!=_DOUBLE_) return false;
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
  return ratn((a+b)/2);
}
// f rel 0 for f a polynomial or a quotient of polynomials: one side 0, factored, its critical
// numbers (zeros, and where the bottom is 0: never kept), a number tested in each interval, the
// intervals with the right sign kept (their zeros too with <= and >=)
static NOINL bool ineq_sign(const gen & L,const gen & R,int rl){
  const gen f=rats(L,R),n=nmr(f),d=dnm(f);
  rel_=rl;
  if (!is_zero(R)) S(R_QSTD,f);
  rel_=0;
  const size_t m=mark();
  S(R_NFAC,zero);
  const gen fn=fac_part(n),fd=dep(d)?fac_part(d):d;
  if (pr(fn)==pr(n) && pr(fd)==pr(d)) cut(m); else { std::string t=frac_txt(n,d); t+="="; t+=frac_txt(fn,fd); (*out_)[m].a=txt(t); }
  const vecteur z=roots_of(n,0),pz=dep(d)?roots_of(d,0):vecteur();
  vecteur c; // the critical numbers in order; pole[i]: the bottom's
  std::vector<char> pole;
  for (int j=0;j<2;++j) EACH(it,j?pz:z){
    if (it->type==_DOUBLE_ || it->type==_REAL) return false; // (approximate: no exact steps)
    unsigned i=0;
    while (i<c.size() && num(c[i])<num(*it)) ++i;
    if (i<c.size() && zr(c[i]-*it)){ if (j) pole[i]=1; continue; }
    c.insert(c.begin()+i,*it); pole.insert(pole.begin()+i,j);
  }
  const unsigned k=c.size();
  std::string t;
  for (unsigned i=0;i<k;++i){ if (i) t+=","; t+=pr(line(pole[i]?"%0!=%1":"%0=%1",X,c[i],0,0,0,0)); }
  if (k) S(R_NCRIT,txt(t));
  std::vector<char> in(2*k+1); // intervals at even places, the critical numbers at odd ones
  for (unsigned i=0;i<=k;++i){
    const gen tp=test_pt(i?c[i-1]:zero,i<k?c[i]:zero,i>0,i<k),v=ratn(sub(f,X,tp));
    const bool pos=spos(v);
    in[2*i]=(rl>=3)==pos;
    PL(i?R_NLINE:R_NTEST,!k?"%0=%1,%2@30":!i?"%0<%4,%0=%1,%2@30":i<k?"%5<%0<%4,%0=%1,%2@30":"%0>%5,%0=%1,%2@30",
       X,tp,v,gen(pos?3:1),i<k?c[i]:zero,i?c[i-1]:zero);
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
    const gen lo=a?c[(a-1)/2]:zero,hi=b<2*k?c[b/2]:zero;
    t+=pr(line(a==b && (a&1)?"%0=%2":!a?"%0@4%2":b==2*k?"%0@3%1":"%1@3%0@4%2",X,lo,hi,gen(!a || b==2*k?((a&1)?4:3):((a&1)?2:1)),gen((b&1)?2:1),0));
    a=b+1;
  }
  if (all) PL(R_NALL,"-oo<%0<oo",X);
  else if (t.empty()) S(R_NNONE,symbolic(*relop(rl),makesequence(f,0)));
  else S(R_NKEEP1+rl-1,txt(t));
  return !ctrl_c && !interrupted;
}
// an inequality: linear as an equation (the sign flipping by a negative), else ineq_sign
static NOINL bool ineq_steps(const gen & L,const gen & R,int rl){
  const gen f=rats(L,R);
  if (!dep(f) || !only_x(f) || !polyf(nmr(f)) || !polyf(dnm(f))) return false;
  const gen dg=degx(nmr(f));
  if (!dep(dnm(f)) && dg.type==_INT_ && dg.val<=1 && !dep(dnm(ratn(L))) && !dep(dnm(ratn(R)))){
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
  const gen p=evx(e),c1=co(p,1);
  return is_zero(c1)?gen(undef):ratn(-co(p,0)/c1);
}
static NOINL void lin_part(int rule,const gen & a,const gen & b,int rl){ // a rl b solved, a part below its title
  rel_=rl; S(rule,a,b); ++depth_; eq_linear(a,b); --depth_; rel_=0;
}
static NOINL bool abs_steps(const gen & eq,int rl){
  if (!rl && !eq.is_symb_of_sommet(at_equal)) return false;
  const gen L=ev(arg(eq,0)),R=ev(arg(eq,1));
  const vecteur ab=lop(makevecteur(L,R),at_abs);
  if (ab.size()!=1) return false;
  const gen B=ab[0],A=arg(B),u(identificateur("u__")),Lu=sub(L,B,u),Ru=sub(R,B,u),fu=evxs(Lu,Ru);
  if (!polyf(A) || !only_x(A) || !is_one(degx(A))) return false;
  const bool xout=dep(fu);
  gen k;
  int r=rl;
  if (xout){ // |A| = B, B linear
    if (rl || !(Lu==u || Ru==u)) return false;
    k=Lu==u?R:L;
    if (!polyf(k) || !only_x(k) || is_strictly_greater(degx(k),1,CTX)) return false;
  }
  else {
    const gen sx=X;
    X=u;
    const gen c1=co(fu,1);
    if (is_zero(c1) || !is_one(_degree(makesequence(fu,u),CTX))){ X=sx; return false; }
    k=ratn(-co(fu,0)/c1);
    if (!(Lu==u && !dep(Ru))){ // the absolute value alone: the moves of a linear equation in it
      const size_t m=mark();
      rel_=rl;
      eq_linear(Lu,Ru);
      r=rel_; rel_=0;
      for (size_t i=m;i<out_->size();++i){ step_t & s=(*out_)[i]; s.a=sub(s.a,u,B); s.b=sub(s.b,u,B); s.c=sub(s.c,u,B); s.v=sx; }
    }
    X=sx;
  }
  const gen mk=evn(k),G(r);
  const bool lt=r==1 || r==2;
  if (!xout && (is_zero(k) || spos(mk))){ // k <= 0
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
      if (is_undef(sj) || (!sol.empty() && zr(sol[0]-sj))) continue;
      if (xout){ // checked: |A| = B there
        const gen lv=ratn(sub(B,X,sj)),rv=ratn(sub(k,X,sj));
        const bool ok=zr(lv-rv);
        PL(ok?R_ROK:R_RBAD,ok?"%0=%1,%2=%3":"%0=%1,%2!=%3",X,sj,lv,rv);
        if (!ok) continue;
      }
      sol.push_back(sj);
    }
    if (sol.empty()) PL(R_RNONE,"%0=%1",B,k);
    else PL(R_BSOL,sol.size()>1?"%0=%1,%0=%2":"%0=%1",X,sol[0],sol.size()>1?sol[1]:zero);
    return true;
  }
  const gen a1=co(A,1),a0=co(A,0);
  const bool neg=spos(-a1);
  if (lt){ // -k < A < k: the same to all three parts, then divide
    const char * const f=r==1?"%0<%1<%2":"%0<=%1<=%2";
    gen lo=mk,hi=k;
    PL(R_BBETW,f,lo,A,hi);
    if (!is_zero(a0)){ lo=evs(lo,a0); hi=evs(hi,a0); PL(R_B3,f,lo,evm(a1,X),hi); }
    if (!is_one(a1)){
      lo=ratq(lo,a1); hi=ratq(hi,a1);
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
  if (el_has(eq)) return eq_el(el_norm(eq.is_symb_of_sommet(at_equal)?arg(eq,0):eq),el_norm(eq.is_symb_of_sommet(at_equal)?arg(eq,1):zero)); // (DeltaCE) 2^x = 8, log(x) = 2
  { vecteur fs; tr_find(eq,fs); if (!fs.empty()) return eq_trig(eq.is_symb_of_sommet(at_equal)?arg(eq,0):eq,eq.is_symb_of_sommet(at_equal)?arg(eq,1):zero); } // (DeltaCE) 2 sin(x) = 1
  gen L=ev(eq.is_symb_of_sommet(at_equal)?arg(eq,0):eq),R=ev(eq.is_symb_of_sommet(at_equal)?arg(eq,1):zero);
  if (!dep(L) && !dep(R)) return false;
  if (nroots(L)+nroots(R)) return eq_radical(L,R,L,R,0); // (sqrt(x+3) = x-3)
  if (dep(dnm(ratn(L))) || dep(dnm(ratn(R)))) return eq_rational(L,R); // (x/(x-2) = 3)
  return eq_poly(L,R);
}

// ---------------------------------------------------------------- F4's forms (DeltaCE)
gen positive_first(const gen & g); // answer.cc: a sum's positive term first (7-2x)
// (canonical_form(f): f's square completed; propfrac(f): its long division. The console's F4
// shows these forms; Enter on one shows its steps with these commands, focus.cc)
// f's one variable (X), false if several or none
static NOINL bool one_var(const gen & f){
  vecteur ids=lidnt(f);
  gen v=0;
  EACH(it,ids) if (evf(*it).type!=_DOUBLE_){ if (!is_zero(v) && *it!=v) return false; v=*it; }
  if (is_zero(v)) return false;
  X=v;
  return true;
}
// q a quadratic with an x term (rational coefficients): a, b, c
static NOINL bool quad(const gen & q,gen & a,gen & b,gen & c){
  if (!dep(q) || dep(dnm(ratn(q))) || degx(nmr(ratn(q)))!=2) return false;
  const gen p=evx(q);
  a=co(p,2); b=co(p,1); c=co(p,0);
  return !is_zero(b) && !dep(a) && !dep(b) && !dep(c);
}
// a x^2 + b x + c with its square completed as a textbook does it (a factored out of the x terms,
// half the x coefficient squared, added and subtracted, the perfect square, the numbers
// combined), then its vertex; the result written as F4 writes it (answer.cc): (x+3)^2-4, 5-(x-2)^2
static NOINL gen cs_quad(const gen & a,const gen & b,const gen & c,bool top){
  const gen B=ratq(b,a),h=ratq(B,plus_two),q=ratn(h*h),k=ratn(c-a*q);
  const bool one=is_one(a);
  std::string in=pr(X); in+="^2"; in+=sg(evm(B,X)); // x^2+6*x
  std::string A; // a as a factor: "", "-", "2*"
  if (is_minus_one(a)) A="-";
  else if (!one){ A=par(a); A+="*"; }
  const std::string sq=pr(symb_pow(X+h,plus_two)); // (x+3)^2
  std::string t;
  if (!one || !is_zero(c)){ t=A; t+="("; t+=in; t+=")"; t+=sg(c); S(one?R_CSGROUP:R_CSFACT,txt(t)); }
  PL(R_CSHALF,B.type==_FRAC?"(#0/2)^2=%1":"(%0/2)^2=%1",B,q); // ((3/2)/2)^2, (-4/2)^2
  t=A; t+="("; t+=in; t+=sg(q); if (!one) t+=sg(evn(q)); t+=")"; if (one) t+=sg(evn(q)); t+=sg(c);
  S(R_CSADD,txt(t));
  t=A; if (!one) t+="("; t+=sq; t+=sg(evn(q)); if (!one) t+=")"; t+=sg(c);
  S(R_CSSQ,txt(t));
  if (!one){ t=A; t+=sq; t+=sg(ratn(-a*q)); t+=sg(c); S(R_CSDIST,txt(t)); }
  const gen s=symb_pow(X+h,plus_two),u=one?s:is_minus_one(a)?symb_neg(s):symb_prod(a,s);
  const gen r=is_zero(k)?u:spos(-a) && spos(k)?SYM2(at_plus,k,u):SYM2(at_plus,u,k);
  if (pr(r)!=pr(txt(t))) S(R_CSCOMB,r);
  if (top) PL(R_CSVERT,"(%0,%1)",ratn(-h),k); // (inside sqrt(4x-x^2): no vertex)
  return r;
}
// every quadratic of g with an x term (sums only below the top, as F4: n(n+2) stays): completed,
// its steps first
static NOINL gen cs_in(const gen & g,bool top){
  gen a,b,c;
  if (g.type==_SYMB && (top || g.is_plus()) && quad(g,a,b,c)){
    if (!top) S(R_CSIN,g);
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
  if (out_->front().rule==R_CSIN) S(R_CSBACK,r); // (inside: the whole again)
  return true;
}
// long division of a polynomial by one of lower or equal degree: the leading terms divided, the
// divisor times that subtracted, again until the remainder's degree is lower; then the check
// (top = divisor * quotient + remainder) and quotient plus remainder over the divisor
static NOINL gen lead(const gen & p,int & d){ // p's leading term (its degree d)
  const gen g=degx(p);
  d=g.type==_INT_?g.val:0;
  return evm(_coeff(makesequence(p,X,d),CTX),symb_pow(X,d));
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
  n=evx(ev(a)); d=evx(ev(b));
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
  const gen rf=ratq(N,d),rd=dnm(rf);
  gen fd=_factor(rd,CTX);
  if (is_undef(fd) || fd.type==_STRNG) fd=rd;
  w.push_back(symb_prod(nmr(rf),symb_inv(fd)));
  S(R_DANS,positive_first(SYM(at_plus,gen(w,_SEQ__VECT))));
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
    const gen r=ratn(e);
    n=evx(nmr(r)); d=evx(dnm(r));
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
    const gen t=ratq(ln,ld),N2=evx(N-t*d);
    PL(R_DLEAD,"(%0)/(%1)=%2",ln,ld,t);
    PL(R_DSUB,"(%0)-#1*(%2)=%3",N,t,d,N2);
    Q=eva(Q,t); N=N2;
    if (is_zero(N)) break;
    lead(N,dn);
  }
  if (!is_zero(N)) S(R_DREM,N);
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
  const gen a=cd[0],c=ratn(-cd[1]/a);
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
    const gen p=evm(c,b),s=eva(cn[k],p);
    PL(R_SYMUL,"#0*#1=%2,%3+#2=%4",c,b,p,cn[k],s); // (2*4=8, -8+8=0)
    r1[k+1]=p; r2[k+1]=s;
    syn_table(R_SYTAB,r0,r1,r2,makevecteur(32+k+1,64+k+1));
    b=s;
  }
  gen Q=0;
  for (int i=1;i<N;++i) Q=Q+r2[i]*up(N-1-i);
  Q=ev(Q);
  const gen R=r2[N],dx=is_one(a)?d:evs(X,c); // (by x - c: (2x^3+...)/(x+1/2) = ...)
  vecteur w;
  flat(at_plus,Q,w);
  if (!is_zero(R)) w.push_back(symb_prod(R,symb_inv(dx)));
  std::string t=frac_txt(n,dx);
  t+="="; t+=pr(w.size()==1?w.front():positive_first(SYM(at_plus,gen(w,_SEQ__VECT))));
  S(R_SYREAD,txt(t));
  if (!is_one(a)){
    const gen Q2=evx(Q/a);
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
  S(R_KFAC,zero); // (its line once the factored forms are known)
  const gen fn=fac_part(n),fd=fac_part(d);
  std::string t=frac_txt(n,d);
  if (pr(fn)==pr(n) && pr(fd)==pr(d)) cut(m); // (neither factors: (x^2+1)/(x^2+1))
  else { t+="="; t+=frac_txt(fn,fd); (*out_)[m].a=txt(t); }
  const vecteur ex=roots_of(d,0);
  if (!ex.empty()) S(R_REXCL,eqseq(ex,at_different));
  const gen r=ratq(n,d),rn=_factor(nmr(r),CTX),rd=_factor(dnm(r),CTX);
  t=frac_txt(fn,fd); t+="="; t+=frac_txt(rn,rd);
  S(R_KCAN,txt(t));
  vecteur lost; // the excluded values the answer no longer shows (x+3 for (x^2-9)/(x-3): x != 3)
  EACH(it,ex) if (!zr(sub(rd,X,*it))) lost.push_back(*it);
  if (!lost.empty()){ t=frac_txt(rn,rd); t+=","; t+=pr(eqseq(lost,at_different)); S(R_DANS,txt(t)); }
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
static NOINL int deg(const gen & g){ const gen d=degx(g); return d.type==_INT_?d.val:0; }
static NOINL gen cof(const gen & g,int p){ return _coeff(makesequence(g,X,p),CTX); }
static NOINL gen sumof(const vecteur & v){ return v.empty()?zero:v.size()==1?v.front():SYMV(at_plus,v); }
static NOINL gen prodof2(const vecteur & v){ return v.size()==1?v.front():SYMV(at_prod,v); }
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
  const gen n=nrm(v);
  if (is_zero(n)) return 0;
  const gen q=dnm(n);
  return symb_prod(nmr(n),symb_inv(is_one(q)?d:symb_prod(q,d)));
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
  const gen x0=ratn(-co(b,0)/co(b,1)),lv=ratn(sub(n2,X,x0));
  const gen c=ratn(sub(ratn(prod/(*PU_)[i]),X,x0));
  const gen v=spos(-c)?ratn(-lv/evn(c)):ratq(lv,c); // (1/2 over -2: -1/4, not 1/-4)
  std::string t=pr(X); t+="="; t+=pr(x0); t+=","; t+=pr(lv); t+="="; // x=1,1=2*A,A=1/2 (no spaces: a name with one is quoted)
  if (is_minus_one(c)) t+="-";
  else if (!is_one(c)){ t+=c.type==_FRAC?par(c):pr(c); t+="*"; }
  t+=pr((*PD_)[i]); t+=","; t+=pr((*PD_)[i]); t+="="; t+=pr(v);
  S(R_PFROOT,txt(t));
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
  S(R_PFCOEF,txt(eqs));
  const gen sol=_linsolve(makesequence(gen(sys,_SEQ__VECT),gen(*PU_,_SEQ__VECT)),CTX);
  if (sol.type!=_VECT || sol._VECTptr->size()!=PU_->size()) return false;
  for (unsigned i=0;i<val.size();++i){ val[i]=nrm(sol[i]); if (i) w+=","; w+=pr((*PD_)[i]); w+="="; w+=pr(val[i]); }
  S(R_PFSOLVE,txt(w));
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
  const gen r=ratn(e),d=evx(dnm(r));
  gen n=evx(nmr(r)),Q=0;
  if (!dep(d) || !polyf(n) || !polyf(d)) return false;
  if (deg(d)<deg(evx(typed_den(e)))) S(R_QSIMP,r); // ((x^3+1)/(x^2-1) is (x^2-x+1)/(x-1))
  if (deg(n)>=deg(d)){ // improper: the quotient, then the remainder's fractions
    Q=_quo(makesequence(n,d,X),CTX);
    n=_rem(makesequence(n,d,X),CTX);
    if (is_zero(n)) return false;
    S(R_PFDIV,SYM2(at_plus,Q,symb_prod(n,symb_inv(d))));
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
  const gen n2=ratq(n,k),rhs=sumof(prods); // (2 x^2-2: 1/2 over (x-1)(x+1))
  PL(R_PFSET,"%0=%1",symb_prod(n2,symb_inv(prodof2(fp))),sh(sumof(terms)));
  PL(R_PFMULT,"%0=%1",n2,sh(rhs));
  vecteur val(PU_->size());
  if (roots) for (unsigned i=0;i<base.size();++i) val[i]=pf_root(n2,base[i],prods[i],i);
  else if (!pf_coeffs(n2,rhs,val)) return false;
  vecteur res(1,Q); // the fractions with their values, written as the answers are
  for (unsigned i=0;i<dens.size();++i) res.push_back(fr(subst(symb_prod(terms[i],dens[i]),*PU_,val,false,CTX),dens[i]));
  vecteur res2;
  EACH(it,res) if (!is_zero(*it)) res2.push_back(*it);
  S(R_PFRES,sumof(res2));
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
  const gen L=ev(is(e,at_equal)?arg(e,0):e),R=ev(is(e,at_equal)?arg(e,1):zero);
  if (!dep(L,y) && !dep(R,y)) return false;
  X=x;
  S(R_IDIFF,L,R);
  const gen Ld=ev(D(L,x)+D(L,y)*Yp),Rd=ev(D(R,x)+D(R,y)*Yp);
  S(R_ICHAIN,Ld,Rd);
  const gen P=evxs(Ld,Rd),A=ratn(D(P,Yp)),B=ev(sub(P,Yp,zero)),mB=ratn(-B);
  if (is_zero(A) || dep(A,Yp)) return false;
  vecteur t;
  flat(at_plus,evx(A),t);
  EACHW(it,t) *it=is_one(*it)?Yp:is_minus_one(*it)?SYM(at_neg,Yp):SYM2(at_prod,*it,Yp); // (2*y*dy/dx: dy/dx last)
  const gen side=t.size()==1?t[0]:SYMV(at_plus,t);
  S(R_ISIDE,side,mB);
  gen l=side;
  if (t.size()>1){ l=SYM2(at_prod,A,Yp); S(R_IFACT,l,mB); }
  const gen r0=nrm(mB/A),n=nmr(r0),d=dnm(r0);
  const gen r=negp(d)?SYM2(at_prod,nrm(-n),SYM(at_inv,nrm(-d))):r0; // (as implicitdiff: no -(...) below)
  if (!is_one(A)){ S(R_ADIV,l,mB,A); S(R_ACONT,Yp,r); }
  return true;
}

// answer.cc: a power series giac has no value for (undef, or the sum left as is): what the steps
// end with, its interval of convergence ((x>-2) and (x<4)); 0 if none
gen steps_series_value(const gen & s,GIAC_CONTEXT){
  if (!s.is_symb_of_sommet(at_sum) || arg(s).type!=_VECT || arg(s)._VECTptr->size()!=4 || arg(s)[1].type!=_IDNT) return 0;
  std::vector<step_t> out;
  gen x(identificateur("x"));
  ctx_=contextptr; out_=&out; depth_=0; nsub_=0; var_=&x; in_=&s; rel_=0; gen la_home=plus_inf; la_=&la_home; ldir_=0; // (no limit point left from a call before: la_ pointed at a gone local)
  quickG_=true; // (the console's: no lines built, no giac sum; the interval's steps took 4 s on the calculator)
  const gen r=one(norm(s)); // (as steps_of reads it: a/b as a*inv(b))
  quickG_=false;
  out_=0;
  return is_undef(r) || interrupted?gen(0):r;
}
static NOINL gen useries(const gen & g); // (below)
// subst(diff(F,x,n),x=a) (or subst(...,x,a)), n >= 3 an integer: F, x, n, a
static NOINL bool dsubst(const gen & g,gen & F,gen & x,gen & n,gen & a){
  if (!is(g,at_subst) || arg(g).type!=_VECT) return false;
  const vecteur & v=*arg(g)._VECTptr;
  if (v.size()==2 && is(v[1],at_equal)){ x=arg(v[1],0); a=arg(v[1],1); }
  else if (v.size()==3){ x=v[1]; a=v[2]; }
  else return false;
  if (!is(v[0],at_derive) || arg(v[0]).type!=_VECT || arg(v[0])._VECTptr->size()!=3 || arg(v[0],1)!=x || x.type!=_IDNT) return false;
  F=arg(v[0],0); n=ev(arg(v[0],2));
  return n.type==_INT_ && (n.val>=3 || (n.val>=1 && (!is_zero(useries(F)) || is(F,at_sum))));
}
// f^(n)(c) of a series f away from its center (does f'(2) converge?): each term differentiated n times,
// c put in for x, then that series' own tests; undef if not that
static NOINL gen doff(const gen & u,const gen & x,const gen & n,const gen & a,const gen & name){
  if (!is(u,at_sum) || arg(u).type!=_VECT || arg(u)._VECTptr->size()!=4 || arg(u,1).type!=_IDNT || dep(a,x)) return undef;
  const vecteur & v=*arg(u)._VECTptr;
  gen td=v[0];
  for (int j=0;j<n.val;++j) td=D(td,x);
  td=rn(ev(td));
  gen k=v[1];
  const gen sd=SYM(at_sum,makesequence(td,k,v[2],v[3])),s2=SYM(at_sum,makesequence(subst(td,x,a,false,CTX),k,v[2],v[3]));
  if (n.val==1) S(R_KDTERM,line("%0'(%1)=%2",name,x,sd,zero,zero,zero),0); else S(R_KDTERM,line("%0^((%1))(%2)=%3",name,n,x,sd,zero,zero),0);
  if (n.val==1) S(R_SUBV,line("%0'(%1)=%2",name,a,s2,zero,zero,zero),0); else S(R_SUBV,line("%0^((%1))(%2)=%3",name,n,a,s2,zero,zero),0);
  gen * ov=var_; var_=&k;
  ++depth_; const gen r=ssteps(s2,*arg(s2)._VECTptr,k); --depth_;
  var_=ov;
  return r;
}
// f^(n)(a) by the series (F: f(x) of a function the user defined, or the expression itself)
static NOINL gen dsteps_n(const gen & g,bool & big){
#ifndef FLOWCE_SERIES
  big=false; return undef; // (no series pack: no f^(n)(a) by the series)
#endif
  gen F,x,n,a;
  big=false;
  if (!dsubst(g,F,x,n,a)) return undef;
  big=n.val>30; // (no series: no steps, differentiating 2025 times never ends)
  var_=&x;
  const gen name=is(F,at_of) && arg(F).type==_VECT && arg(F,0).type==_IDNT?arg(F,0):gen(identificateur("f"));
  const size_t m=mark();
  gen u=useries(F); // (f(x) of f defined as sum(...): the sum itself, not giac's value of it)
  if (is_zero(u) && is(F,at_sum)) u=F; // (the sum typed in diff itself)
  gen r=kder(is_zero(u)?ev(F):u,x,n,ev(a),name);
  if (is_undef(r) && !is_zero(u) && n.val<=6){ cut(m); r=doff(u,x,n,ev(a),name); } // (away from the center: f'(2) by its own series)
  if (is_undef(r)) cut(m);
  return r;
}
// a series with its center put in, sum(c(k)*0^e(k),k,k0,inf) (f(1) of the course's f(x) = sum(...(x-1)^(2k))):
// only the term with e(k) = 0 is left (0^0 = 1; giac's 0^(2k) made it 0 or undef); undef if not that
static NOINL gen spoly(const gen & s,const gen & x,const gen & a,int n); // (above)
static NOINL gen scenter(const gen & s){
  if (!is(s,at_sum) || arg(s).type!=_VECT || arg(s)._VECTptr->size()!=4 || arg(s,1).type!=_IDNT) return undef;
  const gen k=arg(s,1),k0=ev(arg(s,2));
  vecteur w,o; flat(at_prod,norm(arg(s,0)),w);
  gen ks=undef,al1=0;
  EACH(it,w){
    gen b=*it,e=1;
    while (b.is_pow()){ e=arg(b,1)*e; b=arg(b,0); }
    if (is_undef(ks) && dep(e,k) && is_zero(ev(b))){ gen al,be; if (!is_linear_wrt(ev(e),k,al,be,CTX) || is_zero(al)) return undef; ks=ev(-be/al); al1=al; continue; }
    o.push_back(*it);
  }
  if (is_undef(ks)) return undef;
  if (ks.type!=_INT_ || is_strictly_greater(k0,ks,CTX)) return is_strictly_positive(al1,CTX) && !is_strictly_greater(ks,k0,CTX)?gen(0):gen(undef); // (no term with 0^0: every term 0, f(0) of sum(k*x^(2k),k,1,inf))
  return ev(sub(prodof(o),k,ks));
}
// lim |a(n+1)/a(n)| typed as the ratio test's limit, A over B with A = B at n+1 (the course's HW): B and its
// index n; false if not that shape (its limit then by the ratio test's pairs, not simplify's expanded quotient:
// 1.7 s on the PC, minutes on the calculator, for (2(n+1)^2/(4^(n+1)+...))/(2n^2/(4^n+...)))
// n in an exponent of g (2^n, n^n), the index given
static NOINL bool nexn(const gen & g,const gen & n){
  if (g.is_pow() && dep(arg(g,1),n)) return true;
  if (g.type!=_SYMB) return false;
  const gen & a=arg(g);
  if (a.type!=_VECT) return nexn(a,n);
  EACH(it,*a._VECTptr) if (nexn(*it,n)) return true;
  return false;
}
// the factors of a typed ratio without their absolute values (the ratio of positive terms): A/B, A
// times B upside down, |A| times the rest, as typed
static NOINL void rfac(const gen & u,vecteur & w,bool iv){
  if (is(u,at_abs)) return rfac(arg(u),w,iv);
  if (is(u,at_division) && arg(u).type==_VECT && arg(u)._VECTptr->size()==2){ rfac(arg(u,0),w,iv); return rfac(arg(u,1),w,!iv); }
  if (u.is_prod() && arg(u).type==_VECT){ EACH(it,*arg(u)._VECTptr) rfac(*it,w,iv); return; }
  if (u.is_inv()) return rfac(arg(u),w,!iv);
  w.push_back(iv?symb_inv(u):u);
}
static NOINL bool rform(const gen & g,gen & B,gen & n){
  if (!is(g,at_limit) || arg(g).type!=_VECT || arg(g)._VECTptr->size()<3 || arg(g,1).type!=_IDNT || ev(arg(g,2))!=plus_inf) return false;
  n=arg(g,1);
  gen f=arg(g,0),A;
  const bool ab=has_op(f,*at_abs);
  if (is(f,at_abs)) f=arg(f);
  if (is(f,at_division) && arg(f).type==_VECT && arg(f)._VECTptr->size()==2){ A=arg(f,0); B=arg(f,1); }
  else if (f.is_prod() && arg(f).type==_VECT){ // (a*inv(b) as parsed)
    vecteur o; int ni=0;
    EACH(it,*arg(f)._VECTptr){ if (it->is_inv() && dep(arg(*it),n)){ B=arg(*it); ++ni; } else o.push_back(*it); }
    if (ni!=1) B=0;
    A=prodof(o);
  }
  if (!dep(B,n) || !dep(A,n)) B=0;
  gen A1=A,B1=B; // (x and the other names as numbers: 0.37)
  const vecteur ids=lidnt(arg(g,0));
  EACH(it,ids) if (*it!=n){ A1=subst(A1,*it,gen(0.37),false,CTX); B1=subst(B1,*it,gen(0.37),false,CTX); }
  for (int j=3;j<9 && !is_zero(B);j+=2){ const gen a1=evalf(sub(A1,n,j),1,CTX),b1=evalf(sub(B1,n,j+1),1,CTX); if (a1.type!=_DOUBLE_ || b1.type!=_DOUBLE_ || !close(a1,b1)) B=0; }
  if (is_zero(B)){
    // a(n+1) times a(n) upside down in any order (|(n+1)!/(n+1)^(n+1)| n^n/n!, as the course writes it):
    // the factors split so that a(n) = 1 over the ones of 1/a(n), checked at n = 2, 3, 4; a constant goes
    // with a(n+1), or as k^n in a(n); only a(n) with n! or n in an exponent (n/(n+1) is a plain limit)
    vecteur w,F;
    rfac(arg(g,0),w,false);
    gen k=1;
    EACH(it,w) if (dep(*it,n)) F.push_back(*it); else k=k*(*it);
    const int m=F.size();
    if (m<2 || m>8) return false;
    double v[8][4],ke=1;
    for (int i=0;i<=m;++i){
      gen u=i<m?F[i]:k;
      EACH(it,ids) if (*it!=n) u=subst(u,*it,gen(0.37),false,CTX);
      for (int t=0;t<4;++t){
        const gen y=evalf(sub(u,n,2+t),1,CTX);
        if (y.type!=_DOUBLE_ || !(fabs(y._DOUBLE_val)>0)) return false;
        const double yv=ab?fabs(y._DOUBLE_val):y._DOUBLE_val; // (signs kept without |...|: (-1)^n n/(n+1))
        if (i<m) v[i][t]=yv; else { ke=yv; break; }
      }
    }
    for (int q=1;q<(1<<m)-1 && is_zero(B);++q) for (int wk=0;wk<2 && is_zero(B);++wk){
      bool ok=true;
      for (int t=0;t<3 && ok;++t){
        double r=wk?1:ke;
        for (int i=0;i<m;++i) r*=q>>i&1?v[i][t+1]:v[i][t];
        ok=fabs(r-1)<1e-3;
      }
      if (!ok) continue;
      vecteur b;
      for (int i=0;i<m;++i) if (q>>i&1) b.push_back(F[i].is_inv()?arg(F[i]):symb_inv(F[i]));
      if (wk && !is_one(k)) b.push_back(symb_pow(k,n));
      B=prodof(b);
      if (!has_op(B,*at_factorial) && !nexn(B,n)) return false;
    }
    if (is_zero(B)) return false;
  }
  { vecteur bw; flat(at_prod,norm(B),bw); bool q=false; // (a series' term: a denominator or n!; a polynomial's or 3^n's ratio is a plain limit, the course's 2A, 8B)
    EACH(it,bw) if (it->is_inv() || has_op(*it,*at_factorial)) q=true;
    if (!q) return false; }
  return true;
}
// 3^(n+1)*3^(-n), n^(n+1)/n^n: one power of each base, the exponents added (the limit of 3^(n+1)/3^n as
// typed took 19 s on the calculator before its simplified form)
static NOINL gen pmerge(const gen & f,const gen & y){
  vecteur w,o,b,e; flat(at_prod,f,w);
  bool ch=false;
  EACH(it,w){
    gen u=*it; const gen q=bexp(u);
    if (!(u==y || (u.type==_INT_ && u.val>1))){ o.push_back(*it); continue; }
    size_t i=0; while (i<b.size() && b[i]!=u) ++i;
    if (i<b.size()){ e[i]=e[i]+q; ch=true; } else { b.push_back(u); e.push_back(q); }
  }
  if (!ch) return f;
  for (size_t i=0;i<b.size();++i){ const gen q=ev(e[i]); if (!is_zero(q)) o.push_back(is_one(q)?b[i]:symb_pow(b[i],q)); }
  return prodof(o);
}
// g's names all numbers but the index k (pi, e): a series of numbers ((pi/2)^(2n): lidnt saw pi there)
static NOINL bool numof(const gen & g,const gen & k){
  const vecteur ids=lidnt(g);
  bool hk=false;
  EACH(it,ids){ if (*it==k){ hk=true; continue; } if (evalf(*it,1,CTX).type!=_DOUBLE_) return false; }
  return hk;
}
// the console's words for the steps' verdict on a series (why it diverges, which test says it converges)
static NOINL void vmsg(gen & r,const std::vector<step_t> & out){
  if (r==word("diverges")){
    const int k=out.empty()?0:out.back().rule;
    steps_divmsg=out.empty()?"diverges":k==R_SGEOMD?"diverges (geometric, |r| >= 1)":k==R_SDIV || k==R_SDIV2?"diverges (the terms do not go to 0)":out.back().b==minus_inf?"diverges (to -infinity)":"diverges (to +infinity)";
    r=undef;
  }
  else if (r==word("converges") && !out.empty()){ // (which test: its value not asked, giac's sum took 15 to 50 s on the calculator)
    const int k=out.back().rule,k2=out.size()>1?out[out.size()-2].rule:0;
    steps_divmsg=k==R_RATIO || k==R_RCONV?"converges (by the ratio test)":k==R_AST?"converges (alternating series test)":k==R_ROOT?"converges (by the root test)":k==R_PSER?(k2==R_LCMP?"converges (like a p-series)":k2==R_LCMP0?"converges (by limit comparison)":"converges (p-series)"):"converges";
    r=undef;
  }
  else if (r.type==_IDNT) r=undef; // (converges: giac's value, or its decimal)
}
// answer.cc: series(f,x,a,inf) (taylor(f,x=a,inf)): f's Taylor series in summation notation by the
// known ones; subst(diff(f,x,n),x=a), n >= 3: f^(n)(a) from the series' coefficient (95! kept as
// 95!); undef if none
gen steps_series_answer(const gen & g,GIAC_CONTEXT){
  std::vector<step_t> out;
  gen x(identificateur("x")),r=undef;
  ctx_=contextptr; out_=&out; depth_=0; nsub_=0; var_=&x; in_=&g; rel_=0; gen la_home=plus_inf; la_=&la_home; ldir_=0; // (no limit point left from a call before: la_ pointed at a gone local)
  const gen s=norm(serfix(g));
  if (is(s,at_series) && arg(s).type==_VECT && arg(s)._VECTptr->size()==4 && arg(s,1).type==_IDNT && is_inf(ev(arg(s,3)))){
    gen y=arg(s,1); var_=&y;
    const gen u=psum(arg(s,0)); // (a power series typed in: its own Taylor series)
    r=is_zero(u)?ksigma(*arg(s)._VECTptr,y):u;
  }
  else if (is(g,at_of) && arg(g).type==_VECT && arg(g)._VECTptr->size()==2 && arg(g,0).type==_IDNT){ // f(c), f defined as a series
    const gen fv=ev(arg(g,0));
    if (fv.is_symb_of_sommet(at_program) && arg(fv).type==_VECT && arg(fv)._VECTptr->size()==3){
      gen p=arg(fv,0);
      if (p.type==_VECT && p._VECTptr->size()==1) p=p[0];
      if (p.type==_IDNT){ const gen sc=subst(arg(fv,2),p,arg(g,1),true,contextptr); r=scenter(sc);
        if (is_undef(r) && is(sc,at_sum) && arg(sc).type==_VECT && arg(sc)._VECTptr->size()==4 && arg(sc,1).type==_IDNT && is_inf(ev(arg(sc,3))) && numof(ev(arg(sc,0)),arg(sc,1))){ // (f(2) away from the center: its series' tests, f(2) diverges)
          gen y=arg(sc,1); var_=&y; quickG_=true; r=ssteps(sc,*arg(sc)._VECTptr,y); quickG_=false; vmsg(r,out); } }
    }
  }
  else if (!is_undef(r=scenter(s))){} // (the series with the center typed in)
  else if (is(s,at_limit) && arg(s).type==_VECT && arg(s)._VECTptr->size()==3 && arg(s,1).type==_IDNT && is_inf(ev(arg(s,2))) && ev(arg(s,2))==plus_inf && (!strcmp(arg(s,1)._IDNTptr->id_name,"n") || !strcmp(arg(s,1)._IDNTptr->id_name,"k") || has_op(arg(s,0),*at_factorial))){
    gen y=arg(s,1),a=plus_inf; var_=&y; la_=&a; ldir_=0; budget_=80; // (a sequence's limit: lgrow's, by the classes)
    { gen B,n0,xp; if (rform(g,B,n0) && n0==y){ const gen L=rtest(norm(B),y,0,xp,true); if (finv(L) || is_inf(L)) return L; } } // (a typed ratio test's limit: its pairs)
    trust_=false; r=lsteps(pmerge(norm(arg(s,0)),y),0); if (!trust_) r=undef; // (as typed first: limit_simp's simplify of 3*5^n/6^n took 3 s on the calculator)
    if (is_undef(r)){
    out.clear(); budget_=80;
    const gen ls=limit_simp(s);
    gen f=is(ls,at_limit) && arg(ls).type==_VECT?norm(arg(ls,0)):arg(s,0); // (the ratio test's |a(n+1)/a(n)| simplified first)
    if (has_op(f,*at_factorial) && lop(f,at_factorial).size()>1) f=_simplify(ev(f),contextptr); // (a ratio of n!'s: limit_simp leaves them, giac's limit took 3 minutes)
    if (has_op(f,*at_ln) && ev(f).is_plus()){ // (ln(k+1)-ln(k): one log, giac's limit of it is fast; 108 s for the difference on the calculator)
      const gen c=_lncollect(ev(f),contextptr);
      if (is(c,at_ln)) r=ev(SYM(at_ln,ev(Linf(arg(c),y))));
    }
    else { trust_=false; r=lsteps(f,0); if (!trust_) r=undef; }
    }
    if (r==word("DNE")){ steps_divmsg="no limit (it oscillates)"; r=undef; }
  }
  else if (is(s,at_sum) && arg(s).type==_VECT && arg(s)._VECTptr->size()==4 && arg(s,1).type==_IDNT && is_inf(ev(arg(s,3))) && numof(ev(arg(s,0)),arg(s,1))){
    gen y=arg(s,1); var_=&y; // (a series of numbers: the steps' tests, giac's sum not computed: geometric and telescoping values, divergence)
    quickG_=true; r=ssteps(s,*arg(s)._VECTptr,y); quickG_=false;
    vmsg(r,out);
  }
  else if (is(s,at_sum) && arg(s).type==_VECT && arg(s)._VECTptr->size()==4 && arg(s,1).type==_IDNT && (arg(s,3).type==_IDNT || !lidnt(arg(s,0)).empty()) && !is_inf(ev(arg(s,3)))){
    gen y=arg(s,1); var_=&y; // (sum(t,k,a,n): telescoping's explicit s_n)
    r=sfin(s,*arg(s)._VECTptr,y);
  }
  else { bool big; quickG_=true; r=dsteps_n(g,big); quickG_=false; vmsg(r,out); }
  la_=&la_home; ldir_=0; // (the branches' limit points are gone)
  if (is_undef(r) && is(s,at_series) && arg(s).type==_VECT && arg(s)._VECTptr->size()==4 && is(arg(s,0),at_integrate) && ev(arg(s,3)).type==_INT_){ // (an integral's: its integrand's terms integrated)
    const gen q=qser(s);
    if (!is(q,at_series) && !has_op(q,*at_integrate)) r=SYM(at_series,makesequence(q,arg(s,1),arg(s,2),arg(s,3))); // (giac's series of that polynomial: its terms in order, not simplified as a sum)
  }
  if (is_undef(r) && is(s,at_series) && arg(s).type==_VECT && arg(s)._VECTptr->size()==4 && arg(s,1).type==_IDNT && ev(arg(s,3)).type==_INT_){ // (f defined as a sum: its terms, not giac's series of it)
    const gen u=psum(arg(s,0));
    if (!is_zero(u)){ gen y=arg(s,1); var_=&y; r=spoly(u,y,ev(arg(s,2)),ev(arg(s,3)).val); if (!is_undef(r)) r=SYM(at_series,makesequence(r,y,arg(s,2),arg(s,3))); } // (giac's series of p_n: its terms in order, not one fraction)
  }
  if (is_undef(r) && is(s,at_subst) && arg(s).type==_VECT && arg(s)._VECTptr->size()==2 && is(arg(s,1),at_equal) && (is(arg(s,0),at_series) || is(arg(s,0),at_taylor))){ // (p_n(0.4) of series(...): no O)
    vecteur t,q; flat(at_plus,ev(arg(s,0)),t); EACH(it,t) if (!has_op(*it,*at_order_size)) q.push_back(*it);
    r=ev(sub(q.empty()?gen(0):symb_plus(q),arg(arg(s,1),0),arg(arg(s,1),1)));
  }
  if (is_undef(r) && is(s,at_subst) && arg(s).type==_VECT && arg(s)._VECTptr->size()>=2){ // (a value put in a power series: the series with it in, as typed so: the center's 0^0 = 1, not giac's undef)
    const vecteur & v=*arg(s)._VECTptr; gen y,a;
    if (v.size()==2 && is(v[1],at_equal)){ y=arg(v[1],0); a=arg(v[1],1); } else if (v.size()==3){ y=v[1]; a=v[2]; }
    if (y.type==_IDNT && is(v[0],at_sum) && arg(v[0]).type==_VECT && arg(v[0])._VECTptr->size()==4 && is_inf(ev(arg(v[0],3)))){
      const vecteur & w=*arg(v[0])._VECTptr;
      const gen R=steps_series_answer(SYM(at_sum,makesequence(subst(w[0],y,a,true,CTX),w[1],w[2],w[3])),contextptr);
      out_=&out; if (!is_undef(R)) return R;
    }
  }
  if (is_undef(r) && is(s,at_subst) && arg(s).type==_VECT && !has_op(s,*at_derive)){ // (a value with a huge power: kept, 3^2018)
    const vecteur & v=*arg(s)._VECTptr;
    gen y,a;
    if (v.size()==2 && is(v[1],at_equal)){ y=arg(v[1],0); a=arg(v[1],1); } else if (v.size()==3){ y=v[1]; a=v[2]; }
    if (y.type==_IDNT){ const gen e=ev(sub(v[0],y,a)); if (e.type==_STRNG) r=held(subst(v[0],y,a,true,CTX)); }
  }
  out_=0;
  return interrupted?gen(undef):r; // (undef: none; 0 is an answer, f^(2021)(0) of an even f)
}
#if defined FLOWCE_SERIES || defined HOSTGIAC
// ---------------------------------------------------------------- sequences by a recurrence (M2)
// a_1 = 3, a_(n+1) = a_n + 2n typed (answer_prepass: a(1)=3, a(n+1)=a(n)+2n): the definitions kept as
// text (no gen kept across an ON or memory reset), the terms worked out one by one from the first as the
// course does (a_2 = a_1+2(1) = 5, a_3 = a_2+2(2) = 9); their names in ti_seq_names (tiinput_glue.cc: the
// implicit products leave a(...) a call)
extern char ti_seq_names[4];
static char sqd_[3][4][64]; // [sequence][0..2: an initial term "k,v", 3: the recurrence "a(n+1)=..."]
static NOINL int sqi(const gen & x){
  if (x.type!=_IDNT || strlen(x._IDNTptr->id_name)!=1) return -1;
  const char * p=strchr(ti_seq_names,x._IDNTptr->id_name[0]);
  return p?int(p-ti_seq_names):-1;
}
// the recurrence as X(n+c) = ..., its term of the highest index alone on the left (a(n+1)-a(n)=4:
// a(n+1)=a(n)+4); undef if that term is not there to the first power
static NOINL gen sqiso(const gen & g,const gen & Xs){
  vecteur ts=lop(g,at_of);
  gen top=undef,hi=minus_inf;
  EACH(it,ts){ const gen & a=arg(*it); if (a.type!=_VECT || a._VECTptr->size()!=2 || a[0]!=Xs) continue; const vecteur ids=lidnt(a[1]); if (ids.size()!=1) continue; const gen c=rn(a[1]-ids[0]); if (c.type==_INT_ && (is_undef(top) || is_strictly_greater(c,hi,CTX))){ top=*it; hi=c; } } // (rn: n+1-n left as is by ev)
  if (is_undef(top)) return undef;
  const gen T(identificateur("t__"));
  const gen E=ev(subst(arg(g,0)-arg(g,1),top,T,false,CTX)),k=ev(D(E,T));
  if (dep(k,T) || is_zero(k)) return undef;
  return symbolic(at_equal,makesequence(top,rn(ev(T-E/k))));
}
// a sequence's line typed, X(k) = v (a first term) or a recurrence in X(n+c), X(n)... (a(n+1)=a(n)+2n,
// a(n+1)-a(n)=4): kept; false if g is no such line (f(x) = x^2 is giac's own)
bool steps_seqdef(const gen & g,GIAC_CONTEXT){
  ctx_=contextptr; // (the comparisons below: no context left from a steps page)
  if (!is(g,at_equal) || !has_op(g,*at_of)) return false;
  gen Xs=0;
  { vecteur ts=lop(g,at_of); EACH(it,ts){ const gen & a=arg(*it); if (a.type==_VECT && a._VECTptr->size()==2 && a[0].type==_IDNT && strlen(a[0]._IDNTptr->id_name)==1 && strchr(ti_seq_names,a[0]._IDNTptr->id_name[0])) Xs=a[0]; } }
  if (is_zero(Xs)) return false;
  const gen A=is(arg(g,0),at_of) && arg(arg(g,0),0)==Xs?ev(arg(arg(g,0),1)):gen(undef),Rh=arg(g,1);
  int i=sqi(Xs);
  std::string t;
  int j=3;
  if (A.type==_INT_ && !has_op(Rh,*at_of)){ t=pr(A)+","+pr(ev(Rh)); for (j=0;j<2 && sqd_[i][j][0] && atoi(sqd_[i][j])!=A.val;++j) ; } // (a(1)=1, a(2)=1: two first terms)
  else { const gen r=sqiso(g,Xs); if (is_undef(r)) return false; t=pr(r); }
  if (t.size()>63) return false;
  strcpy(sqd_[i][j],t.c_str());
  return true;
}
// term N of sequence i (undef if it has no recurrence or first term before N, or too far: 5000);
// stp: each term's line, a(m) = its recurrence with the last terms in = its value
static NOINL gen sqterm(int i,const gen & N,bool stp){
  if (N.type!=_INT_ || !sqd_[i][3][0]) return undef;
  const gen Xs(identificateur(std::string(1,ti_seq_names[i]))),eq(sqd_[i][3],CTX);
  if (!is(eq,at_equal) || !is(arg(eq,0),at_of)) return undef;
  const gen A=arg(arg(eq,0),1);
  const vecteur ids=lidnt(A);
  if (ids.size()!=1) return undef;
  const gen v=ids[0],c=ev(A-v),rhs=ev(subst(arg(eq,1),v,v-c,false,CTX)); // (a(n) = its recurrence at n-c)
  vecteur t; // (the terms from k0)
  int k0=1<<20;
  for (int j=0;j<3;++j) if (sqd_[i][j][0] && atoi(sqd_[i][j])<k0) k0=atoi(sqd_[i][j]);
  if (k0==1<<20 || N.val<k0 || N.val-k0>5000) return undef;
  for (int m=k0;m<=N.val;++m){
    if (interrupted) return undef;
    gen y=undef;
    for (int j=0;j<3;++j) if (sqd_[i][j][0] && atoi(sqd_[i][j])==m) y=gen(strchr(sqd_[i][j],',')+1,CTX);
    const gen am(identificateur(std::string(1,ti_seq_names[i])+"_"+pr(gen(m)))); // (a_3 as the course writes it)
    if (is_undef(y)){
      gen r=subst(rhs,v,m,false,CTX),rs=r;
      vecteur ps=lop(r,at_of); // (the terms before: a(m-1), a(m-2))
      EACH(it,ps){ const gen & a=arg(*it); if (a.type!=_VECT || a._VECTptr->size()!=2 || a[0]!=Xs) continue; const gen k=ev(a[1]); if (k.type!=_INT_ || k.val<k0 || k.val>=m) return undef; r=subst(r,*it,t[k.val-k0],false,CTX); rs=subst(rs,*it,gen(identificateur(std::string(1,ti_seq_names[i])+"_"+pr(k))),false,CTX); }
      y=rn(ev(r));
      if (stp && N.val-k0<=8) S(R_LINE,line("%0=%1=%2=%3",am,rs,r,y,zero,zero),0); // (a_2 = a_1+2(1) = 3+2(1) = 5)
    }
    else if (stp && N.val-k0<=8) S(R_SEQT,line3("%0=%1",am,y),0); // (the first terms, given)
    t.push_back(y);
  }
  return t.back();
}
// lim of sequence i's terms: worked out as decimals until they settle (60 at most), then L = its
// recurrence at L, the root they approach (a(n) = sqrt(2a(n-1)) from 3: 2); undef if they do not
// settle, or n is in the recurrence, or it has two terms before
static NOINL gen sqlim(int i,bool stp){
  if (!sqd_[i][3][0]) return undef;
  const gen Xs(identificateur(std::string(1,ti_seq_names[i]))),eq(sqd_[i][3],CTX);
  if (!is(eq,at_equal) || !is(arg(eq,0),at_of)) return undef;
  const gen A=arg(arg(eq,0),1);
  const vecteur ids=lidnt(A);
  if (ids.size()!=1) return undef;
  const gen v=ids[0],c=ev(A-v),rhs=ev(subst(arg(eq,1),v,v-c,false,CTX)),L(identificateur("L"));
  vecteur ps=lop(rhs,at_of);
  gen F=rhs;
  EACH(it,ps) F=subst(F,*it,L,false,CTX);
  if (dep(F,v) || ps.size()!=1) return undef;
  int k0=1<<20,j0=-1;
  for (int j=0;j<3;++j) if (sqd_[i][j][0] && atoi(sqd_[i][j])<k0){ k0=atoi(sqd_[i][j]); j0=j; }
  if (j0<0) return undef;
  gen e=evalf_double(gen(strchr(sqd_[i][j0],',')+1,CTX),1,CTX);
  if (e.type!=_DOUBLE_) return undef;
  double y=e._DOUBLE_val,y0;
  int m=0;
  for (;m<60;++m){ y0=y; e=evalf_double(subst(F,L,gen(y),false,CTX),1,CTX); if (e.type!=_DOUBLE_ || interrupted) return undef; y=e._DOUBLE_val; if (fabs(y-y0)<TOL*(1+fabs(y))) break; if (fabs(y)>1e15) return y>0?plus_inf:minus_inf; } // (2a(n): to infinity)
  if (m==60) return undef;
  const gen sol=_solve(makesequence(symbolic(at_equal,makesequence(L,F)),L),CTX);
  if (sol.type!=_VECT) return undef;
  gen best=undef; double bd=1e30;
  EACH(it,*sol._VECTptr){ const gen f=evalf_double(*it,1,CTX); if (f.type==_DOUBLE_ && fabs(f._DOUBLE_val-y)<bd){ bd=fabs(f._DOUBLE_val-y); best=*it; } }
  if (is_undef(best) || bd>1e-2*(1+fabs(y))) return undef;
  best=rn(best); // (2, not sqrt(2)^2)
  if (stp){ S(R_SEQL,symbolic(at_equal,makesequence(L,F)),0); S(R_LINE,line3("L=%0",best),0); }
  return best;
}
// g with the sequences' terms in (a(3): 9) and their finite sums written out first (sum(a(k),k,1,3))
gen steps_seqval(const gen & g,GIAC_CONTEXT){
  if (g.type!=_SYMB || !ti_seq_names[0] || !has_op(g,*at_of)) return g;
  ctx_=contextptr;
  if (is(g,at_limit) && arg(g).type==_VECT && arg(g)._VECTptr->size()>=3 && is(arg(g,0),at_of) && arg(arg(g,0)).type==_VECT && arg(arg(g,0),1)==arg(g,1) && ev(arg(g,2))==plus_inf){ // (lim a(n) of a recurrence)
    const int i=sqi(arg(arg(g,0),0));
    if (i>=0){ const gen L=sqlim(i,false); if (!is_undef(L)) return L; }
  }
  if (is(g,at_sum) && arg(g).type==_VECT && arg(g)._VECTptr->size()==4){
    const vecteur & v=*arg(g)._VECTptr; const gen lo=ev(v[2]),hi=ev(v[3]);
    if (lo.type==_INT_ && hi.type==_INT_ && hi.val>=lo.val && hi.val-lo.val<500){ vecteur w; for (int j=lo.val;j<=hi.val;++j) w.push_back(steps_seqval(subst(v[0],v[1],j,false,CTX),contextptr)); return ev(symb_plus(w)); }
  }
  if (is(g,at_of) && arg(g).type==_VECT && arg(g)._VECTptr->size()==2){ const int i=sqi(arg(g,0)); if (i>=0){ const gen y=sqterm(i,ev(arg(g,1)),false); if (!is_undef(y)) return y; } }
  const gen & a=g._SYMBptr->feuille;
  if (a.type!=_VECT) return symbolic(g._SYMBptr->sommet,steps_seqval(a,contextptr));
  vecteur w; EACH(it,*a._VECTptr) w.push_back(steps_seqval(*it,contextptr));
  return symbolic(g._SYMBptr->sommet,gen(w,a.subtype));
}
#endif
// limit(f,x,a) or sum(f,k,a,b) whose variable is not in f (the templates' x and k; the course's
// sequences in n): f's own variable (n if it has one, else its only name); g itself if not that
gen steps_fixvar(const gen & g,GIAC_CONTEXT){
  if (g.type!=_SYMB) return g;
  if (is(g,at_series) && arg(g).type==_VECT && arg(g)._VECTptr->size()>=1){ vecteur v=*arg(g)._VECTptr; v[0]=steps_fixvar(v[0],contextptr); return symbolic(at_series,gen(v,_SEQ__VECT)); } // (the integral inside: its t)
  const bool in=is(g,at_integrate) && arg(g).type==_VECT && arg(g)._VECTptr->size()==4; // (integrate(t^4 sin(t^2),x,0,x): t, the bounds as typed)
  if (!(is(g,at_limit) || is(g,at_sum) || in) || arg(g).type!=_VECT || arg(g)._VECTptr->size()<3) return g;
  vecteur v=*arg(g)._VECTptr;
  if (v[1].type!=_IDNT || has_op(v[0],*at_integrate) || has_op(v[0],*at_sum) || has_op(v[0],*at_limit) || has_op(v[0],*at_derive)) return g; // (an inner integral's r is not the outer t: int int r e^(-r^2) dr dt was 2 pi)
  const vecteur ids=lidnt(v[0]);
  gen w=0;
  EACH(it,ids){ if (*it==v[1]) return g; if (it->type==_IDNT && evalf(*it,1,contextptr).type!=_DOUBLE_){ if (!strcmp(it->_IDNTptr->id_name,"n")) w=*it; else if (is_zero(w)) w=*it; else if (w.type==_IDNT && strcmp(w._IDNTptr->id_name,"n")) return g; } } // (two names: n, else none)
  if (is_zero(w)) return g;
  v[1]=w;
  return symbolic(g._SYMBptr->sommet,gen(v,_SEQ__VECT));
}
// K|b u^p|^q < R typed (the ratio test's inequality on its own, |x-1|^2/9 < 1): u linear in x;
// the steps as pint's (|u|^n < R/K, |u| < r, -r < u < r, the interval); false if not that
static NOINL bool absineq(const gen & L,const gen & R,const gen & x){
  if (dep(R,x) || !is_strictly_positive(R,CTX)) return false;
  vecteur w; flat(at_prod,norm(L),w); // (|x-1|^2/9 typed: a division)
  gen K=1,u=0,q=1;
  EACH(it,w){
    if (!dep(*it,x)){ K=K*(*it); continue; }
    gen b=*it,e=1;
    if (b.is_pow()){ e=arg(b,1); b=arg(b,0); }
    if (!is(b,at_abs) && e.type==_INT_ && !(e.val&1) && e.val>0) b=SYM(at_abs,b); // (giac's (x-1)^2 for |x-1|^2)
    if (!is_zero(u)){ if (!is(b,at_abs) || ev(arg(b))!=u) return false; q=ev(q+e); continue; } // (|x-1|^3 as |x-1|(x-1)^2)
    if (!is(b,at_abs) || dep(e,x) || !is_strictly_positive(e,CTX)) return false;
    gen g=arg(b),c=1,pp=1,al,be;
    vecteur v; flat(at_prod,g,v);
    g=1;
    EACH(jt,v){ if (!dep(*jt,x)) c=c*(*jt); else { gen bb=*jt; if (bb.is_pow() && !dep(arg(bb,1),x)){ pp=arg(bb,1); bb=arg(bb,0); } g=bb; } }
    g=ev(g); // (x-1 as typed: is_linear_wrt wants it evaluated)
    if (!is_linear_wrt(g,x,al,be,CTX) || is_zero(al)) return false;
    K=K*pow(abs(ev(c),CTX),e,CTX); u=g; q=ev(pp*e);
  }
  K=ev(K);
  if (is_zero(u) || !is_strictly_positive(K,CTX) || q.type!=_INT_) return false;
  gen lo,hi;
  const gen au=is_one(q)?SYM(at_abs,u):symb_pow(SYM(at_abs,u),q);
  S(R_ABSU,SYM2(at_inferieur_strict,is_one(K)?au:symb_prod(K,au),R),0);
  if (is_one(K)) cut(mark()-1); // (|x-1|^2 < 9: pint's first line is it)
  return pint(x,ev(K/R),u,q,lo,hi);
}
// f(c) of a function the user defined as a series (f(x) = sum(...,k,0,inf)): that series with c put
// in for x, unevaluated; 0 if not that
static NOINL gen useries(const gen & g){
  if (!is(g,at_of) || arg(g).type!=_VECT || arg(g)._VECTptr->size()!=2 || arg(g,0).type!=_IDNT || arg(g,1).type==_VECT) return 0;
  const gen p=ev(arg(g,0));
  if (!p.is_symb_of_sommet(at_program) || arg(p).type!=_VECT || arg(p)._VECTptr->size()<3) return 0;
  gen vs=arg(p,0),body=arg(p,2);
  if (vs.type==_VECT){ if (vs._VECTptr->size()!=1) return 0; vs=vs[0]; }
  if (is(body,at_return)) body=arg(body);
  if (vs.type!=_IDNT || !is(body,at_sum) || arg(body).type!=_VECT || arg(body)._VECTptr->size()!=4 || !is_inf(ev(arg(body,3)))) return 0;
  return subst(body,vs,arg(g,1),true,CTX);
}
static NOINL gen pmerge(const gen & f,const gen & y); // (below)
// a power series typed as such (sum(c(k)x^k,k,0,inf)) or f defined as one: that sum, else 0
static NOINL gen psum(const gen & g){
  if (is(g,at_sum) && arg(g).type==_VECT && arg(g)._VECTptr->size()==4 && is_inf(ev(arg(g,3)))) return g;
  if (g.is_prod() && arg(g).type==_VECT){ // (x^2 times a given series: the multiplier in each term, x^2 sum(t(k)) = sum(x^2 t(k)))
    gen sm=0,mu=1; int ns=0;
    EACH(it,*arg(g)._VECTptr){ if (is(*it,at_sum) && arg(*it).type==_VECT && arg(*it)._VECTptr->size()==4 && is_inf(ev(arg(*it,3)))){ sm=*it; ++ns; } else mu=mu*(*it); }
    if (ns==1 && !has_op(mu,*at_sum)){ const vecteur & w=*arg(sm)._VECTptr;
      if (!dep(mu,w[1])){ gen t=symb_prod(mu,w[0]); const vecteur ids=lidnt(mu); if (ids.size()==1) t=qshow(pmerge(norm(t),ids[0])); // (x^2 x^n as x^(n+2))
        return SYM(at_sum,makesequence(t,w[1],w[2],w[3])); } }
  }
  if ((is(g,at_derive) || is(g,at_integrate)) && arg(g).type==_VECT && arg(g)._VECTptr->size()>=2 && arg(g,1).type==_IDNT){ // (a given series differentiated or integrated from 0: term by term)
    const vecteur & a=*arg(g)._VECTptr;
    const gen y=a[1],u=psum(a[0]);
    if (is_zero(u)) return 0;
    const vecteur & w=*arg(u)._VECTptr;
    gen t=w[0],k0=ev(w[2]);
    if (k0.type!=_INT_) return 0;
    if (is(g,at_derive)){
      const gen n=a.size()>2?ev(a[2]):gen(1);
      if (n.type!=_INT_ || n.val<1 || n.val>4) return 0;
      for (int j=0;j<n.val;++j){
        gen b,e,c;
        if (ypart(t,y,b,e,c) && b==y){ const gen e1=rn(ev(e-1)),er=rn(ev(e)),c1=rn(ev(c*e)),c2=symb_prod(er,c),ce=pr(c1).size()<=pr(c2).size()?c1:c2; t=is_zero(e1)?ce:symb_prod(ce,is_one(e1)?y:symb_pow(y,e1)); } // (the power rule on c(k) x^e(k): (2k+4)(-1)^k/(2k+1)! x^(2k+3), not expanded; (k+1)/2 when shorter)
        else t=rn(ev(D(t,y)));
        for (int i=0;i<3 && is_zero(ev(subst(t,w[1],k0,false,CTX)));++i) k0=k0+1; // (the constant term's derivative is 0: from the next k)
      }
      const gen r=SYM(at_sum,makesequence(t,w[1],k0,w[3]));
      S(R_KDTERM,line(n.val==1?"diff(%0,%1)=%3":"diff(%0,%1,%2)=%3",is(a[0],at_derive)?u:a[0],y,n,r,zero,zero),0); // (a second derivative typed as diff(diff(...)): the first one's series, then this one)
      return r;
    }
    gen b,e,c;
    if (a.size()!=4 || !is_zero(ev(a[2])) || !ypart(t,y,b,e,c) || b!=y) return 0; // (c(k) t^e(k) from 0 to X: c X^(e+1)/(e+1))
    const gen E1=rn(ev(e+1)),r=SYM(at_sum,makesequence(qshow(symb_prod(symb_prod(c,symb_inv(E1)),symb_pow(a[3],E1))),w[1],k0,w[3]));
    S(R_KINT,line("integrate(%0,%1,0,%2)=%3",a[0],y,a[3],r,zero,zero),0);
    return r;
  }
  return useries(g);
}
int steps_user_series(const char * s,GIAC_CONTEXT){
  ctx_=contextptr;
  const gen g(s,contextptr);
  return !is_zero(useries(g));
}
#if defined FLOWCE_VECTORS || defined HOSTGIAC
#include "steps_m3.inc"
#include "steps_m3c.inc"
#endif
#if defined FLOWCE_VECTORS || defined HOSTGIAC
const int steps_m3_rule0=R_VGIVEN,steps_m3_noalign=R_VLN;
#endif
bool steps_of(const gen & input_,std::vector<step_t> & out,GIAC_CONTEXT){
  const gen input=steps_fixvar(input_,contextptr); // (lim of n^2/3^n typed in the x template)
  static gen * in0; // the input as the steps take it (in_ points to it; no static constructor in the app)
  if (!in0) in0=new gen;
  gen x(identificateur("x"));
  ctx_=contextptr; out_=&out; depth_=0; nsub_=0; var_=&x; in_=in0; gen la_home=plus_inf; la_=&la_home; ldir_=0; // (no limit point left from a call before: la_ pointed at a gone local)
  steps_methods=0; steps_method_name=""; rel_=0;
#if defined FLOWCE_VECTORS || defined HOSTGIAC
  if (m3_steps(input)){ *in0=input; return !out.empty(); } // (vectors: u x v, proj_v u, |u|, u+3v)
#endif
#ifdef WITH_DESOLVE
  if (has_op(input,*at_desolve)) return false; // (diff(y,x) in it is the equation: "d/dx y = 0" was shown)
#endif
#ifdef FLOWCE_SERIES
  { const gen u=useries(input); if (!is_zero(u)){ *in0=u; return !is_undef(one(norm(u))) && !out.empty(); } } // (f(1) of a series f: that series, its point in)
#endif
#if defined FLOWCE_SERIES || defined HOSTGIAC
  if (is(input,at_of) && arg(input).type==_VECT && arg(input)._VECTptr->size()==2 && sqi(arg(input,0))>=0){ *in0=input; return !is_undef(sqterm(sqi(arg(input,0)),ev(arg(input,1)),true)) && !out.empty(); } // (a(3) of a recurrence: term by term)
  if (is(input,at_limit) && arg(input).type==_VECT && arg(input)._VECTptr->size()>=3 && is(arg(input,0),at_of) && arg(arg(input,0)).type==_VECT && sqi(arg(arg(input,0),0))>=0){ *in0=input; return !is_undef(sqlim(sqi(arg(arg(input,0),0)),true)) && !out.empty(); } // (lim a(n) of a recurrence: L = its recurrence at L)
#endif
#ifdef FLOWCE_SERIES
  if (is(input,at_sto) && arg(input).type==_VECT && arg(input)._VECTptr->size()==2 && arg(input,0).is_symb_of_sommet(at_program) && arg(arg(input,0)).type==_VECT && arg(arg(input,0))._VECTptr->size()>=3 && is(arg(arg(input,0),2),at_sum)){
    const gen b=arg(arg(input,0),2); *in0=b; return !is_undef(one(norm(b))) && !out.empty(); } // (f(x) = sum(...) defined: the series' steps, its interval)
#endif
  { const bool so=input.is_symb_of_sommet(at_solve) && arg(input).type==_VECT && arg(input)._VECTptr->size()==2 && is(arg(input,0),at_inferieur_strict) && arg(input,1).type==_IDNT;
    const gen r=so?arg(input,0):input;
    if (is(r,at_inferieur_strict) && arg(r).type==_VECT && (so || lidnt(r).size()==1)){ // (|x-1|^2/9 < 1, with solve or alone)
      gen y=so?arg(input,1):lidnt(r)[0]; var_=&y; *in0=input;
      if (absineq(arg(r,0),ev(arg(r,1)),y)) return true;
      cut(0); var_=&x;
    } }
  if (is(input,at_subst) && arg(input).type==_VECT && !has_op(input,*at_derive)){ // p(0.1): the number put in, the value
    const vecteur & v=*arg(input)._VECTptr;
    gen y,a;
    if (v.size()==2 && is(v[1],at_equal)){ y=arg(v[1],0); a=arg(v[1],1); } else if (v.size()==3){ y=v[1]; a=v[2]; }
    if (y.type==_IDNT && !dep(a,y) && dep(v[0],y) && lidnt(ev(a)).empty()){
      gen f=v[0];
#ifdef FLOWCE_SERIES
      if (is(f,at_sum) && arg(f).type==_VECT && arg(f)._VECTptr->size()==4 && is_inf(ev(arg(f,3)))){ // (a value put in a power series: as typed with it in, sctr's 0^0 = 1, not giac's undef)
        const vecteur & w=*arg(f)._VECTptr;
        const gen s2=SYM(at_sum,makesequence(subst(w[0],y,a,true,CTX),w[1],w[2],w[3])),k0=ev(w[2]);
        if (k0.type==_INT_ && !is_undef(scenter(norm(s2)))){ // (the center: f(x) written out with x first, as the course writes it, then x = c in)
          vecteur tx; for (int j=k0.val;j<k0.val+3;++j) tx.push_back(ifold(subst(w[0],w[1],j,true,CTX)));
          S(R_SWX,line3("%0=%1",f,txt(wsum(tx,true))),0);
        }
        *in0=s2; return !is_undef(one(norm(s2))) && !out.empty();
      }
#endif
      if (is(f,at_series) || is(f,at_taylor)){ // (p_n(0.1) of series(...): its own steps first, p_n found, then the number in; no O(x^n))
        { const size_t m=mark(); if (is_undef(one(norm(serfix(f))))) cut(m); }
        vecteur t,q; flat(at_plus,ev(f),t); EACH(it,t) if (!has_op(*it,*at_order_size)) q.push_back(*it); f=q.empty()?gen(0):symb_plus(q); }
      vecteur tv; sterms(f,tv);
      const gen q=subst(tv.size()>1?symb_plus(tv):f,y,txt("("+pr(a)+")"),true,CTX); // (one sum, its terms as typed: 1-(0.4)/2-(0.4)^2/8, not (1-(0.4)/2)-(...))
      gen val=ev(sub(f,y,a)); // ((0.4) put in, not worked out)
      if (val.type==_STRNG || is_undef(val)) val=held(subst(f,y,a,true,CTX)); // (3^2018: kept as a power, giac's integer refused)
      *in0=input;
      S(R_SUBV,line3("%0=%1",q,val),0);
      return true;
    }
  }
  if ((input.is_symb_of_sommet(at_equal) || rel_of(input)) && one_var(input)) // (DeltaCE) typed alone: solved, as the console does
    return eq_steps(SYM2(at_solve,input,X)) && !out.empty();
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
  { bool big; if (!is_undef(dsteps_n(input,big))) return !out.empty(); if (big) return false; } // (f^(n)(a): before nest, which takes diff(f,x,3) apart)
  var_=&x;
  *in0=is_zero(ap)?serfix(input):ap;
#ifdef FLOWCE_SERIES
  { gen B,n,xp; if (rform(*in0,B,n)){ gen y=n; var_=&y; const size_t m=mark(); const gen L=rtest(norm(B),y,0,xp); var_=&x; if (finv(L) || is_inf(L)) return true; cut(m); } } // (the ratio test's limit typed: a_n, a_(n+1), the pairs)
#endif
  if (is(*in0,at_limit) && arg(*in0).type==_VECT && arg(*in0)._VECTptr->size()==3 && arg(*in0,1).type==_IDNT && ev(arg(*in0,2))==plus_inf){ // (a sequence's limit as typed first, as the console: the growth classes on it, not simplify's expanded quotient)
    gen y=arg(*in0,1),a=plus_inf; var_=&y; la_=&a; ldir_=0; budget_=80; trust_=false;
    const size_t m=mark(); const gen L=lsteps(pmerge(norm(arg(*in0,0)),y),0);
    var_=&x; la_=&la_home;
    if (trust_ && (finv(L) || is_inf(L)) && !out.empty()) return true;
    cut(m);
  }
  gen ls=limit_simp(*in0);
  { // a quotient with a sum on top, (2n^2+2^n)/2^n: its dominant terms explain it, not the simplified form
    gen f=is(*in0,at_limit) && arg(*in0).type==_VECT?norm(arg(*in0)[0]):gen(0);
    vecteur w; flat(at_prod,f,w);
    int ns=0,nt=0;
    EACH(it,w) if (!it->is_inv() && dep(*it,arg(*in0)[1])){ ++nt; ns+=it->is_plus() || (it->is_pow() && arg(*it,0).is_plus()); } // ((2^(n+1)+1)^3 too)
    if (ns==1 && nt==1) ls=*in0;
  }
  if (is(ls,at_limit) && arg(ls).type==_VECT && lop(arg(ls)[0],at_factorial).size()>1){ // (a ratio of n!'s: giac's limit took 3 minutes on the calculator)
    vecteur w=*arg(ls)._VECTptr; w[0]=_simplify(ev(w[0]),contextptr); ls=symbolic(at_limit,gen(w,_SEQ__VECT));
  }
  if (ls!=*in0){ if (pr(mergek(ls))!=pr(mergek(*in0))) S(R_SIMP,*in0,ls); *in0=ls; } // (no step that changes nothing on the screen)
  const gen & input0=*in0;
  gen e=nest(norm(input0));
  if (is(e,at_series)){ const size_t m0=mark(); if (!is_undef(one(e))) return !out.empty(); cut(m0); } // (as typed: an integral inside is integrated term by term)
  for (int guard=0;guard<8;++guard){
    gen t=inner(e);
    if (is_zero(t)) break;
    if (arg(t).type!=_VECT) return false;
    const vecteur & v=*arg(t)._VECTptr;
    if (v.size()<2 || v[1].type!=_IDNT) return false;
    if (t==e) return !is_undef(one(e));
    S(R_PART,t);
    ++depth_; gen val=one(t); --depth_;
    if (is_undef(val)) return false;
    e=quotesubst(e,t,val,CTX);
  }
  if (out.empty()) return false;
  e=ev(e);
  S(R_EQV,is_zero(lhs)?input0:lhs,bigp(e)?e:nrm(e));
  return true;
}
#endif
