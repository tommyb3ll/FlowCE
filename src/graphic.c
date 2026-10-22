#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <sys/lcd.h>

#include "graphic.h"

#include <ti/ui.h>

#include "dbg.h"

// fontes
#include "ascii_font.h"

// ti screen
#define ti8bpp_screen ((unsigned char*)lcd_Ram)

static const uint8_t minBrightness = 255;//229; // Do NOT set higher, may damage the hardware.
static const uint8_t maxBrightness = 49; // Do NOT set lower, may damage the hardware.

// extern "C" {

void vGL_FlushVScreen(){}

void vGL_set_pixel(unsigned x,unsigned y,int c){
  if (x>=VIR_LCD_PIX_W || y>=VIR_LCD_PIX_H)
    return;
  ti8bpp_screen[x + y * VIR_LCD_PIX_W] = c;
}

int vGL_get_pixel(unsigned int x,unsigned int y){
  if (x >= VIR_LCD_PIX_W || y >= VIR_LCD_PIX_H) 
    return -1;
  return ti8bpp_screen[x + y * VIR_LCD_PIX_W];
}  

inline int my_min(int a,int b){
  return a<b?a:b;
}

void vGL_putChar(unsigned int x0, unsigned int y0, char ch, unsigned char fg, unsigned char bg, const unsigned char * charptrbase, int font_w, int font_h) {
  if (x0>=VIR_LCD_PIX_W || y0>=VIR_LCD_PIX_H)
    return;
  if ((ch < ' ') || (ch > '~' + 1)) {
    return;
  }
  if (ch==' '){
    vGL_setArea(x0,y0,x0+font_w,y0+font_h,bg);
    return;
  }
  const unsigned char * pCh = charptrbase + (ch - ' ') * font_h;
  const int shift=my_min(font_w,VIR_LCD_PIX_W-x0); // shift>0
  unsigned char * ptry = ti8bpp_screen + x0 + (y0 * VIR_LCD_PIX_W);
  int tmp=VIR_LCD_PIX_H-y0;
  if (font_h<tmp)
    tmp=font_h;
  const unsigned char * ptryend=ptry+(tmp * VIR_LCD_PIX_W);
  for (;ptry<ptryend;ptry+=VIR_LCD_PIX_W,++pCh){
    unsigned char * ptr=ptry;
    unsigned char * ptrend = ptr+shift;
    unsigned char val=*pCh;
    if (val==0){
      for (;ptr<ptrend;++ptr)
        *ptr=bg;
    }
    else {
#if 1 // loop unroll, at most 8 iterations (font_w<=8)
      *ptr=(val & 0x80U)?fg:bg;
      ++ptr;
      if (ptr==ptrend) continue;
      *ptr=(val & 0x40U)?fg:bg;
      ++ptr;
      if (ptr==ptrend) continue;
      *ptr=(val & 0x20U)?fg:bg;
      ++ptr;
      if (ptr==ptrend) continue;
      *ptr=(val & 0x10U)?fg:bg;
      ++ptr;
      if (ptr==ptrend) continue;
      *ptr=(val & 0x8U)?fg:bg;
      ++ptr;
      if (ptr==ptrend) continue;
      *ptr=(val & 0x4U)?fg:bg;
      ++ptr;
      if (ptr==ptrend) continue;
      *ptr=(val & 0x2U)?fg:bg;
      ++ptr;
      if (ptr==ptrend) continue;
      *ptr=(val & 0x1U)?fg:bg;
#else
      for (;ptr<ptrend;++ptr){
        *ptr=(val & 0x80U)?fg:bg;
        val <<= 1;
      }
#endif
    }
  }
}

void vGL_putString(int x0, int y0, const char *s, unsigned char fg, unsigned char bg, int fontSize) {
  //dbg_printf("putstring x=%i y=%i s=%s fontSize=%i fg=%i bg=%i\n",x0,y0,s,fontSize,fg,bg);
  const unsigned char * charptrbase = VGA_Ascii_7x14;
  if (fontSize <= 16) {
    int x = 0;
    int y = 0;
    int font_h;
    int font_w;
    switch (fontSize) {
    case 8:
      font_w = 5;
      font_h = 8;
      charptrbase= VGA_Ascii_5x8;
      break;
      /*
    case 12:
      font_w = 8;
      font_h = 12;
      break;
      */
    case 14:
      font_w = 7;
      font_h = 14;
      break;
    case 16:
      font_w = 8;
      font_h = 16;
      charptrbase=VGA_Ascii_8x16;
      break;
    default:
      font_w = 7;
      font_h = 14;
      break;
    }
    
    while (*s) {
      vGL_putChar(x0 + x, y0 + y, *s, fg, bg, charptrbase,font_w,font_h);
      s++;
      x += font_w;
      if (x > VIR_LCD_PIX_W) {
        break; // otherwise goes next line?
        x = 0;
        y += font_h;
        if (y > VIR_LCD_PIX_H) {
          break;
        }
      }
    }
  }
  
}

