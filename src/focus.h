// focus.h - the Focus interface of the console (focus.cc, notes/ui): the expression being edited
// large and centered, past calculations stacked above it. It only draws: editing, history
// navigation and evaluation stay the console's (console.cc).
#ifndef FOCUS_H
#define FOCUS_H

#ifdef __cplusplus
extern "C" {
#endif
#define focus_on 1 // the console is drawn by focus.cc (a constant: the classic screen's paths compile away)
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
int focus_hist_line(int dir);           // history by calculations: the line for up/down/left/right
// a card over the dimmed stage: a title, n choices; returns the index chosen, -1 if cancelled
int focus_choose(const char * title, const char * const * labels, int n);
// a list card under a title (0: none) for KhiCAS's own menus (doMenu); a long list scrolls.
// sel: the item selected first. Returns the item chosen, -1 if cancelled.
int focus_list(const char * title, const char * const * labels, int n, int sel, const char * const * hints = 0);
void focus_note(const char * title, const char * text); // a message card; the caller reads the key
void focus_text(const char * title, const char * text); // About, Shortcuts: a text to read
void focus_splash(int first);           // the start screen (first: tips, "press any key")
// the graph's table of values: head[ncol], cells row by row, hint on the last line
void focus_table(const char * const * head, int ncol, const char * const * cells, int nrow, const char * hint);
// focus_prompt(title, label, std::string & s, numeric): a value prompt (KhiCAS's inputline) as
// a card, KEY_CTRL_EXE or KEY_CTRL_EXIT (declared where std::string is known: console.cc)
// KhiCAS's F-key menus (console_menu) as cards: idx = key - F1 after console_menu's renumbering.
// Returns the entry chosen, -1 if cancelled, -2 - j to open menu j instead.
int focus_fmenu(int idx, const char * const * entries, int n);
int focus_screen();                     // 1 if the screen shows the Focus console (popovers can dim it)
extern int focus_view;                  // 1 while another view (the graph) owns the screen: popovers
                                        // do not repaint the console when they close
void focus_bar_redraw();                // repaints the F-key bar
void focus_bar_reset();                 // repaints the F-key bar, plain layer
void focus_invalidate();                // the next focus_disp repaints everything (after a full-screen view)
void focus_status_label(const char * s); // a view's label in the status bar ("COMMANDS"); 0: the console's
void focus_tab(int i, int on, int bank); // the standard layer's tab of F-key i (on: its menu is open)
void focus_bar_tabs(const char * const * labels, int on); // the F-key bar as 5 text tabs, tab on selected
enum { IC_X2, IC_INT, IC_WAVE, IC_PI, IC_FORMS, IC_MORE, IC_SEARCH };
void focus_icon(int ic, int x, int cy, int bank, int c, int bg); // the prototype's icons

// focus_menu.cc: F1-F5 and the math key open a popover over the dimmed stage. Returns FA_NONE
// (closed), FA_INSERT with the template to insert (*text, then the caret back *back chars), or
// an action for the console.
enum { FA_NONE, FA_INSERT, FA_CATALOG, FA_PLOT, FA_FILE, FA_CLEAR, FA_GRAPH, FA_THEME };
void focus_toggle_theme();             // Paper <-> Night, at once (the palette), saved in the appvar FocusUI
int focus_popover(int key, const char ** text, int * back);

// focus_catalog.cc: the command search. query: initial text (the word before the caret), may be
// empty. Returns 1 and fills out (0-terminated, at most outsize bytes) with the text to insert,
// or 0 when cancelled. On return the console redraws everything (Console_Disp(1)).
int focus_catalog(const char * query, char * out, int outsize);
#endif
#endif
