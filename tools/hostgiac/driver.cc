// Host leak check driver for KhiCAS's giac (tools/hostgiac: build.sh builds it).
// Evaluates expressions the way KhiCAS does (src/main.cc do_run/do_eval):
//   gen g(text,contextptr); [g=add_autosimplify(g,contextptr);] g=eval(g,eval_level(contextptr),contextptr);
// Options:
//   -n N     evaluate the whole list N times (default 1)
//   -f FILE  expressions from FILE, one per line (# comments); default: the built-in list
//   -e EXPR  one expression (repeatable)
//   -a       wrap in add_autosimplify first, as do_run does (integrate -> regroup(integrate))
//   -i       KhiCAS's input pre-pass first (khicas_implicit_mult: 2sin(x) -> 2*sin(x)), as do_run
//   -k       the whole giac path of KhiCAS's do_run instead (khicas_post.cc: pre-pass, equation ->
//            solve, add_autosimplify, eval, auto_simplify/merge_sqrt, decimal value, graph view)
//   -p       print each result (first round)
//   -m       print the heap growth of every evaluation (bytes and blocks still allocated
//            after it, everything included: leaks and anything kept by giac)
//   -t K     trace round K (>=2): group the blocks allocated during each evaluation of round K
//            and still allocated after it by allocation stack, print the stacks (-d depth)
//   -d D     stack depth printed by -t (default 14)
//   -L       leak 100 bytes on purpose (checks that LeakSanitizer reports leaks)
// At exit LeakSanitizer reports the unreachable blocks (ASAN_OPTIONS=detect_leaks=1).
#include "giacPCH.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// <sanitizer/allocator_interface.h> is not shipped with GCC 11: its two functions used here
extern "C" {
  size_t __sanitizer_get_current_allocated_bytes(void);
  int __sanitizer_install_malloc_and_free_hooks(void (*malloc_hook)(const volatile void *,size_t),void (*free_hook)(const volatile void *));
}
#include <sanitizer/asan_interface.h>
#include <sanitizer/common_interface_defs.h>
#include <sanitizer/lsan_interface.h>

using namespace giac;

giac::context * contextptr=0; // as in main.cc: a global, never deleted

static const char * default_exprs[]={
  // leak on the calculator (emulator measurements)
  "integrate(1/(x^2+9),x)",
  "integrate(1/(x+1),x)",
  "integrate(1/(x^2-1),x)",
  "partfrac(1/(x^2+9))",
  "integrate(sqrt(x^2+9),x)",
  "integrate(x^2/sqrt(x^2+9),x)",
  // no leak on the calculator
  "1+1",
  "diff(x^3,x)",
  "factor(x^2-1)",
  "solve(x^2=2,x)",
  "sqrt(x^2+9)",
  "normal((x^2-1)/(x-1))",
  0
};

// ---- allocation tracking (malloc/free hooks of the sanitizer allocator) ----
struct slot { const void * p; size_t n; };
static const size_t TABN=1<<22; // open addressing, power of 2
static slot * tab; // allocated with mmap-like calloc before tracking starts
static bool tracking=false;
static size_t live_count=0,live_bytes=0;
static inline size_t hsh(const void * p){ size_t x=(size_t)p; x^=x>>17; x*=0x9E3779B97F4A7C15ULL; return x>>(64-22); }
static void on_malloc(const volatile void * p,size_t n){
  if (!tracking || !p) return;
  size_t i=hsh((const void *)p);
  for (size_t k=0;k<TABN;++k,i=(i+1)&(TABN-1)){
    if (!tab[i].p || tab[i].p==(const void *)1){ tab[i].p=(const void *)p; tab[i].n=n; ++live_count; live_bytes+=n; return; }
  }
}
static void on_free(const volatile void * p){
  if (!tracking || !p) return;
  size_t i=hsh((const void *)p);
  for (size_t k=0;k<TABN;++k,i=(i+1)&(TABN-1)){
    if (!tab[i].p) return; // not allocated while tracking
    if (tab[i].p==(const void *)p){ tab[i].p=(const void *)1; --live_count; live_bytes-=tab[i].n; return; } // tombstone
  }
}
static void track_start(){ memset(tab,0,TABN*sizeof(slot)); live_count=live_bytes=0; tracking=true; }
static void track_stop(){ tracking=false; }

// ---- grouping of the retained blocks by allocation stack ----
static int stack_depth=14;
struct group { size_t key; size_t bytes,count; void * trace[64]; int n; };
static void report_retained(const char * label){
  static group * groups=(group *)calloc(4096,sizeof(group));
  int ng=0;
  for (size_t i=0;i<TABN;++i){
    if (!tab[i].p || tab[i].p==(const void *)1) continue;
    void * tr[64]; int tid;
    size_t n=__asan_get_alloc_stack((void *)tab[i].p,tr,64,&tid);
    size_t key=1469598103934665603ULL;
    for (size_t k=0;k<n && k<(size_t)stack_depth+4;++k) key=(key^(size_t)tr[k])*1099511628211ULL;
    int g=0;
    for (;g<ng;++g) if (groups[g].key==key) break;
    if (g==ng){
      if (ng==4096) continue;
      groups[g].key=key; groups[g].bytes=groups[g].count=0; groups[g].n=n; memcpy(groups[g].trace,tr,n*sizeof(void *)); ++ng;
    }
    groups[g].bytes+=tab[i].n; groups[g].count++;
  }
  // largest first
  for (int a=0;a<ng;++a) for (int b=a+1;b<ng;++b) if (groups[b].bytes>groups[a].bytes){ group t=groups[a]; groups[a]=groups[b]; groups[b]=t; }
  printf("  retained after %s: %zu bytes in %zu blocks, %d allocation stacks\n",label,live_bytes,live_count,ng);
  for (int g=0;g<ng && g<40;++g){
    printf("  [%d] %zu bytes in %zu blocks\n",g,groups[g].bytes,groups[g].count);
    int shown=0;
    for (int k=0;k<groups[g].n && shown<stack_depth;++k){
      char buf[1024];
      __sanitizer_symbolize_pc(groups[g].trace[k],"%F %S",buf,sizeof(buf));
      if (strstr(buf,"malloc") || strstr(buf,"operator new") || strstr(buf,"nrealloc") || strstr(buf,"tmalloc") || strstr(buf,"realloc")) continue;
      printf("      %s\n",buf);
      ++shown;
    }
  }
  fflush(stdout);
}

