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
//   -T       with -p: each result followed by "  @@" and its time in ms (wall clock)
//   -F       with -k: the answer, then its F4 forms as the calculator cycles them (answer.cc)
//   -m       print the heap growth of every evaluation (bytes and blocks still allocated
//            after it, everything included: leaks and anything kept by giac)
//   -t K     trace round K (>=2): group the blocks allocated during each evaluation of round K
//            and still allocated after it by allocation stack, print the stacks (-d depth)
//   -d D     stack depth printed by -t (default 14)
//   -L       leak 100 bytes on purpose (checks that LeakSanitizer reports leaks)
// At exit LeakSanitizer reports the unreachable blocks (ASAN_OPTIONS=detect_leaks=1).
#include <time.h> // (before giac: first.h defines a clock macro)
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
static size_t live_count=0,live_bytes=0,peak_bytes=0;
static inline size_t hsh(const void * p){ size_t x=(size_t)p; x^=x>>17; x*=0x9E3779B97F4A7C15ULL; return x>>(64-22); }
static void on_malloc(const volatile void * p,size_t n){
  if (!tracking || !p) return;
  size_t i=hsh((const void *)p);
  for (size_t k=0;k<TABN;++k,i=(i+1)&(TABN-1)){
    if (!tab[i].p || tab[i].p==(const void *)1){ tab[i].p=(const void *)p; tab[i].n=n; ++live_count; live_bytes+=n; if (live_bytes>peak_bytes) peak_bytes=live_bytes;
      static size_t traced; // STEPS_PEAK=<bytes>: where the heap first passes that many (the stack)
      if (getenv("STEPS_PEAK") && atol(getenv("STEPS_PEAK"))>1 && live_bytes>(size_t)atol(getenv("STEPS_PEAK")) && !traced){ traced=1; __sanitizer_print_stack_trace(); }
      return; }
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
static void track_start(){ memset(tab,0,TABN*sizeof(slot)); live_count=live_bytes=peak_bytes=0; tracking=true; }
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
std::string khicas_forms(const char * s) __attribute__((weak)); // -F: the answer and its F4 forms
extern void answer_prepass(char * buf,int cap,bool focus);

// -S: the step-by-step solution of each input (src/steps.cc), then FlowCE's answer. Each step: its
// depth, its rule's title, the formula it applies, its math line; then the time and the result.
#include "../../src/steps.h"
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
// a search stopped after 20 s (STEPS_ALARM_MS: another delay, for ON sweeps), as ON stops it on
// the calculator: giac's interruption flags
static void steps_alarm(int){ ctrl_c=interrupted=true; }
static void steps_timer(long ms){
  struct itimerval it={{0,0},{ms/1000,(ms%1000)*1000}};
  setitimer(ITIMER_REAL,&it,0);
}
static void print_steps(const char * s){
  signal(SIGALRM,steps_alarm);
  steps_timer(getenv("STEPS_ALARM_MS")?atol(getenv("STEPS_ALARM_MS")):20000);
  char buf[1024]; strncpy(buf,s,500); buf[500]=0;
  answer_prepass(buf,sizeof(buf),true);
  gen g(buf,contextptr);
  std::vector<step_t> v;
  struct timespec t0,t1; clock_gettime(CLOCK_MONOTONIC,&t0);
  // STEPS_PEAK: the most heap the steps held at once (the calculator has far less than the PC)
  const bool pk=getenv("STEPS_PEAK") && tab;
  if (pk){ track_start(); peak_bytes=0; }
  steps_method=getenv("STEPS_METHOD")?atoi(getenv("STEPS_METHOD")):0; // (DeltaCE: a division's other methods)
  bool ok=steps_of(g,v,contextptr);
  if (pk){ track_stop(); printf("   peak heap %zu bytes\n",peak_bytes); }
  clock_gettime(CLOCK_MONOTONIC,&t1);
  steps_timer(0);
  ctrl_c=kbd_interrupted=interrupted=false;
  printf("== %s   [%s, %d steps, %.0f ms]\n",s,ok?"ok":"NO STEPS",(int)v.size(),(t1.tv_sec-t0.tv_sec)*1e3+(t1.tv_nsec-t0.tv_nsec)/1e6);
  if (steps_methods>1) printf("   method %d of %d: %s\n",steps_method%steps_methods+1,steps_methods,steps_method_name);
  for (unsigned i=0;(ok || getenv("STEPS_REJECTED")) && i<v.size();++i){ // (STEPS_REJECTED: also a failed search's)
    const char * t=steps_title(v[i]);
    printf("%s%*s%s%s%s\n",ok?"":"x",2+4*v[i].depth-!ok,"",*t?t:"",*t?":  ":"= ",steps_math(v[i],contextptr).print(contextptr).c_str());
  }
  if (khicas_do_run) printf("   FlowCE answer: %s\n",khicas_do_run(s).c_str());
  fflush(stdout);
}
double hg_now_ms(){ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec*1e3+t.tv_nsec/1e6; }
// -A: the analysis of each input (src/analyze.cc, DeltaCE): its report, line by line
#include "../../src/analyze.h"
static void print_analysis(const char * s){
  signal(SIGALRM,steps_alarm);
  steps_timer(getenv("STEPS_ALARM_MS")?atol(getenv("STEPS_ALARM_MS")):20000);
  char buf[1024]; strncpy(buf,s,500); buf[500]=0;
  answer_prepass(buf,sizeof(buf),true);
  gen g(buf,contextptr);
  std::vector<an_line> v;
  struct timespec t0,t1; clock_gettime(CLOCK_MONOTONIC,&t0);
  const bool pk=getenv("STEPS_PEAK") && tab;
  if (pk){ track_start(); peak_bytes=0; }
  const char * why=analyze_of(g,v,contextptr);
  if (pk){ track_stop(); printf("   peak heap %zu bytes\n",peak_bytes); }
  clock_gettime(CLOCK_MONOTONIC,&t1);
  steps_timer(0);
  ctrl_c=kbd_interrupted=interrupted=false;
  printf("== %s   [%s, %.0f ms]\n",s,why?why:"ok",(t1.tv_sec-t0.tv_sec)*1e3+(t1.tv_nsec-t0.tv_nsec)/1e6);
  for (unsigned i=0;i<v.size();++i){
    std::string m=v[i].math;
    for (unsigned k=0;k<m.size();++k){ if (m[k]=='\n') m[k]='|'; else if (m[k]=='\1') m[k]='~'; }
    if (v[i].kind==AN_SECTION) printf("  %s\n",v[i].text);
    else printf("    %s%s%s\n",v[i].text?v[i].text:"",v[i].text && *v[i].text && !m.empty()?":  ":"",m.c_str());
  }
  fflush(stdout);
}
// -V <dir>: the steps screen of each input (src/focus_steps.cc) drawn off screen, one image per
// stop (the top, then each step up/down goes to): <dir>/<n>_<stop>.ppm (n: the input's number)
int focus_steps_preview(const char * in,const char * ans,int stop);
#include "../../src/ui_gfx.h"
#include "../../src/ui_font.h"
#include "../../src/ui_fontdata.h"
extern "C" { extern unsigned char * ui_fb; unsigned short * ui_host_palette(void); }
int focus_analyze_preview(const char * in,int stop);
int focus_steps_able(const char * s,const char * ans=0);
static void write_ppm(const char * fn);
// -W <dir>: the analysis screen of each input (DeltaCE), as -V: <dir>/<n>_<stop>.ppm
static void view_analysis(const char * dir,int n,const char * s){
  ui_set_theme(&ui_theme_paper);
  int stops=1;
  for (int k=0;k<stops && k<12;++k){
    stops=focus_analyze_preview(s,k)+1;
    if (stops<=0){ printf("%d: %s  no analysis\n",n,s); return; }
    ui_fill(0,0,UI_W,18,ui_col(0,UC_BG));
    ui_draw_text(&ui_tb9,"ANALYSIS",-1,8,13,ui_ramp(0,UC_SUB,UC_BG),0);
    char fn[600]; snprintf(fn,sizeof(fn),"%s/%d_%d.ppm",dir,n,k);
    write_ppm(fn);
  }
  printf("%d: %s  %d stops\n",n,s,stops);
  fflush(stdout);
}
static void write_ppm(const char * fn){
  FILE * f=fopen(fn,"wb");
  if (!f){ perror(fn); return; }
  fprintf(f,"P6\n%d %d\n255\n",UI_W,UI_H);
  const unsigned short * pal=ui_host_palette();
  for (int i=0;i<UI_W*UI_H;++i){
    unsigned v=pal[ui_fb[i]];
    unsigned char rgb[3]={(unsigned char)((v>>10&31)<<3),(unsigned char)((v>>5&31)<<3),(unsigned char)((v&31)<<3)};
    fwrite(rgb,1,3,f);
  }
  fclose(f);
}
static void view_steps(const char * dir,int n,const char * s){
  static bool themed;
  if (!themed){ ui_set_theme(&ui_theme_paper); themed=true; }
  std::string ans=khicas_do_run?khicas_do_run(s):std::string();
  const size_t ap=ans.find("  ~"); // (the decimal value: shown apart on the calculator)
  if (ap!=std::string::npos) ans=ans.substr(0,ap);
  int stops=1;
  steps_method=getenv("STEPS_METHOD")?atoi(getenv("STEPS_METHOD")):0; // (DeltaCE: a division's other methods)
  for (int k=0;k<stops && k<16;++k){
    stops=focus_steps_preview(s,ans.c_str(),k);
    if (stops<0){ printf("%d: %s  no steps\n",n,s); return; }
    ui_fill(0,0,UI_W,18,ui_col(0,UC_BG)); // the status bar, as on the calculator (its label)
    ui_draw_text(&ui_tb9,"STEPS",-1,8,13,ui_ramp(0,UC_SUB,UC_BG),0);
    char fn[600]; snprintf(fn,sizeof(fn),"%s/%d_%d.ppm",dir,n,k);
    FILE * f=fopen(fn,"wb");
    if (!f){ perror(fn); return; }
    fprintf(f,"P6\n%d %d\n255\n",UI_W,UI_H);
    const unsigned short * pal=ui_host_palette();
    for (int i=0;i<UI_W*UI_H;++i){
      unsigned v=pal[ui_fb[i]];
      unsigned char rgb[3]={(unsigned char)((v>>10&31)<<3),(unsigned char)((v>>5&31)<<3),(unsigned char)((v&31)<<3)};
      fwrite(rgb,1,3,f);
    }
    fclose(f);
  }
  printf("%d: %s  %d stops; answer %s\n",n,s,stops,ans.c_str());
  fflush(stdout);
}
static std::string eval_one(const char * s,bool autosimp,bool prepass){
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
  if (stack_hit) g=string2gen("Too many recursion levels",false); // (main.cc: the console's message)
  ctrl_c=kbd_interrupted=interrupted=stack_hit=false;
  return g.print(contextptr);
}

int main(int argc,char ** argv){
  int N=1,trace_round=0;
  bool autosimp=false,print=false,measure=false,prepass=false,dorun=false,timing=false,forms=false,steps=false; bool analysis=false,able=false;
  const char * viewdir=0; const char * aviewdir=0;
  const char * file=0;
  static const char * exprs[8192]; int ne=0;
  for (int i=1;i<argc;++i){
    if (!strcmp(argv[i],"-n") && i+1<argc) N=atoi(argv[++i]);
    else if (!strcmp(argv[i],"-f") && i+1<argc) file=argv[++i];
    else if (!strcmp(argv[i],"-e") && i+1<argc && ne<8191) exprs[ne++]=argv[++i];
    else if (!strcmp(argv[i],"-a")) autosimp=true;
    else if (!strcmp(argv[i],"-p")) print=true;
    else if (!strcmp(argv[i],"-T")) timing=true;
    else if (!strcmp(argv[i],"-F")) forms=true;
    else if (!strcmp(argv[i],"-i")) prepass=true;
    else if (!strcmp(argv[i],"-k")){
      if (!khicas_do_run){ fprintf(stderr,"-k: built without kdisplay.cc (KDISPLAY=0)\n"); return 2; }
      dorun=true;
    }
    else if (!strcmp(argv[i],"-m")) measure=true;
    else if (!strcmp(argv[i],"-S")) steps=true;
    else if (!strcmp(argv[i],"-A")) analysis=true;
    else if (!strcmp(argv[i],"-Q")) able=true;
    else if (!strcmp(argv[i],"-V") && i+1<argc) viewdir=argv[++i];
    else if (!strcmp(argv[i],"-W") && i+1<argc) aviewdir=argv[++i];
    else if (!strcmp(argv[i],"-t") && i+1<argc) trace_round=atoi(argv[++i]);
    else if (!strcmp(argv[i],"-d") && i+1<argc) stack_depth=atoi(argv[++i]);
    else if (!strcmp(argv[i],"-L")){ static void * volatile p; p=malloc(100); p=0; }
    else { fprintf(stderr,"unknown option %s\n",argv[i]); return 2; }
  }
  if (file){
    FILE * f=fopen(file,"r");
    if (!f){ perror(file); return 2; }
    char line[4096];
    while (ne<8191 && fgets(line,sizeof(line),f)){
      size_t l=strlen(line);
      while (l && (line[l-1]=='\n' || line[l-1]=='\r' || line[l-1]==' ')) line[--l]=0;
      if (!l || line[0]=='#') continue;
      exprs[ne++]=strdup(line);
    }
    fclose(f);
  }
  if (!ne)
    for (;default_exprs[ne];++ne) exprs[ne]=default_exprs[ne];
  if (measure || trace_round || getenv("STEPS_PEAK")){
    tab=(slot *)calloc(TABN,sizeof(slot));
    __sanitizer_install_malloc_and_free_hooks(on_malloc,on_free);
  }
  // KhiCAS (main1): new context, epsilon 1e-5; angle unit from the OS (radian)
  contextptr=new giac::context;
  giac::epsilon(contextptr)=1e-5;
  angle_radian(1,contextptr);
  { char c; giac::stack_floor=(size_t)&c-(6<<20); } // as main.cc's stack_floor: 6 MB of the 8 MB stack
  if (able){ // -Q: the steps chip for each input (focus_steps_able, as typed)
    for (int k=0;k<ne;++k) printf("%d  %s\n",focus_steps_able(exprs[k]),exprs[k]);
    return 0;
  }
  if (aviewdir){
    for (int k=0;k<ne;++k) view_analysis(aviewdir,k,exprs[k]);
    return 0;
  }
  if (analysis){
    for (int k=0;k<ne;++k) print_analysis(exprs[k]);
    return 0;
  }
  if (steps){
    for (int k=0;k<ne;++k) print_steps(exprs[k]);
    return 0;
  }
  if (viewdir){
    for (int k=0;k<ne;++k) view_steps(viewdir,k,exprs[k]);
    return 0;
  }
  for (int r=1;r<=N;++r){
    for (int k=0;k<ne;++k){
      const bool pk=getenv("STEPS_PEAK") && tab && !measure,tr=measure || r==trace_round || pk; // (STEPS_PEAK: the answers too)
      size_t before=__sanitizer_get_current_allocated_bytes();
      if (tr) track_start();
      struct timespec t0,t1;
      clock_gettime(CLOCK_MONOTONIC,&t0);
      const std::string res=dorun?(forms?khicas_forms(exprs[k]):khicas_do_run(exprs[k])):eval_one(exprs[k],autosimp,prepass);
      clock_gettime(CLOCK_MONOTONIC,&t1);
      if (print && r==1){
        if (timing)
          printf("%s -> %s  @@%.1f\n",exprs[k],res.c_str(),(t1.tv_sec-t0.tv_sec)*1e3+(t1.tv_nsec-t0.tv_nsec)/1e6);
        else
          printf("%s -> %s\n",exprs[k],res.c_str());
        fflush(stdout);
      }
      if (tr) track_stop();
      if (pk && print && r==1) printf("   peak heap %zu bytes\n",peak_bytes);
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
