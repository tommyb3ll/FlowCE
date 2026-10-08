#include <cmath>
#include <string>

#include "k_csdk.h"
#include "textGUI.h"
#include "console.h"
#include "focus.h"
#include "main.h"
#include <ti/vars.h>
#include <cstring>
using namespace std;
constexpr const int xwaspy_shift=33; // must be between 32 and 63, reflect in xcas.js and History.cc

string filename_script(const char * filename){
  string f(remove_extension(filename));
  if (f.size()>7)
    f=f.substr(0,7);
  return f + "_.py"; // _ to end var name
}  

bool save_script(const char * filename,const string & s){
  return write_file(filename_script(filename).c_str(),s.c_str());
}

int load_script(const char * filename,std::string & s){
  const char * ptr=read_file(filename_script(filename).c_str());
  //dbg_printf("load %x %s\n",ptr,(ptr?ptr:""));
  if (!ptr){
    ptr=read_file(filename);
    if (!ptr)
      return 0;
  }
  //dbg_printf("load %x %x %x %x\n",ptr[0],ptr[1],ptr[2],ptr[3]);
  s=ptr;
  return 1;
}

size_t Bfile_ReadFile_OS4(const char * & hf_){
  const unsigned char * hf=(const unsigned char *)hf_;
  const size_t n=(((((hf[0]<<8)+hf[1])<<8)+hf[2])<<8)+hf[3];
  hf_ += 4;
  return n;
}

size_t Bfile_ReadFile_OS2(const char * & hf_){
  const unsigned char * hf=(const unsigned char *)hf_;
  const size_t n=(hf[0]<<8)+hf[1];
  hf_ += 2;
  return n;
}

void Bfile_ReadFile_OS(const char * &hf,char * dest,size_t len){
  memcpy(dest,hf,len);
  hf += len;
}

#define Current_Line (Start_Line + Cursor.y)
bool load_console_state_smem(const char * filename){
  char statbuf[32];
  strcpy(statbuf,"Loading ");
  strcat(statbuf,filename);  
  statuslinemsg(statbuf,COLOR_RED);
  //dbg_printf("load_console_state %s\n",filename);
  const char * hf=read_file(filename);
  //if (!hf){ console_output(filename,strlen(filename)); console_output(" not found\n",11); return true; }
  // if (strcmp(filename,"session.xw")){ console_output(hf,8); return true; }
  if (!hf) return false;
  string str;
  if (strncmp(hf,"#xwaspy\n",8)==0){
    hf+=8;
    const char * source=hf;
    for (;*source;source+=4){
      while (*source=='\n' || *source==' ' || (*source>='a' && *source<='~')){
        char c=*source;
        if (c=='}')
          c=')';
        if (c=='|')
          c=';';
        if (c=='~')
          c=':';
        str += c;
        ++source;
      }
      if (!*source)
        break;
      const unsigned char a=source[0]-xwaspy_shift,b=source[1]-xwaspy_shift,c=source[2]-xwaspy_shift,d=source[3]-xwaspy_shift;
      str += char((a<<2)|(b>>4));
      str += char((b<<4)|(c>>2));
      str += char((c<<6)|d);
    }
    hf=str.c_str();
  }
  size_t L=Bfile_ReadFile_OS4(hf);
  if (L>4096)
    return false;
  char BUF[L+4];
  BUF[1]=BUF[0]='/'; // avoid trying python compat.
  BUF[2]='\n';
  Bfile_ReadFile_OS(hf,BUF+3,L);
  BUF[L+3]=0;
  load_khicas_vars(BUF);
  dconsole_mode=1;
  // read script
  L=Bfile_ReadFile_OS4(hf);
  //dbg_printf("read script size=%i\n",L);
  if (L>0){
    char bufscript[L+1];
    Bfile_ReadFile_OS(hf,bufscript,L);
    bufscript[L]=0;
    if (edptr==nullptr)
      edptr=new textArea;
    if (edptr){
      edptr->elements.clear();
      edptr->clipline=-1;
      edptr->filename=remove_path(remove_extension(filename))+".py";
      //cout << "script " << edptr->filename << endl;
      edptr->editable=true;
      edptr->changed=false;
      edptr->python=FLOWCE_PYTHON; // the build's program syntax (a build with units has no Python)
      edptr->elements.clear();
      edptr->y=0;
      add(edptr,bufscript);
      edptr->line=0;
      //edptr->line=edptr->elements.size()-1;
      edptr->pos=0;
    }    
  }
  // read console state
  // insure parse messages are cleared
  Console_Init();
  Console_Clear_EditLine();
  for (;;){
    unsigned short int l = Bfile_ReadFile_OS2(hf);
    //dbg_printf("read level size=%i\n",l);
    if ( l==0){
      break;
    }
    unsigned short int curs = Bfile_ReadFile_OS2(hf);
    unsigned char type = *hf; ++hf;
    unsigned char readonly = *hf; ++hf;
    char buf[l+1];
    Bfile_ReadFile_OS(hf,buf,l);
    buf[l]=0;
    //dbg_printf("read level size=%i %s\n",l,buf);
    // ok line ready in buf
    while (Line[Current_Line].readonly)
      Console_MoveCursor(CURSOR_DOWN);
    Console_Input(buf);
    Console_NewLine(LINE_TYPE_INPUT, 1);
    if (Current_Line>0){
      line & cur=Line[Current_Line-1];
      cur.type=type;
      cur.readonly=readonly;
      cur.start_col+=curs;
    }
  }
  //dbg_printf("console loaded\n");
  console_changed=0;
  Console_FMenu_Init(); // insure the menus are sync-ed
  return true;
}

void Bfile_WriteFile_OS(char * & buf,const void * ptr,size_t len){
  memcpy(buf,ptr,len);
  buf += len;
}
void Bfile_WriteFile_OS4(char * & buf,size_t n){
  buf[0]= 0; // n>>24;
  buf[1]= (n>>16) & 0xff;
  buf[2]= (n & 0xffff)>>8;
  buf[3]= n & 0xff;
  buf += 4;
}
void Bfile_WriteFile_OS2(char * & buf,unsigned short n){
  buf[0]= n>>8;
  buf[1]= n & 0xff;
  buf += 2;
}

