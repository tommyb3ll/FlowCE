// implementation of the minimal C SDK for KhiCAS
extern int (*shutdown)(); // function called after 2 hours of idle
extern short int shutdown_state;
extern short exam_mode,nspire_exam_mode;
extern unsigned exam_start;
extern int exam_duration;

int (*shutdown)()=0;

short shutdown_state=0;
short exam_mode=0,nspire_exam_mode=0;
unsigned exam_start=0; // RTC start
int exam_duration=0;
// <0: indicative duration, ==0 time displayed during exam, >0 end exam_mode after
static const int exam_bg1=0x4321,exam_bg2=0x1234;
int exam_bg(){
  return exam_mode?(exam_duration>0?exam_bg1:exam_bg2):0x50719;
}

void SetQuitHandler( void (*f)(void)){}

#define LONG_MAX ((long)(~0UL>>1))
#define LONG_MIN (~LONG_MAX)

extern int clip_ymin;
int clip_ymin=0;
static const int STATUS_AREA_PX=18;
// debug: dbg_printf() Add #include <debug.h> to a source file, and use make debug instead of make to build a debug program. You may need to run make clean beforehand in order to ensure all source files are rebuilt.
// ASM syscalls: https://wikiti.brandonw.net/index.php?title=Category:84PCE:Syscalls:By_Name
// doc: https://ce-programming.github.io/toolchain/index.html
// Makefile options https://ce-programming.github.io/toolchain/static/makefile-options.html
// memory layout: https://ce-programming.github.io/toolchain/static/faq.html
// parameters are in CEdev/meta (and app_tools if present)
// makefile.mk:
// BSSHEAP_LOW ?= D052C6
// BSSHEAP_HIGH ?= D13FD8
// STACK_HIGH ?= D1A87E
// INIT_LOC ?= D1A87F
// Can we set STACK_HIGH to another value? I think the global area stack+data could be "reversed", I mean stack top at 0xD2A87F and data(+code+ro_data for RAM programs) at a new position: INIT_LOC=D1987E (maybe +1 or +2)

// TI stack 4K D1A87Eh: Top of the SPL stack.
// change stack pointer (if STACK_HIGH change does not work)
// requires assembly code (https://0x04.net/~mwk/doc/z80/eZ80.pdf),
// save stack pointer
// LD (Mmn), SP
// set HL to the new stack address (top of the area-3)
// LD SP,HL
// call main
// restore stack pointer
// LD SP,(Mmn)
// 1023 bytes: uint8_t[1023] os_RamCode (do not use if flash write occurs)
// 0xD052C6: 60989 bytes used for bss+heap (temp buffers in TI OS)
// 0xD1A881: Start of UserMem. 64K for code, data, ro data
// size_t os_MemChk(void **free) size and position of free ram area
// Or we could create a VarApp in RAM with no real data inside and use this area for temporary storage.
// 0xD40000: Start of VRAM. 320x240x2 bytes = 153600 bytes.
// half may be used in 8 bits palette mode (graphx)
// half=0x12C00
// ;safeRAM Locations
// pixelShadow		equ 0D031F6h ; 8400 bytes
// pixelShadow2		equ 0D052C6h ; 8400 bytes
// cmdPixelShadow		equ 0D07396h ; 8400 bytes
// plotSScreen		equ 0D09466h ; 21945 bytes	; Set GraphDraw Flag to redraw graph if used
//saveSScreen		equ 0D0EA1Fh ; 21945 bytes	; Set GraphDraw Flag to redraw graph if used
//cursorImage		equ 0E30800h ; 1020 bytes
#include "k_csdk.h"
#include "focus.h"
#include <ti/getkey.h>
#include <ti/getcsc.h>
#include <ti/screen.h>
#include <ti/flags.h>
#include <ti/vars.h>
#include <sys/power.h>
#include <sys/rtc.h> // boot_GetTime(uint8_t *seconds, uint8_t *minutes, uint8_t *hours), boot_SetTime(uint8_t seconds, uint8_t minutes, uint8_t hours)
#include <sys/timers.h>
#include <sys/lcd.h>
#ifdef STANDALONE
// #include "graphic.h"
#else
#include <graphx.h>
#include <fileioc.h>
#endif
#include <string.h>
#include <debug.h>
#define FILENAME_MAXRECORDS 32
#define FILENAME_MAXSIZE 9
#define FILE_MAXSIZE 16384
static char os_filenames[FILENAME_MAXRECORDS][FILENAME_MAXSIZE];

static inline int sdk_rgb(int a, int b, int c) {
  return (((a*32)/256)<<11) | (((b*64)/256)<<5) | ((c*32)/256);
}

void sdk_init(){
  gfx_Begin();
#ifndef STANDALONE
  unsigned short * addr=gfx_palette;
  dbg_printf("KhiCAS SDK Init palette=%x\n",addr);
  for (int r=0;r<4;r++){
    for (int g=0;g<8;g++){
      for (int b=0;b<4;b++){
        int R=r*255/3,G=g*255/7,B=b*255/3;
        addr[(r<<5)|(g<<2)|b]=sdk_rgb(R,G,B);//gfx_RGBTo1555(R,G,B);
        // dbg_printf("palette %i %i %i %i\n",(r<<5)|(g<<2)|b,R,G,B);
      }
    }
  }
#endif
  // 128-254 arc-en-ciel? 255 should remain white
}

