//#define DBG 0
#define WITH_LCDMALLOC 1
#define KHICAS_STACK 1

#include <string>
#include <stdlib.h>
#include <giac/giacPCH.h>
#include <giac/input_parser.h>
#include "calc.h"
#include <sys/lcd.h>
#include <ti/vars.h>
#include <ti/info.h>

#include "console.h"
#include "menuGUI.h"
#include "textGUI.h"
#include "file.h"
#include "main.h"
#include "focus.h"
#include <giac/kdisplay.h>
#if !defined std
#define std ustl
#endif
using namespace std;

#define EXPR_BUF_SIZE 256
#define GIAC_HISTORY_SIZE 2
#define GIAC_HISTORY_MAX_TAILLE 32

bool freeze=false,freezeturtle=false;
size_t pythonjs_stack_size=20*1024,pythonjs_heap_size=max_heap_size*3*1024/4;
static int xcas_python_eval=0;
char * pythonjs_static_heap=nullptr;
char * python_heap=nullptr;
extern giac::context * contextptr;
giac::context * contextptr=nullptr;
extern "C" int mp_token(const char * line);
extern "C" {
  extern int execution_in_progress_py;
  extern volatile int ctrl_c_py;
  //void set_abort_py();
  //void clear_abort_py();
}

#ifdef WITH_SHEET
void sheet(){
  xcas::sheet(contextptr);
}
#endif

void python_free(){
  if (!python_heap) return;
  mp_deinit();
  if (!pythonjs_static_heap){
    if ( ((size_t) python_heap)>1)
      free(python_heap);
  }
  python_heap=nullptr;
}

int python_init(int stack_size,int heap_size){
  python_free();
  // python_heap=micropy_init(8192,16384);
  //string s(("Heap "+print_INT_(heap_size/1024)+"K MicroPython not available"));
  //Console_Output((const Char *)s.c_str());
  //os_draw_string_medium(0,180,COLOR_BLACK,COLOR_WHITE,s.c_str(),false);
  //dConsolePut(s.c_str());
  //Console_NewLine(LINE_TYPE_OUTPUT,1);
  Console_Disp(1);
  python_heap=micropy_init(stack_size,heap_size);
  //confirm("heap","init");
  if (!python_heap)
    return 0;
  return 1;
}

int micropy_ck_eval(const char *line){
  freeze=false; freezeturtle=false;
  if (python_heap && line[0]==0)
    return 1;
  if (!python_heap){
    python_init(pythonjs_stack_size,pythonjs_heap_size);
  }
  if (!python_heap){
    return RAND_MAX;
  }
  ctrl_c_py=0;
  execution_in_progress_py = 1;
  const int res= micropy_eval(line);
  execution_in_progress_py = 0;
  if (ctrl_c_py & 1){
    confirm(lang?"Interrompu":"Interrupted","F1/F5: ok",""); // insure ON has been removed from keyboard buffer
  }  
  //while (1) { int key; ck_getkey(&key); if (key==KEY_CTRL_EXIT) break; }
  return res;
}

#if defined WITH_EQW && !defined FAKE_GIAC && !focus_on // (the matrix editor is the old equation editor)

const char * input_matrix(const giac::gen &g,giac::gen & ge,const giac::context *){
  if (ge.type==giac::_VECT)
    ge.subtype=0;
  static string input_matrix_s=g.print(contextptr)+'='+ge.print(contextptr);
  return input_matrix_s.c_str();
}    
  
const char * input_matrix(bool list){
  static std::string * sptr=nullptr;
  if (!sptr)
    sptr=new std::string;
  *sptr="";
  giac::gen v(giac::_VARS(0,contextptr));
  giac::vecteur w;
  if (v.type==giac::_VECT){
    for (size_t i=0;i<v._VECTptr->size();++i){
      giac::gen & tmp = (*v._VECTptr)[i];
      if (tmp.type==giac::_IDNT){
	giac::gen tmpe(giac::eval(tmp,1,contextptr));
	if (list){
	  if (tmpe.type==giac::_VECT && !giac::ckmatrix(tmpe))
	    w.push_back(tmp);
	}
	else {
	  if (ckmatrix(tmpe))
	    w.push_back(tmp);
	}
      }
    }
  }
  std::string msg;
  if (w.empty())
    msg=lang?"Creer nouveau":"Create new";
  else
    msg=((lang?"Creer nouveau ou editer ":"Create new or edit ")+(w.size()==1?w.front():giac::gen(w,giac::_SEQ__VECT)).print(contextptr));
  lock_alpha();
  if (inputline(msg.c_str(),(lang?"Nom de variable:":"Variable name:"),*sptr,false) && !sptr->empty() && isalpha((*sptr)[0])){
    reset_kbd();
    giac::gen g(*sptr,contextptr);
    giac::gen ge(eval(g,1,contextptr));
    if (g.type==giac::_IDNT){
      if (ge.type==giac::_VECT){
	ge=xcas::eqw(ge,true);
	ge=giac::eval(ge,1,contextptr);
        freeze=giac::ctrl_c=giac::kbd_interrupted=giac::interrupted=false;
	return input_matrix(g,ge,contextptr);
      }
      if (ge==g || confirm_overwrite()){
	*sptr="";
	if (inputline((lang?"Nombre de lignes":"Line number"),"",*sptr,true)){
	  int l=strtol(sptr->c_str(),nullptr,10);
	  if (l>0 && l<256){
	    int c;
	    if (list)
	      c=0;
	    else {
	      std::string tmp(*sptr+(lang?" lignes.":" lines."));
	      *sptr="";
	      inputline(tmp.c_str(),lang?"Colonnes:":"Columns:",*sptr,true);
	      c=strtol(sptr->c_str(),nullptr,10);
	    }
	    if (c==0){
	      ge=giac::vecteur(l);
	    }
	    else {
	      if (c>0 && l*c<256)
		ge=giac::_matrix(giac::makesequence(l,c),contextptr);
	    }
	    ge=xcas::eqw(ge,true);
	    ge=giac::eval(ge,1,contextptr);
            freeze=giac::ctrl_c=giac::kbd_interrupted=giac::interrupted=false;
            reset_kbd();
	    if (ge.type==giac::_VECT)
	      return input_matrix(g,ge,contextptr);
	    return "";
	  } // l<256
	}
      } // ge==g || overwrite confirmed
    } // g.type==_IDNT
    else {
      invalid_varname();
    }	
  } // isalpha
  reset_kbd();
  return nullptr;
}
#else
const char * input_matrix(bool list){
  return 0;
}
#endif

int select_item(const char ** ptr,const char * title,bool askfor1){
  int nitems=0;
  for (const char ** p=ptr;*p;++p)
    ++nitems;
  if (nitems==0 || nitems>=256)
    return -1;
  if (!askfor1 && nitems==1)
    return 0;
  MenuItem smallmenuitems[nitems];
  for (int i=0;i<nitems;++i){
    smallmenuitems[i].text=(char *) ptr[i];
  }
  Menu smallmenu;
  smallmenu.numitems=nitems; 
  smallmenu.items=smallmenuitems;
  smallmenu.height=nitems>=11?12:nitems+1;
  smallmenu.scrollbar=1;
  smallmenu.scrollout=1;
  smallmenu.title = (char*) title;
  //MsgBoxPush(5);
  const int sres = doMenu(&smallmenu);
  //MsgBoxPop();
  if (sres!=MENU_RETURN_SELECTION && sres!=KEY_CTRL_EXE)
    return -1;
  return smallmenu.selection-1;
}

int fileBrowser(char * filename, const char * ext, const char * title){
  constexpr const int N=32;
  const char * filenames[N]={nullptr};
  //dbg_printf("fileBrowser ext=%s title=%s\n",ext,title);
  int res=os_file_browser(filenames,N,ext,2);
  if (res<=0 || res>=N)
    return 0;
  res=select_item(filenames,title,/* ask for even if 1 file avail*/ true);
  //dbg_printf("fileBrowser %i\n",res);
  if (res<0)
    return 0;
  strcpy(filename,filenames[res]);
  //dbg_printf("fileBrowser %s\n",filename);
  return 1;
}

int process_freeze(){
  if (freezeturtle){
    displaylogo();
    freezeturtle=false;
    return 1;
  }
  if (freeze){
    freeze=false;
    int key;
    ck_getkey(&key);
    return 1;
  }
  return 0;
}    


// extern U ** mem;
// extern unsigned int **free_stack;

// #define SYMBOLSSTATEFILE (char*)"\\\\fls0\\lastvar.py"
  
