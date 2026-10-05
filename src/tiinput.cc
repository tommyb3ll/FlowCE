// TI-style implicit multiplication pre-pass for the console line.
//
// The line is scanned once, left to right, and copied byte for byte; the only
// change is inserting '*' characters. Tokens:
//   blanks       space, tab: copied. The '*' goes right after L, before them.
//   line break   \n \r, and a // comment up to the line end: copied, and
//                nothing is inserted across them.
//   string       "..." with \ escapes: opaque, not a value.
//   number       [0-9]+(.[0-9]*)?([eE][+-]?[0-9]+)?, or .[0-9]+ followed by
//                the same exponent, or 0x1F, 0b101. So 1e5, 2.5E-3 and 3. are
//                one number, while 2e is the number 2 then the name e.
//   name         [A-Za-z_\x80-\xff][A-Za-z0-9_\x80-\xff]*  (UTF-8 bytes are
//                letters).
//   operator     := => -> == != <= >= **, or any other single character.
// A '*' is inserted between adjacent tokens L and R when L ends a value and R
// starts a value:
//   L ends a value:   a number, ')', ']', a postfix '!' (one that follows a
//                     value), or a name classified VALUE or UNKNOWN (for a
//                     cut name, the class of its last piece).
//   R starts a value: a number, '(', or any name except a KEYWORD.
// Exceptions (no '*'):
//   - name '(' when the name is a FUNCTION, or f, g or h and not a VALUE:
//     a function call.
//   - name '(' when the matching ')' is followed by ":=" (blanks allowed):
//     a definition such as f(x):=...
//   - names starting with '_' are units (5_m, 3_(m/s)) and never take part.
//   - number followed by number (2 3, 1.2.3).
// Cutting unknown names (only if split_unknown): a name of 2 to IM_MAXCUT
// ASCII letters, classified UNKNOWN, is cut into the fewest pieces. A piece is
// a single letter, a VALUE name, or a FUNCTION name, but a FUNCTION only as
// the last piece and only when '(' follows. Ties go to the longest first
// piece. Examples: xy -> x*y, 2pir -> 2*pi*r, xsin(x) -> x*sin(x).
// A cut has at most 3 pieces, and at most 2 if they are all single letters
// before '('; oo and inf are never pieces. Otherwise the name is far more
// likely one the tables do not know (expand(, csc(x), radius, foo(x)) than a
// product, so it stays whole, and an uncut name of 2+ letters before '(' is a
// call.
// An assigned name is never cut: name:=..., ...=>name, name(...):=...
// Cost: linear, plus at most len*(len-1)/2 classifier calls per cut name.
// Memory: one allocation for the result (2*size, an upper bound, so it never
// grows), plus up to two temporary ones when the line contains ":=".
#include "tiinput.h"

static const int IM_MAXCUT=32; // longer unknown names are never cut

static inline bool im_digit(char c){ return c>='0' && c<='9'; }
static inline bool im_alpha(char c){ return (c>='a' && c<='z') || (c>='A' && c<='Z'); }
static inline bool im_hexdigit(char c){ return im_digit(c) || (c>='a' && c<='f') || (c>='A' && c<='F'); }
static inline bool im_namestart(char c){ return im_alpha(c) || c=='_' || (unsigned char)c>=0x80; }
static inline bool im_namechar(char c){ return im_namestart(c) || im_digit(c); }
static inline bool im_blank(char c){ return c==' ' || c=='\t'; }
static inline bool im_eol(char c){ return c=='\n' || c=='\r'; }

// end of the string literal starting at s[i]=='"' (n if unterminated)
static int im_strend(const char * s,int i,int n){
  for (++i;i<n;++i){
    if (s[i]=='\\')
      ++i; // skip the escaped character
    else if (s[i]=='"')
      return i+1;
  }
  return n;
}

// end of the line that contains s[i]
static int im_eolpos(const char * s,int i,int n){
  while (i<n && !im_eol(s[i]))
    ++i;
  return i;
}

