#include "ui_bed_mesh_v32.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bed_mesh_controller.h"
#include "esp_heap_caps.h"
#include "ui_button.h"
#include "ui_theme.h"
#include "ui_widgets.h"
#define CW 720
#define CH 360
#define VALUE_CAP ((size_t)BED_MESH_MAX_ROWS*BED_MESH_MAX_COLS)
typedef struct{lv_obj_t*popup,*canvas,*stats;uint16_t*buf;float*values;bed_mesh_snapshot_t mesh;float yaw,pitch,zoom,zscale;lv_point_t last;bool drag;ui_bed_mesh_command_cb_t command;} ui_t;
typedef struct{float x,y,z,sx,sy,d;} vertex_t;
typedef struct{uint16_t a,b,c;float d,z;} tri_t;
static ui_t s;
static void *alloc_psram_first(size_t n,size_t z){void*p=heap_caps_calloc(n,z,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!p)p=heap_caps_calloc(n,z,MALLOC_CAP_8BIT);return p;}
static lv_color_t heat(float z){double t=s.mesh.range>1e-8?(z-s.mesh.minimum)/s.mesh.range:.5;if(t<0)t=0;if(t>1)t=1;lv_color_t lo=lv_color_hex(0x168AAD),mid=lv_color_hex(0x30D5C8),hi=lv_color_hex(0xE45B5B);return t<.5?lv_color_mix(mid,lo,(uint8_t)(t*510)):lv_color_mix(hi,mid,(uint8_t)((t-.5)*510));}
static int cmp(const void*A,const void*B){float a=((const tri_t*)A)->d,b=((const tri_t*)B)->d;return a<b?-1:a>b?1:0;}
static void project(vertex_t*v,float scale){float cy=cosf(s.yaw),sy=sinf(s.yaw),cp=cosf(s.pitch),sp=sinf(s.pitch);float rx=v->x*cy-v->y*sy,ry=v->x*sy+v->y*cy,rz=v->z;float py=ry*cp-rz*sp,pz=ry*sp+rz*cp;v->sx=CW*.5f+rx*scale*s.zoom;v->sy=CH*.56f+py*scale*s.zoom;v->d=pz;}
static void render(void){
 if (!s.canvas || !s.mesh.valid) {
        return;
    }

    uint16_t rows = s.mesh.rows;
    uint16_t cols = s.mesh.cols;
    size_t nv = (size_t)rows * cols;
    size_t nt = (size_t)(rows - 1) * (cols - 1) * 2;
 vertex_t*v=alloc_psram_first(nv,sizeof(*v));tri_t*t=alloc_psram_first(nt,sizeof(*t));if(!v||!t){if(v)heap_caps_free(v);if(t)heap_caps_free(t);return;}
 float w=s.mesh.mesh_max_x-s.mesh.mesh_min_x,h=s.mesh.mesh_max_y-s.mesh.mesh_min_y;if(fabsf(w)<.001f)w=cols-1;if(fabsf(h)<.001f)h=rows-1;float scale=250.0f/(w>h?w:h);
 for(uint16_t y=0;y<rows;y++)for(uint16_t x=0;x<cols;x++){size_t i=(size_t)y*cols+x;v[i].x=-w*.5f+w*x/(cols-1);v[i].y=-h*.5f+h*y/(rows-1);v[i].z=s.values[i]*s.zscale;project(&v[i],scale);}
 size_t k=0;for(uint16_t y=0;y+1<rows;y++)for(uint16_t x=0;x+1<cols;x++){uint16_t a=y*cols+x,b=a+1,c=(y+1)*cols+x,d=c+1;t[k++]=(tri_t){a,b,d,(v[a].d+v[b].d+v[d].d)/3,(v[a].z+v[b].z+v[d].z)/(3*s.zscale)};t[k++]=(tri_t){a,d,c,(v[a].d+v[d].d+v[c].d)/3,(v[a].z+v[d].z+v[c].z)/(3*s.zscale)};}
 qsort(t,nt,sizeof(*t),cmp);lv_canvas_fill_bg(s.canvas,UI_CARD,LV_OPA_COVER);lv_layer_t layer;lv_canvas_init_layer(s.canvas,&layer);
 for(size_t i=0;i<nt;i++){lv_draw_triangle_dsc_t d;lv_draw_triangle_dsc_init(&d);d.p[0]=(lv_point_precise_t){v[t[i].a].sx,v[t[i].a].sy};d.p[1]=(lv_point_precise_t){v[t[i].b].sx,v[t[i].b].sy};d.p[2]=(lv_point_precise_t){v[t[i].c].sx,v[t[i].c].sy};d.opa=LV_OPA_COVER;d.grad.dir=LV_GRAD_DIR_NONE;d.grad.stops_count=1;d.grad.stops[0].color=heat(t[i].z);d.grad.stops[0].opa=LV_OPA_COVER;lv_draw_triangle(&layer,&d);}lv_canvas_finish_layer(s.canvas,&layer);lv_obj_invalidate(s.canvas);heap_caps_free(v);heap_caps_free(t);
}
static void stats(void){if(!s.stats)return;if(!s.mesh.valid){lv_label_set_text(s.stats,"No active bed mesh. Calibrate or load a profile.");return;}char b[256];snprintf(b,sizeof(b),"PROFILE %s   GRID %u x %u   LOW %+.3f mm   HIGH %+.3f mm   RANGE %.3f mm   Z %.0fx%s",s.mesh.profile_name,s.mesh.cols,s.mesh.rows,s.mesh.minimum,s.mesh.maximum,s.mesh.range,s.zscale,s.mesh.truncated?"   TRUNCATED":"");lv_label_set_text(s.stats,b);}
void ui_bed_mesh_v32_refresh(void){if(!s.popup)return;if(!s.values)s.values=alloc_psram_first(VALUE_CAP,sizeof(float));if(!s.values||!bed_mesh_controller_snapshot(&s.mesh,s.values,VALUE_CAP)){memset(&s.mesh,0,sizeof(s.mesh));stats();if(s.canvas)lv_canvas_fill_bg(s.canvas,UI_CARD,LV_OPA_COVER);return;}stats();render();}
static void close_cb(lv_event_t*e){(void)e;ui_bed_mesh_v32_close();}static void reset_cb(lv_event_t*e){(void)e;s.yaw=-.72f;s.pitch=.88f;s.zoom=1;s.zscale=20;stats();render();}static void plus_cb(lv_event_t*e){(void)e;s.zoom=fminf(3.5f,s.zoom*1.15f);render();}static void minus_cb(lv_event_t*e){(void)e;s.zoom=fmaxf(.45f,s.zoom/1.15f);render();}static void calibrate_cb(lv_event_t*e){(void)e;if(s.command)s.command("BED_MESH_CALIBRATE");}
static void canvas_cb(lv_event_t*e){lv_event_code_t code=lv_event_get_code(e);lv_indev_t*i=lv_indev_active();if(!i)return;if(code==LV_EVENT_PRESSED){lv_indev_get_point(i,&s.last);s.drag=true;}else if(code==LV_EVENT_PRESSING&&s.drag){lv_point_t p;lv_indev_get_point(i,&p);s.yaw+=(p.x-s.last.x)*.012f;s.pitch+=(p.y-s.last.y)*.01f;if(s.pitch<.15f)s.pitch=.15f;if(s.pitch>1.45f)s.pitch=1.45f;s.last=p;render();}else if(code==LV_EVENT_RELEASED||code==LV_EVENT_PRESS_LOST)s.drag=false;
#if LV_USE_GESTURE_RECOGNITION
 else if(code==LV_EVENT_GESTURE){lv_indev_gesture_type_t type=lv_event_get_gesture_type(e);if(type==LV_INDEV_GESTURE_TYPE_PINCH){float scale=lv_event_get_pinch_scale(e);if(scale>.01f){s.zoom*=scale;if(s.zoom<.45f)s.zoom=.45f;if(s.zoom>3.5f)s.zoom=3.5f;render();}}else if(type==LV_INDEV_GESTURE_TYPE_ROTATION){s.yaw+=lv_event_get_rotation(e);render();}}
#endif
}
bool ui_bed_mesh_v32_is_open(void){return s.popup!=NULL;}void ui_bed_mesh_v32_close(void){if(s.popup)lv_obj_delete(s.popup);if(s.buf)heap_caps_free(s.buf);if(s.values)heap_caps_free(s.values);memset(&s,0,sizeof(s));}
void ui_bed_mesh_v32_show(ui_bed_mesh_command_cb_t command){ui_bed_mesh_v32_close();s.command=command;s.yaw=-.72f;s.pitch=.88f;s.zoom=1;s.zscale=20;s.popup=lv_obj_create(lv_layer_top());lv_obj_set_size(s.popup,950,540);lv_obj_center(s.popup);lv_obj_clear_flag(s.popup,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_style_bg_color(s.popup,UI_BG,0);lv_obj_set_style_bg_opa(s.popup,LV_OPA_COVER,0);lv_obj_set_style_border_color(s.popup,UI_BORDER_BRIGHT,0);lv_obj_set_style_border_width(s.popup,2,0);lv_obj_set_style_radius(s.popup,18,0);lv_obj_set_style_pad_all(s.popup,18,0);
 lv_obj_t*title=lv_label_create(s.popup);lv_label_set_text(title,"BED MESH 3D");ui_apply_text_title(title);lv_obj_align(title,LV_ALIGN_TOP_LEFT,0,0);s.stats=lv_label_create(s.popup);ui_apply_text_body(s.stats);lv_obj_set_style_text_color(s.stats,UI_TEXT_DIM,0);lv_obj_set_width(s.stats,900);lv_obj_align(s.stats,LV_ALIGN_TOP_LEFT,0,38);
 s.buf=alloc_psram_first((size_t)CW*CH,sizeof(uint16_t));if(s.buf){s.canvas=lv_canvas_create(s.popup);lv_canvas_set_buffer(s.canvas,s.buf,CW,CH,LV_COLOR_FORMAT_RGB565);lv_obj_align(s.canvas,LV_ALIGN_TOP_LEFT,0,82);lv_obj_add_flag(s.canvas,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(s.canvas,canvas_cb,LV_EVENT_ALL,NULL);}lv_obj_t*hint=lv_label_create(s.popup);lv_label_set_text(hint,"DRAG TO ROTATE   •   PINCH TO ZOOM   •   LONG-PRESS BED CARD TO OPEN");ui_apply_text_caption(hint);lv_obj_set_style_text_color(hint,UI_TEXT_DIM,0);lv_obj_align(hint,LV_ALIGN_BOTTOM_LEFT,0,-2);
 lv_obj_t*close=ui_button_create(s.popup,UI_BUTTON_CLOSE,"CLOSE");lv_obj_set_size(close,110,42);lv_obj_align(close,LV_ALIGN_TOP_RIGHT,0,-4);lv_obj_add_event_cb(close,close_cb,LV_EVENT_CLICKED,NULL);lv_obj_t*reset=ui_button_create(s.popup,UI_BUTTON_OUTLINED,"RESET VIEW");lv_obj_set_size(reset,132,42);lv_obj_align(reset,LV_ALIGN_BOTTOM_RIGHT,0,0);lv_obj_add_event_cb(reset,reset_cb,LV_EVENT_CLICKED,NULL);lv_obj_t*plus=ui_button_create(s.popup,UI_BUTTON_OUTLINED,"+");lv_obj_set_size(plus,52,42);lv_obj_align(plus,LV_ALIGN_BOTTOM_RIGHT,-142,0);lv_obj_add_event_cb(plus,plus_cb,LV_EVENT_CLICKED,NULL);lv_obj_t*minus=ui_button_create(s.popup,UI_BUTTON_OUTLINED,"-");lv_obj_set_size(minus,52,42);lv_obj_align(minus,LV_ALIGN_BOTTOM_RIGHT,-202,0);lv_obj_add_event_cb(minus,minus_cb,LV_EVENT_CLICKED,NULL);lv_obj_t*cal=ui_button_create(s.popup,UI_BUTTON_PRIMARY,"CALIBRATE");lv_obj_set_size(cal,130,42);lv_obj_align(cal,LV_ALIGN_BOTTOM_RIGHT,-264,0);lv_obj_add_event_cb(cal,calibrate_cb,LV_EVENT_CLICKED,NULL);ui_bed_mesh_v32_refresh();}