void sdk_end(){
#ifndef WITH_QUAD
  dbg_printf("SDK End\n");
#endif
  gfx_End();
}

void clear_screen(void){
#ifdef STANDALONE
  vGL_setArea(0,0,VIR_LCD_PIX_W,VIR_LCD_PIX_H,GL_WHITE);
#else
  gfx_FillScreen(255); // gfx_ZeroScreen(void);
#endif
}

static int alpha=0,alphalock=0,prevalpha=0,shift=0;
int handle_f5(){
  if (alphalock)
    alphalock=3-alphalock;
  else
    alphalock=2;
  alpha=1;
  return alpha;
}
void dbgprint(int i){
  char buf[16]={0};
  buf[0]='0'+i/100;
  buf[1]='0'+(i % 100)/10;
  buf[2]='0'+(i % 10);
  os_draw_string(20,60,SDK_WHITE,SDK_BLACK,buf,false);
}

#define kb_On (*(volatile uint8_t*)0xF00020 & 1)
#define kb_EnableOnLatch() ((*(volatile uint8_t*)0xF0002C) |= 1)
#define kb_DisableOnLatch() ((*(volatile uint8_t*)0xF0002C) &= ~1)
#define kb_ClearOnLatch() ((*(volatile uint8_t*)0xF00028) = 1)

