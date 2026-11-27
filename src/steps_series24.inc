// steps_series24.inc - included by steps.cc in a build without the series pack (FLOWCE_SERIES): 2.4's
// steps for sums and Taylor polynomials, as 2.4 had them (its steps.cc, unchanged but for this namespace:
// the series pack's versions of these functions, much larger, are in steps.cc). one_ calls s24::ssteps and
// s24::tsteps.
namespace s24 {
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
} // namespace s24
