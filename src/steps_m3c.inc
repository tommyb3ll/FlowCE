// steps_m3c.inc - included by steps.cc after steps_m3.inc (FLOWCE_VECTORS, HOSTGIAC): lines and curves in
// space solved as the course's keys solve them. The forms' calls (focus_menu.cc): lines(l1,l2),
// tanline(r,P or t0), arclen(r,A,B) (points or times), arcparam(r), posvel(r',r0[,t0]),
// meets(r,surface), oncurve(r,P), line2p(P,Q). Each line from a pattern (CL): its text built in one
// place (strings concatenated at each line cost 45 KB in build 1); no blanks in it (giac quotes a
// name with one). cm_: the message instead of an answer (skew, parallel, not on the curve).

// a line from a pattern: %k value k in the module's print, #k in parentheses unless atomic
static NOINL void CL(int r,const char * f,const gen & a,const gen & b=zero,const gen & c=zero,const gen & d=zero){
  const gen * v[4]={&a,&b,&c,&d};
  std::string s;
  for (;*f;++f){
    if ((*f=='%' || *f=='#') && f[1]>='0' && f[1]<='3'){ const gen & x=*v[f[1]-'0']; s+=*f=='%'?vprint(x):vatom(x,1); ++f; }
    else s+=*f;
  }
  VL(r,s);
}
static NOINL gen cpar(const gen & r){ // r's parameter: its one free name that is no number (t)
  const vecteur ids=lidnt(r);
  EACH(it,ids) if (evalf(*it,1,CTX).type!=_DOUBLE_) return *it;
  return gen(identificateur("t"));
}
static NOINL gen ctime0(const gen & g){ return is(g,at_equal)?ev(arg(g,1)):ev(g); } // t=pi or pi
static NOINL gen cpr(const gen & g){ return txt(vatom(g,3)); } // put in: (-2)^2-(-2), 2*3, cos(0)
static NOINL gen cput(const gen & r,const gen & t,const gen & t0){
  const std::string s=vprint(subst(r,t,gen(identificateur("tZ_")),false,CTX)),v=vatom(t0,1),v0=vprint(t0);
  std::string o;
  for (size_t i=0;i<s.size();){
    if (!s.compare(i,3,"tZ_")){ const char l=i?s[i-1]:'[',r=i+3<s.size()?s[i+3]:']'; o+=(l=='(' || l=='[' || l==',') && (r==')' || r==']' || r==',')?v0:v; i+=3; } // (cos(s/5), not cos((s/5)))
    else o+=s[i++];
  }
  return txt(o);
}
static NOINL gen croots(const gen & s,const gen & t,bool num=true){ // a solve's roots free of t (num: real numbers)
  vecteur c;
  if (s.type==_VECT) EACH(it,*s._VECTptr){ const gen v=is(*it,at_equal)?arg(*it,1):*it; if (!dep(v,t) && (!num || evalf(v,1,CTX).type==_DOUBLE_)) c.push_back(rn(v)); }
  return gen(c,_SEQ__VECT);
}

// the time t0 with r(t0) = P: the simplest coordinate that depends on t solved, each root checked in
// the other coordinates (the keys: "x = t^2 = 4 when t = +-2", "y(2) = 2 X, y(-2) = 6 check"); undef
// if P is not on the curve
static NOINL gen ctime(const gen & r,const gen & P,const gen & t,bool show,int r0=R_CTIME){
  const vecteur & R=*r._VECTptr,& Q=*P._VECTptr;
  int b=-1;
  for (unsigned i=0;i<R.size();++i) if (dep(R[i],t) && (b<0 || taille(R[i],200)<taille(R[b],200))) b=i;
  if (b<0 || Q.size()!=R.size()) return undef;
  const gen c=croots(_solve(makesequence(symb_equal(exact(R[b],CTX),exact(Q[b],CTX)),t),CTX),t);
  const vecteur & C=*c._VECTptr;
  if (show){
    CL(C.empty()?R_CNOT:r0,"%0=%1",R[b],Q[b]);
    if (!C.empty()) S(R_VL,C.size()==1?symb_equal(t,C[0]):c,0);
  }
  gen found=undef;
  EACH(it,C){
    bool ok=true;
    std::string w;
    for (unsigned i=0;i<R.size();++i){
      if ((int)i==b) continue;
      const gen v=rn(ev(sub(R[i],t,*it)));
      const bool e=is_zero(rn(v-Q[i])) || is_zero(evalf(v-Q[i],1,CTX));
      if (!e) ok=false;
      if (!w.empty()) w+=",";
      w+=vprint(cput(R[i],t,*it)); w+="="; w+=vprint(v);
      if (!e){ w+="\xe2\x89\xa0"; w+=vprint(Q[i]); }
    }
    if (show) VL(ok?R_CTCHK:R_CTBAD,w);
    if (ok){
      const gen x=evalf(*it,1,CTX),y=evalf(found,1,CTX);
      if (is_undef(found) || (x.type==_DOUBLE_ && y.type==_DOUBLE_ && (x._DOUBLE_val>=0?(y._DOUBLE_val<0 || x._DOUBLE_val<y._DOUBLE_val):(y._DOUBLE_val<0 && x._DOUBLE_val>y._DOUBLE_val)))) found=*it;
    }
  }
  if (show && !is_undef(found)) CL(R_CSO,"%0=%1",t,found);
  return found;
}
static NOINL gen cder(const gen & r,const gen & t){ vecteur w; EACH(it,*r._VECTptr) w.push_back(rn1(D(*it,t))); return gen(w); }