// getkey polls (test harness: tools/emu waits for this counter to move = ready for the next key)
volatile unsigned getkey_polls;
int getkey(int allow_suspend){
  sync_screen();
  display_time();//statusline(0);
  for (;;){
    int i=0,j=0,joff=10000;
    for (;!i;j++){
#if 1
      if (kb_On || j==joff
          //&& allow_suspend
          ){
#ifndef WITH_QUAD
        dbg_printf("on pressed\n");
#endif
        if (shift || j==joff){
          boot_TurnOff();
          shift=false;
        }
        else if (kb_On){
          boot_TurnOn();
          lcd_Control = 0b100100100111; // 8bpp like graphx
          j=0;
          display_time();
        }
        kb_ClearOnLatch();
        os_wait_1ms(100);
        continue;
      }
#endif
      if (j<joff){
        if ((j&0x3ff)==0x3ff)
          display_time();
        if ((j&0x1ff)==0x1ff)
          focus_idle();
        i=os_GetCSC();
        ++getkey_polls;
      }
      os_wait_1ms(10);      
    }
    // dbgprint(i);
    int decal=(alpha>>1)<<5; // 0 or 32 for upper or lowercase
    int Alpha=alpha,Shift=shift;
    shift=0; prevalpha=alpha;
    if (!alphalock)
      alpha=0;
    switch (i){
    case sk_Fx:
      return Alpha?KEY_CTRL_F11:Shift?KEY_CTRL_F6:KEY_CTRL_F1;
    case sk_Fenetre:
      return Alpha?KEY_CTRL_F12:Shift?KEY_CTRL_F7:KEY_CTRL_F2;
    case sk_Zoom:
      return Alpha?KEY_CTRL_F13:Shift?KEY_CTRL_F8:KEY_CTRL_F3;      
    case sk_Trace:
      return Alpha?KEY_CTRL_F14:Shift?KEY_CTRL_F9:KEY_CTRL_F4;      
    case sk_Graph:
      return Alpha?KEY_CTRL_F15:Shift?KEY_CTRL_F10:KEY_CTRL_F5;      
    case sk_Mode:
      return Shift?KEY_CTRL_QUIT:KEY_CTRL_SETUP;
    case sk_Del:
      return Shift?KEY_CTRL_PASTE:KEY_CTRL_DEL;
    case sk_GraphVar:
#ifdef FRANCAIS
      return Shift?KEY_CTRL_UNDO:KEY_CTRL_XTT;
#else
      return Alpha?KEY_CTRL_UNDO:(Shift?KEY_EQW_TEMPLATE:KEY_CTRL_XTT);
#endif
    case sk_Stat:
      return Shift?KEY_CHAR_LIST:KEY_CTRL_STATS;
    case sk_Right:
      return Shift?KEY_SHIFT_RIGHT:KEY_CTRL_RIGHT;
    case sk_Left:
      return Shift?KEY_SHIFT_LEFT:KEY_CTRL_LEFT;
    case sk_Up:
      return Shift?KEY_CTRL_PAGEUP:KEY_CTRL_UP;
    case sk_Down:
      return Shift?KEY_CTRL_PAGEDOWN:KEY_CTRL_DOWN;
    case sk_Enter:
      return Shift?KEY_CHAR_CR:KEY_CTRL_EXE;    
    case sk_Alpha:
      if (alphalock){
        alpha=alphalock=0;
      }
      else {
        if (Shift)
          alphalock=alpha=2;
        else {
          alpha=2;
          if (prevalpha)
            alphalock=alpha=prevalpha;            
        }
      }
      //statusline(0);
      return KEY_CTRL_ALPHA; // continue;
    case sk_2nd:
      if (alphalock)
        alpha=3-alpha; // maj <> min
      else
        shift=!Shift;
      //statusline(0);
      return KEY_CTRL_SHIFT; // continue;
    case sk_Math: // 2nd MATH (TEST on the keyboard): '=' directly, for x^2-4=0 and f(x)=...
      return Alpha?KEY_CHAR_A+decal:(Shift?KEY_CHAR_EQUAL:KEY_CTRL_SYMB);
    case sk_Matrice:
      return Alpha?KEY_CHAR_B+decal:KEY_CHAR_MAT;
    case sk_Prgm:
      return Alpha?KEY_CHAR_C+decal:(Shift?KEY_CTRL_F15:KEY_CTRL_PRGM);
    case sk_Vars:
      return KEY_CTRL_VARS;
    case sk_Annul:
      return Shift?KEY_CTRL_AC:KEY_CTRL_EXIT;
    case sk_TglExact:
      //dbg_printf("tab\n");
      return Alpha?KEY_CHAR_D+decal:'\t';
#ifdef FRANCAIS
    case sk_Trig:
      return Alpha?KEY_CHAR_E+decal:(Shift?KEY_CHAR_PI:KEY_CHAR_SIN);
    case sk_Resol:
      return Alpha?KEY_CHAR_F+decal:(Shift?KEY_CTRL_APPS:KEY_CTRL_SOLVE);
    case sk_Frac:
      return Alpha?KEY_CHAR_G+decal:KEY_EQW_TEMPLATE;
    case sk_Power:
      return Alpha?KEY_CHAR_H+decal:KEY_CHAR_POW;
#else
    case sk_Sin:
      return Alpha?KEY_CHAR_E+decal:(Shift?KEY_CHAR_ASIN:KEY_CHAR_SIN);
    case sk_Cos:
      return Alpha?KEY_CHAR_F+decal:(Shift?KEY_CHAR_ACOS:KEY_CHAR_COS);
    case sk_Tan:
      return Alpha?KEY_CHAR_G+decal:(Shift?KEY_CHAR_ATAN:KEY_CHAR_TAN);
    case sk_Power:
      return Alpha?KEY_CHAR_H+decal:(Shift?KEY_CHAR_PI:KEY_CHAR_POW);
#endif
    case sk_Square:
      return Alpha?KEY_CHAR_I+decal:Shift?KEY_CHAR_ROOT:KEY_CHAR_SQUARE;
    case sk_Comma:
      return Alpha?KEY_CHAR_J+decal:Shift?KEY_CHAR_E:KEY_CHAR_COMMA;      
    case sk_LParen:
      return Alpha?KEY_CHAR_K+decal:Shift?KEY_CHAR_LBRACE:KEY_CHAR_LPAR;      
    case sk_RParen:
      return Alpha?KEY_CHAR_L+decal:Shift?KEY_CHAR_RBRACE:KEY_CHAR_RPAR;      
    case sk_Div:
      return Alpha?KEY_CHAR_M+decal:Shift?KEY_CHAR_E+32:KEY_CHAR_DIV;
    case sk_Log:
      return Alpha?KEY_CHAR_N+decal:Shift?KEY_CHAR_EXPN10:KEY_CHAR_LOG;
    case sk_7:
      return Alpha?KEY_CHAR_O+decal:Shift?KEY_LIST7:KEY_CHAR_7;
    case sk_8:
      return Alpha?KEY_CHAR_P+decal:Shift?KEY_LIST8:KEY_CHAR_8;
    case sk_9:
      return Alpha?KEY_CHAR_Q+decal:Shift?KEY_LIST9:KEY_CHAR_9;
    case sk_Mul:
      return Alpha?KEY_CHAR_R+decal:Shift?KEY_CHAR_LBRCKT:KEY_CHAR_MULT;
    case sk_Ln:
      return Alpha?KEY_CHAR_S+decal:Shift?KEY_CHAR_EXPN:KEY_CHAR_LN;
    case sk_4:
      return Alpha?KEY_CHAR_T+decal:Shift?KEY_LIST4:KEY_CHAR_4;
    case sk_5:
      return Alpha?KEY_CHAR_U+decal:Shift?KEY_LIST5:KEY_CHAR_5;
    case sk_6:
      return Alpha?KEY_CHAR_V+decal:Shift?KEY_LIST6:KEY_CHAR_6;
    case sk_Sub:
      return Alpha?KEY_CHAR_W+decal:Shift?KEY_CHAR_RBRCKT:KEY_CHAR_MINUS;
    case sk_Store:
      return Alpha?KEY_CHAR_X+decal:Shift?KEY_SHIFT_ANS:KEY_CHAR_STORE;
    case sk_1:
      return Alpha?KEY_CHAR_Y+decal:Shift?KEY_LIST1:KEY_CHAR_1;
    case sk_2:
      return Alpha?KEY_CHAR_Z+decal:Shift?KEY_LIST2:KEY_CHAR_2;
    case sk_3:
      return Alpha?KEY_CHAR_THETA:Shift?KEY_LIST3:KEY_CHAR_3;
    case sk_Add:
      return Alpha?'"':KEY_CHAR_PLUS;
    case sk_0:
      return Alpha?KEY_CHAR_SPACE:Shift?KEY_CTRL_CATALOG:KEY_CHAR_0;
    case sk_DecPnt:
      return Alpha?':':Shift?KEY_CHAR_I+32:'.';
    case sk_Chs:
      return Alpha?'?':Shift?KEY_CHAR_ANS:KEY_CHAR_PMINUS;
    default:
      return i;
    }
  }
}
void GetKey(int * key){
  *key=getkey(0);
}
int iskeydown(int key){
  return kb_Data[1] == key;
}

