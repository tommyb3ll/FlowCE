#ifndef MAIN_H
#define MAIN_H
#include "dbg.h"
#include <vector>
#include <string>
#if !defined std
#define std ustl
#endif
#define GEN_PRINT_BUFSIZE 1024
#ifdef XLIGHT
#ifdef FRANCAIS
#define lang 1
#else
#define lang 0
#endif
#else
extern int lang; // menufr.cc
#endif

#ifdef WITH_SHEET
void sheet();
#endif
extern bool freeze,freezeturtle;
void load_khicas_vars(const char * BUF);
int get_free_memory();
bool eqws(char * s,bool eval); // from main.cc, s must be at least 512 char
bool save_script(const char * filename,const std::string & s);
int run_script(const char* filename) ;
void erase_script();
void save(const char * fname); // save session
int restore_session(const char * fname);
void displaylogo();
std::string remove_path(const std::string & st);
void edit_script(const char * fname);
int find_color(const char * s); 
std::string khicas_state();
bool textedit(char * s);
void run(const char * s,int do_logo_graph_eqw=7);
bool stringtodouble(const std::string & s1,double & d);
std::string remove_extension(const std::string &);

struct logo_turtle {
  double x,y;
  float theta; // theta is given in degrees or radians dep. on angle_mode
  int s;
  bool visible:1; // true if turtle visible
  bool mark:1; // true if moving marks
  bool direct; // true if rond/disque is done in the trigonometric direction
  char turtle_length;
  int color;
  int radius; // 0 nothing, >0 -> draw a plain disk 
  // bit 0-8=radius, bit9-17 angle1, bit 18-26 angle2, bit 27=1 filled  or 0 
  // <0 fill a polygon from previous turtle positions
  logo_turtle(): x(35), y(35), theta(0), s(0), visible(true), mark(true), direct(true), turtle_length(5), color(0), radius(0) {}

  inline bool equal_except_nomark(const logo_turtle &t) const {
    return x==t.x && y==t.y && turtle_length==t.turtle_length && s==t.s && radius==t.radius;
  }
};
logo_turtle & turtle();

  struct Turtle {
    void draw();
    void * turtleptr_;
    int turtlex,turtley; // Turtle translate
    double turtlezoom; // Zoom factor for turtle screen
    int maillage; // 0 (none), 1 (square), 2 (triangle), bit3 used for printing
  };

inline int absint(int x){ return x<0?-x:x; }

int select_script_and_run();
int fileBrowser(char * filename, const char * ext, const char * title);

// MICROPY & color compat
extern "C" {
  int do_confirm(const char * s);
  const char * read_file(const char * filename);
  int load_script(const char * filename,std::string & s);
  int do_file(const char *file);
  char * micropy_init(int stack_size,int heap_size);
  int micropy_eval(const char * line);
  void  mp_deinit();
  void mp_stack_ctrl_init();
  int process_freeze(); // returns 1 if freezed
  extern int parser_errorline,parser_errorcol;
  void python_free();
  extern size_t pythonjs_stack_size,pythonjs_heap_size;
  extern char * python_heap,*pythonjs_static_heap;
  int python_init(int stack_size,int heap_size);

  int select_item(const char ** ptr,const char * title,bool askfor1=true);
  void turtle_freeze();
  void console_freeze();
  void console_output(const char * s,int l);
  const char * console_input(const char * msg1,const char * msg2,bool numeric,int ypos);
  int micropy_ck_eval(const char *line);
  const char * caseval(const char * s);
  int kbd_convert(int r,int c);
  int kbd_filter(int key); // replace up/down/right/left/exe/exit with Nuumworks keycode
  bool console_python_mode(); // console evaluates Python (or Python-compatible syntax)
  bool khicas_implicit_mult(char * line,int maxlen); // paper notation + TI implicit multiplication (tiinput_glue.cc)
}

#endif
