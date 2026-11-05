#!/usr/bin/env python3
"""Drive the production metadata worker with deferred tasks and scripted replies."""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
native = root / "tools/theme-maker/linux/native"
cli = argparse.ArgumentParser()
cli.add_argument("--lvgl-dir", type=Path, required=True)
args = cli.parse_args()
with tempfile.TemporaryDirectory() as tmp:
    path = Path(tmp)
    (path / "freertos").mkdir()
    (path / "freertos/FreeRTOS.h").write_text("""
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define taskENTER_CRITICAL(x) ((void)(x))
#define taskEXIT_CRITICAL(x) ((void)(x))
#define pdPASS 1
""")
    (path / "freertos/task.h").write_text("""
int xTaskCreatePinnedToCore(void (*fn)(void *), const char *, int, void *, int, void *, int);
void vTaskDelete(void *);
""")
    manager = (root / "main/thumbnail_manager.c").read_text()
    encoder = re.search(r"void thumbnail_manager_url_encode\(.*?^}", manager, re.S | re.M).group()
    (path / "test.c").write_text(r'''
#include <assert.h>
#include "print_estimate_metadata.c"
static void (*worker)(void *);
static void *argument;
static uint32_t generation=1;
static bool success;
static unsigned requests;
static const char *expected;
uint32_t moonraker_config_generation(void){return generation;}
const char *moonraker_config_host(void){return "host";}
const char *moonraker_config_api_key(void){return "key";}
int moonraker_config_port(void){return 7125;}
int xTaskCreatePinnedToCore(void (*fn)(void *), const char *name, int stack,
                           void *arg, int priority, void *handle, int core)
{ (void)name;(void)stack;(void)priority;(void)handle;(void)core;assert(!worker);worker=fn;argument=arg;return pdPASS; }
void vTaskDelete(void *task){(void)task;}
bool moonraker_fetch_file_metadata(const char *host,int port,const char *api,const char *file,
                                  char *out,size_t size,int *code,esp_err_t *error)
{
    (void)code;(void)error;
    assert(!strcmp(host,"host")&&port==7125&&!strcmp(api,"key")&&!strcmp(file,expected));
    ++requests;
    snprintf(out,size,"{\"result\":{\"gcode_start_byte\":0,\"gcode_end_byte\":500,\"filament_total\":200,\"estimated_time\":1000}}");
    return success;
}
static void run(void){void (*fn)(void *)=worker;assert(fn);worker=NULL;fn(argument);}
int main(void)
{
    print_estimate_metadata_t result;
    expected="nested/a%20file.gcode";
    print_estimate_metadata_poll("nested/a file.gcode",1,1,100);
    run();
    assert(requests==1&&!print_estimate_metadata_get("nested/a file.gcode",1,&result));
    print_estimate_metadata_poll("nested/a file.gcode",1,1,101);
    assert(!worker);
    print_estimate_metadata_poll("nested/a file.gcode",1,1,10000100);
    success=true;run();
    assert(print_estimate_metadata_get("nested/a file.gcode",1,&result));
    assert(result.start_byte==0&&result.end_byte==500&&result.filament_total==200&&result.estimated_time==1000);
    print_estimate_metadata_poll("nested/a file.gcode",2,1,10000101);
    assert(!print_estimate_metadata_get("nested/a file.gcode",1,&result));
    print_estimate_metadata_poll("b.gcode",3,1,10000102);
    run(); /* old result cannot publish into the new job */
    assert(!print_estimate_metadata_get("b.gcode",3,&result));
    print_estimate_metadata_poll("b.gcode",3,1,10000103);
    ++generation;run(); /* stale profile must not even be queried */
    assert(requests==3&&!print_estimate_metadata_get("b.gcode",3,&result));
    print_estimate_metadata_poll("b.gcode",3,1,10000104);
    assert(!worker);
    expected="b.gcode";
    print_estimate_metadata_poll("b.gcode",3,2,10000105);
    run();
    assert(requests==4&&print_estimate_metadata_get("b.gcode",3,&result));
}
''' + "\n" + encoder + "\n")
    cjson = native / "vendor/cjson"
    subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Werror","-fsanitize=undefined",
                    "-DLV_CONF_SKIP","-DLV_KCONFIG_IGNORE","-I",str(path),"-I",str(root/"main"),
                    "-I",str(native/"shims"),"-I",str(args.lvgl_dir),"-I",str(cjson),
                    str(path/"test.c"),str(cjson/"cJSON.c"),"-lm","-o",str(path/"test")],check=True)
    subprocess.run([str(path/"test")],check=True)
print("PASS: background metadata fetch, throttled retry, job replacement, profile fencing and nested filename encoding")
