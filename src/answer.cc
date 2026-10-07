// FlowCE's answers (see answer.h): the steps do_run (main.cc) applies around giac's eval. Plain
// giac, no calculator calls: tools/hostgiac's -k option compiles this file too.
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#ifdef HOSTGIAC
#include <time.h> // (before giac: first.h defines a clock macro)
#endif
#include "answer.h"
#include <giac/kdisplay.h>

using namespace std;
extern giac::context * contextptr;
extern "C" {
  bool khicas_implicit_mult(char * line,int maxlen); // paper notation + TI implicit multiplication (tiinput_glue.cc)
  extern volatile unsigned char focus_phase; // focus.cc: the phase probe (tools/emu)
}
#ifdef HOSTGIAC
// ANSWER_TRACE=1 (host only): each step's result and its tree on stderr
#include <stdio.h>
static void tree(const giac::gen & g,std::string & o){
  using namespace giac;
  if (g.type==_SYMB){
    o+=g._SYMBptr->sommet.ptr()->s;
    o+='(';
    tree(g._SYMBptr->feuille,o);
    o+=')';
  }
  else if (g.type==_VECT){
    char b[16];
    sprintf(b,"v%d[",g.subtype);
    o+=b;
    for (unsigned i=0;i<g._VECTptr->size();++i){
      if (i) o+=',';
      tree((*g._VECTptr)[i],o);
    }
    o+=']';
  }
  else
    o+=g.print(contextptr);
}
static double trace_ms(){ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec*1e3+t.tv_nsec/1e6; }
static double trace_t0;
static void trace_time(const char * what){
  if (!getenv("ANSWER_TRACE"))
    return;
  const double t=trace_ms();
  fprintf(stderr,"  (%s: %.1f ms)\n",what,t-trace_t0);
  trace_t0=t;
}
static void trace_step(const char * what,const giac::gen & g){
  if (!getenv("ANSWER_TRACE"))
    return;
  std::string t;
  tree(g,t);
  fprintf(stderr,"  [%s] %s\n        %s\n",what,g.print(contextptr).c_str(),t.c_str());
}
#else
#define trace_step(w,g)
#define trace_time(w)
#endif

// Results a normalization may improve: sums, quotients, negative powers, numeric radicals
// other than sqrt(n). Products of plain powers (2^x*3^y, cos(x)*sin(x), sqrt(x)) are left alone.
static bool simplify_candidate(const giac::gen & g){
  using namespace giac;
  if (g.type==_VECT){
    for (const_iterateur it=g._VECTptr->begin();it!=g._VECTptr->end();++it){
      if (simplify_candidate(*it))
        return true;
    }
    return false;
  }
  if (g.type!=_SYMB)
    return false;
  const unary_function_ptr & u=g._SYMBptr->sommet;
  const gen & f=g._SYMBptr->feuille;
  if (u==at_plus || u==at_inv || u==at_tan) // (tan: tan(x)*cos(x) is sin(x))
    return true;
  if (u==at_pow && f.type==_VECT && f._VECTptr->size()==2){
    const gen & b=f._VECTptr->front(),e=f._VECTptr->back();
    if ((e.type==_INT_ || e.type==_FRAC) && is_strictly_positive(-e,contextptr))
      return true;
    if (e.type==_FRAC && (b.type==_FRAC || (b.type==_INT_ && !is_one(e*2))))
      return true;
  }
  return simplify_candidate(f);
}

// a term printed with a leading minus: -x^2, -2*x, -ln(...), -3
static bool neg_term(const giac::gen & t){
  using namespace giac;
  if (t.type==_INT_ || t.type==_ZINT || t.type==_DOUBLE_ || t.type==_FRAC)
    return is_strictly_positive(-t,contextptr);
  if (t.is_symb_of_sommet(at_neg))
    return true;
  if (t.is_symb_of_sommet(at_prod) && t._SYMBptr->feuille.type==_VECT && !t._SYMBptr->feuille._VECTptr->empty())
    return neg_term(t._SYMBptr->feuille._VECTptr->front());
  return false;
}
// the order of a product's factors as textbooks write them: numbers, pi, a variable or its power,
// e^(...), the rest (functions, sums); lists last, in their order (matrices do not commute)
static int factor_rank(const giac::gen & t){
  using namespace giac;
  if (t.type==_INT_ || t.type==_ZINT || t.type==_FRAC || t.type==_DOUBLE_)
    return 0;
  if (t.is_symb_of_sommet(at_inv) && (t._SYMBptr->feuille.type==_INT_ || t._SYMBptr->feuille.type==_ZINT))
    return 0; // 1/2 as inv(2): a number (e^(x^2)*(x^2-1)/2, not e^(x^2)/2*(x^2-1))
  if (t==cst_pi) // 2*pi*x
    return 1;
  if (t.type==_IDNT)
    return 2;
  if (t.is_symb_of_sommet(at_pow) && t._SYMBptr->feuille.type==_VECT && t._SYMBptr->feuille._VECTptr->size()==2
      && t._SYMBptr->feuille._VECTptr->front().type==_IDNT && !(t._SYMBptr->feuille._VECTptr->front()==cst_pi))
    return 2;
  if (t.is_symb_of_sommet(at_exp)) // e^x*cos(x), not cos(x)*e^x
    return 3;
  return t.type==_VECT?5:4;
}
// n/d with the sign in front: -3/(16(x-1)), not (-3)/(16(x-1)) (a sum then prints "- 3/...")
static giac::gen tb_quotient(const giac::gen & n,const giac::gen & d){
  using namespace giac;
  if ((n.type==_INT_ || n.type==_ZINT) && is_strictly_positive(-n,contextptr))
    return symbolic(at_neg,symbolic(at_division,makesequence(-n,d)));
  if (n.is_symb_of_sommet(at_prod) && n._SYMBptr->feuille.type==_VECT && !n._SYMBptr->feuille._VECTptr->empty()){
    const gen & c=n._SYMBptr->feuille._VECTptr->front(); // -3*x: -(3x)/d
    if ((c.type==_INT_ || c.type==_ZINT) && is_strictly_positive(-c,contextptr)){
      vecteur v(*n._SYMBptr->feuille._VECTptr);
      v.front()=-c;
      if (is_one(v.front()))
        v.erase(v.begin());
      const gen m=v.size()==1?v.front():symbolic(at_prod,gen(v,_SEQ__VECT));
      return symbolic(at_neg,symbolic(at_division,makesequence(m,d)));
    }
  }
  return symbolic(at_division,makesequence(n,d));
}
// the degree of a monomial c*x^k in its variable (v: 0 until the first one is seen), -1 if t is
// not one (another variable, a function...)
static int mono_deg(const giac::gen & t,giac::gen & v){
  using namespace giac;
  if (t.type==_INT_ || t.type==_ZINT || t.type==_FRAC)
    return 0;
  if (t.type==_IDNT){
    if (is_zero(v)) v=t;
    return t==v?1:-1;
  }
  if (t.type!=_SYMB)
    return -1;
  const gen & a=t._SYMBptr->feuille;
  if (t._SYMBptr->sommet==at_neg)
    return mono_deg(a,v);
  if (t._SYMBptr->sommet==at_inv)
    return a.type==_INT_ || a.type==_ZINT?0:-1;
  if (a.type!=_VECT)
    return -1;
  const vecteur & u=*a._VECTptr;
  if (t._SYMBptr->sommet==at_pow && u.size()==2 && u[1].type==_INT_ && u[1].val>0){
    const int d=mono_deg(u[0],v);
    return d==1?u[1].val:-1;
  }
  if (t._SYMBptr->sommet==at_division && u.size()==2 && (u[1].type==_INT_ || u[1].type==_ZINT))
    return mono_deg(u[0],v);
  if (t._SYMBptr->sommet==at_prod){
    int s=0;
    for (unsigned i=0;i<u.size();++i){
      const int d=mono_deg(u[i],v);
      if (d<0) return -1;
      s+=d;
    }
    return s;
  }
  return -1;
}
// a sum whose leading term is negative: the highest power of a polynomial in one variable (2-x^2),
// else most of its terms (-a-b+c)
static bool lead_negative(const giac::gen & p){
  using namespace giac;
  if (!p.is_symb_of_sommet(at_plus) || p._SYMBptr->feuille.type!=_VECT || p._SYMBptr->feuille._VECTptr->size()<2)
    return false;
  const vecteur & w=*p._SYMBptr->feuille._VECTptr;
  gen v=0;
  int best=-1,neg=0;
  bool lead=false,poly=true;
  for (unsigned k=0;k<w.size();++k){
    const int d=mono_deg(w[k],v);
    const bool n=neg_term(w[k]);
    neg+=n;
    if (d<0)
      poly=false;
    else if (d>best){
      best=d;
      lead=n;
    }
  }
  return poly?lead:2*neg>(int)w.size();
}
// -p, term by term: -(2-x^2) is -2+x^2
static giac::gen neg_sum(const giac::gen & p){
  using namespace giac;
  vecteur w(*p._SYMBptr->feuille._VECTptr);
  for (unsigned k=0;k<w.size();++k){
    gen & t=w[k];
    if (t.is_symb_of_sommet(at_neg)){
      const gen a=t._SYMBptr->feuille; // (a copy before t is replaced)
      t=a;
    }
    else if (t.type==_INT_ || t.type==_ZINT || t.type==_FRAC || t.type==_DOUBLE_)
      t=-t;
    else if (t.is_symb_of_sommet(at_prod) && t._SYMBptr->feuille.type==_VECT && !t._SYMBptr->feuille._VECTptr->empty() && neg_term(t._SYMBptr->feuille._VECTptr->front())){
      vecteur u(*t._SYMBptr->feuille._VECTptr);
      u.front()=-u.front();
      t=is_one(u.front()) && u.size()==2?u.back():symbolic(at_prod,gen(u,_SEQ__VECT));
    }
    else
      t=symbolic(at_neg,t);
  }
  return symbolic(at_plus,gen(w,_SEQ__VECT));
}
// t is a variable's power x^e (x: e=1), e rational
static bool var_power(const giac::gen & t,giac::gen & b,giac::gen & e){
  using namespace giac;
  if (t.type==_IDNT && !(t==cst_pi)){
    b=t;
    e=1;
    return true;
  }
  if (t.is_symb_of_sommet(at_pow) && t._SYMBptr->feuille.type==_VECT && t._SYMBptr->feuille._VECTptr->size()==2){
    const gen & x=t._SYMBptr->feuille._VECTptr->front(),& p=t._SYMBptr->feuille._VECTptr->back();
    if (x.type==_IDNT && !(x==cst_pi) && (p.type==_INT_ || p.type==_FRAC)){
      b=x;
      e=p;
      return true;
    }
  }
  if (t.is_symb_of_sommet(at_inv) && var_power(t._SYMBptr->feuille,b,e)){ // 1/x^(2/9): -2/9
    e=-e;
    return true;
  }
  return false;
}
// t is b^e or (b^f)^n (e=f*n), b a function, e not a number (sin(2x)^x, (sin(2x)^x)^3)
static bool pow_of(const giac::gen & t,giac::gen & b,giac::gen & e){
  using namespace giac;
  if (!t.is_symb_of_sommet(at_pow) || t._SYMBptr->feuille.type!=_VECT || t._SYMBptr->feuille._VECTptr->size()!=2)
    return false;
  b=t._SYMBptr->feuille._VECTptr->front();
  e=t._SYMBptr->feuille._VECTptr->back();
  if (e.type==_INT_ && b.is_symb_of_sommet(at_pow) && b._SYMBptr->feuille.type==_VECT && b._SYMBptr->feuille._VECTptr->size()==2){
    e=b._SYMBptr->feuille._VECTptr->back()*e;
    b=b._SYMBptr->feuille._VECTptr->front();
  }
  return b.type==_SYMB && !is_constant_wrt(e,lidnt(b).empty()?gen(0):lidnt(b).front(),contextptr) && e.type!=_INT_ && e.type!=_FRAC;
}
// the rational content of a sum: the gcd of its terms' numerators over the lcm of their
// denominators, negative when every term is (1/13 for 2sin(3x)/13-3cos(3x)/13, 2 for
// 8x^3-12x^2+12x-6, -1 for -x^2-2x-2); 1 when nothing comes out
// the number factor of a term: 3 for 3x^2, 2/13 for 2sin(3x)/13 (a division positive_first
// built), 1 for sin(x); false for a decimal
static bool num_coef(const giac::gen & t,giac::gen & c){
  using namespace giac;
  c=1;
  if (t.type==_INT_ || t.type==_ZINT || t.type==_FRAC){
    c=t;
    return true;
  }
  if (t.type==_DOUBLE_ || t.type==_CPLX)
    return false;
  if (t.type!=_SYMB)
    return true;
  const gen & f=t._SYMBptr->feuille;
  if (t._SYMBptr->sommet==at_neg)
    return num_coef(f,c);
  if (t._SYMBptr->sommet==at_inv){
    if (f.type==_INT_ || f.type==_ZINT)
      c=inv(f,contextptr);
    return true;
  }
  if ((t._SYMBptr->sommet==at_prod || t._SYMBptr->sommet==at_division) && f.type==_VECT){
    for (unsigned k=0;k<f._VECTptr->size();++k){
      gen d;
      if (!num_coef((*f._VECTptr)[k],d))
        return false;
      c=t._SYMBptr->sommet==at_division && k?c/d:c*d;
    }
  }
  return true;
}
static giac::gen sum_content(const giac::gen & p){
  using namespace giac;
  const vecteur & w=*p._SYMBptr->feuille._VECTptr;
  gen nu=0,de=1;
  bool allneg=true;
  for (unsigned k=0;k<w.size();++k){
    gen c;
    allneg=allneg && neg_term(w[k]);
    if (!num_coef(w[k],c))
      return 1;
    c=abs(c,contextptr);
    nu=gcd(nu,_numer(c,contextptr));
    de=lcm(de,_denom(c,contextptr));
  }
  if (is_zero(nu))
    return 1;
  const gen r=nu/de;
  return allneg?-r:r;
}
// a product's order: factor_rank, then polynomials of one variable by degree and constant term:
// (x-2)(x+2)(x^2+4), (x-2)(x-3) (giac's factor gave its factors in an order that changes with the
// session)
static bool poly_key(const giac::gen & t,int & deg,double & c0,giac::gen & v){
  using namespace giac;
  if (!t.is_symb_of_sommet(at_plus) || t._SYMBptr->feuille.type!=_VECT)
    return false;
  v=0;
  deg=0;
  c0=0;
  const vecteur & w=*t._SYMBptr->feuille._VECTptr;
  for (unsigned k=0;k<w.size();++k){
    const int d=mono_deg(w[k],v);
    if (d<0)
      return false;
    if (d>deg)
      deg=d;
    if (!d){
      const gen e=evalf(w[k],1,contextptr);
      if (e.type!=_DOUBLE_)
        return false;
      c0=e._DOUBLE_val;
    }
  }
  return true;
}
// sin, cos, sec, csc, tan, cot (or a power of one) in that order, as textbooks write them:
// 2sin(x)cos(x), sec(x)tan(x), -csc(x)cot(x) (the calculator's giac gave 2cos(x)sin(x))
static int trig_key(giac::gen t,int & size){
  using namespace giac;
  if (t.is_symb_of_sommet(at_pow) && t._SYMBptr->feuille.type==_VECT)
    t=t._SYMBptr->feuille._VECTptr->front();
  const unary_function_ptr * const fs[]={at_sin,at_cos,at_sec,at_csc,at_tan,at_cot};
  for (int k=0;k<6;++k)
    if (t.is_symb_of_sommet(fs[k])){
      size=taille(t._SYMBptr->feuille,64);
      return k;
    }
  return 6;
}
static bool factor_before(const giac::gen & a,const giac::gen & b){
  const int ra=factor_rank(a),rb=factor_rank(b);
  if (ra!=rb)
    return ra<rb;
  int sa,sb;
  const int ta=trig_key(a,sa),tb=trig_key(b,sb);
  if (ta<6 && tb<6) // the simpler argument first: sin(x)cos(x)sin(1+cos(x)^2)
    return sa<sb || (sa==sb && ta<tb);
  if (ra!=4)
    return false;
  int da,db;
  double ca,cb;
  giac::gen va,vb; // (one variable: (x+1)(y-1) stays)
  const bool pa=poly_key(a,da,ca,va),pb=poly_key(b,db,cb,vb);
  if (pa!=pb) // the polynomial first: (x^2-2)sin(x), not sin(x)(x^2-2) (the calculator's order)
    return pa;
  if (!pa || !(va==vb))
    return false;
  const double aa=ca<0?-ca:ca,ab=cb<0?-cb:cb; // (x-2)(x-3), (x-2)(x+2): the smaller constant, minus first
  return da<db || (da==db && (aa<ab || (aa==ab && ca<cb)));
}
// the power of the variable a term has as a factor: 3*x/8 1, x^2*e^x 2, sin(2x)/4 0
static __attribute__((noinline)) int x_power(const giac::gen & t){
  using namespace giac;
  if (t.is_symb_of_sommet(at_division) && t._SYMBptr->feuille.type==_VECT) // x^3/3
    return x_power(t._SYMBptr->feuille._VECTptr->front());
  if (t.is_symb_of_sommet(at_neg))
    return x_power(t._SYMBptr->feuille);
  // (60 times the exponent: x^(3/2) 90, x 60, sqrt(x) 30, so that 2x^(3/2)-3x+6sqrt(x))
  gen b,e;
  if (!var_power(t,b,e) && t.is_symb_of_sommet(at_prod) && t._SYMBptr->feuille.type==_VECT)
    for (const_iterateur it=t._SYMBptr->feuille._VECTptr->begin();it!=t._SYMBptr->feuille._VECTptr->end() && !var_power(*it,b,e);++it)
      ;
  const gen k=e*60;
  return k.type==_INT_ && k.val>0?k.val:k.type==_FRAC && is_strictly_positive(k,contextptr)?_floor(k,contextptr).val:0;
}
// positive_first's divisions as giac's products: ratnormal took 16*(x^2/8) for 16 times an unknown
// function (5-16x^2/8); eval rewrote cot as cos/sin
static giac::gen undiv(const giac::gen & g){
  using namespace giac;
  if (g.type==_VECT){
    vecteur w(*g._VECTptr);
    for (iterateur it=w.begin();it!=w.end();++it)
      *it=undiv(*it);
    return gen(w,g.subtype);
  }
  if (g.type!=_SYMB)
    return g;
  const gen f=undiv(g._SYMBptr->feuille);
  if (g._SYMBptr->sommet==at_division && f.type==_VECT && f._VECTptr->size()==2)
    return symbolic(at_prod,makesequence(f._VECTptr->front(),symb_inv(f._VECTptr->back())));
  return symbolic(g._SYMBptr->sommet,f);
}
// a sum of two terms starts with the positive one: 4-x^2, ln|x-2|-ln|x+2|, x-ln(e^x+1)
// (giac gives -x^2+4, -ln|x+2|+ln|x-2|, -ln(e^x+1)+x)
giac::gen positive_first(const giac::gen & g){
  using namespace giac;
  if (g.type==_VECT){
    vecteur w(*g._VECTptr);
    for (iterateur it=w.begin();it!=w.end();++it)
      *it=positive_first(*it);
    return gen(w,g.subtype);
  }
  if (g.type!=_SYMB || g._SYMBptr->sommet==at_program)
    return g;
  gen f=positive_first(g._SYMBptr->feuille);
  if (g._SYMBptr->sommet==at_plus && f.type==_VECT && f._VECTptr->size()>2){
    // a polynomial in one variable: descending powers (giac's ratnormal gave 20x^11+88x^9+...
    // +11x^12/2+...)
    gen v=0;
    vecteur & w=*f._VECTptr;
    int deg[64];
    unsigned k=0;
    for (;k<w.size() && k<64;++k)
      if ((deg[k]=mono_deg(w[k],v))<0) break;
    if (k<w.size() && w.size()<=64) // not a polynomial: terms c*x^k*(functions) by the degree of
      for (k=0;k<w.size();++k)          // their power of x, the others 0
        deg[k]=x_power(w[k]);
    for (unsigned i=1;i<w.size() && w.size()<=64;++i)
      for (unsigned j=i;j>0 && deg[j]>deg[j-1];--j){
        swapgen(w[j],w[j-1]);
        const int t=deg[j]; deg[j]=deg[j-1]; deg[j-1]=t;
      }
  }
  if (g._SYMBptr->sommet==at_plus && f.type==_VECT && f._VECTptr->size()==2 && neg_term(f._VECTptr->front()) && !neg_term(f._VECTptr->back())){
    vecteur & w=*f._VECTptr; // two terms only: -x^3+3*x^2-2 keeps its descending powers
    swapgen(w[0],w[1]);
  }
  if (g._SYMBptr->sommet==at_plus && f.type==_VECT && f._VECTptr->size()==2 && has_i(f._VECTptr->front()) && !has_i(f._VECTptr->back())){
    vecteur & w=*f._VECTptr; // a+bi: 1+i*sqrt(3), -1+i*sqrt(3) (the real part first)
    swapgen(w[0],w[1]);
  }
  if (g._SYMBptr->sommet==at_plus && f.type==_VECT){
    // -(2-x^2)*sin(x) is +(x^2-2)*sin(x): 2x*cos(x)+(x^2-2)*sin(x), not 2x*cos(x)-(2-x^2)*sin(x)
    for (unsigned k=0;k<f._VECTptr->size();++k){
      gen & t=(*f._VECTptr)[k];
      if (!t.is_symb_of_sommet(at_neg))
        continue;
      const gen & a=t._SYMBptr->feuille;
      vecteur fac=a.is_symb_of_sommet(at_prod) && a._SYMBptr->feuille.type==_VECT?*a._SYMBptr->feuille._VECTptr:vecteur(1,a);
      for (unsigned i=0;i<fac.size();++i){
        if (!lead_negative(fac[i]))
          continue;
        fac[i]=positive_first(neg_sum(fac[i]));
        t=fac.size()==1?fac[0]:symbolic(at_prod,gen(fac,_SEQ__VECT));
        break;
      }
    }
  }
  if (g._SYMBptr->sommet==at_neg){ // -(cos(x)-sin(x))*u: (sin(x)-cos(x))*u (a difference flips)
    vecteur w=f.is_symb_of_sommet(at_prod) && f._SYMBptr->feuille.type==_VECT?*f._SYMBptr->feuille._VECTptr:vecteur(1,f);
    for (unsigned i=0;i<w.size();++i){
      if (!w[i].is_symb_of_sommet(at_plus) || w[i]._SYMBptr->feuille.type!=_VECT || w[i]._SYMBptr->feuille._VECTptr->size()!=2)
        continue;
      const vecteur & d=*w[i]._SYMBptr->feuille._VECTptr;
      gen v0=0,v1=0;
      if (neg_term(d[0])==neg_term(d[1]) || (mono_deg(d[0],v0)>=0 && mono_deg(d[1],v1)>=0)) // (not -(x-2)(x-3))
        continue;
      w[i]=positive_first(neg_sum(w[i]));
      return w.size()==1?w[0]:symbolic(at_prod,gen(w,_SEQ__VECT));
    }
  }
  if (g._SYMBptr->sommet==at_pow && f.type==_VECT && f._VECTptr->size()==2 && f._VECTptr->back().type==_INT_){
    // (x^(1/3))^2 is x^(2/3); (x^(1/3))^-2 is 1/x^(2/3)
    const gen & b=f._VECTptr->front();
    if (b.is_symb_of_sommet(at_pow) && b._SYMBptr->feuille.type==_VECT && b._SYMBptr->feuille._VECTptr->size()==2
        && b._SYMBptr->feuille._VECTptr->front().type==_IDNT && b._SYMBptr->feuille._VECTptr->back().type==_FRAC){
      const gen e=(*b._SYMBptr->feuille._VECTptr)[1]*f._VECTptr->back(),x=b._SYMBptr->feuille._VECTptr->front();
      if (is_strictly_positive(-e,contextptr))
        return symb_inv(is_one(-e)?x:symbolic(at_pow,makesequence(x,-e)));
      return is_one(e)?x:symbolic(at_pow,makesequence(x,e));
    }
  }
  if (g._SYMBptr->sommet==at_prod && f.type==_VECT && f._VECTptr->size()>=2){
    vecteur & w=*f._VECTptr;
    // a product in the product, one product: 1/2*(2*ln|x|) (normal_args) is ln|x|, not 2ln|x|/2;
    // 1/d built below it is inv(d) again: sin(x)/(2(sin(x)^2-1)), not sin(x)*1/(2(...))
    for (unsigned i=0;i<w.size();++i)
      if (w[i].is_symb_of_sommet(at_division) && w[i]._SYMBptr->feuille.type==_VECT && is_one(w[i]._SYMBptr->feuille._VECTptr->front()))
        w[i]=symb_inv(w[i]._SYMBptr->feuille._VECTptr->back());
      else if (w[i].is_symb_of_sommet(at_prod) && w[i]._SYMBptr->feuille.type==_VECT && !w[i]._SYMBptr->feuille._VECTptr->empty()){
        const vecteur in=*w[i]._SYMBptr->feuille._VECTptr;
        w.erase(w.begin()+i);
        w.insert(w.begin()+i,in.begin(),in.end());
        i+=in.size()-1;
      }
    // b^-q is a denominator: 3x^2/(2sqrt(x^3+1)), not 3x^2/sqrt(x^3+1)/2
    for (unsigned i=0;i<w.size();++i)
      if (w[i].is_symb_of_sommet(at_pow) && w[i]._SYMBptr->feuille.type==_VECT && w[i]._SYMBptr->feuille._VECTptr->size()==2){
        const gen & e=w[i]._SYMBptr->feuille._VECTptr->back();
        if ((e.type==_FRAC || e.type==_INT_) && is_strictly_positive(-e,contextptr)){
          const gen & b=w[i]._SYMBptr->feuille._VECTptr->front();
          w[i]=symb_inv(is_one(-e)?b:symbolic(at_pow,makesequence(b,-e)));
        }
      }
    // x^2*sign(x) is x*|x| (the integral of |x|: x*|x|/2)
    for (unsigned i=0;i<w.size();++i){
      if (!w[i].is_symb_of_sommet(at_sign) || w[i]._SYMBptr->feuille.type!=_IDNT)
        continue;
      const gen v=w[i]._SYMBptr->feuille; // (a copy: w[i] is replaced below)
      for (unsigned j=0;j<w.size();++j){
        gen b,e;
        if (j==i || !var_power(w[j],b,e) || !(b==v) || e.type!=_INT_ || e.val<1)
          continue;
        w[i]=symbolic(at_abs,v);
        w[j]=e.val==1?gen(1):e.val==2?v:symbolic(at_pow,makesequence(v,e-1));
        break;
      }
    }
    // a sum's numbers out: e^(2x)*(2sin(3x)/13-3cos(3x)/13) is e^(2x)(2sin(3x)-3cos(3x))/13,
    // e^(2x)(8x^3-12x^2+12x-6)/16 is e^(2x)(4x^3-6x^2+6x-3)/8, e^(-x)(-x^2-2x-2) is -e^(-x)(x^2+2x+2)
    for (unsigned i=0;i<w.size();++i){
      if (!w[i].is_symb_of_sommet(at_plus) || w[i]._SYMBptr->feuille.type!=_VECT)
        continue;
      const gen c=sum_content(w[i]);
      if (is_one(c))
        continue;
      w[i]=positive_first(ratnormal(undiv(w[i])/c,contextptr));
      w.insert(w.begin(),c); // (the sum moves to i+1: the loop goes on after it)
      ++i;
    }
    // powers of one function with exponents in x merged: giac keeps sin(2x)^(4x) as
    // sin(2x)^x*(sin(2x)^x)^3 (the derivative of sin(2x)^(4x))
    for (unsigned i=0;i<w.size();++i){
      gen bi,ei;
      if (!pow_of(w[i],bi,ei))
        continue;
      for (unsigned j=i+1;j<w.size();++j){
        gen bj,ej;
        if (!pow_of(w[j],bj,ej) || !(bj==bi))
          continue;
        ei=ratnormal(ei+ej,contextptr);
        w.erase(w.begin()+j);
        --j;
        w[i]=symbolic(at_pow,makesequence(bi,ei));
      }
    }
    // the powers of one variable merged: x*x^(1/3) is x^(4/3), 1/(x^(2/9)*x) is 1/x^(11/9)
    for (unsigned i=0;i<w.size();++i){
      gen bi,ei;
      if (!var_power(w[i],bi,ei))
        continue;
      bool merged=false;
      for (unsigned j=i+1;j<w.size();++j){
        gen bj,ej;
        if (!var_power(w[j],bj,ej) || !(bj==bi))
          continue;
        ei=ei+ej;
        w.erase(w.begin()+j);
        --j;
        merged=true;
      }
      if (!merged)
        continue;
      if (is_zero(ei) && w.size()>1){ // x^2/x^2: 1
        w.erase(w.begin()+i);
        --i;
        continue;
      }
      const bool den=is_strictly_positive(-ei,contextptr);
      const gen p=den?-ei:ei,q=is_zero(p)?gen(1):is_one(p)?bi:symbolic(at_pow,makesequence(bi,p));
      w[i]=den?symb_inv(q):q;
    }
    if (w.size()==1)
      return w[0];
    // factors: numbers, then x and its powers, then the rest (x*cos(x), 3*x^2*e^(3x); giac:
    // cos(x)*x, x^2*3*exp(3*x)); a stable insertion sort
    for (unsigned i=1;i<w.size();++i)
      for (unsigned j=i;j>0 && factor_before(w[j],w[j-1]);--j)
        swapgen(w[j],w[j-1]);
    while (w.size()>=2 && factor_rank(w[0])==0 && factor_rank(w[1])==0){ // 3*2*x: 6*x (diff(f(x),x,2))
      w[0]=eval(w[0]*w[1],1,contextptr); // (3*inv(2): 3/2)
      w.erase(w.begin()+1);
    }
    if (w.size()>=2 && is_one(w[0]))
      w.erase(w.begin());
    if (w.size()==1)
      return w[0];
    // two denominators or more, one fraction: (18*x-7)*inv(9)*inv(x^2+9) is (18x-7)/(9(x^2+9)), not
    // ((18x-7)/9)/(x^2+9) (partfrac)
    // (a fraction coefficient's denominator too: 3x^2/(2sqrt(x^3+1)), not 3/2*x^2/sqrt(x^3+1),
    // printed 3*x^2/sqrt(x^3+1)/2)
    int ninv=0;
    for (unsigned i=0;i<w.size();++i)
      ninv+=w[i].is_symb_of_sommet(at_inv);
    const bool fr0=w[0].type==_FRAC && is_strictly_positive(w[0]._FRACptr->den,contextptr) && !is_one(w[0]._FRACptr->den);
    if (ninv>=2 || (ninv>=1 && fr0)){
      vecteur nu,de;
      for (unsigned i=0;i<w.size();++i){
        if (!i && fr0){
          if (!is_one(w[0]._FRACptr->num))
            nu.push_back(w[0]._FRACptr->num);
          de.push_back(w[0]._FRACptr->den);
          continue;
        }
        (w[i].is_symb_of_sommet(at_inv)?de:nu).push_back(w[i].is_symb_of_sommet(at_inv)?w[i]._SYMBptr->feuille:w[i]);
      }
      const gen n=nu.empty()?gen(1):nu.size()==1?nu.front():symbolic(at_prod,gen(nu,_SEQ__VECT));
      return tb_quotient(n,symbolic(at_prod,gen(de,_SEQ__VECT)));
    }
    // 1/2*(x+1) -> (x+1)/2, 3/2*u -> 3*u/2: arctan((x+1)/2), not arctan(1/2*(x+1)) (the 1/2: a
    // fraction or inv(2))
    const bool fr=w.size()==2 && w[0].type==_FRAC && is_strictly_positive(w[0]._FRACptr->den,contextptr);
    const bool iv=w.size()==2 && w[0].is_symb_of_sommet(at_inv) && w[0]._SYMBptr->feuille.type==_INT_ && w[0]._SYMBptr->feuille.val>1;
    if (fr || iv){
      const gen q=iv?w[0]._SYMBptr->feuille:w[0]._FRACptr->den,p=iv?gen(1):w[0]._FRACptr->num;
      if (w[1].is_symb_of_sommet(at_inv)) // 7/9*inv(x^2): 7/(9x^2), not (7/x^2)/9
        return tb_quotient(p,symbolic(at_prod,makesequence(q,w[1]._SYMBptr->feuille)));
      const gen num=is_one(p)?w[1]:symbolic(at_prod,makesequence(p,w[1]));
      return tb_quotient(num,q);
    }
    if (w[0]==-1){ // -1*e^(-x)*(x^2+2x+2): -e^(-x)*(x^2+2x+2)
      w.erase(w.begin());
      return symbolic(at_neg,w.size()==1?w[0]:symbolic(at_prod,f));
    }
  }
  return symbolic(g._SYMBptr->sommet,f);
}