void save_console_state_smem(const char * filename,bool xwaspy){
  console_changed=0;
  //dbg_printf("save_console_state %s\n",filename);
  const string state(khicas_state());
  const int statesize=state.size();
  //dbg_printf("save_console_state %s %i\n",filename,statesize);
  string script;
  if (edptr)
    script=merge_area(edptr->elements);
  const int scriptsize=script.size();
  // save format: line_size (2), start_col(2), line_type (1), readonly (1), line
  int size=2*sizeof(int)+statesize+scriptsize;
  int start_row=0;//Last_Line-max_lines_saved; 
  if (start_row<0) start_row=0;
  for (int i=start_row;i<=Last_Line;++i){
    size += 2*sizeof(short)+2*sizeof(char)+strlen((const char *)Line[i].str);
  }
  char savebuf[size+4];
  char * hFile=savebuf;
  // save variables and modes
  Bfile_WriteFile_OS4(hFile, statesize);
  Bfile_WriteFile_OS(hFile, state.c_str(), statesize);
  // save script
  Bfile_WriteFile_OS4(hFile, scriptsize);
  Bfile_WriteFile_OS(hFile, script.c_str(), scriptsize);
  // save console state
  // save console state
  for (int i=start_row;i<=Last_Line;++i){
    const line & cur=Line[i];
    const unsigned short l=strlen((const char *)cur.str);
    Bfile_WriteFile_OS2(hFile, l);
    const unsigned short s=cur.start_col;
    Bfile_WriteFile_OS2(hFile, s);
    unsigned char c=cur.type;
    Bfile_WriteFile_OS(hFile, &c, sizeof(c));
    c=1;//cur.readonly;
    Bfile_WriteFile_OS(hFile, &c, sizeof(c));
    unsigned char buf[l+1];
    buf[l]=0;
    strcpy((char *)buf,(const char*)cur.str); 
    unsigned char *ptr=buf,*strend=ptr+l;
    for (;ptr<strend;++ptr){
      if (*ptr==0x9c)
        *ptr='\n';
    }
    Bfile_WriteFile_OS(hFile, buf, l);
  }
  constexpr const char BUF[2]={0,0};
  // char BUF[3]={0,0,0}; // might be more secure...
  Bfile_WriteFile_OS(hFile, BUF, sizeof(BUF));
  int len=hFile-savebuf;
  //dbg_printf("save_session xwaspy=%i len=%i data=%x %x %x %x %x %x %x %x\n",xwaspy,len,savebuf[0]& 0xff,savebuf[1]& 0xff,savebuf[2]& 0xff,savebuf[3]& 0xff,savebuf[4]& 0xff,savebuf[5]& 0xff,savebuf[6]& 0xff,savebuf[7]& 0xff);
  if (
#ifdef XWASPY
      xwaspy && len<8192
#else
      0
#endif
      ){
    // save as an ascii file beginning with #xwaspy
    char * buf=savebuf;
    int newlen=4*(len+2)/3+10;
    char newbuf[newlen];
    strcpy(newbuf,"#xwaspy\n");
    hFile=newbuf+8;
    for (int i=0;i<len;i+=3,hFile+=4){
      // keep space \n and a..z chars
      char c;
      while (i<len && ((c=buf[i])==' ' || c=='\n' || c=='{' || c==')' || c==';' || c==':' || c=='\n' || (c>='a' && c<='z')) ){
        if (c==')')
          c='}';
        if (c==':')
          c='~';
        if (c==';')
          c='|';
        *hFile=c;
        ++hFile;
        ++i;
      }
      unsigned char a=buf[i],b=i+1<len?buf[i+1]:0,C=i+2<len?buf[i+2]:0;
      hFile[0]=xwaspy_shift+(a>>2);
      hFile[1]=xwaspy_shift+(((a&3)<<4)|(b>>4));
      hFile[2]=xwaspy_shift+(((b&0xf)<<2)|(C>>6));
      hFile[3]=xwaspy_shift+(C&0x3f);
    }
    //dbg_printf("save_session XWASPY %s\n",newbuf);
    //*hFile=0; ++hFile; 
    //*hFile=0; ++hFile;
    write_file(filename,newbuf,hFile-newbuf);
  }
  else {
    write_file(filename,savebuf,len);
  }
}

int get_filename(char * filename,const char * extension,const char * title){
  lock_alpha();
  if (extension){
    const int l=strlen(extension);
    if (l && extension[l-1]=='y')
      handle_f5();
  }
  string str;
  const int res=inputline(focus_on?(title?title:"Save as"):(lang==1)?"EXIT ou chaine vide: annulation":"EXIT or empty string: cancel",(lang==1)?"Nom de fichier:":focus_on?"Name:":"Filename:",str,false);
  if (res==KEY_CTRL_EXIT || str.empty())
    return 0;
  if (extension && *extension){
    const size_t ext_len=strlen(extension);
    if (str.size()<ext_len || memcmp(str.c_str()+str.size()-ext_len,extension,ext_len))
      str += extension;
  }
  if (str.size()>MAX_FILENAME_SIZE){
    statuslinemsg(lang?"Nom trop long":"Filename too long",COLOR_RED);
    return 0;
  }
  strcpy(filename,str.c_str());
  // if file already exists, warn, otherwise create
  if (!file_exists(filename))
    return 1;
  if (confirm((lang==1)?"  Le fichier existe!":"  File exists!",
              (lang==1)?"F1: ecraser, F5: annuler":"F1: overwrite, F5: cancel"
              )==KEY_CTRL_F1)
    return 1;
  return 0;
}

