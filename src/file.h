#ifndef __FILEPROVIDER_H
#define __FILEPROVIDER_H

#include "menuGUI.h"

#include <string>
#define std ustl
std::string detokenize(const unsigned char * ptr,int len);
std::string get_tivar(const char * varname);
// source==s, target=t, returns size
int in_tokenize(const char * s,unsigned char * t);
void tokenize(const char * s,std::vector<unsigned char> & v);
std::string get_timatrix(int i);

#define MAX_FILENAME_SIZE 32 // 270 //full path with //fls0/, extension and everything
#define MAX_ITEMS_IN_DIR 200
#define MAX_TEXTVIEWER_FILESIZE 16*1024
extern "C" int file_exists(const char * filename);
int get_filename(char * filename,const char * extension,const char * title=0); // title: the Focus prompt's ("Save as")

typedef struct
{
  char filename[MAX_FILENAME_SIZE]; //filename, not proper for use with Bfile.
  char visname[42]; //visible name, only for menus. use nameFromFilename to get the proper name.
  short action; // mostly for clipboard, can be used to tag something to do with the file
  short isfolder; // because menuitem shouldn't be the only struct holding this info
  int size; // file size
} File; // right now File only holds the filename as other fields are now set directly on a MenuItem array

typedef struct
{
  unsigned short id, type;
  unsigned long fsize, dsize;
  unsigned int property;
  unsigned long address;
} file_type_t;

#define GETFILES_SUCCESS 0
#define GETFILES_MAX_FILES_REACHED 1

int GetFiles(File* files, MenuItem* menuitems, char* basepath, int* count, char* filter);
void nameFromFilename(char* filename, char* name);

#define FILE_ICON_FOLDER 0
#define FILE_ICON_G3M 1
#define FILE_ICON_G3E 2
#define FILE_ICON_G3A 3
#define FILE_ICON_G3P 4
#define FILE_ICON_G3B 5
#define FILE_ICON_BMP 6
#define FILE_ICON_TXT 7
#define FILE_ICON_CSV 8
#define FILE_ICON_OTHER 9
int fileIconFromName(char* name);

int casio_fileBrowser(char* filename, const char* filter, const char* title);
int fileBrowserSub(char* browserbasepath, char* filename, const char* filter, const char* title);
void shortenDisplayPath(char* longpath, char* shortpath, int jump=1);
//void buildIconTable(MenuItemIcon* icontable);
void save_console_state_smem(const char * filename,bool xwaspy=false);
bool load_console_state_smem(const char * filename);
#ifdef WITH_PERIODIC
int periodic_table(const char * & name,const char * & symbol,char * protons,char * nucleons,char * mass,char * electroneg);
#endif

#endif