static char varbuf[128];
const char * select_var(){
#ifdef FAKE_GIAC
  int freemem=(int)malloc(0xffffff);
  dbg_printf("malloc free=%i\n",freemem);
  return "";
#else
  giac::gen g(giac::_VARS(0,contextptr));
  if (g.type!=giac::_VECT)
    return "";
  giac::vecteur & v=*g._VECTptr;
  MenuItem smallmenuitems[v.size()+4];
  vector<std::string> vs(v.size()+1);
  int i,total=0;
  constexpr const char typ[]="idzDcpiveSfEsFRmuMwgPF";
  for (i=0;i<v.size();++i){
    vs[i]=v[i].print(contextptr);
    if (v[i].type==giac::_IDNT){
      giac::gen w;
      v[i]._IDNTptr->in_eval(0,v[i],w,contextptr,true);
      //vector<int> vi(9); tailles(w,vi); total += vi[8]; vs[i] += " ~"; vs[i] += giac::print_INT_(vi[8]);
      vs[i] += ',';
      vs[i] += typ[w.type];
    }
    smallmenuitems[i].text=(char *) vs[i].c_str();
  }
  // total += sizeof(giac::context)+contextptr->tabptr->capacity()*(sizeof(const char *)+sizeof(giac::gen)+8)+bytesize(giac::history_in(contextptr))+bytesize(giac::history_out(contextptr));
  vs[i]="purge(~"+giac::print_INT_(total)+')';
  smallmenuitems[i].text=(char *)vs[i].c_str();
  smallmenuitems[i+1].text=(char *)"assume(";
  smallmenuitems[i+2].text=(char *)"restart ";
  smallmenuitems[i+3].text=(char *)"VARS()";
  Menu smallmenu;
  smallmenu.numitems=v.size()+4; 
  smallmenu.items=smallmenuitems;
  smallmenu.height=12;
  smallmenu.scrollbar=1;
  smallmenu.scrollout=1;
  int freemem=(int)malloc(0xffffff);
  string title=("Variables, "+giac::print_INT_(freemem/1024)+" KB free");
  smallmenu.title = (char*) title.c_str();
  //MsgBoxPush(5);
  int sres = doMenu(&smallmenu);
  //MsgBoxPop();
  if (smallmenu.selection && smallmenu.selection<=v.size() && (sres==MENU_RETURN_SELECTION || sres==KEY_CTRL_DEL)){
    g=v[smallmenu.selection-1];
    if (sres==KEY_CTRL_DEL)
      g=giac::symbolic(giac::at_purge,g);
    strcpy(varbuf,g.print(contextptr).c_str());
    return varbuf;
  }
  if (sres==MENU_RETURN_SELECTION){
    if (smallmenu.selection==1+v.size())
      return "purge(";
    if (smallmenu.selection==2+v.size())
      return "assume(";
    if (smallmenu.selection==3+v.size())
      return "restart";
    if (smallmenu.selection==4+v.size())
      return "VARS()";
  }
  return "";  
#endif
}  


//const char * keywords[]={"do","faire","for","if","return","while"}; // added to lexer_tab_int.h

  constexpr char const * const python_keywords[] = {   // List of known giac keywords...
    "False",
    "None",
    "True",
    "and",
    "break",
    "continue",
    "def",
    "default",
    "elif",
    "else",
    "except",
    "for",
    "from",
    "global",
    "if",
    "import",
    "not",
    "or",
    "return",
    "throw",
    "try",
    "while",
    "xor",
    "yield",
  };
  constexpr char const * const python_builtins[]={
    "NoneType",
    "__call__",
    "__class__",
    "__delitem__",
    "__dir__", 
    "__enter__",
    "__exit__",
    "__getattr__",
    "__getitem__",
    "__hash__",
    "__init__",
    "__int__",
    "__iter__",
    "__len__",
    "__main__",
    "__module__",
    "__name__",
    "__new__",
    "__next__",
    "__qualname__",
    "__repr__",
    "__setitem__",
    "__str__",
    "abs",
    "all",
    "any",
    "append",
    "args",
    "bool",
    "builtins",
    "bytearray",
    "bytecode",
    "bytes",
    "callable",
    "chr",
    "classmethod",
    "complex",
    "dict",
    "dir",
    "divmod",
    "eval",
    "exec",
    "float",
    "format",
    "getattr",
    "globals",
    "hasattr",
    "hash",
    "hex",
    "id",
    "index",
    "input",
    "int",
    "iter",
    "len",
    "list",
    "locals",
    "map",
    "max",
    "min",
    "next",
    "object",
    "oct",
    "pow",
    "print",
    "range",
    "repr",
    "reversed",
    "round",
    "self",
    "set",
    "setattr",
    "sorted",
    "split",
    "str",
    "sum",
    "super",
    "tuple",
    "type",
    "zip",
  };

  int dichotomic_search(const char * const * tab,unsigned tab_size,const char * s){
    int beg=0,end=tab_size;
    // string index is always >= begin and < end
    for (;;){
      int cur = (beg + end) / 2;
      int test = strcmp(s, tab[cur]);
      if (!test)
	return cur;
      if (cur==beg)
	return -1;
      if (test>0)
	beg=cur;
      else
	end=cur;
    }
    return -1;
  }

  bool is_python_keyword(const char * s){
    return dichotomic_search(python_keywords,sizeof(python_keywords)/sizeof(char*),s)!=-1;
  }
  
  bool is_python_builtin(const char * s){
    return dichotomic_search(python_builtins,sizeof(python_builtins)/sizeof(char*),s)!=-1;
  }

// Out of memory (allocator_custom.c): giac is asked to stop, as if ON was pressed, and unwinds.
extern "C" {
  extern void * oom_reserve;
  extern volatile char oom_hit, oom_soft;
  void oom_interrupt(){ giac::ctrl_c=giac::interrupted=true; }
}
// the reserve given back when memory runs out (8 KB, OOM_RESERVE): taken again after each
// evaluation while memory allows
static void oom_rearm(){
  oom_hit=0;
  if (!oom_reserve){
    oom_soft=1;
    oom_reserve=malloc(8192);
    oom_soft=0;
  }
}
static int eval_stopped; // the last do_eval: 1 interrupted (ON), 2 out of memory

void do_eval(giac::gen & g){
#ifdef FAKE_GIAC
  statuslinemsg(!lang?"cancel: stop calcul.":"annul: stoppe calcul",COLOR_RED);
  os_wait_1ms(1000);
#else
  freeze=giac::ctrl_c=giac::kbd_interrupted=giac::interrupted=false;
#ifndef WITH_QUAD
  if (taille(g,64)<64)
    dbg_printf("Eval %s\n",g.print(contextptr).c_str());
#endif
  giac::set_abort();
  statuslinemsg(!lang?"cancel: stop calcul.":"annul: stoppe calcul",COLOR_RED);
  g=giac::eval(g,giac::eval_level(contextptr),contextptr);
  eval_stopped=giac::interrupted?(oom_hit?2:1):0;
  if (giac::interrupted && !focus_on){ // Focus: the history says it (run)
    print_msg12(lang?"Interrompu":"Interrupted",nullptr);
    getkey(0);
    freeze=false;
  }
  giac::clear_abort();
  giac::ctrl_c=giac::kbd_interrupted=giac::interrupted=false;
  if (freeze){
    statuslinemsg(lang?"Ecran fige.":"Screen freezed",COLOR_RED);
    getkey(0);
    freeze=false;
  }
#endif
}

bool console_python_mode(){
  return xcas_python_eval==1 || giac::python_compat(contextptr);
}

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
// a sum of two terms starts with the positive one: 4-x^2, ln|x-2|-ln|x+2|, x-ln(e^x+1)
// (giac gives -x^2+4, -ln|x+2|+ln|x-2|, -ln(e^x+1)+x)
static giac::gen positive_first(const giac::gen & g){
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
  if (g._SYMBptr->sommet==at_plus && f.type==_VECT && f._VECTptr->size()==2 && neg_term(f._VECTptr->front()) && !neg_term(f._VECTptr->back())){
    vecteur & w=*f._VECTptr; // two terms only: -x^3+3*x^2-2 keeps its descending powers
    swapgen(w[0],w[1]);
  }
  return symbolic(g._SYMBptr->sommet,f);
}

