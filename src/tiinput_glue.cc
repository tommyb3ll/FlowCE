// Glue between ti_implicit_mult() (tiinput.cc) and giac: names are classified with giac's
// own lexer tables (builtin commands, keywords) and the user's assigned variables.
// Lookup only: unlike find_or_make_symbol, this never creates symbols.
#include <cstring>
#include <giac/giacPCH.h>
#include <giac/input_lexer.h>
#include <giac/input_parser.h>
#include "tiinput.h"
#include "main.h"

extern giac::context * contextptr;
using namespace giac;

namespace {
  // names matched by flex rules of input_lexer.ll instead of the lexer tables
  const char * const flex_values[]={"I","PI","Pi","e","euler_gamma","i","inf","infinity","minus_inf","oo","pi","plus_inf","undef","unsigned_inf","π"}; // (π: the pi key, UTF-8)
  const char * const keywords[]={"and","break","by","case","catch","continue","default","div","do","elif","else","end","fi","for","from","function","global","if","in","local","mod","not","od","of","or","program","repeat","return","step","switch","then","to","try","until","while","xor"};

  bool in_list(const char * const * tab,int n,const char * s){
    for (int k=0;k<n;++k){
      if (!strcmp(tab[k],s))
        return true;
    }
    return false;
  }

  const vecteur * user_vars=0; // VARS() of the line being processed

  int token_class(int token){
    if (token==T_UNARY_OP || token==T_UNARY_OP_38)
      return TI_NAME_FUNCTION;
    if (token==T_SYMBOL || token==T_LITERAL || token==T_NUMBER)
      return TI_NAME_VALUE;
    return TI_NAME_KEYWORD;
  }

  int classify(const char * name,int len){
    char s[33];
    if (len<=0 || len>32)
      return TI_NAME_UNKNOWN;
    memcpy(s,name,len);
    s[len]=0;
    if (in_list(keywords,sizeof(keywords)/sizeof(*keywords),s))
      return TI_NAME_KEYWORD;
    if (in_list(flex_values,sizeof(flex_values)/sizeof(*flex_values),s))
      return TI_NAME_VALUE;
    std::pair<charptr_gen *,charptr_gen *> p=std::equal_range(builtin_lexer_functions_begin(),builtin_lexer_functions_end(),std::pair<const char *,gen>(s,0),tri);
    if (p.first!=p.second && p.first!=builtin_lexer_functions_end()){
      int token=p.first->second.subtype; // same decoding as find_or_make_symbol (kglobal.cc)
      token += (token<0)?512:256;
      return token_class(token);
    }
    lexer_tab_int_type tst={s,0,0,0,0};
    std::pair<const lexer_tab_int_type *,const lexer_tab_int_type *> pp=std::equal_range(lexer_tab_int_values,lexer_tab_int_values_end,tst,tri1);
    if (pp.first!=pp.second && pp.first!=lexer_tab_int_values_end)
      return token_class(pp.first->return_value);
    if (user_vars){
      for (unsigned k=0;k<user_vars->size();++k){
        const gen & v=(*user_vars)[k];
        if (v.type==_IDNT && !strcmp(v._IDNTptr->id_name,s)){
          gen w;
          v._IDNTptr->in_eval(0,v,w,contextptr,true);
          return (w.type==_FUNC || w.is_symb_of_sommet(at_program))?TI_NAME_USERFN:TI_NAME_VALUE;
        }
      }
    }
    return TI_NAME_UNKNOWN;
  }
}

bool khicas_implicit_mult(char * line,int maxlen){
  if (console_python_mode())
    return false;
  gen vars=_VARS(0,contextptr);
  user_vars=vars.type==_VECT?vars._VECTptr:0;
  const std::string res=ti_implicit_mult(ti_rewrite(line,classify),classify,true);
  user_vars=0;
  if (int(res.size())>=maxlen || !strcmp(res.c_str(),line))
    return false;
  strcpy(line,res.c_str());
  return true;
}