// the answer has tan or a power of sin or cos (sin(x)^2, 1/cos(x)): sin^2+cos^2=1 can shorten
// it; e^x*(cos(x)+sin(x)) cannot (the rewrite cost 0.5 s there for nothing)
static bool trig_power(const giac::gen & g){
  using namespace giac;
  if (g.type==_VECT){
    for (const_iterateur it=g._VECTptr->begin();it!=g._VECTptr->end();++it)
      if (trig_power(*it)) return true;
    return false;
  }
  if (g.type!=_SYMB)
    return false;
  const gen & a=g._SYMBptr->feuille;
  if (g._SYMBptr->sommet==at_tan)
    return true;
  if ((g._SYMBptr->sommet==at_pow && a.type==_VECT && a._VECTptr->size()==2) || g._SYMBptr->sommet==at_inv){
    const gen & b=g._SYMBptr->sommet==at_inv?a:a._VECTptr->front();
    if (b.is_symb_of_sommet(at_sin) || b.is_symb_of_sommet(at_cos))
      return true;
  }
  return trig_power(a);
}

// Automatic normalization of results. giac's default autosimplify ("regroup") leaves
// (4*sqrt(2)*pi+pi)/2-pi*(-4*sqrt(2)+1)/2, (x^2-1)/(x-1) or 2*x/(2*sqrt(x^2+1)) as is.
// ratnormal fixes those (4*sqrt(2)*pi, x+1, x/sqrt(x^2+1)) quickly: radicals, pi, sin(x)...
// are just symbols to it. simplify() is only used on tiny trig expressions without radicals
// (sin(x)^2+cos(x)^2 -> 1). History: simplify under a time budget (interrupted through
// control_c) exited the app on diff(sqrt(x^2+1),x) and reset the calculator after 80 s on
// cos(pi/12); KhiCAS must never interrupt giac on its own.
// normal() of g when it has one radical, a square root (the trig substitution answers it shortens):
// with several roots (x^(1/6), x^(1/4), sqrt(x)) normal builds an algebraic extension for each
// and took 15 s on the PC (hours on the calculator) for the derivative of x^(1/6)-7x^(1/4)+3sqrt(x),
// to be thrown away; g then
// (radicals: 1 if g's radicals are square roots of one base (or none), -1 if a base has roots of
// different indices (x^(1/9), x^(1/3), sqrt(x)): factor builds the same extensions for those
// (d/dx 7x^(4/9)-2sqrt(x^7)+x^(4/3) never ended on the calculator), 0 otherwise)
static int radicals(const giac::gen & g){
  using namespace giac;
  const vecteur v=lop(g,at_pow);
  vecteur bases,dens;
  for (const_iterateur it=v.begin();it!=v.end();++it){
    const gen & f=it->_SYMBptr->feuille;
    if (f.type!=_VECT || f._VECTptr->size()!=2 || f._VECTptr->back().type!=_FRAC)
      continue;
    const gen & b=f._VECTptr->front(),& d=f._VECTptr->back()._FRACptr->den;
    unsigned i=0;
    while (i<bases.size() && !is_zero(ratnormal(bases[i]-b,contextptr)))
      ++i;
    if (i<bases.size() && !(dens[i]==d))
      return -1;
    if (i==bases.size()){
      bases.push_back(b);
      dens.push_back(d);
    }
  }
  return bases.size()>1 || (bases.size()==1 && !(dens.front()==2))?0:1;
}
static giac::gen safe_normal(const giac::gen & g){
  return radicals(g)==1?giac::normal(g,contextptr):g;
}
// a sum's terms rational in its variable, collected: the 4th derivative of ln(x^6)-cos(4x) was
// 360/x^4-720/x^4-...-2016/x^4-256cos(4x)+..., now -36/x^4-256cos(4x)+... (kept if shorter)
static giac::gen collect_rational(const giac::gen & g){
  using namespace giac;
  const vecteur ids=lidnt(g);
  if (!g.is_symb_of_sommet(at_plus) || g._SYMBptr->feuille.type!=_VECT || ids.size()!=1)
    return g;
  gen r=0;
  vecteur rest;
  int n=0;
  for (const_iterateur it=g._SYMBptr->feuille._VECTptr->begin();it!=g._SYMBptr->feuille._VECTptr->end();++it){
    const vecteur lv=lvar(*it);
    if (lv.size()==1 && lv.front()==ids.front()){
      r=r+*it;
      ++n;
    }
    else
      rest.push_back(*it);
  }
  if (n<2)
    return g;
  rest.insert(rest.begin(),ratnormal(r,contextptr));
  const gen s=rest.size()==1?rest.front():symbolic(at_plus,gen(rest,_SEQ__VECT));
  return taille(s,1000)<taille(g,1000)?s:g;
}
static giac::gen auto_simplify(const giac::gen & g){
  using namespace giac;
  if ((g.type!=_SYMB && g.type!=_VECT) || is_undef(g) || taille(g,100)>=100)
    return g;
  if (g.type==_SYMB && g._SYMBptr->sommet==at_program)
    return g;
#ifdef WITH_PLOT
  if (xcas::ispnt(g))
    return g;
#endif
  if (!simplify_candidate(g))
    return g;
  // giac's simplify took 5.8 s of the 6.4 s of d/dx sin(x)/x (on any small trig answer): trig
  // answers now get ratnormal and the cheap sin^2+cos^2=1 rewrite below
  const bool trig=false;
  gen s=ratnormal(g,contextptr);
  trace_step("ratnormal",s);
  if (!trig && g.type==_SYMB && radicals(s)>=0){ // factored denominator: 1/(x+1)^2, not 1/(x^2+2x+1)
    gen d=_denom(s,contextptr);
    if (d.type==_SYMB){
      gen n=_numer(s,contextptr),f=_factor(d,contextptr);
      if (!is_undef(f) && f.type!=_STRNG)
        s=is_one(n)?symb_inv(f):symb_prod(n,symb_inv(f));
    }
  }
  trace_step("factored denominator",s);
  if (is_undef(s) || s.type==_STRNG || taille(s,1000)>taille(g,1000)) // not if bigger: pi*(x+1) stays;
    s=g;                                                                // same size: 3*x^2*e^(3x), not x^2*3*e^(3x)
  if (!trig && !xcas::has_radical(g) && (trig_power(g) || trig_power(s))){ // (s: cos(x)*cos(x) is cos(x)^2)
    // trig of a variable: in sin and cos, with sin^2+cos^2=1 (cheap, unlike simplify):
    // d/dx ln(sec(x)+tan(x)) is 1/cos(x), not (1+tan(x)^2+sin(x)/cos(x)^2)/(1/cos(x)+tan(x))
    bool var=false;
    const vecteur ids=lidnt(g);
    for (unsigned k=0;k<ids.size();++k)
      if (!(ids[k]==cst_pi))
        var=true;
    if (var){
      const gen sc=_tan2sincos(g,contextptr);
      for (int k=0;k<3;++k){ // (k=2 trig combined: cos(2x) for the derivative of sin(x)cos(x))
        const gen t=k==2?_tlin(g,contextptr):ratnormal(k?_trigcos(sc,contextptr):_trigsin(sc,contextptr),contextptr);
        if (!is_undef(t) && t.type!=_STRNG && taille(t,1000)<taille(s,1000))
          s=t;
      }
    }
  }
  if (!trig && xcas::has_radical(s) && !lidnt(s).empty()){ // a trig substitution: normal gives
    const gen n=safe_normal(s);                           // -sqrt(4-x^2)/(4*x) for
    if (!is_undef(n) && n.type!=_STRNG && taille(n,1000)<taille(s,1000)) // (x^2+2*sqrt(4-x^2)-4)/(4*(sqrt(4-x^2)-2)*x)
      s=n;
    trace_step("normal",n);
  }
  if (!trig && xcas::has_radical(s)){ // after a u-substitution: (p/u)*u^(3/2), not p*sqrt(u)
    const gen m=xcas::merge_sqrt(s,contextptr);
    if (!(m==s))
      return m;
  }
  return s;
}

