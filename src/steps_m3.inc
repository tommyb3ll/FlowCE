// steps_m3.inc - included by steps.cc (FLOWCE_VECTORS, HOSTGIAC): the vectors pack (3.0).
// Vector computations written out as the course's keys write them: a dot product's products and
// their sum; a cross product's determinant with i, j, k, expanded with each 2x2 written out and the
// minus sign of the j term; a magnitude's squares; sums and differences component by component (the
// signs of a difference shown: -1-(-4/5)); proj and scal from their formulas, the dot products
// put in; the angle from cos(theta). Each step's line is text the math layout draws: [a,b,c] as
// <a,b,c>, "x" (U+00D7) the cross product, "." (U+00B7) the dot product, proj_v u with its subscript.
// Read bottom up: the parts first (u x v inside |u x v|), then what they make.

static const char * vmsg_; // why a computation is not defined (a zero direction vector)
static int vops_;          // how many computations were written out
static int vdepth_;        // the call depth: 1 at the problem itself
static int vtop_;          // what the problem itself was: 1 dot, 2 cross, 3 other computation, 0 none
static NOINL bool isvec(const gen & g){
  return g.type==_VECT && g.subtype!=_SEQ__VECT && (g._VECTptr->size()==2 || g._VECTptr->size()==3) && !ckmatrix(g);
}
static NOINL void VL(int r,const std::string & s){ S(r,txt(s),0); }
static NOINL bool vcompound(const std::string & s);
static NOINL bool vecish(const gen & g);
static NOINL std::string vprint(const gen & g);
// g as an operand: in parentheses unless a name, a vector, a call or a number >= 0. k: 0 a factor of
// a product (a power needs none: 16*sin(t)^2), 1 a denominator, 2 a product's first factor (-3*sin(t)),
// 3 a power's base or a value put in (a root too: (sqrt(4-t^2))^2)
static NOINL std::string vatom(const gen & g,int k=1){
  const std::string s=vprint(g);
  bool par;
  if (g.type==_IDNT) par=vcompound(s);
  else if (g.type==_VECT) par=false;
  else if (g.type==_SYMB){
    const char * nm=g._SYMBptr->sommet.ptr()->s;
    par=is(g,at_plus) || !strcmp(nm,"-") || (k&1 && (is(g,at_prod) || is(g,at_inv) || !strcmp(nm,"/"))) ||
        (is(g,at_neg) && k!=2) || (is(g,at_pow) && k&1 && (k==3 || !(arg(g,1)==plus_one_half))) || (is(g,at_sqrt) && k==3); // (k 3: (sqrt(4-t^2))^2, no t^2^2 look)
  }
  else par=(s[0]=='-' && k!=2) || (g.type==_FRAC && k==1);
  if (!par) return s;
  std::string t="("; t+=s; t+=")"; return t;
}
// the lines' text: giac's print without its blanks ([1, -1, 0] as [1,-1,0]), sums and products
// with the parentheses they need only
static NOINL std::string vprint(const gen & g){
  if (g.type==_VECT){
    std::string s="[";
    EACH(it,*g._VECTptr){ if (it!=g._VECTptr->begin()) s+=","; s+=vprint(*it); }
    s+="]";
    return s;
  }
  if (is(g,at_quote)) return vprint(arg(g));
  if (g.type!=_SYMB || is(g,at_of)){ std::string s=pr(g),t; for (size_t i=0;i<s.size();++i) if (s[i]!=' ') t+=s[i]; return t; }
  const gen & a=arg(g);
  if (is(g,at_plus) && a.type==_VECT){
    std::string s;
    EACH(it,*a._VECTptr){
      std::string t=is(*it,at_neg)?"-"+(is(arg(*it),at_plus)?"("+vprint(arg(*it))+")":vprint(arg(*it))):vprint(*it);
      if (it!=a._VECTptr->begin() && t[0]!='-') s+="+";
      s+=t;
    }
    return s;
  }
  if (is(g,at_neg)) return "-"+(is(a,at_plus) || is(a,at_neg) || (a.type==_SYMB && !strcmp(a._SYMBptr->sommet.ptr()->s,"-")) || (a.type!=_SYMB && vprint(a)[0]=='-')?"("+vprint(a)+")":vprint(a));
  if (is(g,at_prod) && a.type==_VECT){
    std::string n,d;
    int nn=0,nd=0;
    EACH(it,*a._VECTptr){
      if (is(*it,at_inv)){ if (nd++) d+="*"; d+=vatom(arg(*it),is(arg(*it),at_pow)?0:1); }
      else { n+=nn?(it->type==_VECT?vatom(*it,0):"*"+vatom(*it,0)):vatom(*it,2); ++nn; }
    }
    if (!nn) n="1";
    if (!nd) return n;
    return (nn>1?"("+n+")":n)+"/"+(nd>1?"("+d+")":d);
  }
  if (is(g,at_inv)) return "1/"+vatom(a);
  if (is(g,at_pow) && a.type==_VECT && a._VECTptr->size()==2){
    if (a[1]==plus_one_half) return "sqrt("+vprint(a[0])+")";
    return vatom(a[0],3)+"^"+vatom(a[1]);
  }
  std::string s=g._SYMBptr->sommet.ptr()->s;
  if (a.type==_VECT && a._VECTptr->size()==2 && (s=="=" || s=="<" || s==">" || s=="<=" || s==">=")) return vprint(a[0])+s+vprint(a[1]);
  if (a.type==_VECT && a._VECTptr->size()==2 && (s=="-" || s=="/")) // (giac's binary minus and division)
    return s=="-"?vprint(a[0])+"-"+(is(a[1],at_plus) || is(a[1],at_neg) || (a[1].type==_SYMB && !strcmp(a[1]._SYMBptr->sommet.ptr()->s,"-"))?"("+vprint(a[1])+")":vprint(a[1])):vatom(a[0])+"/"+vatom(a[1]);
  s+="(";
  if (a.type==_VECT && a.subtype==_SEQ__VECT){ EACH(it,*a._VECTptr){ if (it!=a._VECTptr->begin()) s+=","; s+=vprint(*it); } }
  else s+=vprint(a);
  s+=")";
  return s;
}
static NOINL std::string vpp(const gen & x){ std::string s="("; s+=vprint(x); s+=")"; return s; }
// a term after a minus or as a factor: in parentheses when negative or a sum (-1-(-4/5))
static NOINL std::string vsg(const gen & x){
  std::string s=vprint(x);
  if (s[0]=='-' || s.find_first_of("+-",1)!=std::string::npos){ std::string t="("; t+=s; t+=")"; return t; }
  return s;
}
// terms as a sum on paper: 6-2+2
static NOINL std::string vssum(const vecteur & t){
  std::string s;
  EACH(it,t){
    const std::string q=vprint(*it);
    if (it!=t.begin() && q[0]!='-') s+="+";
    s+=q;
  }
  return s.empty()?std::string("0"):s;
}
// operators at the top of a display's text (a sum, a product, x, ., a space): it needs parentheses
// as an operand
static NOINL bool vcompound(const std::string & s){
  int d=0;
  for (size_t i=0;i<s.size();++i){
    const unsigned char c=s[i];
    if (c=='(' || c=='[' || c=='{') ++d;
    else if (c==')' || c==']' || c=='}') --d;
    else if (!d && (c=='+' || c=='-' || c==' ' || c=='*' || c=='/' || c==0xc3 || c==0xc2)) return true;
  }
  return false;
}
static NOINL std::string vpd(const gen & d){ return vatom(d); }
static NOINL std::string vdec4(const gen & x){ // a decimal to 4 significant digits
  const gen d=evalf_double(x,1,CTX);
  if (d.type!=_DOUBLE_) return vprint(x);
  char b[24]; sprintf(b,"%.4g",d._DOUBLE_val);
  return b;
}
static NOINL gen vsimp(const gen & g){ // exp and ln of one another simplified (rn keeps e^t*e^(-t))
  if (g.type==_VECT){ vecteur w; EACH(it,*g._VECTptr) w.push_back(vsimp(*it)); return gen(w,g.subtype); }
  if (!has_op(g,*at_exp) && !has_op(g,*at_ln)) return g;
  const gen h=rn(_simplify(g,CTX));
  return taille(h,200)<taille(g,200)?h:g;
}
static NOINL gen vpq(const gen & v){ return isvec(v)?v:txt(vpp(v)); } // a value put in a line
static NOINL gen dotv(const gen & a,const gen & b,vecteur & t){ // u.v, its products in t
  gen s=0;
  for (unsigned i=0;i<a._VECTptr->size();++i){ const gen p=rn((*a._VECTptr)[i]*(*b._VECTptr)[i]); t.push_back(p); s=s+p; }
  return rn(s);
}
static NOINL gen sqv(const gen & a,vecteur & t){ return dotv(a,a,t); } // |u|^2, its squares in t

