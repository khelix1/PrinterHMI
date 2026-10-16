/* Actual pinned PPA dispatcher/cache handlers; ESP cache/DMA are deterministic
 * host models. Device cache coherency and panel acceptance remain separate. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "lv_draw_ppa_private.h"
static lv_draw_task_t *available;
static unsigned available_calls, dispatch_requests;
static lv_draw_task_t *audit_available(lv_layer_t *layer, lv_draw_task_t *previous, uint8_t id)
{ (void)layer;(void)previous;assert(id==80);++available_calls;return available; }
static void *audit_alloc(lv_layer_t *layer){return layer->draw_buf->data;}
static void audit_request(void){++dispatch_requests;}
#define lv_draw_get_available_task audit_available
#define lv_draw_layer_alloc_buf audit_alloc
#define lv_draw_dispatch_request audit_request
#include "lv_draw_ppa.c"
#undef lv_draw_get_available_task
#undef lv_draw_layer_alloc_buf
#undef lv_draw_dispatch_request
#include "lv_draw_ppa_buf.c"
static uint8_t cache_data[256] __attribute__((aligned(64)));
static uint8_t physical[256];
static unsigned sync_calls;
static struct { size_t start,length;int flags; } syncs[8];
static size_t line_size=64;
size_t esp_cache_get_line_size_by_addr(const void *address){(void)address;return line_size;}
esp_err_t esp_cache_msync(void *address,size_t length,int flags)
{
 size_t start=(uint8_t *)address-cache_data;
 assert(start%64==0 && length%64==0 && start+length<=sizeof(cache_data));
 assert(sync_calls<8);syncs[sync_calls].start=start;syncs[sync_calls].length=length;syncs[sync_calls++].flags=flags;
 if(flags&ESP_CACHE_MSYNC_FLAG_DIR_C2M)memcpy(physical+start,cache_data+start,length);
 else {assert(flags&ESP_CACHE_MSYNC_FLAG_DIR_M2C);memcpy(cache_data+start,physical+start,length);}
 return ESP_OK;
}
esp_err_t ppa_register_client(const ppa_client_config_t *cfg,ppa_client_handle_t *client){(void)cfg;*client=(void *)1;return ESP_OK;}
esp_err_t ppa_unregister_client(ppa_client_handle_t client){(void)client;return ESP_OK;}
void lv_draw_ppa_fill(lv_draw_task_t *task,const lv_draw_fill_dsc_t *dsc,const lv_area_t *coords)
{
 (void)dsc;assert(sync_calls==1 && syncs[0].flags==(ESP_CACHE_MSYNC_FLAG_DIR_C2M|ESP_CACHE_MSYNC_FLAG_TYPE_DATA));
 lv_layer_t *layer=task->target_layer;
 for(int y=coords->y1;y<=coords->y2;y++)for(int x=coords->x1;x<=coords->x2;x++){
  size_t p=(size_t)(y-layer->buf_area.y1)*32+(size_t)(x-layer->buf_area.x1)*2;
  physical[p]=physical[p+1]=0x77;
 }
}
void lv_draw_ppa_img(lv_draw_task_t *t,const lv_draw_image_dsc_t *d,const lv_area_t *a){(void)t;(void)d;(void)a;assert(0);}
int main(void)
{
 lv_init();lv_draw_buf_ppa_init_handlers();
 lv_draw_buf_t buffer;assert(lv_draw_buf_init(&buffer,16,8,LV_COLOR_FORMAT_RGB565,32,cache_data,sizeof(cache_data))==LV_RESULT_OK);
 lv_layer_t layer={0};layer.draw_buf=&buffer;layer.buf_area=(lv_area_t){100,200,115,207};layer.color_format=LV_COLOR_FORMAT_RGB565;
 lv_draw_fill_dsc_t fill;lv_draw_fill_dsc_init(&fill);fill.opa=LV_OPA_COVER;fill.base.layer=&layer;
 lv_draw_task_t task={0};task.target_layer=&layer;task.type=LV_DRAW_TASK_TYPE_FILL;task.draw_dsc=&fill;
 task.area=(lv_area_t){100,201,110,206};task.clip_area=(lv_area_t){102,203,105,204};task.preferred_draw_unit_id=80;
 task.preference_score=100;
 lv_draw_ppa_unit_t unit={0};task.draw_unit=&unit.base_unit;unit.task_act=&task;
 memset(cache_data,0x11,sizeof(cache_data));memset(physical,0x55,sizeof(physical));
 ppa_execute_drawing(&unit);
 assert(sync_calls==2 && syncs[0].start==64 && syncs[0].length==128);
 assert(syncs[1].start==64 && syncs[1].length==128 && syncs[1].flags==(ESP_CACHE_MSYNC_FLAG_DIR_M2C|ESP_CACHE_MSYNC_FLAG_TYPE_DATA));
 for(unsigned y=0;y<8;y++)for(unsigned x=0;x<16;x++){
  uint8_t expected=(y>=3&&y<=4&&x>=2&&x<=5)?0x77:0x11;
  assert(cache_data[y*32+x*2]==expected && cache_data[y*32+x*2+1]==expected);
 }
 /* A subsequent CPU glyph update + writeback must preserve hardware pixels. */
 cache_data[64]=0x33;esp_cache_msync(cache_data,256,ESP_CACHE_MSYNC_FLAG_DIR_C2M|ESP_CACHE_MSYNC_FLAG_TYPE_DATA);
 assert(physical[64]==0x33 && physical[3*32+4]==0x77);
 /* Flush full rows, clamp clipped ranges, and skip empty/noncached requests. */
 sync_calls=0;lv_area_t relative={0,-7,15,0};flush_cache(&buffer,&relative);assert(sync_calls==1&&syncs[0].start==0&&syncs[0].length==64);
 relative=(lv_area_t){0,8,15,20};flush_cache(&buffer,&relative);assert(sync_calls==1);
 line_size=0;invalidate_cache(&buffer,NULL);assert(sync_calls==1);line_size=64;
 sync_calls=0;invalidate_cache(&buffer,NULL);assert(sync_calls==1&&syncs[0].length==256);
 /* A software task in the layer must finish before PPA touches shared rows. */
 lv_draw_unit_t other_unit={0};lv_draw_task_t other={0};other.draw_unit=&other_unit;other.state=LV_DRAW_TASK_STATE_IN_PROGRESS;other.next=&task;layer.draw_task_head=&other;
 unit.task_act=NULL;available=&task;assert(ppa_dispatch(&unit.base_unit,&layer)==LV_DRAW_UNIT_IDLE && !available_calls);
 other.state=LV_DRAW_TASK_STATE_FINISHED;assert(!other_unit_busy(&layer,&unit.base_unit));
 other.state=LV_DRAW_TASK_STATE_IN_PROGRESS;other.draw_unit=&unit.base_unit;assert(!other_unit_busy(&layer,&unit.base_unit));
 other.state=LV_DRAW_TASK_STATE_FINISHED;sync_calls=0;
 assert(ppa_dispatch(&unit.base_unit,&layer)==1 && available_calls==1 && dispatch_requests==1 && task.state==LV_DRAW_TASK_STATE_FINISHED && !unit.task_act);
 /* Opaque rectangular fills stay eligible; gradients/rounded fills do not. */
 assert(ppa_evaluate(&unit.base_unit,&task)==1);fill.radius=2;assert(!ppa_evaluate(&unit.base_unit,&task));fill.radius=0;fill.opa=128;assert(!ppa_evaluate(&unit.base_unit,&task));
 lv_deinit();puts("PASS: actual PPA cache/dispatcher code: C2M-before/M2C-after, clipped layer-relative rows, neighbor glyph/pixel preservation, bounds/noncached handling and software-unit exclusion");
}
