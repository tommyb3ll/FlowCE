#ifndef __TEXTGUI_H
#define __TEXTGUI_H
#include <string>
#include <vector>
#if !defined std
#define std ustl
#endif
typedef char Char;

const Char *toksplit(const Char *src, /* Source of tokens */
			      char tokchar, /* token delimiting char */
			      Char *token, /* receiver of parsed token */
			      int lgh); /* length token can receive */

  enum color_values {
    _BLACK=0,
    _RED=0xf800,
    _GREEN=0x0400,
    _YELLOW=0xffe0,
    _BLUE=0x001f,
    _MAGENTA=0xf81f,
    _CYAN=0x07ff,
    _WHITE=0xffff,
    _LINE_WIDTH_1 = 0,
    _LINE_WIDTH_2 = 1 << 16,
    _LINE_WIDTH_3 = 2 << 16,
    _LINE_WIDTH_4 = 3 << 16,
    _LINE_WIDTH_5 = 4 << 16,
    _LINE_WIDTH_6 = 5 << 16,
    _LINE_WIDTH_7 = 6 << 16,
    _LINE_WIDTH_8 = 7 << 16,
    _POINT_WIDTH_1 = 0,
    _POINT_WIDTH_2 = 1 << 19,
    _POINT_WIDTH_3 = 2 << 19,
    _POINT_WIDTH_4 = 3 << 19,
    _POINT_WIDTH_5 = 4 << 19,
    _POINT_WIDTH_6 = 5 << 19,
    _POINT_WIDTH_7 = 6 << 19,
    _POINT_WIDTH_8 = 7 << 19,
  };

bool is_alphanum(char c);
void chk_restart();
typedef short unsigned int color_t;
typedef struct textElement
{
  std::string s;
  color_t color=COLOR_BLACK;
  short int newLine=0; // if 1, new line will be drawn before the text
  short int spaceAtEnd=0;
  short int lineSpacing=0;
  short int minimini=0; // 0 inherit, 1 font 6x4, -1 font 8x6
  int nlines=1;
} textElement;

#define TEXTAREATYPE_NORMAL 0
#define TEXTAREATYPE_INSTANT_RETURN 1
typedef struct textArea
{
  int x=0;
  int y=0;
  int line=0,undoline=0;
  int pos=0,undopos=0;
  int clipline,undoclipline;
  int clippos,undoclippos;
  int width=LCD_WIDTH_PX;
  int lineHeight=16; // -2 if minimini is true
  std::vector<textElement> elements,undoelements;
  char* title = nullptr;
  std::string filename;
  int scrollbar=1;
  bool allowEXE=0; //whether to allow EXE to exit the screen
  bool allowF1=0; //whether to allow F1 to exit the screen
  bool editable=false;
  bool changed=false;
  bool minimini=false; // global font setting
  bool longlinescut=true;
  bool nostatus=false;
  int python=0;
  int type=TEXTAREATYPE_NORMAL;
  int cursorx=0,cursory=0; // position where the cursor should be displayed
} textArea;


#define TEXTAREA_RETURN_EXIT 0
#define TEXTAREA_RETURN_EXE 1
#define TEXTAREA_RETURN_F1 2
int doTextArea(textArea* text); //returns 0 when user EXITs, 1 when allowEXE is true and user presses EXE, 2 when allowF1 is true and user presses F1.
int focus_edit(textArea * text); // focus_edit.cc: doTextArea for editable texts, in the Focus look
std::string merge_area(const std::vector<textElement> & v);
bool save_script(const char * filename,const std::string & s);
void add(textArea *edptr,const std::string & s);

extern textArea * edptr;
int check_leave(textArea * text);
#endif 