// Automatic normalization of results. giac's default autosimplify ("regroup") leaves
// (4*sqrt(2)*pi+pi)/2-pi*(-4*sqrt(2)+1)/2, (x^2-1)/(x-1) or 2*x/(2*sqrt(x^2+1)) as is.
// ratnormal fixes those (4*sqrt(2)*pi, x+1, x/sqrt(x^2+1)) quickly: radicals, pi, sin(x)...
// are just symbols to it. simplify() is only used on tiny trig expressions without radicals
// (sin(x)^2+cos(x)^2 -> 1). History: simplify under a time budget (interrupted through
// control_c) exited the app on diff(sqrt(x^2+1),x) and reset the calculator after 80 s on
// cos(pi/12); KhiCAS must never interrupt giac on its own.
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
  if (is_undef(s) || s.type==_STRNG || taille(s,1000)>taille(g,1000)) // not if bigger: pi*(x+1) stays;
    s=g;                                                                // same size: 3*x^2*e^(3x), not x^2*3*e^(3x)
  if (!trig && !xcas::has_radical(g) && (has_op(g,*at_sin) || has_op(g,*at_cos) || has_op(g,*at_tan))){
    // trig of a variable: in sin and cos, with sin^2+cos^2=1 (cheap, unlike simplify):
    // d/dx ln(sec(x)+tan(x)) is 1/cos(x), not (1+tan(x)^2+sin(x)/cos(x)^2)/(1/cos(x)+tan(x))
    bool var=false;
    const vecteur ids=lidnt(g);
    for (unsigned k=0;k<ids.size();++k)
      if (!(ids[k]==cst_pi))
        var=true;
    if (var){
      const gen sc=_tan2sincos(g,contextptr);
      for (int k=0;k<2;++k){
        const gen t=ratnormal(k?_trigcos(sc,contextptr):_trigsin(sc,contextptr),contextptr);
        if (!is_undef(t) && t.type!=_STRNG && taille(t,1000)<taille(s,1000))
          s=t;
      }
    }
  }
  if (!trig && xcas::has_radical(s) && !lidnt(s).empty()){ // a trig substitution: normal gives
    const gen n=normal(s,contextptr);                     // -sqrt(4-x^2)/(4*x) for
    if (!is_undef(n) && n.type!=_STRNG && taille(n,1000)<taille(s,1000)) // (x^2+2*sqrt(4-x^2)-4)/(4*(sqrt(4-x^2)-2)*x)
      s=n;
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
// The textbook antiderivatives of sec, csc and their cubes (of a linear u = a*x+b): giac took
// 18-34 s on the calculator for them and answered ln(|sin(x)+1/sin(x)+2|)/4-... for sec(x).
// Returns 0 for anything else.
static giac::gen table_integral(const giac::gen & g){
  using namespace giac;
  if (!(g.is_symb_of_sommet(at_integrate) || g.is_symb_of_sommet(at_int)) || g._SYMBptr->feuille.type!=_VECT || g._SYMBptr->feuille._VECTptr->size()!=2)
    return 0;
  const gen & x=(*g._SYMBptr->feuille._VECTptr)[1];
  if (x.type!=_IDNT)
    return 0;
  // f = cos(u)^-n or sin(u)^-n, however written: sec(u)^3, 1/cos(u)^3, (1/cos(u))^3, cos(u)^-3
  gen d=eval((*g._SYMBptr->feuille._VECTptr)[0],1,contextptr);
  int n=1;
  for (int k=0;k<4;++k){
    if (d.is_symb_of_sommet(at_inv)){
      n=-n;
      d=d._SYMBptr->feuille;
    }
    else if (d.is_symb_of_sommet(at_sec) || d.is_symb_of_sommet(at_csc)){
      n=-n;
      d=symbolic(d.is_symb_of_sommet(at_sec)?at_cos:at_sin,d._SYMBptr->feuille);
    }
    else if (d.is_symb_of_sommet(at_pow) && d._SYMBptr->feuille.type==_VECT && d._SYMBptr->feuille._VECTptr->size()==2
             && d._SYMBptr->feuille._VECTptr->back().type==_INT_){
      n*=d._SYMBptr->feuille._VECTptr->back().val;
      d=d._SYMBptr->feuille._VECTptr->front();
    }
    else
      break;
  }
  if (n!=-1 && n!=-3)
    return 0;
  n=-n;
  const bool c=d.is_symb_of_sommet(at_cos);
  if (!c && !d.is_symb_of_sommet(at_sin))
    return 0;
  const gen u=d._SYMBptr->feuille,a=derive(u,x,contextptr);
  if (is_zero(a) || !is_constant_wrt(a,x,contextptr))
    return 0;
  const gen s=symbolic(c?at_sec:at_csc,u),t=symbolic(c?at_tan:at_cot,u);
  const gen l=symbolic(at_ln,symbolic(at_abs,c?symbolic(at_plus,makesequence(s,t)):symbolic(at_plus,makesequence(s,symbolic(at_neg,t)))));
  // ln|sec(u)+tan(u)|, ln|csc(u)-cot(u)|; cubes: (sec(u)*tan(u)+ln|sec(u)+tan(u)|)/2,
  // (-csc(u)*cot(u)+ln|csc(u)-cot(u)|)/2; divided by a: sec(3x) /3, sec(x/2) 2*...
  gen r=n==3?symbolic(at_plus,makesequence(c?symbolic(at_prod,makesequence(s,t)):symbolic(at_neg,symbolic(at_prod,makesequence(s,t))),l)):l;
  gen q=n==3?gen(2):gen(1);
  if (a.type==_INT_)
    q=q*a;
  else
    r=symbolic(at_prod,makesequence(inv(a,contextptr),r));
  if (!is_one(q))
    r=symbolic(at_division,makesequence(r,q));
  return r;
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
static giac::gen known_sum(const giac::gen & g,std::string & msg){
  using namespace giac;
  if (!g.is_symb_of_sommet(at_sum) || g._SYMBptr->feuille.type!=_VECT || g._SYMBptr->feuille._VECTptr->size()!=4)
    return g;
  const vecteur & v=*g._SYMBptr->feuille._VECTptr;
  const gen & f=v[0],n=v[1],a=v[2];
  const vecteur ids=lidnt(f);
  if (n.type!=_IDNT || !(v[3]==plus_inf) || a.type!=_INT_ || ids.size()!=1 || !(ids.front()==n))
    return g;
  const gen L=_limit(makesequence(f,n,plus_inf),contextptr),Le=evalf(L,1,contextptr);
  if (L.is_symb_of_sommet(at_bounded_function) || L==plus_inf || L==minus_inf || L==unsigned_inf || (Le.type==_DOUBLE_ && Le._DOUBLE_val!=0)){
    msg="diverges (the terms do not go to 0)";
    return g;
  }
  const gen R=subst(f,symbolic(at_pow,makesequence(gen(-1),n)),1,false,contextptr); // f=(-1)^n*R
  const bool isalt=alternating(f,n);
  const vecteur lv=lvar(R);
  const bool rational=lv.size()==1 && lv.front()==n;
  const gen P=_numer(R,contextptr),Q=_denom(R,contextptr);
  const gen dp=_degree(makesequence(P,n),contextptr),dq=_degree(makesequence(Q,n),contextptr);
  if (rational && dp.type==_INT_ && dq.type==_INT_){
    const int p=dq.val-dp.val;
    const gen c=_lcoeff(makesequence(P,n),contextptr)/_lcoeff(makesequence(Q,n),contextptr);
    if (!isalt && p<=1) // like the harmonic series
      return is_positive(c,contextptr)?plus_inf:minus_inf;
    if (!isalt && dp.val==0 && p%2==0 && p<=8 && a.val>=1 && is_zero(ratnormal(Q-_lcoeff(makesequence(Q,n),contextptr)*pow(n,p),contextptr))){
      static const int zd[]={6,90,945,9450}; // zeta(2k)=pi^(2k)*b/zd: b=1,1,1,1
      gen s=c*pow(cst_pi,p)/zd[p/2-1];
      for (int k=1;k<a.val;++k)
        s=s-subst(R,n,k,false,contextptr);
      return ratnormal(s,contextptr);
    }
    if (isalt && p==1 && a.val>=0){ // c/(n+b): -ln(2) for 1/n, pi/4 for 1/(2n+1)
      const gen b=ratnormal(Q/_lcoeff(makesequence(Q,n),contextptr)-n,contextptr);
      if (dp.val==0 && dq.val==1 && is_zero(b) && a.val>=1){
        gen s=-c*ln(2,contextptr);
        for (int k=1;k<a.val;++k)
          s=s-subst(f,n,k,false,contextptr);
        return s;
      }
      if (dp.val==0 && dq.val==1 && b==fraction(1,2) && a.val==0) // c/(n+1/2): c*pi/2
        return ratnormal(c*cst_pi/2,contextptr);
    }
    if (p>=2 || isalt){ // a decimal value: 200 terms, then the tail (integral of c/n^p, or the
      double s=0,last=0; // mean of two partial sums of an alternating series), from the
      const int N=a.val+200; // smallest term: floats lose less
      for (int k=N;k>=a.val;--k){
        const gen y=evalf(subst(f,n,k,false,contextptr),1,contextptr);
        if (y.type!=_DOUBLE_)
          return g;
        if (k==N)
          last=y._DOUBLE_val;
        s+=y._DOUBLE_val;
      }
      if (isalt)
        s-=last/2;
      else {
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
  // c*r^n/n!: c*e^r (from 0), 1/n! is e
  const gen cf=ratnormal(f*symbolic(at_factorial,n),contextptr);
  const gen r=ratnormal(subst(cf,n,n+1,false,contextptr)/cf,contextptr);
  if (is_constant_wrt(r,n,contextptr) && a.val>=0){
    gen s=subst(cf,n,0,false,contextptr)*exp(r,contextptr);
    for (int k=0;k<a.val;++k)
      s=s-subst(f,n,k,false,contextptr);
    return ratnormal(s,contextptr);
  }
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
  const vecteur ids=lidnt(f);
  if (ids.size()!=1 || !(ids.front()==n))
    return 0;
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
    double s=0,last=0; // from the smallest term: floats lose less
    const int N=a.val+200;
    for (int k=N;k>=a.val;--k){
      const gen y=evalf(subst(f,n,k,false,contextptr),1,contextptr);
      if (y.type!=_DOUBLE_)
        return 0;
      if (k==N)
        last=y._DOUBLE_val;
      s+=y._DOUBLE_val;
    }
    if (isalt) // the mean of the partial sums to N-1 and to N
      return gen(s-last/2);
    return gen(s+c._DOUBLE_val*::pow(N+0.5,1-p)/(p-1));
  }
  return 0;
}

static giac::gen numeric_hard_integrals(const giac::gen & g){
  using namespace giac;
  if (g.type!=_SYMB)
    return g;
  const gen & a=g._SYMBptr->feuille;
  if ((g._SYMBptr->sommet==at_integrate || g._SYMBptr->sommet==at_int) && a.type==_VECT && a._VECTptr->size()==4){
    const vecteur & v=*a._VECTptr;
    // constant bounds: numbers, pi, e (an infinite bound is left to giac)
    if (v[1].type==_IDNT && evalf(v[2],1,contextptr).type==_DOUBLE_ && evalf(v[3],1,contextptr).type==_DOUBLE_ && hard_radicand(v[0],v[1]))
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
// solutions as equations: x=-2, x=2 (one: x=3), drawn without parentheses (kdisplay.cc)
extern "C" { volatile int heap_free_probe, heap_list_probe, heap_big_probe; } // heap after the last evaluation (tools/emu)
// Focus: the decimal value of an exact numeric result, shown under it (console_approx), for the
// result printed as console_approx_for
static std::string * approx_text;
const char * console_approx_for="";
const char * console_approx(){ return approx_text?approx_text->c_str():""; }
static void result_approx(const giac::gen & g){
  using namespace giac;
  if (!approx_text)
    approx_text=new std::string;
  approx_text->clear();
  console_approx_for="";
  if (g.type==_INT_ || g.type==_ZINT || g.type==_DOUBLE_ || g.type==_FLOAT_ || g.type==_STRNG || g.type==_VECT
      || taille(g,64)>=64) // (pi/2 has an identifier, pi: evalf below tells numbers from expressions)
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
  const size_t k=approx_text->find("*i"); // 1.5+0.5*i: 1.5+0.5i
  if (k!=std::string::npos && k+2==approx_text->size())
    approx_text->erase(k,1);
}

// Results as textbooks write them: pi/2, x^3/3, sqrt(2)/2, x-x^3/6+O(x^6), e (giac prints
// 1/2*pi, 1/3*x^3, x-1/6*x^3+x^6*order_size(x), exp(1)). Each top-level term (split at + - , =):
// p/q*m -> p*m/q, m*order_size(v) -> O(m); exp(1) -> e. Inside brackets nothing changes.
// Plain chars (uSTL string ops are large and substr(pos) is broken).
__attribute__((noinline)) static void textbook(std::string & str){
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
  for (const_iterateur it=s._VECTptr->begin();it!=s._VECTptr->end();++it)
    w.push_back(symb_equal(x,*it));
  return w.size()==1?w.front():gen(w,_SEQ__VECT);
}

#ifdef WITH_EQW
// R3: results drawn in 2D in the console history. A result of k rows is its output line (the 1D
// text: copy, history recall and saved sessions use it) followed by k-1 LINE_TYPE_CONT lines.
// The layouts of the last drawn results are cached (parsing + layout take ~50-100 ms).
static const int H2D_CACHE=8,H2D_W=LCD_WIDTH_PX-8; // 8: the results a screen shows
static const char * h2d_key[H2D_CACHE];
static unsigned h2d_hash[H2D_CACHE];
static giac::gen * h2d_layout; // allocated on first use: a static array of gens needs a static
                               // constructor, which the app's relocation scheme cannot link
static int h2d_next;
static unsigned h2d_strhash(const char * s){
  unsigned h=0;
  for (;*s;++s)
    h=h*31+(unsigned char)*s;
  return h;
}
// puts the layout of history line s in the cache
static void h2d_store(const char * s,const giac::gen & lay){
  if (!h2d_layout)
    h2d_layout=new giac::gen[H2D_CACHE];
  h2d_key[h2d_next]=s;
  h2d_hash[h2d_next]=h2d_strhash(s);
  h2d_layout[h2d_next]=lay;
  h2d_next=(h2d_next+1)%H2D_CACHE;
}
// rows of the console that result g takes in 2D (0: shown as text), and its layout
static int console_rows2d(const giac::gen & g,giac::gen & lay){
  using namespace giac;
  if ((g.type!=_SYMB && g.type!=_FRAC && g.type!=_VECT) || g.is_symb_of_sommet(at_program) || taille(g,100)>=100)
    return 0;
#ifdef WITH_PLOT
  if (xcas::ispnt(g))
    return 0;
#endif
  lay=xcas::history_layout(g,H2D_W,7*CONSOLE_ROW_PX-2,contextptr); // 7 rows: asin(x/3) in a fraction
  if (is_undef(lay))
    return 0;
  return (xcas::Equation_total_size(lay).dy+2+CONSOLE_ROW_PX-1)/CONSOLE_ROW_PX;
}
// draws history result s in 2D, right-aligned in the block [top,top+height), nothing above
// ymin; false if it can't (the console then prints the text)
bool console_draw2d(const char * s,int top,int height,int ymin){
  using namespace giac;
  const unsigned h=h2d_strhash(s);
  int k=0;
  while (h2d_layout && k<H2D_CACHE && !(h2d_key[k]==s && h2d_hash[k]==h))
    ++k;
  if (!h2d_layout || k==H2D_CACHE){ // an older result: parse its text (not evaluated)
    k=h2d_next;
    stdostream * savelog=logptr(contextptr);
    logptr(0,contextptr);
    const gen g(s,contextptr);
    logptr(savelog,contextptr);
    h2d_store(s,xcas::history_layout(g,H2D_W,height,contextptr));
  }
  const gen & lay=h2d_layout[k];
  if (is_undef(lay))
    return false;
  const eqwdata e=xcas::Equation_total_size(lay);
  xcas::draw_layout_at(lay,LCD_WIDTH_PX-e.dx-4,top+(height-e.dy)/2,ymin);
  return true;
}
#endif

#ifdef WITH_EQW
// F4 on a result selected in the history: its next form in place, with the form's name in the
// status line: original, simplified, one fraction, factored, expanded, decimal. Each form is
// computed from the original text (kept here), so decimal never makes the next ones inexact.
// Like the viewer's F4 (kdisplay.cc): no simplify on radicals, never interrupted.
static const char * form_line; // text of the result being cycled (its str pointer)
static std::string * form_orig;
static int form_idx;
const char * console_form_name="exact"; // the form of console_form_line (Focus chip)
const char * console_form_line(){ return form_line; }
void console_cycle_form(int l){
  using namespace giac;
  static const unary_function_ptr * const ops[]={at_simplify,at_ratnormal,at_factor,at_expand,at_evalf};
  static const char * const names[]={"original","simplified","one fraction","factored","expanded","decimal"};
  const int n=sizeof(ops)/sizeof(ops[0]);
  if (!form_orig)
    form_orig=new std::string;
  if ((const char *)Line[l].str!=form_line){
    *form_orig=(const char *)Line[l].str;
    form_idx=0;
  }
  stdostream * savelog=logptr(contextptr);
  logptr(0,contextptr);
  const gen g(*form_orig,contextptr);
  const std::string cur((const char *)Line[l].str); // forms that print like this or the original are skipped
  gen r;
  for (int t=0;t<=n;++t){
    form_idx=(form_idx+1)%(n+1);
    if (!form_idx){
      r=g;
      break;
    }
    if (ops[form_idx-1]==at_simplify && xcas::has_radical(g))
      continue;
    statuslinemsg("computing...");
    r=(*ops[form_idx-1])(g,contextptr);
    if (ops[form_idx-1]==at_factor && !is_undef(r) && r.type!=_STRNG)
      r=xcas::merge_sqrt(r,contextptr); // (1-25*x^2)^(3/2), not (5*x+1)*(5*x-1)*sqrt(...)
    if (!is_undef(r) && r.type!=_STRNG){
      const std::string rt=r.print(contextptr);
      if (rt!=cur && rt!=*form_orig)
        break;
    }
  }
  logptr(savelog,contextptr);
  const bool oom=oom_hit; // memory ran out (a huge expansion): back to the original form
  if (oom){
    ctrl_c=interrupted=false;
    form_idx=0;
    r=g;
  }
  gen lay;
  const int rows=focus_on?0:console_rows2d(r,lay); // Focus draws 2D itself: no continuation rows
  const std::string text=form_idx?r.print(contextptr):*form_orig;
  if (console_replace_result(l,text.c_str(),rows?rows:1)){
    form_line=(const char *)Line[l].str;
    if (rows)
      h2d_store(form_line,lay);
  }
  console_form_name=form_idx?names[form_idx]:"exact"; // Focus: the forms chip
  oom_rearm();
  if (focus_on)
    statuslinemsg(oom?"Out of memory":"");
  else
    statuslinemsg((std::string("form: ")+names[form_idx]+"    F4: next").c_str());
}
#endif

// f(x):=x^2+1 on one line (also typed f(x)=x^2+1): a math function, defined without giac's
// program log ("// Parsing f // Success // compiling f"). Returns the length of "f(x)", or 0.
static int simple_definition(const char * s){
  const char * p=strstr(s,":=");
  if (!p || p==s || strchr(s,'\n') || strchr(s,';'))
    return 0;
  int n=p-s;
  while (n>0 && s[n-1]==' ')
    --n;
  return (n>0 && s[n-1]==')' && strchr(s,'(') && strchr(s,'(')<p)?n:0;
}

// called from editor, return
int check_parse(const std::vector<textElement> & v,int python){
#ifdef FAKE_GIAC
  return 0;
#else
  //dbg_printf("check_parse\n");
  if (v.empty())
    return 0;
  char status[256];
  for (int i=0;i<sizeof(status);++i)
    status[i]=0;
  std::string s=merge_area(v);
  giac::python_compat(python,contextptr);
  if (python) s="@@"+s; // force Python translation
  giac::gen g(s,contextptr);
  int lineerr=giac::first_error_line(contextptr);
  if (lineerr){
    std::string tok=giac::error_token_name(contextptr);
    int pos=-1;
    if (lineerr>=1 && lineerr<=v.size()){
      pos=v[lineerr-1].s.find(tok);
      const std::string & err=v[lineerr-1].s;
      if (pos>=err.size())
	pos=-1;
      if (python){
	// find 1st token, check if it's def/if/elseif/for/while
	size_t i=0,j=0;
	for (;i<err.size();++i){
	  if (err[i]!=' ')
	    break;
	}
	std::string firsterr;
	for (j=i;j<err.size();++j){
	  if (!isalpha(err[j]))
	    break;
	  firsterr += err[j];
	}
	// if there is no : at end set pos=-2
	if (firsterr=="for" || firsterr=="def" || firsterr=="if" || firsterr=="elseif" || firsterr=="while"){
	  for (i=err.size()-1;i>0;--i){
	    if (err[i]!=' ')
	      break;
	  }
	  if (err[i]!=':')
	    pos=-2;
	}
      }
    }
    else {
      lineerr=v.size();
      tok=lang?"la fin":"end";
      pos=0;
    }
    string S((lang?"Erreur ligne ":"Error line ")+giac::print_INT_(lineerr));
    if (pos>=0)
      do_confirm((S+(lang?" a ":" at ")+tok).c_str());
    else {
      if (pos==-2)
        S += lang?". ; manquant ?":", : missing?";
      do_confirm(S.c_str());
    }
  }
  else {
#if 1
    do_eval(g);
    statuslinemsg(lang?"Syntaxe OK.":"Parse OK.",COLOR_CYAN);
    os_wait_1ms(700);
#else
    print_msg12(lang?"Syntaxe OK.":"Parse OK.",lang?"Taper une touche pour evaluer.":"Type any key to eval.");
    int key=getkey(1);
    if (key!=KEY_CTRL_EXIT)
      do_eval(g);
#endif
  }
  return lineerr;
#endif
}

int find_color(const char * s){
  if (!s) return 0;
  const char ch=s[0];
  if (ch=='"')
    return 38052;
  if (!isalpha(s[0]))
    return 0;
  char buf[256];
  const char * ptr=s;
  for (int i=0;i<255 && (isalphanum(*ptr) || *ptr=='_'); ++i){
    ++ptr;
  }
  strncpy(buf,s,ptr-s);
  buf[ptr-s]=0;
  if (strcmp(buf,"def")==0 || strcmp(buf,"import")==0 || is_python_keyword(buf))
    return _BLUE;
#ifdef FAKE_GIAC
  if (is_python_builtin(buf))
    return _CYAN;
  if (strcmp(buf,"sin")==0)
    return 24844;
  return 0;
#else  
  giac::gen g;
  const int token=giac::find_or_make_symbol(buf,g,nullptr,false,contextptr);
  //dbg_printf("find_color %s %i %s\n",buf,token,g.print(contextptr).c_str());
  if (token==T_UNARY_OP || token==T_UNARY_OP_38 || token==T_LOGO)
    return 24844; // 38052;
  if (token==T_NUMBER)
    return _GREEN;
  if (token!=T_SYMBOL)
    return _BLACK;
  return _MAGENTA;
#endif
}

std::string get_searchitem(std::string & replace){
  replace="";
  std::string search;
  lock_alpha();
  int res=inputline(lang?"EXIT ou chaine vide: annulation":"EXIT or empty string: cancel",lang?"Chercher:":"Search:",search,false);
  if (search.empty() || res==KEY_CTRL_EXIT)
    return "";
  replace="";
  std::string tmp=(lang?"EXIT: recherche seule de ":"EXIT: search only ")+search;
  lock_alpha();
  res=inputline(tmp.c_str(),lang?"Remplacer par:":"Replace by:",replace,false);
  if (res==KEY_CTRL_EXIT)
    replace="";
  return search;
}


int select_script_and_run() {
  char filename[MAX_FILENAME_SIZE+1];
  if (fileBrowser(filename, (char*)"*.py", (char *)"Run script")) 
    return run_script(filename);
  return 0;
}

void erase_script(){
  char filename[MAX_FILENAME_SIZE+1];
  const int res=fileBrowser(filename, (char*)"*.py", (char *)"Scripts");
  if (res && do_confirm(lang?"Vraiment effacer":"Really erase?")){
    erase_file(filename);
  }
}

string extract_name(const char * s){
  int l=strlen(s),i,j;
  for (i=l-1;i>=0;--i){
    if (s[i]=='.')
      break;
  }
  if (i<=0)
    return "f";
  for (j=i-1;j>=0;--j){
    if (s[j]=='\\')
      break;
  }
  if (j<0)
    return "f";
  return string(s+j+1).substr(0,i-j-1);
}

void edit_script(const char * fname){
  // clear_abort();
  char fname_[MAX_FILENAME_SIZE+1];
  const char * filename=nullptr;
  int res=1;
  if (fname)
    filename=(char *)fname; // safe, it will not be modified
  else {
    res=fileBrowser(fname_, (char*)"*.py", (char *)"Scripts");
    filename=fname_;
  }
  if (res) {
    string s;
    load_script(filename,s);
    if (s.empty()){
      constexpr const int k=KEY_CTRL_F5; // confirm("Program","F1: Tortue, F5: Python",true);
      if (k==-1)
        return;
      if (k==KEY_CTRL_F1)
        s="\nefface;\n ";
      else{
        s=extract_name(filename);
        if (s=="session")
          s="f";
        s="def "+s+"(x):\n  \n  return x";
      }
    }
    // split s at newlines
    if (edptr==nullptr)
      edptr=new textArea;
    if (!edptr) return;
    edptr->elements.clear();
    edptr->clipline=-1;
    edptr->filename=remove_path(remove_extension(filename));
    //cout << "script " << edptr->filename << endl;
    edptr->editable=true;
    edptr->changed=false;
    edptr->python=true;
    edptr->longlinescut=false;
    edptr->elements.clear();
    edptr->y=0;
    add(edptr,s);
    s.clear();
    edptr->line=0;
    //edptr->line=edptr->elements.size()-1;
    edptr->pos=0;
    //dbg_printf("dotextarea\n");
    int result = doTextArea(edptr);
  }
}

string khicas_state(){
#ifdef FAKE_GIAC
  return "";
#else
  const giac::gen g(giac::_VARS(-1,contextptr));
  const int b=xcas_python_eval==1?4:python_compat(contextptr);
  python_compat(0,contextptr);
  char buf[2048]="";
  //dbg_printf("VARS=%s\n",g.print(contextptr).c_str());
  if (g.type==giac::_VECT){
    for (int i=0;i<g._VECTptr->size();++i){
      string s((*g._VECTptr)[i].print(contextptr));
      //dbg_printf("VAR[%i] %s\n",i,s.c_str());
      if (strlen(buf)+s.size()+128<sizeof(buf)){
	strcat(buf,s.c_str());
	strcat(buf,":;");
      }
    }
  }
  python_compat(b,contextptr);
  if (strlen(buf)+128<sizeof(buf)){
    strcat(buf,"python_compat(");
    strcat(buf,giac::print_INT_(b).c_str());
    strcat(buf,");angle_radian(");
    strcat(buf,angle_radian(contextptr)?"1":"0");
    strcat(buf,");with_sqrt(");
    strcat(buf,withsqrt(contextptr)?"1":"0");
    strcat(buf,");");
  }
  //dbg_printf("khicas_state %s\n",buf);
  return buf;
#endif
}

void save_khicas_symbols_smem(const char * filename) {
  // save variables in xcas mode,
  // because at load time the parser will be in xcas mode
  const string s(khicas_state());
  save_script(filename,s);
}

string remove_path(const string & st){
  int s=int(st.size()),i;
  for (i=s-1;i>=0;--i){
    if (st[i]=='\\')
      break;
  }
  return st.substr(i+1,s-i-1);
}


string remove_extension(const string & st){
  int s=int(st.size()),i;
  for (i=s-1;i>=0;--i){
    if (st[i]=='.')
      break;
  }
  return st.substr(0,i);
}

void save(const char * fname){
  //clear_abort();
  const string filename(remove_path(remove_extension(fname)));
  save_console_state_smem((filename+".xw").c_str()); 
  if (edptr)
    check_leave(edptr);
}

void save_session(){
  if (strcmp(session_filename,"session") && console_changed){
    std::string tmp(session_filename);
    tmp += lang?" a ete modifie!":" was modified!";
    if (confirm(tmp.c_str(),lang?"F1: sauvegarder, F5: tant pis":"F1: save, F5: discard changes")==KEY_CTRL_F1){
      save(session_filename);
      console_changed=0;
    }    
  }
  save("session");
  // this is only called on exit, no need to reinstall the check_execution_abort timer.
  if (edptr && edptr->changed && edptr->filename!="session.py"){
    if (!check_leave(edptr)){
      save_script("lastprg.py",merge_area(edptr->elements));
    }
  }
}

int restore_session(const char * fname){
  clear_screen(); // Bdisp_AllClr_VRAM();
  drawRectangle(0,0,LCD_WIDTH_PX,16,COLOR_BLACK);
#ifdef WITH_DESOLVE
  os_draw_string_medium_(0,0,lang?"KhiCAS pour TI83 [allegee avec desolve]":"KhiCAS for TI84 [with desolve]");
#else
  os_draw_string_medium_(0,0,lang?"KhiCAS pour TI83 [allegee sans desolve]":"KhiCAS for TI84 [without desolve]");
#endif
  os_draw_string_medium_(0,C18,"(c) B. Parisse et al, license GPL 2");
  os_draw_string_medium_(0,2*C18,"www-fourier.univ-grenoble-alpes.fr/~parisse");
  // os_draw_string_medium_(0,3*C18,"");
  os_draw_string_medium_(0,4*C18,lang?"Utiliser les 5 touches sous l'ecran pour":"Press one of the 5 keys below the screen");
  os_draw_string_medium_(0,5*C18,lang?"ouvrir un menu, cf. la legende au-dessus":"will open a fast menu according to the legend");
  os_draw_string_medium_(0,6*C18,lang?"Touche sto: sauvegarde la session":"Press sto to save the session");
  const string filename(remove_path(remove_extension(fname)));
  if (load_console_state_smem((filename+".xw").c_str()))
    return 1;
  else {
    statuslinemsg("");
    os_draw_string_medium_(0,9*C18,lang?"Tapez une touche.":"Press any key");
    int key; ck_getkey(&key);
    return 0;
  }
}

bool textedit(char * s){
  if (!s)
    return false;
  int ss=strlen(s);
  if (ss==0){
    *s=' ';
    s[1]=0;
    ss=1;
  }
  textArea ta;
  ta.elements.clear();
  ta.editable=true;
  ta.clipline=-1;
  ta.changed=false;
  ta.filename="temp.py";
  ta.y=0;
  ta.allowEXE=true;
  const bool str=s[0]=='"' && s[ss-1]=='"';
  if (str){
    s[ss-1]=0;
    add(&ta,s+1);
  }
  else
    add(&ta,s);
  ta.line=0;
  ta.pos=ta.elements[ta.line].s.size();
  const int res=doTextArea(&ta);
  if (res==TEXTAREA_RETURN_EXIT)
    return false;
  string S(merge_area(ta.elements));
  if (str)
    S='"'+S+'"';
  int Ssize=S.size();
  if (Ssize<GEN_PRINT_BUFSIZE){
    strcpy(s,S.c_str());
    for (--Ssize;Ssize>=0;--Ssize){
      if ((unsigned char)s[Ssize]==0x9c || s[Ssize]=='\n')
	s[Ssize]=0;
      if (s[Ssize]!=' ')
	break;
    }
    return true;
  }
  return false;
}

bool stringtodouble(const string & s1,double & d){
  char * ptr=nullptr;
  d=strtod(s1.c_str(),&ptr);
  return ptr!=nullptr;
}

// keep only 6 digits 
void ti_sprint_float(char * ch,float d){
  int i=d;
  if (i==d){
    sprintf(ch,"%i.0",i);
    return ;
  }
  ch[0]=0;
  const bool pos=d>=0;
  if (!pos)
    d=-d;
  float m=frexp(d,&i);
  // d=m*2^i
  // 2^i is near 1000^j
  const bool negexp=i<0;
  if (negexp)
    i=-i;
  const int j=i/10;
  float pow1000=1;
  int exp10=0;
  for (int k=j;k>0;k--){
    pow1000 *= 1000;
    exp10+=3;
  }
  if (negexp){
    d=d*pow1000;
    exp10=-exp10;
  }
  else 
    d=d/pow1000;
  while (d<1e5){
    d*=10;
    exp10--;
  }
  i=d+.5;
  if (!pos){
    ch[0]='-';
    ++ch;
  }
  sprintf(ch,"%ie%i",i,exp10);
}

void ti_sprint_double(char * ch,double d){
  const int d_integer = d;
  if (d_integer == d){
    sprintf(ch,"%i.0", d_integer);
    return;
  }
  const real_t tmp_real = os_FloatToReal(d);
  os_RealToStr(ch, &tmp_real, 11, 1, -1);
  const int s=strlen(ch);
  for (int i=0;i<s;++i){
    if (ch[i]==0x1b)
      ch[i]='e';
    if (ch[i]==0x1a)
      ch[i]='-';
  }
#ifndef WITH_QUAD
  dbg_printf("%x %x %x %x %x %x %x %x",ch[0],ch[1],ch[2],ch[3],ch[4],ch[5],ch[6],ch[7]);
#endif
}

void do_run(const char * s){
  int S=strlen(s);
  const int cap=max(2*S+32,256); // room for the paper-notation pre-pass (* := parentheses)
  char * buf=(char *)malloc(cap);
  if (!buf){
    do_confirm("Memory full");
    return;
  }
  buf[S]=0;
  for (int i=0;i<S;++i){
    char c=s[i];
    if (c==0x1e || c==char(0x9c))
      buf[i]='\n';
    else {
      if (c==0x0d)
        buf[i]=' ';
      else
        buf[i]=c;
    }
  }
  // paper notation and TI implicit multiplication (2sinx -> 2*sin(x), f(x)=... -> f(x):=...)
  // here and not in the console, so that the history keeps what the user typed
  khicas_implicit_mult(buf,cap);
  S=strlen(buf);
  if (S==3 && buf[0]=='[' && buf[2]==']' && buf[1]>='A' && buf[1]<='I'){
    string mats=get_timatrix(buf[1]-'A');
    if (mats.size()){
      mats += "=>";
      mats += char((buf[1]-'A')+'a');
      free(buf);
      do_run(mats.c_str());
      return;
    }
  }
  int yn=0x11;
  if (S>=5){ // detect "...=>Yk"
    //dbg_printf("detect %s %x %x %x %x\n",buf+S-4,buf[S-4],buf[S-3],buf[S-2],buf[S-1]);
    char c=buf[S-1];
    if (c>='0' && c<='9'){
      char y=buf[S-2];
      if ((y=='y' || y=='Y') && buf[S-3]=='>' && buf[S-4]=='='){
        //buf[S-4]=0;
        yn = (c=='0')?0x19:(c-'1'+0x10);
      }
    }
  }
#ifdef FAKE_GIAC
  if (S==2 && (s[0]=='Y' || s[0]=='y') && (s[1]>='0' && s[1]<='9')){
    char buf[3]={0x5e,0,0};
    buf[1]=(s[1]=='0')?0x19:(0x10+(s[1]-'1'));
    string val=get_tivar(buf);
    if (val.size())
      Console_Output(val.c_str());
    else
      Console_Output(s);      
  }
  else if (s[0]>='0' && s[0]<='9'){
    double d=atol(s);
    dbg_printf("atof %f\n",d);
    char ch[128];
    ti_sprint_double(ch,atan(d));
    Console_Output(ch);          
  }
  else {
    Console_Output(buf);
    vector<unsigned char> v;
    tokenize(buf,v);
    if (v.size()>2){
      char buf[3]={0x5e,0,0};
      buf[1]=yn;
      int res=os_CreateEquation(buf,(equ_t *)&v.front());
      //dbg_printf("create Y%i res=%i\n",yn,res);
    }
  }
  Console_NewLine(LINE_TYPE_OUTPUT,1);
#else
  giac::ctrl_c=giac::kbd_interrupted=giac::interrupted=false;
  if (1 && xcas_python_eval==0){
    if (!contextptr)
      contextptr=new giac::context;
    const int defn=simple_definition(buf);
    stdostream * savelog=giac::logptr(contextptr);
    if (defn)
      giac::logptr(0,contextptr);
    // Focus: giac's notes ("No checks were made for answer...", "Warning: ...") are not shown;
    // what a program prints is
    if (focus_on && !strstr(buf,"print"))
      dconsole_mode=0;
    giac::gen g(buf,contextptr);
    // x=a stores a in x (equaltosto), but x=(-x/2)^2 has x on both sides: an equation to solve
    const bool selfref=g.is_symb_of_sommet(giac::at_equal) && g._SYMBptr->feuille.type==giac::_VECT && g._SYMBptr->feuille._VECTptr->size()==2 && g._SYMBptr->feuille._VECTptr->front().type==giac::_IDNT && !giac::is_constant_wrt(g._SYMBptr->feuille._VECTptr->back(),g._SYMBptr->feuille._VECTptr->front(),contextptr);
    if (!selfref)
      g=equaltosto(g,contextptr);
    const giac::gen unknown=equation_unknown(g);
    if (unknown.type==giac::_IDNT)
      g=giac::symbolic(giac::at_solve,giac::makesequence(g,unknown));
    giac::gen var=unknown; // solve(eq,x) typed explicitly: its solutions are shown as x=... too
    if (var.type!=giac::_IDNT && g.is_symb_of_sommet(giac::at_solve)){
      if (g._SYMBptr->feuille.type==giac::_VECT && g._SYMBptr->feuille._VECTptr->size()==2 && g._SYMBptr->feuille._VECTptr->back().type==giac::_IDNT)
        var=g._SYMBptr->feuille._VECTptr->back();
      else if (g._SYMBptr->feuille.type!=giac::_VECT) // solve(expr): giac solves for x
        var=giac::gen("x",contextptr);
    }
    g=numeric_hard_integrals(g);
    giac::gen tab=table_integral(g); // sec, csc: the textbook form, at once
    if (giac::is_zero(tab))
      tab=power_sum(g); // sum(1/sqrt(n),n,1,inf)
    const bool tabled=!giac::is_zero(tab);
    const giac::gen ga=add_autosimplify(g,contextptr);
    // unchanged for programs and explicit forms (factor, expand, diff...); derivatives are
    // simplified anyway: diff(sqrt(y/4),y) is 1/(4*sqrt(y)), not (sqrt(y/4))^-1/8
    const bool autosimp=!(ga==g) || g.is_symb_of_sommet(giac::at_diff);
    g=ga;
    if (tabled){
      g=tab;
      eval_stopped=0;
    }
    else
      do_eval(g);
    std::string msg; // a plain message instead of the result
    if (eval_stopped){ // ON, or memory ran out: what the calculation built is freed
      msg=eval_stopped==2?"Out of memory":"Interrupted";
      g=0;
    }
    else {
      if (giac::contains(g,giac::at_bounded_function)) // lim sin(x) at infinity, sum((-1)^n)
        msg=strstr(buf,"sum(")?"diverges (the terms do not go to 0)":strstr(buf,"limit(")?"no limit (it oscillates)":"";
      else if (g.is_symb_of_sommet(giac::at_sum)) // an infinite sum giac could not do
        g=known_sum(g,msg);
      if (g.type==giac::_FRAC && giac::is_positive(-g._FRACptr->den,contextptr)) // -3/-4 (telescoping sums)
        g=(-g._FRACptr->num)/(-g._FRACptr->den);
      if (autosimp && !tabled)
        g=auto_simplify(g);
      if (g.type==giac::_SYMB && giac::taille(g,200)<200 && !giac::contains(g,giac::at_order_size)) // (series: as is)
        g=positive_first(g);
      if (var.type==giac::_IDNT){
        if (g.type==giac::_VECT && g._VECTptr->empty())
          msg="no solution";
        else
          g=solutions_as_equations(g,var);
      }
      if (focus_on)
        result_approx(g);
    }
    if (oom_hit && msg.empty()){ // memory ran out while simplifying
      msg="Out of memory";
      g=0;
    }
    dconsole_mode=1;
    if (defn){
      giac::logptr(savelog,contextptr);
      if (g.is_symb_of_sommet(giac::at_program)) // the 2D input above shows the function
        msg=std::string(buf,defn)+" defined";
    }
    int rows2d=0;
    giac::gen lay2d;
    if (!msg.empty())
      Console_Output(msg.c_str());
    else {
#ifdef WITH_EQW
    rows2d=focus_on?0:console_rows2d(g,lay2d); // Focus draws results itself
    giac::gen gs;
    int do_logo_graph_eqw=(rows2d || focus_on)?6:7; // drawn in 2D in the history: no viewer
    xcas::check_do_graph(g,gs,do_logo_graph_eqw,contextptr);
#endif
#ifdef WITH_PLOT
    if (xcas::ispnt(g))
      Console_Output("Graphic object");
    else
#endif
    if (taille(g,256)>=256)
      Console_Output("Done");
    else {
      std::string printed=g.print(contextptr); // (a pointer into the temporary dangled)
      if (var.type==giac::_IDNT) // solutions: x=-sqrt(2),x=sqrt(2), not x=(-sqrt(2)),x=(sqrt(2))
        strip_equation_parens(printed);
      if (printed.size()>70){ // (a long polynomial)/182: term by term (x^14/14+...), which wraps
        const giac::gen d=giac::_denom(g,contextptr);
        if ((d.type==giac::_INT_ || d.type==giac::_ZINT) && !giac::is_one(d) && giac::_numer(g,contextptr).is_symb_of_sommet(giac::at_plus)){
          const giac::gen x=giac::expand(g,contextptr);
          if (!giac::is_undef(x) && x.type!=giac::_STRNG)
            printed=x.print(contextptr);
        }
      }
      textbook(printed);
      const char * str = printed.c_str();
      if (focus_on)
        console_approx_for=str;
      Console_Output(str);
      vector<unsigned char> v;
      tokenize(str,v);
      //dbg_printf("Y2 %s v.size()=%i\n",str,v.size());
      if (v.size()>2){
        char equation_buf[3] = {0x5e, 0, 0};
        equation_buf[1] = yn;
        int res = os_CreateEquation(equation_buf,(equ_t *)&v.front());
        // dbg_printf("create Y2 res=%i\n",res);
      }
    }
    }
    Console_NewLine(LINE_TYPE_OUTPUT,1);
    if (oom_hit)
      giac::ctrl_c=giac::interrupted=false;
    oom_rearm();
    heap_free_probe=(int)malloc(0xffffff); // for tools/emu: free heap after each evaluation
    heap_list_probe=(int)malloc(0xfffffe); // the freed blocks (reusable), and the largest one
    heap_big_probe=(int)malloc(0xfffffd);
#ifdef WITH_EQW
    if (rows2d) // drawn from the evaluated result, not from its parsed text
      h2d_store((const char *)Line[Last_Line-1].str,lay2d);
#endif
    for (int k=1;k<rows2d;++k){ // continuation rows of the 2D result
      Console_Output((const Char *)"\x01");
      Console_NewLine(LINE_TYPE_CONT,1);
    }
  }
  else {
    execution_in_progress_py = 1;
    // set_abort_py();
    int res=micropy_ck_eval(buf);
    execution_in_progress_py = 0;
  }
  free(buf);
  // clear_abort_py();
  if (get_free_memory()<8*1024){
    // FIXME? clear turtle, display msg
  } 
  //Console_Output("Done"); return ;
#endif
}

void run(const char * s,int do_logo_graph_eqw){
  if (strlen(s)>=2 && (s[0]=='#' ||
		       (s[0]=='/' && (s[1]=='/' || s[1]=='*'))
		       ))
    return;
  do_run(s);
  process_freeze();
  //return ge; 
}

// returns 1 if script was run, 0 otherwise
int run_script(const char* filename) {
  string s;
  if (load_script(filename,s)<=0)
    return 0;
  do_run(s.c_str());
  return 1;
}

void load_khicas_vars(const char * BUF){
#ifdef FAKE_GIAC
  statuslinemsg("Loading vars",COLOR_RED);
#else
  if (!contextptr)
    contextptr=new giac::context;
  dconsole_mode=0;
  xcas_mode(contextptr)=python_compat(contextptr)=0;
  const bool bi=try_parse_i(contextptr);
  try_parse_i(false,contextptr);
  giac::gen g(BUF,contextptr);
  try_parse_i(bi,contextptr);
  statuslinemsg("Evaluating vars",COLOR_RED);
  //       dbg_printf("load_khicas_vars BUF=%s g=%s\n",BUF,g.print().c_str());
  if (g.type==giac::_VECT){
    const giac::vecteur & v =*g._VECTptr;
    for (int i=0;i<v.size();++i){
      const giac::gen & vi=v[i];
      //dbg_printf("load_khicas_vars i=%i v[i]=%s\n",i,vi.print().c_str());
      if (vi.is_symb_of_sommet(giac::at_nodisp) && vi._SYMBptr->feuille.is_symb_of_sommet(giac::at_sto)){
        const giac::gen &f=vi._SYMBptr->feuille._SYMBptr->feuille;
        if (f.type==giac::_VECT && f._VECTptr->size()==2){
          //dbg_printf("load_khicas_vars i=%i\n",i,vi.print().c_str());
          giac::sto(f._VECTptr->front(),f._VECTptr->back(),contextptr);
        }
      }
      else
        giac::eval(vi,1,contextptr);
    }
  }
  else
    g=giac::eval(g,1,contextptr);
  giac::angle_radian(giac::angle_radian(contextptr),giac::context0);
#endif
}  


void stop(const char * s)
{
  python_free();
#if KHICAS_STACK
  asm("assume	adl = 1\n\t"
    "ld  sp, ($D19888)\n\t"
    : /* output */
    : /* input */
    : /* clobbered registers */
  );
#endif
  lcd_Control = 0b100100101101; // TI-OS default
  exit(1);
}

vector<logo_turtle> & turtle_stack();

#ifdef FAKE_GIAC
namespace giac {
  unsigned ustl_vecteur_prolog=0;
}
#endif

extern "C" unsigned char __heapbot[];
extern "C" unsigned char __heaptop[];

int main1(){
  std::vector<giac::gen> ustlv;
  const unsigned * ustlptr=(unsigned *)&ustlv;
  giac::ustl_vecteur_prolog=*ustlptr;
  //dbg_printf("ustlv len=%i %x %x %x %x\n",sizeof(ustlv),ustlptr[0],ustlptr[1],ustlptr[2],ustlptr[3]);
#ifdef FAKE_GIAC
#if defined WITH_LCDMALLOC
  // check kmalloc
  void * ptr=malloc(65536);
  int freemem=(int)malloc(0xffffff);
  dbg_printf("malloc 65536 ptr=%x free=%i\n",ptr,freemem);
  free(ptr);
  dbg_printf("after free 65536 ptr=%x free=%i\n",ptr,freemem);
#else
  void * ptr=malloc(32768);
  int freemem=(int)malloc(0xffffff);
  dbg_printf("malloc 32768 ptr=%x free=%i\n",ptr,freemem);
#endif
#else  // FAKE_GIAC
  //dbg_printf("ref_eqwdata=%i ref_complex=%i ref_symbolic=%i ref_vecteur=%i\n",sizeof(giac::ref_eqwdata),sizeof(giac::ref_complex),sizeof(giac::ref_symbolic),sizeof(giac::ref_vecteur));
  //dbg_printf("archive index plus=%i sin=%i\n",giac::archive_function_index(giac::at_plus),giac::archive_function_index(giac::at_sin));
  //dbg_printf("allocfast size refv=%i refs=%i refc=%i\n",sizeof(giac::ref_vecteur),sizeof(giac::ref_symbolic),sizeof(giac::ref_complex));
  if (!contextptr)
    contextptr = new giac::context;
  giac::epsilon(contextptr)=1e-5;
#endif // FAKE_GIAC

  //do_confirm("SDK init"); return 0;
  // main starts after malloc init
  int i = 0, j = 0;
  
  SetQuitHandler(save_session); // automatically save session when exiting
  // FIXME turtle
  turtle(); 
  turtle_stack(); // required to init turtle
  Console_Init();
  const system_info_t * sptr=os_GetSystemInfo();
  //if (sptr) dbg_printf("%i %i %i %i\n",sptr->hardwareType,sptr->osMajorVersion,sptr->osMinorVersion, sptr->osRevisionVersion);
  // please do not remove this check, it's here for TI
  if (sptr && sptr->osMajorVersion==5 &&
      (sptr->osMinorVersion>8 ||
       (sptr->osMinorVersion==8 && sptr->osRevisionVersion>=2)
       )
      ){
    if (sptr->hardwareType==0){
      do_confirm("TI84 incompatible OS version");
      return 1;
    }
    else
      do_confirm("! OS incompatible avec mode examen !");
  }
  else if (sptr && sptr->osMajorVersion==5 && sptr->osMinorVersion==8 && sptr->hardwareType==1)
    confirm("!!! Downgradez l'OS avec CERMASTR","pour utiliser KhiCAS en mode examen");
  // do_confirm("console init");
  restore_session("session");
#ifndef FAKE_GIAC
  angle_radian(os_get_angle_unit(),contextptr);
#endif
	
  //dbg_printf("main1\n");
  //dbg_printf("plus %x %x\n",(size_t)giac::at_plus->ptr(),(size_t) giac::_plus);
  //pythonjs_static_heap=(char *)malloc(pythonjs_heap_size);
  python_init(pythonjs_stack_size,pythonjs_heap_size);
  //dbg_printf("main2\n");
  //do_confirm("after init");
  //load_config();
  //{int K; ck_getkey(&K); sdk_end(); return 0;}
  focus_init();
  Console_Disp(1);
  init_locale();
  // { statuslinemsg("after console init"); int key; GetKey(&key); }
  //do_confirm("ready");
  //Bkey_SetAllFlags(0x80); // disable catalog syscall 0x0EA1 on the Prizm
  // initialize failed ?
  // if (!(line && free_stack && mem && stack && symtab && binding && arglist && logbuf))		return 0;
  while (1) { 
    //dbg_printf("main1 loop\n");
    const Char *expr;
    if ( (expr=Console_GetLine())== nullptr)
      stop("memory error");
    else if (strcmp(expr,"kill")==0
        // && confirm("Quitter?",lang?"F1: confirmer,  F5: annuler":"F1: confirm,  F5: cancel")==KEY_CTRL_F1
        ){
      save("session"); // FIXME TICE
      if (strcmp(session_filename,"session")) // save without confirmation
        save(session_filename);
      break;
    }
    else if (strcmp(expr,"restart")==0){
      if (confirm(lang?"Effacer variables?":"Clear variables?",lang?"F1: annul,  F5: confirmer":"F1: cancel,  F5: confirm")!=KEY_CTRL_F5){
        Console_Output(" cancelled");
        Console_NewLine(LINE_TYPE_OUTPUT,1);
        //ck_getkey(&key);
        Console_Disp(1);
        continue;
      }
    }
    // should save in another file
    else if (strcmp(expr,"=>")==0 || strcmp(expr,"=>\n")==0){
      save_session();
      Console_Output((Char*)"Session saved");
    }
    else {
      save_console_state_smem("session.xw"); 
      run(expr);
      focus_evaluated();
    }
    //print_mem_info();
    Console_NewLine(LINE_TYPE_OUTPUT,1);
    //ck_getkey(&key);
    Console_Disp(1);
  }
  return 1;
}

// uintptr_t stack_ptr=0;

int main(){
  uintptr_t appstart=(0x3b0000-3);
  appstart -= *(uintptr_t *) appstart;
  uintptr_t pcmain = (uintptr_t) main;
#ifndef WITH_QUAD
  dbg_printf("appstart=%p pcmain=%p\n",appstart,pcmain);
#endif
  if (pcmain<appstart || pcmain>=0x3b0000)
    return 1;
#if KHICAS_STACK
  asm("assume	adl = 1\n\t"
      "ld  ($D19888), sp\n\t"
      "ld  sp, $D2a800\n\t"
      :        /* output */
      :  /* input */
      : /* clobbered registers */
    );
#endif
  sdk_init();
  mp_stack_ctrl_init();
#ifdef FAKE_GIAC
  dbg_printf("heap bot=%x top=%x\n",__heapbot,__heaptop);
#else 
#ifdef WITH_LCDMALLOC
  giac::tab11=(giac::char11*) malloc(giac::ALLOC11*sizeof(giac::char11));
  giac::tab16=(giac::char16*) malloc(giac::ALLOC16*sizeof(giac::char16));
  giac::tab15=(giac::char15*) malloc(giac::ALLOC15*sizeof(giac::char15));
  giac::tab36=(giac::char36*) malloc(giac::ALLOC36*sizeof(giac::char36));
#else
#ifdef WITH_LCDMALLOC  // should never be reached
  giac::tab11=(giac::char11*) (((unsigned char *) __heapbot);
#else
  giac::tab11=(giac::char11*) (((unsigned char *) lcd_Ram)+LCD_WIDTH_PX*LCD_HEIGHT_PX);
#endif
  giac::tab16=(giac::char16*)(giac::tab11+giac::ALLOC11);
  giac::tab15=(giac::char15*)(giac::tab16+giac::ALLOC16);
  giac::tab36=(giac::char36*)(giac::tab15+giac::ALLOC15);
#endif // WITH_LCDMALLOC
#endif // FAKE_GIAC
  oom_rearm();
  main1();
  python_free();
  sdk_end();
#if KHICAS_STACK
  asm("assume	adl = 1\n\t"
      "ld  sp, ($D19888)\n\t"
      :        /* output */
      :  /* input */
      : /* clobbered registers */
    );
#endif
  return 0;
}

void console_output(const char * s,int l){
  // confirm("console_output",s);
  char buf[l+1];
  strncpy(buf,s,l);
  buf[l]=0;
  dConsolePut(buf);
}

const char * console_input(const char * msg1,const char * msg2,bool numeric,int ypos){
  string str;
  if (!inputline(msg1,msg2,str,numeric,ypos))
    return nullptr;
  const char * ptr=strdup(str.c_str());
  return ptr;
}

int do_confirm(const char * s){
  return confirm(s,(lang?"F1: oui,    F5:annuler":"F1: yes,     F5: cancel"))==KEY_CTRL_F1;
}

int kbd_filter(int key){
  if (key==KEY_CTRL_LEFT) return 0;
  if (key==KEY_CTRL_RIGHT) return 3;
  if (key==KEY_CTRL_UP) return 1;
  if (key==KEY_CTRL_DOWN) return 2;
  if (key==KEY_CTRL_EXE) return 4;
  if (key==KEY_CTRL_EXIT) return 5;
  return key;
}  

int kbd_convert(int r,int c){
  if (r==1 && c==1)
    return KEY_CTRL_AC;
  if (r==2){
    if (c==7) return KEY_CHAR_0;
    if (c==6) return KEY_CHAR_DP;
    if (c==5) return KEY_CHAR_EXPN10;
    if (c==4) return KEY_CHAR_PMINUS;
    if (c==3) return 4; // KEY_CTRL_EXE;
  }
  if (r==3){
    if (c==7) return KEY_CHAR_1;
    if (c==6) return KEY_CHAR_2;
    if (c==5) return KEY_CHAR_3;
    if (c==4) return KEY_CHAR_PLUS;
    if (c==3) return KEY_CHAR_MINUS;
  }
  if (r==4){
    if (c==7) return KEY_CHAR_4;
    if (c==6) return KEY_CHAR_5;
    if (c==5) return KEY_CHAR_6;
    if (c==4) return KEY_CHAR_MULT;
    if (c==3) return KEY_CHAR_DIV;
  }
  if (r==5){
    if (c==7) return KEY_CHAR_FRAC;
    if (c==6) return KEY_CHAR_H;
    if (c==5) return KEY_CHAR_LPAR;
    if (c==4) return KEY_CHAR_RPAR;
    if (c==3) return KEY_CHAR_COMMA;
    if (c==2) return KEY_CHAR_STORE;
  }
  if (r==6){
    if (c==7) return KEY_CTRL_XTT;
    if (c==6) return KEY_CHAR_LOG;
    if (c==5) return KEY_CHAR_LN;
    if (c==4) return KEY_CHAR_SIN;
    if (c==3) return KEY_CHAR_COS;
    if (c==2) return KEY_CHAR_TAN;
  }
  if (r==8){
    if (c==7) return KEY_CTRL_ALPHA;
    if (c==6) return KEY_CHAR_SQUARE;
    if (c==5) return KEY_CHAR_POW;
    if (c==4) return 5; // KEY_CTRL_EXIT;
    if (c==3) return 2; // KEY_CTRL_DOWN;
    if (c==2) return 3; // KEY_CTRL_RIGHT;
  }
  if (r==9){
    if (c==7) return KEY_CTRL_SHIFT;
    if (c==6) return KEY_CTRL_OPTN;
    if (c==5) return KEY_CTRL_VARS;
    if (c==4) return KEY_CTRL_MENU;
    if (c==3) return 0; // KEY_CTRL_LEFT;
    if (c==2) return 1; // KEY_CTRL_UP;
  }
  if (r==10){
    if (c==7) return KEY_CTRL_F1;
    if (c==6) return KEY_CTRL_F2;
    if (c==5) return KEY_CTRL_F3;
    if (c==4) return KEY_CTRL_F4;
    if (c==3) return KEY_CTRL_F5;
    if (c==2) return KEY_CTRL_F6;
  }
  return 0;
}