void vGL_clearArea(unsigned int x0, unsigned int y0, unsigned int x1, unsigned int y1) {
  vGL_setArea(x0,y0,x1,y1,GL_WHITE);
}

void vGL_setArea(unsigned int x0, unsigned int y0, unsigned int x1, unsigned int y1, unsigned int color) {
  if (x0 >= VIR_LCD_PIX_W) 
    x0 = VIR_LCD_PIX_W ;
  if (y0 >= VIR_LCD_PIX_H) 
    y0 = VIR_LCD_PIX_H ;
  if (x1 >= VIR_LCD_PIX_W)
    x1 = VIR_LCD_PIX_W ;
  if ((y1 >= VIR_LCD_PIX_H))
    y1 = VIR_LCD_PIX_H ;
  
  //dbg_printf("set area color=%i x0=%i,y0=%i,x1=%i,y1=%i\n", color,x0, y0, x1, y1);
#if 0
  for (int y = y0; y < y1; y++){
    for (int x = x0; x < x1; x++) {
      //vGL_set_pixel(x,y,color);
      ti8bpp_screen[x + y * VIR_LCD_PIX_W] = color;
    }
  }  
#else
  const unsigned color3=(color<<16)|(color<<8)|color;
  for (int y = y0; y < y1; y++){
    unsigned char * ptr=ti8bpp_screen+x0+y*VIR_LCD_PIX_W,*ptrend=ptr+x1-x0;
    ptrend-=2;
    for (; ptr<ptrend; ptr+=3) {
      //vGL_set_pixel(x,y,color);
      *(unsigned *) ptr = color3;
    }
    ptrend +=2;
    if (ptr<ptrend){
      *ptr=color;
      ++ptr;
      if (ptr<ptrend){
        *ptr=color;
        ++ptr;
      }
    }
  }
#endif
}


void vGL_reverseArea(unsigned int x0, unsigned int y0, unsigned int x1, unsigned int y1) {
    if ((x0 >= VIR_LCD_PIX_W)) {
        x0 = VIR_LCD_PIX_W - 1;
    }
    if ((y0 >= VIR_LCD_PIX_H)) {
        y0 = VIR_LCD_PIX_H - 1;
    }
    if ((x1 >= VIR_LCD_PIX_W)) {
        x1 = VIR_LCD_PIX_W - 1;
    }
    if ((y1 >= VIR_LCD_PIX_H)) {
        y1 = VIR_LCD_PIX_H - 1;
    }
    for (int y = y0; y < y1; y++){
      for (int x = x0; x < x1; x++) {
        ti8bpp_screen[x + y * VIR_LCD_PIX_W] = ~ti8bpp_screen[x + y * VIR_LCD_PIX_W];
      }
    }
}


void vGL_End(){
  //boot_ClearVRAM();
  lcd_Control = 0b100100101101; // TI-OS default
  // _boot_InitializeHardware
  ((void(*)(void))0x000384)();
}

inline uint16_t sdk_rgb(int a,int b,int c){
  return (((a*32)/256)<<11) | (((b*64)/256)<<5) | ((c*32)/256);
}

