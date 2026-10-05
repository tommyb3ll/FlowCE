// Host unit test for src/tiinput.cc (TI-style implicit multiplication).
// Build and run with: sh tests/host/run_tiinput_tests.sh
//
// Each case is (input, split_unknown, expected). Every case is also checked
// for two properties: the output is the input plus '*' characters only, and
// running the pass again changes nothing. The same properties are then
// checked on random lines made of tricky fragments.
#include <cstdio>
#include <cstring>
#include <stdint.h>
#include <string>
#include "tiinput.h"

// ---------------------------------------------------------------- fake classifier

static const char * const fake_functions[]={
  "sin","cos","tan","asin","acos","atan","sqrt","exp","ln","log","log10","abs",
  "simplify","factor","expand","normal","integrate","int","diff","solve","limit",
  "sum","seq","subst","ans","evalf","approx","tlin","trigcos","ratnormal","iegcd",
  "smod","resultant","max","min",
  "sec","csc","cot","sinh","cosh","tanh","asec","acsc","acot","asinh","acosh","atanh",
  0
};
static const char * const fake_userfns[]={
  "myf", // user-defined function
  0
};
static const char * const fake_values[]={
  "pi","e","i","infinity","inf","oo","undef",
  "area","v1", // user variables
  0
};
static const char * const fake_keywords[]={
  "if","then","else","elif","end","for","from","to","step","do","while","repeat",
  "until","return","local","and","or","not","xor","mod","in","of","div",
  "od","fi","break","continue",
  0
};

static bool in_list(const char * const * l,const char * s,int len){
  for (;*l;++l){
    if (int(strlen(*l))==len && memcmp(*l,s,len)==0)
      return true;
  }
  return false;
}

