// analyze.cc - a function's analysis (analyze.h).
// The points where something can change are found exactly (giac's solve): the edges of the domain
// (zeros of denominators, of radicands, of logarithms' arguments), the zeros of f, f' and f''.
// Between two of them nothing changes, so one test point (a decimal value) gives the sign of the
// whole interval, as on a textbook's sign chart. Limits at the domain's edges and at infinity give
// the asymptotes. A function with sin, cos or tan is studied on a window: one period when it is
// periodic (the families of solutions, all_trig_solutions, listed there), else -2 pi to 2 pi. On a
// window, a factor mixing x with sin(x), or sin with cos, is solved numerically (numroots: giac took
// 14 to 100 s on the calculator), its zeros written k pi/12 when they are exactly.
#if defined FLOWCE_STEPS || defined HOSTGIAC
#include "analyze.h"
#include <string.h>
using namespace giac;
#define NOINL __attribute__((noinline))
#ifdef TICE
#define TOL 1e-4 // the calculator's doubles are 32-bit floats
#else
#define TOL 1e-8
#endif
#define EACH(it,v) for (const_iterateur it=(v).begin(),it##_=(v).end();it!=it##_;++it)

static const context * ctx_;
#define CTX ctx_
static gen * var_; // the variable (no global gen: the app has no static constructors)
#define X (*var_)
static std::vector<an_line> * out_;
static gen * per_;          // the period (0: not periodic)
static bool win_;           // studied on the window [lo_, hi_] only
static double lo_, hi_;
static gen * wlo_, * whi_;  // the window's ends, exact
static std::vector<an_pt> * kp_; // analyze_points: the points, no report
static gen * kper_;
static NOINL void kp(int kind,const gen & x,const gen & y){ if (kp_){ an_pt p; p.kind=(unsigned char)kind; p.x=x; p.y=y; kp_->push_back(p); } }

static NOINL bool stop(){ return ctrl_c || interrupted; }
#ifdef HOSTGIAC
double hg_now_ms(); // tools/hostgiac/driver.cc
#define now_ms hg_now_ms
#define TIMED(what,expr) ([&]{ double t0_=now_ms(); auto r_=(expr); if (getenv("ANDBG")) fprintf(stderr,"  %6.2f ms  %s %s\n",now_ms()-t0_,what,""); return r_; }())
#else
#define TIMED(what,expr) (expr)
#endif
static NOINL gen ev(const gen & g){ return eval(g,1,CTX); }
static NOINL gen at(const gen & g,const gen & v){ return ev(subst(g,X,v,false,CTX)); }
static NOINL double mag(double d){ return d<0?-d:d; }
// g's value as a real number (false: undefined, complex, infinite)
static NOINL bool real(const gen & g,double & d){
  gen z=evalf(g,1,CTX);
  if (z.type==_INT_){ d=z.val; return true; }
  if (z.type!=_DOUBLE_) return false;
  d=z._DOUBLE_val;
  return d-d==0;
}
static NOINL bool realat(const gen & g,double t,double & d){ return real(subst(g,X,gen(t),false,CTX),d); }
static bool near(double a,double b){ return mag(a-b)<=100*TOL*(1+mag(a)); }

