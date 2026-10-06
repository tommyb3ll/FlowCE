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
void focus_tab(int i, int on, int bank); // the standard layer's tab of F-key i (on: its menu is open)
void focus_icon(int ic, int x, int cy, int bank, int c, int bg); // the prototype's icons

// focus_menu.cc: F1-F5 and the math key open a popover over the dimmed stage. Returns FA_NONE
// (closed), FA_INSERT with the template to insert (*text, then the caret back *back chars), or
// an action for the console.
enum { FA_NONE, FA_INSERT, FA_CATALOG, FA_PLOT, FA_FILE, FA_CLEAR };
int focus_popover(int key, const char ** text, int * back);
#endif
#endif
