#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "moonraker_config_controller.h"
#include "nvs.h"
#include "esp_err.h"
#ifdef LEGACY_PROFILE_SOURCE
bool moonraker_config_storage_readable(void){return true;}
#endif

/* Model successful NVS setters as immediately visible, without assuming
 * close() rolls back writes or commit() makes multiple keys atomic. */
static struct entry {char key[16], text[256];int32_t number;int type;} entries[64];
static unsigned writes, erases, commits, opens, closes;
static const char *read_failure, *write_failure, *malformed_key;
static int malformed_kind;
static int commit_failure, open_failure;
static struct entry *lookup(const char *key,bool create)
{
    for(unsigned i=0;i<64;i++)if(entries[i].type&&!strcmp(entries[i].key,key))return &entries[i];
    if(create)for(unsigned i=0;i<64;i++)if(!entries[i].type){snprintf(entries[i].key,16,"%s",key);return &entries[i];}
    return NULL;
}
size_t strlcpy(char *out,const char *in,size_t size){size_t n=strlen(in);if(size)snprintf(out,size,"%s",in);return n;}
const char *esp_err_to_name(int e){(void)e;return "injected";}
int nvs_open(const char *space,int mode,nvs_handle_t *h){assert(!strcmp(space,"netcfg")&&mode==NVS_READWRITE);opens++;*h=1;return open_failure;}
void nvs_close(nvs_handle_t h){assert(h==1);closes++;}
static int get(const char *key,int type,struct entry **e){if(read_failure&&!strcmp(key,read_failure))return -9;*e=lookup(key,false);return !*e?ESP_ERR_NVS_NOT_FOUND:(*e)->type==type?ESP_OK:-8;}
int nvs_get_u8(nvs_handle_t h,const char *key,uint8_t *out){assert(h==1);struct entry *e;int r=get(key,1,&e);if(!r)*out=(uint8_t)e->number;return r;}
int nvs_get_i32(nvs_handle_t h,const char *key,int32_t *out){assert(h==1);struct entry *e;int r=get(key,2,&e);if(!r)*out=e->number;return r;}
int nvs_get_str(nvs_handle_t h,const char *key,char *out,size_t *size){assert(h==1);struct entry *e;int r=get(key,3,&e);if(r)return r;size_t n=strlen(e->text)+1;if(n>*size)return -7;size_t capacity=*size;memcpy(out,e->text,n);*size=n;
if(malformed_key&&!strcmp(key,malformed_key)){
 if(malformed_kind==1)*size=0;
 if(malformed_kind==2)*size=capacity+1;
 if(malformed_kind==3){memset(out,'x',capacity);*size=capacity;}
 if(malformed_kind==4){out[n]='x';out[n+1]=0;*size=n+2;}
}return ESP_OK;}
static int set(const char *key,int type,int32_t number,const char *text){writes++;if(write_failure&&!strcmp(write_failure,key))return -9;struct entry *e=lookup(key,true);assert(e);e->type=type;e->number=number;snprintf(e->text,sizeof(e->text),"%s",text?text:"");return ESP_OK;}
int nvs_set_u8(nvs_handle_t h,const char *key,uint8_t value){assert(h==1);return set(key,1,value,NULL);}
int nvs_set_i32(nvs_handle_t h,const char *key,int32_t value){assert(h==1);return set(key,2,value,NULL);}
int nvs_set_str(nvs_handle_t h,const char *key,const char *text){assert(h==1);return set(key,3,0,text);}
int nvs_erase_key(nvs_handle_t h,const char *key){assert(h==1);erases++;if(write_failure&&!strcmp(write_failure,key))return -9;struct entry *e=lookup(key,false);if(!e)return ESP_ERR_NVS_NOT_FOUND;memset(e,0,sizeof(*e));return ESP_OK;}
int nvs_commit(nvs_handle_t h){assert(h==1);commits++;return commit_failure;}
static void seed(void){
    memset(entries,0,sizeof(entries));read_failure=write_failure=malformed_key=NULL;commit_failure=malformed_kind=open_failure=0;
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
    /* Camera edits require an existing profile, including empty no-ops. */
    for(int slot=1;slot<MOONRAKER_CONFIG_MAX_PROFILES;slot++){
        moonraker_profile_t blank=*moonraker_config_profile(slot);
        assert(!moonraker_config_set_camera_stream_url(slot,"http://orphan.local/stream"));
        assert(!moonraker_config_set_camera_stream_url(slot,""));
        assert(!memcmp(&blank,moonraker_config_profile(slot),sizeof(blank)));
    }
    assert(writes==unchanged_writes&&opens==unchanged_opens&&moonraker_config_generation()==generation);
    /* Never report a successful save for a silently truncated printer name. */
    char too_long[MOONRAKER_CONFIG_NAME_LENGTH+1];memset(too_long,'N',sizeof(too_long));too_long[sizeof(too_long)-1]=0;
    assert(!moonraker_config_save_profile(0,too_long,previous.host,previous.port,previous.api_key));
    too_long[sizeof(too_long)-1]='N';
    assert(!moonraker_config_save_profile(1,too_long,"other.local",7126,""));
    assert(writes==unchanged_writes&&opens==unchanged_opens&&moonraker_config_generation()==generation);
    assert(!memcmp(&previous,moonraker_config_profile(0),sizeof(previous)));
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
    seed();read_failure="p0_key";(void)moonraker_config_load();read_failure=NULL;
    (void)moonraker_config_save_profile(1,"Other","other.local",7126,"new-token");
    assert(!strcmp(lookup("p0_key",false)->text,"secret-token"));
    seed();lookup("p0_port",false)->number=7126;read_failure="p0_port";
    (void)moonraker_config_load();read_failure=NULL;
    (void)moonraker_config_save_profile(1,"Other","other.local",7126,"");
    assert(lookup("p0_port",false)->number==7126);
    seed();read_failure="p0_tls";assert(!moonraker_config_load()&&moonraker_config_profile(0)->secure_transport);assert(!writes);
    seed();lookup("p0_tls",false)->number=2;assert(!moonraker_config_load()&&moonraker_config_profile(0)->secure_transport);
    seed();lookup("p0_tls",false)->type=3;assert(!moonraker_config_load()&&moonraker_config_profile(0)->secure_transport);
    seed();lookup("p0_tls",false)->number=0;assert(moonraker_config_load()&&!moonraker_config_profile(0)->secure_transport);
    seed();lookup("p0_tls",false)->type=0;assert(moonraker_config_load()&&!moonraker_config_profile(0)->secure_transport);
    seed();open_failure=-9;assert(!moonraker_config_load()&&!moonraker_config_storage_readable());
    assert(!moonraker_config_profile_count()&&!moonraker_config_host()[0]&&!writes&&!erases&&!closes);
    /* A failed optional read must not become an empty credential on a later save. */
    const char *failed_keys[]={"p0_used","p0_name","p0_host","p0_port","p0_key","p0_cam","p0_tls","mp_active","p1_used","mp_schema"};
    for(unsigned i=0;i<sizeof(failed_keys)/sizeof(*failed_keys);i++){
        seed();read_failure=failed_keys[i];assert(!moonraker_config_load());
        assert(!moonraker_config_storage_readable()&&!writes&&!erases&&!commits&&opens==closes);
        if(strcmp(failed_keys[i],"p1_used") && strcmp(failed_keys[i],"p0_tls"))
            assert(!moonraker_config_profile_count()&&!moonraker_config_host()[0]);
        assert(!moonraker_config_save_profile(1,"Other","other.local",7126,"new-token"));
        assert(!moonraker_config_set_transport_security(0,false));
        assert(!moonraker_config_set_camera_stream_url(0,"http://new.local/stream"));
        assert(!moonraker_config_delete_profile(0));
        assert(!moonraker_config_select_profile(0));
        assert(!writes&&!erases&&!commits);
        assert(!strcmp(lookup("p0_key",false)->text,"secret-token"));
        assert(!strcmp(lookup("p0_cam",false)->text,"http://camera.local/stream"));
        read_failure=NULL;assert(moonraker_config_load()&&moonraker_config_storage_readable());
        assert(!strcmp(moonraker_config_profile(0)->api_key,"secret-token"));
    }
    const char *text_keys[]={"p0_name","p0_host","p0_key","p0_cam"};
    for(unsigned i=0;i<sizeof(text_keys)/sizeof(*text_keys);i++)for(int kind=1;kind<=4;kind++){
        seed();malformed_key=text_keys[i];malformed_kind=kind;
        assert(!moonraker_config_load()&&!moonraker_config_storage_readable());
        assert(!moonraker_config_save_profile(1,"Other","other.local",7126,""));
        assert(!writes&&!erases&&!commits&&opens==closes);
        malformed_key=NULL;assert(moonraker_config_load()&&moonraker_config_storage_readable());
    }
    /* Missing legacy fields remain compatible; invalid existing data does not. */
    seed();lookup("p0_key",false)->type=0;lookup("p0_cam",false)->type=0;lookup("p0_port",false)->type=0;
    assert(moonraker_config_load()&&moonraker_config_storage_readable());
    assert(!moonraker_config_api_key()[0]&&!moonraker_config_camera_stream_url(0)[0]&&moonraker_config_port()==7125);
    seed();lookup("mp_active",false)->number=99;assert(!moonraker_config_load()&&!moonraker_config_profile_count()&&!moonraker_config_host()[0]);
    seed();lookup("mp_active",false)->number=1;assert(!moonraker_config_load()&&!moonraker_config_profile_count());
    seed();lookup("p0_port",false)->number=0;assert(!moonraker_config_load()&&!moonraker_config_storage_readable());
    seed();strcpy(lookup("p0_key",false)->text,"bad\nkey");assert(!moonraker_config_load()&&!moonraker_config_storage_readable());
    seed();lookup("mp_schema",false)->type=0;read_failure="moon_host";assert(!moonraker_config_load()&&!writes&&!commits);
    seed();lookup("mp_schema",false)->type=0;read_failure="moon_port";assert(!moonraker_config_load()&&!writes&&!commits);
    /* A complete explicit replacement can recover the store; failure keeps protection. */
    seed();assert(moonraker_config_load());moonraker_profile_t recovered[MOONRAKER_CONFIG_MAX_PROFILES];
    for(int i=0;i<MOONRAKER_CONFIG_MAX_PROFILES;i++)recovered[i]=*moonraker_config_profile(i);
    read_failure="p0_key";assert(!moonraker_config_load());read_failure=NULL;
    commit_failure=-9;assert(!moonraker_config_replace_profiles(recovered,MOONRAKER_CONFIG_MAX_PROFILES,0));
    assert(!moonraker_config_storage_readable());commit_failure=0;
    assert(moonraker_config_replace_profiles(recovered,MOONRAKER_CONFIG_MAX_PROFILES,0));
    assert(moonraker_config_storage_readable()&&!strcmp(moonraker_config_api_key(),"secret-token"));
    seed();memset(entries,0,sizeof(entries));
    assert(moonraker_config_load()&&moonraker_config_storage_readable()&&moonraker_config_profile_count()==1);
    assert(!strcmp(moonraker_config_host(),"moonraker.local")&&writes&&commits==1&&opens==closes);
    seed();lookup("mp_schema",false)->type=0;set("moon_host",3,0,"legacy.local");set("moon_port",2,7126,NULL);writes=0;
    assert(moonraker_config_load()&&!strcmp(moonraker_config_host(),"legacy.local")&&moonraker_config_port()==7126&&writes&&commits==1);
    assert(opens==closes);
    seed();assert(moonraker_config_load());
    char exact[MOONRAKER_CONFIG_NAME_LENGTH];memset(exact,'P',sizeof(exact));exact[sizeof(exact)-1]=0;
    assert(moonraker_config_save_profile(1,exact,"other.local",7126,""));
    assert(!strcmp(moonraker_config_profile(1)->name,exact));
    assert(moonraker_config_set_camera_stream_url(1,"http://second.local/stream"));
    assert(moonraker_config_load()&&!strcmp(moonraker_config_profile(1)->name,exact));
    assert(!strcmp(moonraker_config_camera_stream_url(1),"http://second.local/stream"));
    assert(moonraker_config_save_profile(1,NULL,"other.local",7126,""));
    assert(moonraker_config_profile(1)->name[0]);
    /* Delete and reuse a real printer slot with camera metadata left in NVS. */
    seed();assert(moonraker_config_load());
    assert(moonraker_config_save_profile(1,"Old printer","old.local",7126,""));
    set("p1_c1",3,0,"http://old.local/camera");
    set("p1_n0",3,0,"Old camera");
    set("p1_n1",3,0,"Old second camera");
    set("p1_v0",1,5,NULL);
    set("p1_v1",1,9,NULL);
    set("p1_cdef",1,1,NULL);
    set("p0_c1",3,0,"http://keep.local/camera");
    assert(moonraker_config_delete_profile(1));
    unchanged_writes=writes;
    write_failure="p1_v0";
    assert(!moonraker_config_save_profile(1,"New printer","new.local",7127,""));
    assert(!moonraker_config_profile(1)->configured&&writes==unchanged_writes);
    write_failure=NULL;
    assert(moonraker_config_save_profile(1,"New printer","new.local",7127,""));
    assert(moonraker_config_load()&&!strcmp(moonraker_config_profile(1)->host,"new.local"));
    assert(!lookup("p1_c1",false)&&!lookup("p1_n0",false)&&!lookup("p1_n1",false));
    assert(!lookup("p1_v0",false)&&!lookup("p1_v1",false)&&!lookup("p1_cdef",false));
    assert(!strcmp(lookup("p0_c1",false)->text,"http://keep.local/camera"));
    set("p1_v0",1,2,NULL);
    assert(moonraker_config_save_profile(1,"Renamed printer","new.local",7127,""));
    assert(lookup("p1_v0",false)->number==2);
    puts("PASS: reused-slot camera cleanup and retry; unused-profile camera refusal, printer-name boundaries and round-trip; profile write failure preserves API key and RAM; camera edits, schema errors, legacy migration, TLS failures, incomplete-load write protection, legacy-read errors, recovery replacement and generation ownership checked");
}
