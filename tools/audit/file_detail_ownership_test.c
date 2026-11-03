#include "file_detail_loader.c"
#include "encoder.inc"
#include <assert.h>
static void (*pending)(void *);
static void *argument;
static uint32_t profile=1;
static unsigned callbacks,fetches;
static bool fail_task,change_profile,cancel_fetch;
static char expected_key[MOONRAKER_CONFIG_API_KEY_LENGTH];
size_t strlcpy(char *out,const char *in,size_t size){size_t n=strlen(in);if(size){size_t take=n<size-1?n:size-1;memcpy(out,in,take);out[take]=0;}return n;}
uint32_t moonraker_config_generation(void){return profile;}
int xTaskCreatePinnedToCore(void(*fn)(void*),const char*n,unsigned stack,void*arg,unsigned priority,void*handle,int core){(void)n;(void)stack;(void)priority;(void)handle;(void)core;if(fail_task)return 0;pending=fn;argument=arg;return 1;}
bool bsp_display_lock(unsigned timeout){(void)timeout;return true;}
void bsp_display_unlock(void){}
bool moonraker_fetch_file_metadata(const char*h,int p,const char*k,const char*f,char*b,size_t size,int*c,esp_err_t*e){
 assert(!strcmp(h,"printer.local")&&p==7125);assert(!strcmp(k,expected_key));
 assert(strlen(f)==456);for(int i=0;i<150;i++)assert(!memcmp(f+3*i,"%20",3));assert(!strcmp(f+450,".gcode"));
 fetches++;if(change_profile)profile++;if(cancel_fetch)file_detail_loader_cancel();snprintf(b,size,"{}");*c=200;*e=ESP_OK;return true;
}
bool printer_files_build_metadata_text(const char*f,const char*b,char*t,size_t ts,char*m,size_t ms){(void)f;(void)b;snprintf(t,ts,"thumb.png");snprintf(m,ms,"ok");return true;}
static void ready(const char*f,bool ok,const char*m,const char*t){(void)f;assert(ok&&!strcmp(m,"ok")&&!strcmp(t,"thumb.png"));callbacks++;}
static void work(void){void(*fn)(void*)=pending;pending=NULL;fn(argument);}
int main(void){char file[160];memset(file,' ',150);strcpy(file+150,".gcode");memset(expected_key,'k',159);expected_key[159]=0;
 assert(file_detail_loader_start("printer.local",7125,expected_key,file,ready));work();assert(callbacks==1&&fetches==1);
 assert(file_detail_loader_start("printer.local",7125,expected_key,file,ready));file_detail_loader_cancel();work();assert(callbacks==1&&fetches==1);
 assert(file_detail_loader_start("printer.local",7125,expected_key,file,ready));change_profile=true;work();change_profile=false;assert(callbacks==1);
 assert(file_detail_loader_start("printer.local",7125,expected_key,file,ready));cancel_fetch=true;work();cancel_fetch=false;assert(callbacks==1);
 char oversized[161];memset(oversized,'x',160);oversized[160]=0;
 assert(!file_detail_loader_start("printer.local",65536,expected_key,file,ready));assert(!file_detail_loader_start("printer.local",7125,oversized,file,ready));assert(!file_detail_loader_start("printer.local",7125,expected_key,oversized,ready));assert(!pending);
 fail_task=true;assert(!file_detail_loader_start("printer.local",7125,expected_key,file,ready));
 puts("PASS: full-length API key and URL encoding; cancelled and changed-printer results discarded; overlength inputs and task failure rejected");return 0;
}
