// ui_math.h - math layouts (mathinput.h) drawn with the Focus fonts and primitives.
#ifndef UI_MATH_H
#define UI_MATH_H

#include "mathinput.h" // last of the std-mapping headers (maps std to ustl on the calculator)

enum { UI_NSIZES = 8 };
extern const unsigned char ui_math_sizes[UI_NSIZES]; // 34 28 24 20 17 14 12 10 px

// metrics of size level lv (0 = 34 px ... 7 = 10 px); exponents use level lv + 2.
// flags: mathinput MI_F_* (MI_F_IMPLDOT for results).
const mi_metrics & ui_math_metrics(int lv, int flags);
// lays out s[0,n) at the largest level <= maxlv that fits w x h (the smallest level otherwise);
// returns the level
int ui_math_fit(const char * s, int n, int caret, int maxlv, int w, int h, int flags, mi_layout & L);
// draws L (built at level lv) with its baseline at y and left edge at x; ink over bg in bank;
// placeholders in acc
void ui_math_draw(const mi_layout & L, const char * s, int lv, int x, int y, int bank, int ink, int bg, int acc);
// the caret of L (a 2 px bar in acc)
void ui_math_caret(const mi_layout & L, int x, int y, int bank, int acc);

#endif