// two lines: their direction vectors (parallel?), then the components set equal with T for the second
// line, t and T from two of them, the third checked
static NOINL gen clines(const gen & l1,const gen & l20,bool show){
  const gen t=cpar(l1),T(identificateur("T")),l2=subst(l20,cpar(l20),T,false,CTX);
  const gen d1=cder(l1,t),d2=cder(l2,T),O=makevecteur(t,T),Z=makevecteur(0,0);
  const vecteur & a=*d1._VECTptr,& b=*d2._VECTptr,& L=*l1._VECTptr,& M=*l2._VECTptr;
  const unsigned n=a.size();
  if (n!=b.size() || n!=M.size()) return undef;
  if (show) CL(R_CDIR,"v1=%0,v2=%1",d1,d2);
  int pi=-1,pj=-1; // two components whose ratios differ
  for (unsigned i=0;i<n && pi<0;++i) for (unsigned j=i+1;j<n;++j) if (!is_zero(rn(a[i]*b[j]-a[j]*b[i]))){ pi=i; pj=j; break; }
  if (pi<0){ // parallel: the same line when a point of the first is on the second
    if (show) CL(R_CPAR,"v1=%0*v2",rn(!is_zero(b[0])?a[0]/b[0]:!is_zero(b[1])?a[1]/b[1]:a[2]/b[2]));
    const gen P1=rn(ev(subst(l1,t,0,false,CTX)));
    const bool same=!is_undef(ctime(l2,P1,T,show));
    if (show) CL(same?R_CSAME:R_CPARD,"l_1(0)=%0",P1);
    cm_=same?"the same line":"parallel: they never meet"; // (the verdict in words; its steps stay offered)
    return undef;
  }
  if (show){
    if (!is_zero(b[pi]) && !is_zero(b[pj])) CL(R_CNPAR,"#0/#1\xe2\x89\xa0#2/#3",a[pi],b[pi],a[pj],b[pj]);
    else if (n==3) CL(R_CNPAR,"v1\xc3\x97v2=%0\xe2\x89\xa0""0",gen(makevecteur(rn(a[1]*b[2]-a[2]*b[1]),rn(a[2]*b[0]-a[0]*b[2]),rn(a[0]*b[1]-a[1]*b[0]))));
    CL(R_CINT,"l_1(t)=%0,l_2(T)=%1",l1,l2);
    for (unsigned i=0;i<n;++i) CL(i?R_VL:R_CEQ,"%0=%1",L[i],M[i]);
  }
  // two equations that fix t and T: one with a single parameter first ("from z: t = 3")
  gen t0=undef,T0=undef;
  int k=-1;
  for (int pass=0;pass<3 && is_undef(t0);++pass)
  for (unsigned i=0;i<n && is_undef(t0);++i) for (unsigned j=0;j<n;++j){
    if (i==j) continue;
    const gen e1=ev(L[i]-M[i]),e2=ev(L[j]-M[j]),p=D(e1,t),q=D(e1,T),r=D(e2,t),s=D(e2,T),det=rn(p*s-q*r);
    if (is_zero(det) || (!pass && !is_zero(p) && !is_zero(q)) || (pass==1 && !is_one(abs(p,CTX)) && !is_one(abs(q,CTX)))) continue;
    const gen c1=sub(e1,O,Z),c2=sub(e2,O,Z),v=rn((q*c2-s*c1)/det),w=rn((r*c1-p*c2)/det);
    if (show){ // the first solved for t (or T: the one alone in it, or with coefficient 1), put into the second
      const bool tq=is_zero(q) || (!is_zero(p) && is_one(abs(p,CTX)) && !is_one(abs(q,CTX)));
      const gen x=tq?t:T,y=tq?T:t,sx=rn(_solve(makesequence(symb_equal(L[i],M[i]),x),CTX)[0]),vy=tq?w:v;
      CL(R_CSOLVE,"%0=%1",x,is(sx,at_equal)?arg(sx,1):sx);
      CL(R_VL,"%0=%1",subst(L[j],x,cpr(sx),false,CTX),subst(M[j],x,cpr(sx),false,CTX));
      CL(R_VLN,"%0=%1",y,vy);
      if (dep(sx,y)) CL(R_VLN,"%0=%1=%2",x,subst(sx,y,cpr(vy),false,CTX),tq?v:w);
    }
    t0=v; T0=w;
    for (unsigned m=0;m<n;++m) if (m!=i && m!=j) k=m;
    break;
  }
  if (is_undef(t0)) return undef;
  const gen P=rn(ev(subst(l1,t,t0,false,CTX)));
  bool meet=true;
  if (k>=0){
    const gen x1=rn(ev(sub(L[k],t,t0))),x2=rn(ev(sub(M[k],T,T0)));
    meet=is_zero(rn(x1-x2));
    if (show) CL(meet?R_CCHK:R_CSKEW,meet?"%0=%1,%2=%3":"%0=%1\xe2\x89\xa0%3=%2",cput(L[k],t,t0),x1,cput(M[k],T,T0),x2);
  }
  if (!meet){ cm_="skew: not parallel, and they never meet"; return undef; }
  if (show) CL(R_CMEET,"l_1(%0)=l_2(%1)=%2",t0,T0,P);
  return txt(SF("l_1(%0)=l_2(%1)=%2",vprint(t0).c_str(),vprint(T0).c_str(),vprint(P).c_str())); // (where: both times and the point)
}
// the tangent line at the point P (or the time t0): P0 and v = r'(t0), l(t) = v t + P0
static NOINL gen ctan(const gen & r,const gen & at,bool show){
  const gen t=cpar(r),v=cder(r,t);
  gen t0,P;
  if (!isvec(at)){ t0=ctime0(at); P=rn(ev(subst(r,t,t0,false,CTX))); if (show) CL(R_CNEED,"P_0=r(%0)=%1",t0,P); }
  else if (show) CL(R_CNEED,"P_0=%0",at);
  if (show) CL(R_CDER,"r'(%0)=%1",t,v);
  if (isvec(at)){ P=at; t0=ctime(r,P,t,show); if (is_undef(t0)){ cm_="that point is not on r(t) (type r(t), not r'(t))"; return undef; } }
  const gen w=rn(ev(subst(v,t,t0,false,CTX)));
  vecteur l;
  for (unsigned i=0;i<w._VECTptr->size();++i) l.push_back(rn((*w._VECTptr)[i]*t+(*P._VECTptr)[i]));
  if (show){ CL(R_CEVAL,"v=r'(%0)=%1",t0,w); CL(R_CLINE,"l(%0)=%1*%0+%2",t,w,P); CL(R_VL,"l(%0)=%1",t,gen(l)); }
  return gen(l);
}
// |r'(t)| written out (the magnitude's steps), r' first
static NOINL gen cspeed(const gen & r,const gen & t,bool show,int r0){
  const gen v=cder(r,t);
  if (!show){ vecteur q; return vsqrt(sqv(v,q)); }
  CL(r0,"r'(%0)=%1",t,v);
  return vmag(v,"abs(r'("+vprint(t)+"))");
}
// s = the integral of |r'(t)| from the time at A to the time at B (points or times), then its value
static NOINL gen carc(const gen & r,const gen & A,const gen & B,bool show){
  const gen t=cpar(r);
  const gen sp=cspeed(r,t,show,R_CARC);
  gen a=isvec(A)?gen(undef):ctime0(A),b=isvec(B)?gen(undef):ctime0(B);
  if (isvec(A) || isvec(B)){
    if (isvec(A)) a=ctime(r,A,t,show,R_CLIM);
    if (isvec(B)) b=ctime(r,B,t,show,isvec(A)?R_CTIME:R_CLIM);
    if (is_undef(a) || is_undef(b)){ cm_="an endpoint is not on the curve"; return undef; }
  }
  if (is_strictly_greater(a,b,CTX)){ const gen c=a; a=b; b=c; }
  const gen I=symbolic(at_integrate,makesequence(sp,t,a,b)),val=evalf(I,1,CTX);
  gen e0=0;
  if (show){
    CL(R_CSO,"s=%0",I);
    bool ex=true; // (no root of t in |r'(t)|: its exact value too, 10 pi; a root's took long on the calculator)
    { vecteur f; flat(at_prod,sp,f); EACH(it,f) if (dep(*it,t) && (has_op(*it,*at_sqrt) || (is(*it,at_pow) && arg(*it,1).type==_FRAC))) ex=false; }
    if (ex){ const gen e=rn(_integrate(makesequence(sp,t,a,b),CTX)); if (!has_op(e,*at_integrate) && !is_undef(e)){ CL(R_VL,"s=%0",e); e0=e; } }
    CL(R_VL,"s\xe2\x89\x88%0",val);
  }
  if (is_zero(e0)) return txt(SF("s=%0,s=%1",vprint(I).c_str(),vprint(val).c_str())); // (the integral as set up, then its value: no approx sign in the answer's font)
  return e0; // (2pi, its decimal shown by the console)
}
// an arclength parameterization: |r'(t)| = 1? else s(t), t(s), p(s) = r(t(s))
static NOINL gen carcp(const gen & r,bool show){
  const gen t=cpar(r),T(identificateur("T")),s(identificateur("s"));
  const gen sp=cspeed(r,t,show,R_CDER);
  if (is_one(sp)){ if (show) CL(R_CAPY,"abs(r'(%0))=1",t); return txt(SF("abs(r'(t))=1,p(s)=%0",vprint(cput(r,t,s)).c_str())); }
  if (show) CL(R_CAPN,"abs(r'(%0))=%1\xe2\x89\xa0""1",t,sp);
  const gen sT=subst(sp,t,T,false,CTX);
  gen S0,ts=undef,tt=0; // (tt: t(s) as written, ((27s+4)/4)^(2/3), giac's ((..)^(1/3))^2 unread)
  { vecteur f; flat(at_prod,sp,f); gen k=1,L=0; // k sqrt(a t+b)
    EACH(it,f){ if (!dep(*it,t)) k=k*(*it); else if (is(*it,at_sqrt) && is_zero(L)) L=arg(*it); else if (is(*it,at_pow) && arg(*it,1)==plus_one_half && is_zero(L)) L=arg(*it,0); else L=undef; }
    const gen a=D(L,t),b=rn(sub(L,t,0));
    if (!is_undef(L) && !is_zero(L) && !dep(a,t) && !is_zero(a)){
      const gen h=gen(3)/2,c=rn(2*k/(3*a));
      S0=symb_prod(c,symb_plus(symb_pow(L,h),symb_neg(rn(pow(b,h,CTX)))));
      ts=rn((pow(rn(s/c+pow(b,h,CTX)),gen(2)/3,CTX)-b)/a);
      const std::string w=vprint(rn(s/c+pow(b,h,CTX))),pb=vprint(b),pa=vprint(a);
      tt=txt(is_one(a)?SF("(%0)^(2/3)-%1",w.c_str(),pb.c_str()):SF("((%0)^(2/3)-%1)/%2",w.c_str(),pb.c_str(),pa.c_str()));
    }
    else S0=rn(_integrate(makesequence(sT,T,0,t),CTX)); }
  if (show) CL(R_CAP1,"s(%0)=%1=%2",t,symbolic(at_integrate,makesequence(sT,T,0,t)),S0);
  if (is_undef(ts)){
    const gen z=croots(_solve(makesequence(symb_equal(s,S0),t),CTX),t,false);
    if (z._VECTptr->empty()){ cm_="no formula for t(s)"; return undef; }
    ts=rn(z[0]);
  }
  if (!is_zero(tt)){ // p(s): r with t(s) put in, as the key leaves it (expanded, it ran off the screen)
    const gen p=cput(r,t,tt);
    if (show){ CL(R_CAP2,"s=%0,%1=%2",S0,t,tt); CL(R_CAP3,"p(s)=%0",p); }
    return txt(SF("abs(r'(t))=%0,p(s)=%1",vprint(sp).c_str(),vprint(p).c_str()));
  }
  const gen p=rn(ev(subst(r,t,ts,false,CTX)));
  if (show){ CL(R_CAP2,"s=%0,%1=%2",S0,t,ts); CL(R_CAP3,"p(s)=r(%0)=%1",ts,p); }
  vecteur pe; EACH(it,*r._VECTptr) pe.push_back(expand(subst(*it,t,ts,false,CTX),CTX)); // (5/3-2s/sqrt(13), cos(s/5): as the keys)
  return txt(SF("abs(r'(t))=%0,p(s)=%1",vprint(sp).c_str(),vprint(gen(pe)).c_str()));
}
// r(t) from r'(t) and r(t0) = r0: each component integrated with its constant, then t0 put in
static NOINL gen cpos(const gen & v,const gen & r0,const gen & t00,bool show){
  const gen t=cpar(v),t0=is_undef(t00)?gen(0):ctime0(t00);
  const vecteur & V=*v._VECTptr,& R0=*r0._VECTptr;
  if (V.size()!=R0.size()) return undef;
  vecteur FC,at,eq,res;
  for (unsigned i=0;i<V.size();++i){
    std::string cn("C_"); cn+=char('1'+i);
    const gen f=rn(_integrate(makesequence(V[i],t),CTX)),c=gen(identificateur(cn));
    FC.push_back(symb_plus(f,c));
    const gen ci=rn(R0[i]-rn(ev(sub(f,t,t0))));
    at.push_back(symb_plus(cput(f,t,t0),c)); eq.push_back(symb_equal(c,ci));
    res.push_back(rn(f+ci));
  }
  if (show){
    CL(R_CINTG,"r(%0)=%1=%2",t,symbolic(at_integrate,makesequence(v,t)),gen(FC));
    CL(R_CIC,"r(%0)=%1=%2",t0,gen(at),r0);
    S(R_VL,gen(eq,_SEQ__VECT),0);
    CL(R_CSO,"r(%0)=%1",t,gen(res));
  }
  return gen(res);
}
// where r meets the surface: the curve put in, solved for t, the points
// which part of it the curve traces, as the keys reason: x(t) = sqrt(4-t^2) >= 0, so only x >= 0; y(t) = t
// gives no restriction. Each coordinate: never negative, never positive, or between two values; a limit
// when the surface has points beyond it (the circle has x < 0; the parabola x = 2y^2 has no x < 0)
static int csgn(const gen & g){ // 1 never negative, -1 never positive, 0 either
  if (isnum(g)){ const gen e=evalf(g,1,CTX); return e.type!=_DOUBLE_?0:e._DOUBLE_val>=0?1:-1; }
  if (is(g,at_exp) || is(g,at_abs) || is(g,at_sqrt)) return 1;
  if (is(g,at_neg)) return -csgn(arg(g));
  if (is(g,at_inv)) return csgn(arg(g));
  if (is(g,at_pow)){
    const gen n=arg(g,1);
    if (n.type==_INT_) return n.val%2==0?1:csgn(arg(g,0));
    if (n.type==_FRAC && n._FRACptr->den.type==_INT_ && n._FRACptr->den.val%2==0) return 1; // (sqrt: ^(1/2))
    return 0;
  }
  if (is(g,at_prod) || is(g,at_plus)){
    const bool p=is(g,at_prod);
    vecteur fs; flat(p?at_prod:at_plus,g,fs);
    int s=p?1:0;
    EACH(it,fs){ const int k=csgn(*it); if (!k) return 0; if (p) s*=k; else if (!s) s=k; else if (s!=k) return 0; }
    return s;
  }
  return 0;
}
static bool cbound(const gen & g,const gen & t,gen & lo,gen & hi){ // a sin(u)+b, a cos(u)+b: b-|a| to b+|a|
  const vecteur s=lop(g,at_sin),c=lop(g,at_cos);
  if (s.size()+c.size()!=1) return false;
  const gen S=s.empty()?c[0]:s[0],w(identificateur("wZ_"));
  const gen g2=subst(g,S,w,false,CTX),a=rn(ev(derive(g2,w,CTX))),b=rn(ev(subst(g2,w,0,false,CTX)));
  if (dep(a,t) || dep(b,t) || dep(a,w) || !dep(arg(S),t) || is_zero(a)) return false;
  lo=rn(b-abs(a,CTX)); hi=rn(b+abs(a,CTX));
  return true;
}
static bool cpt(const vecteur & R,const gen & t,unsigned i,vecteur & P){ // a point of the curve, coordinate i not 0
  for (int k=0;k<6;++k){
    const gen T=k==0?gen(1):k==1?gen(2):k==2?gen(-1):k==3?gen(1)/gen(2):k==4?gen(3):gen(-1)/gen(2);
    P.clear();
    bool ok=true;
    EACH(it,R){ const gen v=rn(ev(subst(*it,t,T,false,CTX))),e=evalf(v,1,CTX); if (e.type!=_DOUBLE_ && e.type!=_INT_) ok=false; P.push_back(v); }
    if (ok && !is_zero(P[i])) return true;
  }
  return false;
}
static bool chas(const gen & eq,const vecteur & from,unsigned i,const gen & v,const vecteur & P){ // a point of the surface with coordinate i = v?
  if (from.size()==2){ // (the other coordinate solved for)
    const gen s=_solve(makesequence(subst(eq,from[i],v,false,CTX),from[1-i]),CTX);
    return s.type==_VECT && !s._VECTptr->empty();
  }
  vecteur Q(P); Q[i]=v; // (3D: the curve's point moved across)
  const gen e2=evalf(subst(eq,from,Q,false,CTX),1,CTX),a=arg(e2,0),b=arg(e2,1);
  if (a.type!=_DOUBLE_ || b.type!=_DOUBLE_) return false;
  return std::abs(a._DOUBLE_val-b._DOUBLE_val)<=1e-4*(1+std::abs(a._DOUBLE_val));
}
static NOINL void cpart(const vecteur & R,const vecteur & from,const gen & eq,const gen & t,bool show){
  static char m[64]; // (the message: only the right half: x is never negative)
  static const char * const NM[3]={"x","y","z"};
  int n=0,ls=0,nr=0,nn=0; unsigned li=0;
  gen L=0,N=0,llo,lhi; // (the limits that cut, for the last line; the ones that do not)
  for (unsigned i=0;i<R.size() && i<from.size();++i){
    if (!dep(R[i],t)) continue;
    const int s=csgn(R[i]);
    gen lo,hi;
    const bool b=!s && cbound(R[i],t,lo,hi);
    if (show){
      const int rr=nr++?R_VLN:R_CRNG;
      if (s) CL(rr,s>0?"%0=%1\xe2\x89\xa5" "0":"%0=%1\xe2\x89\xa4" "0",from[i],R[i]);
      else if (b) CL(rr,"%0\xe2\x89\xa4%1=%2\xe2\x89\xa4%3",lo,from[i],R[i],hi);
      else CL(rr,"%0=%1",from[i],R[i]);
    }
    if (!s && !b) continue;
    vecteur P;
    if (!cpt(R,t,i,P)) continue;
    const bool c=s?chas(eq,from,i,rn(-P[i]),P):(from.size()==2 && (chas(eq,from,i,rn(hi+hi-lo),P) || chas(eq,from,i,rn(lo-hi+lo),P)));
    const gen l=s?(s>0?symbolic(at_superieur_egal,makesequence(from[i],0)):symbolic(at_inferieur_egal,makesequence(from[i],0))):gen(txt(vprint(lo)+"\xe2\x89\xa4"+vprint(from[i])+"\xe2\x89\xa4"+vprint(hi)));
    if (!c){ N=nn++?gen(makevecteur(N,l),_SEQ__VECT):l; continue; }
    L=n?gen(makevecteur(L,l),_SEQ__VECT):l;
    ++n; li=i; ls=s; llo=lo; lhi=hi;
  }
  if (show && nn) S(R_CNOCUT,N,0);
  if (!n) return;
  if (show) S(R_CPART,L,0);
  const std::string a=n>1 || ls?std::string():vprint(llo),b=n>1 || ls?std::string():vprint(lhi);
  if (n>1 || a.size()+b.size()>28) strcpy(m,"only part of it: see the last step");
  else {
    strcpy(m,"only the ");
    if (ls && from.size()==2){ strcat(m,li?(ls>0?"top":"bottom"):(ls>0?"right":"left")); strcat(m," half: "); }
    else strcat(m,"part where ");
    strcat(m,NM[li]);
    if (ls){ strcat(m," is never "); strcat(m,ls>0?"negative":"positive"); }
    else { strcat(m," is from "); strcat(m,a.c_str()); strcat(m," to "); strcat(m,b.c_str()); }
  }
  cm_=m;
}
// a constant a in the surface: put in, f(t) = R. For every t: a from one t, checked for all t (the key: "no
// matter what r is, this cannot hold for all t"). Where a drops out (its coefficient 0), f's values: on
// both sides of R, the curve meets the surface for every a (the key's IVT)
static NOINL gen cparm(const gen & L,const gen & R,const gen & t,const gen & a,bool show){
  static char m[120];
  const gen d=rn(L-R);
  const std::string an=vprint(a);
  for (int i=0;i<3;++i){
    const gen di=rn(ev(sub(d,t,i)));
    if (!dep(di,a)) continue;
    const gen c=croots(_solve(makesequence(symb_equal(di,0),a),CTX),a);
    if (c._VECTptr->empty()) break;
    const gen a0=(*c._VECTptr)[0];
    gen d0=rn(ev(sub(d,a,a0)));
    if (!is_zero(d0)){ const gen d2=rn(_simplify(d0,CTX)); if (taille(d2,200)<taille(d0,200)) d0=d2; }
    if (show){ CL(R_PEVERY,"%0=%1,%2=0,%3",t,i,di,symb_equal(a,a0)); CL(is_zero(d0)?R_CALL:R_PNOT,"%0=0",d0); }
    if (is_zero(d0)){ strcpy(m,("the whole curve lies on it when "+an+" = "+vprint(a0)).c_str()); cm_=m; return undef; }
    break;
  }
  const gen da=rn(derive(d,a,CTX));
  const gen tv[]={0,cst_pi/2,-cst_pi/2,cst_pi,1,-1,2,-2};
  double lo=0,hi=0; gen tl,th;
  for (int i=0;i<8;++i){
    if (!is_zero(rn(ev(sub(da,t,tv[i]))))) continue;
    const gen v=evalf(ev(sub(d,t,tv[i])),1,CTX);
    if (v.type!=_DOUBLE_) continue;
    if (v._DOUBLE_val<lo){ lo=v._DOUBLE_val; tl=tv[i]; }
    if (v._DOUBLE_val>hi){ hi=v._DOUBLE_val; th=tv[i]; }
  }
  if (lo<0 && hi>0){
    const gen fl=rn(ev(sub(L,t,tl))),fh=rn(ev(sub(L,t,th)));
    if (show){ CL(R_PSIGN,"f(%0)=%1",t,L); CL(R_VLN,"f(%0)=%1,f(%2)=%3",tl,fl,th,fh); }
    strcpy(m,("it meets the surface for every "+an+": f(t) goes from "+vprint(fl)+" to "+vprint(fh)+", past "+vprint(R)).c_str());
    cm_=m; return undef;
  }
  strcpy(m,("no "+an+" makes the whole curve lie on it").c_str());
  cm_=m; return undef;
}
static NOINL gen cmeet(const gen & r,const gen & e,bool show){
  const gen t=cpar(r),x(identificateur("x")),y(identificateur("y")),z(identificateur("z"));
  const vecteur & R=*r._VECTptr;
  const gen eq=is(e,at_equal)?e:symb_equal(e,0);
  vecteur from=makevecteur(x,y),to,tp;
  if (R.size()==3) from.push_back(z);
  for (unsigned i=0;i<from.size() && i<R.size();++i){ to.push_back(R[i]); tp.push_back(cpr(R[i])); }
  const gen q=subst(eq,from,tp,false,CTX),e2=subst(eq,from,to,false,CTX);
  gen d=rn(ev(arg(e2,0)-arg(e2,1)));
  if (!is_zero(d)){ const gen d2=rn(_simplify(d,CTX)); if (taille(d2,200)<taille(d,200)) d=d2; }
  if (show){
    CL(R_CSUBS,"%0",q);
    gen L=rn(ev(arg(e2,0))),R=rn(ev(arg(e2,1)));
    if (is_zero(d)){ const gen L2=rn(_simplify(L,CTX)),R2=rn(_simplify(R,CTX)); if (taille(L2,200)<taille(L,200)) L=L2; if (taille(R2,200)<taille(R,200)) R=R2; }
    if (!is_zero(R)) CL(R_VLN,"%0=%1",L,R);
    CL(R_VLN,"%0=0",d);
    const gen f=_factor(d,CTX);
    if (vprint(f)!=vprint(d) && is(f,at_prod)){ vecteur fs; flat(at_prod,f,fs); int nt=0; EACH(it,fs) if (dep(*it,t)) ++nt; if (nt>1) CL(R_VLN,"%0=0",f); } // (t(t+4) = 0, not 4(t-1) = 0)
  }
  if (is_zero(d)){ cm_="the whole curve lies on the surface"; if (show) CL(R_CALL,"%0=0",d); cpart(R,from,eq,t,show); return undef; }
  { const vecteur ids=lidnt(d); // (a constant in the surface: x+ry-z^2 = 2, 2x+3y-az = 5)
    EACH(it,ids) if (*it!=t && evalf(*it,1,CTX).type!=_DOUBLE_) return cparm(rn(ev(arg(e2,0))),rn(ev(arg(e2,1))),t,*it,show); }
  gen ts=croots(_solve(makesequence(symb_equal(exact(d,CTX),0),t),CTX),t);
  if (vprint(d).find('.')!=std::string::npos) ts=evalf(ts,1,CTX); // (decimals typed: decimal roots)
  if (ts._VECTptr->empty()){ cm_="the curve does not meet the surface"; if (show) CL(R_CNONE,"%0=0",d); return undef; }
  vecteur ps,w;
  EACH(it,*ts._VECTptr){ ps.push_back(rn(ev(subst(r,t,*it,false,CTX)))); w.push_back(symb_equal(symbolic(at_of,makesequence(identificateur("r"),*it)),ps.back())); }
  if (show){ vecteur te; EACH(it,*ts._VECTptr) te.push_back(symb_equal(t,*it)); S(R_VL,te.size()==1?te[0]:gen(te,_SEQ__VECT),0); S(R_CPTS,w.size()==1?w[0]:gen(w,_SEQ__VECT),0); }
  { static char m[150]; // (intersects at (1,0,0) when t = 0: the verdict the questions ask for)
    std::string a="intersects at ";
    for (unsigned i=0;i<ps.size();++i){
      std::string p=vprint(ps[i]);
      if (p.size()>1 && p[0]=='['){ p[0]='('; p[p.size()-1]=')'; }
      if (i) a+=" and ";
      a+=p; a+=" when t = "; a+=vprint(ts[i]);
    }
    if (a.size()<sizeof m){ strcpy(m,a.c_str()); cm_=m; return undef; }
  }
  if (ps.size()==1) return txt(vprint(w[0])); // (r(0)=[1,0,0]: the time with the point)
  std::string a;
  EACH(it,w){ if (!a.empty()) a+=","; a+=vprint(*it); }
  return txt(a);
}
// the line through P and Q: l(t) = P + t(Q-P)
static NOINL gen cline2(const gen & P,const gen & Q,bool show){
  const gen t(identificateur("t")),d=rn(ev(Q-P));
  vecteur l;
  for (unsigned i=0;i<d._VECTptr->size();++i) l.push_back(rn((*P._VECTptr)[i]+(*d._VECTptr)[i]*t));
  if (show){ CL(R_CDIR,"v=%0-%1=%2",Q,P,d); CL(R_CLINE,"l(t)=%0+%1*t=%2",P,d,gen(l)); }
  return gen(l);
}
// a curve from its equation (2x+3y=5, y=x^2, x=2y^2, x^2+y^2=9, 4x^2+y^2=4): an r(t) tracing all of it.
// The key: "set x or y equal to t, then solve for the other"; an ellipse (a circle): x = h + a cos t,
// y = k + b sin t, as cos^2 t + sin^2 t = 1
static NOINL bool cpos0(const gen & g){ const gen f=evalf(g,1,CTX); return f.type==_DOUBLE_ && f._DOUBLE_val>0; }
static NOINL std::string csq(const gen & w,const gen & D){ // (x-1)^2/4, x^2
  std::string s=vatom(w,3)+"^2";
  if (!is_one(D)) s+="/"+vatom(D,1);
  return s;
}
static NOINL gen clin(const gen & g,const gen & t){ // 5/3-2t/3 as the key writes it, not (-2t+5)/3
  const gen c1=rn(derive(g,t,CTX));
  if (dep(c1,t)) return g;
  const gen c0=rn(sub(g,t,0)),ct=rn(c1*t);
  if (is_zero(c1) || is_zero(c0)) return is_zero(c1)?c0:ct;
  return cpos0(-c1)?symb_plus(c0,ct):symb_plus(ct,c0); // (3t-2, t/4-5/4)
}
static NOINL gen cparam(const gen & e0,bool show){
  const gen x(identificateur("x")),y(identificateur("y")),t(identificateur("t"));
  const gen e=is(e0,at_equal)?e0:symb_equal(y,e0); // (x^2 alone: y = x^2)
  const gen F=rn(arg(e,0)-arg(e,1));
  if (!dep(F,x) && !dep(F,y)){ cm_="type its equation in x and y: 2x+3y=5"; return undef; }
  const gen Fx=rn(derive(F,x,CTX)),Fy=rn(derive(F,y,CTX)),a=rn(derive(Fx,x,CTX)/2),b=rn(derive(Fy,y,CTX)/2);
  vecteur r(2);
  if (!is_zero(a) && !is_zero(b) && is_zero(rn(derive(Fx,y,CTX))) && !dep(a,x) && !dep(a,y) && !dep(b,x) && !dep(b,y)){ // an ellipse
    const gen h=rn(-sub(sub(Fx,x,0),y,0)/(2*a)),k=rn(-sub(sub(Fy,x,0),y,0)/(2*b)),C=rn(-sub(sub(F,x,h),y,k)),A2=rn(C/a),B2=rn(C/b);
    if (!cpos0(A2) || !cpos0(B2)){ cm_="not a line, a graph or an ellipse"; return undef; }
    r[0]=rn(h+rn(ev(symbolic(at_sqrt,A2)))*symbolic(at_cos,t)); r[1]=rn(k+rn(ev(symbolic(at_sqrt,B2)))*symbolic(at_sin,t));
    if (show){
      VL(R_PELL,csq(rn(x-h),A2)+"+"+csq(rn(y-k),B2)+"=1");
      CL(R_PCS,"x(t)=%0,y(t)=%1",r[0],r[1]);
      CL(R_CSO,"r(t)=%0",gen(r));
    }
    return gen(r);
  }
  const bool sy=!dep(Fy,x) && !dep(Fy,y) && !is_zero(Fy); // (linear in y: x = t, y solved; else y = t)
  if (!sy && (dep(Fx,x) || dep(Fx,y) || is_zero(Fx))){ cm_="not a line, a graph or an ellipse"; return undef; }
  const gen & u=sy?x:y,& w=sy?y:x;
  const gen wt=clin(rn(sub(-sub(F,w,0)/(sy?Fy:Fx),u,t)),t);
  r[sy?0:1]=t; r[sy?1:0]=wt;
  if (show){
    CL(R_PLET,"%0(t)=t",u);
    if (arg(e,0)!=w) CL(R_PPUT,"%0=%1",sub(arg(e,0),u,t),sub(arg(e,1),u,t));
    CL(R_PSOLV,"%0(t)=%1",w,wt);
    CL(R_CSO,"r(t)=%0",gen(r));
  }
  return gen(r);
}
// the domain of r(t): each part's conditions (ln: > 0, even roots: >= 0, denominators: not 0, asin and
// acos: between -1 and 1), the inequalities solved together, the denominators' zeros taken out; written
// as the key writes it: (-inf,-2) U (-2,2)
static NOINL void cdomc(const gen & g,const gen & t,vecteur & in,vecteur & ne){
  if (g.type==_VECT){ EACH(it,*g._VECTptr) cdomc(*it,t,in,ne); return; }
  if (g.type!=_SYMB || !dep(g,t)) return;
  const gen & a=g._SYMBptr->feuille;
  cdomc(a,t,in,ne);
  const char * nm=g._SYMBptr->sommet.ptr()->s;
  if (is(g,at_ln) || is(g,at_log10)) in.push_back(symbolic(at_superieur_strict,makesequence(ev(a),0)));
  else if (is(g,at_sqrt)) in.push_back(symbolic(at_superieur_egal,makesequence(ev(a),0)));
  else if (is(g,at_inv)) ne.push_back(ev(a));
  else if (nm[0]=='/' && !nm[1] && a.type==_VECT && a._VECTptr->size()==2) ne.push_back(ev(a[1]));
  else if (is(g,at_pow) && a.type==_VECT && a._VECTptr->size()==2){
    const gen b=ev(a[0]),e=ev(a[1]);
    const bool neg=is_strictly_positive(-e,CTX);
    if (e.type==_FRAC && e._FRACptr->den.type==_INT_ && !(e._FRACptr->den.val&1)) in.push_back(symbolic(neg?at_superieur_strict:at_superieur_egal,makesequence(b,0)));
    else if (neg) ne.push_back(b);
  }
  else if (is(g,at_asin) || is(g,at_acos)){ in.push_back(symbolic(at_inferieur_egal,makesequence(ev(a),1))); in.push_back(symbolic(at_superieur_egal,makesequence(ev(a),-1))); }
}
static NOINL bool crel(const gen & r,const gen & t,gen & lo,gen & hi,bool & lc,bool & hc){ // one bound of an interval
  if (r.type!=_SYMB || arg(r).type!=_VECT || arg(r)._VECTptr->size()!=2) return false;
  bool lt=is(r,at_inferieur_strict) || is(r,at_inferieur_egal);
  const bool eq=is(r,at_inferieur_egal) || is(r,at_superieur_egal);
  if (!lt && !is(r,at_superieur_strict) && !is(r,at_superieur_egal)) return false;
  gen v;
  if (arg(r,0)==t) v=arg(r,1); else if (arg(r,1)==t){ v=arg(r,0); lt=!lt; } else return false;
  if (lt){ hi=v; hc=eq; } else { lo=v; lc=eq; }
  return true;
}
static NOINL double cnum(const gen & g){ const gen f=evalf(g,1,CTX); return f.type==_DOUBLE_?f._DOUBLE_val:0; }
static NOINL std::string cend(const gen & v){ return is_inf(v)?std::string(v==plus_inf?"inf":"-inf"):vprint(v); }
static NOINL gen cdom(const gen & r,bool show){
  const gen t=cpar(r);
  vecteur in,ne,ex;
  cdomc(r,t,in,ne);
  if (in.empty() && ne.empty()){ cm_="every t: all real numbers, (-inf,inf)"; if (show) VL(R_DCOND,"none"); return undef; }
  if (show){ vecteur c(in); EACH(it,ne) c.push_back(symbolic(at_different,makesequence(*it,0))); S(R_DCOND,c.size()==1?c[0]:gen(c,_SEQ__VECT),0); }
  vecteur sol(1,t); // (no inequality: every t)
  if (!in.empty()){
    const gen s=_solve(makesequence(in.size()==1?in[0]:gen(in),t),CTX);
    sol=s.type==_VECT?*s._VECTptr:vecteur(1,s);
    if (show) S(R_DSOLV,sol.empty()?gen(identificateur("none")):sol.size()==1?sol[0]:gen(sol,_SEQ__VECT),0);
  }
  EACH(it,ne){ const gen z=croots(_solve(makesequence(symb_equal(*it,0),t),CTX),t); EACH(jt,*z._VECTptr) if (!equalposcomp(ex,*jt)) ex.push_back(*jt); }
  // the intervals, cut at the denominators' zeros inside them
  std::string o;
  int pieces=0;
  EACH(it,sol){
    gen lo=minus_inf,hi=plus_inf; bool lc=false,hc=false;
    if (*it!=t){
      if (is(*it,at_and) && arg(*it).type==_VECT){ EACH(jt,*arg(*it)._VECTptr) if (!crel(*jt,t,lo,hi,lc,hc)){ cm_="no formula for the domain"; return undef; } }
      else if (!crel(*it,t,lo,hi,lc,hc)){ cm_="no formula for the domain"; return undef; }
    }
    vecteur cut; // (the zeros in it, in order)
    EACH(jt,ex){
      const double p=cnum(*jt);
      if ((!is_inf(lo) && p<cnum(lo)) || (!is_inf(hi) && p>cnum(hi))) continue;
      if (!is_inf(lo) && p==cnum(lo)){ lc=false; continue; }
      if (!is_inf(hi) && p==cnum(hi)){ hc=false; continue; }
      unsigned k=0; while (k<cut.size() && cnum(cut[k])<p) ++k;
      cut.insert(cut.begin()+k,*jt);
    }
    gen a=lo; bool ac=lc;
    for (unsigned k=0;k<=cut.size();++k){
      const gen b=k<cut.size()?cut[k]:hi; const bool bc=k<cut.size()?false:hc;
      if (!o.empty()) o+=" U ";
      o+=ac?"[":"("; o+=cend(a); o+=","; o+=cend(b); o+=bc?"]":")";
      ++pieces;
      a=b; ac=false;
    }
  }
  if (!pieces){ cm_="no real t: the domain is empty"; return undef; }
  if (show && !ex.empty()){ vecteur c; EACH(it,ex) c.push_back(symbolic(at_different,makesequence(t,*it))); S(R_DOUT,c.size()==1?c[0]:gen(c,_SEQ__VECT),0); }
  static char m[120];
  if (o.size()>=sizeof m) return txt(o);
  strcpy(m,("domain: "+o).c_str());
  cm_=m; return undef;
}
// an unknown letter of e: no value, not t, not the name of a call in it (proj, scal)
static NOINL bool cunk(const gen & e,const gen & id){
  if (!(ev(id)==id && evalf(id,1,CTX).type!=_DOUBLE_ && strcmp(id._IDNTptr->id_name,"t"))) return false;
  const vecteur fs=lop(e,at_of);
  EACH(it,fs) if (arg(*it).type==_VECT && arg(*it,0)==id) return false;
  return true;
}
// solve eqs for sv: giac's solve, else the numerators (denominators cleared; one equation factor by factor:
// (3a+2)(sqrt(a^2+1)-sqrt(13)) = 0), each solution checked in the equations as typed (a 0 denominator: out)
static NOINL vecteur csolve(const vecteur & eqs,const vecteur & sv){
  const gen E=eqs.size()==1?eqs[0]:gen(eqs),SV=sv.size()==1?sv[0]:gen(sv);
  const gen s=_solve(makesequence(E,SV),CTX);
  bool ok=s.type==_VECT && !s._VECTptr->empty();
  if (ok) EACH(it,*s._VECTptr) if (it->type==_STRNG) ok=false;
  if (ok) return *s._VECTptr;
  vecteur ns,c,out;
  EACH(it,eqs) ns.push_back(_numer(rn(ev(arg(*it,0)-arg(*it,1))),CTX));
  if (ns.size()==1 && sv.size()==1){
    vecteur fs;
    flat(at_prod,_factor(ns[0],CTX),fs);
    EACH(it,fs) if (dep(*it,sv[0])){ const gen r=croots(_solve(makesequence(symb_equal(*it,0),sv[0]),CTX),sv[0]); EACH(jt,*r._VECTptr) c.push_back(*jt); }
  }
  else { const gen r=_solve(makesequence(gen(ns),SV),CTX); if (r.type==_VECT) EACH(it,*r._VECTptr) if (it->type!=_STRNG) c.push_back(*it); }
  EACH(it,c){
    const vecteur vals=sv.size()==1?vecteur(1,*it):(it->type==_VECT?*it->_VECTptr:vecteur());
    if (vals.size()!=sv.size()) continue;
    bool good=true;
    EACH(jt,eqs){ const gen d=evalf(subst(arg(*jt,0)-arg(*jt,1),sv,vals,false,CTX),1,CTX); if (d.type!=_DOUBLE_ || d._DOUBLE_val>1e-6 || d._DOUBLE_val<-1e-6) good=false; }
    if (good && !equalposcomp(out,*it)) out.push_back(*it);
  }
  return out;
}
// an equation whose left side computes with vectors (not a name, not r(t) defined) with an unknown letter
// (no value; t excluded): the condition of "give an example of v"
static NOINL bool cfindable(const gen & g){
  if (!is(g,at_equal) || arg(g).type!=_VECT || arg(g)._VECTptr->size()!=2) return false;
  const gen & l=arg(g,0);
  vecteur va;
  if (l.type==_IDNT || (is(l,at_of) && !vcall(l,va)) || !vecish(g)) return false; // (r(t) = ... defines; proj(...) = ... asks)
  const vecteur ids=lidnt(g);
  EACH(it,ids) if (cunk(g,*it)) return true;
  return false;
}
// "give an example of v": the condition typed with v as (a,b); each side worked out (|u+v| its
// length, u x v, proj), the components compared, solved for the last unknowns, the others 0 (1 when 0
// is not allowed) for an example; no solution: no such vector
static NOINL gen cfind(const gen & e,bool show){
  static char m[120];
  cm_=0;
  vecteur us; // (the unknowns: the names with no value, in order)
  { const vecteur ids=lidnt(e); EACH(it,ids) if (cunk(e,*it)) us.push_back(*it); }
  if (us.empty()){ cm_="no unknown: write v as (a,b)"; return undef; }
  gen d0,q0,d1,q1;
  const gen L=vstep(arg(e,0),d0,q0),R=vstep(arg(e,1),d1,q1);
  vecteur eqs;
  if (isvec(L) && isvec(R) && L._VECTptr->size()==R._VECTptr->size()){ for (unsigned i=0;i<L._VECTptr->size();++i) eqs.push_back(symb_equal(rn(L[i]),rn(R[i]))); }
  else if (!isvec(L) && !isvec(R) && !is_undef(L) && !is_undef(R)) eqs.push_back(symb_equal(rn(L),rn(R)));
  else { cm_="one side is a vector, the other a number"; return undef; }
  vmsg_=0;
  if (show) S(R_FEQ,eqs.size()==1?eqs[0]:gen(eqs,_SEQ__VECT),0);
  const unsigned n=eqs.size()<us.size()?eqs.size():us.size();
  const vecteur sv(us.end()-n,us.end()),fv(us.begin(),us.end()-n); // (solved for: b; free: a)
  const vecteur s=csolve(eqs,sv);
  const bool any=!s.empty();
  std::string o;
  if (fv.empty()){ // (as many unknowns as conditions: every solution, "find all a")
    if (!any){ cm_="no solution: no such vector"; if (show) VL(R_FSOLV,"none"); return undef; }
    vecteur w;
    std::string mt; // (one unknown, or one solution: as math, a = -2 sqrt(3), a = 2 sqrt(3))
    EACH(it,s){
      vecteur vals;
      if (sv.size()==1) vals.push_back(*it); else if (it->type==_VECT && it->_VECTptr->size()==sv.size()) vals=*it->_VECTptr; else continue;
      std::string one;
      for (unsigned i=0;i<sv.size();++i){ if (!one.empty()) one+=", "; one+=vprint(sv[i])+" = "+vprint(rn(vals[i])); w.push_back(symb_equal(sv[i],rn(vals[i]))); if (!mt.empty()) mt+=","; mt+=vprint(sv[i])+"="+vprint(rn(vals[i])); }
      if (!o.empty()) o+=" or ";
      o+=one;
    }
    if (show && !w.empty()) S(R_FSOLV,w.size()==1?w[0]:gen(w,_SEQ__VECT),0);
    if (!mt.empty() && (sv.size()==1 || s.size()==1)) return txt(mt);
  }
  else { // (more unknowns: the last ones from the others, then those 0, else 1, for an example)
    vecteur g0;
    if (any){ const gen s0=s[0]; if (sv.size()==1) g0.push_back(rn(s0)); else if (s0.type==_VECT && s0._VECTptr->size()==sv.size()) g0=*s0._VECTptr; }
    if (show && g0.size()==sv.size()){ vecteur w; for (unsigned i=0;i<sv.size();++i) w.push_back(symb_equal(sv[i],rn(g0[i]))); S(R_FSOLV,w.size()==1?w[0]:gen(w,_SEQ__VECT),0); }
    for (int z=0;z<2 && o.empty();++z){
      const vecteur zs(fv.size(),gen(z));
      vecteur ex;
      if (g0.size()==sv.size()){ EACH(it,g0) ex.push_back(rn(ev(subst(*it,fv,zs,false,CTX)))); }
      else {
        vecteur e2;
        EACH(it,eqs) e2.push_back(subst(*it,fv,zs,false,CTX));
        const vecteur s2=csolve(e2,sv);
        if (!s2.empty()){ const gen s0=s2.back(); if (sv.size()==1) ex.push_back(rn(s0)); else if (s0.type==_VECT && s0._VECTptr->size()==sv.size()) ex=*s0._VECTptr; }
      }
      if (ex.size()!=sv.size()) continue;
      bool ok=true,all0=true;
      EACH(it,ex){ if (is_undef(*it) || is_inf(*it)) ok=false; if (!is_zero(*it)) all0=false; }
      if (!ok || (!z && all0)) continue; // (not the zero vector)
      vecteur w;
      EACH(it,fv){ if (!o.empty()) o+=", "; o+=vprint(*it)+" = "+vprint(gen(z)); w.push_back(symb_equal(*it,z)); }
      for (unsigned i=0;i<sv.size();++i){ if (!o.empty()) o+=", "; o+=vprint(sv[i])+" = "+vprint(ex[i]); w.push_back(symb_equal(sv[i],ex[i])); }
      if (show) S(R_FEX,gen(w,_SEQ__VECT),0);
    }
    if (o.empty()){ cm_=any?"no example found":"no solution: no such vector"; return undef; }
    o+=" (one example)";
  }
  if (o.size()>=sizeof m) return txt(o);
  strcpy(m,o.c_str()); cm_=m; return undef;
}
// the curve problems: k 5 lines .. 12 line2p, 13 param, 14 domr; their arguments evaluated
static NOINL gen m3curve(int k,const vecteur & a0,bool show){
  vecteur a;
  EACH(it,a0) a.push_back(ev(*it));
  cm_=0;
  const unsigned n=a.size();
  if (k==5 && n==2 && isvec(a[0]) && isvec(a[1])) return clines(a[0],a[1],show);
  if (k==6 && n==2 && isvec(a[0])) return ctan(a[0],a[1],show);
  if (k==7 && n==3 && isvec(a[0])) return carc(a[0],a[1],a[2],show);
  if (k==8 && n>=1 && isvec(a[0])) return carcp(a[0],show);
  if (k==9 && n>=2 && isvec(a[0]) && isvec(a[1])) return cpos(a[0],a[1],n>2?a[2]:gen(undef),show);
  if (k==10 && n==2 && isvec(a[0])) return cmeet(a[0],a0[1],show);
  if (k==11 && n==2 && isvec(a[0]) && isvec(a[1])){
    const gen t=cpar(a[0]),t0=ctime(a[0],a[1],t,show);
    if (is_undef(t0)){ cm_="not on the curve"; return undef; }
    return txt(vprint(t)+"="+vprint(t0));
  }
  if (k==12 && n==2 && isvec(a[0]) && isvec(a[1])) return cline2(a[0],a[1],show);
  if (k==13 && n==1) return cparam(a[0],show); // (evaluated: x-4y typed is a binary minus, no derivative)
  if (k==14 && n==1 && isvec(a[0])) return cdom(a0[0].type==_VECT?a0[0]:a[0],show); // (as typed: sqrt(t)^2 keeps its t >= 0)
  if (!cm_) cm_="fill in each box (vectors as (a,b,c))";
  return undef;
}
// r'(t), the integral of r(t): each component's own steps, then the vector
static NOINL bool m3_comp(const gen & input){
  if (!(is(input,at_derive) || is(input,at_integrate)) || arg(input).type!=_VECT || arg(input)._VECTptr->size()<2) return false;
  const vecteur & v=*arg(input)._VECTptr;
  if (has_op(v[0],*at_abs)) return false; // (the integral of |<t,t^2>|: vstep, a magnitude first)
  const gen f=ev(v[0]);
  if (!isvec(f)) return false;
  vecteur w;
  for (unsigned i=0;i<f._VECTptr->size();++i){
    vecteur u(v); u[0]=(*f._VECTptr)[i];
    const gen op=symbolic(input._SYMBptr->sommet,gen(u,_SEQ__VECT));
    S(R_CCOMP,op,0);
    ++depth_;
    const size_t m=mark();
    gen r=one(op);
    --depth_;
    if (is_undef(r)){ cut(m); r=ev(op); }
    w.push_back(rn(r));
  }
  S(R_CSO,symb_equal(input,gen(w)),0);
  return true;
}