/*
lcd_Palette[0] R=0/255 G=0/255 B=0/255 RGB565=0
lcd_Palette[1] R=0/255 G=0/255 B=85/255 RGB565=a
lcd_Palette[2] R=0/255 G=0/255 B=170/255 RGB565=15
lcd_Palette[3] R=0/255 G=0/255 B=255/255 RGB565=1f
lcd_Palette[4] R=0/255 G=36/255 B=0/255 RGB565=120
lcd_Palette[5] R=0/255 G=36/255 B=85/255 RGB565=12a
lcd_Palette[6] R=0/255 G=36/255 B=170/255 RGB565=135
lcd_Palette[7] R=0/255 G=36/255 B=255/255 RGB565=13f
lcd_Palette[8] R=0/255 G=72/255 B=0/255 RGB565=240
lcd_Palette[9] R=0/255 G=72/255 B=85/255 RGB565=24a
lcd_Palette[10] R=0/255 G=72/255 B=170/255 RGB565=255
lcd_Palette[11] R=0/255 G=72/255 B=255/255 RGB565=25f
lcd_Palette[12] R=0/255 G=109/255 B=0/255 RGB565=360
lcd_Palette[13] R=0/255 G=109/255 B=85/255 RGB565=36a
lcd_Palette[14] R=0/255 G=109/255 B=170/255 RGB565=375
lcd_Palette[15] R=0/255 G=109/255 B=255/255 RGB565=37f
lcd_Palette[16] R=0/255 G=145/255 B=0/255 RGB565=480
lcd_Palette[17] R=0/255 G=145/255 B=85/255 RGB565=48a
lcd_Palette[18] R=0/255 G=145/255 B=170/255 RGB565=495
lcd_Palette[19] R=0/255 G=145/255 B=255/255 RGB565=49f
lcd_Palette[20] R=0/255 G=182/255 B=0/255 RGB565=5a0
lcd_Palette[21] R=0/255 G=182/255 B=85/255 RGB565=5aa
lcd_Palette[22] R=0/255 G=182/255 B=170/255 RGB565=5b5
lcd_Palette[23] R=0/255 G=182/255 B=255/255 RGB565=5bf
lcd_Palette[24] R=0/255 G=218/255 B=0/255 RGB565=6c0
lcd_Palette[25] R=0/255 G=218/255 B=85/255 RGB565=6ca
lcd_Palette[26] R=0/255 G=218/255 B=170/255 RGB565=6d5
lcd_Palette[27] R=0/255 G=218/255 B=255/255 RGB565=6df
lcd_Palette[28] R=0/255 G=255/255 B=0/255 RGB565=7e0
lcd_Palette[29] R=0/255 G=255/255 B=85/255 RGB565=7ea
lcd_Palette[30] R=0/255 G=255/255 B=170/255 RGB565=7f5
lcd_Palette[31] R=0/255 G=255/255 B=255/255 RGB565=7ff
lcd_Palette[32] R=85/255 G=0/255 B=0/255 RGB565=5000
lcd_Palette[33] R=85/255 G=0/255 B=85/255 RGB565=500a
lcd_Palette[34] R=85/255 G=0/255 B=170/255 RGB565=5015
lcd_Palette[35] R=85/255 G=0/255 B=255/255 RGB565=501f
lcd_Palette[36] R=85/255 G=36/255 B=0/255 RGB565=5120
lcd_Palette[37] R=85/255 G=36/255 B=85/255 RGB565=512a
lcd_Palette[38] R=85/255 G=36/255 B=170/255 RGB565=5135
lcd_Palette[39] R=85/255 G=36/255 B=255/255 RGB565=513f
lcd_Palette[40] R=85/255 G=72/255 B=0/255 RGB565=5240
lcd_Palette[41] R=85/255 G=72/255 B=85/255 RGB565=524a
lcd_Palette[42] R=85/255 G=72/255 B=170/255 RGB565=5255
lcd_Palette[43] R=85/255 G=72/255 B=255/255 RGB565=525f
lcd_Palette[44] R=85/255 G=109/255 B=0/255 RGB565=5360
lcd_Palette[45] R=85/255 G=109/255 B=85/255 RGB565=536a
lcd_Palette[46] R=85/255 G=109/255 B=170/255 RGB565=5375
lcd_Palette[47] R=85/255 G=109/255 B=255/255 RGB565=537f
lcd_Palette[48] R=85/255 G=145/255 B=0/255 RGB565=5480
lcd_Palette[49] R=85/255 G=145/255 B=85/255 RGB565=548a
lcd_Palette[50] R=85/255 G=145/255 B=170/255 RGB565=5495
lcd_Palette[51] R=85/255 G=145/255 B=255/255 RGB565=549f
lcd_Palette[52] R=85/255 G=182/255 B=0/255 RGB565=55a0
lcd_Palette[53] R=85/255 G=182/255 B=85/255 RGB565=55aa
lcd_Palette[54] R=85/255 G=182/255 B=170/255 RGB565=55b5
lcd_Palette[55] R=85/255 G=182/255 B=255/255 RGB565=55bf
lcd_Palette[56] R=85/255 G=218/255 B=0/255 RGB565=56c0
lcd_Palette[57] R=85/255 G=218/255 B=85/255 RGB565=56ca
lcd_Palette[58] R=85/255 G=218/255 B=170/255 RGB565=56d5
lcd_Palette[59] R=85/255 G=218/255 B=255/255 RGB565=56df
lcd_Palette[60] R=85/255 G=255/255 B=0/255 RGB565=57e0
lcd_Palette[61] R=85/255 G=255/255 B=85/255 RGB565=57ea
lcd_Palette[62] R=85/255 G=255/255 B=170/255 RGB565=57f5
lcd_Palette[63] R=85/255 G=255/255 B=255/255 RGB565=57ff
lcd_Palette[64] R=170/255 G=0/255 B=0/255 RGB565=a800
lcd_Palette[65] R=170/255 G=0/255 B=85/255 RGB565=a80a
lcd_Palette[66] R=170/255 G=0/255 B=170/255 RGB565=a815
lcd_Palette[67] R=170/255 G=0/255 B=255/255 RGB565=a81f
lcd_Palette[68] R=170/255 G=36/255 B=0/255 RGB565=a920
lcd_Palette[69] R=170/255 G=36/255 B=85/255 RGB565=a92a
lcd_Palette[70] R=170/255 G=36/255 B=170/255 RGB565=a935
lcd_Palette[71] R=170/255 G=36/255 B=255/255 RGB565=a93f
lcd_Palette[72] R=170/255 G=72/255 B=0/255 RGB565=aa40
lcd_Palette[73] R=170/255 G=72/255 B=85/255 RGB565=aa4a
lcd_Palette[74] R=170/255 G=72/255 B=170/255 RGB565=aa55
lcd_Palette[75] R=170/255 G=72/255 B=255/255 RGB565=aa5f
lcd_Palette[76] R=170/255 G=109/255 B=0/255 RGB565=ab60
lcd_Palette[77] R=170/255 G=109/255 B=85/255 RGB565=ab6a
lcd_Palette[78] R=170/255 G=109/255 B=170/255 RGB565=ab75
lcd_Palette[79] R=170/255 G=109/255 B=255/255 RGB565=ab7f
lcd_Palette[80] R=170/255 G=145/255 B=0/255 RGB565=ac80
lcd_Palette[81] R=170/255 G=145/255 B=85/255 RGB565=ac8a
lcd_Palette[82] R=170/255 G=145/255 B=170/255 RGB565=ac95
lcd_Palette[83] R=170/255 G=145/255 B=255/255 RGB565=ac9f
lcd_Palette[84] R=170/255 G=182/255 B=0/255 RGB565=ada0
lcd_Palette[85] R=170/255 G=182/255 B=85/255 RGB565=adaa
lcd_Palette[86] R=170/255 G=182/255 B=170/255 RGB565=adb5
lcd_Palette[87] R=170/255 G=182/255 B=255/255 RGB565=adbf
lcd_Palette[88] R=170/255 G=218/255 B=0/255 RGB565=aec0
lcd_Palette[89] R=170/255 G=218/255 B=85/255 RGB565=aeca
lcd_Palette[90] R=170/255 G=218/255 B=170/255 RGB565=aed5
lcd_Palette[91] R=170/255 G=218/255 B=255/255 RGB565=aedf
lcd_Palette[92] R=170/255 G=255/255 B=0/255 RGB565=afe0
lcd_Palette[93] R=170/255 G=255/255 B=85/255 RGB565=afea
lcd_Palette[94] R=170/255 G=255/255 B=170/255 RGB565=aff5
lcd_Palette[95] R=170/255 G=255/255 B=255/255 RGB565=afff
lcd_Palette[96] R=255/255 G=0/255 B=0/255 RGB565=f800
lcd_Palette[97] R=255/255 G=0/255 B=85/255 RGB565=f80a
lcd_Palette[98] R=255/255 G=0/255 B=170/255 RGB565=f815
lcd_Palette[99] R=255/255 G=0/255 B=255/255 RGB565=f81f
lcd_Palette[100] R=255/255 G=36/255 B=0/255 RGB565=f920
lcd_Palette[101] R=255/255 G=36/255 B=85/255 RGB565=f92a
lcd_Palette[102] R=255/255 G=36/255 B=170/255 RGB565=f935
lcd_Palette[103] R=255/255 G=36/255 B=255/255 RGB565=f93f
lcd_Palette[104] R=255/255 G=72/255 B=0/255 RGB565=fa40
lcd_Palette[105] R=255/255 G=72/255 B=85/255 RGB565=fa4a
lcd_Palette[106] R=255/255 G=72/255 B=170/255 RGB565=fa55
lcd_Palette[107] R=255/255 G=72/255 B=255/255 RGB565=fa5f
lcd_Palette[108] R=255/255 G=109/255 B=0/255 RGB565=fb60
lcd_Palette[109] R=255/255 G=109/255 B=85/255 RGB565=fb6a
lcd_Palette[110] R=255/255 G=109/255 B=170/255 RGB565=fb75
lcd_Palette[111] R=255/255 G=109/255 B=255/255 RGB565=fb7f
lcd_Palette[112] R=255/255 G=145/255 B=0/255 RGB565=fc80
lcd_Palette[113] R=255/255 G=145/255 B=85/255 RGB565=fc8a
lcd_Palette[114] R=255/255 G=145/255 B=170/255 RGB565=fc95
lcd_Palette[115] R=255/255 G=145/255 B=255/255 RGB565=fc9f
lcd_Palette[116] R=255/255 G=182/255 B=0/255 RGB565=fda0
lcd_Palette[117] R=255/255 G=182/255 B=85/255 RGB565=fdaa
lcd_Palette[118] R=255/255 G=182/255 B=170/255 RGB565=fdb5
lcd_Palette[119] R=255/255 G=182/255 B=255/255 RGB565=fdbf
lcd_Palette[120] R=255/255 G=218/255 B=0/255 RGB565=fec0
lcd_Palette[121] R=255/255 G=218/255 B=85/255 RGB565=feca
lcd_Palette[122] R=255/255 G=218/255 B=170/255 RGB565=fed5
lcd_Palette[123] R=255/255 G=218/255 B=255/255 RGB565=fedf
lcd_Palette[124] R=255/255 G=255/255 B=0/255 RGB565=ffe0
lcd_Palette[125] R=255/255 G=255/255 B=85/255 RGB565=ffea
lcd_Palette[126] R=255/255 G=255/255 B=170/255 RGB565=fff5
lcd_Palette[127] R=255/255 G=255/255 B=255/255 RGB565=ffff
 */