// read equation or matrix from 83 OS to a string
// token list https://github.com/adriweb/tivars_lib_cpp/blob/master/programs_tokens.csv
string detokenize(const unsigned char * ptr,int len){
  //dbg_printf("detokenize ptr=%x len=%i\n",(unsigned)ptr,len);
  string res;
  if (len<=0)
    return res;
  const unsigned char * ptrend=ptr+len;
  char buf[32];
  for (;ptr<ptrend;++ptr,res+=buf){
    unsigned char c=*ptr;
    //dbg_printf("detokenize loop ptr=%x char c=%x\n",(unsigned)ptr,c);
    buf[0]=buf[1]=buf[2]=buf[3]=buf[4]=buf[5]=0; // prepare for *buf=character
    switch (c){
    case 0x6:
      *buf='[';
      continue;
    case 0x7:
      *buf=']';
      continue;
    case 0x8:
      *buf='{';
      continue;
    case 0x9:
      *buf='}';
      continue;
    case 0xc:
      //strcpy(buf,"^-1");
      buf[0]='^'; buf[1]='-'; buf[2]='1';
      continue;
    case 0xd:
      buf[0]='^'; buf[1]='2'; 
      // strcpy(buf,"^2");
      continue;
    case 0xe:
      buf[0]='^'; buf[1]='*'; 
      // strcpy(buf,"^*");
      continue;
    case 0xf:
      buf[0]='^'; buf[1]='3'; 
      // strcpy(buf,"^3");
      continue;
    case 0x10:
      *buf='(';
      continue;
    case 0x11:
      *buf=')';
      continue;
    case 0x24:
      strcpy(buf,"integrate(");
      continue;
    case 0x2a:
      *buf='"';
      continue;
    case 0x2b:
      *buf=',';
      continue;
    case 0x2c:
      *buf='i';
      continue;
    case 0x2d:
      *buf='!';
      continue;
    case 0x3a:
      *buf='.';
      continue;
      /*
    case 0x3b:
      strcpy(buf," or ");
      continue;
    case 0x3c:
      strcpy(buf," xor ");
      continue;
    case 0x40:
      strcpy(buf," and ");
      continue;
    case 0xb8:
      strcpy(buf,"not(");
      continue;
      */
    case 0x3e:
      *buf=':';
      continue;
    case 0x5e: // Yj?
      ++ptr;
      if (c==0x19)
        c=0xf;
      if (c>=0xf && c<=0x18){
        buf[0]='y';
        buf[1]='0'+(c-0xf);
        continue;
      }
      return "";
    case 0x6a:
      buf[0]='=';
      continue;
    case 0x6b:
      *buf='<';
      continue;
    case 0x6c:
      *buf='>';
      continue;
    case 0x6d:
      buf[0]='<'; buf[1]='='; // strcpy(buf,"<=");
      continue;
    case 0x6e:
      buf[0]='>'; buf[1]='='; // strcpy(buf,">=");
      continue;
    case 0x6f:
      buf[0]='!'; buf[1]='='; // strcpy(buf,"!=");
      continue;
    case 0x70:
      *buf='+';
      continue;
    case 0x71: case 0xb0:
      *buf='-';
      continue;
    case 0x82:
      *buf='*';
      continue;
    case 0x83:
      *buf='/';
      continue;
    case 0xf0:
      *buf='^';
      continue;
    case 0xac:
      buf[0]='p'; buf[1]='i'; // strcpy(buf,"pi");
      continue;
    case 0xb2:
      buf[0]='a'; buf[1]='b'; buf[2]='s'; buf[3]='('; // strcpy(buf,"abs(");
      continue;
    case 0xb3:
      buf[0]='d'; buf[1]='e'; buf[2]='t'; buf[3]='('; // strcpy(buf,"det(");
      continue;
    case 0xb4:
      buf[0]='i'; buf[1]='d'; buf[2]='n'; buf[3]='('; // strcpy(buf,"idn(");
      continue;
    case 0xb5:
      buf[0]='d'; buf[1]='i'; buf[2]='m'; buf[3]='('; // strcpy(buf,"dim(");
      continue;
    case 0xb6:
      buf[0]='s'; buf[1]='u'; buf[2]='m'; buf[3]='('; // strcpy(buf,"sum(");
      continue;
    case 0xb7:
      strcpy(buf,"product(");
      continue;
    case 0xb9:
      buf[0]='i'; buf[1]='n'; buf[2]='t'; buf[3]='('; // strcpy(buf,"int(");
      continue;
    case 0xba:
      buf[0]='f'; buf[1]='r'; buf[2]='a'; buf[3]='c';buf[4]='('; // strcpy(buf,"frac(");
      continue;
    case 0xbb:
      ++ptr; c=*ptr;
      if (c==8){
        buf[0]='l'; buf[1]='c'; buf[2]='m'; buf[3]='('; // strcpy(buf,"lcm(");
      } else if (c==9){
        buf[0]='g'; buf[1]='c'; buf[2]='d'; buf[3]='(';//strcpy(buf,"gcd(");
      } else if (c==0x25){
        buf[0]='c'; buf[1]='o'; buf[2]='n'; buf[3]='j';buf[4]='('; // strcpy(buf,"conj(");
      } else if (c==0x26){
        buf[0]='r'; buf[1]='e'; buf[2]='(';//strcpy(buf,"re(");
      } else if (c==0x27){
        buf[0]='i'; buf[1]='m'; buf[2]='(';  // strcpy(buf,"im(");
      } else if (c==0x2d){
        buf[0]='r'; buf[1]='e'; buf[2]='f'; buf[3]='('; // strcpy(buf,"ref(");
        //strcpy(buf,"ref(");
      } else if (c==0x2e){
        buf[0]='r'; buf[1]='r'; buf[2]='e'; buf[3]='f';buf[4]='('; // strcpy(buf,"rref(");
      } else return "";
      continue;
    case 0xbc:
      buf[0]='s'; buf[1]='q'; buf[2]='r'; buf[3]='t';buf[4]='('; // strcpy(buf,"frac(");
      continue;
    case 0xbe:
      buf[0]='l'; buf[1]='n'; buf[2]='(';  // strcpy(buf,"ln(");
      continue;
    case 0xbf:
      buf[0]='e'; buf[1]='x'; buf[2]='p'; buf[3]='('; // strcpy(buf,"exp(");
      continue;
    case 0xc0:
      strcpy(buf,"log10(");
      continue;
    case 0xc1:
      strcpy(buf,"alog10(");
      continue;
    case 0xc2:
      buf[0]='s'; buf[1]='i'; buf[2]='n'; buf[3]='('; // strcpy(buf,"sin(");
      continue;
    case 0xc3:
      buf[0]='a'; buf[1]='s'; buf[2]='i'; buf[3]='n'; buf[4]='('; // strcpy(buf,"asin(");
      continue;
    case 0xc4:
      buf[0]='c'; buf[1]='o'; buf[2]='s'; buf[3]='('; // strcpy(buf,"cos(");
      continue;
    case 0xc5:
      buf[0]='a'; buf[1]='c'; buf[2]='o'; buf[3]='s'; buf[4]='('; // strcpy(buf,"acos(");
      continue;
    case 0xc6:
      buf[0]='t'; buf[1]='a'; buf[2]='n'; buf[3]='('; // strcpy(buf,"tan(");
      continue;
    case 0xc7:
      buf[0]='a'; buf[1]='t'; buf[2]='a'; buf[3]='n'; buf[4]='('; // strcpy(buf,"atan(");
      continue;
    case 0xc8:
      buf[0]='s'; buf[1]='i'; buf[2]='n'; buf[3]='h'; buf[4]='('; // strcpy(buf,"sinh(");
      continue;
    case 0xc9:
      strcpy(buf,"asinh(");
      continue;
    case 0xca:
      buf[0]='c'; buf[1]='o'; buf[2]='s'; buf[3]='h'; buf[4]='('; // strcpy(buf,"cosh(");
      continue;
    case 0xcb:
      strcpy(buf,"acosh(");
      continue;
    case 0xcc:
      buf[0]='t'; buf[1]='a'; buf[2]='n'; buf[3]='h'; buf[4]='('; // strcpy(buf,"tanh(");
      continue;
    case 0xcd:
      strcpy(buf,"atanh(");
      continue;
    case 0xef:
      ++ptr;
      c=*ptr;
      if (c==0x2e){
        *buf='/';
        continue;
      }
      if (c==0x33){
        buf[0]='s'; buf[1]='u'; buf[2]='m'; buf[3]='('; // strcpy(buf,"sum(");
        continue;
      }
    default:
      if (c>=0x41 && c<=0x5a){
        *buf=c+0x20;
        continue;
      }
      if (c>=0x30 && c<=0x39){
        *buf=c;
        continue;
      }
      return ""; // unrecognized
    }
  }
  return res;
}

string get_tivar(const char * varname){
  if (!varname)
    return "";
  const unsigned char * ptr=nullptr;
  int len=0;
  if (varname[0]=='['){
    // matrix
  } else {
    const equ_t * eqptr=os_GetEquationData(varname,nullptr);
    //dbg_printf("get_tivar varname[0]=%x [1]=%x [2]=%x ptr=%x\n",varname[0],varname[1],varname[2],(unsigned) eqptr);
    if (!eqptr)
      return "";
    ptr=(unsigned char *)eqptr->data;
    len=eqptr->len;
  }
  return detokenize(ptr,len);
}

inline bool isalpha_(char c){
  return (c>='A' && c<='Z') || (c>='a' && c<='z') || (c>='0' && c<='9') || c=='_';
}
inline bool isnum(char c){
  return c>='0' && c<='9';
}

