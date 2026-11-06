#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "moonraker_config_controller.h"
#include "nvs.h"
#include "esp_err.h"

/* Model successful NVS setters as immediately visible, without assuming
 * close() rolls back writes or commit() makes multiple keys atomic. */
static struct entry {char key[16], text[256];int32_t number;int type;} entries[64];
static unsigned writes, erases, commits, opens, closes;
static const char *read_failure, *write_failure;
static int commit_failure;
static struct entry *lookup(const char *key,bool create)
{
    for(unsigned i=0;i<64;i++)if(entries[i].type&&!strcmp(entries[i].key,key))return &entries[i];
    if(create)for(unsigned i=0;i<64;i++)if(!entries[i].type){snprintf(entries[i].key,16,"%s",key);return &entries[i];}
    return NULL;
}
size_t strlcpy(char *out,const char *in,size_t size){size_t n=strlen(in);if(size)snprintf(out,size,"%s",in);return n;}
const char *esp_err_to_name(int e){(void)e;return "injected";}
int nvs_open(const char *space,int mode,nvs_handle_t *h){assert(!strcmp(space,"netcfg")&&mode==NVS_READWRITE);opens++;*h=1;return ESP_OK;}
void nvs_close(nvs_handle_t h){assert(h==1);closes++;}
static int get(const char *key,int type,struct entry **e){if(read_failure&&!strcmp(key,read_failure))return -9;*e=lookup(key,false);return !*e?ESP_ERR_NVS_NOT_FOUND:(*e)->type==type?ESP_OK:-8;}
int nvs_get_u8(nvs_handle_t h,const char *key,uint8_t *out){assert(h==1);struct entry *e;int r=get(key,1,&e);if(!r)*out=(uint8_t)e->number;return r;}
int nvs_get_i32(nvs_handle_t h,const char *key,int32_t *out){assert(h==1);struct entry *e;int r=get(key,2,&e);if(!r)*out=e->number;return r;}
int nvs_get_str(nvs_handle_t h,const char *key,char *out,size_t *size){assert(h==1);struct entry *e;int r=get(key,3,&e);if(r)return r;size_t n=strlen(e->text)+1;if(n>*size)return -7;memcpy(out,e->text,n);*size=n;return ESP_OK;}
static int set(const char *key,int type,int32_t number,const char *text){writes++;if(write_failure&&!strcmp(write_failure,key))return -9;struct entry *e=lookup(key,true);assert(e);e->type=type;e->number=number;snprintf(e->text,sizeof(e->text),"%s",text?text:"");return ESP_OK;}
int nvs_set_u8(nvs_handle_t h,const char *key,uint8_t value){assert(h==1);return set(key,1,value,NULL);}
int nvs_set_i32(nvs_handle_t h,const char *key,int32_t value){assert(h==1);return set(key,2,value,NULL);}
int nvs_set_str(nvs_handle_t h,const char *key,const char *text){assert(h==1);return set(key,3,0,text);}
int nvs_erase_key(nvs_handle_t h,const char *key){assert(h==1);erases++;struct entry *e=lookup(key,false);if(!e)return ESP_ERR_NVS_NOT_FOUND;memset(e,0,sizeof(*e));return ESP_OK;}
int nvs_commit(nvs_handle_t h){assert(h==1);commits++;return commit_failure;}
static void seed(void){
    memset(entries,0,sizeof(entries));read_failure=write_failure=NULL;commit_failure=0;
    set("mp_schema",1,1,NULL);set("mp_active",1,0,NULL);set("p0_used",1,1,NULL);
    set("p0_name",3,0,"Printer");set("p0_host",3,0,"printer.local");set("p0_port",2,7125,NULL);
    set("p0_key",3,0,"secret-token");set("p0_cam",3,0,"http://camera.local/stream");set("p0_tls",1,1,NULL);
    writes=erases=commits=opens=closes=0;
}
int main(void){
    seed();assert(moonraker_config_load());assert(!writes&&!erases&&!commits&&opens==closes);
    moonraker_profile_t previous=*moonraker_config_profile(0);uint32_t generation=moonraker_config_generation();
    unsigned unchanged_writes=writes,unchanged_opens=opens;
    assert(moonraker_config_save_profile(0,previous.name,previous.host,previous.port,previous.api_key));
    assert(moonraker_config_set_camera_stream_url(0,previous.camera_stream_url));
    assert(writes==unchanged_writes&&opens==unchanged_opens&&moonraker_config_generation()==generation);
    write_failure="p0_used";
    assert(!moonraker_config_save_profile(0,"Renamed","printer.local",7125,"new-token"));
    assert(!erases&&!commits&&!strcmp(lookup("p0_key",false)->text,"secret-token"));
    assert(!memcmp(&previous,moonraker_config_profile(0),sizeof(previous))&&moonraker_config_generation()==generation);
    write_failure=NULL;assert(moonraker_config_save_profile(0,"Renamed","printer.local",7125,"new-token"));
    assert(!strcmp(moonraker_config_camera_stream_url(0),previous.camera_stream_url));
    assert(moonraker_config_profile(0)->secure_transport&&moonraker_config_generation()!=generation);
    assert(!strcmp(lookup("p0_cam",false)->text,previous.camera_stream_url));
    previous=*moonraker_config_profile(0);generation=moonraker_config_generation();commit_failure=-9;
    assert(!moonraker_config_save_profile(0,"Failed","printer.local",7125,"third-token"));
    assert(!memcmp(&previous,moonraker_config_profile(0),sizeof(previous))&&moonraker_config_generation()==generation);
    seed();read_failure="mp_schema";assert(!moonraker_config_load());assert(!writes&&!erases&&!commits&&opens==closes);assert(!strcmp(lookup("p0_key",false)->text,"secret-token"));
    seed();lookup("mp_schema",false)->number=2;assert(!moonraker_config_load());assert(!writes&&!erases&&!commits);
    seed();lookup("mp_schema",false)->type=3;assert(!moonraker_config_load());assert(!writes&&!erases&&!commits);
    seed();snprintf(lookup("p0_host",false)->text,256,"bad/host");assert(!moonraker_config_load());assert(!writes&&!erases&&!commits);
    seed();read_failure="p0_tls";assert(moonraker_config_load()&&moonraker_config_profile(0)->secure_transport);assert(!writes);
    seed();lookup("p0_tls",false)->number=2;assert(moonraker_config_load()&&moonraker_config_profile(0)->secure_transport);
    seed();lookup("p0_tls",false)->type=3;assert(moonraker_config_load()&&moonraker_config_profile(0)->secure_transport);
    seed();lookup("p0_tls",false)->number=0;assert(moonraker_config_load()&&!moonraker_config_profile(0)->secure_transport);
    seed();lookup("p0_tls",false)->type=0;assert(moonraker_config_load()&&!moonraker_config_profile(0)->secure_transport);
    seed();lookup("mp_schema",false)->type=0;set("moon_host",3,0,"legacy.local");set("moon_port",2,7126,NULL);writes=0;
    assert(moonraker_config_load()&&!strcmp(moonraker_config_host(),"legacy.local")&&moonraker_config_port()==7126&&writes&&commits==1);
    assert(opens==closes);
    puts("PASS: profile write failure preserves API key and RAM; camera edits, schema errors, legacy migration, TLS failures and generation ownership checked");
}