void vGL_Initialize() {
  //dbg_printf("vgl_init addr=%x lcd_Palette=%x w=%i h=%i\n",ti8bpp_screen, lcd_Palette,VIR_LCD_PIX_W,VIR_LCD_PIX_H);

  //lcd_Palette[0xFF] = 0xFFFF; // white also
  // the screen's half of VRAM only (boot_ClearVRAM cleared it all): the other half is the
  // allocator's (allocator_custom.c), and the C++ constructors run before main allocate there
  // already (the C runtime's atexit record of focus_menu.cc's AFL: wiped, 2nd quit called 0xFFFFFF)
  memset(ti8bpp_screen, 255, VIR_LCD_PIX_H * VIR_LCD_PIX_W);

  // lcd_BacklightLevel=minBrightness;
  for (int r=0;r<4;r++){
    for (int g=0;g<8;g++){
      for (int b=0;b<4;b++){
        const int R=r*255/3, G=g*255/7, B=b*255/3;
        const int RGB=sdk_rgb(R,G,B);
        lcd_Palette[(r<<5)|(g<<2)|b]=RGB;//gfx_RGBTo1555(R,G,B);
        //dbg_printf("lcd_Palette[%i] R=%i/255 G=%i/255 B=%i/255 RGB565=%x\n",(r<<5)|(g<<2)|b,R,G,B,RGB);
      }
    }
  }

  //   UNDEFINDED      WTRMRK UN LCDVCMP LCDPWR BEPO BEBO BGR LCDDUAL LCDMONO8 LCDTFT LCDBW LCDBPP LCDEN

  // 0b000000000000000 0      00 00      1      0    0    1   0       0        1      0     011    1;
  lcd_Control = 0b100100100111; // 8bpp like graphx
  memset(ti8bpp_screen, 127, VIR_LCD_PIX_H * VIR_LCD_PIX_W);
}