// A definite integral over constant bounds whose integrand holds a square root (or another
// fractional power) of something that is not a polynomial of degree <= 2 in the variable almost
// never has an elementary antiderivative: giac then searches until memory runs out (exam day:
// sqrt(1+sin(x)^4*cos(x)^6) from 0 to 2 left the calculator dead; the ellipse perimeter took
// 80 s and exited). Those are integrated numerically: their bounds are made decimal, which makes
// giac integrate numerically. sqrt(1+16x^2) or sqrt(9-x^2) stay exact. KhiCAS cannot stop giac
// on a timer instead (see auto_simplify).
// a radicand (evaluated) of degree <= 2 in x: its third derivative is 0
static bool quadratic_radicand(const giac::gen & b,const giac::gen & x){
  using namespace giac;
  const gen d3=ratnormal(derive(derive(derive(b,x,contextptr),x,contextptr),x,contextptr),contextptr);
  if (is_zero(d3))
    return true;
  const gen v1=evalf(subst(d3,x,gen(0.618),false,contextptr),1,contextptr),v2=evalf(subst(d3,x,gen(1.414),false,contextptr),1,contextptr);
  return v1.type==_DOUBLE_ && v2.type==_DOUBLE_ && v1._DOUBLE_val*v1._DOUBLE_val<1e-10 && v2._DOUBLE_val*v2._DOUBLE_val<1e-10;
}
// what stays under the square root of f (factored): the product of its factors of odd power
// (c*p^2*q/r^4: q)
static void odd_factors(const giac::gen & f,const giac::gen & x,giac::gen & rest){
  using namespace giac;
  if (is_constant_wrt(f,x,contextptr))
    return;
  if (f.type==_SYMB){
    const gen & a=f._SYMBptr->feuille;
    if (f._SYMBptr->sommet==at_prod && a.type==_VECT){
      for (const_iterateur it=a._VECTptr->begin();it!=a._VECTptr->end();++it)
        odd_factors(*it,x,rest);
      return;
    }
    if (f._SYMBptr->sommet==at_inv || f._SYMBptr->sommet==at_neg){
      odd_factors(a,x,rest);
      return;
    }
    if (f._SYMBptr->sommet==at_pow && a.type==_VECT && a._VECTptr->size()==2 && a._VECTptr->back().type==_INT_){
      if (a._VECTptr->back().val%2)
        rest=rest*a._VECTptr->front();
      return;
    }
  }
  rest=rest*f;
}
static bool hard_radicand(const giac::gen & f,const giac::gen & x){
  using namespace giac;
  if (f.type!=_SYMB)
    return false;
  const gen & a=f._SYMBptr->feuille;
  gen base;
  bool frac=false;
  if (f._SYMBptr->sommet==at_sqrt){
    base=a;
    frac=true;
  }
  else if (f._SYMBptr->sommet==at_pow && a.type==_VECT && a._VECTptr->size()==2){
    const gen & e=a._VECTptr->back();
    frac=e.type==_FRAC || (e.type==_SYMB && e._SYMBptr->sommet==at_division);
    base=a._VECTptr->front();
  }
  if (frac && !is_constant_wrt(base,x,contextptr)){
    // evaluated first: derive leaves the parsed 1-(x^2) as diff(1-(x^2),x,3), and keeps
    // (3/2*sqrt(x))^2 unsimplified, so sqrt(1-x^2) and arc lengths went numeric (2026-10-06).
    // Factored, textbook arc lengths are easy too: 4t^2+9t^4 = t^2(9t^2+4), perfect squares.
    const gen b=eval(base,1,contextptr);
    if (!quadratic_radicand(b,x)){
      gen rest=1;
      odd_factors(_factor(normal(b,contextptr),contextptr),x,rest);
      if (!quadratic_radicand(rest,x))
        return true;
    }
  }
  if (a.type==_VECT){
    for (const_iterateur it=a._VECTptr->begin();it!=a._VECTptr->end();++it)
      if (hard_radicand(*it,x))
        return true;
    return false;
  }
  return hard_radicand(a,x);
}
// The textbook antiderivatives of sec, csc, their squares and cubes, tan^2 and cot^2 (of a linear
// u = a*x+b): giac took 18-34 s on the calculator for them and answered ln(|sin(x)+1/sin(x)+2|)/4-...
// for sec(x). Returns 0 for anything else (tan, cot: trig_powers).
static giac::gen table_integral(const giac::gen & g); // (below: table0, then trig_powers)
static giac::gen table0(const giac::gen & g){
  using namespace giac;
  if (!(g.is_symb_of_sommet(at_integrate) || g.is_symb_of_sommet(at_int)) || g._SYMBptr->feuille.type!=_VECT || g._SYMBptr->feuille._VECTptr->size()!=2)
    return 0;
  const gen & x=(*g._SYMBptr->feuille._VECTptr)[1];
  if (x.type!=_IDNT)
    return 0;
  const gen & f0=(*g._SYMBptr->feuille._VECTptr)[0];
  if (f0.is_symb_of_sommet(at_neg)){ // -cot(x): -ln|sin(x)| (by parts of x*csc(x)^2)
    const gen r=table_integral(symbolic(at_integrate,makesequence(f0._SYMBptr->feuille,x)));
    return is_zero(r)?r:symbolic(at_neg,r);
  }
  if (f0.is_symb_of_sommet(at_prod) && f0._SYMBptr->feuille.type==_VECT){ // 3*sec(x)^2, 2*tan(x)
    gen c=1,t=1;
    const vecteur & w=*f0._SYMBptr->feuille._VECTptr;
    for (unsigned i=0;i<w.size();++i)
      (is_constant_wrt(w[i],x,contextptr)?c:t)=(is_constant_wrt(w[i],x,contextptr)?c:t)*w[i];
    if (!is_one(c) && !is_constant_wrt(t,x,contextptr)){
      const gen r=table_integral(symbolic(at_integrate,makesequence(t,x)));
      return is_zero(r)?r:symbolic(at_prod,makesequence(c,r));
    }
  }
  // f = cos(u)^-n or sin(u)^-n, however written: sec(u)^3, 1/cos(u)^3, (1/cos(u))^3, cos(u)^-3;
  // tan(u)^2 = sec(u)^2-1, cot(u)^2 = csc(u)^2-1: tan(u)/a-x, -cot(u)/a-x (giac gave
  // 2*(tan(x/2)/4-1/(4*tan(x/2))-x/2) for cot(x)^2)
  // (the typed form first: cot(x)^2 is (cos(x)/sin(x))^2 evaluated, 1/cos(x)^3 only evaluated)
  gen d;
  int n,sq;
  for (int p=0;;++p){
    d=p?eval(f0,1,contextptr):f0;
    n=1;
    sq=0;
    for (int k=0;k<4;++k){
      if (d.is_symb_of_sommet(at_inv)){
        n=-n;
        d=d._SYMBptr->feuille;
      }
      else if (d.is_symb_of_sommet(at_sec) || d.is_symb_of_sommet(at_csc) || ((d.is_symb_of_sommet(at_tan) || d.is_symb_of_sommet(at_cot)) && ++sq)){
        n=-n;
        d=symbolic(d.is_symb_of_sommet(at_sec) || d.is_symb_of_sommet(at_tan)?at_cos:at_sin,d._SYMBptr->feuille);
      }
      else if (d.is_symb_of_sommet(at_pow) && d._SYMBptr->feuille.type==_VECT && d._SYMBptr->feuille._VECTptr->size()==2
               && d._SYMBptr->feuille._VECTptr->back().type==_INT_){
        n*=d._SYMBptr->feuille._VECTptr->back().val;
        d=d._SYMBptr->feuille._VECTptr->front();
      }
      else
        break;
    }
    if ((n==-1 || n==-2 || n==-3) && (!sq || n==-2) && sq<2)
      break;
    if (p)
      return 0;
  }
  n=-n;
  const bool c=d.is_symb_of_sommet(at_cos);
  if (!c && !d.is_symb_of_sommet(at_sin))
    return 0;
  const gen u=d._SYMBptr->feuille,a=derive(eval(u,1,contextptr),x,contextptr);
  if (is_zero(a) || !is_constant_wrt(a,x,contextptr))
    return 0;
  // ln|sec(u)+tan(u)|, ln|csc(u)-cot(u)|; cubes: (sec(u)*tan(u)+ln|sec(u)+tan(u)|)/2,
  // (-csc(u)*cot(u)+ln|csc(u)-cot(u)|)/2; divided by a: sec(3x) /3, sec(x/2) 2*...
  // squares: tan(u), -cot(u) (giac: -2/(2*tan(x)) for csc(x)^2). Parsed: as trees they took KBs.
  static const char * const T[]={"ln(abs(sec(u)+tan(u)))","tan(u)","sec(u)*tan(u)+ln(abs(sec(u)+tan(u)))",
                                 "ln(abs(csc(u)+-cot(u)))","-cot(u)","-(csc(u)*cot(u))+ln(abs(csc(u)+-cot(u)))"};
  gen r=quotesubst(gen(T[c?n-1:n+2],contextptr),gen("u",contextptr),u,contextptr),q=(n==3?2:1)*a;
  // over q = 2a for the cubes: csc(x/2)^2 -2*cot(x/2), csc(x/2)^3 without /2, tan(-x)^2 -tan(-x)-x
  bool ng=r.is_symb_of_sommet(at_neg);
  if (ng)
    r=r._SYMBptr->feuille;
  if (q.type==_FRAC){
    r=symbolic(at_prod,makesequence(q._FRACptr->den,r));
    q=q._FRACptr->num;
  }
  if (q.type==_INT_ && q.val<0){
    q=-q;
    ng=!ng;
  }
  if (!is_one(q))
    r=symbolic(at_division,makesequence(r,q));
  if (ng)
    r=symbolic(at_neg,r);
  return sq?symbolic(at_plus,makesequence(r,symbolic(at_neg,x))):r;
}

// sin^m(v)cos^n(v), tan, sec, csc and cot written as those (tan^3*sec^10 is sin^3*cos^-13), v=a*x+b,
// by the textbook substitutions: tan(v) for a power of sec (cot(v) for csc), sin(v) for an odd
// power of cos, cos(v) for an odd power of sin: a polynomial or rational function of t integrated,
// t put back unevaluated (sin(3x)^8cos(3x)^5: sin^9/27-2sin^11/33+sin^13/39). giac went through
// tan(x/2): tan(6x)^3*sec(6x)^10 over 90 s on the calculator, sin(3x)^8*cos(3x)^5 39 s. A sum (or a
// product with one): term by term. 0 if not such a product.
// h^e into c*sin(v)^m*cos(v)^n (products, powers, 1/... and - in any nesting: giac's expand gave
// (1/sin(x/2))^4/sin(x/2)^2); false if h has another function of x, or two arguments
static bool trig_factor(const giac::gen & h,int e,const giac::gen & x,giac::gen & c,giac::gen & v,int & m,int & n){
  using namespace giac;
  static const unary_function_ptr * const fs[]={at_sin,at_cos,at_tan,at_sec,at_csc,at_cot};
  if (is_constant_wrt(h,x,contextptr)){
    c=c*pow(h,e,contextptr);
    return true;
  }
  if (h.type!=_SYMB)
    return false;
  const gen & a=h._SYMBptr->feuille;
  if (h._SYMBptr->sommet==at_neg){
    if (e%2)
      c=-c;
    return trig_factor(a,e,x,c,v,m,n);
  }
  if (h._SYMBptr->sommet==at_inv)
    return trig_factor(a,-e,x,c,v,m,n);
  if (h._SYMBptr->sommet==at_prod && a.type==_VECT){
    for (const_iterateur it=a._VECTptr->begin();it!=a._VECTptr->end();++it)
      if (!trig_factor(*it,e,x,c,v,m,n))
        return false;
    return true;
  }
  if (h._SYMBptr->sommet==at_pow && a.type==_VECT && a._VECTptr->size()==2 && a._VECTptr->back().type==_INT_)
    return trig_factor(a._VECTptr->front(),e*a._VECTptr->back().val,x,c,v,m,n);
  int k=0;
  while (k<6 && h._SYMBptr->sommet!=fs[k])
    ++k;
  if (k==6 || (!is_zero(v) && !(a==v)))
    return false;
  v=a;
  m+=k==0 || k==2?e:k==4 || k==5?-e:0; // sin^m cos^n
  n+=k==1 || k==5?e:k==2 || k==3?-e:0;
  return true;
}
static giac::gen trig_powers(const giac::gen & f0,const giac::gen & x,int depth=0){
  using namespace giac;
  gen f=!depth && f0.is_symb_of_sommet(at_division)?eval(f0,1,contextptr):f0; // ((cos(x)^3+sin(x))/cos(x)^2: a product)
  if (f.is_symb_of_sommet(at_prod) && depth==0 && f._SYMBptr->feuille.type==_VECT)
    for (const_iterateur it=f._SYMBptr->feuille._VECTptr->begin();it!=f._SYMBptr->feuille._VECTptr->end();++it)
      if (it->is_symb_of_sommet(at_plus) || it->is_symb_of_sommet(at_binary_minus)){
        f=expand(f,contextptr); // csc(x/2)cot(x/2)(csc(x/2)^6+3csc(x/2)^4-8csc(x/2))
        break;
      }
  if (f.is_symb_of_sommet(at_plus) && f._SYMBptr->feuille.type==_VECT && depth<2){
    gen r=0;
    for (const_iterateur it=f._SYMBptr->feuille._VECTptr->begin();it!=f._SYMBptr->feuille._VECTptr->end();++it){
      const gen t=trig_powers(*it,x,2);
      if (is_zero(t))
        return 0;
      r=r+t;
    }
    return r;
  }
  gen c=1,v=0;
  int m=0,n=0;
  if (!trig_factor(f,1,x,c,v,m,n))
    return 0;
  const gen a=derive(eval(v,1,contextptr),x,contextptr); // (evaluated: the parsed x/3 gave none)
  if (is_zero(v) || is_zero(a) || !is_constant_wrt(a,x,contextptr))
    return 0;
  // the substitution t: tan(v) for tan^m sec^k (m>=0, k=-(m+n)>=2 even), cot(v) for cot^n csc^k,
  // sin(v) for an odd power of cos, cos(v) for an odd power of sin: first the one that leaves a
  // polynomial, the smaller (sin(x)cos(x)^3: -cos^4/4; tan^3: sec^2/2+ln|cos|) (x stands for t)
  const int k=-(m+n),r=k>=2 && k%2==0 && (m>=0 || n>=0)?(m>=0?0:1):n%2 && n>=1 && (!(m%2 && m>=1) || n<=m)?2:m%2 && m>=1?3:n%2?2:m%2?3:4;
  if (r==4)
    return 0;
  // the integrand in t: t^e*(1+w*t^2)^j, times cf (- for cot and cos)
  const int e=r==0 || r==2?m:n,j=r<2?k/2-1:r==2?(n-1)/2:(m-1)/2,w=r<2?1:-1;
  // a polynomial: its binomial terms integrated one by one, in ascending powers as textbooks
  // write them (sin(x)-2sin(x)^3/3+sin(x)^5/5; giac gave one fraction); a rational function
  // (j<0, tan(x)^2*sec(x)): giac's antiderivative of it
  const gen cf=(r==1 || r==3?-c:c)/a;
  vecteur F;
  for (int i=0,b=1;i<=(j<0?0:j);b=b*(j-i)/(i+1),++i)
    F.push_back(_integrate(makesequence(j<0?cf*pow(x,e,contextptr)*pow(1+w*x*x,j,contextptr):cf*(w<0 && i%2?-b:b)*pow(x,e+2*i,contextptr),x),contextptr));
  const gen G=F.size()==1?F.front():symbolic(at_plus,gen(F,_SEQ__VECT));
  return contains(G,at_integrate) || is_undef(G)?gen(0):quotesubst(G,x,symbolic(r==0?at_tan:r==1?at_cot:r==2?at_sin:at_cos,v),contextptr);
}

// the table, then trig_powers: integrate(h,x) at once, 0 if neither (tan(u): -ln|cos(u)|/a)
static giac::gen table_integral(const giac::gen & g){
  using namespace giac;
  const gen r=table0(g);
  if (!is_zero(r) || !g.is_symb_of_sommet(at_integrate) || g._SYMBptr->feuille.type!=_VECT || g._SYMBptr->feuille._VECTptr->size()!=2
      || g._SYMBptr->feuille._VECTptr->back().type!=_IDNT)
    return r;
  return trig_powers(g._SYMBptr->feuille._VECTptr->front(),g._SYMBptr->feuille._VECTptr->back());
}

// integrate(f,x,a,b) over constant bounds with an antiderivative F of the above (sec(x)^3 from 0 to
// pi/4, sin(x)^5*cos(x)^2 from 0 to pi/2): F(b)-F(a), kept when a quadrature of f agrees, so F is
// continuous on [a,b] (not across a pole of tan(x)); 0 otherwise.
// giac's definite integration solved inequations and searched singularities: 36 s on the
// calculator for sec(x)^3 from 0 to pi/4
static __attribute__((noinline)) giac::gen table_definite(const giac::gen & g){
  using namespace giac;
  if (!g.is_symb_of_sommet(at_integrate) || g._SYMBptr->feuille.type!=_VECT || g._SYMBptr->feuille._VECTptr->size()!=4)
    return 0;
  const vecteur & v=*g._SYMBptr->feuille._VECTptr;
  const gen & f=v[0],& x=v[1],A=evalf(v[2],1,contextptr),B=evalf(v[3],1,contextptr);
  // (not the numeric integrals of numeric_hard_integrals, decimal bounds: evaluating their
  // integrands for the table took 30 s on the calculator; nor radicals, not in the table)
  if (x.type!=_IDNT || A.type!=_DOUBLE_ || B.type!=_DOUBLE_ || v[2].type==_DOUBLE_ || v[3].type==_DOUBLE_ || xcas::has_radical(f))
    return 0;
  const gen F=table_integral(symbolic(at_integrate,makesequence(f,x)));
  if (is_zero(F))
    return 0;
  const gen r=eval(subst(F,x,v[3],false,contextptr)-subst(F,x,v[2],false,contextptr),1,contextptr),re=evalf(r,1,contextptr);
  if (re.type!=_DOUBLE_)
    return 0;
  // Gauss-Legendre, 4 points on [a,a+.3(b-a)], [..,a+.65(b-a)], [..,b] (not symmetric: an odd pole
  // in the middle does not cancel)
  static const double gx[]={0.0694318442,0.3300094782,0.6699905218,0.9305681558},gw[]={0.1739274226,0.3260725774,0.3260725774,0.1739274226},
    cut[]={0,0.3,0.65,1};
  const double a=A._DOUBLE_val,w=B._DOUBLE_val-a;
  double s=0,m=0;
  for (int k=0;k<12;++k){
    const double h=(cut[k/4+1]-cut[k/4])*w;
    const gen y=evalf(subst(f,x,gen(a+cut[k/4]*w+gx[k%4]*h),false,contextptr),1,contextptr);
    if (y.type!=_DOUBLE_ || !(y._DOUBLE_val==y._DOUBLE_val))
      return 0;
    s+=gw[k%4]*h*y._DOUBLE_val;
    m+=std::abs(gw[k%4]*h*y._DOUBLE_val);
  }
  return std::abs(s-re._DOUBLE_val)<=1e-3*m?safe_normal(r):gen(0); // (sec(pi/4) is 2/sqrt(2))
}

// g with fn applied to each of its nodes from the leaves up: fn gets a node, its argument already
// mapped and x (fn's parameter), and returns the new node; lists are mapped element by element,
// programs stay
typedef giac::gen (*node_fn)(const giac::gen & g,const giac::gen & f,const giac::gen & x);
static giac::gen map_nodes(const giac::gen & g,node_fn fn,const giac::gen & x){
  using namespace giac;
  if (g.type==_VECT){
    vecteur w(*g._VECTptr);
    for (iterateur it=w.begin();it!=w.end();++it)
      *it=map_nodes(*it,fn,x);
    return gen(w,g.subtype);
  }
  if (g.type!=_SYMB || g._SYMBptr->sommet==at_program)
    return g;
  return fn(g,map_nodes(g._SYMBptr->feuille,fn,x),x);
}

// Odd roots of negative numbers are real, as on TI calculators and in textbooks (giac takes the
// complex root: (-8)^(1/3) was 1+i*sqrt(3)): (-8)^(1/3) is -(8^(1/3)), (-1)^(2/3) is 1^(2/3).
// plot: every x^(p/q) with q odd becomes surd(x,q)^p, real for x<0 (the graph of x^(1/3) had
// only its right half)
static giac::gen real_root_node(const giac::gen & e,const giac::gen & a,const giac::gen & plotg){
  using namespace giac;
  const bool plot=!is_zero(plotg);
  if (e._SYMBptr->sommet==at_pow && a.type==_VECT && a._VECTptr->size()==2){
    const gen & b=a._VECTptr->front();
    gen p=a._VECTptr->back();
    if (p.type!=_FRAC && lidnt(p).empty()) // 1/3 as typed
      p=eval(p,1,contextptr);
    if (p.type==_FRAC && p._FRACptr->num.type==_INT_ && p._FRACptr->den.type==_INT_ && p._FRACptr->den.val%2){
      if (plot){
        const gen r=symbolic(at_surd,makesequence(b,p._FRACptr->den));
        return p._FRACptr->num.val==1?r:symbolic(at_pow,makesequence(r,p._FRACptr->num));
      }
      const gen be=evalf(b,1,contextptr);
      if (be.type==_DOUBLE_ && be._DOUBLE_val<0){
        const gen r=normal(pow(-b,p,contextptr),contextptr); // (-8)^(1/3): 2, not 8^(1/3)
        return p._FRACptr->num.val%2?-r:r;
      }
    }
  }
  return symbolic(e._SYMBptr->sommet,a);
}
static giac::gen real_roots(const giac::gen & e,bool plot=false){
  return map_nodes(e,real_root_node,plot?1:0);
}
// a definite integral of a real integrand that giac made complex (x^(-1/3) from -1 to 8): the
// antiderivative at the bounds with real roots (giac's value was finite: the integral converges)
static giac::gen real_ftc(const giac::gen & in,const giac::gen & g){
  using namespace giac;
  const vecteur & v=*in._SYMBptr->feuille._VECTptr; // integrate(f,x,a,b)
  const gen F=_integrate(makesequence(eval(v[0],1,contextptr),v[1]),contextptr);
  if (contains(F,at_integrate) || has_i(F))
    return g;
  const gen a=eval(v[2],1,contextptr),b=eval(v[3],1,contextptr);
  const gen r=normal(eval(real_roots(subst(F,v[1],b,true,contextptr))-real_roots(subst(F,v[1],a,true,contextptr)),1,contextptr),contextptr);
  return has_i(r) || has_inf_or_undef(r)?g:r;
}