// end of the number starting at s[i] (a digit, or '.' followed by a digit)
static int im_numend(const char * s,int i,int n){
  if (s[i]=='0' && i+2<n){
    const char c=s[i+1];
    if ((c=='x' || c=='X') && im_hexdigit(s[i+2])){
      i+=2;
      while (i<n && im_hexdigit(s[i]))
        ++i;
      return i;
    }
    if ((c=='b' || c=='B') && (s[i+2]=='0' || s[i+2]=='1')){
      i+=2;
      while (i<n && (s[i]=='0' || s[i]=='1'))
        ++i;
      return i;
    }
  }
  while (i<n && im_digit(s[i]))
    ++i;
  if (i<n && s[i]=='.'){
    ++i;
    while (i<n && im_digit(s[i]))
      ++i;
  }
  if (i<n && (s[i]=='e' || s[i]=='E')){
    int j=i+1;
    if (j<n && (s[j]=='+' || s[j]=='-'))
      ++j;
    if (j<n && im_digit(s[j])){
      i=j;
      while (i<n && im_digit(s[i]))
        ++i;
    }
  }
  return i;
}

// end of the name starting at s[i]
static int im_nameend(const char * s,int i,int n){
  ++i;
  while (i<n && im_namechar(s[i]))
    ++i;
  return i;
}

// length of the operator at s[i]
static int im_oplen(const char * s,int i,int n){
  if (i+1<n){
    const char a=s[i],b=s[i+1];
    if (b=='=' && (a==':' || a=='=' || a=='!' || a=='<' || a=='>'))
      return 2; // := == != <= >=
    if (b=='>' && (a=='=' || a=='-'))
      return 2; // => ->
    if (a=='*' && b=='*')
      return 2;
  }
  return 1;
}

// Sets mk to a copy of s[0..n) where strings and comments are blanked out
// (as '"'), and where each '(' whose matching ')' is followed by blanks and
// ":=" (a definition f(x):=...) is replaced by 'D'.
// Linear: the parentheses are matched right to left with a stack of flags
// "this ')' is followed by :=" kept in a string.
static void im_defmarks(const char * s,int n,std::string & mk){
  mk.reserve(n);
  int nclose=0;
  for (int i=0;i<n;){
    int j;
    if (s[i]=='"')
      j=im_strend(s,i,n);
    else if (s[i]=='/' && i+1<n && s[i+1]=='/')
      j=im_eolpos(s,i,n);
    else {
      if (s[i]==')')
        ++nclose;
      mk.push_back(s[i]);
      ++i;
      continue;
    }
    for (;i<j;++i)
      mk.push_back('"');
  }
  if (!nclose)
    return;
  std::string st;
  st.reserve(nclose);
  int sp=0;
  bool asg=false; // the next non-blank token on the right is ":="
  for (int i=n-1;i>=0;--i){
    const char c=mk[i];
    if (im_blank(c))
      continue;
    if (c==')'){
      if (sp<int(st.size()))
        st[sp]=char(asg);
      else
        st.push_back(char(asg));
      ++sp;
    }
    else if (c=='(' && sp>0){
      --sp;
      if (st[sp])
        mk[i]='D';
    }
    asg = c==':' && i+1<n && mk[i+1]=='=';
  }
}

static bool im_cuttable(const char * s,int len){
  if (len<2 || len>IM_MAXCUT)
    return false;
  for (int k=0;k<len;++k){
    if (!im_alpha(s[k]))
      return false;
  }
  return true;
}

// Appends the UNKNOWN name s[0..len) to out, cut into the fewest pieces with
// '*' between them. A piece is a letter, a VALUE name, or (only as the last
// piece, and only if fn) a FUNCTION name. Ties go to the longest first piece.
// Returns the offset of the last piece, or -1 (nothing appended) when the
// only cut is into single letters and the name is too long for that.
static int im_cut(const char * s,int len,ti_classify_fn classify,bool fn,std::string & out){
  // cnt[k]: fewest pieces for s[k..len), nxt[k]: end of the first of them
  unsigned char cnt[IM_MAXCUT+1],nxt[IM_MAXCUT+1];
  cnt[len]=0;
  for (int k=len-1;k>=0;--k){
    cnt[k]=0xff;
    nxt[k]=(unsigned char)(k+1);
    // longest pieces first, so that a tie keeps the longest; s[0..len) itself
    // is known to be UNKNOWN
    for (int e=(k?len:len-1);e>k;--e){
      if (cnt[e]+1>=cnt[k])
        continue; // cannot do better, no need to classify
      if (e>k+1){
        if ((e-k==2 && s[k]=='o' && s[k+1]=='o') || (e-k==3 && s[k]=='i' && s[k+1]=='n' && s[k+2]=='f'))
          continue; // infinity: foo, info are not f*oo, inf*o
        const int t=classify(s+k,e-k);
        if (t!=TI_NAME_VALUE && !(fn && e==len && t==TI_NAME_FUNCTION))
          continue;
      }
      cnt[k]=(unsigned char)(cnt[e]+1);
      nxt[k]=(unsigned char)e;
    }
  }
  if (cnt[0]>3 || (cnt[0]==len && fn && len>2))
    return -1;
  int k=0;
  for (;;){
    const int e=nxt[k];
    out.append(s+k,e-k);
    if (e>=len)
      return k;
    out+='*';
    k=e;
  }
}

