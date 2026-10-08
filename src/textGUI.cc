#include "calc.h"
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include "menuGUI.h"
#include "textGUI.h"
#include "console.h"
#include "file.h"
#include "main.h"
#include <sys/lcd.h>
#if !defined std
#define std ustl
#endif
using namespace std;

//typedef scrollbar TScrollbar;
textArea * edptr=nullptr;
void displaylogo();
#define C24 24 // 24 on 90
#define C22 22
#define C19 19
#define C154 154
#define C6 6 // 6
#define C7 21

void swapint(int &a,int &b){
  const int t=a;
  a=b;
  b=t;
}

bool is_alphanum(char c){  return isalpha(c) || (c>='0' && c<='9');}

int get_line_number(const char * msg1,const char * msg2);


void clearLine(int x, int y,bool minimini) {
  // clear text line. x and y are text cursor coordinates
  // this is meant to achieve the same effect as using Printxy with a line full of spaces (except it doesn't waste strings).
  const int X=(minimini?4:6)*(x-1);
  const int width=LCD_WIDTH_PX-X;
  drawRectangle(X, (y-1)*C24, width, C24, _WHITE);
}

void drawScreenTitle(char* title) {
  if(title != nullptr)
    Printxy(0, 0, title, 1);
}

char tolower(char c)
{
  if (c >= 'A' && c <= 'Z')
    c += 32;
  return c;
}
char toupper(char c)
{
  if (c >= 'a' && c <= 'z')
    c -= 32;
  return c;
}
int strncasecmp_duplicate(const char *s1, const char *s2, size_t n)
{
  if (n != 0) {
    const Char *us1 = (const Char *)s1;
    const Char *us2 = (const Char *)s2;

    do {
      if (tolower(*us1) != tolower(*us2++))
        return (tolower(*us1) - tolower(*--us2));
      if (*us1++ == '\0')
        break;
    } while (--n != 0);
  }
  return (0);
}
char *strcasestr_duplicate(const char *s, const char *find)
{
  char c;

  if ((c = *find++) != 0) {
    c = tolower((Char)c);
    const size_t len = strlen(find);
    do {
      char sc;
      do {
        if ((sc = *s++) == 0)
          return (nullptr);
      } while ((char)tolower((Char)sc) != c);
    } while (strncasecmp(s, find, len) != 0);
    s--;
  }
  return ((char *)s);
}


/* copy over the next token from an input string, WITHOUT
   skipping leading blanks. The token is terminated by the
   first appearance of tokchar, or by the end of the source
   string.

   The caller must supply sufficient space in token to
   receive any token, Otherwise tokens will be truncated.

   Returns: a pointer past the terminating tokchar.

   This will happily return an infinity of empty tokens if
   called with src pointing to the end of a string. Tokens
   will never include a copy of tokchar.

   A better name would be "strtkn", except that is reserved
   for the system namespace. Change to that at your risk.

   released to Public Domain, by C.B. Falconer.
   Published 2006-02-20. Attribution appreciated.
   Modified by gbl08ma not to skip blanks at the beginning.
*/

const Char *toksplit(const Char *src, /* Source of tokens */
                     char tokchar, /* token delimiting char */
                     Char *token, /* receiver of parsed token */
                     int lgh) /* length token can receive */
/* not including final '\0' */
{
  if (src) {
    while (*src && (tokchar != *src)) {
      if (lgh) {
        *token++ = *src;
        --lgh;
      }
      src++;
    }
    if (*src && (tokchar == *src)) src++;
  }
  *token = '\0';
  return src;
} /* toksplit */