// ---- the computations
// a line's text from a pattern: %0..%5 the strings given (built in one place: each line's own
// concatenation cost about 5 KB a function on the eZ80)
static NOINL std::string SF(const char * f,const char * a="",const char * b="",const char * c="",const char * d="",const char * e="",const char * g=""){
  const char * v[6]={a,b,c,d,e,g};
  std::string s;
  for (;*f;++f){ if (*f=='%' && f[1]>='0' && f[1]<='5'){ s+=v[f[1]-'0']; ++f; } else s+=*f; }
  return s;
}
// the line D=pattern
static NOINL void DL(int r,const std::string & D,const char * f,const char * a="",const char * b="",const char * c="",const char * d="",const char * e="",const char * g=""){
  std::string s=D;
  s+="=";
  s+=SF(f,a,b,c,d,e,g);
  VL(r,s);
}
// u.v: (3)(2)+(1)(-2)+(2)(1) = 6-2+2 = 6
static NOINL gen vdot(const gen & a,const gen & b,const std::string & D,int r0=R_VDOT){
  const vecteur & x=*a._VECTptr,& y=*b._VECTptr;
  std::string p;
  for (unsigned i=0;i<x.size();++i){ if (i) p+="+"; p+=vpp(x[i]); p+=vpp(y[i]); }
  vecteur t;
  const gen s=dotv(a,b,t);
  DL(r0,D,"%0",p.c_str());
  const std::string q=vssum(t),ps=vprint(s);
  if (q!=ps) DL(R_VL,D,"%0",q.c_str());
  DL(R_VL,D,"%0",ps.c_str());
  ++vops_;
  if (vdepth_==1){ vtop_=1; if (is_zero(s)) DL(R_VORTH,D,"0"); }
  return s;
}
// u x v: det[[i,j,k],u,v] = i(..)-j(..)+k(..), each 2x2 written out, then its products, then the vector
static NOINL gen vcross(const gen & a,const gen & b,const std::string & D){
  vecteur x=*a._VECTptr,y=*b._VECTptr;
  if (x.size()==2) x.push_back(0); // (vectors of the xy-plane: in R3 with z = 0)
  if (y.size()==2) y.push_back(0);
  DL(R_VCROSS,D,"det[[i,j,k],%0,%1]",vprint(gen(x)).c_str(),vprint(gen(y)).c_str());
  std::string m[3];
  static const unsigned char P[6]={1,2,0,2,0,1}; // the 2x2 of each unit vector: its columns
  gen c[6];
  for (int k=0;k<3;++k){
    const int r=P[2*k],s=P[2*k+1];
    m[k]=SF("(%0)(%1)-(%2)(%3)",vprint(x[r]).c_str(),vprint(y[s]).c_str(),vprint(x[s]).c_str(),vprint(y[r]).c_str());
    c[2*k]=rn(x[r]*y[s]); c[2*k+1]=rn(x[s]*y[r]);
  }
  DL(R_VL,D,"i(%0)",m[0].c_str());
  VL(R_VLN,SF("-j(%0)",m[1].c_str()));
  VL(R_VLN,SF("+k(%0)",m[2].c_str()));
  DL(R_VL,D,"[%0-%1,-(%2-%3),%4-%5]",vprint(c[0]).c_str(),vsg(c[1]).c_str(),vprint(c[2]).c_str(),vsg(c[3]).c_str(),vprint(c[4]).c_str(),vsg(c[5]).c_str());
  const gen r=vsimp(gen(makevecteur(rn(c[0]-c[1]),rn(c[3]-c[2]),rn(c[4]-c[5]))));
  const std::string pr0=vprint(r);
  DL(R_VL,D,"%0",pr0.c_str());
  ++vops_;
  if (vdepth_==1){ vtop_=2; if (is_zero(r[0]) && is_zero(r[1]) && is_zero(r[2])) DL(R_VPAR,D,"%0",pr0.c_str()); }
  return r;
}
// sqrt(w) as the key writes it: a square root of a perfect square worked out, k^2 m as k sqrt(m)
static NOINL gen vsqrt(const gen & w){
  { const gen n=_numer(w,CTX),dd=_denom(w,CTX);
    if (w.type==_FRAC){ const gen q=ev(symbolic(at_sqrt,dd)); if (q.type==_INT_) return rn(vsqrt(n)/q); } // (sqrt(13/9) = sqrt(13)/3, as the key; giac: 3 sqrt(13)/9)
    if (!is_one(dd) && !isnum(dd)){ const gen sd=vsqrt(dd); if (!has_op(sd,*at_sqrt) && !(is(sd,at_pow) && arg(sd,1).type==_FRAC)) return rn(vsqrt(n)/sd); } }
  gen r=ev(symbolic(at_sqrt,w));
  if (!isnum(w)){ // 36t+4 = 4(9t+1): 2 sqrt(9t+1)
    const gen f=_factor(w,CTX);
    vecteur fs; flat(at_prod,f,fs);
    gen k=1,rest=1;
    EACH(it,fs){ if (isnum(*it)) k=k*(*it); else rest=rest*(*it); }
    const gen sk=ev(symbolic(at_sqrt,k));
    if (!is_one(k) && isnum(sk) && is_strictly_positive(sk,CTX)) r=sk*symbolic(at_sqrt,ev(rest));
  }
  return r;
}
// |u| = sqrt(u1^2+u2^2+u3^2): the squares, worked out, added; sin^2+cos^2 = 1 when it is there
static NOINL gen vmag(const gen & a,const std::string & D){
  const vecteur & x=*a._VECTptr;
  std::string s1;
  EACH(it,x){
    if (it!=x.begin()) s1+="+";
    const std::string q=vprint(*it);
    if ((isnum(*it) && q[0]!='-') || it->type==_IDNT) s1+=q; else { s1+="("; s1+=q; s1+=")"; }
    s1+="^2";
  }
  vecteur t;
  gen s=sqv(a,t);
  DL(R_VMAG,D,"sqrt(%0)",s1.c_str());
  const std::string q=vssum(t);
  DL(R_VL,D,"sqrt(%0)",q.c_str());
  std::string ps=vprint(s);
  if (ps!=q) DL(R_VL,D,"sqrt(%0)",ps.c_str());
  if ((has_op(s,*at_sin) || has_op(s,*at_cos)) && evalf(s,1,CTX).type!=_DOUBLE_){ // 25sin(t)^2+25cos(t)^2: 25(sin(t)^2+cos(t)^2) = 25
    const vecteur ls=lop(s,at_sin);
    EACH(it,ls){
      const gen u=arg(*it),S2=symb_pow(*it,2),C2=symb_pow(symbolic(at_cos,u),2);
      const gen s2=rn(subst(s,S2,1-C2,false,CTX));
      bool left=false; // (a square of sin(u) or cos(u) still there: 4sin^2+9cos^2 kept; 3-2sin(t)-2cos(t) taken)
      { const vecteur pw=lop(s2,at_pow); EACH(jt,pw) if ((arg(*jt,0)==symbolic(at_cos,u) || arg(*jt,0)==*it) && arg(*jt,1).type==_INT_) left=true; }
      if (left) continue; // (4sin^2+9cos^2: kept as it is, as the key keeps it)
      const std::string su=vprint(u),p2=vprint(s2);
      const bool f=!dep(s2,u) && is_zero(rn(s-s2*(S2+C2)));
      if (f) DL(R_VTRIG,D,"sqrt(%0(sin(%1)^2+cos(%1)^2))",vsg(s2).c_str(),su.c_str());
      DL(f?R_VL:R_VTRIG,D,"sqrt(%0)",p2.c_str());
      s=s2; ps=p2;
      break;
    }
  }
  const gen r=vsqrt(s);
  const std::string rs=vprint(r);
  if (rs!=SF("sqrt(%0)",ps.c_str())) DL(R_VL,D,"%0",rs.c_str());
  ++vops_;
  if (vdepth_==1) vtop_=3;
  return r;
}
// a sum and difference of vectors, component by component: <1-0,-1-(-4/5),0-(-2/5)>
static NOINL gen vadd(const vecteur & v,const std::vector<char> & neg,const std::string & D,bool put,int r0=R_VADD){
  const unsigned n=v.front()._VECTptr->size();
  if (put){ // the vectors put in for their names
    std::string s=vprint(v[0]);
    for (unsigned k=1;k<v.size();++k){ s+=neg[k]?"-":"+"; s+=vprint(v[k]); }
    DL(r0,D,"%0",s.c_str());
  }
  std::string s="[";
  vecteur r;
  for (unsigned i=0;i<n;++i){
    if (i) s+=",";
    const gen & e0=(*v[0]._VECTptr)[i];
    gen c=neg[0]?-e0:e0;
    if (neg[0]){ s+="-"; s+=vsg(e0); } else s+=vprint(e0);
    for (unsigned k=1;k<v.size();++k){
      const gen & e=(*v[k]._VECTptr)[i];
      s+=neg[k]?"-":"+"; s+=vsg(e);
      c=neg[k]?c-e:c+e;
    }
    r.push_back(rn(c));
  }
  s+="]";
  DL(put?R_VL:r0,D,"%0",s.c_str());
  const gen R=gen(r);
  DL(R_VL,D,"%0",vprint(R).c_str());
  ++vops_;
  if (vdepth_==1) vtop_=3;
  return R;
}
// c times a vector: (5/sqrt(41))<0,4,5> = <0,20/sqrt(41),25/sqrt(41)>
static NOINL gen vscale(const gen & c,const gen & a,const std::string & D){
  DL(R_VSMUL,D,"(%0)%1",vprint(c).c_str(),vprint(a).c_str());
  vecteur r;
  EACH(it,*a._VECTptr) r.push_back(rn(c*(*it)));
  const gen R=gen(r);
  DL(R_VL,D,"%0",vprint(R).c_str());
  ++vops_;
  if (vdepth_==1) vtop_=3;
  return R;
}
// proj_v u = ((u.v)/(v.v)) v, scal_v u = (u.v)/|v|: the formula, the vectors in, their products, the value
static NOINL gen vproj(bool scal,const gen & v,const gen & u,const gen & dv,const gen & du,const std::string & D){
  vecteur tu,tv;
  const gen uv=dotv(u,v,tu),vv=dotv(v,v,tv);
  if (is_zero(vv)){ vmsg_="The vector it projects onto is 0: not defined"; VL(R_VUNDEF,D); return undef; }
  const std::string sv=vpd(dv),su=vpd(du),pv=vprint(v),pu=vprint(u),a1=vssum(tu),a2=vssum(tv);
  const bool names=!(dv.type==_VECT && du.type==_VECT);
  ++vops_;
  if (vdepth_==1) vtop_=3;
  if (!scal){
    static const char F[]="((%0\xc2\xb7%1)/(%1\xc2\xb7%1))%1";
    if (names) DL(R_VPROJ,D,F,su.c_str(),sv.c_str());
    DL(names?R_VL:R_VPROJ,D,F,pu.c_str(),pv.c_str());
    DL(R_VL,D,"((%0)/(%1))%2",a1.c_str(),a2.c_str(),pv.c_str());
    const gen c=rn(uv/vv);
    DL(R_VL,D,"(%0)%1",vprint(c).c_str(),pv.c_str());
    vecteur r;
    EACH(it,*v._VECTptr) r.push_back(rn(c*(*it)));
    const gen R=gen(r);
    DL(R_VL,D,"%0",vprint(R).c_str());
    return R;
  }
  static const char F[]="(%0\xc2\xb7%1)/abs(%1)";
  if (names) DL(R_VSCAL,D,F,su.c_str(),sv.c_str());
  DL(names?R_VL:R_VSCAL,D,F,pu.c_str(),pv.c_str());
  DL(R_VL,D,"(%0)/sqrt(%1)",a1.c_str(),a2.c_str());
  const gen sq=vsqrt(vv),R=rn(uv/sq);
  const std::string q=vprint(symbolic(at_prod,gen(makevecteur(uv,symbolic(at_inv,sq)),_SEQ__VECT))),pR=vprint(R);
  if (q!=SF("%0/sqrt(%1)",a1.c_str(),a2.c_str())) DL(R_VL,D,"%0",q.c_str());
  if (pR!=q) DL(R_VL,D,"%0",pR.c_str());
  return R;
}
// the angle: cos(theta) = (u.v)/(|u||v|), then theta = acos(...) and its value
static NOINL gen vangle(const gen & u,const gen & v,const gen & du,const gen & dv){
  vecteur tu,uu,vv;
  const gen uv=dotv(u,v,tu),nu=dotv(u,u,uu),nv=dotv(v,v,vv);
  if (is_zero(nu) || is_zero(nv)){ vmsg_="A zero vector has no angle"; VL(R_VUNDEF,"theta"); return undef; }
  const std::string C="cos(\xce\xb8)";
  DL(R_VANG,C,"(%0\xc2\xb7%1)/(abs(%0)abs(%1))",vpd(du).c_str(),vpd(dv).c_str());
  DL(R_VL,C,"(%0)/(sqrt(%1)sqrt(%2))",vssum(tu).c_str(),vssum(uu).c_str(),vssum(vv).c_str());
  const gen c=rn(uv/(vsqrt(nu)*vsqrt(nv)));
  const std::string pc=vprint(c);
  DL(R_VL,C,"%0",pc.c_str());
  gen t=symbolic(at_acos,c);
  { const gen te=rn(ev(t)); if (!has_op(te,*at_acos) && !has_op(te,*at_asin) && !has_op(te,*at_atan)) t=te; } // (acos(-1) = pi)
  const std::string pt=vprint(t),d1=vdec4(t),d2=vdec4(t*180/cst_pi);
  if (is(t,at_acos)) VL(R_VANGD,SF("\xce\xb8=%0,\xce\xb8\xe2\x89\x88%1,\xce\xb8\xe2\x89\x88%2",pt.c_str(),d1.c_str(),d2.c_str()));
  else VL(R_VANGD,SF("\xce\xb8=acos(%0)=%1,\xce\xb8\xe2\x89\x88%2,\xce\xb8\xe2\x89\x88%3",pc.c_str(),pt.c_str(),d1.c_str(),d2.c_str()));
  ++vops_;
  if (vdepth_==1) vtop_=3;
  return is(t,at_acos)?symbolic(at_quote,t):t;
}

