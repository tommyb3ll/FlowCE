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
#include "focus.h"
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


void clearLine(int x, int y,bool minimini) {
  // clear text line. x and y are text cursor coordinates
  // this is meant to achieve the same effect as using Printxy with a line full of spaces (except it doesn't waste strings).
  const int X=(minimini?4:6)*(x-1);
  const int width=LCD_WIDTH_PX-X;
  drawRectangle(X, (y-1)*C24, width, C24, _WHITE);
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
  const int ind=find_indentation(s);
  if (textpos<=ind && ind<int(s.size())){ // at the start of the text: the line moves down, an empty line above
    s=s.substr(0,textpos);
    ++textline;
    v[textline].nlines=1;
    return;
  }
  int indent=ind+2*end_do_then(s.substr(0,textpos)); // a block opens if the text before the caret ends with : (do, then)
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


// the editing keys of the program editor (focus_edit.cc reads the keys, draws, and handles its own
// menus, the search and leaving). Returns >= 0 to leave (allowEXE: EXE), -1 to check the program
// (2nd enter), -2 after a change that can move other lines, -3/-4 when only the caret's line
// changed, -5 after a menu or card covered the screen.
int handle_key(textArea * text,int key,int keyflag){
  std::vector<textElement> & v=text->elements;
  int & clipline=text->clipline;
  int & clippos=text->clippos;
  int & textline=text->line;
  int & textpos=text->pos;
  if (key==KEY_CTRL_SETUP){
    text->minimini=!text->minimini;
    return -2;
  }
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
    return -5;
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
    if (key==KEY_CTRL_F6 || (key>=KEY_CTRL_F7 && key<=KEY_CTRL_F15)){ // 2nd and alpha F-keys (F6 is not next to F7): KhiCAS's menus, as Focus cards
      const char * adds=console_menu(key,2);
      if (adds && *adds){
        if (*adds=='\n')
          ++adds;
        insert(text,adds,true);
      }
      return -5;
    }
    if (key<32 || key>=127){
      const char * adds=keytostring(key,keyflag,text->python);
      if (adds){
        insert(text,adds,key!=KEY_CTRL_PASTE); // a paste keeps its own indentation
        return -4;
      }
    }
  }
  textElement * ptr=& v[textline];
  switch(key){
  case KEY_CTRL_DEL:
    if (clipline>=0){
      copy_clipboard(get_selection(text,true),true);
      clipline=-1;
    }
    else if (textpos){
      set_undo(text);
      std::string & s=v[textline].s;
      int nextpos=textpos-1;
      if (textpos==find_indentation(s)){ // in the indentation: back to the enclosing block's
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
      return -3;
    }
    else if (textline){ // at the start of a line: joined to the previous one
      set_undo(text);
      --textline;
      textpos=v[textline].s.size();
      v[textline].s += v[textline+1].s;
      v.erase(v.begin()+textline+1);
    }
    break;
  case KEY_CHAR_CR:
    return -1;
  case KEY_CTRL_UNDO:
    undo(text);
    break;
  case KEY_CTRL_LEFT:
    if (--textpos>=0)
      return -4;
    if (textline==0)
      textpos=0;
    else {
      --textline;
      textpos=v[textline].s.size();
    }
    return -3;
  case KEY_CTRL_UP:
    if (textline>0)
      --textline;
    else
      textpos=0;
    if (find_indentation(v[textline].s)==int(v[textline].s.size())) // an empty line of a block: after its indentation
      textpos=v[textline].s.size();
    return -3;
  case KEY_CTRL_RIGHT:
    ++textpos;
    if (textpos<=ptr->s.size())
      return -4;
    if (textline==v.size()-1){
      textpos=ptr->s.size();
      return -3;
    }
    textpos=0;
    // the next line's start
  case KEY_CTRL_DOWN:
    if (textline<v.size()-1)
      ++textline;
    else
      textpos=v[textline].s.size();
    if (find_indentation(v[textline].s)==int(v[textline].s.size()))
      textpos=v[textline].s.size();
    return -3;
  case KEY_SHIFT_LEFT:
    textpos=0;
    return -3;
  case KEY_SHIFT_RIGHT:
    textpos=v[textline].s.size();
    return -3;
  case KEY_CTRL_PAGEDOWN:
    textline=v.size()-1;
    textpos=v[textline].s.size();
    return -2;
  case KEY_CTRL_PAGEUP:
    textline=0;
    return -2;
  case KEY_CTRL_EXE:
    if (text->allowEXE) return TEXTAREA_RETURN_EXE;
    if (clipline<0){
      set_undo(text);
      add_indented_line(v,textline,textpos);
    }
    break;
  case KEY_CTRL_EXIT:
    if (clipline>=0){
      clipline=-1;
      return -2;
    }
    return TEXTAREA_RETURN_EXIT;
  default:
    if (clipline<0 && key>=32 && key<128){
      char buf[2]={char(key),0};
      insert(text,buf,false);
      return -4;
    }
    if (key==KEY_CTRL_AC){
      if (clipline>=0)
        clipline=-1;
      else { // the line to the clipboard
        copy_clipboard(v[textline].s+'\n');
        if (v.size()==1)
          v[0].s="";
        else {
          v.erase(v.begin()+textline);
          if (textline>=v.size())
            --textline;
        }
        statuslinemsg((char*)("Line -> clipboard"),COLOR_CYAN);
      }
    }
  }
  return -2;
}


// editable texts: the program editor (focus_edit.cc); the others (a command's help) to read
int doTextArea(textArea* text) {
  if (text->editable)
    return focus_edit(text);
  focus_text(text->title?text->title:"",merge_area(text->elements).c_str());
  return TEXTAREA_RETURN_EXIT;
}


