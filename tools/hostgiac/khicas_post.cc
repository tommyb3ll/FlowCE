// KhiCAS's do_run (src/main.cc) for the host driver's -k option, Focus mode: src/answer.cc's steps
// (the same file the calculator compiles: input pre-pass, real roots, equation -> solve, integral
// and sum tables, add_autosimplify, then after eval the sums and limits giac leaves,
// auto_simplify, textbook order, solutions as equations, the printed text), with giac's eval in
// the middle and check_do_graph (graph view, drawn off screen). What stays in main.cc needs the
// calculator: ans() (the history), [A] matrices, =>Yk, the ON key, out-of-memory recovery.
#include <time.h> // (before giac: first.h defines a clock macro)
#include "giacPCH.h"
#include "kdisplay.h"
#include "../../src/answer.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

using namespace giac;
extern giac::context * contextptr;
extern "C" { volatile unsigned char focus_phase; } // focus.cc's phase probe (answer.cc sets it)

std::string khicas_do_run(const char * s){
  const int S=strlen(s),cap=2*S+32>256?2*S+32:256;
  char * buf=(char *)malloc(cap);
  strcpy(buf,s);
  answer_prepass(buf,cap,true);
  ctrl_c=kbd_interrupted=interrupted=false;
  std::string printed;
  {
    gen g(buf,contextptr);
    answer_ctx a;
    answer_before(g,a,buf,true);
    if (!a.tabled)
      g=eval(g,eval_level(contextptr),contextptr); // do_eval
    const bool stopped=interrupted;
    ctrl_c=kbd_interrupted=interrupted=false;
    std::string msg;
    gen graw;
    if (stopped){
      msg="Interrupted";
      g=0;
    }
    else {
      answer_after(g,a,msg,graw);
      if (msg.empty()) // focus_on
        result_approx(g);
    }
    if (a.definite && msg.empty() && is_undef(g))
      msg="diverges (the integrand is unbounded on the interval)";
    if (!msg.empty())
      printed=msg;
    else {
#ifdef WITH_EQW
      gen gs;
      xcas::check_do_graph(g,gs,6,contextptr); // Focus: graph view for graphic results
#endif
#ifdef WITH_PLOT
      if (xcas::ispnt(g))
        printed="Graphic object";
      else
#endif
      if (taille(g,256)>=256)
        printed="Done";
      else {
        printed=answer_print(g,graw,a);
        set_approx_for(printed);
      }
    }
    if (*console_approx() && printed==console_approx_for)
      { printed += "  ~ "; printed += console_approx(); }
  }
  free(buf);
  ctrl_c=kbd_interrupted=interrupted=false;
  return printed;
}

// -F: the F4 forms of the answer, as the calculator cycles them (answer.cc), each press timed:
// "answer | name: text @ms | ... | back @ms"
std::string khicas_forms(const char * s){
  std::string printed=khicas_do_run(s);
  const size_t a=printed.find("  ~ ");
  const std::string bare=a==std::string::npos?printed:printed.substr(0,a);
  answer_forms F;
  answer_forms_start(F,bare,*console_approx() && bare==console_approx_for);
  std::string out=printed,cur=bare;
  char b[32];
  for (int k=0,guard=0;guard<=F.n;++guard){
    struct timespec t0,t1;
    clock_gettime(CLOCK_MONOTONIC,&t0);
    k=answer_forms_next(F,k,cur,0);
    clock_gettime(CLOCK_MONOTONIC,&t1);
    sprintf(b," @%.1f",(t1.tv_sec-t0.tv_sec)*1e3+(t1.tv_nsec-t0.tv_nsec)/1e6);
    if (!k){
      out+=" | back";
      out+=b;
      break;
    }
    out+=" | ";
    out+=answer_form_names[F.order[k-1]];
    out+=": ";
    out+=F.text[k-1];
    out+=b;
    cur=F.text[k-1];
  }
  ctrl_c=kbd_interrupted=interrupted=false;
  return out;
}
