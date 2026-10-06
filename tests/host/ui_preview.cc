// ui_preview.cc - renders Focus UI drawing code on the PC (same C sources as the calculator,
// with a RAM framebuffer and palette) and writes a 320x240 PPM.
//   bash tests/host/run_ui_preview.sh out.png [scene]
#include <cstdio>
#include <cstring>
#include <string>
#include "ui_gfx.h"
#include "ui_font.h"
#include "ui_fontdata.h"
#include "ui_math.h"

extern "C" unsigned short * ui_host_palette(void);
extern "C" { volatile unsigned char focus_phase; } // the calculator's timing probe (focus.cc)

static void ppm(const char * path) {
  FILE * f = fopen(path, "wb");
  fprintf(f, "P6\n%d %d\n255\n", UI_W, UI_H);
  unsigned short * pal = ui_host_palette();
  for (int i = 0; i < UI_W * UI_H; ++i) {
    unsigned v = pal[ui_fb[i]];
    unsigned char rgb[3] = {(unsigned char)(((v >> 10) & 31) * 255 / 31), (unsigned char)(((v >> 5) & 31) * 255 / 31),
                            (unsigned char)((v & 31) * 255 / 31)}; // 1555, as the LCD palette
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
}

static void math_centered(const char * s, int maxlv, int x0, int y0, int w, int h, int ink, int flags, int caret) {
  mi_layout L;
  int lv = ui_math_fit(s, (int)strlen(s), caret, maxlv, w, h, flags, L);
  int x = x0 + (w - L.width) / 2, base = y0 + (h - (L.asc + L.desc)) / 2 + L.asc;
  ui_math_draw(L, s, lv, x, base, 0, ink, UC_BG, UC_ACC);
  if (caret >= 0) ui_math_caret(L, x, base, 0, UC_ACC);
}

int main(int argc, char ** argv) {
  const char * out = argc > 1 ? argv[1] : "preview.ppm";
  std::string scene = argc > 2 ? argv[2] : "hero";
  ui_set_theme(&ui_theme_paper);
  ui_noclip();
  ui_fill(0, 0, UI_W, UI_H, ui_col(0, UC_BG));
  const unsigned char * sub = ui_ramp(0, UC_SUB, UC_BG);
  if (scene == "hero") {
    ui_draw_text(&ui_tb9, "RAD   EXACT", -1, 8, 11, sub, 0);
    // previous answer peeking at the top
    math_centered("4*asin(3*x-1)/3", 3, 120, 16, 196, 30, UC_INK, MI_F_IMPLDOT, -1);
    ui_fill(12, 47, 296, 1, ui_col(0, UC_LINE));
    math_centered("integrate(50x^3 sqrt(1-25x^2),x)", 4, 12, 52, 296, 36, UC_SUB, 0, -1);
    math_centered("-2*(75*x^2+2)*(1-25*x^2)^(3/2)/375", 0, 12, 92, 296, 96, UC_INK, MI_F_IMPLDOT, -1);
    ui_rrect(0, 130, 196, 60, 14, 6, UC_ACCSOFT, UC_BG);
    ui_draw_text(&ui_tb9, "exact", -1, 137, 207, ui_ramp(0, UC_ACC, UC_ACCSOFT), 0);
    ui_draw_text(&ui_tb9, "F4", -1, 170, 207, ui_ramp(0, UC_SUB, UC_ACCSOFT), 0);
    ui_fill(0, 218, UI_W, 22, ui_col(0, UC_BAR));
    ui_fill(0, 218, UI_W, 1, ui_col(0, UC_LINE));
    const char * tabs[] = {"algebra", "calculus", "trig", "forms", "more"};
    for (int i = 0; i < 5; ++i) {
      int w = ui_text_width(&ui_tr9, tabs[i], -1);
      ui_draw_text(&ui_tr9, tabs[i], -1, 32 + 64 * i - w / 2 + 6, 233, ui_ramp(0, UC_BARINK, UC_BAR), 0);
    }
  } else if (scene == "typing") {
    math_centered("integrate(4/sqrt(6x-9x^2),x)", 0, 12, 30, 296, 170, UC_INK, 0, 26);
  } else if (scene == "deco") { // radicals, integrals, sums, tall parentheses at 2 sizes
    const char * ex[] = {"sqrt(1-x^2)+(x^2+1)^3", "integrate(x*e^x,x,0,1)+sum(1/k^2,k,1,oo)", "((x+1)/(x-1))^2*{1,2}"};
    int y = 6;
    for (int i = 0; i < 3; ++i) {
      math_centered(ex[i], 0, 4, y, 312, 46, UC_INK, 0, -1);
      math_centered(ex[i], 4, 4, y + 46, 312, 30, UC_INK, 0, -1);
      y += 78;
    }
  } else if (scene == "pv") { // template previews (boxes)
    const char * ex[] = {"^2", "^()", "()^()", "diff(,x)", "limit(,x,)", "sum(,k,,)", "factor()", "pi"};
    for (int i = 0; i < 8; ++i) {
      mi_layout L;
      int lv = ui_math_fit(ex[i], (int)strlen(ex[i]), -1, 2, 150, 56, MI_F_CALLBOX, L);
      int x = 4 + (i % 2) * 160, y = 4 + (i / 2) * 58;
      ui_math_draw(L, ex[i], lv, x + (150 - L.width) / 2, y + (56 - L.asc - L.desc) / 2 + L.asc, 0, UC_INK, UC_BG, UC_ACC);
    }
  } else if (scene == "sizes") {
    int y = 4;
    for (int lv = 0; lv < UI_NSIZES; ++lv) {
      mi_layout L;
      const char * s = "x^2+sin(y)=pi/2";
      mi_build(s, (int)strlen(s), -1, ui_math_metrics(lv, 0), L);
      ui_math_draw(L, s, lv, 8, y + L.asc, 0, UC_INK, UC_BG, UC_ACC);
      y += L.asc + L.desc + 2;
    }
  }
  ppm(out);
  return 0;
}
