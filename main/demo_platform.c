#include "demo_platform.h"
#include "demo_storage.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
static bool active;
static SemaphoreHandle_t mutex;
bool demo_mode_enabled(void) { return active; }
bool demo_mode_init(void) {
  nvs_handle_t h;
  uint8_t selected = 0;
  esp_err_t err = nvs_open("demo_ctl", NVS_READONLY, &h);
  if (err == ESP_OK) {
    err = nvs_get_u8(h, "enabled", &selected);
    nvs_close(h);
  }
  if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
    return false;
  if (!selected)
    return true;
  mutex = xSemaphoreCreateMutex();
  if (!mutex || !demo_storage_init())
    return false;
  active = true;
  return true;
}
bool demo_mode_select(bool enabled) {
  nvs_handle_t h;
  if (nvs_open("demo_ctl", NVS_READWRITE, &h) != ESP_OK)
    return false;
  esp_err_t err = nvs_set_u8(h, "enabled", enabled ? 1 : 0);
  if (err == ESP_OK)
    err = nvs_commit(h);
  nvs_close(h);
  return err == ESP_OK;
}
#define ENTER() xSemaphoreTake(mutex, portMAX_DELAY)
#define LEAVE() xSemaphoreGive(mutex)
esp_err_t demo_nvs_open(const char *space, nvs_open_mode_t mode,
                        nvs_handle_t *out) {
  if (!active)
    return nvs_open(space, mode, out);
  ENTER();
  esp_err_t e = demo_storage_open(space, mode, out);
  LEAVE();
  return e;
}
void demo_nvs_close(nvs_handle_t h) {
  if (!active) {
    nvs_close(h);
    return;
  }
  ENTER();
  demo_storage_close(h);
  LEAVE();
}
esp_err_t demo_nvs_commit(nvs_handle_t h) {
  if (!active)
    return nvs_commit(h);
  ENTER();
  esp_err_t e = demo_storage_commit(h);
  LEAVE();
  return e;
}
static esp_err_t read_value(nvs_handle_t h, const char *key, unsigned type,
                            void *out, size_t *size) {
  ENTER();
  esp_err_t e = demo_storage_read(h, key, type, out, size);
  LEAVE();
  return e;
}
static esp_err_t write_value(nvs_handle_t h, const char *key, unsigned type,
                             const void *data, size_t size) {
  ENTER();
  esp_err_t e = demo_storage_write(h, key, type, data, size);
  LEAVE();
  return e;
}
esp_err_t demo_nvs_erase_key(nvs_handle_t h, const char *key) {
  return active ? write_value(h, key, 0, NULL, 0) : nvs_erase_key(h, key);
}
esp_err_t demo_nvs_get_u8(nvs_handle_t h, const char *key, uint8_t *out) {
  if (!active)
    return nvs_get_u8(h, key, out);
  if (!out)
    return ESP_ERR_INVALID_ARG;
  size_t n = sizeof(*out);
  return read_value(h, key, DEMO_VALUE_U8, out, &n);
}
esp_err_t demo_nvs_set_u8(nvs_handle_t h, const char *key, uint8_t v) {
  return active ? write_value(h, key, DEMO_VALUE_U8, &v, sizeof(v))
                : nvs_set_u8(h, key, v);
}
esp_err_t demo_nvs_get_i32(nvs_handle_t h, const char *key, int32_t *out) {
  if (!active)
    return nvs_get_i32(h, key, out);
  if (!out)
    return ESP_ERR_INVALID_ARG;
  size_t n = sizeof(*out);
  return read_value(h, key, DEMO_VALUE_I32, out, &n);
}
esp_err_t demo_nvs_set_i32(nvs_handle_t h, const char *key, int32_t v) {
  return active ? write_value(h, key, DEMO_VALUE_I32, &v, sizeof(v))
                : nvs_set_i32(h, key, v);
}
esp_err_t demo_nvs_get_str(nvs_handle_t h, const char *key, char *out,
                           size_t *n) {
  return active ? read_value(h, key, DEMO_VALUE_STR, out, n)
                : nvs_get_str(h, key, out, n);
}
esp_err_t demo_nvs_set_str(nvs_handle_t h, const char *key, const char *v) {
  if (!active)
    return nvs_set_str(h, key, v);
  return v ? write_value(h, key, DEMO_VALUE_STR, v, strlen(v) + 1)
           : ESP_ERR_INVALID_ARG;
}
esp_err_t demo_nvs_get_blob(nvs_handle_t h, const char *key, void *out,
                            size_t *n) {
  return active ? read_value(h, key, DEMO_VALUE_BLOB, out, n)
                : nvs_get_blob(h, key, out, n);
}
esp_err_t demo_nvs_set_blob(nvs_handle_t h, const char *key, const void *v,
                            size_t n) {
  return active ? write_value(h, key, DEMO_VALUE_BLOB, v, n)
                : nvs_set_blob(h, key, v, n);
}
esp_err_t demo_nvs_flash_erase(void) {
  return active ? ESP_ERR_INVALID_STATE : nvs_flash_erase();
}
esp_http_client_handle_t
demo_http_client_init(const esp_http_client_config_t *config) {
  return active ? NULL : esp_http_client_init(config);
}
esp_websocket_client_handle_t
demo_websocket_client_init(const esp_websocket_client_config_t *config) {
  return active ? NULL : esp_websocket_client_init(config);
}
