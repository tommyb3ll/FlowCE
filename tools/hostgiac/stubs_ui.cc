// Host stand-ins for what src/giac/kdisplay.cc (graph view, merge_sqrt...) needs from the rest of
// KhiCAS: menus, status line, keyboard, the spreadsheet (not built). Drawing itself goes through
// src/ui_gfx.c / ui_font.c compiled for the host into an off-screen buffer (include/sys/lcd.h).
#include "giacPCH.h"
#include "kdisplay.h"
#include "k_csdk.h"
#include "console.h"
#include "main.h"
#include "menuGUI.h"
#include "focus.h"
#include <sys/lcd.h>

extern "C" {
  uint16_t hostgiac_lcd_Ram[LCD_WIDTH*LCD_HEIGHT];
  int os_draw_string(int x,int y,int c,int bg,const char * s,int fake){ return x+8*int(strlen(s)); }
  void statuslinemsg(const char * msg,int warncolor){}
  void statusflags(void){}
  void display_time(){}
  int getkey(int allow_suspend){ return KEY_CTRL_EXIT; }
  int GetSetupSetting(int k){ return 0; }
  int do_confirm(const char * s){ return 0; }
  int select_item(const char ** ptr,const char * title,bool askfor1){ return -1; }
}
int focus_view=0;
bool stringtodouble(const std::string & s1,double & d){ d=atof(s1.c_str()); return true; }
bool inputdouble(const char * msg1,double & d){ return false; }
void draw_arc(int xc,int yc,int rx,int ry,int color,double theta1,double theta2){}
int doMenu(Menu* menu, MenuItemIcon* icontable){ return MENU_RETURN_EXIT; }
void copy_clipboard(const std::string & s,bool status){}
namespace xcas { tableur * sheetptr=0; }
namespace giac {
  bool iscell(const gen & g,int & r,int & c,GIAC_CONTEXT){ return false; }
  void makespreadsheetmatrice(matrice & m,GIAC_CONTEXT){}
  matrice extractmatricefromsheet(const matrice & m,bool value){ return m; }
}
