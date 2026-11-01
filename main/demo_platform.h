#pragma once
#include "esp_http_client.h"
#include "esp_websocket_client.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <stdbool.h>
bool demo_mode_init(void);
bool demo_mode_enabled(void);
/* Commit the boot selection only; caller restarts after success. */
bool demo_mode_select(bool enabled);
esp_err_t demo_nvs_open(const char *, nvs_open_mode_t, nvs_handle_t *);
void demo_nvs_close(nvs_handle_t);
esp_err_t demo_nvs_commit(nvs_handle_t);
esp_err_t demo_nvs_erase_key(nvs_handle_t, const char *);
esp_err_t demo_nvs_get_u8(nvs_handle_t, const char *, uint8_t *);
esp_err_t demo_nvs_set_u8(nvs_handle_t, const char *, uint8_t);
esp_err_t demo_nvs_get_i32(nvs_handle_t, const char *, int32_t *);
esp_err_t demo_nvs_set_i32(nvs_handle_t, const char *, int32_t);
esp_err_t demo_nvs_get_str(nvs_handle_t, const char *, char *, size_t *);
esp_err_t demo_nvs_set_str(nvs_handle_t, const char *, const char *);
esp_err_t demo_nvs_get_blob(nvs_handle_t, const char *, void *, size_t *);
esp_err_t demo_nvs_set_blob(nvs_handle_t, const char *, const void *, size_t);
esp_err_t demo_nvs_flash_erase(void);
esp_http_client_handle_t
demo_http_client_init(const esp_http_client_config_t *);
esp_websocket_client_handle_t
demo_websocket_client_init(const esp_websocket_client_config_t *);
#ifndef PRINTERHMI_DEMO_PLATFORM_IMPLEMENTATION
#define nvs_open demo_nvs_open
#define nvs_close demo_nvs_close
#define nvs_commit demo_nvs_commit
#define nvs_erase_key demo_nvs_erase_key
#define nvs_get_u8 demo_nvs_get_u8
#define nvs_set_u8 demo_nvs_set_u8
#define nvs_get_i32 demo_nvs_get_i32
#define nvs_set_i32 demo_nvs_set_i32
#define nvs_get_str demo_nvs_get_str
#define nvs_set_str demo_nvs_set_str
#define nvs_get_blob demo_nvs_get_blob
#define nvs_set_blob demo_nvs_set_blob
#define nvs_flash_erase demo_nvs_flash_erase
#define esp_http_client_init demo_http_client_init
#define esp_websocket_client_init demo_websocket_client_init
#endif
