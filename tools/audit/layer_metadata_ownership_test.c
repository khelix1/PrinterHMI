#include "thumbnail_session.c"
#include "encoder.inc"
#include <assert.h>
#include <stdlib.h>
static uint32_t generation=1;
static bool fail_fetch,change_owner;
static char requested[512];
uint32_t moonraker_config_generation(void){return generation;}
bool moonraker_fetch_file_metadata(const char *host,int port,const char *key,const char *file,char *body,size_t size,int *code,esp_err_t *err){
 (void)host;(void)port;(void)key;snprintf(requested,sizeof(requested),"%s",file);
 snprintf(body,size,"{\"result\":{\"object_height\":20,\"layer_height\":0.2}}");*code=200;*err=ESP_OK;
 if(change_owner)generation++;
 return !fail_fetch;
}
bool printer_files_build_metadata_text(const char *file,const char *json,char *path,size_t path_size,char *out,size_t size){
 (void)file;(void)json;snprintf(path,path_size,"preview.png");snprintf(out,size,"metadata");return true;
}
int main(void){
 char out[256],name[160];
 assert(thumbnail_session_build_metadata("host",7125,"","first.gcode",out,sizeof(out)));
 assert(!strcmp(s_selected_thumbnail_path,"preview.png"));
 generation++;
 assert(thumbnail_session_build_metadata("host",7125,"","first.gcode",out,sizeof(out)));
 fail_fetch=true;assert(!thumbnail_session_build_metadata("host",7125,"","second.gcode",out,sizeof(out)));
 assert(!s_selected_thumbnail_path[0]);fail_fetch=false;
 change_owner=true;assert(!thumbnail_session_build_metadata("host",7125,"","first.gcode",out,sizeof(out)));
 assert(!s_selected_thumbnail_path[0]);change_owner=false;
 memset(name,' ',150);strcpy(name+150,".gcode");assert(thumbnail_session_build_metadata("host",7125,"",name,out,sizeof(out)));
 assert(strlen(requested)==456);for(int i=0;i<150;i++)assert(!memcmp(requested+i*3,"%20",3));assert(!strcmp(requested+450,".gcode"));
 puts("PASS: thumbnail metadata request generation/failure ownership and long filenames; retired thumbnail layer getter absent");
 return 0;
}
