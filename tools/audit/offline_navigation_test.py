#!/usr/bin/env python3
"""Host checks for offline navigation using the camera worker and extracted WebSocket lifecycle functions."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
def function(source,name):
    import re
    match=re.search(r"(?:static )?(?:void|bool) "+name+r"\([^)]*\)\s*\{",source)
    if not match:raise RuntimeError("Missing production function: "+name)
    start=match.start();body=source.index("{",match.start());depth=1;end=body+1
    while depth:
        if source[end]=="{":depth+=1
        elif source[end]=="}":depth-=1
        end+=1
    return source[start:end]+"\n"
with tempfile.TemporaryDirectory(prefix="offline-navigation-") as directory:
    tmp=Path(directory);(tmp/"freertos").mkdir()
    stubs={
      "esp_err.h":"#pragma once\ntypedef int esp_err_t;\n#define ESP_OK 0\n#define ESP_FAIL -1\n",
      "esp_heap_caps.h":"#pragma once\n#include <stddef.h>\n#include <stdint.h>\n#define MALLOC_CAP_SPIRAM 1\n#define MALLOC_CAP_8BIT 2\nvoid *heap_caps_malloc(size_t,uint32_t);\nvoid heap_caps_free(void*);\n",
      "esp_http_client.h":"#pragma once\n#include \"esp_err.h\"\ntypedef void *esp_http_client_handle_t;\ntypedef struct {const char *url,*cert_pem;int method,timeout_ms,buffer_size;} esp_http_client_config_t;\n#define HTTP_METHOD_GET 1\nesp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t*);\nesp_err_t esp_http_client_open(esp_http_client_handle_t,int);\nint esp_http_client_fetch_headers(esp_http_client_handle_t);\nint esp_http_client_read(esp_http_client_handle_t,char*,int);\nesp_err_t esp_http_client_close(esp_http_client_handle_t);\nesp_err_t esp_http_client_cleanup(esp_http_client_handle_t);\n",
      "freertos/FreeRTOS.h":"#pragma once\n#include <stddef.h>\n#define pdPASS 1\n#define pdMS_TO_TICKS(ms) (ms)\n#define tskNO_AFFINITY -1\nsize_t strlcpy(char*,const char*,size_t);\n",
      "freertos/task.h":"#pragma once\ntypedef void *TaskHandle_t;\nvoid vTaskDelay(unsigned);\nvoid vTaskDelete(void*);\nint xTaskCreatePinnedToCoreWithCaps(void (*)(void*),const char*,unsigned,void*,unsigned,TaskHandle_t*,int,unsigned);\n",
      "freertos/idf_additions.h":"#pragma once\n#include \"task.h\"\n",
      "freertos/queue.h":"#pragma once\n#include <stddef.h>\ntypedef void *QueueHandle_t;\nQueueHandle_t xQueueCreate(unsigned,size_t);\nint xQueueReceive(QueueHandle_t,void*,unsigned);\nint xQueueSend(QueueHandle_t,const void*,unsigned);\n"}
    for name,text in stubs.items():(tmp/name).write_text(text)
    ws=(root/"main/moonraker_live_websocket.c").read_text()
    preamble="""#include <stdbool.h>
#include <stdint.h>
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define TAG "test"
#define WS_RETRY_INTERVAL_US 3000000LL
static void *s_client;
static bool s_connected,s_subscribed,s_discovery_pending,s_macro_config_pending,s_subscription_ready,s_subscribe_pending,s_file_change_pending,in_runtime;
static uint32_t s_generation,s_accepted_generation;
static int64_t s_last_status_update_us,s_retry_after_us;
static char s_host[128];
uint32_t moonraker_config_generation(void);
int64_t esp_timer_get_time(void);
static void destroy_client(void);
static bool create_client(const char*,int,const char*,uint32_t);
static bool send_identify(void),send_object_list(void),send_subscription(void);
int esp_websocket_client_send_text(void*,const char*,int,unsigned);
"""
    names=["prepare_profile_change","connected","subscribed","fresh","file_change_pending","take_file_change","tasklet"]
    (tmp/"offline_ws_extracted.inc").write_text(preamble+"\n".join(function(ws,"moonraker_live_websocket_"+name) for name in names))
    # UI-only call sites must never reintroduce the waiting camera stop path.
    camera=(root/"main/ui_camera.c").read_text()
    for name in ["ui_camera_hide","ui_camera_destroy","camera_picker_select_cb","camera_view_set_cb"]:
        assert "camera_stream_stop(" not in function(camera,name),name
    dashboard=(root/"main/ui_dashboard.c").read_text()
    assert "camera_stream_stop(" not in function(dashboard,"dashboard_camera_mode_set")
    for variant in ["CAMERA","WS"]:
        executable=tmp/variant.lower()
        subprocess.run(["cc","-std=c11","-O2","-Wall","-Wextra","-Werror","-DTEST_"+variant,"-I",str(tmp),"-I",str(root/"main"),str(root/"tools/audit/offline_navigation_test.c"),"-o",str(executable)],check=True)
        subprocess.run([str(executable)],check=True)
