// focus.h - the Focus interface of the console (focus.cc, notes/ui): the expression being edited
// large and centered, past calculations stacked above it. It only draws: editing, history
// navigation and evaluation stay the console's (console.cc).
#ifndef FOCUS_H
#define FOCUS_H

#ifdef __cplusplus
extern "C" {
#endif
extern int focus_on;                    // 1: the console is drawn by focus.cc
void focus_status(void);                // status bar (statusline / statusflags in focus mode)
void focus_status_msg(const char * msg); // a message in the status bar ("computing...")
void focus_idle(void); // getkey, idle for a few seconds: slow refreshes (battery)
#ifdef __cplusplus
}

struct mi_metrics;
void focus_init();                      // palette; call once the LCD is in 8 bpp
void focus_disp(int redraw_mode);       // Console_Disp: bit 0 = everything, else the edit line
void focus_bar(int keyflag);            // the F-key bar (keyflag: 1 = 2nd, 4/8 = alpha)
void focus_evaluated();                 // after an evaluation: the result is shown large
int focus_clear_hero();                 // CLEAR on an empty edit line: 1 if it hid the result
const mi_metrics & focus_metrics();     // metrics of the edit line as drawn (caret moves)
int focus_result_line();                // the output line F4 cycles (selected, or shown large); -1
void focus_repaint(int y0, int y1);     // repaints stage rows [y0, y1) from the model
void focus_bar_redraw();                // repaints the F-key bar
void focus_invalidate();                // the next focus_disp repaints everything (after a full-screen view)
void focus_status_label(const char * s); // a view's label in the status bar ("COMMANDS"); 0: the console's
void focus_tab(int i, int on, int bank); // the standard layer's tab of F-key i (on: its menu is open)
void focus_bar_tabs(const char * const * labels, int on); // the F-key bar as 5 text tabs, tab on selected
enum { IC_X2, IC_INT, IC_WAVE, IC_PI, IC_FORMS, IC_MORE, IC_SEARCH };
void focus_icon(int ic, int x, int cy, int bank, int c, int bg); // the prototype's icons

// focus_menu.cc: F1-F5 and the math key open a popover over the dimmed stage. Returns FA_NONE
// (closed), FA_INSERT with the template to insert (*text, then the caret back *back chars), or
// an action for the console.
enum { FA_NONE, FA_INSERT, FA_CATALOG, FA_PLOT, FA_FILE, FA_CLEAR };
int focus_popover(int key, const char ** text, int * back);

// focus_catalog.cc: the command search. query: initial text (the word before the caret), may be
// empty. Returns 1 and fills out (0-terminated, at most outsize bytes) with the text to insert,
// or 0 when cancelled. On return the console redraws everything (Console_Disp(1)).
int focus_catalog(const char * query, char * out, int outsize);
#endif
#endif