// a two-sided limit that does not exist, said as textbooks say it: giac's unsigned infinity (1/x
// at 0) or its error "Unidirectional limits are distinct -1,1" (|x|/x at 0, left value first)
static void dne_text(char * b,const char * l,int ln,const char * r,int rn){
  if (ln>40) ln=40;
  if (rn>40) rn=40;
  strcpy(b,"does not exist: ");
  strncat(b,l,ln);
  strcat(b," from the left, ");
  strncat(b,r,rn);
  strcat(b," from the right");
}
static bool limit_dne(const giac::gen & in,const giac::gen & g,std::string & msg){
  using namespace giac;
  if (!in.is_symb_of_sommet(at_limit) || in._SYMBptr->feuille.type!=_VECT || in._SYMBptr->feuille._VECTptr->size()!=3)
    return false;
  char b[128];
  if (g==unsigned_inf){
    const vecteur & v=*in._SYMBptr->feuille._VECTptr;
    const gen f=eval(v[0],1,contextptr),a=eval(v[2],1,contextptr);
    const gen L=_limit(makesequence(f,v[1],a,-1),contextptr),R=_limit(makesequence(f,v[1],a,1),contextptr);
    if (is_undef(L) || is_undef(R) || L.type==_STRNG || R.type==_STRNG)
      return false;
    const std::string ls=L.print(contextptr),rs=R.print(contextptr);
    dne_text(b,ls.c_str(),ls.size(),rs.c_str(),rs.size());
  }
  else if (g.type==_STRNG){
    const char * s=strstr(g._STRNGptr->c_str(),"limits are distinct");
    if (!s)
      return false;
    s+=19;
    if (*s=='s')
      ++s;
    while (*s==' ')
      ++s;
    const char * c=strchr(s,','),* e=strstr(s," Error");
    if (!c)
      return false;
    if (!e || e<c)
      e=s+strlen(s);
    dne_text(b,s,c-s,c+1,e-c-1);
  }
  else
    return false;
  msg=b;
  return true;
}

// the signs of f(n) alternate (at n=10, 11, 12): (-1)^n*R
static bool alternating(const giac::gen & f,const giac::gen & n){
  using namespace giac;
  double y[3];
  for (int k=0;k<3;++k){
    const gen e=evalf(subst(f,n,10+k,false,contextptr),1,contextptr);
    if (e.type!=_DOUBLE_)
      return false;
    y[k]=e._DOUBLE_val;
  }
  return y[0]*y[1]<0 && y[1]*y[2]<0;
}

// Infinite sums this giac leaves as they are (no Zeta, no Psi): sum(1/n^2,n,1,inf) is pi^2/6.
// A divergent one gives "diverges" in msg (n-th term test) or +-infinity (rational terms like
// 1/n); the famous values are given exactly (1/n^(2k), (-1)^n/n, (-1)^n/(2n+1), 1/n!); other
// rational terms get a decimal value (partial sum + the tail's integral). Else g unchanged.
// f=q*r^n: the factors b^(k*n+m) of a product give r (b^k) and q (b^m, the other factors); t is
// in a numerator (sign 1) or a denominator (-1)
static void split_geo(const giac::gen & t,const giac::gen & n,giac::gen & r,giac::gen & q,int sign){
  using namespace giac;
  if (t.is_symb_of_sommet(at_prod) && t._SYMBptr->feuille.type==_VECT){
    const vecteur & w=*t._SYMBptr->feuille._VECTptr;
    for (unsigned i=0;i<w.size();++i)
      split_geo(w[i],n,r,q,sign);
    return;
  }
  if (t.is_symb_of_sommet(at_inv)){
    split_geo(t._SYMBptr->feuille,n,r,q,-sign);
    return;
  }
  if (t.is_symb_of_sommet(at_neg)){
    q=-q;
    split_geo(t._SYMBptr->feuille,n,r,q,sign);
    return;
  }
  if (t.is_symb_of_sommet(at_pow) && t._SYMBptr->feuille.type==_VECT && t._SYMBptr->feuille._VECTptr->size()==2){
    const gen & b=t._SYMBptr->feuille._VECTptr->front(),& e=t._SYMBptr->feuille._VECTptr->back();
    if (is_constant_wrt(b,n,contextptr) && !is_constant_wrt(e,n,contextptr)){
      const gen k=derive(e,n,contextptr);
      if (is_constant_wrt(k,n,contextptr)){
        r=r*pow(b,sign*k,contextptr);
        q=q*pow(b,sign*ratnormal(e-k*n,contextptr),contextptr);
        return;
      }
    }
    if (e==-1){ // 1/(n*3^n)
      split_geo(b,n,r,q,-sign);
      return;
    }
    if (b.is_symb_of_sommet(at_pow) && b._SYMBptr->feuille.type==_VECT && b._SYMBptr->feuille._VECTptr->size()==2 && is_constant_wrt(e,n,contextptr)){
      const vecteur & u=*b._SYMBptr->feuille._VECTptr; // (x^n)^2 (giac's x^(2n))
      split_geo(symbolic(at_pow,makesequence(u[0],u[1]*e)),n,r,q,sign);
      return;
    }
  }
  q=sign>0?q*t:q/t;
}
static giac::gen less_terms(giac::gen s,const giac::gen & f,const giac::gen & n,int from,int to); // (below)
// sums of c*r^n/n, c*n*r^n, c*r^n, c*r^n/n! from n=a, r constant in n (x allowed: power series):
// -c*ln(1-r), c*r/(1-r)^2, c*r^a/(1-r), c*e^r, less the terms before a. 0: f is none of these, or the
// sum diverges (|r|>1)
static giac::gen power_series(const giac::gen & f,const giac::gen & n,int a){
  using namespace giac;
  gen r=1,q=1,c,s;
  split_geo(f,n,r,q,1);
  r=normal(r,contextptr);
  q=ratnormal(q,contextptr);
  if (is_one(r) || !is_constant_wrt(r,n,contextptr))
    return 0;
  const gen re=evalf(r,1,contextptr);
  const double ar=re.type==_DOUBLE_?(re._DOUBLE_val<0?-re._DOUBLE_val:re._DOUBLE_val):0;
  int from=0;
  if (is_constant_wrt(c=ratnormal(q*n,contextptr),n,contextptr) && a>=1){ // c/n: -c*ln(1-r) (r=-1: -c*ln 2)
    if (ar>1 || (ar==1 && re._DOUBLE_val>0))
      return 0;
    s=-c*ln(ratnormal(1-r,contextptr),contextptr);
    from=1;
  }
  else if (is_constant_wrt(c=ratnormal(q/n,contextptr),n,contextptr)){
    if (ar>=1)
      return 0;
    s=c*r/pow(1-r,2,contextptr);
    from=1; // (its n=0 term is 0)
  }
  else if (is_constant_wrt(c=ratnormal(q*symbolic(at_factorial,n),contextptr),n,contextptr))
    s=c*exp(r,contextptr);
  else if (is_constant_wrt(c=q,n,contextptr)){
    if (ar>=1)
      return 0;
    s=c*pow(r,a,contextptr)/(1-r); // from a: x/(1-x) from 1
    from=a;
  }
  else
    return 0;
  if (from>=a) // 1/(1-x), x/(1-x)^2 as textbooks write them (normal: -1/(x-1))
    return s;
  return normal(less_terms(s,f,n,from,a),contextptr);
}

// The textbook Maclaurin series: sum((-1)^n*x^(2n+1)/(2n+1)!,n,0,inf) is sin(x) (giac left it).
// The sum (25 terms) at x=0.2, 0.35, 0.5 against c*F(x)+d for F = sin, cos, sinh, cosh, e^x,
// atan, ln(1+x), with c and d fractions of denominators up to 24 (two points give them, the
// third checks); 0 if none fits
static bool small_fraction(double v,giac::gen & q){
  for (int den=1;den<=24;++den){
    const double t=v*den,r=t<0?-(long)(-t+0.5):(long)(t+0.5);
    if (t-r<1e-5 && r-t<1e-5){
      q=giac::gen((long)r)/den;
      return true;
    }
  }
  return false;
}
static giac::gen series_identify(const giac::gen & f,const giac::gen & n,int a,const giac::gen & x){
  using namespace giac;
  static const unary_function_ptr * const fn[]={at_sin,at_cos,at_sinh,at_cosh,at_exp,at_atan,at_ln};
  static const double X[3]={0.2,0.35,0.5}; // (25 terms: the error stays below 1e-9 there)
  double S[3],v[3];

  for (int k=0;k<3;++k){
    const gen fx=subst(f,x,gen(X[k]),false,contextptr);
    S[k]=0;
    for (int m=a;m<a+25;++m){
      const gen y=evalf(subst(fx,n,m,false,contextptr),1,contextptr);
      const double t=y.type==_DOUBLE_?y._DOUBLE_val:0;
      if (y.type!=_DOUBLE_ || t!=t || t>1e30 || t<-1e30) // (NaN: past 3.4e38, the calculator's
        return 0;                                          // floats; giac's (2n+1)! gave NaN)
      S[k]+=t;
      if (m>a+3 && t!=0 && (t<0?-t:t)<1e-9*(S[k]<0?-S[k]:S[k])) // the rest is negligible, before n!
        break;                                            // grows past the floats
    }
  }
  for (int j=0;j<7;++j){
    const gen F=symbolic(fn[j],j==6?x+1:x); // (ln(1+x))
    for (int k=0;k<3;++k){
      const gen y=evalf(subst(F,x,gen(X[k]),false,contextptr),1,contextptr);
      if (y.type!=_DOUBLE_)
        return 0;
      v[k]=y._DOUBLE_val;
    }
    const double c=(S[1]-S[0])/(v[1]-v[0]),d=S[0]-c*v[0],e=c*v[2]+d-S[2];
    gen cq,dq;
    if (e<1e-5 && e>-1e-5 && small_fraction(c,cq) && small_fraction(d,dq) && !is_zero(cq)) // (the
      // calculator's doubles have 7 digits)
      return is_zero(dq)?(is_one(cq)?F:cq*F):cq*F+dq;
  }
  return 0;
}
// n in a power's base and exponent: ((n-1)/n)^n, n^n/n!
static bool n_power(const giac::gen & f,const giac::gen & n){
  using namespace giac;
  const vecteur pw=lop(f,at_pow);
  for (const_iterateur it=pw.begin();it!=pw.end();++it)
    if (it->_SYMBptr->feuille.type==_VECT && it->_SYMBptr->feuille._VECTptr->size()==2
        && !is_constant_wrt(it->_SYMBptr->feuille._VECTptr->front(),n,contextptr) && !is_constant_wrt(it->_SYMBptr->feuille._VECTptr->back(),n,contextptr))
      return true;
  return false;
}
// the term f at n=m in decimals: in floats first ((1-1/n)^(n^2) at 42 is exactly a fraction of
// 2900 digits, past giac's limit; minutes on the calculator), exactly if they overflow or
// vanish ((n!)^2/(2n)!: 40! is past the calculator's floats)
static bool term(const giac::gen & f,const giac::gen & n,int m,double & y){
  using namespace giac;
  for (int k=0;k<2;++k){
    const gen e=evalf(subst(f,n,k?gen(m):gen(double(m)),false,contextptr),1,contextptr);
    if (e.type==_DOUBLE_ && e._DOUBLE_val==e._DOUBLE_val && (k || (e._DOUBLE_val!=0 && e._DOUBLE_val<1e30 && e._DOUBLE_val>-1e30))){
      y=e._DOUBLE_val;
      return true;
    }
  }
  return false;
}
// f(n) for n=a..a+200 added from the smallest term (floats lose less); alt: an alternating
// series' value, the partial sums to N-2, N-1, N averaged (1/4, 1/2, 1/4: S-(3t_N+t_N-1)/4, an
// error of 1e-7 for sum((-1)^n/sqrt(n)), 4e-5 for the mean of two)
static bool sum200(const giac::gen & f,const giac::gen & n,int a,bool alt,double & s){
  double t1=0,t2=0,y;
  s=0;
  for (int m=a+200;m>=a;--m){
    if (!term(f,n,m,y))
      return false;
    if (m==a+200)
      t1=y;
    if (m==a+199)
      t2=y;
    s+=y;
  }
  if (alt)
    s-=(3*t1+t2)/4;
  return true;
}
// s less the terms f(k) for k=from..to-1 (a sum from n=to: the closed form's terms before it)
static __attribute__((noinline)) giac::gen less_terms(giac::gen s,const giac::gen & f,const giac::gen & n,int from,int to){
  for (int k=from;k<to;++k)
    s=s-giac::subst(f,n,k,false,contextptr);
  return s;
}
static giac::gen known_sum(const giac::gen & g,std::string & msg){
  using namespace giac;
  if (!g.is_symb_of_sommet(at_sum) || g._SYMBptr->feuille.type!=_VECT || g._SYMBptr->feuille._VECTptr->size()!=4)
    return g;
  const vecteur & v=*g._SYMBptr->feuille._VECTptr;
  const gen & n=v[1],a=v[2];
  if (n.type!=_IDNT || !(v[3]==plus_inf) || a.type!=_INT_)
    return g;
  // cos(n*pi) is (-1)^n (giac leaves it: sum(cos(n*pi)/n) is -ln 2)
  const gen f=subst(v[0],eval(symbolic(at_cos,n*cst_pi),1,contextptr),pow(gen(-1),n,contextptr),false,contextptr);
  const vecteur ids=lidnt(f);
  if (ids.size()!=1 || !(ids.front()==n)){ // sum(x^n/n,n,1,inf): a power series in x
    gen ps=power_series(f,n,a.val);
    if (is_zero(ps) && ids.size()==2) // the Maclaurin series of sin, cos...
      ps=series_identify(f,n,a.val,ids.front()==n?ids.back():ids.front());
    return is_zero(ps)?g:ps;
  }
  const gen R=subst(f,symbolic(at_pow,makesequence(gen(-1),n)),1,false,contextptr); // f=(-1)^n*R
  const bool isalt=alternating(f,n),hard=n_power(f,n);
  // decimal tests first: giac's limit took minutes on the calculator (1/(2n)!, ((n-1)/n)^n).
  // The ratio of terms at n=a+5 and a+10: growing terms diverge; shrinking like r^n (r<0.95,
  // not rising toward 1 like 1/n^2's) they converge, no limit needed
  double y1,y2,rt[2];
  int k=0;
  for (;k<2 && term(f,n,a.val+5+5*k,y1) && term(f,n,a.val+6+5*k,y2) && y1!=0;++k)
    rt[k]=y2/y1<0?-y2/y1:y2/y1;
  const bool geo=k==2 && rt[0]<0.95 && rt[1]<0.95 && rt[1]<rt[0]+0.05;
  bool away=k==2 && rt[0]>1.02 && rt[1]>1.02;
  if (!geo && !away){
    // the n-th term test, on R for an alternating series: terms that stay away from 0 at
    // n=10^3, 10^4, 10^5 diverge (floats: exact powers of 10^5 are too big), else giac's limit
    double z[3];
    for (k=0;k<3;++k){
      const gen e=evalf(subst(isalt?R:f,n,gen(k?k==1?1e4:1e5:1e3),false,contextptr),1,contextptr);
      if (e.type!=_DOUBLE_)
        break;
      z[k]=e._DOUBLE_val<0?-e._DOUBLE_val:e._DOUBLE_val;
    }
    away=k==3 && z[0]>1e-3 && z[1]-z[0]<0.2*z[0] && z[0]-z[1]<0.2*z[0] && z[2]-z[1]<0.2*z[1] && z[1]-z[2]<0.2*z[1];
    if (!away && !(k==3 && z[2]<z[1] && z[1]<z[0])){ // (shrinking: they go to 0; giac's limit of
      // (n/(n+ln(n)))^n never ended on the calculator)
      const gen L=_limit(makesequence(isalt?R:f,n,plus_inf),contextptr),Le=evalf(L,1,contextptr);
      away=L.is_symb_of_sommet(at_bounded_function) || L==plus_inf || L==minus_inf || L==unsigned_inf || (Le.type==_DOUBLE_ && Le._DOUBLE_val!=0);
    }
  }
  if (away){
    msg="diverges (the terms do not go to 0)";
    return g;
  }
  const vecteur lv=lvar(R);
  const bool rational=lv.size()==1 && lv.front()==n;
  const gen P=_numer(R,contextptr),Q=_denom(R,contextptr);
  const gen dp=_degree(makesequence(P,n),contextptr),dq=_degree(makesequence(Q,n),contextptr);
  if (rational && dp.type==_INT_ && dq.type==_INT_){
    const int p=dq.val-dp.val;
    const gen lq=_lcoeff(makesequence(Q,n),contextptr),c=_lcoeff(makesequence(P,n),contextptr)/lq;
    if (!isalt && p<=1) // like the harmonic series
      return is_positive(c,contextptr)?plus_inf:minus_inf;
    if (!isalt && dp.val==0 && p%2==0 && p<=8 && a.val>=1 && is_zero(ratnormal(Q-lq*pow(n,p),contextptr))){
      static const int zd[]={6,90,945,9450}; // zeta(2k)=pi^(2k)*b/zd: b=1,1,1,1
      return ratnormal(less_terms(c*pow(cst_pi,p)/zd[p/2-1],R,n,1,a.val),contextptr);
    }
    if (isalt && p==1 && a.val>=0){ // c/(n+b): -ln(2) for 1/n, pi/4 for 1/(2n+1)
      const gen b=ratnormal(Q/lq-n,contextptr);
      if (dp.val==0 && dq.val==1 && is_zero(b) && a.val>=1){
        return less_terms(-c*ln(2,contextptr),f,n,1,a.val);
      }
      if (dp.val==0 && dq.val==1 && b==fraction(1,2) && a.val==0) // c/(n+1/2): c*pi/2
        return ratnormal(c*cst_pi/2,contextptr);
    }
    if (p>=2 || isalt){ // a decimal value: 200 terms, then the tail (integral of c/n^p)
      double s;
      const int N=a.val+200;
      if (!sum200(f,n,a.val,isalt,s))
        return g;
      if (!isalt){
        const gen ce=evalf(c,1,contextptr);
        if (ce.type!=_DOUBLE_)
          return g;
        double m=1;
        for (int k=1;k<p;++k)
          m*=N+0.5;
        s+=ce._DOUBLE_val/((p-1)*m);
      }
      return gen(s);
    }
    return g;
  }
  const gen ps=power_series(f,n,a.val); // 3^n/n!: e^3, 1/(2^n*n): ln 2, n/2^n: 2
  if (!is_zero(ps))
    return ps;
  // c*r^n/n!: c*e^r (from 0), 1/n! is e
  const gen cf=ratnormal(f*symbolic(at_factorial,n),contextptr);
  const gen r=ratnormal(subst(cf,n,n+1,false,contextptr)/cf,contextptr);
  if (is_constant_wrt(r,n,contextptr) && a.val>=0){
    return ratnormal(less_terms(subst(cf,n,0,false,contextptr)*exp(r,contextptr),f,n,0,a.val),contextptr);
  }
  // terms shrinking like r^n, added up to a negligible one: (n!)^2/(2n)! 0.736399, (-1)^n/(2n)!
  // 0.540302 (cos(1))
  if (geo){
    double s=0;
    for (int m=a.val;m<a.val+400 && term(f,n,m,y1);++m){
      s+=y1;
      if (m>a.val+3 && y1!=0 && (y1<0?-y1:y1)<1e-9*(s<0?-s:s))
        return gen(s);
    }
  }
  // the integral test (1/(n*ln(n)) diverges); convergent: 200 terms, then the tail's integral
  if (!isalt && a.val>=1 && !contains(f,at_factorial) && !hard){
    const gen I=_integrate(makesequence(f,n,a,plus_inf),contextptr);
    if (I==plus_inf || I==minus_inf)
      return I;
    if (I.type==_DOUBLE_ || contains(I,at_integrate) || evalf(I,1,contextptr).type!=_DOUBLE_)
      return g; // (only an exact integral tells: a numeric one may hide a divergence)
    double s;
    const int N=a.val+200;
    if (!sum200(f,n,a.val,false,s))
      return g;
    const gen T=evalf(_integrate(makesequence(f,n,gen(N)+fraction(1,2),plus_inf),contextptr),1,contextptr);
    if (T.type==_DOUBLE_)
      return gen(s+T._DOUBLE_val);
  }
  double s; // alternating, terms going to 0 (the n-th term test): sum((-1)^n(1-n^(1/n))) -0.18786
  if (isalt && sum200(f,n,a.val,true,s))
    return gen(s);
  return g;
}