// source==s, target=t, returns size
int in_tokenize(const char * s,unsigned char * t){
  unsigned char * t0=t;
  bool instring=false,maybeneg=true,nextmaybeneg;
  for (;*s;maybeneg=nextmaybeneg){
    char c=*s;
    nextmaybeneg=false;
    if (c=='"'){
      instring = !instring;
      *t=0x2a; ++t; ++s;
      continue;
    }
    switch (c){
    case ',':
      nextmaybeneg=true;
      *t=0x2b; ++t; ++s;
      continue;
    case '.':
      *t=0x3a; ++t; ++s;
      continue;
    case '+':
      *t=0x70; ++t; ++s;
      continue;
    case '-':
      *t=maybeneg?0xb0:0x71; ++t; ++s;
      continue;
    case '*':
      *t=0x82; ++t; ++s;
      continue;
    case '/':
      nextmaybeneg=true;
      //*t=0x83; ++t; ++s;
      *t=0xef; t[1]=0x2e; t+=2; ++s;
      continue;
    case '^':
      nextmaybeneg=true;
      if (s[1]=='2'){
        *t=0xd; ++s;
      }
      else if (s[1]=='*'){
        *t=0xe; ++s;
      }
      else if (s[1]=='3'){
        *t=0xf; ++s;
      }
      else if (s[1]=='-' && s[2]=='1'){
        *t=0xc; s+=2;
      }
      else
        *t=0xf0;
      ++t; ++s;
      continue;
    case '=':
      nextmaybeneg=true;
      *t=0x6a; ++t; ++s;
      continue;
    case ':':
      nextmaybeneg=true;
      *t=0x3e; ++t; ++s;
      continue;
    case '[':
      nextmaybeneg=true;
      *t=0x6; ++t; ++s;
      continue;
    case ']':
      *t=0x7; ++t; ++s;
      continue;
    case '{':
      nextmaybeneg=true;
      *t=0x8; ++t; ++s;
      continue;
    case '}':
      *t=0x9; ++t; ++s;
      continue;
    case '>':
      nextmaybeneg=true;
      if (s[1]=='='){
        *t=0x6e; ++s;
      } else 
        *t=0x6c; 
      ++t; ++s;
      continue;
    case '<':
      nextmaybeneg=true;
      if (s[1]=='='){
        *t=0x6d; ++s;
      } else 
        *t=0x6b; 
      ++t; ++s;
      continue;
    case '!':
      if (s[1]=='='){
        *t=0x6f; ++s;
      } else 
        *t=0x2d; 
      ++t; ++s;
      continue;
    case '(':
      *t=0x10; ++t; ++s;
      nextmaybeneg=true;
      continue;
    case ')':
      *t=0x11; ++t; ++s;
      continue;
    }
    if (instring || isnum(c)){
      *t=c;
      ++t; ++s;
      continue;
    }
    const char * ptr=s;
    for (;*ptr;++ptr){
      if (!isalpha_(*ptr))
        break;
    }
    // we have a word between s and ptr, *s included, *ptr not
    int l=ptr-s;
    //dbg_printf("tokenize s=%s ptr=%s l=%i\n",s,ptr,l);
    if (l==0) return 0; // not recognized
    if (l==1){
      if (c=='i')
        *t=0x2c;
      else {
        if (c>='a' && c<='z') // capitalize single char
          *t=c-'a'+'A';
        else
          *t=c;
      }
      ++t; ++s; continue;
    }
    if (l==2){
      if (c=='p' && s[1]=='i'){
        *t=0xac;
        ++t; s+=2; continue;
      }
      else if ((c=='y' || c=='Y') && isnum(s[1])){
        *t=0x5e;
        c=s[1]-'0';
        if (c==0) c+=10;
        t[1]=c-1+0x10;
        t+=2; s+=2; continue;
      }
    }
    if (*ptr!='('){
      // copy unchanged
      memcpy(t,s,l);
      t+=l; s+=l;
      continue;
    }
    const char *s1=s;
    s=ptr+1; // skip open(
    bool invert=false,hyper=false;
    if (l==4){
      if (c=='a'){
        invert=true;
        ++s1; --l;
      }
      else if (s[3]=='h'){
        hyper=true;
        --l;
      }
    }
    if (l==5 && c=='a' && s[4]=='h'){
      hyper=true;
      ++s1; --l;
    }
    //dbg_printf("tokenize1 rems=%s s1=%s l=%i invert-%i hyper=%i\n",s,s1,l,invert,hyper);
    if (l==3){
      if (s1[0]=='e' && s1[1]=='x' && s1[2]=='p' && !hyper && !invert){
        nextmaybeneg=true;
        *t=0xbf;
        ++t;
        continue;
      }
      if (s1[0]=='l' && s1[1]=='o' && s1[2]=='g' && !hyper && !invert){
        nextmaybeneg=true;
        *t=0xbe;
        ++t;
        continue;
      }
      if (s1[0]=='a' && s1[1]=='b' && s1[2]=='s' && !hyper && !invert){
        nextmaybeneg=true;
        *t=0xb2;
        ++t;
        continue;
      }
      if (s1[0]=='s' && s1[1]=='i' && s1[2]=='n'){
        nextmaybeneg=true;
        *t=hyper?(invert?0xc9:0xc8):(invert?0xc3:0xc2);
        ++t;
        continue;
      }
      if (s1[0]=='c' && s1[1]=='o' && s1[2]=='s'){
        nextmaybeneg=true;
        *t=hyper?(invert?0xcb:0xca):(invert?0xc5:0xc4);
        ++t;
        continue;
      }
      if (s1[0]=='t' && s1[1]=='a' && s1[2]=='n'){
        nextmaybeneg=true;
        *t=hyper?(invert?0xcd:0xcc):(invert?0xc7:0xc6);
        ++t;
        continue;
      }
      if (s1[0]=='s' && s1[1]=='u' && s1[2]=='m'){
        nextmaybeneg=true;
        *t=0xb6;
      }
      else if (s1[0]=='g' && s1[1]=='c' && s1[2]=='d'){
        nextmaybeneg=true;
        *t=0xbb; ++t; *t=9;
      }
      else if (s1[0]=='l' && s1[1]=='c' && s1[2]=='m'){
        nextmaybeneg=true;
        *t=0xbb; ++t; *t=8;
      }
      else if (s1[0]=='d' && s1[1]=='e' && s1[2]=='t'){
        nextmaybeneg=true;
        *t=0xb3;
      }
      else if (s1[0]=='i' && s1[1]=='d' && s1[2]=='n'){
        nextmaybeneg=true;
        *t=0xb4;
      }
      else if (s1[0]=='d' && s1[1]=='i' && s1[2]=='m'){
        nextmaybeneg=true;
        *t=0xb5;
      }
      else if (s1[0]=='i' && s1[1]=='n' && s1[2]=='t'){
        nextmaybeneg=true;
        *t=0xb9;
      }
      else return 0; // not recognized
      ++t; continue;
    } // l==3
    if (l==2){
      if (s1[0]=='l' && s1[1]=='n'){
        nextmaybeneg=true;
        *t=0xbe;
      }
      else if (s1[0]=='r' && s1[1]=='e'){
        nextmaybeneg=true;
        *t=0xbb; ++t; *t=0x26;
      }
      else if (s1[0]=='i' && s1[1]=='m'){
        nextmaybeneg=true;
        *t=0xbb; ++t; *t=0x27;
      }
      else return 0;
      ++t;
      continue;
    }
    if (l==4){
      if (strncmp(s1,"sqrt",4)==0){
        nextmaybeneg=true;
        //dbg_printf("tokenize sqrt\n");
        *t=0xbc;
      }
      else if (strncmp(s1,"conj",4)==0){
        nextmaybeneg=true;
        *t=0xbb; ++t; *t=0x25;
      }
      else return 0;
      ++t;
      continue;
    }
    if (l==5 && strncmp(s1,"log10",5)==0){
      nextmaybeneg=true;
      *t=0xc0;
      ++t;
      continue;
    }
    if (l==6 && strncmp(s1,"alog10",6)==0){
      nextmaybeneg=true;
      *t=0xc1;
      ++t;
      continue;
    }
    if (l==7 && strncmp(s1,"product",7)==0){
      nextmaybeneg=true;
      *t=0xb7;
      ++t;
      continue;
    }
    if (l==9 && strncmp(s1,"integrate",9)==0){
      nextmaybeneg=true;
      *t=0x24;
      ++t;
      continue;
    }
  } // end while (*s)
  return t-t0;
}

void tokenize(const char * s,vector<unsigned char> & v){
  v.clear();
  const int l=strlen(s);
  if (l>4096)
    return;
  int extra=0;
  for (const char * ptr=s;*ptr;++ptr){
    if (*ptr=='/')
      ++extra;
  }
  if (l+extra>0xffff)
    return;
  unsigned char buf[2+l+extra]; // header + room for / translated to 2 bytes
  unsigned char * ptr=buf+2;
  const int tsize=in_tokenize(s,ptr);
  if (tsize<=0 || tsize>l+extra)
    return;
  buf[0]=tsize%256;
  buf[1]=tsize/256;
  vector<unsigned char> V(buf,buf+tsize+2);
  V.swap(v);
}

struct fracfloat {
  float n,d;
  fracfloat(float n_,float d_):n(n_),d(d_){}
  fracfloat(float n_):n(n_),d(1){}
  void inv(){
 const float tmp=n; n=d; d=tmp; }
};

fracfloat & operator += (fracfloat & f,float x){
  f.n += f.d*x;
  return f;
}

