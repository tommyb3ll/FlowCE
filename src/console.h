/*
  Console Lib for CASIO fx-9860G
  by Mike Smith.
*/

#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdlib.h>
#include "calc.h"
#include <string>
#include <vector>
#if !defined std
#define std ustl
#endif
typedef char Char;
// Python syntax (giac's python2xcas): in every build without units (the makefile's FEATURE_DEFS)
#ifdef WITH_UNITS
#define FLOWCE_PYTHON 0
#else
#define FLOWCE_PYTHON 1
#endif

#define LARGEDOUBLE 3e38

#ifdef XLIGHT
#ifdef FRANCAIS
#define lang 1
#else
#define lang 0
#endif
#else
extern int lang; // menufr.cc
#endif

  extern int xthetat;
extern Char* original_cfg;

  bool isalphanum(char c);
  bool isalpha(char c);
const char * paste_clipboard();
const char * select_var();
  void copy_clipboard(const std::string & s,bool status=true);
  void delete_clipboard();
void get_current_console_menu(std::string & menu,std::string & shiftmenu,std::string & alphamenu,int &menucolorbg,int app);
void set_xcas_status();
void console_disp_status(int keyflag);
  int confirm(const char * msg1,const char * msg2,bool acexit=false);
  bool confirm_overwrite();
  void invalid_varname();
  const char * keytostring(int key,int keyflag,bool py);
  int chartab();
void text_disp_menu(int keyflag);
bool tooltip(int x,int y,int pos,const char * editline);
  
int print_color(int print_x,int print_y,const char *s,int color,bool invert,bool minimini);
  void print_alpha_shift(int keyflag);
int print_msg12(const char * msg1,const char * msg2,int textY=40);
void PrintMini(int x,int y,const char * s,int mode);
void insert(std::string & s,int pos,const char * add);
int inputline(const char * msg1,const char * msg2,std::string & s,bool numeric,int ypos=65);
  void set_pixel(int xc,int yc,unsigned short color);
  extern "C" int clip_ymin; // 0 or 24
  void draw_line(int x1, int y1, int x2, int y2, int color=0,unsigned short motif=0xffff);
  void draw_circle(int xc,int yc,int r,int color,bool q1=true,bool q2=true,bool q3=true,bool q4=true);
  void draw_arc(int xc,int yc,int rx,int ry,int color,double theta1,double theta2);
  void draw_arc(int xc,int yc,int rx,int ry,int color,double t1, double t2,bool q1,bool q2,bool q3,bool q4);
  void draw_filled_circle(int xc,int yc,int r,int color,bool left=true,bool right=true);
  void draw_rectangle(int x, int y, int width, int height, unsigned short color);
void draw_filled_polygon(std::vector< std::vector<int> > & L,int xmin=0,int xmax=384,int ymin=0,int ymax=218,int color=0);
  void draw_polygon(std::vector< std::vector<int> > & L,int color=0);
  void draw_filled_arc(int x,int y,int rx,int ry,int theta1_deg,int theta2_deg,int color=0,int xmin=0,int xmax=384,int ymin=0,int ymax=218,bool segment=false);

void printCentered(const char* text, int y) ;
const char * input_matrix(bool list);
std::string help_insert(const char * cmdline,int & back,bool warn);
std::string printint(int );
int giacmin(int a,int b);
int giacmax(int a,int b);

#define MAX_FILENAME_SIZE 32 // was 270
//#define CONSOLESTATEFILE (char*)DATAFOLDER"\\khicas.erd"

struct DISPBOX {
  int     left;
  int     top;
  int     right;
  int     bottom;
  Char mode;
} ;

enum CONSOLE_SCREEN_SPEC{
  LINE_MAX = 64,
  LINE_DISP_MAX = 14,
  COL_DISP_MAX = 39,
  EDIT_LINE_MAX = 2048
};
#define CONSOLE_ROW_PX 15 // height of a console row (vfontsize in console.cc)

struct location{
  int x;
  int y;
};


enum CONSOLE_RETURN_VAL{
  CONSOLE_NEW_LINE_SET = 1,
  CONSOLE_SUCCEEDED = 0,
  CONSOLE_MEM_ERR = -1,
  CONSOLE_ARG_ERR = -2,
  CONSOLE_NO_EVENT = -3
};

enum CONSOLE_CURSOR_DIRECTION{
  CURSOR_UP,
  CURSOR_DOWN,
  CURSOR_LEFT,
  CURSOR_RIGHT,
  CURSOR_SHIFT_LEFT,
  CURSOR_SHIFT_RIGHT,
  CURSOR_ALPHA_UP,
  CURSOR_ALPHA_DOWN,
};

enum CONSOLE_LINE_TYPE{
  LINE_TYPE_INPUT=0,
  LINE_TYPE_OUTPUT=1,
  LINE_TYPE_CONT=2 // continuation row of a result drawn in 2D (see console_displine)
};

enum CONSOLE_CASE{
  LOWER_CASE,
  UPPER_CASE
};

struct line{
  Char *str=nullptr;
  short int readonly;
  short int type;
  int start_col;
  int disp_len;
};
extern struct line * Line;
extern int Start_Line, Last_Line,editline_cursor;

struct FMenu{
  char* name;
  char** str;
  Char count;
};

extern struct location Cursor;

#define MAX_FMENU_ITEMS 8
#define FMENU_TITLE_LENGHT 4

#define is_wchar(c) (((unsigned char)c == 0x7F) || ((unsigned char)c == 0xF7) || ((unsigned char)c == 0xF9) || ((unsigned char)c == 0xE5) || ((unsigned char)c == 0xE6) || ((unsigned char)c == 0xE7))
#define printf(s) Console_Output((const Char *)s);

int Console_DelStr(Char *str, int end_pos, int n);
int Console_InsStr(Char *dest, const Char *src, int disp_pos);
int Console_GetActualPos(const Char *str, int disp_pos);
int Console_GetDispLen(const Char *str);
int Console_MoveCursor(int direction);
int Console_Input(const Char *str);
int Console_Output(const Char *str);
void Console_Clear_EditLine();
int Console_NewLine(int pre_line_type, int pre_line_readonly);
int Console_Backspace(void);
int Console_GetKey(void);
int Console_Init(void);
void console_displine(int i,int redraw_mode);
int Console_Disp(int redraw_mode); // bit0=0 means minimal redraw (current line only)
int Console_FMenu(int key);
extern char menu_f1[16],menu_f2[16],menu_f3[16],menu_f4[16],menu_f5[16],menu_f6[16];
const char * console_menu(int key,Char* cfg,int active_app);
const char * console_menu(int key,int active_app);
void Console_FMenu_Init(void);
const char * Console_Draw_FMenu(int key, struct FMenu* menu,Char * cfg_,int active_app);
char *Console_Make_Entry(const Char* str);
const Char *Console_GetLine(void);
Char* Console_GetEditLine();
void Console_Draw_TeX_Popup(Char* str, int width, int height);
void dConsolePut(const char *);
void dConsolePutChar(const char );
void dConsoleRedraw(void);
extern int dconsole_mode;
extern int console_changed; // 1 if something new in history
extern char session_filename[MAX_FILENAME_SIZE+1];
unsigned translate_fkey(unsigned input_key);
bool inputdouble(const char * msg1,double & d);
#endif
