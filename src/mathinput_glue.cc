// Device side of the MathPrint-style input (mathinput.cc): draws a layout with KhiCAS's fonts,
// in the style of the results (kdisplay.cc), so input and output look the same.
#include <string.h>
#include "console.h"
#include "main.h"
#include "mathinput.h" // last: it maps std to ustl on the calculator

namespace xcas {
  void text_print(int fontsize,const char * s,int x,int y,int c,int bg,int mode); // kdisplay.cc
}

// KhiCAS fonts: big = text_print size 18 (8 px per char; glyphs up to 11 px above and 3 below
// the baseline, so a plain line fits a 15 px console row), small = size 12 (exponents, bounds:
// 5 px per char). text_print's y is not the baseline: it is 4 (big) / 3 (small) px below it.
extern const mi_metrics mi_device_metrics={{8,11,3},{5,7,2},2}; // extern: a const is internal otherwise

static const int MI_INK=COLOR_BLACK;

// line clipped to the box [x0,x1) x [y0,y1) (the input area); only axis-aligned clipping
// of the endpoints is needed: shapes are drawn whole or not at all
static void mi_line(int xa,int ya,int xb,int yb,int x0,int y0,int x1,int y1,int c){
  if (xa<x0 || xb<x0 || xa>=x1 || xb>=x1 || ya<y0 || yb<y0 || ya>=y1 || yb>=y1)
    return;
  draw_line(xa,ya,xb,yb,c);
}