// ---------------------------------------------------------------- points
struct pt { gen v; double d; };
typedef std::vector<pt> pts;
static NOINL void add(pts & v,const gen & p0,double d){
  gen p=p0;
  if (p.type==_DOUBLE_ && mag(d)<100*TOL){ p=0; d=0; } // (a numeric solution: 1e-26 for 0)
  unsigned i=0;
  while (i<v.size() && v[i].d<d && !near(v[i].d,d)) ++i;
  if ((i<v.size() && near(v[i].d,d)) || v.size()>=24) return;
  pt q; q.v=p; q.d=d;
  v.insert(v.begin()+i,q);
}
static bool inwin(double d){ return !win_ || (d>=lo_-TOL && d<=hi_+TOL); }
// x itself and a sin, cos or tan of x in u (x cos(x) - sin(x)): no exact solution, giac's
// approximations over many periods took 100 s on the calculator
static NOINL bool mixed(const gen & u){
  vecteur k=lvarx(u,X);
  bool bare=false,trig=false;
  EACH(it,k){
    if (*it==X || (it->is_symb_of_sommet(at_pow) && it->_SYMBptr->feuille[0]==X)) bare=true;
    else if (it->is_symb_of_sommet(at_sin) || it->is_symb_of_sommet(at_cos) || it->is_symb_of_sommet(at_tan)) trig=true;
  }
  return bare && trig;
}
static NOINL gen nice(const gen & g);
// the number of different sin, cos, tan of x in u (2 in cos(x) - sin(x): giac solves it in 14 s
// on the calculator, through tan(x/2))
static NOINL int kinds(const gen & u){
  vecteur k=lvarx(u,X);
  int n=0;
  EACH(it,k) n+=it->is_symb_of_sommet(at_sin) || it->is_symb_of_sommet(at_cos) || it->is_symb_of_sommet(at_tan);
  return n;
}
// r found as a zero of u: k pi/12 when it is one exactly (pi/4, not 0.785398), else the decimal
static NOINL void found(const gen & u,double r,pts & v){
  const double q=r*12/3.14159265358979;
  const int k=(int)(q<0?q-0.5:q+0.5);
  if (mag(q-k)<2e-3){
    gen e=ratnormal(gen(k)*cst_pi/12,CTX);
    if (is_zero(nice(at(u,e)))){ add(v,e,r); return; }
  }
  add(v,gen(r),r);
}
// its zeros on the window: on its 96 test points (every multiple of pi/48), where its sign changes
// between two (bisected), where it touches 0 (the smallest |u| near 0, by thirds)
static NOINL void numroots(const gen & u,pts & v){
  enum { N=96 };
  double y[N+1], sc=0;
  char ok[N+1];
  for (int i=0;i<=N && !stop();++i){
    ok[i]=realat(u,lo_+(hi_-lo_)*i/N,y[i]);
    if (ok[i] && mag(y[i])>sc) sc=mag(y[i]);
  }
  if (stop()) return;
  const double tiny=1e-4*sc;
#ifdef HOSTGIAC
  if (getenv("ANDBG")){ fprintf(stderr,"  numroots %s on [%g,%g] sc=%g:",u.print(CTX).c_str(),lo_,hi_,sc); for (int i=0;i<=N;i+=12) fprintf(stderr," %d:%g",ok[i],y[i]); fprintf(stderr,"\n"); }
#endif
  for (int i=0;i<=N;++i){
    const double xi=lo_+(hi_-lo_)*i/N;
    if (!ok[i]) continue;
    if (mag(y[i])<=tiny){ found(u,xi,v); continue; } // (on a test point: pi/4 is one)
    if (i && ok[i-1] && (y[i-1]<0)!=(y[i]<0) && mag(y[i-1])>tiny){ // a sign change: bisected
      double l=xi-(hi_-lo_)/N,h=xi,yl=y[i-1];
      for (int k=0;k<24;++k){
        const double m=(l+h)/2;
        double ym;
        if (!realat(u,m,ym)) break;
        if ((ym<0)==(yl<0)){ l=m; yl=ym; } else h=m;
      }
      double yr;
      const double r=(l+h)/2;
      if (realat(u,r,yr) && mag(yr)<1e3*TOL*(1+sc)) found(u,r,v); // (not a pole)
    }
    else if (i && i<N && ok[i-1] && ok[i+1] && mag(y[i])<mag(y[i-1]) && mag(y[i])<=mag(y[i+1]) && (y[i-1]<0)==(y[i+1]<0) && mag(y[i])<0.05*sc){
      double l=xi-(hi_-lo_)/N,h=xi+(hi_-lo_)/N; // touching 0? the smallest |u| there
      for (int k=0;k<24;++k){
        const double a=l+(h-l)/3,b=h-(h-l)/3;
        double ya,yb;
        if (!realat(u,a,ya) || !realat(u,b,yb)) break;
        if (mag(ya)<mag(yb)) h=b; else l=a;
      }
      double ym;
      if (realat(u,(l+h)/2,ym) && mag(ym)<=tiny) found(u,(l+h)/2,v);
    }
  }
}
// the real zeros of u (in the window), into v
static NOINL void solve0(const gen & u,pts & v);
static NOINL void zeros(const gen & u,pts & v){
  if (stop() || !depend(u,*X._IDNTptr)) return;
  // each factor of the numerator on its own: -cos(x)(2 sin(x)+1) solved in 4 ms on the PC, 20 ms
  // expanded; exactly, unless mixed on a window
  gen n=_numer(ratnormal(u,CTX),CTX), fa=_factor(n,CTX);
  if (fa.is_symb_of_sommet(at_neg)) fa=fa._SYMBptr->feuille;
  vecteur fs;
  if (fa.is_symb_of_sommet(at_prod) && fa._SYMBptr->feuille.type==_VECT) fs=*fa._SYMBptr->feuille._VECTptr;
  else fs.push_back(fa);
  EACH(it,fs){
    gen g=it->is_symb_of_sommet(at_pow)?it->_SYMBptr->feuille[0]:*it;
    if (g.is_symb_of_sommet(at_neg)) g=g._SYMBptr->feuille;
    if (!depend(g,*X._IDNTptr) || g.is_symb_of_sommet(at_exp)) continue; // (exp: never 0)
    if (win_ && (mixed(g) || kinds(g)>1)) numroots(g,v); else solve0(g,v);
  }
}
static NOINL void solve0(const gen & u,pts & v){
  gen s=TIMED("solve",_solve(makesequence(symbolic(at_equal,makesequence(u,0)),X),CTX));
#ifdef HOSTGIAC
  if (getenv("ANDBG")) fprintf(stderr,"     solve %s -> %s\n",u.print(CTX).c_str(),s.print(CTX).c_str());
#endif
  if (s.type!=_VECT) return;
  EACH(it,*s._VECTptr){
    double d;
    vecteur ids=lidnt(*it);
    gen n=0; // a family's integer (all trig solutions: n_0)
    EACH(jt,ids) if (evalf(*jt,1,CTX).type!=_DOUBLE_) n=*jt;
    if (is_zero(n)){ if (real(*it,d) && inwin(d)) add(v,*it,d); continue; }
    double d0,d1;
    if (!win_ || !real(subst(*it,n,0,false,CTX),d0) || !real(subst(*it,n,1,false,CTX),d1) || near(d0,d1)) continue;
    const double st=d1-d0;
    int k0=(int)((lo_-d0)/st)-1,k1=(int)((hi_-d0)/st)+1;
    if (k0>k1){ int t=k0; k0=k1; k1=t; }
    for (int k=k0;k<=k1 && k-k0<40;++k){
      gen r=ratnormal(subst(*it,n,k,false,CTX),CTX); // 3*pi/2, not (2*pi+pi)/2
      if (real(r,d) && inwin(d)) add(v,r,d);
    }
  }
}
// the expressions whose zeros can be edges of g's domain: denominators, the radicands of even
// roots, the arguments of logarithms, of asin and acos (minus and plus 1), of tan (its cos)
static NOINL void guards(const gen & g,vecteur & u){
  if (g.type==_VECT){ EACH(it,*g._VECTptr) guards(*it,u); return; }
  if (g.type!=_SYMB || !depend(g,*X._IDNTptr)) return;
  const gen & a=g._SYMBptr->feuille;
  guards(a,u);
  const unary_function_ptr & s=g._SYMBptr->sommet;
  if (s==at_inv || s==at_ln || s==at_log10 || s==at_sqrt) u.push_back(a);
  else if (s==at_pow && a.type==_VECT && a._VECTptr->size()==2){
    const gen & b=a[0], & e=a[1];
    if (depend(e,*X._IDNTptr) || is_strictly_positive(-e,CTX) || (e.type==_FRAC && e._FRACptr->den.type==_INT_ && !(e._FRACptr->den.val&1))) u.push_back(b);
  }
  else if (s==at_asin || s==at_acos){ u.push_back(a-1); u.push_back(a+1); }
  else if (s==at_tan) u.push_back(symbolic(at_cos,a));
}
static NOINL void edges(const gen & g,pts & v){
  vecteur u;
  guards(g,u);
  EACH(it,u) zeros(*it,v);
}