int EndsIWith(const char *str, const char *suffix)
{
  if (!str || !suffix)
    return 0;
  const size_t lenstr = strlen(str);
  const size_t lensuffix = strlen(suffix);
  if (lensuffix >  lenstr)
    return 0;
  //return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
  return strncasecmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

// not really for strings, but anyway:
// based on http://dsss.be/w/c:memmem
// added case-insensitive functionality
void* memmem(char* haystack, int hlen, char* needle, int nlen, int matchCase) {
  if (nlen > hlen) return nullptr;
  int j=0;
  switch(nlen) { // we have a few specialized compares for certain needle sizes
  case 0: // no needle? just give the haystack
    return haystack;
  case 1: // just use memchr for 1-byte needle
    if(matchCase) return memchr(haystack, needle[0], hlen);
    else {
      void* lc = memchr(haystack, tolower(needle[0]), hlen);
      if(lc!= nullptr) return lc;
      else return memchr(haystack, toupper(needle[0]), hlen);
    }
  default: // generic compare for any other needle size
    // walk i through the haystack, matching j as long as needle[j] matches haystack[i]
    for (int i = 0; i<hlen-nlen+1; i++) {
      if (matchCase ? haystack[i]==needle[j] : tolower(haystack[i])==tolower(needle[j])) {
        if (j==nlen-1) { // end of needle and it all matched?  win.
          return haystack+i-j;
        } else { // keep advancing j (and i, implicitly)
          j++;
        }
      } else { // no match, rewind i the length of the failed match (j), and reset j
        i-=j;
        j=0;
      }
    }
  }
  return nullptr;
}

// convert a normal text string into a multibyte one where letters become their mini variants (F5 screen of the OS's character select dialog)
// dest must be at least double the size of orig.
void stringToMini(char* dest, char* orig) {
  const int len = strlen(orig);
  int dlen = 0;
  for (int i = 0; i < len; i++) {
    if((orig[i] >= 65 && orig[i] <= 90) || (orig[i] >= 97 && orig[i] <= 122)) { // A-Z a-z
      dest[dlen] = '\xe7';
      dlen++;
      dest[dlen] = orig[i];
    } else if((orig[i] >= 48 && orig[i] <= 57)) { // 0-9
      dest[dlen] = '\xe5';
      dlen++;
      dest[dlen] = orig[i]-48+208;
    } else if(orig[i] == '+') {
      dest[dlen] = '\xe5';
      dlen++;
      dest[dlen] = '\xdb';
    } else dest[dlen] = orig[i];
    dlen++;
  }
  dest[dlen] = '\0';
}

void fix_newlines(textArea * r_edptr){
  //dbg_printf("fix newlines %i %x %x\n",r_edptr->elements.size(),&r_edptr->elements[0],&r_edptr->elements[1]);
  r_edptr->elements[0].newLine=0;
  for (size_t i=1;i<r_edptr->elements.size();++i)
    r_edptr->elements[i].newLine=1;
  for (size_t i=0;i<r_edptr->elements.size();++i){
    string S=r_edptr->elements[i].s;
    if (S.size()>120)
      r_edptr->minimini=1;
    constexpr const int cut=240;
    if (r_edptr->longlinescut && S.size()>cut){
      // string too long, cut it, set font to minimini
      int j;
      for (j=(4*cut)/5;j>=(2*cut)/5;--j){
        if (!isalphanum(S[j]))
          break;
      }
      textElement elem; elem.newLine=1; elem.s=S.substr(j,S.size()-j);
      r_edptr->elements[i].s=S.substr(0,j);
      r_edptr->elements.insert(r_edptr->elements.begin()+i+1,elem);
    }
  }
  
}

int end_do_then(const std::string & s){
  // skip spaces from end
  int l=s.size(),i,i0;
  const char * ptr=s.c_str();
  for (i=l-1;i>0;--i){
    if (ptr[i]!=' '){
      if (ptr[i]==':' || ptr[i]=='{')
        return 1;
      if (ptr[i]=='}')
        return -1;
      break;
    }
  }
  if (i>0){
    for (i0=i;i0>=0;--i0){
      if (!isalphanum(ptr[i0]) && ptr[i0]!=';' && ptr[i0]!=':')
        break;
    }
    if (i>i0+2){
      if (ptr[i]==';')
        --i;
      if (ptr[i]==':')
        --i;
    }
    std::string keyw(ptr+i0+1,ptr+i+1);
    const char * str = keyw.c_str();
    if (strcmp(str,"faire")==0 || strcmp(str,"do")==0 || strcmp(str,"alors")==0 || strcmp(str,"then")==0)
      return 1;
    if (strcmp(str,"fsi")==0 || strcmp(str,"end")==0 || strcmp(str,"fi")==0 || strcmp(str,"od")==0 || strcmp(str,"ftantque")==0 || strcmp(str,"fpour")==0 || strcmp(str,"ffonction")==0 || strcmp(str,"ffunction")==0)
      return -1;
  }
  return 0;
}

void add(textArea *r_edptr,const std::string & s){
  //dbg_printf("add %s\n",s.c_str());
  int r=1;
  for (size_t i=0;i<s.size();++i){
    if (s[i]=='\n' || s[i]==char(0x9c))
      ++r;
  }  
  //dbg_printf("before add %i %i\n",r_edptr->elements.size(),r);
  r_edptr->elements.reserve(r_edptr->elements.size()+r);
  //dbg_printf("after add\n");
  textElement cur;
  cur.lineSpacing=0;
  for (size_t i=0;i<s.size();++i){
    char c=s[i];
    if (c==char(0x9c))
      c='\n';
    if (c!='\n'){
      if (c!=char(0x0d))
        cur.s += c;
      continue;
    }
    string tmp=string(cur.s.begin(),cur.s.end());
    cur.s.swap(tmp);
    //dbg_printf("add %i %s\n",r_edptr->elements.size(),cur.s.c_str());
    r_edptr->elements.push_back(cur);
    //dbg_printf("added %i %x\n",r_edptr->elements.size(),&r_edptr->elements[i]);
    ++r_edptr->line;
    cur.s="";
  }
  if (cur.s.size()){
    //dbg_printf("add %i %s\n",r_edptr->elements.size(),cur.s.c_str());
    r_edptr->elements.push_back(cur);
    //dbg_printf("added %i %x\n",r_edptr->elements.size(),&r_edptr->elements.back());
    ++r_edptr->line;
  }
  //dbg_printf("added %i\n",r_edptr->elements.size());
  fix_newlines(r_edptr);
}

int find_indentation(const std::string & s){
  size_t indent=0;
  for (;indent<s.size();++indent){
    if (s[indent]!=' ')
      break;
  }
  return indent;
}

string makestring(int n,char ch){
  string res;
  for (int i=0;i<n;++i)
    res+=ch;
  return res;
}

void add_indented_line(std::vector<textElement> & v,int & textline,int & textpos){
  // add line
  v.insert(v.begin()+textline+1,v[textline]);
  std::string & s=v[textline].s;
  int indent=find_indentation(s);
  if (!s.empty())
    indent += 2*end_do_then(s);
  // dbg_printf("indent %i\n",indent);
  //cout << indent << s << ":" << endl;
  if (indent<0)
    indent=0;
  v[textline+1].s=makestring(indent,' ')+s.substr(textpos,s.size()-textpos);
  v[textline+1].newLine=1;
  v[textline].s=s.substr(0,textpos);
  ++textline;
  v[textline].nlines=1; // will be recomputed by cursor moves
  textpos=indent;
}

void undo(textArea * text){
  if (text->undoelements.empty())
    return;
  swapint(text->line,text->undoline);
  swapint(text->pos,text->undopos);
  swapint(text->clipline,text->undoclipline);
  swapint(text->clippos,text->undoclippos);
  swap(text->elements,text->undoelements);
}

void set_undo(textArea * text){
  text->changed=true;
  text->undoelements=text->elements;
  text->undopos=text->pos;
  text->undoline=text->line;
  text->undoclippos=text->clippos;
  text->undoclipline=text->clipline;
}

void add_nl(textArea * text,const std::string & ins){
  std::vector<textElement> & v=text->elements;
  std::vector<textElement> w(v.begin()+text->line+1,v.end());
  v.erase(v.begin()+text->line+1,v.end());
  add(text,ins);
  for (size_t i=0;i<w.size();++i)
    v.push_back(w[i]);
  fix_newlines(text);
  text->changed=true;
}

void insert(textArea * text,const char * adds,bool indent){
  //dbg_printf("insert %s\n",adds);
  size_t n=strlen(adds),i=0;
  if (!n)
    return;
  set_undo(text);
  const int l=text->line;
  if (l<0 || l>=text->elements.size())
    return; // invalid line number
  std::string & s=text->elements[l].s;
  const int ss=int(s.size());
  int & pos=text->pos;
  if (pos>ss)
    pos=ss;
  std::string ins=s.substr(0,pos);
  for (;i<n;++i){
    if (adds[i]=='\n' || adds[i]==0x1e) {
      break;
    }
    else {
      if (adds[i]!=char(0x0d))
        ins += adds[i];
    }
  }
  if (i==n){ // no newline in inserted string
    s=ins+s.substr(pos,ss-pos);
    pos += n;
    return;
  }
  std::string S(adds+i+1);
  const int decal=ss-pos;
  S += s.substr(pos,decal);
  // cout << S << " " << ins << endl;
  s=ins;
  if (indent){
    pos=s.size();
    int debut=0;
    for (i=0;i<S.size();++i){
      if (S[i]=='\n' || S[i]==0x1e){
        add_indented_line(text->elements,text->line,pos);
        // cout << S.substr(debut,i-debut) << endl;
        text->elements[text->line].s += S.substr(debut,i-debut);
        pos = text->elements[text->line].s.size();
        debut=i+1;
      }
    }
    //cout << S << " " << debut << " " << i << S.c_str()+debut << endl;
    add_indented_line(text->elements,text->line,pos);
    text->elements[text->line].s += (S.c_str()+debut);
  }
  else 
    add_nl(text,S);
  pos = text->elements[text->line].s.size()-decal;
  fix_newlines(text);
}

int merged_size(const std::vector<textElement> & v){
  int l=0;
  for (size_t i=0;i<v.size();++i){
    l += v[i].s.size();
  }
  return l;
}

std::string merge_area(const std::vector<textElement> & v){
  std::string s;
  for (size_t i=0;i<v.size();++i){
    s += v[i].s;
    s += '\n';
  }
  return s;
}

void search_msg(){
  statuslinemsg(lang?"EXE: suivant, AC: annuler":"EXE: next, AC: cancel",COLOR_CYAN);
}  

int check_parse(const std::vector<textElement> & v,int python); // in main.cc

void show_status(textArea * text,const std::string & search,const std::string & replace){
  if (text->nostatus){
    text->nostatus=false;
    return;
  }
  if (text->editable && text->clipline>=0)
    statuslinemsg(lang?"curseur|F4: copier|annul":"move|F4:copy|cancel",COLOR_CYAN);
  else {
    std::string status;
    status += text->changed?"* ":"- ";
    // status += (xthetat?" t":" x");
    if (search.size()){
      status += "EXE: " + search;
      if (replace.size())
        status += "->"+replace;
    }
    else {
      if (text->editable){
        status += text->python?" Py ":" ";
        status += remove_path(text->filename);
        status += ' ';
        status += printint(text->line+1);
        status += '/';
        status += printint(text->elements.size());
      }
    }
    statuslinemsg(status.c_str());
  }
  statusflags();
  //DisplayStatusArea();    
}

int check_leave(textArea * text){
  if (text->editable && text->filename.size()){
    if (text->changed){
      // save or cancel?
      std::string tmp=text->filename;
      if (strcmp(tmp.c_str(),"session.py")==0){
        if (
            0 //confirm(lang?"Les modifs seront perdues":"Changes will be lost",lang?"F1: annuler, F5: tant pis":"F1: cancel, F5: confirm")==KEY_CTRL_F1
            )
          return 2;
        else
          return 0;
      }
      tmp += lang?" a ete modifie!":" was modified!";
      if (confirm(tmp.c_str(),lang?"F1: sauvegarder, F5: tant pis":"F1: save, F5: discard changes")==KEY_CTRL_F1){
        save_script(text->filename.c_str(),merge_area(text->elements));
        text->changed=false;
        return 1;
      }
      return 0;
    }
    return 1;
  }
  return 0;
}

void do_restart(){
  python_free();
}

void chk_restart(){
  drawRectangle(0, 18, LCD_WIDTH_PX, LCD_HEIGHT_PX-18, _WHITE);
  if (confirm(lang?"Conserver les variables?":"Keep variables?",lang?"F1: conserver,   F5: effacer":"F1: keep,   F5: erase")==KEY_CTRL_F5)
    do_restart();
}

void print(int &X,int&Y,const char * buf,int color,bool revert,bool fake,bool minimini);

bool match(textArea * text,int pos,int & line1,int & pos1,int & line2,int & pos2){
  line2=-1;line1=-1;
  int linepos=text->line;
  const std::vector<textElement> & v=text->elements;
  if (linepos<0 || linepos>=v.size()) return false;
  const std::string * s=&v[linepos].s;
  int ss=s->size();
  if (pos<0 || pos>=ss) return false;
  const char ch=(*s)[pos];
  int open1=0,open2=0,open3=0,inc=0;
  if (ch=='(' || ch=='['
      || ch=='{'
      ){
    line1=linepos;
    pos1=pos;
    inc=1;
  }
  if (
      ch=='}' ||
      ch==']' || ch==')'
      ){
    line2=linepos;
    pos2=pos;
    inc=-1;
  }
  if (!inc) return false;
  bool instring=false;
  for (;;){
    for (;pos>=0 && pos<ss;pos+=inc){
      if ((*s)[pos]=='"' && (pos==0 || (*s)[pos-1]!='\\'))
        instring=!instring;
      if (instring)
        continue;
      switch ((*s)[pos]){
      case '(':
        open1++;
        break;
      case '[':
        open2++;
        break;
      case '{':
        open3++;
        break;
      case ')':
        open1--;
        break;
      case ']':
        open2--;
        break;
      case '}':
        open3--;
        break;
      }
      if (open1==0 && open2==0 && open3==0){
        //char buf[128];sprintf(buf,"%i %i",pos_orig,pos);puts(buf);
        if (inc>0){
          line2=linepos; pos2=pos;
        }
        else {
          line1=linepos; pos1=pos;
        }
        return true;
      } // end if
    } // end for pos
    linepos+=inc;
    if (linepos<0 || linepos>=v.size())
      return false;
    s=&v[linepos].s;
    ss=s->size();
    pos=inc>0?0:ss-1;
  } // end for linepos
  return false;
}

std::string get_selection(textArea * text,bool erase){
  int sel_line1=-1,sel_line2=-1,sel_pos1,sel_pos2;
  const int clipline=text->clipline,clippos=text->clippos,textline=text->line,textpos=text->pos;
  //dbg_printf("select clip=%i,%i text=%i,%i\n",clipline,clippos,textline,textpos);
  if (clipline>=0){
    if (clipline<textline || (clipline==textline && clippos<textpos)){
      sel_line1=clipline;
      sel_line2=textline;
      sel_pos1=clippos;
      sel_pos2=textpos;
    }
    else {
      sel_line1=textline;
      sel_line2=clipline;
      sel_pos1=textpos;
      sel_pos2=clippos;
    }
  }
  std::string s(text->elements[sel_line1].s);
  if (erase){
    set_undo(text);
    text->line=sel_line1;
    text->pos=sel_pos1;
    text->elements[sel_line1].s=s.substr(0,sel_pos1)+text->elements[sel_line2].s.substr(sel_pos2,text->elements[sel_line2].s.size()-sel_pos2);
  }
  if (sel_line1==sel_line2){
    s=s.substr(sel_pos1,sel_pos2-sel_pos1);
    //dbg_printf("select %s\n",s.c_str());
    return s;
  }
  s=s.substr(sel_pos1,s.size()-sel_pos1)+'\n';
  const int sel_line1_=sel_line1;
  for (sel_line1++;sel_line1<sel_line2;sel_line1++){
    s += text->elements[sel_line1].s;
    s += '\n';
  }
  s += text->elements[sel_line2].s.substr(0,sel_pos2);
  if (erase)
    text->elements.erase(text->elements.begin()+sel_line1_+1,text->elements.begin()+sel_line2+1);
  //dbg_printf("select %s\n",s.c_str());
  return s;
}

void warn_python(int mode,bool autochange){
  if (mode==0)
    confirm(autochange?(lang?"Source en syntaxe Xcas detecte.":"Xcas syntax source code detected."):(lang?"Syntaxe Xcas.":"Xcas syntax."),"F1/F5: ok");
  if (mode==1){
    if (autochange)
      confirm(lang?"Passage en syntaxe Python.":"Setting Python syntax source.",lang?"avec ^=**, F1/F5: ok":"with ^=**, F1/F5:ok");
    else
      confirm(lang?"Syntaxe Python avec ^==**,":"Python syntax with ^==**,",lang?"python_compat(2): xor. F1: ok":"python_compat(2): xor. F1: ok");
  }
  if (mode==2){
    confirm(lang?"Syntaxe Python avec ^==xor":"Python syntax with ^==xor",lang?"python_compat(1): **. F1: ok":"python_compat(1): **. F1: ok");
  }
}

bool change_mode(textArea * text,int flag){
  return false;
}

void match_print(char * singleword,int delta,int X,int Y,bool match,bool minimini){
  // char buflog[128];sprintf(buflog,"%i %i %s               ",delta,(int)match,singleword);puts(buflog);
  const char ch=singleword[delta];
  singleword[delta]=0;
  print(X,Y,singleword,0,false,/* fake*/true,minimini);
  singleword[delta]=ch;
  char buf[4];
  buf[0]=ch;
  buf[1]=0;
  // inverted print: 
  int color;
  if (minimini)
    color=match?TEXT_COLOR_GREEN:TEXT_COLOR_RED;
  else
    color=match?COLOR_GREEN:COLOR_RED;
  print(X,Y,buf,color,true,/*fake*/false,minimini);
}


// isFirstDraw==1 don't draw anything, just compute totalTextY
// isFirstDraw==2 redraw current line only (assumes rest of screen still ok)
void textarea_disp(textArea * text,int & isFirstDraw,int & totalTextY,int & scroll,int & textY,bool minimini){
  bool editable=text->editable;
  int showtitle = !editable && (text->title != nullptr);
  std::vector<textElement> & v=text->elements;
  if (v.empty()) v.push_back(textElement());
  //if (!isFirstDraw) drawRectangle(text->x, 24, text->width, 18, COLOR_WHITE);
  // insure cursor is visible
  if (editable && isFirstDraw!=1){
    int linesbefore=0;
    for (int cur=0;cur<text->line;++cur){
      linesbefore += v[cur].nlines;
    }
    // line begin Y is at scroll+linesbefore*17, must be positive
    if (linesbefore*19+scroll<0){
      scroll = -19*linesbefore;
      if (isFirstDraw==2) isFirstDraw=0;
    }
    linesbefore += v[text->line].nlines;
    // after line Y is at scroll+linesbefore*17
    if (linesbefore*19+scroll>154){
      scroll = 154-19*linesbefore;
      if (isFirstDraw==2) isFirstDraw=0;
    }      
  }
  textY = scroll+(showtitle ? C24 : 0)+text->y; // C24 pixels for title (or not)
  int deltax=0;
  if (editable){
    if (v.size()<10){
      deltax=9;
    }
    else {
      if (v.size()<100)
        deltax=18;
      else
        deltax=27;
    }
  }
  int & clipline=text->clipline;
  int & clippos=text->clippos;
  int & textline=text->line;
  int & textpos=text->pos;
  if (textline<0) textline=0;
  if (textline>=text->elements.size())
    textline=text->elements.size()-1;
  if (textpos<0) textpos=0;
  if (textpos>text->elements[textline].s.size())
    textpos=text->elements[textline].s.size();
  //char bufpos[512];  sprintf(bufpos,"%i,%i:%i,%i       ",textpos,textline,text->elements[textline].s.size(),text->elements.size());  puts(bufpos);
  if (clipline>=0){
    if (clipline>=v.size())
      clipline=-1;
    else {
      if (clippos<0)
        clippos=0;
      if (clippos>=v[clipline].s.size())
        clippos=v[clipline].s.size()-1;
    }
  }
  int line1,line2,pos1=0,pos2=0;
  if (!match(text,text->pos,line1,pos1,line2,pos2) && line1==-1 && line2==-1)
    match(text,text->pos-1,line1,pos1,line2,pos2);
  //char bufpos[512];  sprintf(bufpos,"%i,%i:%i,%i       ",line1,pos1,line2,pos2);  puts(bufpos);
  for (int cur=0;cur < v.size();++cur) {
    bool do_printline=(isFirstDraw==0) || (isFirstDraw==2 && cur==textline);
    const char* src = v[cur].s.c_str();
    // dbg_printf("textarea_disp %i %s\n",cur,src);
    if (cur==0){
      int l=v[cur].s.size();
      if (l>=1 && src[0]=='#')
        change_mode(text,1); // text->python=true;
      if (l>=2 && src[0]=='/' && src[1]=='/')
        change_mode(text,0); // text->python=false;
      if (l>=8 && src[0]=='f' && (src[1]=='o' || src[1]=='u') && src[2]=='n' && src[3]=='c' && src[4]=='t' && src[5]=='i' && src[6]=='o' && src[7]=='n')
        change_mode(text,0); // text->python=false;
      if (l>=4 && src[0]=='d' && src[1]=='e' && src[2]=='f' && src[3]==' ')
        change_mode(text,1); // text->python=true;
      if (!isFirstDraw)
        drawRectangle(text->x, giacmax(STATUS_AREA_PX,text->y+18), text->width, LCD_HEIGHT_PX-18, COLOR_WHITE);
    }
    int textX=text->x;
    if(v[cur].newLine) {
      if (v[cur].lineSpacing>4) // avoid large skip
        v[cur].lineSpacing=4;
      textY=textY+text->lineHeight+v[cur].lineSpacing;
    }
    if (editable){
      char line_s[16];
      sprint_int(line_s,cur+1);
      textX=os_draw_string_small(textX,textY,TEXT_COLOR_PURPLE,TEXT_COLOR_WHITE,line_s,/*fake*/!do_printline); // PrintMiniMini(&textX, &textY, (Char *)line_s, 0, TEXT_COLOR_PURPLE, 0 );
    }
    textX=text->x+deltax;
    int tlen = v[cur].s.size();
    if (isFirstDraw && cur!=textline){
      // direct update textY and skip to next line
      int nlines= tlen*(minimini?5:7)/(LCD_WIDTH_PX-textX);
      v[cur].nlines=1+nlines;
      //dbg_printf("textarea_disp cur=%i textline=%i textY=%i nlines=%i\n",cur,textline,textY,nlines);
      textY += nlines*(text->lineHeight+v[cur].lineSpacing);
      //dbg_printf("textarea_disp after textY=%i\n",textY);
      if (isFirstDraw==1) 
        totalTextY = textY+(showtitle ? 0 : C24);
      else if (textY>LCD_HEIGHT_PX)
        break;
      continue;
    }
    char singleword[256]; // [tlen+32];
    // char* singleword = (char*)malloc(tlen+1); // because of this, a single text element can't have more bytes than malloc can provide
    //dbg_printf("textarea_disp cur=%i textline=%i\n",cur,textline);
    if (cur==textline){
      if (textpos<0 || textpos>tlen)
        textpos=tlen;
      if (tlen==0 && text->editable){ // cursor on empty line
        text->cursorx=textX;
        text->cursory=textY+16;
        //drawRectangle(textX,textY+16,3,16,COLOR_BLACK);
      }
      if (isFirstDraw==2){
        //dbg_printf("textarea_disp cur=%i\n",cur);
        drawRectangle(textX,giacmax(STATUS_AREA_PX,textY+STATUS_AREA_PX),LCD_WIDTH_PX-textX,text->lineHeight,COLOR_WHITE);
      }
    }
    bool chksel=false;
    int sel_line1,sel_line2,sel_pos1,sel_pos2;
    if (clipline>=0){
      if (clipline<textline || (clipline==textline && clippos<textpos)){
        sel_line1=clipline;
        sel_line2=textline;
        sel_pos1=clippos;
        sel_pos2=textpos;
      }
      else {
        sel_line1=textline;
        sel_line2=clipline;
        sel_pos1=textpos;
        sel_pos2=clippos;
      }
      chksel=(sel_line1<=cur && cur<=sel_line2);
    }
    const char * match1=nullptr; // matching parenthesis (or brackets?)
    const char * match2=nullptr;
    if (cur==line1)
      match1=v[cur].s.c_str()+pos1;
    else
      match1=nullptr;
    if (cur==line2)
      match2=v[cur].s.c_str()+pos2;
    else
      match2=nullptr;
    // if (cur==textline && !match(v[cur].s.c_str(),textpos,match1,match2) && !match1 && !match2) match(v[cur].s.c_str(),textpos-1,match1,match2);
    // char buf[128];sprintf(buf,"%i %i %i        ",cur,(int)match1,(int)match2);puts(buf);
    const char * srcpos=src+textpos;
    int miniminiv=v[cur].minimini;
    bool mini_=miniminiv==0?minimini:miniminiv==1;
    int couleur=v[cur].color;
    int nlines=1;
    bool linecomment=false;
    while (*src){
      const char * oldsrc=src;
      if ( (text->python && *src=='#') ||
           (!text->python && *src=='/' && *(src+1)=='/')){
        linecomment=true;
        couleur=4;
      }
      if (linecomment || !text->editable)
        src = (char*)toksplit((Char*)src, ' ', (Char*)singleword, mini_?50:35); //break into words; next word
      else { // skip string (only with delimiters " ")
        if (*src=='"'){
          for (++src;*src;++src){
            if (*src=='"' && *(src-1)!='\\')
              break;
          }
          if (*src=='"')
            ++src;
          int i=src-oldsrc;
          strncpy(singleword,oldsrc,i);
          singleword[i]=0;
        }
        else {
          size_t i=0;
          for (;*src==' ';++src){ // skip initial whitespaces
            ++i;
          }
          if (i==0){
            if (isalpha(*src)){ // skip keyword
              for (;i<35 && (isalphanum(*src) || *src=='_');++src){
                ++i;
              }
            }
            // go to next space or non alphabetic char
            for (;i<35 && *src;++i,++src){
              if (*src==' ' || (i && *src==',') || (text->python && *src=='#') || (!text->python && *src=='/' && *(src+1)=='/')|| *src=='"' || isalpha(*src))
                break;
            }
          }
          strncpy(singleword,oldsrc,i);
          singleword[i]=0;
          if (i==0){
            puts(src); // free(singleword);
            return; // return KEY_CTRL_F2;
          }
        } // end normal case
      } // end else linecomment case
      // take care of selection
      bool invert=false;
      if (chksel){
        if (cur<sel_line1 || cur>sel_line2)
          invert=false;
        else {
          int printpos1=oldsrc-v[cur].s.c_str();
          int printpos2=src-v[cur].s.c_str();
          if (cur==sel_line1 && printpos1<sel_pos1 && printpos2>sel_pos1){
            // cut word in 2 parts: first part not selected
            src=oldsrc+sel_pos1-printpos1;
            singleword[sel_pos1-printpos1]=0;
            printpos2=sel_pos1;
          }
          if (cur==sel_line2 && printpos1<sel_pos2 && printpos2>sel_pos2){
            src=oldsrc+sel_pos2-printpos1;
            singleword[sel_pos2-printpos1]=0;
            printpos2=sel_pos2;
          }
          // now singleword is totally unselected or totally selected
          // which one?
          if (cur==sel_line1){
            if (cur==sel_line2)
              invert=printpos1>=sel_pos1 && printpos2<=sel_pos2;
            else
              invert=printpos1>=sel_pos1;
          }
          else {
            if (cur==sel_line2)
              invert=printpos2<=sel_pos2;
            else
              invert=true;
          }
        }
      }
      //dbg_printf("%s\n",singleword);
      //check if printing this word would go off the screen, with fake PrintMini drawing:
      int temptextX = 0,temptextY=0;
      print(temptextX,temptextY,singleword,couleur,false,/*fake*/true,mini_);
      if(temptextX<text->width && temptextX + textX > text->width-6) {
        if (editable)
          os_draw_string_medium(textX,textY,COLOR_MAGENTA,COLOR_WHITE,">",/* fake*/!do_printline);//PrintMini(&textX, &textY, (Char*)"\xe6\x9b", 0x02, 0xFFFFFFFF, 0, 0, COLOR_MAGENTA, COLOR_WHITE, 1, 0);	  
        //time for a new line
        textX=text->x+deltax;
        textY=textY+text->lineHeight+v[cur].lineSpacing;
        ++nlines;
      } //else still fits, print new word normally (or just increment textX, if we are not "on stage" yet)
      if (textY>=-C24 && textY<LCD_HEIGHT_PX) {
        temptextX=textX;
        if (editable){
          couleur=linecomment?5:find_color(singleword);
          if (!mini_){ // translate colors, not the same for printingg
            if (couleur==1) couleur=COLOR_BLUE;
            if (couleur==2) couleur=COLOR_ORANGE;
            if (couleur==3) couleur=COLOR_BROWN;
            if (couleur==4) couleur=COLOR_MAGENTA;
            if (couleur==5) couleur=COLOR_GREEN;
          }
          //char ch[32];
          //sprint_int(ch,couleur);
          //puts(singleword); puts(ch);
        }
        if (linecomment || !text->editable || singleword[0]=='"')
          print(textX,textY,singleword,couleur,invert,/*fake*/!do_printline,mini_);
        else { // print two parts, commandname in color and remain in black
          char * ptr=singleword;
          if (isalpha(*ptr)){
            while (isalphanum(*ptr) || *ptr=='_')
              ++ptr;
          }
          char ch=*ptr;
          *ptr=0;
          print(textX,textY,singleword,couleur,invert,/*fake*/!do_printline,mini_);
          *ptr=ch;
          print(textX,textY,ptr,COLOR_BLACK,invert,/*fake*/!do_printline,mini_);
        }
        // ?add a space removed from token
        if( ((linecomment || !text->editable)?*src:*src==' ') || v[cur].spaceAtEnd){
          if (*src==' ')
            ++src;
          print(textX,textY," ",COLOR_BLACK,invert,/*fake*/!do_printline,mini_);
        }
        // ?print cursor, and par. matching
        if (editable){
          if (match1 && oldsrc<=match1 && match1<src && do_printline)
            match_print(singleword,match1-oldsrc,temptextX,textY,
                        line2!=-1,
                        // match2,
                        mini_);
          if (match2 && oldsrc<=match2 && match2<src && do_printline)
            match_print(singleword,match2-oldsrc,temptextX,textY,
                        line1!=-1,
                        //match1,
                        mini_);
        }
        if (editable && cur==textline){
          if (oldsrc<=srcpos && (srcpos<src || (srcpos==src && textpos==tlen))){
            if (textpos>=2 && v[cur].s[textpos-1]==' ' && v[cur].s[textpos-2]!=' ' && srcpos-oldsrc==strlen(singleword)+1){ // fix cursor position after space
              //char ch[512];
              //sprintf(ch,"%s %i %i %i %i",singleword,strlen(singleword),srcpos-oldsrc,textpos,v[cur].s[textpos-2]);
              //puts(ch);
              singleword[srcpos-oldsrc-1]=' ';
            }
            singleword[srcpos-oldsrc]=0;
            print(temptextX,temptextY,singleword,couleur,false,/*fake*/true,mini_);
            //drawLine(temptextX, textY+14, temptextX, textY-14, COLOR_BLACK);
            //drawLine(temptextX+1, textY+14, temptextX+1, textY-14, COLOR_BLACK);
            text->cursorx=temptextX-1;
            text->cursory=textY+16;
            // drawRectangle(temptextX-1,textY+16,3,16,COLOR_BLACK);
          }
        }
      } // end if testY visible
      else {
        textX += temptextX;
        if(*src || v[cur].spaceAtEnd) textX += 7; // size of a PrintMini space
      }
    }
    // free(singleword);
    v[cur].nlines=nlines;
    if (isFirstDraw==1) 
      totalTextY = textY+(showtitle ? 0 : C24);
    else if (textY>LCD_HEIGHT_PX)
      break;
  } // end main draw loop
  if (editable && isFirstDraw!=1){
    int keyflag = (Char)Setup_GetEntry(0x14);
    text_disp_menu(keyflag);
  }
  if (showtitle && isFirstDraw!=1) {
    // clearLine(1,1,false);
    drawScreenTitle((char*)text->title);
  }
  isFirstDraw=0;
#if 0
  int scrollableHeight = LCD_HEIGHT_PX-C24*(showtitle ? 2 : 1)-text->y;
  //draw a scrollbar:
  if(text->scrollbar) {
    TScrollbar sb;
    sb.I1 = 0;
    sb.I5 = 0;
    sb.indicatormaximum = totalTextY;
    sb.indicatorheight = scrollableHeight;
    sb.indicatorpos = -scroll;
    sb.barheight = scrollableHeight;
    sb.bartop = (showtitle ? C24 : 0)+text->y;
    sb.barleft = text->width - 6;
    sb.barwidth = 6;
    
    Scrollbar(&sb);
  }
#endif
}  

bool chk_replace(textArea * text,const std::string & search,const std::string & replace,int & totalTextY,int & scroll,int & textY){
  if (replace.size()){
    statuslinemsg(lang?"Remplacer? EXE ou N":"Replace? EXE or N",COLOR_CYAN);
  }
  else
    search_msg();
  for (;;){
    int key,isFirstDraw=0;
    textarea_disp(text,isFirstDraw,totalTextY,scroll,textY,text->minimini);
    ck_getkey(&key);
    if (key==KEY_CHAR_MINUS || key==KEY_CHAR_Y || key==KEY_CHAR_9 || key==KEY_CHAR_O || key==KEY_CTRL_EXE){
      if (replace.size()){
        set_undo(text);
        std::string & s = text->elements[text->line].s;
        s=s.substr(0,text->pos-search.size())+replace+s.substr(text->pos,s.size()-text->pos);
        search_msg();
      }
      return true;
    }
    if (key==KEY_CHAR_8 || key==KEY_CHAR_N || key==KEY_CTRL_EXIT){
      search_msg();
      return true;
    }
    if (key==KEY_CTRL_AC){
      return false;
    }
  }
}


bool move_to_word(textArea * text,const std::string & s,const std::string & replace,int & isFirstDraw,int & totalTextY,int & scroll,int & textY){
  if (!s.size())
    return false;
  int line=text->line,pos=text->pos;
  if (line>=text->elements.size())
    line=0;
  if (pos>=text->elements[line].s.size())
    pos=0;
  for (;line<text->elements.size();++line){
    const int p=text->elements[line].s.find(s,pos);
    if (p>=0 && p<text->elements[line].s.size()){
      text->line=line;
      text->clipline=line;
      text->clippos=p;
      text->pos=p+s.size();
      text->clipline=-1;
      return chk_replace(text,s,replace,totalTextY,scroll,textY);
    }
    pos=0;
  }
  for (line=0;line<text->line;++line){
    const int p=text->elements[line].s.find(s,0);
    if (p>=0 && p<text->elements[line].s.size()){
      text->line=line;
      text->clipline=line;
      text->clippos=p;
      text->pos=p+s.size();
      text->clipline=-1;
      return chk_replace(text,s,replace,totalTextY,scroll,textY);
    }
  }
  return false;
}

void textarea_help_insert(textArea * text,int exec){
  const string curs=text->elements[text->line].s.substr(0,text->pos);
  if (!curs.empty()){
    int b;
    const string adds=help_insert(curs.c_str(),b,exec);
    if (!adds.empty()){
      if (b>0){
        string & s=text->elements[text->line].s;
        if (b>text->pos)
          b=text->pos;
        if (b>s.size())
          b=s.size();
        s=s.substr(0,text->pos-b)+s.substr(text->pos,s.size()-text->pos);//+s.substr(b,s.size()-b);
      }
      insert(text,adds.c_str(),false);
    }
  }
}


// returns >=0 if we should exit, -1 if check_parse, -2 otherwise (full redraw), -3 otherwise (only cursor redraw)
int handle_key(textArea * text,int key,int keyflag,int & isFirstDraw,int & scroll,int & textY,int & totalTextY,std::string & search,std::string & replace){
  //dbg_printf("key=%i\n",key);
  std::vector<textElement> & v=text->elements;
  bool editable=text->editable;
  int showtitle = !editable && (text->title != nullptr);
  int scrollableHeight = LCD_HEIGHT_PX-C24*(showtitle ? 2 : 1)-text->y;
  int & clipline=text->clipline;
  int & clippos=text->clippos;
  int & textline=text->line;
  int & textpos=text->pos;
  if (key==KEY_CTRL_SETUP){
    text->minimini=!text->minimini;
    isFirstDraw=0;
    return -2;
  }
  //dbg_printf("handle_key=%i keyflag=%i\n",key,keyflag);
  if (editable){
    if ( (key==KEY_CHAR_FRAC || key=='\t') && clipline<0){
      if (textline==0) return -2;
      std::string & s=v[textline].s;
      std::string & prev_s=v[textline-1].s;
      int indent=find_indentation(s),prev_indent=find_indentation(prev_s);
      if (!prev_s.empty())
        prev_indent += 2*end_do_then(prev_s);
      int diff=indent-prev_indent; 
      if (diff>0 && diff<=s.size())
        s=s.substr(diff,s.size()-diff);
      if (diff<0)
        s=string(-diff,' ')+s;
      textpos -= diff;
      return -2;
    }
    if (key==KEY_CTRL_VARS){
      insert(text,select_var(),true);
      return -2;
    }
    if (key==KEY_CHAR_ANS){
      displaylogo();  return -2; // parse is done by EXE
    }
    if (key==KEY_CTRL_CLIP) {
      if (clipline>=0){
        copy_clipboard(get_selection(text,false),true);
        clipline=-1;
      }
      else {
        clipline=textline;
        clippos=textpos;
      }
      return -2;
    }
    if (clipline<0){
      //dbg_printf("handle_key1=%i\n",key);
      const char * adds=nullptr;
      if ( (key>=KEY_CTRL_F1 && key<=KEY_CTRL_F4) ||
           key==KEY_CTRL_F6 ||
           (key >= KEY_CTRL_F7 && key <= KEY_CTRL_F15)
           ){
        string le_menu="F1 test\nif \nelse \n<\n>\n==\n!=\n&&\n||\nF2 loop\nfor \nfor in\nrange(\nwhile \nbreak\ndef\nreturn \n#\nF4 misc\nchar table\n:\n;\n=\n!\n%\nprint(\ninput(\n";
        if (original_cfg){
          string add((char *)original_cfg);
          int posf7=add.find("F6");
          if (posf7>=0 && posf7<add.size()){
            add=add.substr(posf7,add.size()-posf7);
            le_menu += add;
          }
        }
        const char * ptr=console_menu(key,(Char *)le_menu.c_str(),2);
        if (!ptr){
          return -2;
        }
        adds=ptr;
      }
      else if (key<32 || key>=127){
        adds=keytostring(key,keyflag,text->python);
        if (adds){
          insert(text,adds,key!=KEY_CTRL_PASTE);
          return -4;
        }
      }
      if (adds){
        bool isex=adds[0]=='\n';
        if (isex)
          ++adds;
        bool isif=strcmp(adds,"if ")==0,
          iselse=strcmp(adds,"else ")==0,
          isfor=strcmp(adds,"for ")==0,
          isforin=strcmp(adds,"for in")==0,
          isdef=(strcmp(adds,"f(x):=")==0 || strcmp(adds,"def")==0),
          iswhile=strcmp(adds,"while ")==0,
          islist=strcmp(adds,"list ")==0,
          ismat=strcmp(adds,"matrix ")==0;
#if 0
        if (islist){
          input_matrix(true);
          return -2;
        }
        if (ismat){
          input_matrix(false);
          return -2;
        }
#endif
        if (1 || text->python){
          if (isif)
            adds=isex?"if x<0:\nx=-x":"if :\n";
          if (iselse)
            adds="else:\n";
          if (isfor)
            adds=isex?"for j in range(10):\nprint(j*j)":"for  in range():\n";
          if (isforin)
            adds=isex?"for j in [1,4,9,16]:\nprint(j)":"for  in :\n";
          if (iswhile)
            adds=isex?"a,b=25,15\nwhile b!=0:\na,b=b,a%b":"while :\n";
          if (isdef)
            adds=isex?"def f(x):\nreturn x*x*x\n":"def f(x):\n\nreturn\n";
        } else {
          if (isif)
            adds=lang?(isex?"si x<0 alors x:=-x; fsi;":"si  alors\n\nsinon\n\nfsi;"):(isex?"if x<0 then x:=-x; fi;":"if  then\n\nelse\n\nfi;");
          if (lang && iselse)
            adds="sinon ";
          if (isfor)
            adds=lang?(isex?"pour j de 1 jusque 10 faire\nprint(j*j);\nfpour;":"pour  de  jusque  faire\n\nfpour;"):(isex?"for j from 1 to 10 do\nprint(j*j);\nod;":"for  from  to  do\n\nod;");
          if (isforin)
            adds=lang?(isex?"pour j in [1,4,9,16] faire\nprint(j)\nfpour;":"pour  in  faire\n\nfpour;"):(isex?"for j in [1,4,9,16] do\nprint(j);od;":"for  in  do\n\nod;");
          if (iswhile)
            adds=lang?(isex?"a,b:=25,15;\ntantque b!=0 faire\na,b:=b,irem(a,b);\nftantque;a;":"tantque  faire\n\nftantque;"):(isex?"a,b:=25,15;\nwhile b!=0 do\na,b:=b,irem(a,b);\nod;a;":"while  do\n\nod;");
          if (isdef)
            adds=lang?(isex?"fonction f(x)\nlocal j;\nj:=x*x;\nreturn j;\nffonction:;\n":"fonction f(x)\nlocal j;\n\nreturn ;\nffonction:;"):(isex?"function f(x)\nlocal j;\nj:=x*x;\nreturn j;\nffunction:;\n":"function f(x)\n  local j;\n\n return ;\nffunction:;");
        }
        //dbg_printf("insert 1 %s\n",adds);
        insert(text,adds,key!=KEY_CTRL_PASTE); // was true, but we should not indent when pasting
        //dbg_printf("insert 2 %s\n",adds);
        return -2;
      }
    }
  }
  textElement * ptr=& v[textline];
  //dbg_printf("handle_key2=%i\n",key);
  constexpr const int interligne=8;
  switch(key){
  case KEY_CTRL_DEL:
    if (clipline>=0){
      copy_clipboard(get_selection(text,true),true);
      // erase selection
      clipline=-1;
    }
    else {
      if (editable){
        if (textpos){
          set_undo(text);
          std::string & s=v[textline].s;
          bool fullredraw=(s.size()*(text->minimini?5:7)>LCD_WIDTH_PX-20);
          int nextpos=textpos-1;
          if (textpos==find_indentation(s)){
            for (int line=textline-1;line>=0;--line){
              int ind=find_indentation(v[line].s);
              if (textpos>ind){
                nextpos=ind;
                break;
              }
            }
          }
          s.erase(s.begin()+nextpos,s.begin()+textpos);
          textpos=nextpos;
          return fullredraw?-2:-3;
        }
        else {
          if (textline){
            set_undo(text);
            --textline;
            textpos=v[textline].s.size();
            v[textline].s += v[textline+1].s;
            v[textline].nlines += v[textline+1].nlines;
            v.erase(v.begin()+textline+1);
          }
        }
      }
    }
    break;
  case KEY_CHAR_CR:
    return -1;
  case KEY_CTRL_UNDO:
    undo(text);
    break;
  case KEY_CTRL_LEFT:
    if (editable){
      --textpos;
      if (textpos>=0)
        return -4;
      if (textpos<0){
        if (textline==0)
          textpos=0;
        else {
          --textline;
          textpos=v[textline].s.size();
        }
      }
      if (textpos>=0)
        return -3;
    }
  case KEY_CTRL_UP:
    if (editable){
      if (textline>0){
        --textline;
      }
      else {
        textline=0;
        textpos=0;
      }
      return -3;
    } else {
      if (scroll < 0) {
        scroll = scroll + interligne;
        if(scroll > 0) scroll = 0;
      }
      return -2;
    }
  case KEY_CTRL_RIGHT:
    ++textpos;
    if (textpos<=ptr->s.size())
      return -4;
    if (textline==v.size()-1){
      textpos=ptr->s.size();
      return -3;
    }
    textpos=0;
  case KEY_CTRL_DOWN:
    if (editable){
      if (textline<v.size()-1)
        ++textline;
      else {
        textline=v.size()-1;
        textpos=v[textline].s.size();
      }
      show_status(text,search,replace);
      return -3;
    }
    else {
      if (textY > scrollableHeight-(showtitle ? 0 : interligne)) {
        scroll = scroll - interligne;
        if(scroll < -totalTextY+scrollableHeight-(showtitle ? 0 : interligne))
          scroll = -totalTextY+scrollableHeight-(showtitle ? 0 : interligne);
      }
      return -2;
    }
  case KEY_CTRL_F1:
    if(text->allowF1) return KEY_CTRL_F1;
    break;
  case KEY_SHIFT_LEFT:
    textpos=0;
    return -3;
  case KEY_SHIFT_RIGHT:
    textpos=v[textline].s.size();
    return -3;
  case KEY_CTRL_PAGEDOWN:
    if (editable){
      textline=v.size()-1;
      show_status(text,search,replace);
      textpos=v[textline].s.size();
    }
    else {
      if (textY > scrollableHeight-(showtitle ? 0 : interligne)) {
        scroll = scroll - scrollableHeight;
        if(scroll < -totalTextY+scrollableHeight-(showtitle ? 0 : interligne))
          scroll = -totalTextY+scrollableHeight-(showtitle ? 0 : interligne);
      }
    }
    return -2;
  case KEY_CTRL_PAGEUP:
    if (editable){
      textline=0;
      show_status(text,search,replace);
    }
    else {
      //dbg_printf("pageup\n");
      if (scroll<0) {
        scroll = scroll + scrollableHeight;
        if (scroll>0) scroll = 0;
      }
    }
    return -2;
  case KEY_CTRL_EXE:
    //dbg_printf("handle_keyexe=%i\n",key);    
    if (text->allowEXE) return TEXTAREA_RETURN_EXE;
    if (search.size()){
      for (;;){ 
        if (!move_to_word(text,search,replace,isFirstDraw,totalTextY,scroll,textY))
          break;
      }
      show_status(text,search,replace);
      return -2;
    }
    else {
      if (clipline<0 && editable){
        set_undo(text);
        add_indented_line(v,textline,textpos);
      }
    }
    break;
  case KEY_CTRL_F5:	
    if (clipline<0 && text->editable && text->filename.size()){
      Menu smallmenu;
      smallmenu.numitems=11;
      MenuItem smallmenuitems[smallmenu.numitems];
      smallmenu.items=smallmenuitems;
      smallmenu.height=12;
      smallmenu.scrollbar=0;
      //smallmenu.title = "KhiCAS";
      smallmenuitems[0].text = (char*)(lang?"Tester syntaxe":"Check syntax");
      smallmenuitems[1].text = (char*)(lang?"Sauvegarder":"Save");
      smallmenuitems[2].text = (char*)(lang?"Sauvegarder comme":"Save as");
      smallmenuitems[3].text = (char*)(lang?"Inserer":"Insert");
      smallmenuitems[4].text = (char*)(lang?"Effacer":"Clear");
      smallmenuitems[5].text = (char*)(lang?"Chercher,remplacer":"Search, replace");
      smallmenuitems[6].text = (char*)(lang?"Aller a la ligne":"Goto line");
      smallmenuitems[7].type = MENUITEM_CHECKBOX;
      smallmenuitems[7].text = (char*)"Petite fonte";
      smallmenuitems[7].value = text->minimini;
      smallmenuitems[8].text = (char *)(lang?"Aide":"Help");
      smallmenuitems[9].text = (char *)"A propos";
      smallmenuitems[10].text = (char*)(lang?"Quitter":"Quit");
      int sres = doMenu(&smallmenu);
      show_status(text,search,replace);
      if(sres == MENU_RETURN_SELECTION) {
        sres=smallmenu.selection;
        if(sres==9 || sres==10) {
          textArea menu_text;
          menu_text.editable=false;
          menu_text.clipline=-1;
          menu_text.title = smallmenuitems[sres-1].text;
          add(&menu_text,smallmenu.selection==9?shortcuts_string:apropos_string);
          menu_text.minimini=false;
          doTextArea(&menu_text);
          return -2;
        }
        if (sres==1)
          return -1;
        if (sres==3){
          char filename[MAX_FILENAME_SIZE+1];
          if (get_filename(filename,".py")){
            text->filename=filename;
            sres=2;
          }
        }
        if(sres == 2) {
          string s(merge_area(v));
          //dbg_printf("textarea save %s",s.c_str());
          save_script(text->filename.c_str(),s);
          text->changed=false;
          string status=text->filename+" saved";
          statuslinemsg(status.c_str(),COLOR_CYAN);
          text->nostatus=true;
          //DisplayStatusArea();    	    
        }
        if (sres==4){
          char filename[MAX_FILENAME_SIZE+1];
          std::string ins;
          if (fileBrowser(filename, (char*)"*.py", (char *)"Scripts") && load_script(filename,ins))
            insert(text,ins.c_str(),false);//add_nl(text,ins);
          show_status(text,search,replace);
        }
        if (sres==5){
          if (merged_size(v)<get_free_memory()/8){
            std::string s(merge_area(v));
            copy_clipboard(s);
          }
          set_undo(text);
          textline=0;
          v.clear();
          add(text,"");
        }
        if (sres==6){
          textarea_disp(text,isFirstDraw,totalTextY,scroll,textY,text->minimini);
          search=get_searchitem(replace);
          if (!search.empty()){
            for (;;){
              if (!move_to_word(text,search,replace,isFirstDraw,totalTextY,scroll,textY)){
                break;
              }
            }
            show_status(text,search,replace);
          }
        }
        if (sres==7){
          textarea_disp(text,isFirstDraw,totalTextY,scroll,textY,text->minimini);
          int l=get_line_number(lang?"Negatif: en partant de la fin":"Negative: counted from the end",lang?"Numero de ligne:":"Line number:");
          if (l>0)
            text->line=l-1;
          if (l<0)
            text->line=v.size()+l;
        }
        if (sres==8){
          text->minimini=!text->minimini;
        }
        if (sres==11){
          int res=check_leave(text);
          if (res==2)
            return -2;
          return TEXTAREA_RETURN_EXIT;
        }
      }
    }
    break;
  case KEY_CTRL_F2:
    if (clipline<0)
      return KEY_CTRL_F2;
  case KEY_CTRL_EXIT: 
    if (!editable)
      return TEXTAREA_RETURN_EXIT;
    if (clipline>=0 || search.size()){
      clipline=-1;
      search="";
      return -2;
    }
    if (strcmp(text->filename.c_str(),"temp.py")==0)
      return TEXTAREA_RETURN_EXIT;
  case KEY_CTRL_PRGM: case KEY_CTRL_F16:{
    int res=check_leave(text);
    if (res==2) return -2;
    return TEXTAREA_RETURN_EXIT;
  }
  case KEY_CTRL_INS:
    break;
  default:
    //dbg_printf("handle_key3=%i\n",key);
    if (clipline<0 && key>=32 && key<128 && editable){
      int ret=text->changed?-4:-3;
      char buf[2]={char(key),0};
      insert(text,buf,false);
      std::string & s=v[textline].s;
      bool fullredraw=s.size()*(text->minimini?5:7)>LCD_WIDTH_PX-20;
      return fullredraw?-2:ret;
    }
    if (key==KEY_CTRL_AC){
      if (clipline>=0){
        clipline=-1;
      }
      else {
        if (search.size()){
          search="";
        }
        else {
          copy_clipboard(v[textline].s+'\n');
          if (v.size()==1)
            v[0].s="";
          else {
            v.erase(v.begin()+textline);
            if (textline>=v.size())
              --textline;
          }
          statuslinemsg((char*)("Line -> clipboard"),COLOR_CYAN);
          text->nostatus=true;
          // DisplayStatusArea();
        }
      }
    }
  }
  return -2;
}


int doTextArea(textArea* text) {
  int scroll = 0;
  int isFirstDraw = 1;
  int totalTextY = 0,textY=0;
  std::vector<textElement> & v=text->elements;
  std::string search,replace;
  show_status(text,search,replace);
  if (text->line>=v.size())
    text->line=0;
  textarea_disp(text,isFirstDraw,totalTextY,scroll,textY,text->minimini);
  while(1) {
    if (text->line>=v.size())
      text->line=0;
    if (isFirstDraw==3)
      isFirstDraw=2;
    else
      show_status(text,search,replace);
    textarea_disp(text,isFirstDraw,totalTextY,scroll,textY,text->minimini);
    if(text->type == TEXTAREATYPE_INSTANT_RETURN)
      return 0;
    // save rectangle under cursor and show cursor
    volatile unsigned char *ti8bpp_screen;
    unsigned char buf[2*16];
    const int taille=text->minimini?8:16;
    if (text->cursorx>=0 && text->cursory>=0 && text->cursorx<LCD_WIDTH_PX-2 && text->cursory<LCD_HEIGHT_PX-taille){
      ti8bpp_screen=(unsigned char *) lcd_Ram;
      ti8bpp_screen += text->cursorx+text->cursory*LCD_WIDTH_PX,*ti8bpp_screen;
      unsigned char * bufptr=buf,*bufend=buf+2*taille;
      for (;bufptr<bufend;bufptr+=2,ti8bpp_screen+=LCD_WIDTH_PX){
        bufptr[0]=ti8bpp_screen[0];
        bufptr[1]=ti8bpp_screen[1];
      }
    }
    drawRectangle(text->cursorx,text->cursory,2,taille,COLOR_BLACK);
    const unsigned kf=GetSetupSetting( (unsigned int)0x14);
    unsigned int key;
    //char keylog[32];sprint_int(keylog,key); puts(keylog);
    ck_getkey((int*)&key);
    while (key==KEY_CTRL_SHIFT || key==KEY_CTRL_ALPHA){
      const int keyflag = (Char)Setup_GetEntry(0x14);
      bool alph=keyflag==4||keyflag==0x84||keyflag==8||keyflag==0x88;
      console_disp_status(keyflag);
      ck_getkey((int*)&key);
    }
    // restore graph under cursor
    if (text->cursorx>=0 && text->cursory>=0 && text->cursorx<LCD_WIDTH_PX-2 && text->cursory<LCD_HEIGHT_PX-taille){
      ti8bpp_screen=(unsigned char *)lcd_Ram;
      ti8bpp_screen += text->cursorx+text->cursory*LCD_WIDTH_PX,*ti8bpp_screen;
      unsigned char * bufptr=buf,*bufend=buf+2*taille;
      for (;bufptr<bufend;bufptr+=2,ti8bpp_screen+=LCD_WIDTH_PX){
        ti8bpp_screen[0]=bufptr[0];
        ti8bpp_screen[1]=bufptr[1];
      }
    }
    // end cursor display/getkey
    if (key!=KEY_CTRL_PRGM && key!=KEY_CHAR_FRAC && key!=KEY_CTRL_MIXEDFRAC)
      key=translate_fkey(key);
    if (key==KEY_CTRL_F3){
      // CLIP/UNDO/PASTE
      Menu smallmenu;
      smallmenu.numitems=5;
      MenuItem smallmenuitems[smallmenu.numitems];
      smallmenu.items=smallmenuitems;
      smallmenu.height=smallmenu.numitems;
      smallmenu.scrollbar=0;
      //smallmenu.title = "KhiCAS";
      smallmenuitems[0].text = (char*)(lang?"Selection debut/fin":"Select begin/end");
      smallmenuitems[1].text = (char*)(lang?"Annuler [echanger]":"Undo");
      smallmenuitems[2].text = (char*)(lang?"Coller [inserer]":"Paste clipboard");
      smallmenuitems[3].text = (char*)(lang?"Supprimer ligne":"Cut line");
      smallmenuitems[4].text = (char*)(lang?"Saut de ligne [2nd enter]":"Newline [2nd enter]");
      int sres = doMenu(&smallmenu);
      if(sres == MENU_RETURN_SELECTION) {
        sres=smallmenu.selection;
        if (sres==1)
          key=KEY_CTRL_CLIP;
        else if (sres==2)
          key=KEY_CTRL_UNDO;
        else if (sres==3)
          key=KEY_CTRL_PASTE;
        else if (sres==4)
          key=KEY_CTRL_AC;
        else if (sres==5)
          key=KEY_CHAR_CR;
      }
      else
        continue;
    }
    if (key==KEY_CTRL_SYMB)
      key=KEY_CTRL_F4;
    if (!text->editable && ( (key>=KEY_CTRL_F1 && key<=KEY_CTRL_F4) || key==KEY_CTRL_F6 ||  key==KEY_CTRL_F7 || key==KEY_CTRL_F8 || (key>=KEY_CTRL_F11 && key<=KEY_CTRL_F13) ) )
      return key;
    const int ans=handle_key(text,key,kf,isFirstDraw,scroll,textY,totalTextY,search,replace);
    if (ans>=0)
      return ans;
    if (ans==-1){
      const int err=check_parse(v,text->python);
      if (err) // move cursor to the error line
        text->line=err-1;
    }
    if (ans==-4 || (ans==-3 && text->clipline<0) )
      isFirstDraw=-ans-1; // no redraw, just need to find cursor position
  }
}



int get_line_number(const char * msg1,const char * msg2){
  std::string s;
  int res=inputline(msg1,msg2,s,false);
  if (res==KEY_CTRL_EXIT)
    return 0;
  res=strtol(s.c_str(),nullptr,10);
  return res;
}