extern "C" bool khicas_implicit_mult(char * line,int maxlen); // tiinput_glue.cc
std::string khicas_do_run(const char * s) __attribute__((weak)); // khicas_post.cc (KDISPLAY=1)
static void eval_one(const char * s,bool autosimp,bool print,bool prepass){
  ctrl_c=kbd_interrupted=interrupted=false; // do_eval
  std::string text(s);
  if (prepass){ // do_run: buffer of max(2*S+32,256) chars
    const int S=strlen(s),cap=2*S+32>256?2*S+32:256;
    char * buf=(char *)malloc(cap);
    strcpy(buf,s);
    khicas_implicit_mult(buf,cap);
    text=buf;
    free(buf);
  }
  gen g(text,contextptr);
  if (autosimp)
    g=add_autosimplify(g,contextptr);
  g=eval(g,eval_level(contextptr),contextptr);
  if (print){
    printf("%s -> %s\n",s,g.print(contextptr).c_str());
    fflush(stdout);
  }
  ctrl_c=kbd_interrupted=interrupted=false;
}

int main(int argc,char ** argv){
  int N=1,trace_round=0;
  bool autosimp=false,print=false,measure=false,prepass=false,dorun=false;
  const char * file=0;
  static const char * exprs[1024]; int ne=0;
  for (int i=1;i<argc;++i){
    if (!strcmp(argv[i],"-n") && i+1<argc) N=atoi(argv[++i]);
    else if (!strcmp(argv[i],"-f") && i+1<argc) file=argv[++i];
    else if (!strcmp(argv[i],"-e") && i+1<argc && ne<1023) exprs[ne++]=argv[++i];
    else if (!strcmp(argv[i],"-a")) autosimp=true;
    else if (!strcmp(argv[i],"-p")) print=true;
    else if (!strcmp(argv[i],"-i")) prepass=true;
    else if (!strcmp(argv[i],"-k")){
      if (!khicas_do_run){ fprintf(stderr,"-k: built without kdisplay.cc (KDISPLAY=0)\n"); return 2; }
      dorun=true;
    }
    else if (!strcmp(argv[i],"-m")) measure=true;
    else if (!strcmp(argv[i],"-t") && i+1<argc) trace_round=atoi(argv[++i]);
    else if (!strcmp(argv[i],"-d") && i+1<argc) stack_depth=atoi(argv[++i]);
    else if (!strcmp(argv[i],"-L")){ static void * volatile p; p=malloc(100); p=0; }
    else { fprintf(stderr,"unknown option %s\n",argv[i]); return 2; }
  }
  if (file){
    FILE * f=fopen(file,"r");
    if (!f){ perror(file); return 2; }
    char line[4096];
    while (ne<1023 && fgets(line,sizeof(line),f)){
      size_t l=strlen(line);
      while (l && (line[l-1]=='\n' || line[l-1]=='\r' || line[l-1]==' ')) line[--l]=0;
      if (!l || line[0]=='#') continue;
      exprs[ne++]=strdup(line);
    }
    fclose(f);
  }
  if (!ne)
    for (;default_exprs[ne];++ne) exprs[ne]=default_exprs[ne];
  if (measure || trace_round){
    tab=(slot *)calloc(TABN,sizeof(slot));
    __sanitizer_install_malloc_and_free_hooks(on_malloc,on_free);
  }
  // KhiCAS (main1): new context, epsilon 1e-5; angle unit from the OS (radian)
  contextptr=new giac::context;
  giac::epsilon(contextptr)=1e-5;
  angle_radian(1,contextptr);
  for (int r=1;r<=N;++r){
    for (int k=0;k<ne;++k){
      const bool tr=measure || r==trace_round;
      size_t before=__sanitizer_get_current_allocated_bytes();
      if (tr) track_start();
      if (dorun){
        const std::string res=khicas_do_run(exprs[k]);
        if (print && r==1){ printf("%s -> %s\n",exprs[k],res.c_str()); fflush(stdout); }
      }
      else
        eval_one(exprs[k],autosimp,print && r==1,prepass);
      if (tr) track_stop();
      size_t after=__sanitizer_get_current_allocated_bytes();
      if (measure)
        printf("round %d  %-34s  +%zu bytes in %zu blocks (allocator total %+ld)\n",r,exprs[k],live_bytes,live_count,(long)(after-before));
      if (r==trace_round){
        char label[512];
        snprintf(label,sizeof(label),"round %d of %s",r,exprs[k]);
        report_retained(label);
      }
    }
  }
  fflush(stdout);
  return 0;
}