// ---- the problem read bottom up
// proj(v,u) 1, scal(v,u) 2, angle(u,v) 3, pn(u,v) 4 (u = P+N along v); their arguments in a
static NOINL int vcall(const gen & g,vecteur & a){
  if (is(g,at_of) && arg(g).type==_VECT && arg(g)._VECTptr->size()==2){
    const gen & f=arg(g,0),& x=arg(g,1);
    static const char * const N[]={"proj","scal","angle","pn","lines","tanline","arclen","arcparam","posvel","meets","oncurve","line2p","param","domr"};
    int k=0;
    for (int i=0;i<14 && !k;++i) if (named(f,N[i])) k=i+1;
    if (!k) return 0;
    a=x.type==_VECT && x.subtype==_SEQ__VECT?*x._VECTptr:vecteur(1,x);
    return k;
  }
  return 0;
}
static NOINL gen vstep(const gen & g,gen & d,gen & q);
static NOINL gen m3curve(int k,const vecteur & a0,bool show); // (steps_m3c.inc)
static NOINL gen cfind(const gen & e,bool show); // (steps_m3c.inc: an example of v from a condition)
static NOINL bool cfindable(const gen & g); // (steps_m3c.inc)
static const char * cm_; // (the message of a line or curve problem: steps_m3c.inc)
static NOINL gen vstep1(const gen & g,gen & d,gen & q){
  if (g.type==_IDNT){ d=g; const gen v=ev(g); q=isvec(v)?v:g; return v; }
  if (g.type==_VECT){ // typed: its components worked out
    vecteur w;
    EACH(it,*g._VECTptr){ gen dd,qq; w.push_back(rn(vstep(*it,dd,qq))); }
    d=g; q=gen(w,g.subtype); return q;
  }
  if (g.type!=_SYMB){ d=q=g; return ev(g); }
  const gen & A=arg(g);
  { const char * nm=g._SYMBptr->sommet.ptr()->s;
    if (A.type==_VECT && A._VECTptr->size()==2 && !nm[1] && (nm[0]=='-' || nm[0]=='/'))
      return vstep1(symbolic(nm[0]=='-'?at_plus:at_prod,gen(makevecteur(A[0],symbolic(nm[0]=='-'?at_neg:at_inv,A[1])),_SEQ__VECT)),d,q); }
  if (is(g,at_prod) && A.type==_VECT){ // its factors, those of products and quotients inside it too
    vecteur f;
    bool flat=false;
    EACH(it,*A._VECTptr){
      const char * nm=it->type==_SYMB?it->_SYMBptr->sommet.ptr()->s:"";
      int nvv=0;
      if (is(*it,at_prod) && arg(*it).type==_VECT){ EACH(jt,*arg(*it)._VECTptr) if (vecish(*jt)) ++nvv; }
      if (is(*it,at_prod) && arg(*it).type==_VECT && nvv<2){ EACH(jt,*arg(*it)._VECTptr) f.push_back(*jt); flat=true; }
      else if (it->type==_SYMB && arg(*it).type==_VECT && arg(*it)._VECTptr->size()==2 && nm[0]=='/' && !nm[1]){ f.push_back(arg(*it,0)); f.push_back(symbolic(at_inv,arg(*it,1))); flat=true; }
      else f.push_back(*it);
    }
    if (flat) return vstep1(symbolic(at_prod,gen(f,_SEQ__VECT)),d,q);
  }
  const bool many=A.type==_VECT && (A.subtype==_SEQ__VECT || is(g,at_plus) || is(g,at_prod));
  vecteur ca;
  if (const int k=vcall(g,ca)){
    if (k>=5){ // lines and curves: their own steps (steps_m3c.inc)
      const gen r=m3curve(k,ca,true);
      ++vops_; vtop_=3; d=g; q=r;
      if (is_undef(r)) vmsg_=cm_;
      return r;
    }
    if (ca.size()!=2) return undef;
    gen d0,q0,d1,q1;
    const gen x=vstep(ca[0],d0,q0),y=vstep(ca[1],d1,q1);
    if (!isvec(x) || !isvec(y) || x._VECTptr->size()!=y._VECTptr->size()) return undef;
    if (k==1 || k==2){ // proj_x y: x the direction
      const std::string D=SF(k==1?"proj_(%0)(%1)":"scal_(%0)(%1)",vprint(d0).c_str(),vprint(d1).c_str());
      const gen r=vproj(k==2,x,y,d0,d1,D);
      d=txt(D); q=vpq(r); return r;
    }
    if (k==3){ const gen r=vangle(x,y,d0,d1); d=txt("\xce\xb8"); q=vpq(r); return r; }
    // u = P+N along v: P = proj_v u, N = u-P, N.v = 0
    ++vdepth_;
    const gen P=vproj(false,y,x,d1,d0,SF("P=proj_(%0)(%1)",vprint(d1).c_str(),vprint(d0).c_str()));
    if (is_undef(P)){ --vdepth_; return undef; }
    std::vector<char> ng(2,0); ng[1]=1;
    VL(R_VPN,SF("N=%0-P",vpd(d0).c_str()));
    const gen N=vadd(makevecteur(x,P),ng,"N",true,R_VL);
    vdot(N,y,SF("N\xc2\xb7%0",vpd(d1).c_str()),R_VCHK);
    --vdepth_;
    const std::string pp=vprint(P),pn=vprint(N);
    VL(R_VANS,SF("P=%0,N=%1",pp.c_str(),pn.c_str()));
    vtop_=3;
    d=txt("u=P+N"); q=d;
    return txt(SF("P=%0,N=%1",pp.c_str(),pn.c_str()));
  }
  if (is(g,at_cross) && many && A._VECTptr->size()==2){
    gen d0,q0,d1,q1;
    const gen x=vstep(A[0],d0,q0),y=vstep(A[1],d1,q1);
    if (!isvec(x) || !isvec(y)) return undef;
    const std::string D=SF("%0\xc3\x97%1",vpd(d0).c_str(),vpd(d1).c_str());
    const gen r=vcross(x,y,D);
    d=txt(D); q=r; return r;
  }
  if ((is(g,at_dot) || is(g,at_dotprod)) && many && A._VECTptr->size()==2){
    gen d0,q0,d1,q1;
    const gen x=vstep(A[0],d0,q0),y=vstep(A[1],d1,q1);
    if (!isvec(x) || !isvec(y) || x._VECTptr->size()!=y._VECTptr->size()) return undef;
    const std::string D=SF("%0\xc2\xb7%1",vpd(d0).c_str(),vpd(d1).c_str());
    const gen r=vdot(x,y,D);
    d=txt(D); q=vpq(r); return r;
  }
  if ((is(g,at_abs) || is(g,at_l2norm) || is(g,at_norm)) && !many){
    gen d0,q0;
    const gen x=vstep(A,d0,q0);
    if (isvec(x)){
      const std::string D=SF("abs(%0)",vprint(d0).c_str());
      const gen r=vmag(x,D);
      d=txt(D); q=vpq(r); return r;
    }
    d=SYM(at_abs,d0); q=SYM(at_abs,q0); return rn(ev(SYM(at_abs,x)));
  }
  if (is(g,at_prod) && many){
    const vecteur & f=*A._VECTptr;
    vecteur vals,ds,qs;
    int nv=0,i1=-1,i2=-1;
    for (unsigned i=0;i<f.size();++i){
      gen di,qi;
      vals.push_back(vstep(f[i],di,qi)); ds.push_back(di); qs.push_back(qi);
      if (is_undef(vals.back())) return undef;
      if (isvec(vals.back())){ ++nv; if (i1<0) i1=i; else i2=i; }
    }
    if (!nv || nv>2){ d=symbolic(at_prod,gen(ds,_SEQ__VECT)); q=symbolic(at_prod,gen(qs,_SEQ__VECT)); return nv?ev(symbolic(at_prod,gen(vals,_SEQ__VECT))):rn(symbolic(at_prod,gen(vals,_SEQ__VECT))); }
    gen c=1;
    for (unsigned i=0;i<f.size();++i) if ((int)i!=i1 && (int)i!=i2) c=c*vals[i];
    c=rn(c);
    if (nv==2){ // u.v (times numbers)
      if (vals[i1]._VECTptr->size()!=vals[i2]._VECTptr->size()) return undef;
      const std::string D=SF("%0\xc2\xb7%1",vpd(ds[i1]).c_str(),vpd(ds[i2]).c_str());
      const gen r=vdot(vals[i1],vals[i2],D);
      vecteur dd,qq;
      for (unsigned i=0;i<f.size();++i){ if ((int)i==i2) continue; dd.push_back((int)i==i1?txt(D):ds[i]); qq.push_back((int)i==i1?vpq(r):qs[i]); }
      d=dd.size()==1?dd[0]:symbolic(at_prod,gen(dd,_SEQ__VECT)); q=qq.size()==1?qq[0]:symbolic(at_prod,gen(qq,_SEQ__VECT));
      return rn(c*r);
    }
    d=symbolic(at_prod,gen(ds,_SEQ__VECT));
    if (is_one(c)){ q=vals[i1]; return vals[i1]; }
    const gen r=vscale(c,vals[i1],vprint(d));
    q=r; return r;
  }
  if (is(g,at_plus) && many){
    const vecteur & f=*A._VECTptr;
    vecteur vals,ds,qs;
    std::vector<char> neg;
    bool anyv=false,lit=true;
    EACH(it,f){
      const bool n=is(*it,at_neg);
      gen di,qi;
      const gen v=vstep(n?arg(*it):*it,di,qi);
      if (is_undef(v)) return undef;
      vals.push_back(v); ds.push_back(n?symb_neg(di):di); qs.push_back(n?symb_neg(qi):qi); neg.push_back(n);
      if (isvec(v)) anyv=true;
      if (di.type!=_VECT) lit=false;
    }
    d=symbolic(at_plus,gen(ds,_SEQ__VECT)); q=symbolic(at_plus,gen(qs,_SEQ__VECT));
    if (!anyv){ vecteur w; for (unsigned i=0;i<vals.size();++i) w.push_back(neg[i]?-vals[i]:vals[i]); return rn(symb_plus(w)); }
    EACH(it,vals) if (!isvec(*it) || it->_VECTptr->size()!=vals.front()._VECTptr->size()) return undef;
    const gen r=vadd(vals,neg,vprint(d),!lit);
    q=r; return r;
  }
  if (is(g,at_neg)){
    gen d0,q0;
    const gen x=vstep(A,d0,q0);
    d=symb_neg(d0); q=symb_neg(q0);
    if (isvec(x)){ const gen r=vscale(-1,x,vprint(d)); q=r; return r; }
    return rn(-x);
  }
  if ((is(g,at_derive) || is(g,at_integrate)) && many){ // r'(t) inside: its vector (|r'(t)|, r'(t)/|r'(t)|)
    vecteur a=*A._VECTptr;
    if (vecish(a[0]) && !isvec(a[0])){ gen d0,q0; a[0]=vstep(a[0],d0,q0); if (is_undef(a[0])) return undef; } // (the integral of |<t,t^2>|)
    const gen r=vsimp(rn(ev(symbolic(g._SYMBptr->sommet,gen(a,_SEQ__VECT)))));
    if (isvec(r)){ DL(R_CCOMP,vprint(g),"%0",vprint(r).c_str()); ++vops_; }
    d=g; q=r; return r;
  }
  // anything else (a power, an inverse, sqrt, a function): its arguments' computations, then it
  vecteur vals,ds,qs;
  if (many){
    EACH(it,*A._VECTptr){ gen di,qi; vals.push_back(vstep(*it,di,qi)); ds.push_back(di); qs.push_back(qi); if (is_undef(vals.back())) return undef; }
    d=symbolic(g._SYMBptr->sommet,gen(ds,_SEQ__VECT)); q=symbolic(g._SYMBptr->sommet,gen(qs,_SEQ__VECT));
    return rn(ev(symbolic(g._SYMBptr->sommet,gen(vals,_SEQ__VECT))));
  }
  gen d0,q0;
  const gen x=vstep(A,d0,q0);
  if (is_undef(x)) return undef;
  d=symbolic(g._SYMBptr->sommet,d0); q=symbolic(g._SYMBptr->sommet,q0);
  return rn(ev(symbolic(g._SYMBptr->sommet,x)));
}
static vecteur * vmemo_; // the parts computed: [part, value, display, values' display] each
static NOINL gen vstep(const gen & g,gen & d,gen & q){
  if (g.type==_SYMB && vmemo_){
    for (unsigned i=0;i+3<vmemo_->size();i+=4) if ((*vmemo_)[i]==g){ d=(*vmemo_)[i+2]; q=(*vmemo_)[i+3]; return (*vmemo_)[i+1]; }
  }
  ++vdepth_;
  const int o=vops_;
  const gen r=vstep1(g,d,q);
  --vdepth_;
  if (vops_>o && g.type==_SYMB && vmemo_ && !is_undef(r)){ vmemo_->push_back(g); vmemo_->push_back(r); vmemo_->push_back(d); vmemo_->push_back(q); }
  return r;
}