// d must be positive
bool dfc(real_t d,double eps,vector<int> &v){
  //real_t d=os_RealCopy(&d_);
  if (eps<1e-12)
    eps=1e-12;
  real_t i,f;
  for (;;){
    double df=os_RealToFloat(&d);
    int di=int(df);
    if (di>=1024)
      return false;
    v.push_back(di);
    i=os_Int24ToReal(di);
    //dbg_printf("dfc df=%f di=%i i=%f\n",df,di,os_RealToFloat(&i));
    d=os_RealSub(&d,&i);
    df=os_RealToFloat(&d);
    //dbg_printf("dfc push=%i rem=%f\n",di,df);
    if (df<eps)
      return true;
    d=os_RealInv(&d);
    eps=eps/(df*df);
  }
}

// x must be positive
bool dfc2f(real_t x,double eps,int & n,int & d){
  vector<int> v;
  if (!dfc(x,eps,v) || v.empty())
    return false;
  fracfloat res(v.back());
  for (;;){
    //dbg_printf("dfc2f vback=%i res=%f/%f\n",v.back(),res.n,res.d);
    v.pop_back();
    if (v.empty()){
      real_t N=os_FloatToReal(res.n),D=os_FloatToReal(res.d);
      D=os_RealInv(&D);
      const real_t F=os_RealMul(&N,&D),E=os_RealSub(&F,&x);
      const float e=os_RealToFloat(&E);
      //dbg_printf("dfc2f N=%f D=%f F=%f E=%f err=%f\n",os_RealToFloat(&N),os_RealToFloat(&D),os_RealToFloat(&F),os_RealToFloat(&E),e);
      if (fabs(e)<eps)
        break;
      return false;
    }
    res.inv();
    res+=v.back();
  }
  n=res.n; d=res.d;
  return n==res.n && d==res.d;  
}
  
bool dfc2s(real_t x,double eps,char * buf){
  unsigned char * ptr = (unsigned char *) &x;
  //dbg_printf("size=%i %x %x %x %x %x %x %x %x %x\n",sizeof(x),ptr[0],ptr[1],ptr[2],ptr[3],ptr[4],ptr[5],ptr[6],ptr[7],ptr[8]);
  const bool neg=ptr[0]&0x80;
  ptr[0] &= 0x7f; // make it positive
  const int type=ptr[0];
  if (type==0x1c){ // internal format thanks to Adrien Bertrand
    const int subtype=(ptr[1]&0xf0)/16;
    const int deno=(ptr[1]&0xf)*100+((ptr[2]&0xf0)/16)*10+(ptr[2]&0xf);
    const int a=((ptr[3]&0xf0)/16)*100+(ptr[3]&0xf)*10+(ptr[4]&0xf0)/16;
    const int b=(ptr[4]&0xf)*100+((ptr[5]&0xf0)/16)*10+(ptr[5]&0xf);
    const int c=((ptr[6]&0xf0)/16)*100+(ptr[6]&0xf)*10+((ptr[7]&0xf0)/16);
    const int d=(ptr[7]&0xf)*100+((ptr[8]&0xf0)/16)*10+(ptr[8]&0xf);
    if (c==0 || d==0){
      char fmt[]=" %i*sqrt(%i)/%i";
      if (subtype/2)
        fmt[0]='-';
      sprintf(buf,fmt,a,b,deno);
    }
    else if (a==0 || b==0){
      char fmt[]=" %i*sqrt(%i)/%i";
      if (subtype%2)
        fmt[0]='-';
      sprintf(buf,fmt,c,d,deno);
    } else {
      dbg_printf("sqrt subtype=%i deno=%i a=%i b=%i c=%i d=%i\n",subtype,deno,a,b,c,d);
      if (d==1){
        char fmt[]="( %i*sqrt(%i)+%i)/%i";
        if (subtype/2)
          fmt[1]='-';
        if (subtype%2)
          fmt[13]='-';
        sprintf(buf,fmt,a,c,b,deno);
      }
      else if (c==1){
        char fmt[]="( %i+%i*sqrt(%i))/%i";
        if (subtype/2)
          fmt[1]='-';
        if (subtype%2)
          fmt[4]='-';
        sprintf(buf,fmt,a,b,d,deno);
      } else {
        char fmt[]="( %i*sqrt(%i)+%i*sqrt(%i))/%i";
        if (subtype/2)
          fmt[1]='-';
        if (subtype%2)
          fmt[13]='-';
        sprintf(buf,fmt,a,c,b,d,deno);
      }
    }
    int l=strlen(buf);
    if (l>2 && buf[l-2]=='/' && buf[l-1]=='1'){
      buf[l-2]=0;
      l-=2;
    }
    for (int i=0;i<l-1;++i){
      if (buf[i]=='1' && buf[i+1]=='*'){
        for (int j=i+2;j<l;++j){
          buf[j-2]=buf[j];
        }
        l-=2;
        buf[l]=0;
      }
    }
    //dbg_printf("sqrt %s\n",buf);
    return true;
  }
  const bool divbypi= (type==0x20 || type==0x21);
  if (divbypi)
    ptr[0]=0;
  //double xf=os_RealToFloat(&x); dbg_printf("dfc2s %f\n",xf);
  int n,d;
  if (!dfc2f(x,eps,n,d))
    return false;
  sprintf(buf,divbypi?(neg?"-%d/%d*pi":"%d/%d*pi"):(neg?"-%d/%d":"%d/%d"),n,d);
  return true;
}

// ti/vars.h
// int os_GetMatrixDims(const char *name, int *rows, int *cols)
// int os_GetMatrixElement(const char *name, int row, int col, real_t *value)
// row/col start at 1
// int os_SetMatrixDims(const char *name, int rows, int cols)
// int os_SetMatrixElement(const char *name, int row, int col, const real_t *value)
//
// ti/real.h
// int24_t os_RealToInt24(const real_t *arg)
// real_t os_Int24ToReal(int24_t arg)
// float os_RealToFloat(const real_t *arg)
// real_t os_FloatToReal(float arg)
string get_timatrix(int matrix_index){
  //dbg_printf("get matrix %i\n",i);
  string res;
  if (matrix_index < 0 || matrix_index > 9)
    return res;
  const char * tab[]={
    OS_VAR_MAT_A,OS_VAR_MAT_B,OS_VAR_MAT_C,OS_VAR_MAT_D,OS_VAR_MAT_E,
    OS_VAR_MAT_F,OS_VAR_MAT_G,OS_VAR_MAT_H,OS_VAR_MAT_I,OS_VAR_MAT_J,
  };
  const char * name = tab[matrix_index];
  int r=0,c=0,err=os_GetMatrixDims(name,&r,&c);
  //dbg_printf("get matrix err=%i r=%i c=%i\n",err,r,c);
  if (err || r==0 || c==0)
    return res;
  res += '[';
  real_t aij;
  for (int i=0;i<r;){
    res += '[';
    for (int j=0;j<c;){
      if (os_GetMatrixElement(name,i+1,j+1,&aij))
        return "";
      const float f=os_RealToFloat(&aij);
      const int fi=f;
      //dbg_printf("get matrixelem err=%i i=%i j=%i f=%f fi=%i\n",err,i,j,f,fi);
      char buf[256];
      if (f==fi)
        sprintf(buf,"%d",fi);
      else {
        if (!dfc2s(aij,1e-12,buf))
          sprintf(buf,"%f",f);
      }
      res += buf;
      ++j;
      if (j<c)
        res += ',';
    }
    res += ']';
    ++i;
    if (i<r)
      res += ',';
  }
  res +=']';
  //dbg_printf("get matrix %s\n",res.c_str());
  return res;
}

#ifdef WITH_PERIODIC
  // table periodique, code adapte de https://github.com/M4xi1m3/nw-atom
  // avec l'aimable autorisation de diffusion sous licence GPL de Maxime Friess
  // https://tiplanet.org/forum/viewtopic.php?f=97&t=23094&p=247471#p247471