// sum(c*n^q,n,a,inf) for a non-integer q (1/sqrt(n), (-1)^n/n^(3/2)) before giac sees it:
// giac gave 2.47131 for sum(1/n^(3/2),n,1,inf) (2.61238). +-infinity for p=-q<=1, else a
// decimal (200 terms, then the tail's integral; alternating: the mean of two partial sums).
// Returns 0 if f is not such a power.
static giac::gen power_sum(const giac::gen & g){
  using namespace giac;
  if (!g.is_symb_of_sommet(at_sum) || g._SYMBptr->feuille.type!=_VECT || g._SYMBptr->feuille._VECTptr->size()!=4)
    return 0;
  const vecteur & v=*g._SYMBptr->feuille._VECTptr;
  const gen & n=v[1];
  const gen a=eval(v[2],1,contextptr);
  if (n.type!=_IDNT || !(eval(v[3],1,contextptr)==plus_inf) || a.type!=_INT_)
    return 0;
  const gen f=eval(v[0],1,contextptr);
  // n in a power's base and exponent (((n-1)/n)^n, n^n/n!): giac's sum simplifies them at
  // length (minutes on the calculator) for nothing; known_sum (answer_after) gets them as is
  if (n_power(f,n))
    return symbolic(at_sum,makesequence(f,n,a,plus_inf));
  const vecteur ids=lidnt(f);
  if (ids.size()!=1 || !(ids.front()==n)) // a power series in x with n!: known_sum's (giac's
    return ids.size()==2 && contains(f,at_factorial)?symbolic(at_sum,makesequence(f,n,a,plus_inf)):gen(0); // sum hung)
  const gen R=subst(f,symbolic(at_pow,makesequence(gen(-1),n)),1,false,contextptr); // f=(-1)^n*R
  const bool isalt=alternating(f,n);
  const vecteur lv=lvar(R);
  if (lv.size()==1 && lv.front()==n) // rational: giac, then known_sum
    return 0;
  // c*n^q (1/sqrt(n): q=-1/2): n*R'/R is q (compared at n=10 and 37: ratnormal leaves
  // (-n-2*n)/(2*n) for 1/(sqrt(n)*n)). A p-series: diverges for p=-q<=1, else a decimal
  const gen q=n*derive(R,n,contextptr)/R;
  const gen q1=evalf(subst(q,n,10,false,contextptr),1,contextptr),q2=evalf(subst(q,n,37,false,contextptr),1,contextptr);
  if (q1.type==_DOUBLE_ && q2.type==_DOUBLE_ && (q1._DOUBLE_val-q2._DOUBLE_val)*(q1._DOUBLE_val-q2._DOUBLE_val)<1e-10 && a.val>=1){
    const double p=-q1._DOUBLE_val;
    const gen c=evalf(subst(R,n,10,false,contextptr)*pow(gen(10),gen(p),contextptr),1,contextptr);
    if (c.type!=_DOUBLE_)
      return 0;
    if (!isalt && p<=1)
      return c._DOUBLE_val>0?plus_inf:minus_inf;
    double s;
    const int N=a.val+200;
    if (!sum200(f,n,a.val,isalt,s))
      return 0;
    return gen(isalt?s:s+c._DOUBLE_val*::pow(N+0.5,1-p)/(p-1));
  }
  // factorials, ln(n), trig of n: known_sum without giac's sum, which simplifies the term at
  // length (a minute on the calculator) and leaves these as they are (n^2/2^n: giac's 6)
  return contains(f,at_factorial) || contains(f,at_ln) || contains(f,at_sin) || contains(f,at_cos) || contains(f,at_tan)?
    symbolic(at_sum,makesequence(f,n,a,plus_inf)):gen(0);
}

// the integrand F is c*b'(x)*b(x)^e for one of its radicals b^e: a u-substitution, elementary
// (surface areas: 2*pi*x^3*sqrt(1+9x^4) went numeric)
static bool usub(const giac::gen & F,const giac::gen & x){
  using namespace giac;
  const vecteur v=lop(F,at_pow);
  for (const_iterateur it=v.begin();it!=v.end();++it){
    const gen & a=it->_SYMBptr->feuille;
    if (a.type!=_VECT || a._VECTptr->size()!=2 || a._VECTptr->back().type!=_FRAC || is_constant_wrt(a._VECTptr->front(),x,contextptr))
      continue;
    const gen q=ratnormal(F/(*it*derive(a._VECTptr->front(),x,contextptr)),contextptr);
    if (!is_undef(q) && !is_inf(q) && is_constant_wrt(q,x,contextptr))
      return true;
  }
  return false;
}

// d/dx of an integral with bounds in x, by the fundamental theorem: f(b(x))*b'(x)-f(a(x))*a'(x).
// giac integrated first: d/dx int(sin(t^2),t,0,x) came out as (i*e^(-i*x^2)-i*e^(i*x^2))/2
static giac::gen ftc(const giac::gen & g){
  using namespace giac;
  if (g.type!=_SYMB)
    return g;
  const gen & a=g._SYMBptr->feuille;
  if (g._SYMBptr->sommet==at_diff && a.type==_VECT && a._VECTptr->size()==2){
    const gen & F=a._VECTptr->front(),x=a._VECTptr->back();
    if (x.type==_IDNT && (F.is_symb_of_sommet(at_integrate) || F.is_symb_of_sommet(at_int)) && F._SYMBptr->feuille.type==_VECT && F._SYMBptr->feuille._VECTptr->size()==4){
      const vecteur & v=*F._SYMBptr->feuille._VECTptr;
      if (v[1].type==_IDNT && !(v[1]==x) && is_constant_wrt(v[0],x,contextptr)){
        const gen lo=eval(v[2],1,contextptr),hi=eval(v[3],1,contextptr);
        gen r=0;
        if (!is_constant_wrt(hi,x,contextptr))
          r=subst(v[0],v[1],hi,false,contextptr)*derive(hi,x,contextptr);
        if (!is_constant_wrt(lo,x,contextptr))
          r=r-subst(v[0],v[1],lo,false,contextptr)*derive(lo,x,contextptr);
        return r;
      }
    }
  }
  if (a.type==_VECT){
    vecteur w(*a._VECTptr);
    for (iterateur it=w.begin();it!=w.end();++it)
      *it=ftc(*it);
    return symbolic(g._SYMBptr->sommet,gen(w,a.subtype));
  }
  return symbolic(g._SYMBptr->sommet,ftc(a));
}

static giac::gen numeric_hard_integrals(const giac::gen & g){
  using namespace giac;
  if (g.type!=_SYMB)
    return g;
  const gen & a=g._SYMBptr->feuille;
  if ((g._SYMBptr->sommet==at_integrate || g._SYMBptr->sommet==at_int) && a.type==_VECT && a._VECTptr->size()==4){
    const vecteur & v=*a._VECTptr;
    // constant bounds: numbers, pi, e (an infinite bound is left to giac)
    if (v[1].type==_IDNT && evalf(v[2],1,contextptr).type==_DOUBLE_ && evalf(v[3],1,contextptr).type==_DOUBLE_ && hard_radicand(v[0],v[1])
        && !usub(eval(v[0],1,contextptr),v[1])){
      // numerically, after x = a+(b-a)*sin(pi*t/2)^2: its dx vanishes at both ends and cancels an
      // endpoint singularity (ellipse perimeters: 13.3616 for 13.3649 before)
      const gen a=evalf(v[2],1,contextptr),b=evalf(v[3],1,contextptr),t=gen("t__n",contextptr);
      const gen x=a+(b-a)*pow(symbolic(at_sin,cst_pi*t/2),2),dx=(b-a)*cst_pi/2*symbolic(at_sin,cst_pi*t);
      return symbolic(g._SYMBptr->sommet,makesequence(subst(v[0],v[1],x,false,contextptr)*dx,t,gen(0.0),gen(1.0)));
    }
    return g;
  }
  if (a.type==_VECT){
    vecteur w(*a._VECTptr);
    for (iterateur it=w.begin();it!=w.end();++it)
      *it=numeric_hard_integrals(*it);
    return symbolic(g._SYMBptr->sommet,gen(w,a.subtype));
  }
  return symbolic(g._SYMBptr->sommet,numeric_hard_integrals(a));
}

// An equation typed alone (x^2-4=0, 2x+1=5, sin(x)=1/2) is solved for its only unknown, as an
// online calculator does; x=5 or f(x)=... never get here (equaltosto stores, the input pre-pass
// turns f(x)= into a definition). Returns the unknown, or 0.
static giac::gen equation_unknown(const giac::gen & g){
  using namespace giac;
  if (!g.is_symb_of_sommet(at_equal))
    return 0;
  vecteur v=lidnt(g),u;
  for (unsigned k=0;k<v.size();++k){
    if (v[k]==cst_pi || is_inf(v[k]) || is_undef(v[k]))
      continue;
    if (eval(v[k],1,contextptr)==v[k]) // not assigned
      u.push_back(v[k]);
  }
  return u.size()==1?u.front():gen(0);
}
// Focus: the decimal value of an exact numeric result, shown under it (console_approx), for the
// result printed as console_approx_for
// 862519 rather than 8.62519e5 (6 significant digits, as giac prints, without the exponent)
static void plain_decimal(const giac::gen & g,std::string & out){
  if (g.type!=giac::_DOUBLE_)
    return;
  const double d=g._DOUBLE_val;
  if ((d>=1e5 && d<1e6) || (d<=-1e5 && d>-1e6)){
    char b[16];
    sprintf(b,"%ld",(long)(d+(d>0?0.5:-0.5))); // (%f: no floats in the calculator's printf)
    out=b;
  }
}
static std::string * approx_text;
const char * console_approx_for="";
const char * console_approx(){ return approx_text?approx_text->c_str():""; }
// console_approx_for points into a copy of the printed result (it pointed into a temporary)
static std::string * approx_for;
void set_approx_for(const std::string & s){
  if (!approx_for)
    approx_for=new std::string;
  *approx_for=s;
  console_approx_for=approx_for->c_str();
}
void result_approx(const giac::gen & g){
  using namespace giac;
  if (!approx_text)
    approx_text=new std::string;
  approx_text->clear();
  console_approx_for="";
  if (g.type==_INT_ || g.type==_ZINT || g.type==_DOUBLE_ || g.type==_FLOAT_ || g.type==_STRNG || g.type==_VECT
      || taille(g,64)>=64 // (pi/2 has an identifier, pi: evalf below tells numbers from expressions)
      || contains(g,at_sum)) // (evalf would try it numerically: 87 s on a sum known_sum gave up)
    return;
  if (g.type==_CPLX && (g._CPLXptr->type==_INT_ || g._CPLXptr->type==_ZINT) && ((g._CPLXptr+1)->type==_INT_ || (g._CPLXptr+1)->type==_ZINT))
    return; // 5+5i: nothing to add
  const gen a=evalf(g,1,contextptr);
  if (a.type!=_DOUBLE_ && a.type!=_CPLX)
    return;
  const int dd=decimal_digits(contextptr);
  decimal_digits(6,contextptr);
  *approx_text=a.print(contextptr);
  decimal_digits(dd,contextptr);
  plain_decimal(a,*approx_text);
  const size_t k=approx_text->find("*i"); // 1.5+0.5*i: 1.5+0.5i
  if (k!=std::string::npos && k+2==approx_text->size())
    approx_text->erase(k,1);
  const char * t=approx_text->c_str(); // 1.0+1.73205i: 1+1.73205i
  const char * z=strstr(t,".0");
  if (z && z>t && isdigit((unsigned char)z[-1]) && (z[2]=='+' || z[2]=='-'))
    approx_text->erase(z-t,2);
}

// Results as textbooks write them: pi/2, x^3/3, sqrt(2)/2, x-x^3/6+O(x^6), e (giac prints
// 1/2*pi, 1/3*x^3, x-1/6*x^3+x^6*order_size(x), exp(1)). Each top-level term (split at + - , =):
// p/q*m -> p*m/q, m*order_size(v) -> O(m); exp(1) -> e. Inside brackets nothing changes.
// Plain chars (uSTL string ops are large and substr(pos) is broken).
// log( typed as a word becomes log10( (outside strings), when buf (capacity cap) has the room
static void ti_log10(char * buf,int cap){
  int n=strlen(buf);
  bool str=false;
  for (int i=0;i+4<=n;++i){
    if (buf[i]=='"')
      str=!str;
    if (str || strncmp(buf+i,"log(",4) || (i && (isalnum((unsigned char)buf[i-1]) || buf[i-1]=='_')))
      continue;
    if (n+3>cap)
      return;
    memmove(buf+i+5,buf+i+3,n-i-2); // "log" + "10" + "(..."
    buf[i+3]='1';
    buf[i+4]='0';
    n+=2;
    i+=5;
  }
}

static bool word_char(char c){ return isalnum((unsigned char)c) || c=='_'; }

// Riemann sums use i as the index, sum((i/n)^2/n,i,1,n), but giac's i is sqrt(-1) ("please use a
// valid identifier name"). In a sum(, product( or seq( whose index is i, i becomes k (or j, m:
// a letter the call does not use). Same length: no room needed.
static void sum_index_i(char * buf){
  const int n=strlen(buf);
  for (int i=0;i<n;++i){
    const int L=!strncmp(buf+i,"sum(",4)?4:!strncmp(buf+i,"seq(",4)?4:!strncmp(buf+i,"product(",8)?8:0;
    if (!L || (i && word_char(buf[i-1])))
      continue;
    int depth=0,c1=-1,c2=-1,end=-1;
    for (int j=i+L;j<n && end<0;++j){
      const char c=buf[j];
      if (c=='(' || c=='[' || c=='{')
        ++depth;
      else if (c==')' || c==']' || c=='}'){
        if (!depth)
          end=j;
        --depth;
      }
      else if (c==',' && !depth){
        if (c1<0) c1=j; else if (c2<0) c2=j;
      }
    }
    if (c1<0 || end<0)
      continue;
    int a=c1+1;
    while (buf[a]==' ') ++a;
    int b=a+1;
    while (buf[b]==' ') ++b;
    if (buf[a]!='i' || (b!=(c2>=0?c2:end) && buf[b]!='=')) // the index: i, or i=1..n
      continue;
    char name=0;
    for (const char * c="kjm";*c && !name;++c){
      name=*c;
      for (int j=i+L;j<end;++j)
        if (buf[j]==*c && !word_char(buf[j-1]) && !word_char(buf[j+1]))
          name=0;
    }
    for (int j=i+L;name && j<end;++j)
      if (buf[j]=='i' && !word_char(buf[j-1]) && !word_char(buf[j+1]))
        buf[j]=name;
  }
}

// desolve's constants c_0, c_1 as textbooks write them: C1, C2 (plain C: uSTL's substr bites)
__attribute__((noinline)) void textbook_constants(std::string & str){
  const char * s=str.c_str();
  if (!strstr(s,"c_"))
    return;
  const int n=strlen(s);
  char * out=(char *)malloc(n+1);
  if (!out)
    return;
  int k=0;
  for (int i=0;i<n;){
    const bool word=i==0 || !(isalnum((unsigned char)s[i-1]) || s[i-1]=='_');
    if (word && s[i]=='c' && s[i+1]=='_' && s[i+2]>='0' && s[i+2]<='9'){
      int j=i+2,v=0;
      while (j<n && s[j]>='0' && s[j]<='9') v=10*v+(s[j++]-'0');
      if (!(j<n && (isalnum((unsigned char)s[j]) || s[j]=='_'))){
        k+=sprintf(out+k,"C%d",v+1);
        i=j;
        continue;
      }
    }
    out[k++]=s[i++];
  }
  out[k]=0;
  str=out;
  free(out);
}

__attribute__((noinline)) void textbook(std::string & str){
  const char * s=str.c_str();
  const int n=strlen(s);
  if (n>=1024 || (!strchr(s,'/') && !strstr(s,"order_size(") && !strstr(s,"exp(1)")))
    return;
  char * out=(char *)malloc(2*n+1); // p/q/m -> p/(q*m) adds 2 chars per term
  if (!out)
    return;
  int k=0;
  for (int i=0,j;i<n;i=j){
    if (s[i]==',' || s[i]=='='){ // a separator: kept
      out[k++]=s[i];
      j=i+1;
      continue;
    }
    int d=0;
    j=i+(s[i]=='+' || s[i]=='-');
    for (;j<n;++j){ // the term ends at a top-level + - , = (not an exponent's or a factor's sign)
      const char c=s[j];
      if (c=='(' || c=='[') ++d;
      else if (c==')' || c==']') --d;
      else if (!d && (c==',' || c=='=' || ((c=='+' || c=='-') && !strchr("^*/(e=,",s[j-1])))) break;
    }
    int t=i,e=j; // the term without its sign: s[t,e)
    char sg=0;
    if (s[t]=='+' || s[t]=='-') sg=s[t++];
    const char * o=strstr(s+t,"order_size(");
    if (o && o<s+e){ // m*order_size(v) -> O(m)
      const int m=o>s+t && o[-1]=='*'?int(o-s)-1:t;
      if (k && out[k-1]!=',' && out[k-1]!='=') out[k++]='+';
      out[k++]='O'; out[k++]='(';
      if (m==t) out[k++]='1';
      else { memcpy(out+k,s+t,m-t); k+=m-t; }
      out[k++]=')';
      continue;
    }
    if (sg) out[k++]=sg;
    int a=t,b;
    while (a<e && s[a]>='0' && s[a]<='9') ++a;
    if (a>t && a<e && s[a]=='/'){
      for (b=a+1;b<e && s[b]>='0' && s[b]<='9';++b) ;
      if (b>a+1 && b<e && s[b]=='*'){ // 1/6*x^3 -> x^3/6, 3/40*x^5 -> 3*x^5/40
        if (!(a==t+1 && s[t]=='1')){ memcpy(out+k,s+t,a-t); k+=a-t; out[k++]='*'; }
        memcpy(out+k,s+b+1,e-b-1); k+=e-b-1;
        out[k++]='/';
        memcpy(out+k,s+a+1,b-a-1); k+=b-a-1;
        continue;
      }
      if (b>a+1 && b<e && s[b]=='/'){ // 1/8/tan(2*x^4) -> 1/(8*tan(2*x^4))
        memcpy(out+k,s+t,a+1-t); k+=a+1-t;
        out[k++]='(';
        memcpy(out+k,s+a+1,b-a-1); k+=b-a-1;
        out[k++]='*';
        memcpy(out+k,s+b+1,e-b-1); k+=e-b-1;
        out[k++]=')';
        continue;
      }
    }
    memcpy(out+k,s+t,e-t); k+=e-t;
  }
  out[k]=0;
  for (char * p=out;(p=strstr(p,"exp(1)"));) // e^1 -> e (not inside a longer name: myexp(1))
    if (p>out && ((p[-1]>='a' && p[-1]<='z') || (p[-1]>='A' && p[-1]<='Z') || p[-1]=='_' || (p[-1]>='0' && p[-1]<='9')))
      p+=6;
    else {
      *p='e';
      memmove(p+1,p+6,strlen(p+6)+1);
      ++p;
    }
  str=out;
  free(out);
}

// giac prints each right side of x=... in parentheses: drops them when they enclose the whole side
static void strip_equation_parens(std::string & s){
  for (size_t i=0;i+1<s.size();++i){
    if (s[i]!='=' || s[i+1]!='(')
      continue;
    size_t j=i+1;
    int d=0;
    for (;j<s.size();++j){
      if (s[j]=='(' || s[j]=='[') ++d;
      else if ((s[j]==')' || s[j]==']') && --d==0) break;
    }
    if (j<s.size() && (j+1==s.size() || s[j+1]==',')){
      s.erase(j,1);
      s.erase(i+1,1);
    }
  }
}

static giac::gen solutions_as_equations(const giac::gen & s,const giac::gen & x){
  using namespace giac;
  if (s.type!=_VECT || s._VECTptr->empty())
    return s;
  vecteur w;
  for (const_iterateur it=s._VECTptr->begin();it!=s._VECTptr->end();++it) // an inequality's
    w.push_back(contains(*it,x)?*it:symb_equal(x,*it));                     // solution: as it is
                                                       // (is_constant_wrt: x<=-3 is constant)
  return w.size()==1?w.front():gen(w,_SEQ__VECT);
}
// solve([x+y=3,x-y=1],[x,y]) gives [[2,1]]: x=2, y=1 (several solutions: one list each)
static giac::gen system_as_equations(const giac::gen & s,const giac::gen & xs){
  using namespace giac;
  if (s.type!=_VECT || xs.type!=_VECT)
    return s;
  vecteur rows;
  for (const_iterateur it=s._VECTptr->begin();it!=s._VECTptr->end();++it){
    if (it->type!=_VECT || it->_VECTptr->size()!=xs._VECTptr->size())
      return s;
    vecteur r;
    for (unsigned k=0;k<xs._VECTptr->size();++k)
      r.push_back(symb_equal((*xs._VECTptr)[k],(*it->_VECTptr)[k]));
    rows.push_back(gen(r,_SEQ__VECT));
  }
  if (rows.size()==1)
    return rows.front();
  for (unsigned k=0;k<rows.size();++k)
    rows[k]=gen(*rows[k]._VECTptr);
  return gen(rows,_SEQ__VECT);
}
// (x>-2) and (x<2) as -2<x<2 (and <=, >=), in the printed solutions of an inequality
static void chain_inequalities(std::string & str){
  const char * s=str.c_str();
  if (!strstr(s," and "))
    return;
  const int n=strlen(s);
  char * out=(char *)malloc(n+1);
  if (!out)
    return;
  int k=0;
  for (int i=0;i<n;){
    // ((v>a) and (v<b)) or (v>a) and (v<b): v a name, a and b without parentheses
    const int p2=s[i]=='(' && s[i+1]=='('?2:s[i]=='('?1:0;
    int j=i+p2,v0=j;
    while (j<n && (isalnum((unsigned char)s[j]) || s[j]=='_')) ++j;
    const int v1=j;
    if (p2 && v1>v0 && (s[j]=='>' || s[j]=='<')){
      const bool gt=s[j]=='>';
      const bool e1=s[j+1]=='=';
      int a0=j+1+e1,a1=a0;
      while (a1<n && s[a1]!=')' && s[a1]!='(') ++a1;
      const int vl=v1-v0;
      if (a1<n && s[a1]==')' && !strncmp(s+a1+1," and (",6) && !strncmp(s+a1+7,s+v0,vl) && s[a1+7+vl]==(gt?'<':'>')){
        int b0=a1+8+vl;
        const bool e2=s[b0]=='=';
        b0+=e2;
        int b1=b0;
        while (b1<n && s[b1]!=')' && s[b1]!='(') ++b1;
        if (b1<n && s[b1]==')'){
          // gt: a<v<b with v>a, v<b; else v<a and v>b: b<v<a
          const int lo0=gt?a0:b0,lo1=gt?a1:b1,hi0=gt?b0:a0,hi1=gt?b1:a1;
          const bool elo=gt?e1:e2,ehi=gt?e2:e1;
          memcpy(out+k,s+lo0,lo1-lo0); k+=lo1-lo0;
          out[k++]='<'; if (elo) out[k++]='=';
          memcpy(out+k,s+v0,vl); k+=vl;
          out[k++]='<'; if (ehi) out[k++]='=';
          memcpy(out+k,s+hi0,hi1-hi0); k+=hi1-hi0;
          i=b1+1+(p2==2 && s[b1+1]==')');
          continue;
        }
      }
    }
    out[k++]=s[i++];
  }
  out[k]=0;
  str=out;
  free(out);
}