// if (kb_On) ...
void enable_back_interrupt(){
  // kb_EnableOnLatch();
}
void disable_back_interrupt(){
  // kb_DisableOnLatch();
}
int isalphaactive(){
  return alpha;
}
int alphawasactive(int * key){
  return prevalpha;
}
void lock_alpha(){
  alpha=2; alphalock=1;
  shift=0;
  statusflags();
}
void reset_kbd(){
  shift=alpha=alphalock=0;
  statusflags();
}
int GetSetupSetting(int k){
  if (k!=0x14) return -1;
  if (!alpha) return shift?1:0;
  if (!alphalock) return alpha==2?8:4;
  return alpha==2?0x88:0x84;
}

void os_wait_1ms(int ms){
  msleep(ms); // delay(ms)?
}
double millis(){
  return rtc_Days*86400.0+rtc_Hours*3600.+rtc_Minutes*60.+rtc_Seconds;
}
int os_set_angle_unit(int mode){
  if (mode) os_ResetFlag(TRIG,DEGREES); else os_SetFlag(TRIG,DEGREES);
  return true;
}

int os_get_angle_unit(){
  const int i=os_TestFlag(TRIG,DEGREES);
  return i?0:1;
}

int file_exists(const char * filename){
#ifdef STANDALONE
  int archived=0;
  var_t * ptr_=os_GetAppVarData(filename,&archived);
#ifndef WITH_QUAD
  dbg_printf("file exists name=%s ptr=%x archived=%i\n",filename,ptr_,archived);
#endif
  if (!ptr_)
    return 0;
  return archived?2:1;
#else
  int h=ti_Open(filename, "r");
  if (!h)
    return false;
  ti_Close(h);
  return true;
#endif
}

void warn_archived(const char * filename){
  char buf[256];
  strcpy(buf,filename);
  strcat(buf,": unarchive variable!");
  os_fill_rect(0,115,LCD_WIDTH_PX,25,COLOR_BLACK);
  os_draw_string_medium(20,102,COLOR_RED,COLOR_WHITE,buf,0);
  getkey(1);
}

int erase_file(const char * filename){
  const int i=file_exists(filename);
#ifndef WITH_QUAD
  dbg_printf("erase file=%s i=%i\n",filename,i);
#endif
  if (!i)
    return 0;
#ifdef STANDALONE
  if (i==2){
    warn_archived(filename);
    // appvar in archive should be unarchived first
    return 0;
  }
  os_DelAppVar(filename);
  return 1;
#else
  ti_Delete(filename);
  return true;
#endif
}