// draws layout L of buffer s with its baseline at screen y=by and its left edge at x=bx,
// inside the box [x0,x1) x [y0,y1); caret: draw the caret too
void mi_draw(const mi_layout & L,const char * s,int bx,int by,int x0,int y0,int x1,int y1,bool caret){
  const mi_metrics & m=mi_device_metrics;
  for (size_t k=0;k<L.ops.size();++k){
    const mi_op & o=L.ops[k];
    const int x=bx+o.x,y=by+o.y,w=o.w,h=o.h;
    switch (o.code){
    case MI_TEXT: {
      const mi_font & f=o.small?m.small:m.big;
      char buf[40];
      const char * t=o.lit;
      if (!t){
        const int n=o.len<int(sizeof(buf))-1?o.len:int(sizeof(buf))-1;
        memcpy(buf,s+o.pos,n);
        buf[n]=0;
        t=buf;
      }
      if (x<x0 || x+o.w>x1 || y-f.asc<y0 || y+f.desc>y1)
        break; // text is drawn whole or not at all
      if (o.lit && (!strcmp(o.lit,"pi") || !strcmp(o.lit,"oo"))){
        // drawn here in one glyph cell: text_print's pi is ~12 px wide and overlapped
        // the next glyph, oo would be two letters
        const int a=f.adv,s2=o.small?1:2,top=y-(o.small?6:8);
        if (o.lit[0]=='p'){ // pi: a bar on two legs, digit height
          drawRectangle(x,top,a-1,s2,MI_INK);
          drawRectangle(x+(o.small?1:1),top,s2,y-top,MI_INK);
          drawRectangle(x+a-1-s2-(o.small?0:1),top,s2,y-top,MI_INK);
        }
        else { // infinity: two loops side by side
          const int m=y-(o.small?3:4),r=o.small?1:2,h2=a/2;
          for (int lp=0;lp<2;++lp){
            const int xl=x+lp*h2;
            draw_line(xl,m-r,xl+h2-1,m-r,MI_INK);
            draw_line(xl,m+r,xl+h2-1,m+r,MI_INK);
            draw_line(xl+(lp?h2-1:0),m-r,xl+(lp?h2-1:0),m+r,MI_INK);
          }
        }
        break;
      }
      xcas::text_print(o.small?12:18,t,x,y+(o.small?3:4),MI_INK,COLOR_WHITE,0);
      break;
    }
    case MI_DOT:
      if (x>=x0 && x+w<=x1 && y>=y0 && y+h<=y1)
        drawRectangle(x+w/2-1,y+h/2-1,2,2,MI_INK);
      break;
    case MI_HLINE: // fraction bar, 2 px like the results'
      mi_line(x,y,x+w-1,y,x0,y0,x1,y1,MI_INK);
      mi_line(x,y+1,x+w-1,y+1,x0,y0,x1,y1,MI_INK);
      break;
    case MI_SQRT: { // radical sign (6 px) and overbar, doubled like the results'
      const int ym=y+(2*h)/3;
      for (int d=0;d<2;++d){
        mi_line(x,ym+d,x+2,y+h-1,x0,y0,x1,y1,MI_INK);
        mi_line(x+2,y+h-1,x+5,y+d,x0,y0,x1,y1,MI_INK);
        mi_line(x+5,y+d,x+w-1,y+d,x0,y0,x1,y1,MI_INK);
      }
      break;
    }
    case MI_INTEGRAL: // a tall S: stem, hooks at both ends
      mi_line(x+3,y+2,x+3,y+h-3,x0,y0,x1,y1,MI_INK);
      mi_line(x+4,y+2,x+4,y+h-3,x0,y0,x1,y1,MI_INK);
      mi_line(x+4,y+1,x+5,y,x0,y0,x1,y1,MI_INK);
      mi_line(x+5,y,x+6,y,x0,y0,x1,y1,MI_INK);
      mi_line(x+2,y+h-2,x+3,y+h-2,x0,y0,x1,y1,MI_INK);
      mi_line(x,y+h-1,x+2,y+h-1,x0,y0,x1,y1,MI_INK);
      break;
    case MI_SIGMA:
      mi_line(x,y,x+w-1,y,x0,y0,x1,y1,MI_INK);
      mi_line(x,y+h-1,x+w-1,y+h-1,x0,y0,x1,y1,MI_INK);
      mi_line(x,y,x+w/2,y+h/2,x0,y0,x1,y1,MI_INK);
      mi_line(x+w/2,y+h/2,x,y+h-1,x0,y0,x1,y1,MI_INK);
      break;
    case MI_LPAREN: case MI_RPAREN: { // curved: 3 segments, 2 px apart at the middle
      const bool l=o.code==MI_LPAREN;
      const int xo=l?x+w-2:x+1,xi=l?x+1:x+w-2,q=h/4;
      mi_line(xo,y,xi,y+q,x0,y0,x1,y1,MI_INK);
      mi_line(xi,y+q,xi,y+h-1-q,x0,y0,x1,y1,MI_INK);
      mi_line(xi,y+h-1-q,xo,y+h-1,x0,y0,x1,y1,MI_INK);
      break;
    }
    case MI_LBRACKET: case MI_RBRACKET: {
      const bool l=o.code==MI_LBRACKET;
      const int xs=l?x+1:x+w-2,xe=l?x+w-1:x;
      mi_line(xs,y,xs,y+h-1,x0,y0,x1,y1,MI_INK);
      mi_line(xs,y,xe,y,x0,y0,x1,y1,MI_INK);
      mi_line(xs,y+h-1,xe,y+h-1,x0,y0,x1,y1,MI_INK);
      break;
    }
    case MI_LBRACE: case MI_RBRACE: {
      const bool l=o.code==MI_LBRACE;
      const int xo=l?x+w-1:x,xm=l?x+w/2:x+w/2,xi=l?x:x+w-1,ym=y+h/2;
      mi_line(xo,y,xm,y+1,x0,y0,x1,y1,MI_INK);
      mi_line(xm,y+1,xm,ym-1,x0,y0,x1,y1,MI_INK);
      mi_line(xm,ym-1,xi,ym,x0,y0,x1,y1,MI_INK);
      mi_line(xi,ym,xm,ym+1,x0,y0,x1,y1,MI_INK);
      mi_line(xm,ym+1,xm,y+h-2,x0,y0,x1,y1,MI_INK);
      mi_line(xm,y+h-2,xo,y+h-1,x0,y0,x1,y1,MI_INK);
      break;
    }
    case MI_BAR:
      mi_line(x+w/2,y,x+w/2,y+h-1,x0,y0,x1,y1,MI_INK);
      break;
    case MI_BOX: // an empty slot: a light frame, like an online calculator's placeholder
      mi_line(x,y,x+w-1,y,x0,y0,x1,y1,COLOR_BLUE);
      mi_line(x,y+h-1,x+w-1,y+h-1,x0,y0,x1,y1,COLOR_BLUE);
      mi_line(x,y,x,y+h-1,x0,y0,x1,y1,COLOR_BLUE);
      mi_line(x+w-1,y,x+w-1,y+h-1,x0,y0,x1,y1,COLOR_BLUE);
      break;
    }
  }
  if (caret){
    const int cx=bx+L.cx,cy=by+L.cy;
    if (cx>=x0 && cx+2<=x1 && cy>=y0 && cy+L.ch<=y1)
      drawRectangle(cx,cy,2,L.ch,MI_INK);
  }
}
