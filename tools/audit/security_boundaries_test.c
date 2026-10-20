#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <strings.h>
#include "nvs.h"
#include "moonraker_transport_security_controller.h"
#include "moonraker_tls_trust_store.h"

static moonraker_profile_t profiles[MOONRAKER_CONFIG_MAX_PROFILES];
static int active;
const moonraker_profile_t *moonraker_config_profile(int i){return i>=0&&i<4?&profiles[i]:NULL;}
int moonraker_config_active_profile_index(void){return active;}
static int open_error,set_error,commit_error,writes;
static char stored[4][4096],pending[4096];
static int pending_index;
int nvs_open(const char *n,int mode,int *h){(void)n;(void)mode;*h=1;return open_error;}
int nvs_get_str(int h,const char *k,char *out,size_t *n){(void)h;int index=-1;assert(sscanf(k,"ca_pem_%d",&index)==1&&index>=0&&index<4);strlcpy(out,stored[index],*n);return 0;}
int nvs_set_str(int h,const char *k,const char *s){(void)h;assert(sscanf(k,"ca_pem_%d",&pending_index)==1);writes++;strlcpy(pending,s,sizeof(pending));return set_error;}
int nvs_commit(int h){(void)h;if(!commit_error)strcpy(stored[pending_index],pending);return commit_error;}
void nvs_close(int h){(void)h;}
#include "validators.c"
#include "size.c"

typedef struct cJSON {struct cJSON *next,*prev,*child;int type;char *valuestring;int valueint;double valuedouble;char *string;} cJSON;
extern cJSON *cJSON_CreateObject(void);
extern cJSON *cJSON_AddStringToObject(cJSON *,const char *,const char *);
extern int cJSON_PrintPreallocated(cJSON *,char *,int,int);
extern cJSON *cJSON_Parse(const char *);
extern cJSON *cJSON_GetObjectItemCaseSensitive(const cJSON *,const char *);
extern void cJSON_Delete(cJSON *);
extern cJSON *cJSON_AddArrayToObject(cJSON *,const char *);
extern cJSON *cJSON_AddObjectToObject(cJSON *,const char *);
extern cJSON *cJSON_AddNumberToObject(cJSON *,const char *,double);
extern cJSON *cJSON_AddBoolToObject(cJSON *,const char *,int);
extern int cJSON_AddItemToArray(cJSON *,cJSON *);
extern cJSON *cJSON_GetArrayItem(const cJSON *,int);
extern int cJSON_IsBool(const cJSON *);
extern int cJSON_IsTrue(const cJSON *);
#include "backup.c"
#define ESP_FAIL -1
#define ESP_ERR_INVALID_STATE -2
#define HTTP_METHOD_POST 1
typedef void *esp_http_client_handle_t;
typedef struct {const char *url,*cert_pem;int method,timeout_ms;
 void *event_handler,*user_data;bool disable_auto_redirect;} esp_http_client_config_t;
static void *moonraker_http_event_handler;
static int requests,initializations,status=200;
static bool shared=true,init_ok=true;
static char wire[300];
static void *esp_http_client_init(const esp_http_client_config_t *c){assert(c->url&&c->method==1&&c->disable_auto_redirect);initializations++;return init_ok?(void*)1:NULL;}
static int esp_http_client_set_header(void *c,const char *k,const char *v){assert(c&&k&&v);return 0;}
static int esp_http_client_set_post_field(void *c,const char *b,int n){assert(c&&n<300);memcpy(wire,b,n);wire[n]=0;return 0;}
static bool network_activity_controller_try_begin_shared(void){return shared;}
static void network_activity_controller_end_shared(void){}
static int esp_http_client_perform(void *c){assert(c);requests++;return 0;}
static int esp_http_client_get_status_code(void *c){assert(c);return status;}
static int esp_http_client_cleanup(void *c){assert(c);return 0;}
#include "print.c"

