/* Bounded process-local NVS adapter. Real preference controllers and event
 * history are compiled from main/. Nothing here reads or writes user files. */
#include <stdio.h>
#include <string.h>
#include "nvs.h"
#define NS_MAX 12
#define KEY_MAX 128
typedef struct {char key[24],value[512];bool present,blob;size_t blob_size;} entry_t;
typedef struct {char name[24];entry_t items[KEY_MAX];} namespace_t;
static namespace_t stored[NS_MAX],pending[NS_MAX],plain[NS_MAX],encrypted[NS_MAX];
static bool plain_exists,encrypted_exists,storage_fail;
static char encrypted_secret[97];
static bool factory_erased;
bool host_storage_erased(void){return factory_erased;}
void host_storage_fail(bool fail){storage_fail=fail;}
esp_err_t nvs_open(const char *name,int mode,nvs_handle_t *out){
 (void)mode;for(unsigned i=0;i<NS_MAX;i++)if(!stored[i].name[0]||!strcmp(stored[i].name,name)){
  if(!stored[i].name[0])snprintf(stored[i].name,sizeof(stored[i].name),"%s",name);
  pending[i]=stored[i];*out=i+1;return ESP_OK;
 }return ESP_FAIL;
}
static entry_t *entry(nvs_handle_t handle,const char *key,bool create){
 if(!handle||handle>NS_MAX)return NULL;
 namespace_t *space=&pending[handle-1];
 for(unsigned i=0;i<KEY_MAX;i++)if(space->items[i].present&&!strcmp(space->items[i].key,key))return &space->items[i];
 if(create)for(unsigned i=0;i<KEY_MAX;i++)if(!space->items[i].present){entry_t *item=&space->items[i];*item=(entry_t){.present=true};snprintf(item->key,sizeof(item->key),"%s",key);return item;}
 return NULL;
}
esp_err_t nvs_get_u8(nvs_handle_t handle,const char *key,uint8_t *out){entry_t *item=entry(handle,key,false);if(!item)return ESP_ERR_NVS_NOT_FOUND;unsigned value;if(sscanf(item->value,"%u",&value)!=1||value>255)return ESP_FAIL;*out=value;return ESP_OK;}
esp_err_t nvs_set_u8(nvs_handle_t handle,const char *key,uint8_t value){entry_t *item=entry(handle,key,true);if(!item)return ESP_FAIL;snprintf(item->value,sizeof(item->value),"%u",value);return ESP_OK;}
esp_err_t nvs_get_str(nvs_handle_t handle,const char *key,char *out,size_t *size){entry_t *item=entry(handle,key,false);if(!item)return ESP_ERR_NVS_NOT_FOUND;if(item->blob)return ESP_FAIL;size_t needed=strlen(item->value)+1;if(out&&*size<needed){*size=needed;return ESP_FAIL;}if(out)memcpy(out,item->value,needed);*size=needed;return ESP_OK;}
esp_err_t nvs_set_str(nvs_handle_t handle,const char *key,const char *value){entry_t *item=entry(handle,key,true);if(!item||strlen(value)>=sizeof(item->value))return ESP_FAIL;snprintf(item->value,sizeof(item->value),"%s",value);return ESP_OK;}
esp_err_t nvs_erase_key(nvs_handle_t handle,const char *key){entry_t *item=entry(handle,key,false);if(!item)return ESP_ERR_NVS_NOT_FOUND;memset(item,0,sizeof(*item));return ESP_OK;}
esp_err_t nvs_commit(nvs_handle_t handle){if(storage_fail||!handle||handle>NS_MAX)return ESP_FAIL;stored[handle-1]=pending[handle-1];return ESP_OK;}
void nvs_close(nvs_handle_t handle){(void)handle;}
esp_err_t nvs_flash_erase(void){if(storage_fail)return ESP_FAIL;memset(stored,0,sizeof(stored));memset(pending,0,sizeof(pending));factory_erased=true;return ESP_OK;}
bool host_backup_save(bool secret,const char *password){if(storage_fail)return false;if(secret){memcpy(encrypted,stored,sizeof(stored));snprintf(encrypted_secret,sizeof(encrypted_secret),"%s",password);encrypted_exists=true;}else{memcpy(plain,stored,sizeof(stored));
 for(unsigned n=0;n<NS_MAX;n++)if(!strcmp(plain[n].name,"netcfg"))for(unsigned i=0;i<KEY_MAX;i++){
  int profile;char tail;if(sscanf(plain[n].items[i].key,"p%d_key%c",&profile,&tail)==1&&profile>=0&&profile<4)memset(&plain[n].items[i],0,sizeof(entry_t));
 }
 plain_exists=true;}return true;}
bool host_backup_exists(bool secret,const char *password){return secret?encrypted_exists&&password&&!strcmp(password,encrypted_secret):plain_exists;}
bool host_backup_restore(bool secret,const char *password){if(storage_fail||!host_backup_exists(secret,password))return false;memcpy(stored,secret?encrypted:plain,sizeof(stored));memcpy(pending,stored,sizeof(stored));return true;}
void host_backup_remove(void){memset(plain,0,sizeof(plain));memset(encrypted,0,sizeof(encrypted));memset(encrypted_secret,0,sizeof(encrypted_secret));plain_exists=encrypted_exists=false;}

esp_err_t nvs_get_i32(nvs_handle_t handle,const char *key,int32_t *out){entry_t *item=entry(handle,key,false);if(!item)return ESP_ERR_NVS_NOT_FOUND;int value;if(sscanf(item->value,"%d",&value)!=1)return ESP_FAIL;*out=value;return ESP_OK;}
esp_err_t nvs_set_i32(nvs_handle_t handle,const char *key,int32_t value){entry_t *item=entry(handle,key,true);if(!item)return ESP_FAIL;snprintf(item->value,sizeof(item->value),"%d",value);return ESP_OK;}

esp_err_t nvs_get_blob(nvs_handle_t handle,const char *key,void *out,size_t *size){entry_t *item=entry(handle,key,false);if(!item)return ESP_ERR_NVS_NOT_FOUND;if(!item->blob)return ESP_FAIL;size_t needed=item->blob_size;if(out&&*size<needed){*size=needed;return ESP_FAIL;}if(out)memcpy(out,item->value,needed);*size=needed;return ESP_OK;}
esp_err_t nvs_set_blob(nvs_handle_t handle,const char *key,const void *value,size_t size){entry_t *item=entry(handle,key,true);if(!item||size>sizeof(item->value))return ESP_FAIL;memcpy(item->value,value,size);item->blob=true;item->blob_size=size;return ESP_OK;}