// ---------------------------------------------------------------- the line cut at points
// Interval j of v: between v[j-1] and v[j] (j = 0: from the left end, v.size(): to the right end)
static NOINL double test(const pts & v,int j){
  const int n=v.size();
  if (win_){
    const double a=j?v[j-1].d:lo_,b=j<n?v[j].d:hi_;
    return a+(b-a)*0.47;
  }
  if (!n) return 0.37;
  if (!j) return v[0].d-1-mag(v[0].d)/2;
  if (j==n) return v[n-1].d+1+mag(v[n-1].d)/2;
  return v[j-1].d+(v[j].d-v[j-1].d)*0.47;
}
static bool empty_iv(const pts & v,int j){ // a window's end on a point: nothing between
  const int n=v.size();
  return win_ && ((!j && n && v[0].d<=lo_+TOL) || (j==n && n && v[n-1].d>=hi_-TOL));
}
// the sign of g on each interval (2: f undefined there); defined at the points (dp)
static gen * f_;
static NOINL void chart(const gen & g,const pts & v,std::vector<signed char> & s,std::vector<char> * dp=0){
  s.clear();
  for (int j=0;j<=(int)v.size() && !stop();++j){
    double t=test(v,j),y;
    signed char c=2;
    if (!empty_iv(v,j) && realat(*f_,t,y)) c=realat(g,t,y)?(y>0?1:y<0?-1:0):2;
    s.push_back(c);
  }
  if (dp){ dp->clear(); for (unsigned i=0;i<v.size();++i){ double y; dp->push_back(real(at(*f_,v[i].v),y)); } }
}

