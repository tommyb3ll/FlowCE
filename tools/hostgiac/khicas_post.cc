// The giac part of KhiCAS's do_run (src/main.cc, 2026-10-06) for the host driver's -k option:
// input pre-pass, parse, equaltosto, equation -> solve, numeric_hard_integrals, add_autosimplify,
// eval, auto_simplify (ratnormal/simplify, kdisplay.cc's merge_sqrt), solutions as equations, the
// decimal value (Focus), check_do_graph (graph view, drawn off screen), print.
// The static helpers below are copies of main.cc's (main.cc itself needs the calculator: inline
// eZ80 assembly, TI-OS calls); keep them in sync when main.cc changes.
#include "giacPCH.h"
#include "kdisplay.h"
#include "focus.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

using namespace giac;
extern giac::context * contextptr;
extern "C" bool khicas_implicit_mult(char * line,int maxlen); // tiinput_glue.cc

// ---- copies of src/main.cc helpers ----
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
  if (u==at_plus || u==at_inv)
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
  const bool trig=taille(g,16)<16 && (has_op(g,*at_sin) || has_op(g,*at_cos)) && !xcas::has_radical(g);
  gen s=(*(trig?at_simplify:at_ratnormal))(g,contextptr);
  if (!trig && g.type==_SYMB){ // factored denominator: 1/(x+1)^2, not 1/(x^2+2*x+1)
    gen d=_denom(s,contextptr);
    if (d.type==_SYMB){
      gen n=_numer(s,contextptr),f=_factor(d,contextptr);
      if (!is_undef(f) && f.type!=_STRNG)
        s=is_one(n)?symb_inv(f):symb_prod(n,symb_inv(f));
    }
  }
  if (is_undef(s) || s.type==_STRNG || taille(s,1000)>=taille(g,1000)) // only if simpler: pi*(x+1) stays
    s=g;
  if (!trig && xcas::has_radical(s)){ // after a u-substitution: (p/u)*u^(3/2), not p*sqrt(u)
    const gen m=xcas::merge_sqrt(s,contextptr);
    if (!(m==s))
      return m;
  }
  return s;
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
    const gen d3=derive(derive(derive(base,x,contextptr),x,contextptr),x,contextptr);
    if (!is_zero(ratnormal(d3,contextptr)))
      return true;
  }
  if (a.type==_VECT){
    for (const_iterateur it=a._VECTptr->begin();it!=a._VECTptr->end();++it)
      if (hard_radicand(*it,x))
        return true;
    return false;
  }
  return hard_radicand(a,x);
}
static giac::gen numeric_hard_integrals(const giac::gen & g){
  using namespace giac;
  if (g.type!=_SYMB)
    return g;
  const gen & a=g._SYMBptr->feuille;
  if ((g._SYMBptr->sommet==at_integrate || g._SYMBptr->sommet==at_int) && a.type==_VECT && a._VECTptr->size()==4){
    const vecteur & v=*a._VECTptr;
    if (v[1].type==_IDNT && lidnt(v[2]).empty() && lidnt(v[3]).empty() && hard_radicand(v[0],v[1]))
      return symbolic(g._SYMBptr->sommet,makesequence(v[0],v[1],evalf(v[2],1,contextptr),evalf(v[3],1,contextptr)));
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

static std::string * approx_text;
static void result_approx(const giac::gen & g){
  using namespace giac;
  if (!approx_text)
    approx_text=new std::string;
  approx_text->clear();
  if (g.type==_INT_ || g.type==_ZINT || g.type==_DOUBLE_ || g.type==_FLOAT_ || g.type==_STRNG || g.type==_VECT
      || taille(g,64)>=64)
    return;
  const gen a=evalf(g,1,contextptr);
  if (a.type!=_DOUBLE_ && a.type!=_CPLX)
    return;
  const int dd=decimal_digits(contextptr);
  decimal_digits(6,contextptr);
  *approx_text=a.print(contextptr);
  decimal_digits(dd,contextptr);
}

static giac::gen solutions_as_equations(const giac::gen & s,const giac::gen & x){
  using namespace giac;
  if (s.type!=_VECT || s._VECTptr->empty())
    return s;
  vecteur w;
  for (const_iterateur it=s._VECTptr->begin();it!=s._VECTptr->end();++it)
    w.push_back(symb_equal(x,*it));
  return w.size()==1?w.front():gen(w,_SEQ__VECT);
}

// ---- do_run, the giac path ----
std::string khicas_do_run(const char * s){
  const int S=strlen(s),cap=2*S+32>256?2*S+32:256;
  char * buf=(char *)malloc(cap);
  strcpy(buf,s);
  khicas_implicit_mult(buf,cap);
  ctrl_c=kbd_interrupted=interrupted=false;
  std::string printed;
  {
    gen g(buf,contextptr);
    const bool selfref=g.is_symb_of_sommet(at_equal) && g._SYMBptr->feuille.type==_VECT && g._SYMBptr->feuille._VECTptr->size()==2 && g._SYMBptr->feuille._VECTptr->front().type==_IDNT && !is_constant_wrt(g._SYMBptr->feuille._VECTptr->back(),g._SYMBptr->feuille._VECTptr->front(),contextptr);
    if (!selfref)
      g=equaltosto(g,contextptr);
    const gen unknown=equation_unknown(g);
    if (unknown.type==_IDNT)
      g=symbolic(at_solve,makesequence(g,unknown));
    gen var=unknown;
    if (var.type!=_IDNT && g.is_symb_of_sommet(at_solve)){
      if (g._SYMBptr->feuille.type==_VECT && g._SYMBptr->feuille._VECTptr->size()==2 && g._SYMBptr->feuille._VECTptr->back().type==_IDNT)
        var=g._SYMBptr->feuille._VECTptr->back();
      else if (g._SYMBptr->feuille.type!=_VECT)
        var=gen("x",contextptr);
    }
    g=numeric_hard_integrals(g);
    const gen ga=add_autosimplify(g,contextptr);
    const bool autosimp=!(ga==g) || g.is_symb_of_sommet(at_diff);
    g=ga;
    g=eval(g,eval_level(contextptr),contextptr); // do_eval
    const bool stopped=interrupted;
    ctrl_c=kbd_interrupted=interrupted=false;
    std::string msg;
    if (stopped){
      msg="Interrupted";
      g=0;
    }
    else {
      if (autosimp)
        g=auto_simplify(g);
      if (var.type==_IDNT){
        if (g.type==_VECT && g._VECTptr->empty())
          msg="no solution";
        else
          g=solutions_as_equations(g,var);
      }
      result_approx(g); // focus_on
    }
    if (!msg.empty())
      printed=msg;
    else {
#ifdef WITH_EQW
      gen gs;
      xcas::check_do_graph(g,gs,6,contextptr); // Focus: graph view for graphic results
#endif
#ifdef WITH_PLOT
      if (xcas::ispnt(g))
        printed="Graphic object";
      else
#endif
      if (taille(g,256)>=256)
        printed="Done";
      else {
        printed=g.print(contextptr);
        if (printed.size()>70){
          const gen d=_denom(g,contextptr);
          if ((d.type==_INT_ || d.type==_ZINT) && !is_one(d) && _numer(g,contextptr).is_symb_of_sommet(at_plus)){
            const gen x=expand(g,contextptr);
            if (!is_undef(x) && x.type!=_STRNG)
              printed=x.print(contextptr);
          }
        }
      }
    }
    if (approx_text && !approx_text->empty())
      { printed += "  ~ "; printed += *approx_text; }
  }
  free(buf);
  ctrl_c=kbd_interrupted=interrupted=false;
  return printed;
}
