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
#ifdef __cplusplus
}

struct mi_metrics;
void focus_init();                      // palette; call once the LCD is in 8 bpp
void focus_disp(int redraw_mode);       // Console_Disp: bit 0 = everything, else the edit line
void focus_bar(int keyflag);            // the F-key bar (keyflag: 1 = 2nd, 4/8 = alpha)
void focus_evaluated();                 // after an evaluation: the result is shown large
int focus_clear_hero();                 // CLEAR on an empty edit line: 1 if it hid the result
const mi_metrics & focus_metrics();     // metrics of the edit line as drawn (caret moves)
#endif
#endif