const char * read_file(const char * filename){
  const char * ext=0;
  int l=strlen(filename);
  char var[9]={0};
  strncpy(var,filename,8);
  for (--l;l>0;--l){
    if (filename[l]=='.'){
      ext=filename+l+1;
      if (l<9)
        var[l]=0;
      break;
    }
  }
#ifdef STANDALONE
  int archived;
  //dbg_printf("read_file var=%s\n",var);
  var_t * ptr_=os_GetAppVarData(var,&archived);
  if (!ptr_)
    return 0;
  const char * ptrc = (const char*)ptr_->data;
  int s=ptr_->size;
  //dbg_printf("read_file name=%s size=%i %x %x %x %x | %x %x %x %x | %x %x %x %x | %x %x %x %x | %x %x %x %x | %x %x %x %x\n",var,s,ptrc[0],ptrc[1],ptrc[2],ptrc[3],ptrc[4],ptrc[5],ptrc[6],ptrc[7],ptrc[8],ptrc[9],ptrc[10],ptrc[11],ptrc[12],ptrc[13],ptrc[14],ptrc[15],ptrc[16],ptrc[17],ptrc[18],ptrc[19],ptrc[20],ptrc[21],ptrc[22],ptrc[23]);
  if (s>7){
    //unsigned short u;
    //ti_Read(&u,1,2,h);
    char subtype[8]={0};
    strncpy(subtype,ptrc,4); ptrc+=4;
    if (strncmp(subtype,"PYCD",4)==0 || strncmp(subtype,"PYSC",4)==0 || strncmp(subtype,"XCAS",4)==0 || strncmp(subtype,"TABL",4)==0){
      unsigned char dx = *ptrc; ++ptrc;
      //dbg_printf("read subtype=%s dx=%i ptrc=%s\n",subtype,(int)dx,ptrc);
      if (dx>0){
        // skip desktop filename
        char buf[256]={0};
        strncpy(buf,ptrc,dx+1); ptrc+=dx+1;
        s -= 5+dx;
        //dbg_printf("tiread subtype=%s filename=%s dx=%i effsize=%i\n",subtype,buf,dx,s);
      }
      else
        s -= 4;
    }
  }
  //dbg_printf("strlen=%i s=%i ptrc[s-1]=%x\n",strlen(ptrc),s,ptrc[s-1]);
  if (ptrc[s-1]==0)
    return ptrc; // if it ends with a 0, we are done
  // we must insure a final 0
  // without lcd, using os_MemChk
  char * ptr; int S=os_MemChk((void **)&ptr);
  if (s>=S)
    return 0;
  memcpy(ptr,ptrc,s);
  ptr[s]=0;
  //dbg_printf("data=%x %x %x %x | %x %x %x %x | %x %x %x %x | %x %x %x %x\n",ptr[0],ptr[1],ptr[2],ptr[3],ptr[4],ptr[5],ptr[6],ptr[7],ptr[8],ptr[9],ptr[10],ptr[11],ptr[12],ptr[13],ptr[14],ptr[15]);
  return ptr;
#else // STANDALONE
  // old code using fileio
  int h=ti_Open(var, "r");
  if (!h)
    return 0;
  int s=ti_GetSize(h);
  if (s>7){
    //unsigned short u;
    //ti_Read(&u,1,2,h);
    char subtype[8]={0};
    ti_Read(subtype,1,4,h);
    if (strncmp(subtype,"PYCD",4)==0 || strncmp(subtype,"PYSC",4)==0 || strncmp(subtype,"XCAS",4)==0 || strncmp(subtype,"TABL",4)==0){
      unsigned char dx;
      ti_Read(&dx,1,1,h);
      if (dx!=0){
        // skip desktop filename
        char buf[256]={0};
        ti_Read(buf,1,dx-1,h);
        s -= 4+dx;
        //dbg_printf("tiread subtype=%s filename=%s dx=%i effsize=%i\n",subtype,buf,dx,s);
      }
      else
        s -= 4;
    }
    else
      ti_Seek(0,SEEK_SET,h);
  }
  char * ptr=0;
  // Code requiring a copy
  // if it starts with 
  // char * ptr=(char *) gfx_vram+LCD_WIDTH_PX*LCD_HEIGHT_PX; // pointer in vram buffer
  int S=os_MemChk((void **)&ptr);
  if (s>=S)
    return 0;
  S=ti_Read(ptr,1,s,h);
  ptr[S]=0;
  ti_Close(h);
  //dbg_printf("data=%x %x %x %x %x | %x %x %x %x | %x %x %x %x | %x %x %x\n",ptr[0],ptr[1],ptr[2],ptr[3],ptr[4],ptr[5],ptr[6],ptr[7],ptr[8],ptr[9],ptr[10],ptr[11],ptr[12],ptr[13],ptr[14],ptr[15]);
  return ptr;
#endif // fileio
}
int write_file(const char * filename,const char * s,int len){
  if (!len)
    len=strlen(s)+1;
  //dbg_printf("write_file file=%s len=%i\n",filename,len);
  // find extension
  const char * ext=0;
  int l=strlen(filename);
  char var[32]={0};
  strncpy(var,filename,8);
  for (--l;l>0;--l){
    if (filename[l]=='.'){
      ext=filename+l+1;
      if (l<9)
        var[l]=0;
      break;
    }
  }
  //dbg_printf("write_file var=%s fname=%s ext=%s\n",var,filename,ext);
#ifdef STANDALONE
  // first erase filename
  const int i=file_exists(var);
  //dbg_printf("write_file exist=%i\n",i);
  if (i==2){
    warn_archived(filename);
    // appvar in archive should be unarchived first
    return 0; 
  }
  if (i==1)
    os_DelAppVar(var);
  int effsize=len;
  bool ispy=false,isxw=false,istab=false,with_filename=false;
  if (ext){
    ispy=strncmp(ext,"py",2)==0;
    isxw=strncmp(ext,"xw",2)==0;
    istab=strncmp(ext,"tab",3)==0;
    effsize += 5; // 4 + 1 byte for size of filename
    if (with_filename)
      effsize += strlen(filename)+1;
  }
  //dbg_printf("write_file ext=%s\n",ext);
  var_t * ptr_=os_CreateAppVar(var, effsize);
  if (!ptr_){
#ifndef WITH_QUAD    
    dbg_printf("Unable to write appvar %s\n",var);
#endif
    return false;
  }
  //dbg_printf("write_file var=%s file=%s size=%i effsize=%i\n",var,filename,len,effsize);
  char * ptrc = (char*)ptr_->data;
  if (ispy || isxw || istab){
    strncpy(ptrc,istab?"TABL":(isxw?"XCAS":"PYCD"),4); ptrc+=4;
    if (with_filename){
      *ptrc=strlen(filename)+1;
      ++ptrc;
      strncpy(ptrc,filename,strlen(filename)); ptrc+=strlen(filename);
    }
    else {
      *ptrc=0;
      ++ptrc;
    }
  }
  memcpy(ptrc,s,len);
  //dbg_printf("strlen=%i len=%i ptrc[len-1]=%x\n",strlen(s),len,ptrc[len-1]);
  return true;
#else
  //dbg_printf("tiwrite %s\n",filename);
  int h=ti_Open(var,"w");
  if (!h) return false;
  if (ext){
    bool ispy=strncmp(ext,"py",2)==0;
    bool isxw=strncmp(ext,"xw",2)==0;
    if (ispy || isxw || istab){
      const char * subtype=istab?"TABL":(isxw?"XCAS":"PYCD");
      //dbg_printf("type %s\n",subtype);
      ti_Write(subtype,strlen(subtype),1,h);
      unsigned char dx=strlen(filename)+1;
      ti_Write(&dx,1,1,h);
      ti_Write(filename,dx-1,1,h);
    }
  }
  //dbg_printf("len=%i %x %x %x %x | %x %x %x %x | %x %x %x %x | %x %x %x %x \n",len,s[0]&0xff,s[1]&0xff,s[2]&0xff,s[3]&0xff,s[4]&0xff,s[5]&0xff,s[6]&0xff,s[7]&0xff,s[8]&0xff,s[9]&0xff,s[10]&0xff,s[11]&0xff,s[12]&0xff,s[13]&0xff,s[14]&0xff,s[15]&0xff);
  int Len=ti_Write(s,1,len,h);
  ti_Close(h);
  return Len==len;
#endif
}

