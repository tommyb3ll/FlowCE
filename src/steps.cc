// steps.cc - step-by-step solutions (steps.h).
// Built for size: a step stores a rule number and its parts. Each rule's math line is a template
// (giac syntax; A, B, C the parts, V the variable, dV and dA their differentials) filled only when
// the step is shown. The methods share helpers: a rewritten integrand (via), a substitution (sub).
#if defined FLOWCE_STEPS || defined HOSTGIAC
#include "steps.h"
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
  R_SDIV, R_SRAT, R_SGEOM, R_PSUM, R_PSUMT, R_SLIM, R_PSER, R_LCMP, R_LFORM, R_AST, R_RATIO, R_ROOT, R_LNQ, R_ITEST, R_COSPI, R_LOSC, R_LDIVQ, R_NRULES
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
};

const char * steps_title(const step_t & s){ return RULES[3*s.rule]; }
const char * steps_formula(const step_t & s){ return RULES[3*s.rule+1]; }

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
  return mergek(subst(gen(RULES[3*s.rule+2],contextptr),*phm_,makevecteur(s.a,s.b,s.c,s.v,dname(s.v),dname(s.a),U,W,dname(U),dname(W)),true,contextptr));
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

static NOINL void S(int r,const gen & a,const gen & b,const gen & c){
  step_t s; s.rule=r; s.depth=depth_; s.a=a; s.b=b; s.c=c; s.v=X;
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
// integrate(f,x,a,b) with |g| in f: one integral per interval where g keeps its sign. |g| alone
// (area between curves): g simplified, one antiderivative, each interval evaluated with it.
static NOINL gen absplit(const gen & op,const gen & g0,const gen & a,const gen & b){
  gen f=arg(op,0),g=rn(g0);
  if (taille(g,200)>=taille(g0,200)) g=g0;
  bool alone=f==symb_abs(g0);
  gen z=_solve(makesequence(symb_equal(g,0),X),CTX);
  vecteur pts(1,a);
  if (z.type==_VECT)
    EACH(it,*z._VECTptr){
      gen zi=ev(*it),d=evalf(zi,1,CTX);
      if (d.type==_DOUBLE_ && is_strictly_greater(zi,a,CTX) && is_strictly_greater(b,zi,CTX)) pts.push_back(zi);
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
  S(R_ABS,op,symb_plus(pieces));
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
    return tot;
  }
  EACH(it,pieces){
    S(R_PART,*it,0);
    ++depth_; gen vi=one(*it); --depth_;
    if (is_undef(vi)) return vi;
    tot=tot+vi;
  }
  return ev(tot);
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
// every factor of g a constant to a power of n (or e^(...)): maybe geometric (no simplify of the
// ratio of (n/(2n+1))^n: seconds on the calculator)
static NOINL bool geo(const vecteur & w){
  EACH(it,w){ const gen & f=it->is_inv()?arg(*it):*it; if (!is(f,at_exp) && !(f.is_pow() && !dep(arg(f,0)))) return false; }
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
    if (!dep(r) && is_strictly_greater(1,evalf(abs(r,CTX),1,CTX),CTX)){
      gen t0=ev(sub(t,x,a));
      val=ev(t0/(1-r));
      S(R_SRAT,ratio_of(v[0],t,x),r);
      S(R_SGEOM,op,symb_prod(t0,symb_inv(symb_plus(1,symb_neg(r)))));
      S(R_EQV,op,val);
      return same(val,G)?val:gen(undef);
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
// the problem as typed (mergek without its rewrites: one denominator, 1/(n(n+2)), not 1/n/(n+2))
gen steps_problem(const gen & input,GIAC_CONTEXT){ ctx_=contextptr; raw_=true; gen r=mergek(nest(norm(serfix(input)))); raw_=false; return r; }
bool steps_of(const gen & input,std::vector<step_t> & out,GIAC_CONTEXT){
  static gen * in0; // the input as the steps take it (in_ points to it; no static constructor in the app)
  if (!in0) in0=new gen;
  gen x(identificateur("x"));
  ctx_=contextptr; out_=&out; depth_=0; nsub_=0; var_=&x; in_=in0;
  *in0=serfix(input);
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
  S(R_EQV,input0,bigp(e)?e:normal(e,CTX));
  return true;
}
#endif
