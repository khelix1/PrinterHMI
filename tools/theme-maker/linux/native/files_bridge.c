/* Sample library backend only. Every Files widget comes from main/ui_files.c. */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui_files.h"
#include "ui_shell.h"
#include "ui_preview_lightbox.h"
#include "printer_preview_cache.h"
#include "printer_connection_policy.h"
#include "ui_theme.h"
#include "moonraker.h"
typedef struct {const char *path;double mib;int minutes;bool extras;} sample_file_t;
static const sample_file_t library[]={
 {"Spool holder.gcode",1.4,128,false},{"Tool tray.gcode",2.2,210,true},{"Cable organizer.gcode",.8,70,false},
 {"Calibration cube.gcode",.2,26,false},{"Planter.gcode",3.1,240,true},{"Bracket.gcode",1.1,90,false},
 {"Calibration/Calibration cube.gcode",.2,26,false}};
static bool files_open;
static bool (*files_send)(const char *);
static char folder[64],query[64],selected[256];
static unsigned sort_kind;
static ui_thumbnail_t *detail_thumbnail;
static const lv_image_dsc_t *sample_image(void){const char *file;uint32_t revision;return printer_preview_cache_image(0,&file,&revision);}
static bool contains(const char *text,const char *needle){
 for(;*text;text++){const char *a=text,*b=needle;while(*a && *b && tolower((unsigned char)*a)==tolower((unsigned char)*b)){a++;b++;}if(!*b)return true;}
 return !*needle;
}
static const sample_file_t *lookup(const char *path){for(unsigned i=0;i<sizeof(library)/sizeof(library[0]);i++)if(!strcmp(path,library[i].path))return &library[i];return NULL;}
static void preview(const char *path){if(files_open && lookup(path))ui_files_set_file_thumbnail(path,sample_image());}
static int compare(const void *left,const void *right){
 unsigned a=*(const unsigned *)left,b=*(const unsigned *)right;
 if(sort_kind==1)return a>b?-1:a<b?1:0;
 if(sort_kind==2 && library[a].mib!=library[b].mib)return library[a].mib>library[b].mib?-1:1;
 return strcmp(library[a].path,library[b].path);
}
static void refresh(void){
 if(!files_open || !ui_files_can_refresh_rows())return;
 ui_files_clear_rows();ui_files_set_status("");
 ui_files_set_breadcrumb(folder[0]?folder:NULL);ui_files_set_search_text(query);
 const char *sorts[]={"Name","Newest","Size"};ui_files_set_sort_text(sorts[sort_kind]);
 unsigned order[sizeof(library)/sizeof(library[0])],count=0;
 for(unsigned i=0;i<sizeof(library)/sizeof(library[0]);i++){
  const char *slash=strchr(library[i].path,'/');
  if((folder[0] && (!slash || strncmp(library[i].path,"Calibration/",12))) || (!folder[0] && slash))continue;
  if(contains(library[i].path,query))order[count++]=i;
 }
 qsort(order,count,sizeof(*order),compare);
 int y=0,step=ui_theme_density_metric(76,92,104);
 if(!folder[0] && !query[0]){ui_files_add_folder_button("Calibration","Calibration",y);y+=step;}
 for(unsigned i=0;i<count;i++){unsigned index=order[i];ui_files_add_file_entry(library[index].path,library[index].mib*1024*1024,1700000000+index*86400,y);y+=step;}
 if(!count && (folder[0] || query[0]))ui_files_set_status(query[0]?"No files match current search":"No files in this folder");
}
static void cancel_detail(void){selected[0]=0;ui_files_close_detail_popup();}
static void start(void){
 moonraker_state_t m;moonraker_state_snapshot(&m);
 if(!files_open || !selected[0] || !lookup(selected) || !files_send || printer_idle_reason(&m)[0])return;
 char command[320];snprintf(command,sizeof(command),"SDCARD_PRINT_FILE FILENAME=\"%s\"",selected);
 if(!files_send(command)){ui_files_print_feedback("Simulated command was not accepted.");return;}
 cancel_detail();ui_shell_page_action(UI_SHELL_PAGE_DASHBOARD);
}
static void select_file(const char *path){
 const sample_file_t *file=lookup(path);if(!files_open || !file)return;
 snprintf(selected,sizeof(selected),"%s",path);
 char metadata[1600];snprintf(metadata,sizeof(metadata),"Layer height: 0.20 mm\nFilament: PLA\nEstimated time: %d min\nSize: %.1f MB%s",file->minutes,file->mib,file->extras?"\nSlicer: PrusaSlicer\nPerimeters: 3\nInfill: 20% gyroid\nNozzle diameter: 0.4 mm\nFilament diameter: 1.75 mm\nBed temperature: 60 C\nNozzle temperature: 210 C\nSupport material: off\nFirst layer height: 0.24 mm\nPrinter: Sermoon D1\nNotes: Local demonstration metadata; no file is downloaded.":"");
 lv_obj_t *box=NULL;ui_thumbnail_t *view=NULL;
 ui_files_show_detail_popup(path,metadata,&box,&view,cancel_detail,start);
 if(view){ui_thumbnail_bind_owner(view,&detail_thumbnail);ui_thumbnail_show_file_image(view,sample_image(),256,path);}
 ui_files_update_detail_metadata(metadata,true);
}
static void search(const char *text){snprintf(query,sizeof(query),"%s",text?text:"");cancel_detail();refresh();}
static void sort(void){sort_kind=(sort_kind+1)%3;refresh();}
static void choose_folder(const char *path){if(strcmp(path,"Calibration"))return;snprintf(folder,sizeof(folder),"Calibration");query[0]=0;cancel_detail();refresh();}
static void up(void){folder[0]=query[0]=0;cancel_detail();refresh();}
void host_files_close(void){
 if(!files_open)return;
 files_open=false;files_send=NULL;selected[0]=0;
 ui_preview_lightbox_close();ui_files_hide();
 ui_files_set_callbacks(NULL,NULL,NULL);ui_files_set_browser_callbacks(NULL,NULL,NULL,NULL);
}
void host_files_show(bool (*send)(const char *)){
 files_open=true;files_send=send;folder[0]=query[0]=selected[0]=0;sort_kind=0;
 ui_files_set_callbacks(refresh,select_file,preview);ui_files_set_browser_callbacks(search,sort,choose_folder,up);ui_files_show();
 refresh();
}
