/* Storage/log adapters only. Notification layout, queue and dismiss actions
 * are compiled unchanged from the firmware's ui_toast.c. */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "nvs.h"
#include "operator_event_log.h"
#include "console_controller.h"
static uint8_t level;
esp_err_t nvs_open(const char *name,int mode,nvs_handle_t *out){(void)name;(void)mode;*out=1;return ESP_OK;}
esp_err_t nvs_get_u8(nvs_handle_t handle,const char *key,uint8_t *out){(void)handle;if(strcmp(key,"level"))return ESP_ERR_NVS_NOT_FOUND;*out=level;return ESP_OK;}
esp_err_t nvs_set_u8(nvs_handle_t handle,const char *key,uint8_t value){(void)handle;if(strcmp(key,"level"))return ESP_ERR_NVS_NOT_FOUND;level=value;return ESP_OK;}
esp_err_t nvs_commit(nvs_handle_t handle){(void)handle;return ESP_OK;}
void nvs_close(nvs_handle_t handle){(void)handle;}
void operator_event_log_add(operator_event_level_t severity,const char *format,...){
 (void)severity;char message[512];va_list args;va_start(args,format);vsnprintf(message,sizeof(message),format,args);va_end(args);console_controller_add(CONSOLE_ENTRY_RESPONSE,message);
}
