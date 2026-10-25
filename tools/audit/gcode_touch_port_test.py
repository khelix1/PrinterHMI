#!/usr/bin/env python3
"""Compile the actual LVGL9 raw touch reader with fake GT911 data."""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
source=(root/'components/espressif__esp_lvgl_port/src/lvgl9/esp_lvgl_port_touch.c').read_text()
match=re.search(r'^static void lvgl_port_touchpad_read\(lv_indev_t \*indev_drv, lv_indev_data_t \*data\)\n\{.*?^\}',source,re.M|re.S)
assert match
prefix='''#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define CONFIG_ESP_LCD_TOUCH_MAX_POINTS 5
#define CONFIG_LV_USE_GESTURE_RECOGNITION 0
#define ESP_ERROR_CHECK(x) assert((x)==0)
#define LV_INDEV_STATE_PRESSED 1
#define LV_INDEV_STATE_RELEASED 0
typedef struct {int32_t x,y;} point_t;
typedef struct {point_t point;int state;} lv_indev_data_t;
typedef struct {void *driver_data;} lv_indev_t;
typedef point_t esp_lcd_touch_point_data_t;
typedef struct {void *handle;struct {float x,y;} scale;} lvgl_port_touch_ctx_t;
static void *lv_indev_get_driver_data(lv_indev_t *i){return i->driver_data;}
static uint8_t count;
static unsigned bed_calls,viewer_calls;
static int seen[2][5];
static int esp_lcd_touch_read_data(void *h){assert(h);return 0;}
static int esp_lcd_touch_get_data(void *h,point_t *points,uint8_t *n,int capacity){assert(h&&capacity==5);*n=count;for(unsigned i=0;i<count;i++)points[i]=(point_t){10+(int)i*20,30+(int)i*20};return 0;}
static void bed(uint8_t n,int32_t x,int32_t y,int32_t a,int32_t b){bed_calls++;int values[]={n,x,y,a,b};memcpy(seen[0],values,sizeof(values));}
static void viewer(uint8_t n,int32_t x,int32_t y,int32_t a,int32_t b){viewer_calls++;int values[]={n,x,y,a,b};memcpy(seen[1],values,sizeof(values));}
static void (*printerhmi_bed_mesh_multitouch_update)(uint8_t,int32_t,int32_t,int32_t,int32_t)=bed;
static void (*printerhmi_gcode_viewer_multitouch_update)(uint8_t,int32_t,int32_t,int32_t,int32_t)=viewer;
'''
main='''
int main(void){
 lvgl_port_touch_ctx_t ctx={.handle=(void*)1,.scale={2,3}};lv_indev_t input={&ctx};lv_indev_data_t data={0};
 for(count=0;count<=5;count++){
  lvgl_port_touchpad_read(&input,&data);assert(!memcmp(seen[0],seen[1],sizeof(seen[0])));assert(seen[0][0]==(count>2?2:count));
  if(count){assert(data.state==LV_INDEV_STATE_PRESSED&&data.point.x==20&&data.point.y==90);assert(seen[0][1]==20&&seen[0][2]==90);}
  else assert(data.state==LV_INDEV_STATE_RELEASED);
  if(count>1)assert(seen[0][3]==60&&seen[0][4]==150);
  else assert(!seen[0][3]&&!seen[0][4]);
 }
 unsigned previous=bed_calls;printerhmi_bed_mesh_multitouch_update=NULL;count=2;lvgl_port_touchpad_read(&input,&data);assert(bed_calls==previous&&viewer_calls==previous+1);
 printerhmi_bed_mesh_multitouch_update=bed;printerhmi_gcode_viewer_multitouch_update=NULL;lvgl_port_touchpad_read(&input,&data);assert(bed_calls==previous+1);
 printerhmi_bed_mesh_multitouch_update=NULL;lvgl_port_touchpad_read(&input,&data);assert(data.state==LV_INDEV_STATE_PRESSED);
 puts("PASS: actual touch-reader body forwards scaled 0/1/2/5-point input to both optional observers and preserves LVGL pointer state");
}
'''
with tempfile.TemporaryDirectory(prefix='gcode-touch-') as d:
 p=Path(d);(p/'test.c').write_text(prefix+match.group(0)+main)
 subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