static const char *old_ca="-----BEGIN CERTIFICATE-----\nold\n-----END CERTIFICATE-----\n";
static const char *new_ca="-----BEGIN CERTIFICATE-----\nnew\n-----END CERTIFICATE-----\n";
static void preserved(void){assert(!strcmp(moonraker_tls_trust_store_pem_for_profile(0),old_ca));}
static void file_bytes(const char *path,const char *data,size_t size){FILE *f=fopen(path,"wb");assert(f);assert(fwrite(data,1,size,f)==size);assert(!fclose(f));}
int main(int argc,char **argv){
 assert(argc==2);const char *file=argv[1];
 assert(moonraker_tls_trust_store_set_pem_for_profile(0,old_ca));preserved();
 for(int mode=0;mode<3;mode++){
  open_error=mode==0;set_error=mode==1;commit_error=mode==2;
  assert(!moonraker_tls_trust_store_set_pem_for_profile(0,new_ca));preserved();
  open_error=set_error=commit_error=0;
 }
 file_bytes(file,"garbage",7);int before=writes;
 assert(!moonraker_tls_trust_store_import_file_for_profile(0,file));preserved();assert(writes==before);
 char big[4096];memset(big,'x',sizeof(big));file_bytes(file,big,sizeof(big));
 assert(!moonraker_tls_trust_store_import_file_for_profile(0,file));preserved();
 char nul[128];size_t len=strlen(new_ca);memcpy(nul,new_ca,len+1);memcpy(nul+len+1,"hidden",6);
 file_bytes(file,nul,len+7);assert(!moonraker_tls_trust_store_import_file_for_profile(0,file));preserved();
 file_bytes(file,new_ca,strlen(new_ca));commit_error=1;
 assert(!moonraker_tls_trust_store_import_file_for_profile(0,file));preserved();commit_error=0;
 assert(moonraker_tls_trust_store_import_file_for_profile(0,file));
 assert(!strcmp(moonraker_tls_trust_store_pem_for_profile(0),new_ca));
 assert(!moonraker_tls_trust_store_import_file_for_profile(0,"/no/such/file"));
 assert(!moonraker_tls_trust_store_set_pem_for_profile(-1,new_ca));
 profiles[0]=(moonraker_profile_t){.configured=true,.port=7125};strcpy(profiles[0].host,"printer.local");strcpy(profiles[0].name,"Printer");
 profiles[1]=profiles[0];profiles[1].secure_transport=true;
 char url[256];const char *ca=(void*)1;
 /* A secure duplicate without a CA fails closed even with plaintext first. */
 assert(!moonraker_transport_security_build_http_url_for_endpoint("printer.local",7125,"/server/info",url,sizeof(url),&ca)&&!ca);
 assert(moonraker_tls_trust_store_set_pem_for_profile(1,new_ca));
 assert(moonraker_transport_security_build_http_url_for_endpoint("PRINTER.LOCAL",7125,"/server/info",url,sizeof(url),&ca)&&ca&&!strncmp(url,"https://",8));
 for(active=0;active<2;active++){
  assert(moonraker_transport_security_build_http_url_for_endpoint("printer.local",7125,"/server/info",url,sizeof(url),&ca));assert(!strcmp(url,"https://printer.local:7125/server/info")&&ca);
  assert(moonraker_transport_security_build_websocket_uri_for_endpoint("printer.local",7125,url,sizeof(url),&ca));assert(!strcmp(url,"wss://printer.local:7125/websocket")&&ca);
 }
 profiles[1].configured=false;
 assert(moonraker_transport_security_build_http_url_for_endpoint("printer.local",7125,"/x?q=one%20two",url,sizeof(url),&ca)&&!ca);
 const char *bad_hosts[]={NULL,"","host@evil","host/path","host\r\nX: v","-host","host.","https://host"};
 for(unsigned i=0;i<sizeof(bad_hosts)/sizeof(*bad_hosts);i++){
  ca=(void*)1;assert(!moonraker_transport_security_build_http_url_for_endpoint(bad_hosts[i],7125,"/x",url,sizeof(url),&ca)&&!ca);
  assert(!moonraker_transport_security_build_websocket_uri_for_endpoint(bad_hosts[i],7125,url,sizeof(url),&ca));
 }
 assert(!moonraker_transport_security_build_http_url_for_endpoint("p",65536,"/x",url,sizeof(url),&ca));
 assert(!moonraker_transport_security_build_http_url_for_endpoint("p",0,"/x",url,sizeof(url),&ca));
 const char *bad_paths[]={NULL,"x","/x\r\nX: y","/x#fragment","/x y"};
 for(unsigned i=0;i<sizeof(bad_paths)/sizeof(*bad_paths);i++)assert(!moonraker_transport_security_build_http_url_for_endpoint("p",7125,bad_paths[i],url,sizeof(url),&ca));
 assert(!moonraker_transport_security_build_http_url_for_endpoint("p",7125,"/x",NULL,10,&ca));
 assert(!moonraker_transport_security_build_http_url_for_endpoint("p",7125,"/x",url,0,&ca));
 assert(!moonraker_transport_security_build_http_url_for_endpoint("p",7125,"/x",url,4,&ca));
 assert(valid_api_key("abc-_.123")&&!valid_api_key("key\r\nInjected: yes"));
 assert(profile_valid(&profiles[0]));strcpy(profiles[0].api_key,"key\r\nInjected: yes");assert(!profile_valid(&profiles[0]));
 strcpy(profiles[0].api_key,"abc-_.123");assert(profile_valid(&profiles[0]));
 memset(profiles[0].name,'n',sizeof(profiles[0].name));assert(!profile_valid(&profiles[0]));strcpy(profiles[0].name,"Printer");
 settings_backup_snapshot_t snapshot={0};snapshot.profiles[0]=profiles[0];
 for(int include_keys=0;include_keys<2;include_keys++)for(int tls=0;tls<2;tls++){
  snapshot.profiles[0].secure_transport=tls;
  cJSON *j=snapshot_to_json(&snapshot,include_keys);assert(j);
  cJSON *item=cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(j,"profiles"),0);assert(item);
  moonraker_profile_t restored=profiles[0];restored.secure_transport=!tls;
  assert(parse_profile_transport_security(item,&restored)&&restored.secure_transport==tls);
  assert((cJSON_GetObjectItemCaseSensitive(item,"api_key")!=NULL)==include_keys);cJSON_Delete(j);
 }
 profiles[1]=profiles[0];profiles[1].secure_transport=true;
 cJSON *legacy=cJSON_Parse("{}");moonraker_profile_t restored=profiles[0];
 strcpy(restored.host,"PRINTER.LOCAL");
 assert(parse_profile_transport_security(legacy,&restored)&&restored.secure_transport);
 strcpy(restored.host,"other.local");assert(parse_profile_transport_security(legacy,&restored)&&!restored.secure_transport);cJSON_Delete(legacy);
 const char *invalid_tls[]={"{\"secure_transport\":1}","{\"secure_transport\":null}","{\"secure_transport\":\"true\"}"};
 for(unsigned i=0;i<3;i++){cJSON *j=cJSON_Parse(invalid_tls[i]);assert(!parse_profile_transport_security(j,&restored));cJSON_Delete(j);}
 const char *names[]={"models/test.gcode","quote\".gcode","back\\slash.gcode","line\nfeed\t.gcode","a\",\"filename\":\"other.gcode","unicode-\xc3\xa9.gcode"};
 for(unsigned i=0;i<sizeof(names)/sizeof(*names);i++){
  assert(moonraker_start_print_file("p",7125,"key",names[i],NULL,NULL));
  cJSON *j=cJSON_Parse(wire);assert(j);cJSON *name=cJSON_GetObjectItemCaseSensitive(j,"filename");assert(name&&!strcmp(name->valuestring,names[i])&&!name->next);cJSON_Delete(j);
 }
 before=initializations;char long_name[400];memset(long_name,'a',399);long_name[399]=0;
 assert(!moonraker_start_print_file("p",7125,NULL,long_name,NULL,NULL)&&initializations==before);
 before=requests;shared=false;assert(!moonraker_start_print_file("p",7125,NULL,"test",NULL,NULL)&&requests==before);shared=true;
 status=302;assert(!moonraker_start_print_file("p",7125,NULL,"test",NULL,NULL));
 status=401;assert(!moonraker_start_print_file("p",7125,NULL,"test",NULL,NULL));status=200;
 init_ok=false;assert(!moonraker_start_print_file("p",7125,NULL,"test",NULL,NULL));
 uint8_t byte=0,*out=NULL;size_t out_size=0;char message[100];
 assert(size_guard(&byte,1,&out,&out_size,message,sizeof(message)));
 assert(size_guard(&byte,(size_t)UINT32_MAX-57,&out,&out_size,message,sizeof(message)));
 assert(!size_guard(&byte,(size_t)UINT32_MAX-56,&out,&out_size,message,sizeof(message)));
 assert(!size_guard(&byte,UINT32_MAX,&out,&out_size,message,sizeof(message)));
 puts("PASS: TLS import rollback, duplicate-endpoint TLS, URL validation, restore key validation, plain/encrypted backup TLS round trips and legacy migration, real JSON filename round trips, HTTP failures, 32-bit encrypted-size guards (UBSan)");
 return 0;
}