enum AtomType {
  ALKALI_METAL,
  ALKALI_EARTH_METAL,
  LANTHANIDE,
  ACTINIDE,
  TRANSITION_METAL,
  POST_TRANSITION_METAL,
  METALLOID,
  HALOGEN,
  REACTIVE_NONMETAL,
  NOBLE_GAS,
  UNKNOWN
};

struct AtomDef {
  uint8_t num;
  uint8_t x;
  uint8_t y;
  AtomType type;
  const char* name;
  const char* symbol;
  uint8_t neutrons;
  double mass;
  double electroneg;
};

const AtomDef atomsdefs[] = {
  {  1,  0,  0, REACTIVE_NONMETAL       , "Hydrogen"     , "H"   ,   0, 1.00784     , 2.2   },
  {  2, 17,  0, NOBLE_GAS               , "Helium"       , "He"  ,   2, 4.002602    , -1    },
  
  
  {  3,  0,  1, ALKALI_METAL            , "Lithium"      , "Li"  ,   4, 6.938       , 0.98  },
  {  4,  1,  1, ALKALI_EARTH_METAL      , "Beryllium"    , "Be"  ,   5, 9.012182    , 1.57  },
  {  5, 12,  1, METALLOID               , "Boron"        , "B"   ,   6, 10.806      , 2.04  },
  {  6, 13,  1, REACTIVE_NONMETAL       , "Carbon"       , "C"   ,   6, 12.0096     , 2.55  },
  {  7, 14,  1, REACTIVE_NONMETAL       , "Nitrogen"     , "N"   ,   7, 14.00643    , 3.04  },
  {  8, 15,  1, REACTIVE_NONMETAL       , "Oxygen"       , "O"   ,   8, 15.99903    , 3.44  },
  {  9, 16,  1, HALOGEN                 , "Fluorine"     , "F"   ,  10, 18.9984032  , 3.98  },
  { 10, 17,  1, NOBLE_GAS               , "Neon"         , "Ne"  ,  10, 20.1797     , -1    },
  
  
  { 11,  0,  2, ALKALI_METAL            , "Sodium"       , "Na"  ,  12, 22.9897693  , 0.93  },
  { 12,  1,  2, ALKALI_EARTH_METAL      , "Magnesium"    , "Mg"  ,  12, 24.3050     , 1.31  },
  { 13, 12,  2, POST_TRANSITION_METAL   , "Aluminium"    , "Al"  ,  14, 26.9815386  , 1.61  },
  { 14, 13,  2, METALLOID               , "Silicon"      , "Si"  ,  14, 28.084      , 1.9   },
  { 15, 14,  2, REACTIVE_NONMETAL       , "Phosphorus"   , "P"   ,  16, 30.973762   , 2.19  },
  { 16, 15,  2, REACTIVE_NONMETAL       , "Sulfur"       , "S"   ,  16, 32.059      , 2.58  },
  { 17, 16,  2, HALOGEN                 , "Chlorine"     , "Cl"  ,  18, 35.446      , 3.16  },
  { 18, 17,  2, NOBLE_GAS               , "Argon"        , "Ar"  ,  22, 39.948      , -1    },
  
  
  { 19,  0,  3, ALKALI_METAL            , "Potassium"    , "K"   ,  20, 39.0983     , 0.82  },
  { 20,  1,  3, ALKALI_EARTH_METAL      , "Calcium"      , "Ca"  ,  20, 40.078      , 1     },
  { 21,  2,  3, TRANSITION_METAL        , "Scandium"     , "Sc"  ,  24, 44.955912   , 1.36  },
  { 22,  3,  3, TRANSITION_METAL        , "Titanium"     , "Ti"  ,  26, 47.867      , 1.54  },
  { 23,  4,  3, TRANSITION_METAL        , "Vanadium"     , "V"   ,  28, 50.9415     , 1.63  },
  { 24,  5,  3, TRANSITION_METAL        , "Chromium"     , "Cr"  ,  28, 51.9961     , 1.66  },
  { 25,  6,  3, TRANSITION_METAL        , "Manganese"    , "Mn"  ,  30, 54.938045   , 1.55  },
  { 26,  7,  3, TRANSITION_METAL        , "Iron"         , "Fe"  ,  30, 55.845      , 1.83  },
  { 27,  8,  3, TRANSITION_METAL        , "Cobalt"       , "Co"  ,  32, 58.933195   , 1.88  },
  { 28,  9,  3, TRANSITION_METAL        , "Nickel"       , "Ni"  ,  30, 58.6934     , 1.91  },
  { 29, 10,  3, TRANSITION_METAL        , "Copper"       , "Cu"  ,  34, 63.546      , 1.9   },
  { 30, 11,  3, POST_TRANSITION_METAL   , "Zinc"         , "Zn"  ,  34, 65.38       , 1.65  },
  { 31, 12,  3, POST_TRANSITION_METAL   , "Gallium"      , "Ga"  ,  38, 69.723      , 1.81  },
  { 32, 13,  3, METALLOID               , "Germanium"    , "Ge"  ,  42, 72.63       , 2.01  },
  { 33, 14,  3, METALLOID               , "Arsenic"      , "As"  ,  42, 74.92160    , 2.18  },
  { 34, 15,  3, REACTIVE_NONMETAL       , "Selenium"     , "Se"  ,  46, 78.96       , 2.55  },
  { 35, 16,  3, HALOGEN                 , "Bromine"      , "Br"  ,  44, 79.904      , 2.96  },
  { 36, 17,  3, NOBLE_GAS               , "Krypton"      , "Kr"  ,  48, 83.798      , -1    },
  
  { 37,  0,  4, ALKALI_METAL            , "Rubidium"     , "Rb"  ,  48, 85.4678     , 0.82  },
  { 38,  1,  4, ALKALI_EARTH_METAL      , "Strontium"    , "Sr"  ,  50, 87.62       , 0.95  },
  { 39,  2,  4, TRANSITION_METAL        , "Yttrium"      , "Y"   ,  50, 88.90585    , 1.22  },
  { 40,  3,  4, TRANSITION_METAL        , "Zirconium"    , "Zr"  ,  50, 91.224      , 1.33  },
  { 41,  4,  4, TRANSITION_METAL        , "Niobium"      , "Nb"  ,  52, 92.90638    , 1.6   },
  { 42,  5,  4, TRANSITION_METAL        , "Molybdenum"   , "Mo"  ,  56, 95.96       , 2.16  },
  { 43,  6,  4, TRANSITION_METAL        , "Technetium"   , "Tc"  ,  55, 98          , 2.10  },
  { 44,  7,  4, TRANSITION_METAL        , "Ruthemium"    , "Ru"  ,  58, 101.07      , 2.2   },
  { 45,  8,  4, TRANSITION_METAL        , "Rhodium"      , "Rh"  ,  58, 102.90550   , 2.28  },
  { 46,  9,  4, TRANSITION_METAL        , "Palladium"    , "Pd"  ,  60, 106.42      , 2.20  },
  { 47, 10,  4, TRANSITION_METAL        , "Silver"       , "Ag"  ,  60, 107.8682    , 1.93  },
  { 48, 11,  4, POST_TRANSITION_METAL   , "Cadmium"      , "Cd"  ,  66, 112.411     , 1.69  },
  { 49, 12,  4, POST_TRANSITION_METAL   , "Indium"       , "In"  ,  66, 114.818     , 1.78  },
  { 50, 13,  4, POST_TRANSITION_METAL   , "Tin"          , "Sn"  ,  70, 118.710     , 1.96  },
  { 51, 14,  4, METALLOID               , "Antimony"     , "Sb"  ,  70, 121.760     , 2.05  },
  { 52, 15,  4, METALLOID               , "Tellurium"    , "Te"  ,  78, 127.60      , 2.1   },
  { 53, 16,  4, HALOGEN                 , "Indine"       , "I"   ,  74, 126.90447   , 2.66  },
  { 54, 17,  4, NOBLE_GAS               , "Xenon"        , "Xe"  ,  78, 131.293     , 2.60  },
  
  
  { 55,  0,  5, ALKALI_METAL            , "Caesium"      , "Cs"  ,  78, 132.905452  , 0.79  },
  { 56,  1,  5, ALKALI_EARTH_METAL      , "Barium"       , "Ba"  ,  82, 137.327     , 0.89  },

  { 57,  3,  7, LANTHANIDE              , "Lanthanum"    , "La"  ,  82, 138.90547   , 1.10  },
  { 58,  4,  7, LANTHANIDE              , "Cerium"       , "Ce"  ,  82, 140.116     , 1.12  },
  { 59,  5,  7, LANTHANIDE              , "Praseodymium" , "Pr"  ,  82, 140.90765   , 1.13  },
  { 60,  6,  7, LANTHANIDE              , "Neodymium"    , "Nd"  ,  82, 144.242     , 1.14  },
  { 61,  7,  7, LANTHANIDE              , "Promethium"   , "Pm"  ,  84, 145         , 1.13  },
  { 62,  8,  7, LANTHANIDE              , "Samarium"     , "Sm"  ,  90, 150.36      , 1.17  },
  { 63,  9,  7, LANTHANIDE              , "Europium"     , "Eu"  ,  90, 151.964     , 1.12  },
  { 64, 10,  7, LANTHANIDE              , "Gadolinium"   , "Gd"  ,  94, 157.25      , 1.20  },
  { 65, 11,  7, LANTHANIDE              , "Terbium"      , "Tb"  ,  94, 158.92535   , 1.12  },
  { 66, 12,  7, LANTHANIDE              , "Dyxprosium"   , "Dy"  ,  98, 162.500     , 1.22  },
  { 67, 13,  7, LANTHANIDE              , "Holmium"      , "Ho"  ,  98, 164.93032   , 1.23  },
  { 68, 14,  7, LANTHANIDE              , "Erbium"       , "Er"  ,  98, 167.259     , 1.24  },
  { 69, 15,  7, LANTHANIDE              , "Thulium"      , "Tm"  , 100, 168.93421   , 1.25  },
  { 70, 16,  7, LANTHANIDE              , "Ytterbium"    , "Yb"  , 104, 173.054     , 1.1   },
  { 71, 17,  7, LANTHANIDE              , "Lutetium"     , "Lu"  , 104, 174.9668    , 1.0   },

  { 72,  3,  5, TRANSITION_METAL        , "Hafnium"      , "Hf"  , 108, 178.49      , 1.3   },
  { 73,  4,  5, TRANSITION_METAL        , "Tantalum"     , "Ta"  , 108, 180.94788   , 1.5   },
  { 74,  5,  5, TRANSITION_METAL        , "Tungsten"     , "W"   , 110, 183.84      , 1.7   },
  { 75,  6,  5, TRANSITION_METAL        , "Rhenium"      , "Re"  , 112, 186.207     , 1.9   },
  { 76,  7,  5, TRANSITION_METAL        , "Osmium"       , "Os"  , 116, 190.23      , 2.2   },
  { 77,  8,  5, TRANSITION_METAL        , "Iridium"      , "Ir"  , 116, 192.217     , 2.2   },
  { 78,  9,  5, TRANSITION_METAL        , "Platinum"     , "Pt"  , 117, 195.084     , 2.2   },
  { 79, 10,  5, TRANSITION_METAL        , "Gold"         , "Au"  , 118, 196.966569  , 2.4   },
  { 80, 11,  5, POST_TRANSITION_METAL   , "Mercury"      , "Hg"  , 122, 200.59      , 1.9   },
  { 81, 12,  5, POST_TRANSITION_METAL   , "Thalium"      , "Tl"  , 124, 204.382     , 1.8   },
  { 82, 13,  5, POST_TRANSITION_METAL   , "Lead"         , "Pb"  , 126, 207.2       , 1.8   },
  { 83, 14,  5, POST_TRANSITION_METAL   , "Bismuth"      , "Bi"  , 126, 208.98040   , 1.9   },
  { 84, 15,  5, POST_TRANSITION_METAL   , "Polonium"     , "Po"  , 126, 209         , 2.0   },
  { 85, 16,  5, HALOGEN                 , "Astatine"     , "At"  , 125, 210         , 2.2   },
  { 86, 17,  5, NOBLE_GAS               , "Radon"        , "Rn"  , 136, 222         , 2.2   },
  
  
  { 87,  0,  6, ALKALI_METAL            , "Francium"     , "Fr"  , 136, 223         , 0.7   },
  { 88,  1,  6, ALKALI_EARTH_METAL      , "Radium"       , "Ra"  , 138, 226         , 0.9   },

  { 89,  3,  8, ACTINIDE                , "Actinium"     , "Ac"  , 138, 227         , 1.1   },
  { 90,  4,  8, ACTINIDE                , "Thorium"      , "Th"  , 142, 232.03806   , 1.3   },
  { 91,  5,  8, ACTINIDE                , "Protactinium" , "Pa"  , 140, 231.03588   , 1.5   },
  { 92,  6,  8, ACTINIDE                , "Uranium"      , "U"   , 146, 238.02891   , 1.38   },
  { 93,  7,  8, ACTINIDE                , "Neptunium"    , "Np"  , 144, 237         , 1.36   },
  { 94,  8,  8, ACTINIDE                , "Plutonium"    , "Pu"  , 150, 244         , 1.28   },
  { 95,  9,  8, ACTINIDE                , "Americium"    , "Am"  , 148, 243         , 1.13  },
  { 96, 10,  8, ACTINIDE                , "Curium"       , "Cm"  , 151, 247         , 1.28  },
  { 97, 11,  8, ACTINIDE                , "Berkellum"    , "Bk"  , 150, 247         , 1.3   },
  { 98, 12,  8, ACTINIDE                , "Californium"  , "Cf"  , 153, 251         , 1.3   },
  { 99, 13,  8, ACTINIDE                , "Einsteinium"  , "Es"  , 153, 252         , 1.3   },
  {100, 14,  8, ACTINIDE                , "Fermium"      , "Fm"  , 157, 257         , 1.3   },
  {101, 15,  8, ACTINIDE                , "Mendelevium"  , "Md"  , 157, 258         , 1.3   },
  {102, 16,  8, ACTINIDE                , "Nobelium"     , "No"  , 157, 259         , 1.3   },
  {103, 17,  8, ACTINIDE                , "Lawrencium"   , "Lr"  , 163, 262         , 1.3   },

  {104,  3,  6, TRANSITION_METAL        , "Rutherfordium", "Rf"  , 163, 261         , -1    },
  {105,  4,  6, TRANSITION_METAL        , "Dubnium"      , "Db"  , 163, 262         , -1    },
  {106,  5,  6, TRANSITION_METAL        , "Seaborgium"   , "Sg"  , 163, 263         , -1    },
  {107,  6,  6, TRANSITION_METAL        , "Bohrium"      , "Bh"  , 163, 264         , -1    },
  {108,  7,  6, TRANSITION_METAL        , "Hassium"      , "Hs"  , 169, 265         , -1    },
  {109,  8,  6, UNKNOWN                 , "Meitnerium"   , "Mt"  , 169, 268         , -1    },
  {110,  9,  6, UNKNOWN                 , "Damstadtium"  , "Ds"  , 171, 281         , -1    },
  {111, 10,  6, UNKNOWN                 , "Roentgenium"  , "Rg"  , 171, 273         , -1    },
  {112, 11,  6, POST_TRANSITION_METAL   , "Coppernicium" , "Cn"  , 173, 277         , -1    },
  {113, 12,  6, UNKNOWN                 , "Nihonium"     , "Nh"  , 173, 283         , -1    },
  {114, 13,  6, UNKNOWN                 , "Flerovium"    , "Fl"  , 175, 285         , -1    },
  {115, 14,  6, UNKNOWN                 , "Moscovium"    , "Mv"  , 174, 287         , -1    },
  {116, 15,  6, UNKNOWN                 , "Livermorium"  , "Lv"  , 177, 289         , -1    },
  {117, 16,  6, UNKNOWN                 , "Tennessine"   , "Ts"  , 177, 294         , -1    },
  {118, 17,  6, NOBLE_GAS               , "Oganesson"    , "Og"  , 176, 293         , -1    },
  
};

  constexpr const int C16=16;
  constexpr const int C17=17;
  constexpr const int c18=18;
  constexpr const int c6=18;

  int rgb24to16(int c){
    const int r=(c>>16)&0xff,g=(c>>8)&0xff,b=c&0xff;
    return (((r*32)/256)<<11) | (((g*64)/256)<<5) | (b*32/256);
  }


  void stroke_rectangle(int x,int y,int w,int h,int c){
    draw_line(x,y,x+w,y,c);
    draw_line(x,y+h,x+w,y+h,c);
    draw_line(x,y,x,y+h,c);
    draw_line(x+w,y,x+w,y+h,c);
  }


