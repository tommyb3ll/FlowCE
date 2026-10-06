// Host stand-ins for the KhiCAS UI/OS functions the giac core links against (console.cc,
// main.cc, k_csdk.c, file.cc, fake.cc...). No screen, no keyboard: drawing does nothing,
// confirmations answer "cancel", the ON key is never pressed. Console output from giac
// (dConsolePut: messages, print()) goes to stderr when HOSTGIAC_LOG is set.
#include "giacPCH.h"
#include "k_csdk.h"
#include "console.h"
#include "main.h"
#include "file.h"
#include <keypadc.h>
#include <ti/vars.h>
#include <stdio.h>
#include <stdlib.h>

static bool host_log(){
  static int on=-1;
  if (on<0) on=getenv("HOSTGIAC_LOG")!=0;
  return on;
}

// main.cc
bool freeze=false,freezeturtle=false;

// console.cc
bool isalpha(char c){ return (c>='A' && c<='Z') || (c>='a' && c<='z'); }
void dConsolePut(const char * s){ if (host_log()) fputs(s,stderr); }
void dConsolePutChar(const char ch){ if (host_log()) fputc(ch,stderr); }
void dConsoleRedraw(void){}
int Console_Disp(int redraw_mode){ return 0; }
int confirm(const char * msg1,const char * msg2,bool acexit){ return KEY_CTRL_F6; } // neither F1 nor F5
int inputline(const char * msg1,const char * msg2,std::string & s,bool numeric,int ypos){ return 0; }
void draw_line(int x1, int y1, int x2, int y2, int color,unsigned short motif){}
void draw_circle(int xc,int yc,int r,int color,bool q1,bool q2,bool q3,bool q4){}
void draw_filled_circle(int xc,int yc,int r,int color,bool left,bool right){}
void draw_rectangle(int x, int y, int width, int height, unsigned short color){}
void draw_filled_polygon(std::vector< std::vector<int> > & L,int xmin,int xmax,int ymin,int ymax,int color){}
void draw_polygon(std::vector< std::vector<int> > & L,int color){}

// main.cc: the console evaluates Python only in Python mode
extern giac::context * contextptr;
bool console_python_mode(){ return giac::python_compat(contextptr)!=0; }

// file.cc
std::string get_tivar(const char * varname){ return ""; }

extern "C" {
  // k_csdk.c
  int clip_ymin=0;
  void clear_screen(void){}
  int os_draw_string_medium(int x,int y,int c,int bg,const char * s,int fake){ return x; }
  int os_draw_string_small(int x,int y,int c,int bg,const char * s,int fake){ return x; }
  int os_get_pixel(int x,int y){ return 0; }
  void os_set_pixel(int x,int y,int c){}
  void os_fill_rect(int x,int y,int w,int h,int c){}
  void os_wait_1ms(int ms){}
  void sync_screen(){}
  void set_time(int h,int m){}
  double millis(){ return 0; }
  void GetKey(int * key){ *key=KEY_CTRL_EXIT; }
  int iskeydown(int key){ return 0; }
  // fake.cc
  int ctrl_c_interrupted(int exception){ return exception; }
  // CEdev: no AppVars, the keypad registers read as zeros (never interrupted)
  var_t * os_GetAppVarData(const char * name,int * archived){ return 0; }
  volatile uint8_t hostgiac_kb_Data[8];
}