// does g compute with vectors (a vector typed, a name holding one, a cross product, proj, scal...)?
static NOINL bool vecish(const gen & g){
  if (g.type==_IDNT) return isvec(ev(g));
  if (isvec(g)) return true;
  if (g.type==_VECT){ EACH(it,*g._VECTptr) if (vecish(*it)) return true; return false; }
  if (g.type!=_SYMB) return false;
  vecteur a;
  if (vcall(g,a) || is(g,at_cross)) return true;
  if (is(g,at_of)) return false; // (r(t): a function's value, not this)
  return vecish(arg(g));
}
// the names in g that hold vectors (u, v, w), in order
static NOINL void vnames(const gen & g,vecteur & ns){
  if (g.type==_IDNT){ if (!equalposcomp(ns,g) && isvec(ev(g))) ns.push_back(g); return; }
  if (g.type==_VECT){ EACH(it,*g._VECTptr) vnames(*it,ns); return; }
  if (g.type==_SYMB) vnames(arg(g),ns);
}
// the steps of a computation with vectors: the vectors named, then each part bottom up, then the
// parts' values put in what they make (|u|^2|v|^2-(u.v)^2)
static NOINL bool m3_comp(const gen & input); // (steps_m3c.inc)
static NOINL bool m3_steps(const gen & input){
  if (m3_comp(input)) return true; // (r'(t), the integral of r(t): component by component)
  if (cfindable(input)){ vops_=0; vdepth_=0; vtop_=0; vmsg_=0; vecteur memo; vmemo_=&memo; cfind(input,true); vmemo_=0; return true; }
  if (has_op(input,*at_limit) || has_op(input,*at_sum) ||
      has_op(input,*at_solve) || has_op(input,*at_sto) || is(input,at_equal) || !vecish(input)) return false;
  const size_t m=mark();
  vecteur ns;
  vnames(input,ns);
  if (!ns.empty()){
    std::string s;
    EACH(it,ns){ if (it!=ns.begin()) s+=","; s+=vprint(*it); s+="="; s+=vprint(ev(*it)); }
    VL(R_VGIVEN,s);
  }
  vops_=0; vdepth_=0; vtop_=0; vmsg_=0;
  gen d,q;
  vecteur memo; vmemo_=&memo;
  const gen r=vstep(input,d,q);
  vmemo_=0;
  if ((is_undef(r) && !vmsg_) || !vops_){ cut(m); return false; } // (a line's "skew": its steps stay)
  if (!vtop_ && !isvec(r) && r.type!=_IDNT){ // a combination of the parts: their values in, worked out
    const std::string sd=vprint(d),sq=vprint(q);
    if (sq!=sd) VL(R_VALL,sd+"="+sq);
    gen rs=r;
    if (has_op(r,*at_pow) || has_op(r,*at_sqrt)){ const gen n=normal(r,CTX); if (taille(n,200)<taille(r,200)) rs=n; }
    VL(R_VL,sd+"="+vprint(rs));
  }
  return true;
}
// the value of a problem with proj, scal, the angle or u = P+N (giac has none of them; its angle is
// geometry's), worked out by the steps' code; 0: none in g
bool steps_m3_answer(gen & g,const char ** msg,GIAC_CONTEXT){
  *msg=0;
  ctx_=contextptr; // (vecish's ev of a name below: |u| with u = (1,2,1) stored, read in the calculator's context)
  if (g.type!=_SYMB) return false;
  if (cfindable(g)){ // (|(1,2)+(a,b)| = |(a,b)| typed: an example of v, a and b found)
    std::vector<step_t> tmp;
    std::vector<step_t> * o=out_;
    gen x(identificateur("x")),* ov=var_;
    out_=&tmp; var_=&x; depth_=0; vops_=0; vdepth_=0; vtop_=0; vmsg_=0;
    vecteur memo; vmemo_=&memo;
    const gen r=cfind(g,false);
    vmemo_=0; out_=o; var_=ov;
    if (is_undef(r)){ *msg=cm_?cm_:"no solution"; return true; }
    g=r; return true;
  }
  if ((is(g,at_sto) || is(g,at_equal)) && arg(g).type==_VECT && arg(g)._VECTptr->size()==2){ // (w:=7u/|u|: the value worked out, still stored)
    const bool st=is(g,at_sto);
    gen v=arg(g,st?0:1);
    const gen n=arg(g,st?1:0);
    if (n.type!=_IDNT || !steps_m3_answer(v,msg,contextptr)) return false;
    if (!*msg) g=st?symbolic(at_sto,makesequence(v,n)):symbolic(at_equal,makesequence(n,v));
    return true;
  }
  { vecteur a; bool any=false;
    const vecteur ops=lop(g,at_of);
    EACH(it,ops) if (vcall(*it,a)) any=true;
    if (!any){ const vecteur ab=lop(g,at_abs); EACH(it,ab) if (vecish(arg(*it))) any=true; } // (|u|: giac's abs of a vector is each component's)
    if (!any && !vcall(g,a)) return false; }
  std::vector<step_t> tmp;
  std::vector<step_t> * o=out_;
  gen x(identificateur("x")),* ov=var_;
  ctx_=contextptr; out_=&tmp; var_=&x; depth_=0;
  vops_=0; vdepth_=0; vtop_=0; vmsg_=0;
  gen d,q;
  vecteur memo; vmemo_=&memo;
  const gen r=vstep(g,d,q);
  vmemo_=0;
  out_=o; var_=ov;
  if (vmsg_){ *msg=vmsg_; return true; }
  if (is_undef(r)){ *msg="Give each one two vectors of the same size"; return true; }
  g=r;
  return true;
}
// the problem's header on the steps page, in the course's notation (u x v, proj_v u, |u|)
static NOINL gen m3_problem(const gen & input){
  if (!vecish(input)) return 0;
  std::vector<step_t> tmp;
  std::vector<step_t> * o=out_;
  out_=&tmp; vops_=0; vdepth_=0; vtop_=0; vmsg_=0;
  gen d,q;
  vecteur memo; vmemo_=&memo;
  const gen r=vstep(input,d,q);
  vmemo_=0;
  out_=o;
  if (is_undef(r)) return 0;
  std::string h=vprint(d);
  if (h.size()>2 && h[0]=='(' && h[h.size()-1]==')' && d.type==_IDNT) h=h.substr(1,h.size()-2);
  return txt(h);
}
