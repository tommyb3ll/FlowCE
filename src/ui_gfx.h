// ui_gfx.h - drawing primitives of the Focus interface on the 8 bpp screen.
// Colors live in palette entries 128-255, which the rest of KhiCAS never uses (its palette is
// the 128-entry 2-3-2 cube, graphic.c): bank 0 (128-191) for the scene, bank 1 (192-255) for
// popovers drawn over it. ui_dim(1) darkens the scene bank only, so the whole scene behind a
// popover dims at once without a redraw.
// Anti-aliasing: a "ramp" is 4 palette indices {background, 1/3, 2/3, ink} of an ink color
// over a known background color; drawing keeps the darker shade where shapes overlap.
#ifndef UI_GFX_H
#define UI_GFX_H

#ifdef __cplusplus
extern "C" {
#endif

#define UI_W 320
#define UI_H 240

extern unsigned char * ui_fb;               // drawing target: the screen (lcd_Ram, 8 bpp) or a band
extern int ui_cx0, ui_cy0, ui_cx1, ui_cy1;  // clip rectangle [x0,x1) x [y0,y1)
void ui_clip(int x0, int y0, int x1, int y1); // set the clip (intersected with the screen or band)
void ui_noclip(void);

// Bands: rows of the screen drawn into a RAM strip, then copied to the screen at once, so a
// repaint never shows a blank frame (there is no back buffer: the second half of VRAM is giac's
// heap). Drawing code is unchanged: inside a band, ui_fb points so that screen coordinates land
// in the strip, and the clip never leaves the band.
int ui_band_open(int maxrows);       // allocates the strip (fewer rows when memory is short);
                                     // returns its rows, 0 = none (draw on the screen directly)
void ui_band_close(void);            // frees it
void ui_band_begin(int y0, int y1);  // screen rows [y0, y1), at most the strip's rows
void ui_band_end(void);              // copies the band to the screen

// theme colors
enum { UC_BG, UC_INK, UC_SUB, UC_LINE, UC_ACC, UC_ACCSOFT, UC_ONACC, UC_CARD, UC_BAR, UC_BARINK,
       UC_GREEN, UC_SHADOW, UC_WHITE, UC_COUNT };
typedef struct { unsigned char rgb[UC_COUNT][3]; unsigned char dim_to[3], dim_pct; } ui_theme;
extern const ui_theme ui_theme_paper;

void ui_set_theme(const ui_theme * t);      // (re)writes every palette entry in use
unsigned char ui_col(int bank, int c);      // palette index of color c in bank 0/1
const unsigned char * ui_ramp(int bank, int fg, int bg); // allocated on first use
void ui_dim(int on);                         // darken / restore the scene bank
void ui_restore_os_palette(void);            // not needed today: KhiCAS uses 0-127 only

void ui_fill(int x, int y, int w, int h, unsigned char idx);
// rounded rectangle filled with color c over background bg (anti-aliased corners)
void ui_rrect(int bank, int x, int y, int w, int h, int r, int c, int bg);
// 1 px rounded outline in color c over background bg
void ui_rframe(int bank, int x, int y, int w, int h, int r, int c, int bg);
// anti-aliased thick segment / polyline; coordinates and thickness in 1/16 pixel
void ui_seg16(int ax, int ay, int bx, int by, int th, const unsigned char * ramp);
void ui_poly16(const int * xy, int n, int th, const unsigned char * ramp);

#ifdef __cplusplus
}
#endif
#endif