// the sign just left (side -1) or right (1) of point i: a period's window wraps around; 2: none
static NOINL signed char beside(const pts & v,const std::vector<signed char> & s,int i,int side){
  const int k=v.size();
  int j=side<0?i:i+1;
  if (!empty_iv(v,j)) return s[j];
  if (!per_) return 2;
  j=side<0?k:0;
  if (empty_iv(v,j)) j+=side<0?-1:1;
  return j>=0 && j<=k?s[j]:2;
}

// d is inside the domain (defined on both sides): B, sd its edges and chart
static NOINL bool interior(const pts & B,const std::vector<signed char> & sd,double d){
  unsigned j=0;
  while (j<B.size() && B[j].d<d && !near(B[j].d,d)) ++j;
  if (j<B.size() && near(B[j].d,d)) return beside(B,sd,j,-1)!=2 && beside(B,sd,j,1)!=2;
  return sd[j]!=2;
}

// ---------------------------------------------------------------- the report
static NOINL void line(int kind,const char * text,const std::string & m,int sep=AN_COMMA){
  an_line l; l.kind=kind; l.sep=sep; l.text=text; l.math=m;
  out_->push_back(l);
}
static NOINL void item(std::string & m,const std::string & s){ if (!m.empty()) m+='\n'; m+=s; }
static NOINL std::string P(const gen & g){ return g.print(CTX); }
// odd roots as real roots (the textbooks' cube root of -8 is -2; giac's x^(1/3) is complex there)
static NOINL gen realroots(const gen & g){
  if (g.type==_VECT){ vecteur v=*g._VECTptr; for (unsigned i=0;i<v.size();++i) v[i]=realroots(v[i]); return gen(v,g.subtype); }
  if (g.type!=_SYMB) return g;
  gen a=realroots(g._SYMBptr->feuille);
  if (g.is_symb_of_sommet(at_pow) && a.type==_VECT && a._VECTptr->size()==2 && a[1].type==_FRAC){
    const gen & e=a[1], & q=e._FRACptr->den;
    if (q.type==_INT_ && (q.val&1) && q.val>1 && depend(a[0],*X._IDNTptr)){
      gen r=symbolic(at_surd,makesequence(a[0],q));
      return is_one(e._FRACptr->num)?r:symbolic(at_pow,makesequence(r,e._FRACptr->num));
    }
  }
  return symbolic(g._SYMBptr->sommet,a);
}
// exact when short: (normal over radicals: 3 sqrt(3), not sqrt(3)^3), else a decimal
static NOINL gen nice(const gen & g){
  gen r=TIMED("ratnormal",ratnormal(g,CTX));
  if (taille(r,80)<60){ gen n=TIMED("recursive_normal",recursive_normal(r,CTX)); if (taille(n,80)<=taille(r,80)) r=n; } // 3/4, not 9/(3+9); exp(-1/2)
  if (taille(r,80)>60) r=evalf(r,1,CTX);
  return r;
}
static NOINL std::string point(const gen & a,const gen & b){
  std::string s="(";
  s+=P(a); s+=","; s+=P(nice(b)); s+=")";
  return s;
}
// the points a periodic function repeats: a + k*period
static NOINL std::string family(const char * op,const gen & a){
  std::string s=P(X); s+=op;
  if (!is_zero(a)){ s+=P(a); s+="+"; }
  s+="k*"; s+=P(*per_);
  return s;
}
// the intervals where the chart s has sign c, merged across the points in the domain (dp): as
// inequalities ("x<-1", "-1<x<1"), with the ends in the domain (closed) for the domain itself
static NOINL std::string runs(const pts & v,const std::vector<signed char> & s,const std::vector<char> & dp,int c,bool closed,int & n){
  std::string m;
  n=0;
  const int k=v.size();
  for (int j=0;j<=k;){
    if (s[j]!=c){ ++j; continue; }
    int e=j;
    while (e<k && dp[e] && (s[e+1]==c || empty_iv(v,e+1))) ++e;
    std::string t;
    const bool left=j || win_, right=e<k || win_;
    const gen & a=j?v[j-1].v:*wlo_, & b=e<k?v[e].v:*whi_;
    const bool ca=closed && j && dp[j-1], cb=closed && e<k && dp[e];
    if (!left && !right) t="\1all real numbers";
    else if (!left){ t=P(X); t+=cb?"<=":"<"; t+=P(b); }
    else if (!right){ t=P(X); t+=ca?">=":">"; t+=P(a); }
    else { t=P(a); t+=ca?"<=":"<"; t+=P(X); t+=cb?"<=":"<"; t+=P(b); }
    item(m,t); ++n;
    j=e+1;
  }
  return m;
}

