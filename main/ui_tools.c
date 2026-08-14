#include "ui_tools.h"
#include "lvgl.h"
#include "ui_theme.h"
#include "ui_page_geometry.h"
#include "ui_page_title.h"
#include "ui_button.h"
#include "ui_shell.h"
static lv_obj_t *s_root;
static ui_tools_open_cb_t s_callbacks[4];
static void tile_cb(lv_event_t *e) { int n=(int)(intptr_t)lv_event_get_user_data(e); if (lv_event_get_code(e)==LV_EVENT_CLICKED && n>=0 && n<4 && s_callbacks[n]) s_callbacks[n](); }
static void add_tile(const char *title,const char *body,int x,int y,int index) { lv_obj_t *b=ui_button_create_empty(s_root,UI_BUTTON_OUTLINED); lv_obj_set_size(b,380,172); lv_obj_set_pos(b,x,y); lv_obj_t *l=ui_button_create_label(b,title); lv_obj_set_pos(l,24,24); lv_obj_t *d=lv_label_create(b); lv_label_set_text(d,body); ui_apply_text_body(d); lv_obj_set_pos(d,24,78); lv_obj_add_event_cb(b,tile_cb,LV_EVENT_CLICKED,(void*)(intptr_t)index); }
void ui_tools_set_callbacks(ui_tools_open_cb_t c,ui_tools_open_cb_t m,ui_tools_open_cb_t d,ui_tools_open_cb_t a){s_callbacks[0]=c;s_callbacks[1]=m;s_callbacks[2]=d;s_callbacks[3]=a;}
void ui_tools_show(void){if(s_root){lv_obj_clear_flag(s_root,LV_OBJ_FLAG_HIDDEN);lv_obj_move_foreground(s_root);return;}s_root=lv_obj_create(lv_screen_active());lv_obj_set_size(s_root,UI_PAGE_ROOT_WIDTH,UI_PAGE_ROOT_HEIGHT);lv_obj_set_pos(s_root,UI_PAGE_ROOT_X,UI_PAGE_ROOT_Y);lv_obj_clear_flag(s_root,LV_OBJ_FLAG_SCROLLABLE);ui_apply_root_style(s_root);ui_page_title_create(s_root,LV_SYMBOL_SETTINGS " TOOLS","Calibration and operator utilities");add_tile("CALIBRATION","Tune and validate your printer",20,88,0);add_tile("BED MESH","Probe and visualize bed mesh",420,88,1);add_tile("DEVICES","Manage connected devices",20,276,2);add_tile("MACROS","Run printer macros",420,276,3);}
void ui_tools_hide(void){if(s_root)lv_obj_add_flag(s_root,LV_OBJ_FLAG_HIDDEN);ui_shell_raise_topbar();ui_shell_raise_nav();}