static inline bool isalpha(char c) {
  return (c>='a' && c<='z') || (c>='A' && c<='Z');
}

int os_file_browser(const char ** filenames,int maxrecords,const char * extension,int storage){
  if (extension && extension[0]=='*' && extension[1]=='.')
    extension+=2;
  //dbg_printf("os_file_browser max=%i extension=%s\n",maxrecords,extension);
  if (maxrecords>FILENAME_MAXRECORDS)
    maxrecords=FILENAME_MAXRECORDS;
  void * ptr=os_GetSymTablePtr();
  int cur=0;
  for (int count=0;cur<maxrecords && ptr;count++){
    uint24_t type, l;
    char s[16]={0};
    char * dataptr = NULL;
    char * ext = NULL;
    const uint8_t * ptr2 = (uint8_t*)ptr - 1;
    ptr = os_NextSymEntry(ptr, &type, &l, s, (void**)&dataptr);
    //    if (isalpha(s[0])) dbg_printf("os_file_browser name=%s l=%i type=%i dataptr=%x [count=%i cur=%i ptr=%x ptr2=%x]\n",s,l,type,dataptr,count,cur,ptr,ptr2);
    if (l >= FILENAME_MAXSIZE || !dataptr || !isalpha(s[0]) || type != 21 || *ptr2)
      continue;
    s[l] = '\0';
    var_t * ptr_=os_GetAppVarData(s,0);
    dataptr = (char*)(ptr_->data - 2);
    //dbg_printf("filebrowser varname=%s type=%i data=%x %x %x %x | %x %x %x %x | %x %x %x %x | %x %x %x %x\n",s,type,dataptr[0]&0xff,dataptr[1]&0xff,dataptr[2]&0xff,dataptr[3]&0xff,dataptr[4]&0xff,dataptr[5]&0xff,dataptr[6]&0xff,dataptr[7]&0xff,dataptr[8]&0xff,dataptr[9]&0xff,dataptr[10]&0xff,dataptr[11]&0xff,dataptr[12]&0xff,dataptr[13]&0xff,dataptr[14]&0xff,dataptr[15]&0xff);
    // if type==21 dataptr[1]*256+dataptr[0]==size, then data
    // xcas session begins with 4 bytes size, on the 83 should be 00 00 xx xx
    if (type==21 && dataptr[2]==0 && dataptr[3]==0)
      ext="xw";
    // python app, starts with 2 bytes size, "PYCD" or "PYSC"
    // the script ifself begins at data.begin() + 6 + scriptOffset
    // where scriptOffset = dataptr[6] + 1
    if (!ext){
      if (strncmp(&dataptr[2],"PYCD",4)==0 || strncmp(&dataptr[2],"PYSC",4)==0)
        ext="py";
      else if (strncmp(&dataptr[2],"XCAS",4)==0)
        ext="xw";
      else if (strncmp(&dataptr[2],"TABL",4)==0)
        ext="tab";
      else { // extension from filename _xw or _py or _...
        //dbg_printf("os_file_browser2 %i %i %x\n",type,l,dataptr);
        //dbg_printf("filename2 %i %s\n",count,s);
        for (uint24_t j = l - 1;j>0;--j){
          if (s[j]=='_'){
            ext=s+j+1;
            break;
          }
        }
      }
    }
    //dbg_printf("filebrowser ext=%s [arg extension=%s]\n",ext,extension);
    if (ext && strcmp(ext,extension)==0){
      strncpy(os_filenames[cur],s,FILENAME_MAXSIZE);
      filenames[cur]=os_filenames[cur];
      //dbg_printf("extension match %i %s %s\n",cur,s,filenames[cur]);
      ++cur;
    }
  }
  //dbg_printf("filebrowser end %i\n",cur);
  return cur;
}
// gfx_Begin, gfx_SetDrawBuffer(); gfx_End
// GFX_LCD_WIDTH, HEIGHT, gfx_vbuffer=LCD RAM buffer 76800 bytes
// gfx_vram Total of 153600 bytes in size = 320x240x2
// gfx_SetDrawBuffer()gfx_SetDrawScreen()
// uint8_t gfx_SetColor(uint8_t index)
// gfx_SetPixel(uint24_t x, uint8_t y)
// uint8_t gfx_GetPixel(uint24_t x, uint8_t y)
// gfx_FillRectangle(int x, int y, int width, int height)
// gfx_FillRectangle_NoClip(uint24_t x, uint8_t y, uint24_t width, uint8_t height)
// gfx_Wait(void)
// gfx_PrintStringXY(const char *string, int x, int y)
//gfx_SetTextFGColor(uint8_t color)
// gfx_SetTextScale(uint8_t width_scale, uint8_t height_scale)
// gfx_SetTextConfig
void sync_screen(){
  //gfx_Wait();
  // gfx_BlitBuffer(); // shoud be done if gfx_SetDrawBuffer() is active;
}
int c_rgb565to888(int c){
  c &= 0xffff;
  const int r=(c>>11)&0x1f,g=(c>>5)&0x3f,b=c&0x1f;
  return (r<<19)|(g<<10)|(b<<3);
}