// ---- F4's forms: helpers ----
// a sin, cos or tan below a fraction bar: 1/cos(x), sin(x)/(1+cos(x)), cos(x)^-2. tlin and
// texpand make a mess of those (sin(x)/cos(x)^2: 2/(cos(2x)+1)*sin(x)), and a form that puts
// one there (in terms of sin of tan(x)^5: (...)/(15cos(x)sin(x)^4-...)) is no better
static bool trig_below(const giac::gen & g,bool out_of_ln=false){ // (out_of_ln: not inside a ln)
  using namespace giac;
  if (g.type==_VECT){
    for (const_iterateur it=g._VECTptr->begin();it!=g._VECTptr->end();++it)
      if (trig_below(*it,out_of_ln))
        return true;
    return false;
  }
  if (g.type!=_SYMB || (out_of_ln && g._SYMBptr->sommet==at_ln))
    return false;
  const gen & f=g._SYMBptr->feuille;
  const bool below=g.is_symb_of_sommet(at_inv) ||
    (g.is_symb_of_sommet(at_pow) && f.type==_VECT && f._VECTptr->size()==2 && (*f._VECTptr)[1].type==_INT_ && (*f._VECTptr)[1].val<0);
  if (below && (contains(f,at_sin) || contains(f,at_cos) || contains(f,at_tan)))
    return true;
  return trig_below(f,out_of_ln);
}
// two forms written with the same characters: the same terms or factors in another order
// (2sin(x)cos(x), 2cos(x)sin(x)). The forms of one result have one value: nothing new
static bool reordered(const std::string & a,const std::string & b){
  if (a.size()!=b.size())
    return false;
  short c[128]={0};
  for (size_t i=0;i<a.size();++i)
    ++c[(unsigned char)a[i]&127];
  for (size_t i=0;i<b.size();++i)
    if (--c[(unsigned char)b[i]&127]<0)
      return false;
  return true;
}
// the decimal form of an expression without its .0: 2.0cos(x)sin(x) is 2cos(x)sin(x), nothing new
static std::string no_point_zero(const std::string & t){
  std::string s;
  for (size_t i=0;i<t.size();++i){
    if (t[i]=='.' && i && t[i-1]>='0' && t[i-1]<='9' && i+1<t.size() && t[i+1]=='0' && (i+2==t.size() || t[i+2]<'0' || t[i+2]>'9')){
      ++i; // skip ".0"
      continue;
    }
    s+=t[i];
  }
  return s;
}
// a multiple angle large enough for texpand to blow up: sin(7x), cos(12*x) (the text as printed)
static bool big_multiple_angle(const std::string & t){
  for (size_t i=0;i+4<t.size();++i){
    if (t.compare(i,4,"sin(") && t.compare(i,4,"cos(") && t.compare(i,4,"tan("))
      continue;
    int k=0;
    for (size_t j=i+4;j<t.size() && t[j]>='0' && t[j]<='9' && k<100;++j)
      k=10*k+(t[j]-'0');
    if (k>6)
      return true;
  }
  return false;
}
// a long text with no + or - outside brackets: x^2*(13x^12+...)/182 cannot be drawn on several
// lines (the forms skip it)
static bool long_one_term(const std::string & t){
  if (t.size()<90)
    return false;
  int d=0;
  for (size_t i=0;i<t.size();++i){
    const char c=t[i];
    if (c=='(' || c=='[') ++d;
    else if (c==')' || c==']') --d;
    else if (i && !d && (c=='+' || c=='-') && !strchr("^*/(e=,",t[i-1])) return false;
  }
  return true;
}

// ---- F4's forms (answer.h) ----
const char * const answer_form_names[]={"simplified","one fraction","factored","expanded","trig combined","trig expanded","in terms of sin","in terms of cos","in terms of tan","decimal"};
static const unsigned char o_plain[]={FORM_SIMP,FORM_RAT,FORM_FACT,FORM_EXPA,FORM_DEC},
  o_number[]={FORM_DEC,FORM_SIMP,FORM_RAT,FORM_FACT,FORM_EXPA},
  o_trig[]={FORM_TLIN,FORM_TEXP,FORM_TSIN,FORM_TCOS,FORM_TTAN,FORM_SIMP,FORM_RAT,FORM_FACT,FORM_EXPA,FORM_DEC};

// tan is the only trig function of g (and it has one): tan identities are quotients by nature,
// 2tan(x)/(1-tan(x)^2) for tan(2x)
static bool tan_only(const giac::gen & g){
  using namespace giac;
  return contains(g,at_tan) && !contains(g,at_sin) && !contains(g,at_cos) && !contains(g,at_sec) && !contains(g,at_csc) && !contains(g,at_cot);
}

// the sin, cos, tan... of g as printed, each once: a trig form must change them (tlin of
// e^x*sin(x)*... only expands; in terms of sin of x*cos(x)-sin(x) is the same)
static void trig_nodes(const giac::gen & g,std::vector<std::string> & v){
  using namespace giac;
  if (g.type==_VECT){
    for (const_iterateur it=g._VECTptr->begin();it!=g._VECTptr->end();++it)
      trig_nodes(*it,v);
    return;
  }
  if (g.type!=_SYMB)
    return;
  const unary_function_ptr & u=g._SYMBptr->sommet;
  if (u==at_sin || u==at_cos || u==at_tan || u==at_sec || u==at_csc || u==at_cot){
    const std::string t=g.print(contextptr);
    for (size_t k=0;k<v.size();++k)
      if (v[k]==t)
        return;
    v.push_back(t);
    return;
  }
  trig_nodes(g._SYMBptr->feuille,v);
}
static bool same_trig_nodes(const giac::gen & a,const giac::gen & b){
  std::vector<std::string> u,v;
  trig_nodes(a,u);
  trig_nodes(b,v);
  if (u.size()!=v.size())
    return false;
  for (size_t i=0;i<u.size();++i){
    bool found=false;
    for (size_t j=0;j<v.size() && !found;++j)
      found=u[i]==v[j];
    if (!found)
      return false;
  }
  return true;
}
// a sin, cos or tan of something other than a polynomial (sin(cos(x)), cos(ln(x))): tlin and
// texpand turn products of those into sums of sin(x+cos(x)): no textbook form
static bool nested_trig(const giac::gen & g){
  using namespace giac;
  if (g.type==_VECT){
    for (const_iterateur it=g._VECTptr->begin();it!=g._VECTptr->end();++it)
      if (nested_trig(*it))
        return true;
    return false;
  }
  if (g.type!=_SYMB)
    return false;
  const unary_function_ptr & u=g._SYMBptr->sommet;
  if (u==at_sin || u==at_cos || u==at_tan){
    const gen & a=g._SYMBptr->feuille;
    vecteur v=lvar(a); // the non-polynomial parts of the argument
    for (unsigned k=0;k<v.size();++k)
      if (v[k].type==_SYMB)
        return true;
    return false;
  }
  return nested_trig(g._SYMBptr->feuille);
}
// a constant a decimal value tells something about: pi, e, sqrt(2), ln(2) (not 1/3: x^3/3 as
// 0.333333x^3 is no form a textbook shows)
static bool irrational_constant(const giac::gen & g){
  using namespace giac;
  if (g==cst_pi || g.type==_DOUBLE_)
    return true;
  if (g.type==_VECT){
    for (const_iterateur it=g._VECTptr->begin();it!=g._VECTptr->end();++it)
      if (irrational_constant(*it))
        return true;
    return false;
  }
  if (g.type!=_SYMB)
    return false;
  if (lidnt(g).empty() && !(g._SYMBptr->sommet==at_inv || g._SYMBptr->sommet==at_neg || g._SYMBptr->sommet==at_prod || g._SYMBptr->sommet==at_plus))
    return true; // sqrt(2), ln(2), e (exp(1)), atan(2)
  return irrational_constant(g._SYMBptr->feuille);
}
// x^0.5 is sqrt(x) in a decimal form (evalf writes 2.0*x^0.5)
static giac::gen half_sqrt(const giac::gen & g,const giac::gen & f,const giac::gen &){
  using namespace giac;
  if (g._SYMBptr->sommet==at_pow && f.type==_VECT && f._VECTptr->size()==2 && f._VECTptr->back().type==_DOUBLE_){
    const double e=f._VECTptr->back()._DOUBLE_val;
    if (e==0.5)
      return symbolic(at_pow,makesequence(f._VECTptr->front(),fraction(1,2)));
    if (e==-0.5)
      return symb_inv(symbolic(at_pow,makesequence(f._VECTptr->front(),fraction(1,2))));
  }
  return symbolic(g._SYMBptr->sommet,f);
}
static giac::gen normal_args(const giac::gen & g); // (below)
// a term with a denominator: 1/x, x/2, ln(x)/3 (one fraction of a sum of those is a form)
static bool with_denominator(const giac::gen & t){
  using namespace giac;
  if (t.type==_FRAC || t.is_symb_of_sommet(at_inv))
    return true;
  if (t.is_symb_of_sommet(at_pow) && t._SYMBptr->feuille.type==_VECT && t._SYMBptr->feuille._VECTptr->size()==2){
    const gen & e=t._SYMBptr->feuille._VECTptr->back();
    return (e.type==_INT_ || e.type==_FRAC) && is_strictly_positive(-e,contextptr);
  }
  if (t.is_symb_of_sommet(at_neg))
    return with_denominator(t._SYMBptr->feuille);
  if (t.is_symb_of_sommet(at_prod) && t._SYMBptr->feuille.type==_VECT){
    for (const_iterateur it=t._SYMBptr->feuille._VECTptr->begin();it!=t._SYMBptr->feuille._VECTptr->end();++it)
      if (with_denominator(*it))
        return true;
  }
  return false;
}
// what a form is at its top: a sum (expanded), a product of factors depending on x (factored)
// a sum of terms without a sum as a factor (outside functions): x^2*e^x-2x*e^x+2e^x, not
// x*(2+ln(x)^2)-2x*ln(x); and not one denominator under every term (-x^4/(x^6-..)-4x^2/(x^6-..))
static bool fully_expanded(const giac::gen & r){
  using namespace giac;
  const gen & t=r.is_symb_of_sommet(at_neg)?r._SYMBptr->feuille:r;
  if (!t.is_symb_of_sommet(at_plus) || t._SYMBptr->feuille.type!=_VECT)
    return true;
  const vecteur & w=*t._SYMBptr->feuille._VECTptr;
  gen d0;
  bool same=true;
  for (unsigned k=0;k<w.size();++k){
    const gen & u=w[k].is_symb_of_sommet(at_neg)?w[k]._SYMBptr->feuille:w[k];
    if (u.is_symb_of_sommet(at_prod) && u._SYMBptr->feuille.type==_VECT){
      for (const_iterateur it=u._SYMBptr->feuille._VECTptr->begin();it!=u._SYMBptr->feuille._VECTptr->end();++it)
        if (it->is_symb_of_sommet(at_plus))
          return false;
    }
    gen d=1; // the denominator as written (denom cancels: x^4/(x^6-2x^4+x^2) gives x^4-2x^2+1)
    if (u.is_symb_of_sommet(at_inv))
      d=u._SYMBptr->feuille;
    else if (u.is_symb_of_sommet(at_prod) && u._SYMBptr->feuille.type==_VECT){
      for (const_iterateur it=u._SYMBptr->feuille._VECTptr->begin();it!=u._SYMBptr->feuille._VECTptr->end();++it)
        if (it->is_symb_of_sommet(at_inv))
          d=d*it->_SYMBptr->feuille;
    }
    if (!k)
      d0=d;
    else if (!(d==d0))
      same=false;
  }
  return !same || lidnt(d0).empty();
}
static bool top_sum(const giac::gen & r){
  using namespace giac;
  const gen & t=r.is_symb_of_sommet(at_neg)?r._SYMBptr->feuille:r;
  return t.is_symb_of_sommet(at_plus);
}
// (factored: two factors with x or more above the bar or below it, or a power of a sum:
// (cos(x)ln|..|+tan(x))/(2cos(x)) is one fraction, not factored)
static bool power_of_sum(const giac::gen & t){
  using namespace giac;
  return t.is_symb_of_sommet(at_pow) && t._SYMBptr->feuille.type==_VECT && t._SYMBptr->feuille._VECTptr->front().is_symb_of_sommet(at_plus);
}
static void count_factors(const giac::gen & t,int & up,int & down,bool & pw,bool below){
  using namespace giac;
  if (t.is_symb_of_sommet(at_prod) && t._SYMBptr->feuille.type==_VECT){
    for (const_iterateur it=t._SYMBptr->feuille._VECTptr->begin();it!=t._SYMBptr->feuille._VECTptr->end();++it)
      count_factors(*it,up,down,pw,below);
    return;
  }
  if (t.is_symb_of_sommet(at_inv)){
    count_factors(t._SYMBptr->feuille,up,down,pw,!below);
    return;
  }
  if (lidnt(t).empty())
    return;
  pw=pw || power_of_sum(t);
  ++(below?down:up);
}
static bool top_product(const giac::gen & r){
  using namespace giac;
  const gen & t=r.is_symb_of_sommet(at_neg)?r._SYMBptr->feuille:r;
  int up=0,down=0;
  bool pw=false;
  count_factors(t,up,down,pw,false);
  return pw || up>=2 || down>=2;
}
// every sin, cos, tan... of g is inside a ln (ln|sec(x)+tan(x)|, ln|sin(x)|): its trig and
// algebra forms only rewrite the inside of the log, slowly (10 s on the calculator); in terms of
// sin and cos is the one that tells something: ln|(sin(x)+1)/cos(x)|
static bool trig_out_of_ln(const giac::gen & g){
  using namespace giac;
  if (g.type==_VECT){
    for (const_iterateur it=g._VECTptr->begin();it!=g._VECTptr->end();++it)
      if (trig_out_of_ln(*it))
        return true;
    return false;
  }
  if (g.type!=_SYMB || g._SYMBptr->sommet==at_ln)
    return false;
  const unary_function_ptr & u=g._SYMBptr->sommet;
  if (u==at_sin || u==at_cos || u==at_tan || u==at_sec || u==at_csc || u==at_cot)
    return true;
  return trig_out_of_ln(g._SYMBptr->feuille);
}
// a power with the variable in its exponent (2^x): its decimal form is exp(0.693147x)
static bool var_exponent(const giac::gen & g){
  using namespace giac;
  if (g.type==_VECT){
    for (const_iterateur it=g._VECTptr->begin();it!=g._VECTptr->end();++it)
      if (var_exponent(*it))
        return true;
    return false;
  }
  if (g.type!=_SYMB)
    return false;
  if (g._SYMBptr->sommet==at_pow && g._SYMBptr->feuille.type==_VECT && g._SYMBptr->feuille._VECTptr->size()==2 && !lidnt(g._SYMBptr->feuille._VECTptr->back()).empty())
    return true;
  return var_exponent(g._SYMBptr->feuille);
}
// The functions of g (ln(...), |...|, sin(...), sec(...), e^(...), atan(...)...) as names: one
// fraction, factored and expanded then work on the algebra around them, as their names say, and
// fast (factor of (sec(x)tan(x)+ln|sec(x)+tan(x)|)/2 took 0.3 s on the PC, minutes on the
// calculator, to write 1/cos(x)); expanded keeps sec(x)tan(x)/2+ln|sec(x)+tan(x)|/2
static void function_nodes(const giac::gen & g,giac::vecteur & from,bool trig){
  using namespace giac;
  if (g.type==_VECT){
    for (const_iterateur it=g._VECTptr->begin();it!=g._VECTptr->end();++it)
      function_nodes(*it,from,trig);
    return;
  }
  if (g.type!=_SYMB)
    return;
  const unary_function_ptr & u=g._SYMBptr->sommet;
  if (u==at_plus || u==at_prod || u==at_neg || u==at_inv || u==at_pow
      || (trig && (u==at_sin || u==at_cos || u==at_tan || u==at_sec || u==at_csc || u==at_cot))){
    function_nodes(g._SYMBptr->feuille,from,trig);
    return;
  }
  if (lidnt(g).empty()) // a number: sqrt(2), ln(2)
    return;
  for (unsigned k=0;k<from.size();++k)
    if (from[k]==g)
      return;
  from.push_back(g);
}
// 1/cos(u) as sec(u), 1/sin(u) as csc(u) (giac's eval writes them so): the algebra forms of an
// answer written with sec keep it (sec(x)tan(x)/2+ln|sec(x)+tan(x)|/2)
static giac::gen sec_back(const giac::gen & g,const giac::gen & f,const giac::gen &){
  using namespace giac;
  if (g._SYMBptr->sommet==at_pow && f.type==_VECT && f._VECTptr->size()==2 && f._VECTptr->back().type==_INT_ && f._VECTptr->back().val<0){
    const int k=-f._VECTptr->back().val; // cos(x)^-1 (trig_powers): 1/cos(x), sec(x)
    const gen h=k==1?f._VECTptr->front():symbolic(at_pow,makesequence(f._VECTptr->front(),k));
    return sec_back(symbolic(at_inv,h),h,0);
  }
  if (g._SYMBptr->sommet==at_inv && f.is_symb_of_sommet(at_prod) && f._SYMBptr->feuille.type==_VECT){
    vecteur w(*f._SYMBptr->feuille._VECTptr); // giac's 1/(5cos(x)^5): sec(x)^5/5
    for (iterateur it=w.begin();it!=w.end();++it)
      *it=sec_back(symbolic(at_inv,*it),*it,0);
    return symbolic(at_prod,gen(w,_SEQ__VECT));
  }
  if (g._SYMBptr->sommet==at_inv){
    gen b=f,n=1;
    if (f.is_symb_of_sommet(at_pow) && f._SYMBptr->feuille.type==_VECT && f._SYMBptr->feuille._VECTptr->size()==2){
      b=f._SYMBptr->feuille._VECTptr->front();
      n=f._SYMBptr->feuille._VECTptr->back();
    }
    if (n.type==_INT_ && (b.is_symb_of_sommet(at_cos) || b.is_symb_of_sommet(at_sin))){
      const gen t=symbolic(b.is_symb_of_sommet(at_cos)?at_sec:at_csc,b._SYMBptr->feuille);
      return is_one(n)?t:symbolic(at_pow,makesequence(t,n));
    }
  }
  if (g._SYMBptr->sommet==at_prod && f.type==_VECT){ // cos(u)*csc(u)^n: cot(u)*csc(u)^(n-1);
    vecteur w(*f._VECTptr);                         // sin(u)*sec(u)^n: tan(u)*sec(u)^(n-1)
    for (unsigned i=0;i<w.size();++i){
      const bool sn=w[i].is_symb_of_sommet(at_sin);
      if (!sn && !w[i].is_symb_of_sommet(at_cos))
        continue;
      const gen & u=w[i]._SYMBptr->feuille;
      for (unsigned j=0;j<w.size();++j){
        gen b=w[j],n=1;
        if (b.is_symb_of_sommet(at_pow) && b._SYMBptr->feuille.type==_VECT){
          n=b._SYMBptr->feuille._VECTptr->back();
          b=b._SYMBptr->feuille._VECTptr->front();
        }
        if (!b.is_symb_of_sommet(sn?at_sec:at_csc) || !(b._SYMBptr->feuille==u) || n.type!=_INT_ || n.val<1)
          continue;
        w[i]=symbolic(sn?at_tan:at_cot,u);
        if (n.val==1)
          w.erase(w.begin()+j);
        else
          w[j]=n.val==2?b:symbolic(at_pow,makesequence(b,n-1));
        return w.size()==1?w.front():symbolic(at_prod,gen(w,_SEQ__VECT));
      }
    }
  }
  return symbolic(g._SYMBptr->sommet,f);
}
static bool trig_out_of_ln(const giac::gen & g); // (below)
// sec_back of g if it leaves no sin or cos ((2-cos(x)^2)*sec(x)^3 mixes them: g as it was)
static giac::gen sec_only(const giac::gen & g){
  using namespace giac;
  const gen r=map_nodes(g,sec_back,0);
  return contains(r,at_sin) || contains(r,at_cos)?g:r;
}
static giac::gen form_op(int f,const giac::gen & g){
  using namespace giac;
  static const unary_function_ptr * const ops[]={at_simplify,at_ratnormal,at_factor,at_expand,at_tlin,at_texpand,at_trigsin,at_trigcos,at_trigtan,at_evalf};
  // (the trig forms: the other functions as names, the trig ones as they are: in terms of sin of
  // x*tan(x)+ln|cos(x)| rewrote inside the ln too, 4 s per form on the calculator)
  if (f==FORM_SIMP || f==FORM_DEC)
    return (*ops[f])(g,contextptr);
  vecteur from,to;
  const bool trig=f>=FORM_TLIN && f<=FORM_TTAN;
  function_nodes(g,from,trig);
  if (trig && !trig_out_of_ln(g)) // (all the trig inside a log: rewriting there is the point)
    from.clear();
  if (from.empty())
    return (*ops[f])(g,contextptr);
  for (unsigned k=0;k<from.size();++k){
    char b[16];
    sprintf(b,"fn__%u",k);
    to.push_back(gen(b,contextptr));
  }
  const gen r=(*ops[f])(subst(g,from,to,false,contextptr),contextptr);
  if (is_undef(r) || r.type==_STRNG)
    return r;
  return subst(r,to,from,false,contextptr);
}
// form k of F's order, computed (once): F.text[k] ("" if the form is not offered), F.form[k]
static void form_compute(answer_forms & F,int k){
  using namespace giac;
  F.done[k]=1;
  F.text[k].clear();
  const int f=F.order[k];
  const gen & g=F.g;
  // giac's simplify of trig is slow (2.6 s on the calculator for 2sin(x)cos(x)) and gives the tan
  // form (in terms of tan has it) or tan(x/2) messes, except for trig below a bar: sin(x)/(1+cos(x))
  // is tan(x/2)
  if (f==FORM_SIMP && (xcas::has_radical(g) || (F.trig && (!F.below || F.size>12)))) // (and small: 0.26 s
    return; // on the PC for the integral of sec(x)^3, for nothing)
  if ((f==FORM_TLIN || f==FORM_TEXP) && (F.below || nested_trig(g)))
    return;
  if (f==FORM_TEXP && big_multiple_angle(F.orig))
    return;
  // a big trig answer (1/(sin(x)+cos(x)) integrated): its trig forms took 1 s on the PC (minutes
  // on the calculator) for messes
  if (f>=FORM_TLIN && f<=FORM_TTAN && F.size>60)
    return;
  if (F.trig && !trig_out_of_ln(g) && ((f!=FORM_TSIN && f!=FORM_DEC) || F.size>20)) // (in terms of sin
    return; // of the integral of 1/(sin(x)+cos(x)): 0.2 s on the PC)
  // one fraction: of a sum with a fraction in it (of a product, ratnormal only expands the
  // numerator: (x^2+1)^(3/2)/3 as (x^2*sqrt(x^2+1)+sqrt(x^2+1))/3; of x^2*sin(x)+..., it expands)
  if (f==FORM_RAT && !F.number){
    if (!top_sum(g))
      return;
    const gen & t=g.is_symb_of_sommet(at_neg)?g._SYMBptr->feuille:g;
    bool den=false;
    for (const_iterateur it=t._SYMBptr->feuille._VECTptr->begin();!den && it!=t._SYMBptr->feuille._VECTptr->end();++it)
      den=with_denominator(*it);
    if (!den)
      return;
  }
  // decimal of an expression: not with a variable exponent (2^x: exp(0.693147x)), nor when large
  // (1/(sin(x)+cos(x)) integrated: 2.7 s on the PC)
  if (f==FORM_DEC && !F.number && (var_exponent(g) || F.size>40))
    return;
  const vecteur lv=lvar(g);
  const bool poly=lv.size()==1 && lv.front().type==_IDNT; // a rational function of x
  if (f==FORM_DEC && !F.number && !irrational_constant(g)) // x^3/3 is not 0.333333x^3
    return;
  gen r;
  if (f==FORM_DEC && !lidnt(g).empty()) // 0.0714286x^14, not x^14/14.0; expand only polynomials:
    r=evalf(poly?expand(g,contextptr):g,1,contextptr); // (expand of ln|..|: 6 s on the PC)
  else
    r=form_op(f,g);
  trace_time(answer_form_names[f]);
  if (f==FORM_FACT && !is_undef(r) && r.type!=_STRNG)
    r=xcas::merge_sqrt(r,contextptr); // (1-25*x^2)^(3/2), not (5*x+1)*(5*x-1)*sqrt(...)
  trace_time("merge_sqrt");
  if (is_undef(r) || r.type==_STRNG)
    return;
  // each form is what its name says: factored is a product (factor of ln|sec(x)+tan(x)| only
  // wrote 1/cos(x)), expanded a sum (expand of 1/(x^2+1)^(3/2) expanded the denominator)
  if ((f==FORM_FACT && !top_product(r)) || (f==FORM_EXPA && !top_sum(r)))
    return;
  if (f==FORM_RAT){ // one fraction over a denominator with x: of rational terms only (not
    vecteur fn;    // ((x-1)ln|x|-(x-1)ln|x-1|-1)/(x-1) for ln|x|-ln|x-1|-1/(x-1))
    function_nodes(g,fn,false);
    if (!fn.empty() && !lidnt(_denom(r,contextptr)).empty())
      return;
  }
  if (f==FORM_RAT && r.type==_SYMB){ // its denominator factored, as the answer's: (...)/(2(x^2+1))
    const gen d=_denom(r,contextptr);
    if (d.type==_SYMB){
      const gen fd=_factor(d,contextptr);
      if (!is_undef(fd) && fd.type!=_STRNG && taille(fd,200)<=taille(d,200)){
        const gen nu=_numer(r,contextptr);
        r=is_one(nu)?symb_inv(fd):symb_prod(nu,symb_inv(fd));
      }
    }
  }
  if ((f==FORM_FACT || f==FORM_EXPA) && (strstr(F.orig.c_str(),"sec(") || strstr(F.orig.c_str(),"csc(") || strstr(F.orig.c_str(),"cot(")))
    r=sec_only(r);
  if (f!=FORM_DEC && r.type==_SYMB && taille(r,200)<200) // in lowest terms inside functions too
    r=normal_args(r); // (ln|(sin(x)+1)/cos(x)|, atan((2x-1)/sqrt(3)))
  trace_time("normal_args");
  trace_step(answer_form_names[f],r);
  if (f==FORM_TTAN){
    if (!tan_only(r)) // sin(x) is cos(x)*tan(x): no
      return;
    const gen d=_denom(r,contextptr); // tan(x)^4/(tan(x)^2+1)^2, not .../(tan(x)^4+2*tan(x)^2+1)
    if (d.type==_SYMB){
      const gen fd=_factor(d,contextptr);
      if (!is_undef(fd) && fd.type!=_STRNG && taille(fd,200)<taille(d,200)){
        const gen nu=_numer(r,contextptr);
        r=is_one(nu)?symb_inv(fd):symb_prod(nu,symb_inv(fd));
      }
    }
  }
  const int size=F.size,rsize=taille(r,4*size+64);
  if (F.trig && f==FORM_SIMP && rsize>=size) // kept only when shorter
    return;
  if (f==FORM_SIMP && rsize>=size) // "simplified" is smaller (x*e^x-e^x for e^x*(x-1): expanded)
    return;
  // factored over radicals: (cos(x)+sqrt(2)/2)*(2cos(x)-sqrt(2)) for 2cos(x)^2-1,
  // 2x/((x^2+x*sqrt(2)+1)*(x^2-x*sqrt(2)+1)) for 2x/(1+x^4): no textbook form (factor(...) typed
  // still gives them)
  if (f==FORM_FACT && xcas::has_radical(r) && !xcas::has_radical(g))
    return;
  if (f>=FORM_TSIN && f<=FORM_TTAN && contains(r,at_ln) && trig_below(r,true) && taille(r,4*F.size+64)>F.size)
    return; // ((ln|..|*sin(x)^2-ln|..|-sin(x))/(2sin(x)^2-2) for the integral of sec(x)^3)
  if (f==FORM_EXPA && !fully_expanded(r))
    return;
  // a trig form that rewrites no trig function only moved the algebra around
  if (f>=FORM_TLIN && f<=FORM_TTAN && same_trig_nodes(g,r))
    return;
  // a trig form that puts trig below a bar (tlin, texpand of tan(x)^5: messes), or a blown-up
  // one, unless shorter; tan identities are quotients (tan(2x), in terms of tan)
  if (f>=FORM_TLIN && f<=FORM_TTAN && rsize>=size && ((trig_below(r) && !F.below && !tan_only(r)) || rsize>4*size+40))
    return;
  // in textbook form, as the answer was: arctan((x+2)/2), C1, 4-x^2; compared as shown
  if (f==FORM_DEC)
    r=map_nodes(r,half_sqrt,0);
  if (r.type==_SYMB && taille(r,200)<200 && !contains(r,at_order_size))
    r=positive_first(r);
  trace_time("positive_first");
  std::string text=r.print(contextptr);
  textbook(text);
  textbook_constants(text);
  if (f==FORM_DEC && !F.number)
    text=no_point_zero(text); // 0.5*ln(abs(x+1)), not 0.5*ln(abs(x+1.0))
  if (long_one_term(text))
    return;
  F.text[k]=text;
  F.form[k]=r;
}