// the period of f (0: none), checked at test points; trig: f has sin, cos or tan of x
static NOINL gen period(const gen & f,bool & trig){
  vecteur ts=lop(f,at_sin), c=lop(f,at_cos), t=lop(f,at_tan);
  ts.insert(ts.end(),c.begin(),c.end());
  const unsigned nst=ts.size();
  ts.insert(ts.end(),t.begin(),t.end());
  trig=false;
  gen num=0,den=1; // the period over pi: the lcm of the arguments' periods
  for (unsigned i=0;i<ts.size();++i){
    const gen & a=ts[i]._SYMBptr->feuille;
    if (!depend(a,*X._IDNTptr)) continue;
    trig=true;
    gen k=ev(derive(a,X,CTX));
    if ((k.type!=_INT_ && k.type!=_FRAC) || is_zero(k) || depend(ev(a-k*X),*X._IDNTptr)) return 0;
    gen r=ev(gen(i<nst?2:1)/abs(k,CTX)); // over pi
    gen rn=r.type==_FRAC?r._FRACptr->num:r, rd=r.type==_FRAC?r._FRACptr->den:gen(1);
    num=is_zero(num)?rn:lcm(num,rn); den=gcd(den,rd);
  }
  if (!trig) return 0;
  gen p=ev(num*cst_pi/den);
  for (int h=0;h<3;++h){ // the smallest: sin(x)^2 repeats every pi
    gen q=h?ratnormal(p/2,CTX):p;
    static const double ts_[]={0.37,1.29,-2.13,2.9};
    bool same=true;
    for (int i=0;i<4 && same;++i){
      double a,b,x0=ts_[i],xp;
      if (!real(q,xp)) return 0;
      if (realat(f,x0,a) && realat(f,x0+xp,b)) same=mag(a-b)<=100*TOL*(1+mag(a));
    }
    if (!same){ if (!h) return 0; break; }
    p=q;
  }
  return p;
}