void drawAtom(uint8_t id) {
  int fill = rgb24to16(0xeeeeee);

  switch(atomsdefs[id].type) {
    case ALKALI_METAL:
      fill = rgb24to16(0xffaa00);
      break;
    case ALKALI_EARTH_METAL:
      fill = rgb24to16(0xf6f200);
      break;
    case LANTHANIDE:
      fill = rgb24to16(0xffaa8b);
      break;
    case ACTINIDE:
      fill = rgb24to16(0xdeaacd);
      break;
    case TRANSITION_METAL:
      fill = rgb24to16(0xde999c);
      break;
    case POST_TRANSITION_METAL:
      fill = rgb24to16(0x9cbaac);
      break;
    case METALLOID:
      fill = rgb24to16(0x52ce8b);
      break;
    case REACTIVE_NONMETAL:
      fill = rgb24to16(0x00ee00);
      break;
    case NOBLE_GAS:
      fill = rgb24to16(0x8baaff);
      break;
    case HALOGEN:
      fill = rgb24to16(0x00debd);
      break;
    default:
      break;
  }
  const int delta = atomsdefs[id].y>=7?0:2;
  drawRectangle(6+atomsdefs[id].x*C17, c6+2-delta+atomsdefs[id].y*C17, c18, c18, fill);
  stroke_rectangle(6+atomsdefs[id].x*C17, c6+2-delta+atomsdefs[id].y*C17, c18, c18, rgb24to16(0x525552));
  os_draw_string_small(6+2+atomsdefs[id].x*C17, -18+c6+4-delta+atomsdefs[id].y* C17, _BLACK, fill, atomsdefs[id].symbol);
}

  int periodic_table(const char * & name,const char * & symbol,char * protons,char * nucleons,char * mass,char * electroneg){
    bool partial_draw=false,redraw=true;
    int cursor_pos=0;
    constexpr const int ATOM_NUMS=sizeof(atomsdefs)/sizeof(AtomDef);
    for (;;){
      if (redraw){
	if (partial_draw) {
	  partial_draw = false;
	  drawRectangle(50, 18, 160, 57-18, _WHITE);
	  drawRectangle(0, 185, LCD_WIDTH_PX, 15, _WHITE);
	} else {
	  drawRectangle(0,STATUS_AREA_PX,LCD_WIDTH_PX,LCD_HEIGHT_PX-STATUS_AREA_PX,_WHITE);
          os_draw_string_medium_(2,200,"enter: all, P:protons, N:nucl., M:mass, E:khi");
          for(int i = 0; i < ATOM_NUMS; i++) {
            drawAtom(i);
          }
        }
        // show cursor position
        const int delta = atomsdefs[cursor_pos].y>=7?0:2;
        stroke_rectangle(6+atomsdefs[cursor_pos].x*C17, c6+2-delta+atomsdefs[cursor_pos].y*C17, c18, c18, 0);
        stroke_rectangle(6+1+atomsdefs[cursor_pos].x*C17, c6+3-delta+atomsdefs[cursor_pos].y*C17, C16, C16, 0x000000);
  
	drawRectangle(48,  99, 2, 61,rgb24to16(0x525552));
	drawRectangle(48, 141, 9,  2, rgb24to16(0x525552));
	drawRectangle(48, 158, 9,  2, rgb24to16(0x525552));

	const int prot=atomsdefs[cursor_pos].num;
	sprintf(protons,"%i",prot);
	const int nuc=atomsdefs[cursor_pos].neutrons+atomsdefs[cursor_pos].num;
	sprintf(nucleons,"%i",nuc);
	
	symbol=atomsdefs[cursor_pos].symbol;
	os_draw_string_(73,23,symbol);
	name=atomsdefs[cursor_pos].name;
	os_draw_string_small_(110,27,name);
	os_draw_string_small_(50,18,nucleons);
	os_draw_string_small_(50,31,protons);
	sprintf(mass,"M:%f",atomsdefs[cursor_pos].mass);
	sprintf(electroneg,"khi:%f",atomsdefs[cursor_pos].electroneg);
	os_draw_string_medium_(2,186,mass);
	os_draw_string_medium_(160,186,electroneg);
      }
      redraw=false; partial_draw=true;
      int key;
      GetKey(&key);
      drawAtom(cursor_pos); // undraw stroke position
      if (key==KEY_SHUTDOWN)
	return key;
      if (key==KEY_PRGM_ACON)
	redraw=true;
      if (key==KEY_CTRL_EXIT)
	return 0;
      if (key==KEY_CTRL_EXE || key==KEY_CTRL_OK)
	return 1|4|8|16|32;
      if (key=='s' || key==KEY_CHAR_5)
	return 2;
      if (key=='p' || key==KEY_CHAR_LPAR)
	return 4;
      if (key=='n' || key==KEY_CHAR_8)
	return 8;
      if (key=='m' || key==KEY_CHAR_7)
	return 16;
      if (key=='e' || key==KEY_CHAR_COMMA)
	return 32;
      if (key==KEY_CTRL_LEFT){
	if (cursor_pos>0)
	  --cursor_pos;
        redraw=true;
      }
      if (key==KEY_CTRL_RIGHT){
	if (cursor_pos< ATOM_NUMS-1)
	  ++cursor_pos;
        redraw=true;
      }
      if (key==KEY_CTRL_UP){
        redraw=true;
	const uint8_t curr_x = atomsdefs[cursor_pos].x;
	const uint8_t curr_y = atomsdefs[cursor_pos].y;
	if (curr_y > 0 && curr_y <= 9) {
	  for(uint8_t i = 0; i < ATOM_NUMS; i++) {
	    if (atomsdefs[i].x == curr_x && atomsdefs[i].y == curr_y - 1) {
	      cursor_pos = i;
	    }
	  }
	}	
      }
      if (key==KEY_CTRL_DOWN){
        redraw=true;
	const uint8_t curr_x = atomsdefs[cursor_pos].x;
	const uint8_t curr_y = atomsdefs[cursor_pos].y;
	if (curr_y >= 0 && curr_y < 9) {
	  for (uint8_t i = 0; i < ATOM_NUMS; i++) {
	    if (atomsdefs[i].x == curr_x && atomsdefs[i].y == curr_y + 1) {
	      cursor_pos = i;
	      break;
	    }
	  }
	}
      }
    } // end endless for
  } // end periodic_table

#endif