int convertcolor(int c){
  // convert 16 bits to default palette
  c &= 0xffff;
  const int r=(c>>11)&0x1f,g=(c>>5)&0x3f,b=c&0x1f;
  const int R = ((r>>3)<<5) | ((g>>3)<<2) | (b>>3);
  //dbg_printf("convertcolor rgb565=%x r=%i/32 g=%i/64 b=%i/32 to rgb232=%i palette[]=%x\n",c,r,g,b,R,lcd_Palette[R]);
  return R;
}

int convertbackcolor(int c){
  c &= 0x7f;
  int r=(c>>5)&0x3,g=(c>>2)&0x7,b=c&0x3;
  r <<= 3; g <<= 3; b <<= 3;
  const int R = (r<<11) | (g<<5) | b;
  //dbg_printf("convert %i r=%i g=%i b=%i to %i\n",c,r,g,b,R);
  return R;
  
}

#ifdef STANDALONE
  void os_set_pixel(int x,int y,int c){
    vGL_set_pixel(x,y,convertcolor(c));
  }
#else
void setcolor(int c){
  gfx_SetColor(convertcolor(c));
  //gfx_SetTextTransparentColor(0);
}
void os_set_pixel(int x,int y,int c){
  setcolor(c);
  gfx_SetPixel(x,y);
}
#endif

void os_fill_rect(int x,int y,int w,int h,int c){
#ifdef STANDALONE
  if (x<0){
    w+=x;
    x=0;
  }
  if (y<0){
    h+=y;
    y=0;
  }
  vGL_setArea(x,y,x+w,y+h,convertcolor(c));
#else
  setcolor(c);
  gfx_FillRectangle(x,y,w,h);
#endif
}
int os_get_pixel(int x,int y){
#ifdef STANDALONE
  return convertbackcolor(vGL_get_pixel(x,y));
#else
  return convertbackcolor(gfx_GetPixel(x,y));
#endif
}

#ifdef STANDALONE
int os_draw_string(int x,int y,int c,int bg,const char * s,int fake){
  y+=STATUS_AREA_PX;
  if (!fake)
    vGL_putString(x,y,s,convertcolor(c),convertcolor(bg),16);
  return x+8*strlen(s);
}

int os_draw_string_medium(int x,int y,int c,int bg,const char * s,int fake){
  y+=STATUS_AREA_PX;
  if (!fake)
    vGL_putString(x,y,s,convertcolor(c),convertcolor(bg),14);
  return x+7*strlen(s);
}

int os_draw_string_small(int x,int y,int c,int bg,const char * s,int fake){
  y+=STATUS_AREA_PX;
  if (!fake)
    vGL_putString(x,y,s,convertcolor(c),convertcolor(bg),8);
  return x+5*strlen(s);
}

#else
void printstringxdxy(const char * s,int x,int dx,int y){
  if (x+dx>LCD_WIDTH_PX){
    char buf[64];
    int pos=sizeof(buf)-1;
    strncpy(buf,s,pos);
    buf[pos]=0;
    for (;pos>=1 && x+gfx_GetStringWidth(buf)>LCD_WIDTH_PX;--pos)
      buf[pos]=0;
    //dbg_printf("buf printstringxdxy %s x=%i dx=%i y=%i\n",buf,x,dx,y);
    gfx_PrintStringXY(buf,x,y);
  }
  else {
    //dbg_printf("s printstringxdxy %s x=%i dx=%i y=%i\n",s,x,dx,y);
    gfx_PrintStringXY(s,x,y);
  }
}

static inline int minint(int a,int b){
  return a<b?a:b;
}

// FIXME? use gfx_SetTransparentColor with a value != FG and BG instead of fill rectangle
int os_draw_string_small(int x,int y,int c,int bg,const char * s,int fake){
  //dbg_printf("small at %i,%i c=%i bg=%i %s\n",x,y,c,bg,s);
  y+=STATUS_AREA_PX;
  gfx_SetTextScale(1,1);
  int dx=gfx_GetStringWidth(s);
  if (x<LCD_WIDTH_PX && !fake){
    setcolor(bg);
    gfx_FillRectangle(x,y,minint(dx,LCD_WIDTH_PX-x),8);
    int c_=gfx_SetTextFGColor(convertcolor(c));
    int bg_=gfx_SetTextBGColor(convertcolor(bg));
    printstringxdxy(s,x,dx,y);
    gfx_SetTextFGColor(c_);
    gfx_SetTextBGColor(bg_);
  }
  return x+dx; 
}

int os_draw_string_medium(int x,int y,int c,int bg,const char * s,int fake){
  y+=STATUS_AREA_PX;
  gfx_SetTextScale(1,2);
  //gfx_SetFontHeight(12);
  int dx=gfx_GetStringWidth(s);
  if (x<LCD_WIDTH_PX && !fake){
    setcolor(bg);
    int effdx=minint(dx,LCD_WIDTH_PX-x);
    //dbg_printf("draw_string_medium rect x=%i y=%i effdx=%i\n",x,y,effdx);
    gfx_FillRectangle(x,y,effdx,16);
    int c_=gfx_SetTextFGColor(convertcolor(c));
    int bg_=gfx_SetTextBGColor(convertcolor(bg));
    printstringxdxy(s,x,dx,y);
    gfx_SetTextFGColor(c_);
    gfx_SetTextBGColor(bg_);
  }
  return x+dx; 
}
int os_draw_string(int x,int y,int c,int bg,const char * s,int fake){
  y+=STATUS_AREA_PX;
  gfx_SetTextScale(2,2);
  int dx=gfx_GetStringWidth(s);
  if (x<LCD_WIDTH_PX && !fake){
    setcolor(bg);
    gfx_FillRectangle(x,y,minint(dx,LCD_WIDTH_PX-x),16);
    int c_=gfx_SetTextFGColor(convertcolor(c));
    int bg_=gfx_SetTextBGColor(convertcolor(bg));
    printstringxdxy(s,x,dx,y);
    gfx_SetTextFGColor(c_);
    gfx_SetTextBGColor(bg_);
  }
  return x+dx; 
}
#endif