void answer_forms_start(answer_forms & F,const std::string & orig,bool number){
  using namespace giac;
  F.orig=orig;
  // left unevaluated by giac (sum(...), integrate(...)): no forms (evaluating it again for them
  // took 69 s for a sum)
  const bool open=strstr(orig.c_str(),"sum(") || strstr(orig.c_str(),"integrate(") || strstr(orig.c_str(),"limit(");
  // evaluated: ratnormal of the parsed text printed ((x^2)-1)/(x^3+x)
  F.g=open?gen(0):eval(gen(orig,contextptr),1,contextptr);
  F.number=number;
  F.trig=!number && (contains(F.g,at_sin) || contains(F.g,at_cos) || contains(F.g,at_tan));
  F.order=number?o_number:F.trig?o_trig:o_plain;
  F.n=number?sizeof(o_number):F.trig?sizeof(o_trig):sizeof(o_plain);
  if (open)
    F.n=0;
  F.size=taille(F.g,400);
  F.below=F.trig && trig_below(F.g);
  F.text.assign(F.n,std::string());
  F.form.assign(F.n,gen(0));
  F.done.assign(F.n,0);
  F.seen.clear();
}

int answer_forms_next(answer_forms & F,int k,const std::string & cur,void (*busy)()){
  if (!k){ // a new round
    F.seen.clear();
    F.seen.push_back(F.orig);
  }
  for (++k;k<=F.n;++k){
    if (!F.done[k-1]){
      if (busy)
        busy();
      form_compute(F,k-1);
      if (giac::ctrl_c || giac::interrupted){ // CLEAR, or memory ran out: not this form, the original
        F.text[k-1].clear();
        return 0;
      }
    }
    const std::string & text=F.text[k-1];
    if (text.empty())
      continue;
    // nothing new: the same terms in another order as what is shown or was shown (2.0cos(x)sin(x)
    // is 2cos(x)sin(x))
    const std::string bare=F.order[k-1]==FORM_DEC && !F.number?no_point_zero(text):text;
    bool seen=reordered(text,cur) || reordered(bare,cur);
    for (size_t j=0;!seen && j<F.seen.size();++j)
      seen=reordered(F.seen[j],text) || reordered(F.seen[j],bare);
    if (!seen){
      F.seen.push_back(text);
      return k;
    }
  }
  return 0;
}

// A result as a textbook writes it, inside its functions too: atan((2x-1)/sqrt(3)), not giac's
// atan((x-1/2)/(1/2*sqrt(3))) (ratnormal of the whole leaves a function's argument as it is);
// 2^x*(x*ln(2)-1)/ln(2)^2, not exp(x*ln(2))*(...)
static giac::gen normal_node(const giac::gen & g,const giac::gen & f0,const giac::gen &){
  using namespace giac;
  const unary_function_ptr & u=g._SYMBptr->sommet;
  gen f=f0;
  if (u==at_neg && f.is_symb_of_sommet(at_neg)) // -ln(sqrt(2)/2) is -(-ln(2)/2) below: ln(2)/2
    return f._SYMBptr->feuille;
  if (u==at_ln && f.is_symb_of_sommet(at_pow) && f._SYMBptr->feuille.type==_VECT && f._SYMBptr->feuille._VECTptr->size()==2){
    const gen & b=f._SYMBptr->feuille._VECTptr->front(),& e=f._SYMBptr->feuille._VECTptr->back();
    const vecteur vb=lidnt(b); // ln(x^2), ln((x-1)^2): 2*ln|x-1| (b linear in one variable)
    if (e.type==_INT_ && e.val>0 && e.val%2==0 && vb.size()==1 && is_constant_wrt(derive(b,vb.front(),contextptr),vb.front(),contextptr))
      return symbolic(at_prod,makesequence(e,symbolic(at_ln,symbolic(at_abs,b))));
  }
  if (u==at_ln && f.type==_SYMB && lidnt(f).empty()){ // -ln(2)/2, not ln(sqrt(2)/2); ln(sqrt(2)+1), not
    const gen l=symbolic(at_ln,xcas::has_radical(f)?safe_normal(f):f),r=ratnormal(lnexpand(l,contextptr),contextptr); // ln(2/sqrt(2)+1)
    return !is_undef(r) && r.type!=_STRNG && taille(r,100)<taille(l,100)?r:l;
  }
  if ((u==at_atan || u==at_asin || u==at_acos || u==at_ln || u==at_abs || u==at_exp || u==at_sin || u==at_cos || u==at_tan)
      && f.type==_SYMB && !lidnt(f).empty()){
    const gen r=ratnormal(f,contextptr);
    if (!is_undef(r) && r.type!=_STRNG && taille(r,200)<taille(f,200))
      f=r;
  }
  return symbolic(u,f);
}
static giac::gen normal_args(const giac::gen & g){
  return map_nodes(g,normal_node,0);
}
static giac::gen textbook_powers(const giac::gen & g){
  using namespace giac;
  if (!contains(g,at_exp) || !contains(g,at_ln))
    return g;
  const gen r=_exp2pow(g,contextptr); // exp(x*ln(2)): 2^x
  return is_undef(r) || r.type==_STRNG || taille(r,400)>taille(g,400)?g:r;
}

// giac's antiderivatives -ln(sqrt(x^2+9)-x) (1/sqrt(x^2+9)) and ln|x-sqrt(x^2-4)| are
// ln(x+sqrt(x^2+9)) and -ln|x+sqrt(x^2-4)| up to a constant: ln|sqrt(P)-Q| = ln|c|-ln|sqrt(P)+Q|
// when c = P-Q^2 is a constant (the constant then goes into + C; |..| kept when c<0)
static bool sqrt_of(const giac::gen & s,giac::gen & p){
  using namespace giac;
  if (!s.is_symb_of_sommet(at_pow) || s._SYMBptr->feuille.type!=_VECT || s._SYMBptr->feuille._VECTptr->size()!=2)
    return false;
  const gen & e=s._SYMBptr->feuille._VECTptr->back();
  if (e.type!=_FRAC || !is_one(e._FRACptr->num) || !(e._FRACptr->den==2))
    return false;
  p=s._SYMBptr->feuille._VECTptr->front();
  return true;
}
static giac::gen ln_conjugate(const giac::gen & g,const giac::gen & f,const giac::gen & x){
  using namespace giac;
  if (g._SYMBptr->sommet==at_ln){
    const gen A=f.is_symb_of_sommet(at_abs)?f._SYMBptr->feuille:f;
    if (A.is_symb_of_sommet(at_plus) && A._SYMBptr->feuille.type==_VECT){
      const vecteur & v=*A._SYMBptr->feuille._VECTptr;
      for (unsigned k=0;k<v.size();++k){
        gen P,Q,R=0;
        const gen & s=v[k];
        for (unsigned j=0;j<v.size();++j)
          if (j!=k)
            R=R+v[j];
        // sqrt(P)+R with R decreasing (sqrt(P)-x+1: Q=x-1), or R-sqrt(P) with R increasing
        const bool flip=s.is_symb_of_sommet(at_neg) && sqrt_of(s._SYMBptr->feuille,P);
        if (!flip && !sqrt_of(s,P))
          continue;
        const gen a=derive(R,x,contextptr),ae=evalf(a,1,contextptr);
        if (!is_constant_wrt(a,x,contextptr) || ae.type!=_DOUBLE_ || ae._DOUBLE_val==0 || (ae._DOUBLE_val>0)!=flip)
          break;
        Q=flip?R:-R;
        const gen c=ratnormal(P-Q*Q,contextptr),ce=evalf(c,1,contextptr);
        if (is_zero(c) || !is_constant_wrt(c,x,contextptr) || ce.type!=_DOUBLE_)
          break;
        gen B=symbolic(at_plus,makesequence(Q,symbolic(at_pow,makesequence(P,fraction(1,2))))); // Q+sqrt(P)
        if (ce._DOUBLE_val<0)
          B=symbolic(at_abs,B);
        const gen L=ln(abs(c,contextptr),contextptr),m=symbolic(at_neg,symbolic(at_ln,B));
        return is_zero(L)?m:symbolic(at_plus,makesequence(L,m));
      }
    }
  }
  if (g._SYMBptr->sommet==at_neg && f.is_symb_of_sommet(at_neg)) // -(-ln(...))
    return f._SYMBptr->feuille;
  return symbolic(g._SYMBptr->sommet,f);
}

// Integration by parts where giac gives up (it left x*sec(x)^2, x*csc(x)^2, x*sec(x)*tan(x),
// x*tan(x)^2 as integrate(...)): f = P*h, P the polynomial factors (degree <= 3), h with an
// antiderivative H: int f = P*H - int P'*H, the last one by giac or by the same rule. undef if not.
static bool poly_in(const giac::gen & t,const giac::gen & x){
  using namespace giac;
  const vecteur lv=lvar(t);
  return lv.size()==1 && lv.front()==x && is_constant_wrt(_denom(t,contextptr),x,contextptr);
}
static giac::gen by_parts(const giac::gen & f,const giac::gen & x,int depth){
  using namespace giac;
  if (depth>3)
    return undef;
  const vecteur fac=f.is_symb_of_sommet(at_prod) && f._SYMBptr->feuille.type==_VECT?*f._SYMBptr->feuille._VECTptr:vecteur(1,f);
  gen P=1,h=1;
  for (unsigned i=0;i<fac.size();++i)
    if (poly_in(fac[i],x) || is_constant_wrt(fac[i],x,contextptr))
      P=P*fac[i];
    else
      h=h*fac[i];
  if (is_constant_wrt(P,x,contextptr) || is_one(h))
    return undef;
  const gen dg=_degree(makesequence(P,x),contextptr);
  if (dg.type!=_INT_ || dg.val>3)
    return undef;
  gen H=table_integral(symbolic(at_integrate,makesequence(h,x))); // sec(x)tan(x): sec(x) at once
  if (is_zero(H))
    H=_integrate(makesequence(h,x),contextptr);
  if (is_undef(H) || H.type==_STRNG || contains(H,at_integrate))
    return undef;
  const gen R=ratnormal(derive(P,x,contextptr)*H,contextptr);
  gen I=table_integral(symbolic(at_integrate,makesequence(R,x)));
  if (is_zero(I))
    I=_integrate(makesequence(R,x),contextptr);
  if (is_undef(I) || I.type==_STRNG || contains(I,at_integrate))
    I=by_parts(R,x,depth+1);
  if (is_undef(I))
    return undef;
  return P*H-I;
}
// x*sec(x)^2, x^2*csc(x)^2...: a polynomial times a function of the table (sec(u)^2...), by parts
// at once (giac took 38 s on the calculator to give up); 0 for anything else
static giac::gen parts_table(const giac::gen & g){
  using namespace giac;
  if (!(g.is_symb_of_sommet(at_integrate) || g.is_symb_of_sommet(at_int)) || g._SYMBptr->feuille.type!=_VECT || g._SYMBptr->feuille._VECTptr->size()!=2)
    return 0;
  const gen & x=(*g._SYMBptr->feuille._VECTptr)[1];
  if (x.type!=_IDNT)
    return 0;
  const gen f=eval((*g._SYMBptr->feuille._VECTptr)[0],1,contextptr);
  if (!f.is_symb_of_sommet(at_prod) || f._SYMBptr->feuille.type!=_VECT)
    return 0;
  gen P=1,h=1;
  const vecteur & fac=*f._SYMBptr->feuille._VECTptr;
  for (unsigned i=0;i<fac.size();++i)
    if (poly_in(fac[i],x) || is_constant_wrt(fac[i],x,contextptr))
      P=P*fac[i];
    else
      h=h*fac[i];
  gen c=1,v=0; // h with sec, csc, tan, cot (negative powers of sin, cos): x*sec(x)tan(x) took 75 s on
  int m=0,n=0; // the calculator; giac's for x^2*cos(x): (x^2-2)sin(x)+2x*cos(x)
  if (is_constant_wrt(P,x,contextptr) || (is_zero(table0(symbolic(at_integrate,makesequence(h,x))))
      && !(trig_factor(h,1,x,c,v,m,n) && (m<0 || n<0))))
    return 0;
  const gen r=by_parts(f,x,0);
  return is_undef(r)?gen(0):r;
}
// giac's integrate(...) left as is, done by parts: an antiderivative, or a definite integral
// (the antiderivative at the bounds, when it agrees with the integral computed numerically: a
// pole between the bounds makes it wrong; else the numeric value)
static giac::gen integrate_by_parts(const giac::gen & in,const giac::gen & g){
  using namespace giac;
  if (!in.is_symb_of_sommet(at_integrate) && !in.is_symb_of_sommet(at_int))
    return g;
  const gen & a=in._SYMBptr->feuille;
  if (a.type!=_VECT || (a._VECTptr->size()!=2 && a._VECTptr->size()!=4) || (*a._VECTptr)[1].type!=_IDNT)
    return g;
  const vecteur & v=*a._VECTptr;
  const gen F=by_parts(eval(v[0],1,contextptr),v[1],0);
  if (v.size()==2)
    return is_undef(F)?g:F;
  const gen lo=eval(v[2],1,contextptr),hi=eval(v[3],1,contextptr),le=evalf(lo,1,contextptr),he=evalf(hi,1,contextptr);
  if (le.type!=_DOUBLE_ || he.type!=_DOUBLE_)
    return g;
  const gen num=evalf(_integrate(makesequence(eval(v[0],1,contextptr),v[1],le,he),contextptr),1,contextptr);
  if (num.type!=_DOUBLE_)
    return g;
  if (!is_undef(F)){
    const gen ex=ratnormal(subst(F,v[1],hi,false,contextptr)-subst(F,v[1],lo,false,contextptr),contextptr),exe=evalf(ex,1,contextptr);
    if (exe.type==_DOUBLE_ && fabs(exe._DOUBLE_val-num._DOUBLE_val)<1e-4*(1+fabs(num._DOUBLE_val))) // (7-digit floats)
      return ex;
  }
  return num;
}