std::string ti_implicit_mult(const std::string & line,ti_classify_fn classify,bool split_unknown){
  if (!classify)
    return line;
  const char * s=line.c_str();
  const int n=int(line.size());
  // definition marks, only needed if the line contains ":="
  bool defs=false;
  for (int i=0;i+1<n && !defs;++i)
    defs = s[i]==':' && s[i+1]=='=';
  std::string mk;
  if (defs)
    im_defmarks(s,n,mk);
  std::string out;
  // at most one '*' per boundary between two bytes: out never grows
  out.reserve(2*n+1);
  // the previous token, L
  enum { K_NONE, K_NUM, K_NAME, K_STORE, K_OTHER };
  int lk=K_NONE;
  bool lend=false;  // L ends a value
  bool lcall=false; // L is a name, and a '(' after it is a call or a definition
  for (int i=0;i<n;){
    const int ws=i; // blanks before the token, copied after a possible '*'
    while (i<n && im_blank(s[i]))
      ++i;
    if (i==n){
      out.append(s+ws,n-ws);
      break;
    }
    const char c=s[i];
    int j=i+1,k=K_OTHER;
    bool end=false;
    if (im_eol(c) || (c=='/' && i+1<n && s[i+1]=='/')){
      // line break or comment: copied, and nothing carries over
      if (!im_eol(c))
        j=im_eolpos(s,i,n);
      k=K_NONE;
    }
    else if (c=='"')
      j=im_strend(s,i,n);
    else if (im_digit(c) || (c=='.' && i+1<n && im_digit(s[i+1]))){
      j=im_numend(s,i,n);
      if (lend && lk!=K_NUM)
        out+='*';
      k=K_NUM;
      end=true;
    }
    else if (c=='_') // unit (5_m, 3_(m/s)): never takes part
      j=im_nameend(s,i,n);
    else if (im_namestart(c)){
      j=im_nameend(s,i,n);
      const int len=j-i,cls=classify(s+i,len);
      if (lend && cls!=TI_NAME_KEYWORD)
        out+='*';
      out.append(s+ws,i-ws);
      int q=j; // the token after the name
      while (q<n && im_blank(s[q]))
        ++q;
      const bool paren = q<n && s[q]=='(';
      const bool def = paren && defs && mk[q]=='D';
      const bool asg = q+1<n && s[q]==':' && s[q+1]=='=';
      int p=i,pcls=cls; // last piece of the name and its class
      const int k=(split_unknown && cls==TI_NAME_UNKNOWN && !def && !asg && lk!=K_STORE && im_cuttable(s+i,len))?im_cut(s+i,len,classify,paren,out):-1;
      if (k>=0){
        p=i+k;
        pcls=classify(s+p,j-p);
      }
      else
        out.append(s+i,len);
      lk=K_NAME;
      lend = pcls==TI_NAME_VALUE || pcls==TI_NAME_UNKNOWN;
      lcall = def || pcls==TI_NAME_FUNCTION ||
        (j-p==1 && (s[p]=='f' || s[p]=='g' || s[p]=='h') && pcls!=TI_NAME_VALUE) ||
        (j-p>1 && pcls==TI_NAME_UNKNOWN); // an uncut unknown name: a call
      i=j;
      continue;
    }
    else { // operator
      j=i+im_oplen(s,i,n);
      if (j==i+1){
        if (c=='(' && lend && !lcall)
          out+='*';
        end = c==')' || c==']' || (c=='!' && lend); // '!' after a value: factorial
      }
      else if (c=='=' && s[i+1]=='>')
        k=K_STORE;
    }
    out.append(s+ws,j-ws);
    lk=k;
    lend=end;
    lcall=false;
    i=j;
  }
  return out;
}
