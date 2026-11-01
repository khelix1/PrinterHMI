#include "moonraker_live_websocket.h"
#include "device_catalog_controller.h"
#include "macro_controller.h"
#include "endstop_status_controller.h"
#define PRINTERHMI_DEMO_PLATFORM_IMPLEMENTATION 1
#include "bed_mesh_controller.h"
#include "case_lighting.h"
#include "demo_files.h"
#include "demo_camera.h"
#include "demo_setup.h"
#include "demo_calibration.h"
#include "calibration_session_controller.h"
#include "calibration_capability_controller.h"
#include "console_controller.h"
#include "camera_catalog_controller.h"
#include "demo_mode.h"
#include "demo_platform.h"
#include "demo_storage.h"
#include "esp_heap_caps.h"
#include "moonraker.h"
#include "moonraker_config_controller.h"
#include "printer_profile_health.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned calls;
static unsigned demo_transport_live_calls;
#include "demo_file_transport.inc"
static bool boot_selected, fail_alloc;
static int64_t now = 1000000;
static moonraker_state_t published;
static moonraker_filament_state_t published_filament;
static bool published_exclusions[2];
void moonraker_state_snapshot(moonraker_state_t *out){*out=published;}
void *demo_test_calloc(size_t n, size_t s) {
  return fail_alloc ? NULL : calloc(n, s);
}
int64_t esp_timer_get_time(void) { return now; }
void moonraker_state_publish_demo(const moonraker_state_t *s,
                                  const moonraker_filament_state_t *f,const bool excluded[2]) {
  assert(excluded);published_exclusions[0]=excluded[0];published_exclusions[1]=excluded[1];
  published = *s;
  published_filament = *f;
}
void printer_profile_health_set_live_state(int i, bool known, bool online,
                                           const char *s) {
  (void)i;
  (void)known;
  (void)online;
  (void)s;
}
esp_err_t nvs_open(const char *s, int m, nvs_handle_t *h) {
  (void)s;
  (void)m;
  ++calls;
  *h = 99;
  return ESP_OK;
}
void nvs_close(nvs_handle_t h) {
  assert(h == 99);
  ++calls;
}
esp_err_t nvs_commit(nvs_handle_t h) {
  assert(h == 99);
  ++calls;
  return ESP_OK;
}
esp_err_t nvs_get_u8(nvs_handle_t h, const char *k, uint8_t *v) {
  assert(h == 99);
  (void)k;
  ++calls;
  *v = boot_selected;
  return ESP_OK;
}
esp_err_t nvs_set_u8(nvs_handle_t h, const char *k, uint8_t v) {
  assert(h == 99);
  (void)k;
  ++calls;
  boot_selected = v;
  return ESP_OK;
}
esp_err_t nvs_get_i32(nvs_handle_t h, const char *k, int32_t *v) {
  (void)h;
  (void)k;
  (void)v;
  ++calls;
  return ESP_OK;
}
esp_err_t nvs_set_i32(nvs_handle_t h, const char *k, int32_t v) {
  (void)h;
  (void)k;
  (void)v;
  ++calls;
  return ESP_OK;
}
esp_err_t nvs_get_str(nvs_handle_t h, const char *k, char *v, size_t *n) {
  (void)h;
  (void)k;
  (void)v;
  (void)n;
  ++calls;
  return ESP_OK;
}
esp_err_t nvs_set_str(nvs_handle_t h, const char *k, const char *v) {
  (void)h;
  (void)k;
  (void)v;
  ++calls;
  return ESP_OK;
}
esp_err_t nvs_get_blob(nvs_handle_t h, const char *k, void *v, size_t *n) {
  (void)h;
  (void)k;
  (void)v;
  (void)n;
  ++calls;
  return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h, const char *k, const void *v, size_t n) {
  (void)h;
  (void)k;
  (void)v;
  (void)n;
  ++calls;
  return ESP_OK;
}
esp_err_t nvs_erase_key(nvs_handle_t h, const char *k) {
  (void)h;
  (void)k;
  ++calls;
  return ESP_OK;
}
esp_err_t nvs_flash_erase(void) {
  ++calls;
  return ESP_OK;
}
esp_http_client_handle_t
esp_http_client_init(const esp_http_client_config_t *c) {
  (void)c;
  ++calls;
  return (void *)1;
}
esp_websocket_client_handle_t
esp_websocket_client_init(const esp_websocket_client_config_t *c) {
  (void)c;
  ++calls;
  return (void *)2;
}
static void live(void) {
  nvs_handle_t h;
  uint8_t u;
  int32_t i = 0;
  size_t n = 0;
  assert(demo_nvs_open("test", NVS_READWRITE, &h) == ESP_OK && h == 99);
  assert(demo_nvs_set_u8(h, "u", 0) == ESP_OK);
  assert(demo_nvs_get_u8(h, "u", &u) == ESP_OK);
  assert(demo_nvs_set_i32(h, "i", 3) == ESP_OK);
  assert(demo_nvs_get_i32(h, "i", &i) == ESP_OK);
  assert(demo_nvs_set_str(h, "s", "value") == ESP_OK);
  assert(demo_nvs_get_str(h, "s", NULL, &n) == ESP_OK);
  assert(demo_nvs_set_blob(h, "b", "x", 1) == ESP_OK);
  assert(demo_nvs_get_blob(h, "b", NULL, &n) == ESP_OK);
  assert(demo_nvs_erase_key(h, "s") == ESP_OK);
  assert(demo_nvs_commit(h) == ESP_OK);
  demo_nvs_close(h);
  assert(demo_nvs_flash_erase() == ESP_OK);
  assert(demo_http_client_init(NULL) == (void *)1);
  assert(demo_websocket_client_init(NULL) == (void *)2);
  char body[32]="live";int code=0;esp_err_t error=ESP_FAIL;uint8_t *png=NULL;size_t png_size=0;
  assert(!moonraker_fetch_file_metadata("real.invalid",7125,"","Spool+holder.gcode",body,sizeof(body),&code,&error));
  assert(!moonraker_fetch_thumbnail_encoded_quiet("real.invalid",7125,".demo/previews/0.png",&png,&png_size));
  assert(!moonraker_start_print_file("real.invalid",7125,"","Spool holder.gcode",&code,&error));
  assert(!moonraker_live_websocket_connected());assert(!moonraker_live_websocket_request_endstops(0));
  assert(demo_transport_live_calls==5); /* Live suffix spies ran, never the sample routes. */
}
static void transactions(void) {
  nvs_handle_t a, b, ro;
  uint8_t value = 0;
  char text[8];
  size_t size;
  assert(demo_nvs_open("saved", NVS_READONLY, &a) == ESP_ERR_NVS_NOT_FOUND);
  assert(demo_nvs_open("test", NVS_READWRITE, &a) == ESP_OK);
  assert(demo_nvs_open("test", NVS_READWRITE, &b) == ESP_OK);
  assert(demo_nvs_set_u8(a, "value", 42) == ESP_OK);
  assert(demo_nvs_get_u8(b, "value", &value) == ESP_ERR_NVS_NOT_FOUND);
  assert(demo_nvs_commit(a) == ESP_OK);
  assert(demo_nvs_get_u8(b, "value", &value) == ESP_OK && value == 42);
  assert(demo_nvs_open("test", NVS_READONLY, &ro) == ESP_OK);
  assert(demo_nvs_set_u8(ro, "value", 0) == ESP_ERR_NVS_READ_ONLY);
  assert(demo_nvs_set_u8(a, "value", 43) == ESP_OK);
  demo_nvs_close(a);
  assert(demo_nvs_get_u8(b, "value", &value) == ESP_OK && value == 42);
  assert(demo_nvs_commit(a) == ESP_ERR_NVS_INVALID_HANDLE);
  assert(demo_nvs_set_str(b, "text", "hello") == ESP_OK);
  size = 1;
  assert(demo_nvs_get_str(b, "text", text, &size) ==
             ESP_ERR_NVS_INVALID_LENGTH &&
         size == 6);
  size = sizeof(text);
  assert(demo_nvs_get_str(b, "text", text, &size) == ESP_OK &&
         !strcmp(text, "hello"));
  assert(demo_nvs_get_u8(b, "text", &value) == ESP_ERR_NVS_TYPE_MISMATCH);
  fail_alloc = true;
  assert(demo_nvs_set_str(b, "text", "changed") == ESP_ERR_NO_MEM);
  fail_alloc = false;
  size = sizeof(text);
  assert(demo_nvs_get_str(b, "text", text, &size) == ESP_OK &&
         !strcmp(text, "hello"));
  assert(demo_nvs_erase_key(b, "value") == ESP_OK);
  assert(demo_nvs_commit(b) == ESP_OK);
  assert(demo_nvs_get_u8(ro, "value", &value) == ESP_ERR_NVS_NOT_FOUND);
  assert(demo_nvs_erase_key(b, "missing") == ESP_ERR_NVS_NOT_FOUND);
  assert(demo_nvs_set_blob(b, "oversize", text, 8193) == ESP_ERR_INVALID_ARG);
  demo_nvs_close(b);
  demo_nvs_close(ro);
}
static void camera(void) {
  camera_catalog_entry_t entry;
  assert(camera_catalog_get(0,0,&entry) && entry.configured && !strcmp(entry.stream_url, DEMO_CAMERA_FRONT_URL));
  assert(camera_catalog_get(0,1,&entry) && entry.configured && !strcmp(entry.stream_url, DEMO_CAMERA_SIDE_URL));
  assert(camera_catalog_get(1,0,&entry) && entry.configured);
  assert(!demo_camera_stream_start("http://example.com/camera"));
  fail_alloc=true; assert(!demo_camera_stream_start(DEMO_CAMERA_FRONT_URL)); fail_alloc=false;
  assert(demo_camera_stream_start(DEMO_CAMERA_FRONT_URL));
  assert(!demo_camera_stream_start(DEMO_CAMERA_SIDE_URL));
  uint8_t *pixels=NULL; size_t size=0; int width=0,height=0; bool ok=false;
  fail_alloc=true; assert(!demo_camera_stream_take(&pixels,&size,&width,&height,&ok)); fail_alloc=false;
  assert(demo_camera_stream_take(&pixels,&size,&width,&height,&ok));
  assert(ok && width==480 && height==320 && size==480U*320U*2U);
  unsigned front=((uint16_t *)pixels)[180*480+210]; free(pixels);
  assert(!demo_camera_stream_take(NULL,NULL,NULL,NULL,NULL));
  now+=1000000; assert(demo_camera_stream_take(NULL,&size,NULL,NULL,NULL));
  demo_camera_stream_stop(); assert(!demo_camera_stream_busy());
  assert(!demo_camera_stream_take(&pixels,&size,&width,&height,&ok));
  assert(demo_camera_stream_start(DEMO_CAMERA_SIDE_URL));
  assert(demo_camera_stream_take(&pixels,&size,&width,&height,&ok));
  assert(((uint16_t *)pixels)[180*480+210] != front); free(pixels);
  demo_camera_stream_stop(); demo_camera_stream_stop();
  assert(demo_camera_test_start(DEMO_CAMERA_FRONT_URL));
  assert(!demo_camera_test_start(DEMO_CAMERA_SIDE_URL));
  assert(demo_camera_test_take(&ok,&width,&height,&size) && ok && width==480 && height==320);
  assert(!demo_camera_test_take(&ok,&width,&height,&size));
  assert(demo_camera_test_start("http://example.com/camera"));
  assert(demo_camera_test_take(&ok,&width,&height,&size) && !ok && size==0);
  moonraker_webcam_t webcam; size_t count=0;
  assert(!demo_camera_discovery_start(0,"",7125));
  assert(!demo_camera_discovery_start(2,"demo.invalid",7125));
  assert(demo_camera_discovery_start(0,"demo.invalid",7125));
  assert(!demo_camera_discovery_start(0,"demo.invalid",7125));
  assert(demo_camera_discovery_take(&webcam,&ok,&count) && ok && count==2 && !strcmp(webcam.stream_url,DEMO_CAMERA_FRONT_URL));
  assert(!demo_camera_discovery_take(&webcam,&ok,&count));
  assert(demo_camera_discovery_start(0,"demo.invalid",7125));
  assert(moonraker_config_select_profile(1));
  assert(!demo_camera_discovery_take(&webcam,&ok,&count));
  assert(moonraker_config_select_profile(0)); demo_printer_tick();
  puts("PASS demo camera local catalog, discovery, frames, timing, allocation failure and URL isolation");
}
static void runtime(void) {
  bed_mesh_controller_init();
  assert(demo_printer_init());
  assert(!strcmp(moonraker_config_host(), "demo.invalid"));
  assert(!strcmp(published.printer_state, "printing"));
  assert(published.capabilities.has_exclude_object);
  assert(demo_printer_command("EXCLUDE_OBJECT NAME=part_1"));assert(published_exclusions[0]&&!published_exclusions[1]);
  double progress = published.progress;
  now += 10000000;
  demo_printer_tick();
  assert(published.progress > progress);
  assert(demo_printer_command("PAUSE"));
  progress = published.progress;
  now += 10000000;
  demo_printer_tick();
  assert(published.progress == progress &&
         !strcmp(published.printer_state, "paused"));
  assert(!demo_printer_command("M104 Snan"));
  assert(!demo_printer_command("UNSUPPORTED"));
  assert(demo_printer_command("M104 S220"));
  assert(demo_printer_command(
      "SET_HEATER_TEMPERATURE HEATER=extruder TARGET=215"));
  assert(published.nozzle_target == 215);
  assert(!demo_printer_command(
      "SET_HEATER_TEMPERATURE HEATER=extruder TARGET=nan"));
  assert(!demo_printer_command(
      "SET_HEATER_TEMPERATURE HEATER=unknown TARGET=215"));
  assert(demo_printer_command("SET_FILAMENT_SENSOR SENSOR=runout ENABLE=0"));
  assert(!published_filament.sensors[0].enabled);
  assert(demo_printer_light(moonraker_config_generation(), 53));
  case_lighting_snapshot_t light;
  case_lighting_snapshot(&light, now);
  assert(light.available && light.known && light.value == .53);
  uint32_t old = moonraker_config_generation();
  assert(moonraker_config_select_profile(1));
  demo_printer_tick();
  assert(!strcmp(published.printer_state, "standby"));
  assert(!published_exclusions[0]&&!published_exclusions[1]);
  case_lighting_snapshot(&light, now);
  assert(!light.available);
  assert(!demo_printer_light(old, 20));
  assert(moonraker_config_select_profile(0));
  demo_printer_tick();
  assert(!strcmp(published.printer_state, "paused") &&
         published.progress == progress);
  assert(published_exclusions[0]&&!published_exclusions[1]);
  assert(demo_printer_command("RESUME"));
  assert(demo_printer_scenario("complete"));
  assert(published.progress == 1);
  assert(demo_printer_scenario("offline"));
  assert(!published.moonraker_ok);
  assert(demo_printer_scenario("printing"));
  assert(!published_exclusions[0]&&!published_exclusions[1]);
  assert(demo_printer_command("M112"));
  assert(published.nozzle_target == 0 &&
         !strcmp(published.printer_state, "shutdown"));
  assert(demo_printer_command("FIRMWARE_RESTART"));
  assert(!strcmp(published.printer_state, "standby"));
  bed_mesh_snapshot_t mesh;
  float values[9];
  bed_mesh_profile_name_t names[16];
  assert(bed_mesh_controller_snapshot(&mesh, values, 9, names, 16) &&
         mesh.valid && mesh.rows == 3 && mesh.cols == 3);
}
static void files(void) {
  char body[4096];
  int code = 0;
  esp_err_t error = ESP_FAIL;
  assert(demo_files_count() == 7);
  assert(!demo_files_at(SIZE_MAX));
  assert(moonraker_fetch_file_metadata("demo.invalid", 7125, "",
                                       "Calibration%2FCalibration%20cube.gcode",
                                       body, sizeof(body), &code, &error));
  assert(code == 200 && error == ESP_OK && strstr(body, "900") &&
         strstr(body, "1800"));
  cJSON *json = cJSON_Parse(body);
  assert(json);
  cJSON *result = cJSON_GetObjectItemCaseSensitive(json, "result");
  assert(
      !strcmp(cJSON_GetObjectItemCaseSensitive(result, "filename")->valuestring,
              "Calibration/Calibration cube.gcode"));
  cJSON_Delete(json);
  const char *bad[] = {"",
                       "%",
                       "%0",
                       "%GG",
                       "%00",
                       "../Spool%20holder.gcode",
                       "Calibration%2F..%2FSpool%20holder.gcode",
                       "Spool%20holder.gcode%00ignored",
                       "Spool%20holder.gcode%0A"};
  for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
    assert(!moonraker_fetch_file_metadata("demo.invalid", 7125, "", bad[i],
                                          body, sizeof(body), &code, &error));
  assert(!demo_files_metadata("Spool+holder.gcode", body, 1) && body[0] == 0);
  assert(demo_files_metadata("Spool+holder.gcode", body, sizeof(body)));
  assert(!demo_files_metadata(NULL, body, sizeof(body)));
  assert(!demo_files_metadata("Spool+holder.gcode", NULL, sizeof(body)));
  for (unsigned i = 0; i < 3; i++) {
    char path[80];
    snprintf(path, sizeof(path), ".demo%%2Fpreviews%%2F%u.png", i);
    uint8_t *png = NULL;
    size_t size = 0;
    assert(moonraker_fetch_thumbnail_encoded_quiet("demo.invalid", 7125, path,
                                                   &png, &size));
    assert(size > 100 && !memcmp(png, "\x89PNG\r\n\x1a\n", 8));
    assert(png[16] == 0 && png[17] == 0 && png[18] == 3 && png[19] == 132);
    free(png);
  }
  uint8_t *png = (void *)1;
  size_t size = 123;
  assert(!moonraker_fetch_thumbnail_encoded_quiet("demo.invalid", 7125, "../x",
                                                  &png, &size) &&
         !png && !size);
  assert(!moonraker_fetch_thumbnail_encoded_quiet(
      "demo.invalid", 7125, ".demo/previews/0.png", NULL, &size));
  fail_alloc = true;
  assert(!moonraker_fetch_thumbnail_encoded_quiet(
      "demo.invalid", 7125, ".demo/previews/0.png", &png, &size));
  fail_alloc = false;
  assert(demo_printer_scenario("idle"));
  assert(!moonraker_start_print_file("demo.invalid", 7125, "", "unknown.gcode",
                                     &code, &error) &&
         code == 404);
  assert(moonraker_start_print_file("demo.invalid", 7125, "",
                                    "Calibration/Calibration cube.gcode", &code,
                                    &error) &&
         code == 200);
  assert(
      !strcmp(published.printer_file, "Calibration/Calibration cube.gcode") &&
      published.progress == 0 && published.nozzle_target == 210 &&
      published.total_layer == 100);
  assert(!moonraker_start_print_file("demo.invalid", 7125, "", "Bracket.gcode",
                                     &code, &error) &&
         code == 409);
  assert(!demo_printer_command("SDCARD_PRINT_FILE FILENAME=\"Bracket.gcode\""));
  assert(demo_printer_command("CANCEL_PRINT"));
  assert(demo_printer_command("SDCARD_PRINT_FILE FILENAME=\"Bracket.gcode\""));
  assert(!strcmp(published.printer_file, "Bracket.gcode"));
  assert(moonraker_config_select_profile(1));demo_printer_tick();
  assert(demo_printer_start_file("Spool holder.gcode"));
  assert(moonraker_config_select_profile(0));demo_printer_tick();assert(!strcmp(published.printer_file,"Bracket.gcode"));
  assert(moonraker_config_select_profile(1));demo_printer_tick();assert(!strcmp(published.printer_file,"Spool holder.gcode"));
  assert(moonraker_config_select_profile(0));demo_printer_tick();

  assert(demo_printer_scenario("idle"));
  assert(!demo_printer_command("SDCARD_PRINT_FILE FILENAME=\"unknown.gcode\""));
  assert(!demo_printer_command(
      "SDCARD_PRINT_FILE FILENAME=\"Spool holder.gcode\"\nM112"));
  assert(!demo_transport_live_calls);
}
static void services(void){
 device_catalog_status_t devices;macro_controller_status_t macros;endstop_status_snapshot_t stops;
 assert(demo_printer_scenario("idle"));device_catalog_controller_status(&devices);macro_controller_status(&macros);
 assert(devices.discovered && devices.stored_count==24 && macros.count==20);
 uint32_t request=endstop_status_controller_begin(moonraker_config_generation());assert(moonraker_live_websocket_request_endstops(request));endstop_status_controller_snapshot(&stops);assert(stops.valid && stops.count==3);
 assert(demo_printer_command("G34"));request=endstop_status_controller_begin(moonraker_config_generation());assert(moonraker_live_websocket_request_endstops(request));endstop_status_controller_snapshot(&stops);for(unsigned i=0;i<3;i++)assert(stops.items[i].triggered);
 assert(demo_printer_command("PARK_TOOLHEAD"));request=endstop_status_controller_begin(moonraker_config_generation());assert(moonraker_live_websocket_request_endstops(request));endstop_status_controller_snapshot(&stops);for(unsigned i=0;i<3;i++)assert(!stops.items[i].triggered);
 assert(demo_printer_command("CLEAN_NOZZLE TEMP=215 SPEED=8"));assert(published.nozzle_target==215);
 const char *bad[]={"CLEAN_NOZZLE TEMP=nan","CLEAN_NOZZLE TEMP=301","CLEAN_NOZZLE TEMP=200 TEMP=210","CLEAN_NOZZLE EXTRA=1","BED_HEAT_SOAK MINUTES=121","CLEAN_NOZZLE TEMP=200;M112","HMI_FILAMENT_HEAT\nTEMP=300","START_PRINT BED_TEMP=60","PID_BED BAD=1"};
 for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++){assert(!demo_printer_command(bad[i]));assert(published.nozzle_target==215);}
 assert(demo_printer_command("HMI_FILAMENT_UNLOAD TEMP=210 LENGTH=50"));assert(!published_filament.sensors[0].filament_detected);
 assert(demo_printer_command("LOAD_FILAMENT LENGTH=50"));assert(published_filament.sensors[0].filament_detected);
 assert(demo_printer_command("DRY_PETG"));double temp=published.chamber_temp,humidity=published.humidity;now+=120000000;demo_printer_tick();assert(published.chamber_temp>temp && published.humidity<humidity && published.heater_target==55);
 assert(demo_printer_command("DRY_HOLD"));assert(!published.heater_on && published.drybox_fan_speed==40);
 assert(demo_printer_command("DRY_RESUME"));assert(published.heater_on && published.heater_target==55);
 assert(demo_printer_light(moonraker_config_generation(),53));bool found=false;for(size_t i=0;i<devices.stored_count;i++){device_descriptor_t item;assert(device_catalog_controller_get(i,&item));if(!strcmp(item.object_name,"output_pin case_led")){found=true;assert(item.live_value_valid && strstr(item.live_value,"53"));}}assert(found);
 assert(macro_controller_toggle_favorite("CLEAN_NOZZLE"));assert(macro_controller_is_favorite("CLEAN_NOZZLE"));
 assert(moonraker_config_select_profile(1));demo_printer_tick();device_catalog_controller_status(&devices);macro_controller_status(&macros);assert(devices.stored_count==4 && macros.count==3);assert(!demo_printer_command("DRY_PLA"));assert(!published.capabilities.has_drybox_heater && !published.heater_on);
 assert(demo_printer_scenario("offline"));device_catalog_controller_status(&devices);macro_controller_status(&macros);assert(!devices.stored_count && !macros.count && !moonraker_live_websocket_request_endstops(request));
 assert(moonraker_config_select_profile(0));demo_printer_tick();assert(published.heater_on && published.heater_target==55);assert(macro_controller_is_favorite("CLEAN_NOZZLE"));
 assert(demo_printer_command("DRY_STOP"));assert(!published.heater_on && !published.drybox_fan_speed);
 assert(demo_printer_command("EXCLUDE_OBJECT NAME=part_1"));assert(demo_printer_command("START_PRINT BED_TEMP=65 EXTRUDER_TEMP=220"));assert(published.bed_target==65 && published.nozzle_target==220 && !published_exclusions[0]);
 request=endstop_status_controller_begin(moonraker_config_generation());assert(!moonraker_live_websocket_request_endstops(request));
 assert(demo_printer_command("M112"));assert(!published.heater_on && !published.heater_target);assert(demo_printer_command("FIRMWARE_RESTART"));
}
static void calibration(void) {
  assert(demo_printer_scenario("idle"));
  calibration_capabilities_t caps; calibration_capability_controller_snapshot(&caps);
  assert(caps.discovered && caps.probe_accuracy && caps.axis_twist);
  calibration_session_snapshot_t session;
  assert(!demo_calibration_send("PROBE_ACCURACY SAMPLES=5"));
  calibration_session_controller_begin(CALIBRATION_SESSION_PROBE_ACCURACY,console_controller_latest_sequence());
  assert(!demo_calibration_send("PROBE_ACCURACY SAMPLES=nan"));
  assert(!demo_calibration_send("PROBE_ACCURACY SAMPLES=500"));
  assert(demo_calibration_send("PROBE_ACCURACY SAMPLES=5"));
  calibration_session_controller_poll();calibration_session_controller_snapshot(&session);
  assert(session.completed && !session.save_available && strstr(session.results,"probe accuracy"));
  calibration_session_controller_begin(CALIBRATION_SESSION_PID,console_controller_latest_sequence());
  assert(!demo_calibration_send("PID_CALIBRATE HEATER=extruder TARGET=nan"));
  assert(!demo_calibration_send("PID_CALIBRATE HEATER=other TARGET=210"));
  assert(demo_calibration_send("PID_CALIBRATE HEATER=extruder TARGET=210"));
  calibration_session_controller_poll();calibration_session_controller_snapshot(&session);
  assert(session.completed && session.save_available && strstr(session.results,"PID parameters"));
  assert(demo_calibration_send("SAVE_CONFIG"));
  calibration_session_controller_begin(CALIBRATION_SESSION_INPUT_SHAPER,console_controller_latest_sequence());
  assert(demo_calibration_send("SHAPER_CALIBRATE"));calibration_session_controller_poll();calibration_session_controller_snapshot(&session);
  assert(session.completed && session.save_available && strstr(session.results,"shaper_type_x"));
  calibration_session_controller_begin(CALIBRATION_SESSION_RESONANCE_TEST,console_controller_latest_sequence());
  assert(!demo_calibration_send("TEST_RESONANCES AXIS=Z"));assert(demo_calibration_send("TEST_RESONANCES AXIS=X"));
  calibration_session_controller_poll();calibration_session_controller_snapshot(&session);assert(!session.save_available && strstr(session.results,"resonance data written"));
  calibration_session_controller_begin(CALIBRATION_SESSION_ACCELEROMETER_CHECK,console_controller_latest_sequence());
  assert(demo_calibration_send("MEASURE_AXES_NOISE"));calibration_session_controller_poll();calibration_session_controller_snapshot(&session);assert(session.completed && !session.save_available);
  assert(demo_calibration_send("SET_VELOCITY_LIMIT SQUARE_CORNER_VELOCITY=1 ACCEL=500\nTUNING_TOWER COMMAND=SET_PRESSURE_ADVANCE PARAMETER=ADVANCE START=0 FACTOR=.005"));
  assert(!demo_calibration_send("SET_VELOCITY_LIMIT SQUARE_CORNER_VELOCITY=1 ACCEL=500\nTUNING_TOWER COMMAND=SET_PRESSURE_ADVANCE PARAMETER=ADVANCE START=0 FACTOR=nan"));
  calibration_session_controller_begin(CALIBRATION_SESSION_PROBE_Z,console_controller_latest_sequence());
  assert(demo_calibration_send("PROBE_CALIBRATE"));
  double z=published.toolhead_z;
  assert(!demo_calibration_send("TESTZ Z=nan"));
  assert(demo_calibration_send("TESTZ Z=-0.1"));assert(published.toolhead_z<z);
  assert(demo_calibration_send("ACCEPT"));calibration_session_controller_poll();calibration_session_controller_snapshot(&session);
  assert(session.completed && session.save_available);
  calibration_session_controller_begin(CALIBRATION_SESSION_AXIS_TWIST,console_controller_latest_sequence());
  assert(demo_calibration_send("AXIS_TWIST_COMPENSATION_CALIBRATE"));
  assert(demo_calibration_send("ACCEPT"));assert(demo_calibration_send("ACCEPT"));
  calibration_session_controller_poll();calibration_session_controller_snapshot(&session);assert(!session.completed);
  assert(demo_calibration_send("ACCEPT"));calibration_session_controller_poll();calibration_session_controller_snapshot(&session);assert(session.completed && session.save_available);
  calibration_session_controller_begin(CALIBRATION_SESSION_SCREWS_TILT,console_controller_latest_sequence());
  assert(demo_calibration_send("G28\nSCREWS_TILT_CALCULATE"));calibration_session_controller_poll();calibration_session_controller_snapshot(&session);
  assert(session.completed && strstr(session.results,"front_right"));
  assert(!demo_calibration_send("G28\nM112"));
  assert(moonraker_config_select_profile(1));demo_printer_tick();calibration_capability_controller_snapshot(&caps);assert(!caps.axis_twist && !caps.probe_accuracy);
  calibration_session_controller_snapshot(&session);assert(session.status==CALIBRATION_SESSION_IDLE);
  assert(moonraker_config_select_profile(0));demo_printer_tick();
  assert(demo_printer_scenario("printing"));assert(!demo_calibration_send("G28"));
  assert(demo_printer_scenario("offline"));assert(!demo_calibration_send("G28"));
  assert(demo_printer_scenario("idle"));
  calibration_session_controller_begin(CALIBRATION_SESSION_CUSTOM,console_controller_latest_sequence());
  assert(demo_calibration_send("PID_BED"));calibration_session_controller_poll();calibration_session_controller_snapshot(&session);
  assert(session.completed && session.save_available && published.bed_target==60);
  puts("PASS demo calibration capabilities, results, safety guards and profile switching");
}
static void capacity(void) {
  nvs_handle_t handles[8];
  for (unsigned i = 0; i < 8; i++)
    assert(demo_nvs_open("limits", NVS_READWRITE, &handles[i]) == ESP_OK);
  nvs_handle_t extra;
  assert(demo_nvs_open("limits", NVS_READWRITE, &extra) == ESP_ERR_NO_MEM);
  for (unsigned i = 0; i < 8; i++)
    demo_nvs_close(handles[i]);
  nvs_handle_t h, observer;
  assert(demo_nvs_open("limits", NVS_READWRITE, &h) == ESP_OK);
  char key[16];
  for (unsigned i = 0; i < 48; i++) {
    snprintf(key, sizeof(key), "p%u", i);
    assert(demo_nvs_set_u8(h, key, 1) == ESP_OK);
  }
  assert(demo_nvs_set_u8(h, "overflow", 1) == ESP_ERR_NO_MEM);
  demo_nvs_close(h);
  assert(demo_nvs_open("limits", NVS_READWRITE, &h) == ESP_OK);
  char blob[8192] = {0};
  unsigned count = 0;
  for (; count < 32; count++) {
    snprintf(key, sizeof(key), "b%u", count);
    esp_err_t e = demo_nvs_set_blob(h, key, blob, sizeof(blob));
    if (e == ESP_ERR_NO_MEM)
      break;
    assert(e == ESP_OK);
    assert(demo_nvs_commit(h) == ESP_OK);
  }
  assert(count > 0 && count < 16);
  for (unsigned i = 0; i < count; i++) {
    snprintf(key, sizeof(key), "b%u", i);
    assert(demo_nvs_erase_key(h, key) == ESP_OK);
  }
  assert(demo_nvs_commit(h) == ESP_OK);
  assert(demo_nvs_open("limits", NVS_READWRITE, &observer) == ESP_OK);
  for (unsigned batch = 0; batch < 8; batch++) {
    for (unsigned j = 0; j < 32; j++) {
      snprintf(key, sizeof(key), "k%u", batch * 32 + j);
      assert(demo_nvs_set_u8(h, key, 42) == ESP_OK);
    }
    esp_err_t e = demo_nvs_commit(h);
    if (e == ESP_ERR_NO_MEM) {
      uint8_t v = 0;
      for (unsigned j = 0; j < 32; j++) {
        snprintf(key, sizeof(key), "k%u", batch * 32 + j);
        assert(demo_nvs_get_u8(observer, key, &v) == ESP_ERR_NVS_NOT_FOUND);
      }
      assert(batch > 0);
      demo_nvs_close(h);
      demo_nvs_close(observer);
      return;
    }
    assert(e == ESP_OK);
  }
  assert(!"store exceeded committed key limit");
}
int main(int argc, char **argv) {
  assert(argc == 2);
  boot_selected = strcmp(argv[1], "live") != 0;
  if (!strcmp(argv[1], "oom")) {
    fail_alloc = true;
    assert(!demo_mode_init());
    assert(!demo_mode_enabled());
    puts("PASS demo initialization fails closed on allocation failure");
    return 0;
  }
  assert(demo_mode_init());
  calls = 0;
  if (!boot_selected) {
    assert(!demo_mode_enabled());
    live();
    assert(calls == 15);
    puts("PASS live APIs delegate unchanged");
    return 0;
  }
  assert(demo_mode_enabled());
  transactions();
  runtime();
  files();
  char demo_ssid[33];
  assert(!demo_setup_network(demo_ssid,sizeof(demo_ssid)));
  assert(!demo_setup_connect("", "secret"));
  assert(!demo_setup_connect("012345678901234567890123456789012", "secret"));
  assert(demo_setup_connect("Demo network", "secret"));
  assert(!demo_setup_network(demo_ssid,4));
  assert(demo_setup_network(demo_ssid,sizeof(demo_ssid)) && !strcmp(demo_ssid,"Demo network"));
  camera();
  services();
  calibration();
  capacity();
  assert(!demo_http_client_init(NULL));
  assert(!demo_websocket_client_init(NULL));
  assert(demo_nvs_flash_erase() == ESP_ERR_INVALID_STATE);
  assert(calls == 0);
  assert(demo_mode_select(false));
  assert(!boot_selected && demo_mode_enabled());
  assert(calls == 4);
  puts("PASS demo settings/commands/lighting/mesh are local; only explicit "
       "boot selection writes NVS");
  return 0;
}