const char * analyze_of(const gen & f0,std::vector<an_line> & out,GIAC_CONTEXT){
  ctx_=contextptr; out_=&out;
  gen f=f0;
  if (f.is_symb_of_sommet(at_equal)) f=f._SYMBptr->feuille[1]; // y = ..., f(x) = ...
  f=ev(f);
  const gen fe=f; // (as shown: x^3-3*x, not (x^3)-(3*x) as typed)
  if (f.type==_VECT || f.type==_STRNG || is_undef(f)) return "Type a function of x";
  vecteur ids=lidnt(f);
  gen x=0;
  EACH(it,ids) if (evalf(*it,1,CTX).type!=_DOUBLE_){ if (!is_zero(x) && *it!=x) return "Only one letter, please"; x=*it; }
  if (is_zero(x)) return "Type a function of x";
  var_=&x; f_=&f;
  f=realroots(f);
  // the window
  bool trig;
  gen per=period(f,trig), wlo=0, whi=0;
  per_=is_zero(per)?0:&per;
  if (kper_) *kper_=per;
  win_=trig;
  if (trig){
    if (per_){ wlo=0; whi=per; } else { wlo=ev(-2*cst_pi); whi=ev(2*cst_pi); }
    real(wlo,lo_); real(whi,hi_);
  }
  wlo_=&wlo; whi_=&whi;
  const bool ats=all_trig_sol(CTX),ws=withsqrt(CTX);
  stdostream * log=logptr(CTX);
  logptr(0,CTX); // (solve's notes, "Unable to isolate x...", went to the history)
  all_trig_sol(win_,CTX);
  withsqrt(false,CTX); // factor: x^2-4x+1, not (x+sqrt(3)-2)(x-sqrt(3)-2)
  std::string m;
  gen d1;
  { std::string h="f("; h+=P(x); h+=")="; h+=P(fe); line(AN_HEAD,0,h); }
  if (win_){
    if (per_) line(AN_ROW,"Periodic, period",P(per));
    std::string w=P(wlo); w+="<="; w+=P(x); w+="<="; w+=P(whi);
    line(AN_ROW,"Shown for",w);
  }
  // domain
  pts B;
  edges(f,B);
  std::vector<signed char> sd;
  std::vector<char> dp;
  chart(f,B,sd,&dp);
  int n=0;
  for (unsigned j=0;j<sd.size();++j) if (sd[j]!=2) ++n;
  if (stop()) goto done;
  if (!n){ all_trig_sol(ats,CTX); withsqrt(ws,CTX); logptr(log,CTX); return "No real values"; }
  line(AN_SECTION,"DOMAIN",m);
  {
    bool holes=true,all=true; // only points missing: x != a, b; none
    for (unsigned j=0;j<sd.size();++j) if (sd[j]==2 && !empty_iv(B,j)) holes=all=false;
    for (unsigned i=0;i<B.size();++i){ if (dp[i]) holes=false; else all=false; }
    if (all) m="\1all real numbers";
    else if (holes && B.size() && per_){ for (unsigned i=0;i<B.size();++i) if (B[i].d<hi_-TOL) item(m,family("!=",B[i].v)); }
    else if (holes && B.size() && !win_){ for (unsigned i=0;i<B.size();++i){ std::string t=P(x); t+="!="; t+=P(B[i].v); item(m,t); } }
    else {
      // every sign but 2 counts: the domain
      std::vector<signed char> in(sd);
      for (unsigned j=0;j<in.size();++j) in[j]=in[j]!=2;
      m=runs(B,in,dp,1,true,n);
    }
    line(AN_ROW,"",m,holes?AN_COMMA:AN_OR); m="";
  }
  // intercepts
  line(AN_SECTION,"INTERCEPTS",m);
  {
    double y;
    if (inwin(0) && real(at(f,0),y)){ line(AN_ROW,"y-intercept",point(0,at(f,0))); kp(1,0,at(f,0)); }
    else line(AN_ROW,"y-intercept",std::string("\1none"));
    pts Z;
    zeros(f,Z);
    for (unsigned i=0;i<Z.size();++i) if (real(at(f,Z[i].v),y) && mag(y)<=1000*TOL){ item(m,point(Z[i].v,0)); kp(0,Z[i].v,0); }
    line(AN_ROW,"x-intercepts",m.empty()?std::string("\1none"):m); m="";
  }
  // symmetry
  if (!kp_){
    static const double ts[]={0.37,1.29,2.13,0.61,3.7};
    int ev_=1,od=1,both=0;
    for (int i=0;i<5;++i){
      double a,b;
      bool da=realat(f,ts[i],a),db=realat(f,-ts[i],b);
      if (da!=db){ ev_=od=0; both=1; break; }
      if (!da) continue;
      ++both;
      if (mag(a-b)>100*TOL*(1+mag(a))) ev_=0;
      if (mag(a+b)>100*TOL*(1+mag(a))) od=0;
    }
    if (both){
      line(AN_SECTION,"SYMMETRY",m);
      line(AN_ROW,ev_?"Even: symmetric about the y-axis":od?"Odd: symmetric about the origin":"Neither even nor odd",m);
    }
  }
  if (stop()) goto done;
  // asymptotes and holes
  if (!kp_){
    line(AN_SECTION,"ASYMPTOTES",m);
    std::string va,ho;
    int k=0;
    for (unsigned i=0;i<B.size() && !stop();++i){
      if (dp[i]) continue;
      const bool l=sd[i]!=2 && !empty_iv(B,i), r=sd[i+1]!=2 && !empty_iv(B,i+1);
      if (!l && !r) continue;
      gen L1=l?limit(f,*x._IDNTptr,B[i].v,-1,CTX):undef, L2=r?limit(f,*x._IDNTptr,B[i].v,1,CTX):undef;
      double a,b;
      if (is_inf(L1) || is_inf(L2)){
        if (per_ && B[i].d>=hi_-TOL) continue;
        std::string t;
        if (per_) t=family("=",B[i].v); else { t=P(x); t+="="; t+=P(B[i].v); }
        item(va,t);
      }
      else if (l && r && real(L1,a) && real(L2,b) && near(a,b)) item(ho,point(B[i].v,L1));
    }
    if (!va.empty()){ line(AN_ROW,"Vertical",va); ++k; }
    if (!win_){ // at infinity: y = L, or y = m x + c
      std::string side[2];
      const char * kind[2]={0,0};
      for (int s=0;s<2 && !stop();++s){
        if (sd[s?sd.size()-1:0]==2) continue;
        const gen inf=s?plus_inf:minus_inf;
        gen L=limit(f,*x._IDNTptr,inf,0,CTX);
        double d;
        if (real(L,d)){ side[s]="y="; side[s]+=P(nice(L)); kind[s]="Horizontal"; continue; }
        if (!is_inf(L)) continue;
        gen M=limit(ev(f/x),*x._IDNTptr,inf,0,CTX);
        if (!real(M,d) || near(d,0)) continue;
        gen C=limit(ev(f-M*x),*x._IDNTptr,inf,0,CTX);
        if (!real(C,d)) continue;
        { // (f is the line itself out there, a line or abs(x): no asymptote)
          gen r=ev(f-M*x-C);
          double a,b;
          if (realat(r,s?37.3:-37.3,a) && realat(r,s?91.7:-91.7,b) && mag(a)<100*TOL && mag(b)<100*TOL) continue;
        }
        side[s]="y="; side[s]+=P(nice(ev(M*x+C))); kind[s]="Slant";
      }
      if (kind[0] && kind[1] && side[0]==side[1]){ line(AN_ROW,kind[0],side[0]); ++k; }
      else for (int s=0;s<2;++s) if (kind[s]){ // the side: as x -> oo, as x -> -oo
        std::string t=side[s];
        t+="\n\1as "; t+=P(x); t+=s?" \xe2\x86\x92 \xe2\x88\x9e":" \xe2\x86\x92 \xe2\x88\x92\xe2\x88\x9e";
        line(AN_ROW,kind[s],t,AN_SPACE); ++k;
      }
    }
    if (!ho.empty()){ line(AN_ROW,"Hole",ho); ++k; }
    if (!k) line(AN_ROW,"None",m);
  }
  if (stop()) goto done;
  // the first and the second derivative: their sign charts
  for (int o=1;o<=2 && !stop();++o){
    gen d=ev(derive(o==1?f:d1,x,CTX)), dn=nice(d);
    if (o==1) d1=d;
    if (taille(dn,100)<100){ gen fd=TIMED("factor",_factor(dn,CTX)); if (taille(fd,200)<=2*taille(dn,200)+4) dn=fd; } // (factored: its sign is read off)
    line(AN_SECTION,o==1?"INCREASING / DECREASING":"CONCAVITY",m);
    { std::string t=o==1?"f'(":"f''("; t+=P(x); t+=")="; t+=P(dn); line(AN_ROW,"",t); }
    if (is_zero(dn)){
      line(AN_ROW,o==1?"Constant: no extrema":"No concavity",m); break;
    }
    pts C, Q=B;
    zeros(d,C);
    edges(d,C);
    for (unsigned i=0;i<C.size();){ // in the domain
      double y;
      if (real(at(f,C[i].v),y) && interior(B,sd,C[i].d)) ++i;
      else C.erase(C.begin()+i);
    }
    for (unsigned i=0;i<C.size();++i) add(Q,C[i].v,C[i].d);
    if (o==1){
      for (unsigned i=0;i<C.size();++i){ std::string t=P(x); t+="="; t+=P(C[i].v); item(m,t); }
      line(AN_ROW,"Critical numbers",m.empty()?std::string("\1none"):m); m="";
    }
    std::vector<signed char> s;
    std::vector<char> qp;
    chart(d,Q,s,&qp);
    if (stop()) break;
    std::vector<char> qm(qp); // the intervals merged across these points (concavity: not across a cusp)
    if (o==2) for (unsigned i=0;i<Q.size();++i){ double y; if (!real(at(d1,Q[i].v),y)) qm[i]=0; }
    for (int c=1;c>=-1;c-=2){
      m=runs(Q,s,qm,c,false,n);
      line(AN_ROW,o==1?(c>0?"Increasing":"Decreasing"):(c>0?"Concave up":"Concave down"),n?m:std::string("\1nowhere"),AN_OR);
    }
    m="";
    // where the sign changes (a period's window: its end is its start)
    std::string mx,mn;
    for (int i=0;i<(int)Q.size();++i){
      if (!qp[i] || (per_ && Q[i].d>=hi_-TOL)) continue;
      const signed char a=beside(Q,s,i,-1),b=beside(Q,s,i,1);
      if (a==2 || b==2 || a==b || !a || !b) continue;
      std::string t=point(Q[i].v,at(f,Q[i].v));
      kp(o==2?4:a>0?2:3,Q[i].v,at(f,Q[i].v));
      if (o==2) item(m,t);
      else item(a>0?mx:mn,t);
    }
    if (o==1){
      line(AN_SECTION,"EXTREMA",std::string());
      if (!mx.empty()) line(AN_ROW,"Local max",mx);
      if (!mn.empty()) line(AN_ROW,"Local min",mn);
      if (mx.empty() && mn.empty()) line(AN_ROW,"None",std::string());
    }
    else line(AN_ROW,"Inflection points",m.empty()?std::string("\1none"):m);
    m="";
  }
 done:
  all_trig_sol(ats,CTX);
  withsqrt(ws,CTX);
  logptr(log,CTX);
  return stop()?"Stopped":0;
}
void analyze_zeros(const gen & g,const gen & x,double lo,double hi,bool edg,vecteur & out,GIAC_CONTEXT){
  ctx_=contextptr;
  gen xx=x;
  var_=&xx; per_=0;
  win_=lo<hi; lo_=lo; hi_=hi;
  const bool ats=all_trig_sol(CTX),ws=withsqrt(CTX);
  stdostream * log=logptr(CTX);
  logptr(0,CTX);
  all_trig_sol(win_,CTX);
  withsqrt(false,CTX);
  pts v;
  zeros(g,v);
  if (edg) edges(g,v);
  all_trig_sol(ats,CTX); withsqrt(ws,CTX); logptr(log,CTX);
  for (unsigned i=0;i<v.size();++i) out.push_back(v[i].v);
}
const char * analyze_points(const gen & f,std::vector<an_pt> & pts,gen & per,GIAC_CONTEXT){
  std::vector<an_line> out;
  kp_=&pts; kper_=&per; per=0;
  const char * r=analyze_of(f,out,contextptr);
  kp_=0; kper_=0;
  for (unsigned i=0;i<pts.size();++i) pts[i].y=nice(pts[i].y); // (3/4, as the report)
  return r;
}
#endif