static bool is_namechar(unsigned char c){
  return (c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='_' || c>=0x80;
}

static int bad_calls=0; // calls on something that is not a name
static long ncalls=0;

static int fake_classify(const char * s,int len){
  ++ncalls;
  bool ok = s && len>0 && !(s[0]>='0' && s[0]<='9');
  for (int k=0;ok && k<len;++k)
    ok=is_namechar(s[k]);
  if (!ok){
    ++bad_calls;
    return TI_NAME_UNKNOWN;
  }
  if (in_list(fake_keywords,s,len)) return TI_NAME_KEYWORD;
  if (in_list(fake_functions,s,len)) return TI_NAME_FUNCTION;
  if (in_list(fake_userfns,s,len)) return TI_NAME_USERFN;
  if (in_list(fake_values,s,len)) return TI_NAME_VALUE;
  return TI_NAME_UNKNOWN;
}

// ---------------------------------------------------------------- cases

struct tcase { const char * in; bool split; const char * out; };

// \xCE\xB8 is theta, \xCF\x80 is pi in UTF-8
static const tcase cases[]={
  // required cases from the work order
  {"pi(x+1)",true,"pi*(x+1)"},
  {"(2/3)2^(3/2)",true,"(2/3)*2^(3/2)"},
  {"(x+1)(y-1)",true,"(x+1)*(y-1)"},
  {"2pir",true,"2*pi*r"},
  {"sin(x)cos(x)",true,"sin(x)*cos(x)"},
  {"xy+1",true,"x*y+1"},
  {"2x",true,"2*x"},
  {"2(x+1)",true,"2*(x+1)"},
  {"x(x+1)",true,"x*(x+1)"},
  {"f(x)",true,"f(x)"},
  {"f(x):=x^2",true,"f(x):=x^2"},
  {"area(r):=pi*r^2",true,"area(r):=pi*r^2"},
  {"abc:=5",true,"abc:=5"},
  {"5=>abc",true,"5=>abc"},
  {"myf(3)",true,"myf(3)"},
  {"area*2",true,"area*2"},
  {"simplify((x+1)^2)",true,"simplify((x+1)^2)"},
  {"integrate(pi*(x+1),x,-3*pi/4,pi/4)",true,"integrate(pi*(x+1),x,-3*pi/4,pi/4)"},
  {"seq(k^2,k,1,5)",true,"seq(k^2,k,1,5)"},
  {"l[2]",true,"l[2]"},
  {"5_m",true,"5_m"},
  {"1e5",true,"1e5"},
  {"2.5e-3x",true,"2.5e-3*x"},
  {"0x1F",true,"0x1F"},
  {"\"xy\"",true,"\"xy\""},
  {"x y",true,"x* y"},
  {"x mod 3",true,"x mod 3"},
  {"if x>0 then 1 else 2 end",true,"if x>0 then 1 else 2 end"},
  {"3!x",true,"3!*x"},
  {"x^2y",true,"x^2*y"},
  {"-xy",true,"-x*y"},
  {"sqrt(2)x",true,"sqrt(2)*x"},
  {"2sin(x)",true,"2*sin(x)"},
  {"(x+1)^2(x-1)",true,"(x+1)^2*(x-1)"},
  {"xy",false,"xy"},
  {"x(x+1)",false,"x*(x+1)"},
  {"diff(x^2,x)",true,"diff(x^2,x)"},
  {"e^x",true,"e^x"},
  {"2e^x",true,"2*e^x"},
  {"ab2",true,"ab2"},
  {"\xCE\xB8",true,"\xCE\xB8"},
  // splitting examples from the work order
  {"xy",true,"x*y"},
  {"abc",true,"a*b*c"},
  {"pir",true,"pi*r"},
  {"rpi",true,"r*pi"},
  {"xsin(x)",true,"x*sin(x)"},
  {"piy",true,"pi*y"},
  {"simplify",true,"simplify"},
  {"area",true,"area"},
  {"myf",true,"myf"},
  {"a(b+c)",true,"a*(b+c)"},
  // segmentation details
  {"ooo",true,"o*o*o"},                  // tie: longest first piece
  {"ooooo",true,"ooooo"},
  {"xinfinity",true,"x*infinity"},      // fewest pieces
  {"infx",true,"infx"},
  {"xasin(x)",true,"x*asin(x)"},        // asin beats a*sin
  {"xsinx",true,"xsinx"},               // FUNCTION only as the last piece: letters only, too long
  {"xsin",true,"xsin"},                 // ... and only before '('
  {"xsin (x)",true,"x*sin (x)"},
  {"xmyf(2)",true,"x*myf(2)"},
  {"myfx",true,"myfx"},
  {"areax",true,"area*x"},
  {"piarea(2)",true,"pi*area*(2)"},     // last piece is a VALUE: not a call
  {"XY",true,"X*Y"},
  {"2ab",true,"2*a*b"},
  {"abcdefghijklmnopqrstuvwxyzabcdef",true,"abcdefghijklmnopqrstuvwxyzabcdef"}, // 32 letters: > 3 pieces
  {"abcdefghijklmnopqrstuvwxyzabcdefg",true,"abcdefghijklmnopqrstuvwxyzabcdefg"}, // 33: not cut
  {"x_m",true,"x_m"},                   // '_' inside: not all letters
  {"v1",true,"v1"},
  {"2v1",true,"2*v1"},
  {"\xCF\x80r",true,"\xCF\x80r"},       // UTF-8: never cut
  {"2\xCF\x80",true,"2*\xCF\x80"},
  {"2\xCE\xB8",true,"2*\xCE\xB8"},
  // function calls (exception 1)
  {"g(2)",true,"g(2)"},
  {"h (t)",true,"h (t)"},
  {"2f(x)",true,"2*f(x)"},
  {"xf(x)",true,"x*f(x)"},
  {"f(x)g(x)",true,"f(x)*g(x)"},
  {"F(x)",true,"F*(x)"},                // only lowercase f g h
  {"e(x+1)",true,"e*(x+1)"},
  {"ab(x)",true,"a*b*(x)"},
  {"ab(x)",false,"ab(x)"},             // an uncut unknown name before '(' is a call
  {"xsin(x)",false,"xsin(x)"},
  // letters-only cuts: at most 3 letters, at most 2 before '(' (expand( was cut into e*x*p*a*n*d*()
  {"expandx((x+1)^3)",true,"expandx((x+1)^3)"},
  {"csc(x)",true,"csc(x)"},
  {"abc(x)",true,"abc(x)"},
  {"2abc(x)",true,"2*abc(x)"},
  {"xy(x+1)",true,"x*y*(x+1)"},
  {"pir(x)",true,"pi*r*(x)"},
  {"abc",true,"a*b*c"},
  {"2xyz",true,"2*x*y*z"},
  {"abcd",true,"abcd"},
  {"radius*2",true,"radius*2"},
  {"2radius",true,"2*radius"},
  {"foo(x)+1",true,"foo(x)+1"},
  {"foo (x)",true,"foo (x)"},
  {"sin(x)csc(x)",true,"sin(x)*csc(x)"},
  {"pivot(x)",true,"pivot(x)"},          // pi*v*o*t would be 4 pieces
  {"xpiy",true,"x*pi*y"},
  {"book*2",true,"book*2"},              // oo is not a piece
  {"2oo",true,"2*oo"},
  {"sin x",true,"sin x"},               // a FUNCTION does not end a value
  {"max(2x,3)",true,"max(2*x,3)"},
  {"ans()2",true,"ans()*2"},
  {"2ans()",true,"2*ans()"},
  {"xln(x)",true,"x*ln(x)"},
  // definitions and stores (exceptions 2 and 3)
  {"g(x):=2x",true,"g(x):=2*x"},
  {"abc(x):=x+1",true,"abc(x):=x+1"},
  {"area(r) := pi r^2",true,"area(r) := pi* r^2"},
  {"ab(\")\"):=1",true,"ab(\")\"):=1"}, // ')' in a string is not matched
  {"ab(x):=x;ab(2)",true,"ab(x):=x;a*b*(2)"},
  {"abc := 5",true,"abc := 5"},
  {"5 => abc",true,"5 => abc"},
  {"ab:=2cd",true,"ab:=2*c*d"},
  {"x^4-1=>*",true,"x^4-1=>*"},
  {"sin(x)^2=>cos",true,"sin(x)^2=>cos"},
  {"15_m=>_cm",true,"15_m=>_cm"},
  {"abc:=5",false,"abc:=5"},
  // units
  {"3_(m/s)",true,"3_(m/s)"},
  {"5_m*2x",true,"5_m*2*x"},
  // numbers
  {"0b101",true,"0b101"},
  {"0b101x",true,"0b101*x"},
  {"0x1Fy",true,"0x1F*y"},
  {".5x",true,".5*x"},
  {"3.x",true,"3.*x"},
  {"x.5",true,"x*.5"},
  {"2e",true,"2*e"},
  {"2ex",true,"2*e*x"},
  {"2e+x",true,"2*e+x"},
  {"2E5",true,"2E5"},
  {"2E-3x",true,"2E-3*x"},
  {"1.2.3",true,"1.2.3"},               // number number
  {"1..5",true,"1..5"},
  {"2 3",true,"2 3"},
  {"2i",true,"2*i"},
  {"3+4i",true,"3+4*i"},
  // postfix '!', brackets, quote
  {"n!",true,"n!"},
  {"x!y",true,"x!*y"},
  {"(2)!3",true,"(2)!*3"},
  {"3!(2)",true,"3!*(2)"},
  {"!x",true,"!x"},
  {"x!=y",true,"x!=y"},
  {"l[2]x",true,"l[2]*x"},
  {"[1,2]x",true,"[1,2]*x"},
  {"{1,2}x",true,"{1,2}x"},
  {"x[1]",true,"x[1]"},
  {"f'(x)",true,"f'(x)"},
  {"x->x^2",true,"x->x^2"},
  {"x<=y",true,"x<=y"},
  // strings and comments
  {"\"a\\\"b\"x",true,"\"a\\\"b\"x"},   // escaped quote
  {"\"a\\\"xy\"2x",true,"\"a\\\"xy\"2*x"}, // ... does not end the string
  {"\"a\\\\\"xy",true,"\"a\\\\\"x*y"},  // escaped backslash ends the string
  {"2\"x\"",true,"2\"x\""},
  {"\"abc",true,"\"abc"},               // unterminated
  {"2x // 2x",true,"2*x // 2x"},
  {"x // c",true,"x // c"},
  {"xy//xy\nxy",true,"x*y//xy\nx*y"},
  // keywords
  {"2x mod 3",true,"2*x mod 3"},
  {"return 2x",true,"return 2*x"},
  {"while x<5 do x:=2x od",true,"while x<5 do x:=2*x od"},
  {"for k from 1 to n do s:=s+k end",true,"for k from 1 to n do s:=s+k end"},
  {"x and not y",true,"x and not y"},
  {"a div b",true,"a div b"},
  // whitespace and line breaks
  {"2 x",true,"2* x"},
  {"x sin(x)",true,"x* sin(x)"},
  {"x\ty",true,"x*\ty"},
  {" 2x ",true," 2*x "},
  {"x  (x+1)",true,"x*  (x+1)"},
  {"(x+1) (x-1)",true,"(x+1)* (x-1)"},
  {"x + y",true,"x + y"},
  {"",true,""},
  {"x\ny",true,"x\ny"},
  {"2x\n3y",true,"2*x\n3*y"},
  {"2x\r\n3y",true,"2*x\r\n3*y"},
  // split_unknown=false keeps unknown names whole
  {"2pir",false,"2*pir"},
  {"pi r",false,"pi* r"},
  {"x y",false,"x* y"},
  {"f(x)",false,"f(x)"},
  {"sin(x)cos(x)",false,"sin(x)*cos(x)"},
  // larger expressions
  {"expand((x+1)(x-1))",true,"expand((x+1)*(x-1))"},
  {"normal(xy/y)",true,"normal(x*y/y)"},
  {"diff(x^2y,y)",true,"diff(x^2*y,y)"},
  {"solve(x^2-4=0,x)",true,"solve(x^2-4=0,x)"},
  {"limit(sin(x)/x,x,0)",true,"limit(sin(x)/x,x,0)"},
  {"approx(2pi)",true,"approx(2*pi)"},
  {"x^-1y",true,"x^-1*y"},
  {"1/2x",true,"1/2*x"},
  {"(1)(2)(3)",true,"(1)*(2)*(3)"},
  {"x(y)(z)",true,"x*(y)*(z)"},
};

// ---------------------------------------------------------------- helpers

// quoted, with control and non-ASCII bytes escaped
static std::string show(const std::string & s){
  std::string r="\"";
  for (size_t k=0;k<s.size();++k){
    const unsigned char c=s[k];
    if (c=='\n') r+="\\n";
    else if (c=='\r') r+="\\r";
    else if (c=='\t') r+="\\t";
    else if (c=='"' || c=='\\'){ r+='\\'; r+=char(c); }
    else if (c<0x20 || c>=0x7f){
      char b[8];
      snprintf(b,sizeof b,"\\x%02X",c);
      r+=b;
    }
    else r+=char(c);
  }
  r+='"';
  return r;
}

// out is in plus some '*' characters
static bool only_stars_added(const std::string & in,const std::string & out){
  size_t i=0;
  for (size_t j=0;j<out.size();++j){
    if (i<in.size() && out[j]==in[i])
      ++i;
    else if (out[j]!='*')
      return false;
  }
  return i==in.size();
}

// returns an empty string if all properties hold, else what failed
static const char * check_properties(const std::string & in,const std::string & out,bool split){
  if (!only_stars_added(in,out))
    return "changed something other than inserting '*'";
  if (out.size()>(in.empty()?0:2*in.size()-1))
    return "output longer than 2*size-1";
  if (ti_implicit_mult(out,fake_classify,split)!=out)
    return "not idempotent";
  return "";
}

static uint32_t rng_state=20261005u;
static unsigned rnd(unsigned m){
  rng_state=rng_state*1103515245u+12345u;
  return (rng_state>>16)%m;
}

// fragments for random lines
static const char * const frags[]={
  "x","y","ab","pir","pi","e","i","f","g","h","sin","cos(","myf","area","v1","oo",
  "inf","xsin","_m","_","2","3.5",".5","1e5","2e","0x1F","0b1","0","(",")","[","]",
  "{","}",",",";","+","-","*","/","^","!","'",":=","=>","->","==","!=","<=",">=",
  "**",":","=","<",">","..",".","$","%","&","|","~","?","@","#","\\"," ","  ","\t",
  "\n","\r","\"s\"","\"a\\\"b\"","\"","// c\n","//","mod","if","then","and","in",
  "\xCE\xB8","\xCF\x80","\xC3",
};

// ---------------------------------------------------------------- main

// ti_rewrite: math as written on paper
struct rcase { const char * in; const char * out; };
static const rcase rcases[]={
  {"f(x)=x^2","f(x):=x^2"},
  {"f(x) = x^2+1","f(x) := x^2+1"},
  {"g(x,y)=x*y","g(x,y):=x*y"},
  {"g( x , y )=x","g( x , y ):=x"},
  {"myf(x)=2x","myf(x):=2x"},                 // redefining a user function
  {"sinx(t)=t","sinx(t):=t"},                 // the defined name is not rewritten
  {"f(x)=sinx","f(x):=sin(x)"},
  {"sin(x)=1/2","sin(x)=1/2"},                // builtin: an equation
  {"f(2)=5","f(2)=5"},
  {"f(x)==2","f(x)==2"},
  {"f(x)<=2","f(x)<=2"},
  {"f(x)=","f(x)="},
  {"a=5","a=5"},
  {"2f(x)=4","2f(x)=4"},
  {"f()=1","f()=1"},
  {"sin^2(x)","sin(x)^2"},
  {"sin^2x","sin(x)^2"},
  {"sin^2 x","sin(x)^2"},
  {"cos^2(2x)+sin^2(2x)","cos(2x)^2+sin(2x)^2"},
  {"sin^2(x)+cos^2(x)","sin(x)^2+cos(x)^2"},
  {"sec^2x","sec(x)^2"},
  {"sin^-1(x)","asin(x)"},
  {"tan^-1(1)","atan(1)"},
  {"sec^-1(x)","asec(x)"},
  {"ln^-1(x)","ln^-1(x)"},                    // no inverse name: unchanged
  {"sin^(2)(x)","sin(x)^(2)"},
  {"sin^2","sin^2"},
  {"sin^2(sinx)","sin(sin(x))^2"},
  {"sinx","sin(x)"},
  {"sin x","sin(x)"},
  {"sin 2x","sin(2x)"},
  {"sin2x","sin(2x)"},
  {"2sinxcosx","2sin(x)cos(x)"},
  {"sinxcosx","sin(x)cos(x)"},
  {"lnx","ln(x)"},
  {"ln2","ln(2)"},
  {"ln x+1","ln(x)+1"},
  {"sqrtx","sqrt(x)"},
  {"sinx^2","sin(x^2)"},
  {"sin x^2","sin(x^2)"},
  {"sin(x)","sin(x)"},
  {"sin(x)^2","sin(x)^2"},
  {"sin x+1","sin(x)+1"},
  {"cost","cos(t)"},
  {"sinpi","sin(pi)"},
  {"sin pi/2","sin(pi)/2"},
  {"exp x","exp(x)"},
  {"log10x","log10(x)"},
  {"sinh x","sinh(x)"},
  {"expand(x)","expand(x)"},
  {"xsin(x)","xsin(x)"},
  {"sin","sin"},
  {"sin sin x","sin sin(x)"},
  {"sin if","sin if"},
  {"\"sinx\"","\"sinx\""},
  {"5_m","5_m"},
  {"area","area"},
  {"x // sinx","x // sinx"},
};

int main(){
  const int ncases=int(sizeof(cases)/sizeof(cases[0]));
  int npass=0,nfail=0;
  for (unsigned k=0;k<sizeof(rcases)/sizeof(rcases[0]);++k){
    const std::string in(rcases[k].in),want(rcases[k].out);
    const int bad0=bad_calls;
    const std::string got=ti_rewrite(in,fake_classify);
    const char * why=got!=want?"wrong output":bad_calls!=bad0?"classifier called on a non-name":ti_rewrite(got,fake_classify)!=got?"not idempotent":"";
    const bool ok=!*why;
    printf("%s r%2u %s -> %s",ok?"PASS":"FAIL",k+1,show(in).c_str(),show(got).c_str());
    if (!ok)
      printf("   [%s; expected %s]",why,show(want).c_str());
    printf("\n");
    if (ok) ++npass; else ++nfail;
  }
  for (int k=0;k<ncases;++k){
    const tcase & t=cases[k];
    const std::string in(t.in),want(t.out);
    const int bad0=bad_calls;
    const std::string got=ti_implicit_mult(in,fake_classify,t.split);
    const char * why="";
    if (got!=want)
      why="wrong output";
    else if (bad_calls!=bad0)
      why="classifier called on a non-name";
    else
      why=check_properties(in,got,t.split);
    const bool ok=!*why;
    printf("%s %3d %s %s -> %s",ok?"PASS":"FAIL",k+1,t.split?"split  ":"nosplit",show(in).c_str(),show(got).c_str());
    if (!ok)
      printf("   [%s; expected %s]",why,show(want).c_str());
    printf("\n");
    if (ok) ++npass; else ++nfail;
  }
  // random lines: properties only
  const int nrandom=20000;
  const int nfrags=int(sizeof(frags)/sizeof(frags[0]));
  int rfail=0;
  for (int r=0;r<nrandom;++r){
    std::string in;
    const unsigned len=1+rnd(14);
    for (unsigned k=0;k<len;++k)
      in+=frags[rnd(nfrags)];
    for (int sp=0;sp<2;++sp){
      const int bad0=bad_calls;
      const std::string got=ti_implicit_mult(in,fake_classify,sp!=0);
      const char * why=bad_calls!=bad0?"classifier called on a non-name":check_properties(in,got,sp!=0);
      if (!*why && sp){ // ti_rewrite: valid classifier calls, idempotent
        const std::string rw=ti_rewrite(in,fake_classify);
        if (bad_calls!=bad0)
          why="rewrite: classifier called on a non-name";
        else if (ti_rewrite(rw,fake_classify)!=rw)
          why="rewrite: not idempotent";
      }
      if (*why){
        if (++rfail<=20)
          printf("FAIL random %s %s -> %s   [%s]\n",sp?"split  ":"nosplit",show(in).c_str(),show(got).c_str(),why);
      }
    }
  }
  printf("random lines: %d lines x 2 modes checked (only '*' inserted, idempotent, valid classifier calls), %d failed\n",nrandom,rfail);
  printf("classifier calls: %ld\n",ncalls);
  printf("SUMMARY: %d cases, %d passed, %d failed; random property checks: %d failed -> %s\n",
         ncases+int(sizeof(rcases)/sizeof(rcases[0])),npass,nfail,rfail,(nfail||rfail)?"FAIL":"PASS");
  return (nfail||rfail)?1:0;
}