int os_key_flags(void){ // for the Focus status bar: 1 2nd, 2 alpha, 4 alpha lock, 8 lowercase
  return (shift?1:0)|(alpha?2:0)|(alphalock?4:0)|(alpha==2?8:0);
}
static const int statuscolor=2016;
void statuslinemsg(const char * msg,int warncolor){
  if (focus_on){ focus_status_msg(msg); return; }
  os_fill_rect(0,0,154,16,SDK_BLACK);
  const int l=strlen(msg);
  if (l<=22)
    os_draw_string_medium(0,-STATUS_AREA_PX,warncolor?warncolor:statuscolor,SDK_BLACK,msg,false);
  else {
    char buf[64]={0};
    strncpy(buf,msg,35);
    buf[36]=buf[35]='.';
    os_draw_string_small(0,3-STATUS_AREA_PX,warncolor?warncolor:statuscolor,SDK_BLACK,buf,false);
  }
  os_fill_rect(0,16,154,2,COLOR_WHITE);
}

void set_time(int h,int m){
  uint8_t month;
  uint16_t year;
  uint8_t day_of_month;
  uint8_t s = rtc_Seconds;
  boot_GetDate(&day_of_month, &month, &year);
  //dbg_printf("set_time s=%i m=%i h=%i d=%i\n",s,m,h,d);
  boot_SetTime(s,m,h);
  boot_SetDate(day_of_month, month, year);
  //rtc_Set(s,m,h,d);
}

void get_time(int *h,int *m){
  *h=rtc_Hours;
  *m=rtc_Minutes;
}

void display_time(){
  if (focus_on) return;
  const int h=rtc_Hours,m=rtc_Minutes;
  char msg[10];
  msg[0]=' ';
  msg[1]='0'+(h/10);
  msg[2]='0'+(h%10);
  msg[3]= 'h';
  msg[4]= ('0'+(m/10));
  msg[5]= ('0'+(m%10));
  msg[6]=0;
  //msg[6]= 'm';
  //msg[7] = ('0'+(s/10));
  //msg[8] = ('0'+(s%10));
  //msg[9]=0;
  os_draw_string_medium(260,-STATUS_AREA_PX,statuscolor,SDK_BLACK,msg,false);
}

void display_flags(){
  if (focus_on){ focus_status(); return; }
  const char *msg=0;
  if (alpha==2){
    msg=alphalock?"alock ":"alpha ";
  }
  else if (alpha==1){
    msg=alphalock?"ALOCK ":"ALPHA ";
  }
  else {
    if (shift)
      msg=" 2nd  ";
    else
      msg="      ";
  }
  os_draw_string_medium(220,-STATUS_AREA_PX,statuscolor,SDK_BLACK,msg,false);
}  

void statusflags(){
  if (focus_on){ focus_status(); return; }
  os_fill_rect(150,0,LCD_WIDTH_PX-150,16,SDK_BLACK);
  display_flags();
  os_draw_string_medium(150,-STATUS_AREA_PX,statuscolor,SDK_BLACK,os_get_angle_unit()?" rad CAS  ":" deg CAS  ",false);
  display_time();
  int x0=302,w=LCD_WIDTH_PX-x0;
  os_fill_rect(x0,0,6,STATUS_AREA_PX-2,SDK_BLACK);
  x0+=3; w-=6;
  os_fill_rect(x0+w-2,0,5,STATUS_AREA_PX-2,SDK_BLACK);
  os_fill_rect(x0,0,w,2,COLOR_RED);
  os_fill_rect(x0,STATUS_AREA_PX-5,w,2,COLOR_RED);
  os_fill_rect(x0,0,2,STATUS_AREA_PX-4,COLOR_RED);
  os_fill_rect(x0+w,0,2,STATUS_AREA_PX-4,COLOR_RED);
  const uint8_t i=boot_GetBatteryStatus(); int color=statuscolor;
  //i=0; // emu check colors
  switch (i){
  case 4:
    color=COLOR_GREEN;
    break;
  case 3:
    color=COLOR_CYAN;
    break;
  case 2:
    color=COLOR_YELLOW;
    break;
  case 1:
    color=COLOR_RED;
    break;
  case 0:
    color=COLOR_MAGENTA;
    break;
  }
  os_fill_rect(x0+4,1+(4-i)*3,w-6,i?i*3:1,color);
  os_fill_rect(0,16,LCD_WIDTH_PX,STATUS_AREA_PX-16,COLOR_WHITE);
  // dbg_printf("Battery level=%i color=%i\n",i,color);
  //os_draw_string_medium(302,-STATUS_AREA_PX,color,SDK_BLACK,msg,false);
}
void statusline(int mode){
  statusflags();
  if (mode==0)
    return;
  sync_screen();
}