// A sum of fractional powers of x (the power rule's answers) in textbook form: -7x^(5/2)+4x^(1/3)/3
// +28/(9x^(5/9)), not -x^3/sqrt(x)-6x^(5/2)+28x^(1/3)/(9x^(8/9))+x/(3x^(2/3))+x^(1/3) (giac keeps
// x^(4/9) as (x^(1/9))^4 and never mixes roots; its normal does, slowly). x = t^L, L the lcm of
// the roots' indices, makes every power an integer power of t: expand collects them, and t^k is
// x^(k/L) again. pow_den: L, or 0 if g has more than numbers and powers of x
static int pow_den(const giac::gen & g,const giac::gen & x){
  using namespace giac;
  if (g==x || g.type==_INT_ || g.type==_ZINT || g.type==_FRAC)
    return 1;
  if (g.type==_VECT){
    int L=1;
    for (const_iterateur it=g._VECTptr->begin();it!=g._VECTptr->end();++it){
      const int d=pow_den(*it,x);
      if (!d || (L=L/gcd(L,d)*d)>360)
        return 0;
    }
    return L;
  }
  if (g.type!=_SYMB)
    return 0;
  const unary_function_ptr & u=g._SYMBptr->sommet;
  const gen & f=g._SYMBptr->feuille;
  if (u==at_plus || u==at_prod || u==at_neg || u==at_inv)
    return pow_den(f,x);
  if (u==at_pow && f.type==_VECT && f._VECTptr->size()==2){
    const gen & b=f._VECTptr->front(),& e=f._VECTptr->back();
    if (e.type==_INT_)
      return pow_den(b,x);
    if (e.type==_FRAC && b==x && e._FRACptr->den.type==_INT_)
      return e._FRACptr->den.val;
  }
  return 0;
}
static giac::gen pow_to(const giac::gen & g,const giac::gen & x,const giac::gen & t,int L,bool back){
  using namespace giac;
  if (!back && g==x)
    return symbolic(at_pow,makesequence(t,L));
  if (back && g==t)
    return symbolic(at_pow,makesequence(x,fraction(1,L)));
  if (g.type==_VECT){
    vecteur w(*g._VECTptr);
    for (iterateur it=w.begin();it!=w.end();++it)
      *it=pow_to(*it,x,t,L,back);
    return gen(w,g.subtype);
  }
  if (g.type!=_SYMB)
    return g;
  const gen & f=g._SYMBptr->feuille;
  if (g._SYMBptr->sommet==at_pow && f.type==_VECT && f._VECTptr->size()==2 && f._VECTptr->front()==(back?t:x)){
    const gen e=back?f._VECTptr->back()/L:f._VECTptr->back()*L; // x^(p/q) <-> t^(pL/q)
    return symbolic(at_pow,makesequence(back?x:t,e));
  }
  return symbolic(g._SYMBptr->sommet,pow_to(f,x,t,L,back));
}
static giac::gen power_sums(const giac::gen & g){
  using namespace giac;
  const vecteur v=lidnt(g);
  if (v.size()!=1 || !contains(g,at_pow))
    return g;
  const int L=pow_den(g,v.front());
  if (L<2)
    return g;
  const gen t=gen("t__p",contextptr),e=expand(pow_to(g,v.front(),t,L,false),contextptr);
  if (is_undef(e) || e.type==_STRNG)
    return g;
  const gen r=eval(pow_to(e,v.front(),t,L,true),1,contextptr);
  return taille(r,400)<=taille(g,400)?r:g;
}

// the terms of the sums in g
static int plus_terms(const giac::gen & g){
  using namespace giac;
  int n=0;
  if (g.type==_VECT)
    for (const_iterateur it=g._VECTptr->begin();it!=g._VECTptr->end();++it)
      n+=plus_terms(*it);
  else if (g.type==_SYMB)
    n=(g._SYMBptr->sommet==at_plus && g._SYMBptr->feuille.type==_VECT?g._SYMBptr->feuille._VECTptr->size():0)+plus_terms(g._SYMBptr->feuille);
  return n;
}
// ln(4) as 2ln(2): ln of an integer that is a power (then 12ln(2)-6ln(4)+3 is 3)
static giac::gen ln_power(const giac::gen & g,const giac::gen & f,const giac::gen &){
  using namespace giac;
  if (g._SYMBptr->sommet==at_ln && f.type==_INT_)
    for (long b=2;b*b<=f.val;++b){ // the smallest base: 64 is 2^6
      long p=b;
      int k=1;
      for (;p<f.val;++k)
        p*=b;
      if (p==f.val)
        return k*symbolic(at_ln,int(b));
    }
  return symbolic(g._SYMBptr->sommet,f);
}

// The constant terms of an antiderivative go into + C: -sqrt(x^2+4)/(4x), not giac's
// -1/(x(sqrt(x^2+4)-x)), whose normal form is (-sqrt(x^2+4)-x)/(4x) = -sqrt(x^2+4)/(4x)-1/4;
// sqrt(x^2-1)/x, not (sqrt(x^2-1)+x)/x. The terms of g's numerator (or of its normal form's:
// a radical below a bar, rationalized) whose quotient by the denominator is constant are
// dropped; g is kept unless that makes it smaller.
static giac::gen drop_constants(const giac::gen & g,const giac::gen & x){
  using namespace giac;
  if (g.type!=_SYMB || is_undef(g) || contains(g,at_integrate) || taille(g,200)>=200)
    return g;
  gen best=g;
  int bt=taille(g,400);
  for (int k=0;k<2;++k){
    const gen h=k?safe_normal(g):g;
    if (is_undef(h) || h.type==_STRNG)
      continue;
    const gen n=_numer(h,contextptr),d=_denom(h,contextptr);
    if (!n.is_symb_of_sommet(at_plus) || n._SYMBptr->feuille.type!=_VECT || is_zero(d))
      continue;
    const vecteur & t=*n._SYMBptr->feuille._VECTptr;
    vecteur keep;
    for (unsigned i=0;i<t.size();++i)
      if (!is_constant_wrt(ratnormal(t[i]/d,contextptr),x,contextptr))
        keep.push_back(t[i]);
    if (keep.empty() || keep.size()==t.size())
      continue;
    const gen m=keep.size()==1?keep.front():symbolic(at_plus,gen(keep,_SEQ__VECT));
    gen r=is_one(d)?m:symb_prod(m,symb_inv(d));
    const gen rn=ratnormal(r,contextptr); // (4*ln(x)+1)/4 less 1/4: ln(x), not 4*ln(x)/4
    if (!is_undef(rn) && rn.type!=_STRNG && taille(rn,400)<taille(r,400))
      r=rn;
    const int rt=taille(r,400);
    if (rt<bt){
      best=r;
      bt=rt;
    }
  }
  return best;
}

// ---- do_run's steps ----
void answer_prepass(char * buf,int cap,bool focus){
  // paper notation and TI implicit multiplication (2sinx -> 2*sin(x), f(x)=... -> f(x):=...)
  // here and not in the console, so that the history keeps what the user typed
  khicas_implicit_mult(buf,cap);
  if (focus){ // TI and textbooks: log is base 10 (giac's log is ln): log(100) is 2
    ti_log10(buf,cap);
    sum_index_i(buf);
  }
}

// nPr(n,k), perm(n,k) (TI's MATH PRB nPr; this giac has none): comb(n,k)*k!
static giac::gen perms(const giac::gen & g,const giac::gen & f,const giac::gen &){
  using namespace giac;
  if (g._SYMBptr->sommet==at_of && f.type==_VECT && f._VECTptr->size()==2 && f._VECTptr->front().type==_IDNT){
    const char * n=f._VECTptr->front()._IDNTptr->id_name;
    const gen & b=f._VECTptr->back();
    if ((!strcmp(n,"perm") || !strcmp(n,"nPr")) && b.type==_VECT && b._VECTptr->size()==2)
      return symbolic(at_prod,makesequence(symbolic(at_comb,b),symbolic(at_factorial,b._VECTptr->back())));
  }
  return symbolic(g._SYMBptr->sommet,f);
}
void answer_before(giac::gen & g,answer_ctx & a,const char * buf,bool focus){
  using namespace giac;
  if (strstr(buf,"perm(") || strstr(buf,"nPr("))
    g=map_nodes(g,perms,0);
  a.typed_sum=strstr(buf,"sum(");
  a.typed_sec=strstr(buf,"sec(") || strstr(buf,"csc(") || strstr(buf,"cot(");
  a.typed_limit=strstr(buf,"limit(");
  a.real_in=focus && !has_i(g); // (-8)^(1/3) is -2
  if (a.real_in)
    g=real_roots(g,g.is_symb_of_sommet(at_plot) || g.is_symb_of_sommet(at_plotfunc));
  // x=a stores a in x (equaltosto), but x=(-x/2)^2 has x on both sides: an equation to solve
  const bool selfref=g.is_symb_of_sommet(at_equal) && g._SYMBptr->feuille.type==_VECT && g._SYMBptr->feuille._VECTptr->size()==2 && g._SYMBptr->feuille._VECTptr->front().type==_IDNT && !is_constant_wrt(g._SYMBptr->feuille._VECTptr->back(),g._SYMBptr->feuille._VECTptr->front(),contextptr);
  if (!selfref)
    g=equaltosto(g,contextptr);
  const gen unknown=equation_unknown(g);
  if (unknown.type==_IDNT)
    g=symbolic(at_solve,makesequence(g,unknown));
  a.var=unknown; // solve(eq,x) typed explicitly: its solutions are shown as x=... too
  a.vars=0;      // solve(eqs,[x,y]): x=2, y=1
  if (g.is_symb_of_sommet(at_solve) && g._SYMBptr->feuille.type==_VECT && g._SYMBptr->feuille._VECTptr->size()==2 && g._SYMBptr->feuille._VECTptr->back().type==_VECT)
    a.vars=g._SYMBptr->feuille._VECTptr->back();
  if (a.var.type!=_IDNT && (g.is_symb_of_sommet(at_solve) || g.is_symb_of_sommet(at_csolve) || g.is_symb_of_sommet(at_fsolve))){
    if (g._SYMBptr->feuille.type==_VECT && g._SYMBptr->feuille._VECTptr->size()==2 && g._SYMBptr->feuille._VECTptr->back().type==_IDNT)
      a.var=g._SYMBptr->feuille._VECTptr->back();
    else if (g._SYMBptr->feuille.type!=_VECT) // solve(expr): giac solves for x
      a.var=gen("x",contextptr);
  }
  // decimals: giac's solve fails (solve(x^2=0.5,x): "Bad Argument Value"): solved exactly, the
  // solutions in decimals
  if (g.is_symb_of_sommet(at_solve) && g._SYMBptr->feuille.type==_VECT && g._SYMBptr->feuille._VECTptr->size()==2 && has_num_coeff(g._SYMBptr->feuille._VECTptr->front()))
    g=symbolic(at_evalf,symbolic(at_solve,makesequence(exact(g._SYMBptr->feuille._VECTptr->front(),contextptr),g._SYMBptr->feuille._VECTptr->back())));
  // a definite integral (integrate(f,x,a,b)): undef means it diverges
  a.definite=(g.is_symb_of_sommet(at_integrate) || g.is_symb_of_sommet(at_int)) && g._SYMBptr->feuille.type==_VECT && g._SYMBptr->feuille._VECTptr->size()==4;
  a.ivar=0;
  if ((g.is_symb_of_sommet(at_integrate) || g.is_symb_of_sommet(at_int)) && g._SYMBptr->feuille.type==_VECT && g._SYMBptr->feuille._VECTptr->size()==2 && g._SYMBptr->feuille._VECTptr->back().type==_IDNT)
    a.ivar=g._SYMBptr->feuille._VECTptr->back();
  a.gin=g;
  g=ftc(g);
  g=numeric_hard_integrals(g);
  gen tab=table_integral(g); // sec, csc, sin(x)^3*cos(x)^2: the textbook form, at once
  if (is_zero(tab))
    tab=power_sum(g); // sum(1/sqrt(n),n,1,inf)
  gen parts=is_zero(tab)?parts_table(g):gen(0); // x*sec(x)^2 by parts
  if (is_zero(tab) && is_zero(parts) && a.definite)
    parts=table_definite(g); // sec(x)^3 from 0 to pi/4 (normalized as giac's: sqrt(2), not 2/sqrt(2))
  a.tabled=!is_zero(tab);
  const gen ga=add_autosimplify(g,contextptr);
  // unchanged for programs and explicit forms (factor, expand, diff...); derivatives are
  // simplified anyway: diff(sqrt(y/4),y) is 1/(4*sqrt(y)), not (sqrt(y/4))^-1/8
  a.autosimp=!(ga==g) || g.is_symb_of_sommet(at_diff);
  g=a.tabled?tab:is_zero(parts)?ga:parts; // (parts: evaluated and normalized as giac's answers)
}

void answer_after(giac::gen & g,const answer_ctx & a,std::string & msg,giac::gen & graw){
  using namespace giac;
  if (a.definite && a.real_in && has_i(g) && !has_inf_or_undef(g))
    g=real_ftc(a.gin,g); // x^(-1/3) from -1 to 8: 9/2
  if (contains(g,at_bounded_function)) // lim sin(x) at infinity, sum((-1)^n)
    msg=a.typed_sum?"diverges (the terms do not go to 0)":a.typed_limit?"no limit (it oscillates)":"";
  else if (g.is_symb_of_sommet(at_sum)) // an infinite sum giac could not do
    g=known_sum(g,msg);
  else if (is_undef(g) && a.gin.is_symb_of_sommet(at_sum) && a.gin._SYMBptr->feuille.type==_VECT && a.gin._SYMBptr->feuille._VECTptr->size()==4){
    vecteur w(*a.gin._SYMBptr->feuille._VECTptr); // sum(n*x^n,n,1,inf): giac's undef
    for (unsigned i=0;i<w.size();++i)
      if (i!=1)
        w[i]=eval(w[i],1,contextptr);
    const gen s=symbolic(at_sum,gen(w,_SEQ__VECT));
    const gen h=known_sum(s,msg);
    if (!(h==s))
      g=h;
  }
  else
    limit_dne(a.gin,g,msg); // lim 1/x at 0: does not exist (giac: an unsigned infinity)
  if (!msg.empty()) // "diverges": g is not shown (evalf below re-ran giac's sum of it: 40 s)
    return;
  // a finite sum: over more than 800 terms giac stops ("Invalid dimension", its list limit), and
  // sum(1/k^2,k,1,100) is a fraction of 80 digits: decimal values (the terms added in floats; ON
  // or CLEAR stops)
  if (a.gin.is_symb_of_sommet(at_sum) && a.gin._SYMBptr->feuille.type==_VECT && a.gin._SYMBptr->feuille._VECTptr->size()==4
      && (g.type==_STRNG || (g.type==_FRAC && g.print(contextptr).size()>40))){
    const vecteur & v=*a.gin._SYMBptr->feuille._VECTptr;
    const gen lo=eval(v[2],1,contextptr),hi=eval(v[3],1,contextptr),f=eval(v[0],1,contextptr);
    if (g.type==_FRAC)
      g=evalf(g,1,contextptr);
    else if (lo.type==_INT_ && hi.type==_INT_ && hi.val-lo.val<1000000 && v[1].type==_IDNT){
      double s=0;
      for (int k=lo.val;k<=hi.val;++k){
        control_c();
        const gen y=evalf(subst(f,v[1],k,false,contextptr),1,contextptr);
        if (interrupted || y.type!=_DOUBLE_ || y._DOUBLE_val!=y._DOUBLE_val) // (NaN: overflow)
          break;
        s+=y._DOUBLE_val;
        if (k==hi.val)
          g=gen(s);
      }
    }
  }
  if (g.type==_FRAC && is_positive(-g._FRACptr->den,contextptr)) // -3/-4 (telescoping sums)
    g=(-g._FRACptr->num)/(-g._FRACptr->den);
  focus_phase=83;
  trace_step("eval",g);
  if (contains(g,at_integrate) && !a.tabled){ // x*sec(x)^2: by parts
    g=integrate_by_parts(a.gin,g);
    trace_step("by parts",g);
  }
  if (a.autosimp && !a.tabled){
    g=auto_simplify(g);
    g=collect_rational(power_sums(g)); // -7x^(5/2)+4x^(1/3)/3+28/(9x^(5/9))
  }
  trace_step("auto_simplify",g);
  if (g.type==_SYMB && taille(g,200)<200){
    if (contains(g,gen("c_0",contextptr)) && !g.is_symb_of_sommet(at_plus)){ // desolve's constants
      const gen e=expand(g,contextptr);
      if (!is_undef(e) && e.type!=_STRNG)
        g=e;
    }
    if (a.gin.is_symb_of_sommet(at_sum) && lidnt(g).size()==1 && lvar(g).size()==1 && !has_inf_or_undef(g)){ // to n: factored
      const gen f=_factor(g,contextptr);
      if (!is_undef(f) && f.type!=_STRNG && taille(f,200)<=taille(g,200))
        g=f;
    }
    if (a.gin.is_symb_of_sommet(at_simplify) && contains(g,at_tan) && !contains(a.gin,at_tan)){
      const gen in=eval(a.gin._SYMBptr->feuille,1,contextptr); // sin(x)^4-cos(x)^4: -cos(2x)
      const gen sc=_tan2sincos(in,contextptr);
      for (int k=0;k<3;++k){
        const gen t=k==2?_tlin(in,contextptr):ratnormal(k?_trigcos(sc,contextptr):_trigsin(sc,contextptr),contextptr);
        if (!is_undef(t) && t.type!=_STRNG && (contains(g,at_tan) || taille(t,200)<taille(g,200)))
          g=t;
      }
    }
  }
  if (a.autosimp && !a.tabled && g.type==_SYMB && taille(g,200)<200){
    g=textbook_powers(normal_args(g));
    trace_step("normal_args",g);
  }
  if (a.ivar.type==_IDNT && !a.tabled){
    g=drop_constants(map_nodes(g,ln_conjugate,a.ivar),a.ivar);
    if (contains(g,at_exp) && top_sum(_numer(g,contextptr))){ // e^x(sin(x)+cos(x))/2, not
      const gen fg=form_op(FORM_FACT,g);                       // (e^x*cos(x)+e^x*sin(x))/2
      if (!is_undef(fg) && fg.type!=_STRNG && taille(fg,400)<taille(g,400) && !xcas::has_radical(fg))
        g=fg;
    }
    trace_step("drop_constants",g);
  }
  focus_phase=84;
  if (a.typed_sec) // sec(x)tan(x), not sin(x)/cos(x)^2, for the derivative of sec(x)
    g=sec_only(g);
  if (g.type==_SYMB && !contains(g,at_sum) && evalf(g,1,contextptr).type==_DOUBLE_){ // a number (not a sum giac
    // left: evalf would sum it), its terms collected:
    const gen m=map_nodes(g,ln_power,0),e=radicals(m)==1?normal(m,contextptr):ratnormal(m,contextptr); // 12ln(2)-6ln(4)+3: 3; subst's
    if (!is_undef(e) && e.type!=_STRNG && plus_terms(e)<plus_terms(g) && taille(e,400)<=taille(g,400))
      g=e; // (2523-4*29*sqrt(29)-5046+348*sqrt(29)+2339)/16 is (29sqrt(29)-23)/2
  }
  graw=g; // (the long-polynomial check of answer_print expands it: expand ignores a built division)
  if (g.type==_SYMB && taille(g,200)<200 && !contains(g,at_order_size)) // (series: as is)
    g=positive_first(g);
  trace_step("positive_first",g);
  focus_phase=85;
  if (a.var.type==_IDNT){
    if (g.type==_VECT && g._VECTptr->empty())
      msg="no solution";
    else
      g=solutions_as_equations(g,a.var);
  }
  else if (a.vars.type==_VECT){
    if (g.type==_VECT && g._VECTptr->empty())
      msg="no solution";
    else
      g=system_as_equations(g,a.vars);
  }
}

std::string answer_print(const giac::gen & g,const giac::gen & graw,const answer_ctx & a){
  using namespace giac;
  std::string printed=g.print(contextptr);
  plain_decimal(g,printed);
  if (a.var.type==_IDNT || a.vars.type==_VECT){ // solutions: x=-sqrt(2),x=sqrt(2), not
    strip_equation_parens(printed);              // x=(-sqrt(2)),x=(sqrt(2))
    chain_inequalities(printed);
  }
  if (printed.size()>70 && graw.type==_SYMB){ // (a long polynomial)/182: term by term
    const gen d=_denom(graw,contextptr);       // (x^14/14+...), which wraps
    if ((d.type==_INT_ || d.type==_ZINT) && !is_one(d) && _numer(graw,contextptr).is_symb_of_sommet(at_plus)){
      gen x=expand(graw,contextptr);
      if (!is_undef(x) && x.type!=_STRNG){
        if (x.type==_SYMB && taille(x,200)<200)
          x=positive_first(x); // descending powers, 99x^10/2
        printed=x.print(contextptr);
      }
    }
  }
  focus_phase=86;
  textbook(printed);
  textbook_constants(printed);
  return printed;
}
