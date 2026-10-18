#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
#include "custom_theme.h"
static bool fail_async,fail_save;
static lv_result_t queue_apply(lv_async_cb_t cb,void *data){return fail_async?LV_RESULT_INVALID:lv_async_call(cb,data);}
#define lv_async_call queue_apply
#include "ui_appearance_popups.c"
#undef lv_async_call
static unsigned changes,saves,background_draws;
static void(*appearance_save_hook)(void);
static void run_save_hook(void){if(appearance_save_hook){void(*hook)(void)=appearance_save_hook;appearance_save_hook=NULL;hook();}}
static void (*s_theme_rebuild_cb)(void);
static lv_obj_t *s_settings_content;
#include "settings_scroll_owner.inc"
static void background_draw(lv_event_t*e){(void)e;background_draws++;}
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t *b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t *a){(void)a;return false;}
ui_accent_id_t theme_manager_accent(void){return ui_theme_get_accent();}
ui_density_id_t theme_manager_density(void){return ui_theme_get_density();}
ui_accessibility_t theme_manager_accessibility(void){return ui_theme_get_accessibility();}
const char *theme_manager_accent_name(ui_accent_id_t a){(void)a;return "Accent color option";}
const char *theme_manager_density_name(ui_density_id_t d){return (const char*[]){"Compact","Comfortable","Spacious"}[d];}
bool theme_manager_select_accent(ui_accent_id_t a){saves++;run_save_hook();if(fail_save)return false;ui_theme_set_accent(a);return true;}
bool theme_manager_select_density(ui_density_id_t d){saves++;run_save_hook();if(fail_save)return false;ui_theme_set_density(d);return true;}
bool theme_manager_set_accessibility(ui_accessibility_t a){saves++;run_save_hook();if(fail_save)return false;ui_theme_set_accessibility(a);return true;}
static void changed(void){assert(!s_popup&&!s_body&&!s_change_pending);changes++;ui_apply_root_style(lv_screen_active());}
static void replacement_changed(void){changes++;ui_appearance_popups_show_density(changed);lv_obj_update_layout(s_popup);}
static void transient_changed(void){changes++;ui_appearance_popups_show_density(changed);ui_appearance_popups_close_all();}
static void other_changed(void){assert(!"Displayed callback replaced by a repeated show");}
static void close_during_save(void){ui_appearance_popups_close_all();}
static void nested_during_save(void){lv_obj_send_event(s_rows[2],LV_EVENT_CLICKED,NULL);}
static uint16_t raster[1024*600];
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t*p){
 uint16_t *pixels=(uint16_t*)p;int width=lv_display_get_horizontal_resolution(d);
 for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)raster[y*width+x]=*pixels++;
 lv_display_flush_ready(d);
}
static void snapshot(unsigned theme,unsigned large,const char *name){
 const char *folder=getenv("APPEARANCE_SCREENSHOTS");if(!folder)return;
 int scroll=lv_obj_get_scroll_y(s_body);lv_obj_scroll_to_y(s_body,0,LV_ANIM_OFF);lv_refr_now(NULL);
 char path[512];snprintf(path,sizeof(path),"%s/%s-theme%u-large%u.ppm",folder,name,theme,large);
 FILE *f=fopen(path,"wb");assert(f);fputs("P6\n1024 600\n255\n",f);
 for(unsigned i=0;i<1024*600;i++){uint16_t v=raster[i];unsigned char c[3]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(c,1,3,f);}
 fclose(f);lv_obj_scroll_to_y(s_body,scroll,LV_ANIM_OFF);
}
static void pump(void){lv_tick_inc(20);lv_timer_handler();}
static void click(lv_obj_t *o){lv_obj_send_event(o,LV_EVENT_CLICKED,NULL);}
static unsigned objects(lv_obj_t*p){unsigned n=1;for(uint32_t i=0;i<lv_obj_get_child_count(p);i++)n+=objects(lv_obj_get_child(p,i));return n;}
static unsigned timers(void){unsigned n=0;lv_timer_t*t=NULL;while((t=lv_timer_get_next(t)))n++;return n;}
static void inside(lv_obj_t *o,lv_obj_t *p){lv_area_t a,b;lv_obj_get_coords(o,&a);lv_obj_get_coords(p,&b);assert(a.x1>=b.x1&&a.x2<=b.x2&&a.y1>=b.y1&&a.y2<=b.y2);}
static void layout(void){lv_obj_update_layout(s_popup);inside(s_popup,lv_screen_active());inside(s_body,s_popup);lv_obj_t*footer=lv_obj_get_child(s_popup,-1);inside(footer,s_popup);inside(lv_obj_get_child(footer,0),footer);assert(lv_obj_get_height(lv_obj_get_child(footer,0))>=48);
 lv_obj_t *title=lv_obj_get_child(s_popup,0);inside(title,s_popup);assert(lv_obj_get_style_border_side(title,0)==LV_BORDER_SIDE_BOTTOM);assert(lv_obj_get_style_border_width(title,0)>0);
 assert(lv_obj_get_scrollbar_mode(s_body)==LV_SCROLLBAR_MODE_OFF);assert(lv_obj_get_scroll_dir(s_body)==LV_DIR_VER);
 lv_area_t title_area,body_area,footer_area;lv_obj_get_coords(title,&title_area);lv_obj_get_coords(s_body,&body_area);lv_obj_get_coords(footer,&footer_area);assert(title_area.y2<body_area.y1&&body_area.y2<footer_area.y1);
 if(s_kind==APPEARANCE_ACCESSIBILITY && lv_display_get_horizontal_resolution(NULL)>=1024 && lv_display_get_vertical_resolution(NULL)>=600){
  if(lv_obj_get_scroll_y(s_body)!=0||lv_obj_get_scroll_bottom(s_body)>0)fprintf(stderr,"Accessibility overflow theme=%d density=%d large=%d high=%d y=%d bottom=%d popup=%d body=%d\n",ui_theme_get_active(),ui_theme_get_density(),ui_theme_get_accessibility().large_text,ui_theme_get_accessibility().high_contrast,(int)lv_obj_get_scroll_y(s_body),(int)lv_obj_get_scroll_bottom(s_body),(int)lv_obj_get_height(s_popup),(int)lv_obj_get_height(s_body));
  assert(lv_obj_get_scroll_y(s_body)==0&&lv_obj_get_scroll_bottom(s_body)<=0);
 }
 int label_x=lv_obj_get_x(lv_obj_get_child(lv_obj_get_child(s_body,1),0));
 for(uint32_t i=1;i<lv_obj_get_child_count(s_body);i++){lv_obj_t*row=lv_obj_get_child(s_body,i),*label=lv_obj_get_child(row,0);assert(lv_obj_get_height(row)>=48);inside(label,row);assert(lv_obj_get_x(label)==label_x);assert(lv_obj_get_style_text_align(label,0)==LV_TEXT_ALIGN_LEFT);
  if(ui_theme_is_studio()){assert(lv_obj_get_style_radius(row,0)==0);assert(lv_obj_get_style_border_side(row,0)==LV_BORDER_SIDE_BOTTOM);}
  if(s_kind==APPEARANCE_ACCESSIBILITY){
   static const char *names[]={"Large text","High contrast","Reduced transparency","Reduced motion"};
   ui_accessibility_t v=theme_manager_accessibility();bool enabled[]={v.large_text,v.high_contrast,v.reduced_transparency,v.reduced_motion};
   assert(!strcmp(lv_label_get_text(label),names[i-1]));lv_obj_t *state=lv_obj_get_child(row,1);inside(state,row);
   assert(!strcmp(lv_label_get_text(state),enabled[i-1]?"On":"Off"));lv_area_t name_area,state_area;lv_obj_get_coords(label,&name_area);lv_obj_get_coords(state,&state_area);assert(name_area.x1==state_area.x1&&name_area.y2<state_area.y1);
  }
  lv_obj_t*expected=lv_label_create(lv_layer_top());ui_apply_custom_label_style(expected,UI_FONT_BODY_LARGE,UI_TEXT);assert(lv_obj_get_style_text_font(label,0)==lv_obj_get_style_text_font(expected,0));assert(lv_color_eq(lv_obj_get_style_text_color(label,0),lv_obj_get_style_text_color(expected,0)));lv_obj_delete(expected);}
 lv_area_t before,after;lv_obj_get_coords(footer,&before);lv_refr_now(NULL);background_draws=0;lv_obj_scroll_to_y(s_body,500,LV_ANIM_OFF);lv_obj_update_layout(s_popup);lv_obj_get_coords(footer,&after);assert(!memcmp(&before,&after,sizeof(before)));lv_refr_now(NULL);assert(background_draws==0);}
static void render_dialogs(lv_display_t *d){
 if(!getenv("APPEARANCE_SCREENSHOTS"))return;
 lv_display_set_resolution(d,1024,600);
 const unsigned themes[]={UI_THEME_OPERATOR,UI_THEME_STUDIO_DARK};
 for(unsigned i=0;i<2;i++)for(unsigned large=0;large<2;large++){
  unsigned theme=themes[i];ui_theme_set_active(theme);ui_theme_set_density(UI_DENSITY_COMFORTABLE);ui_theme_set_accent(UI_ACCENT_DEFAULT);
  ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});ui_apply_root_style(lv_screen_active());
  ui_appearance_popups_show_accessibility(changed);layout();snapshot(theme,large,"accessibility");ui_appearance_popups_close_all();
  ui_appearance_popups_show_density(changed);layout();snapshot(theme,large,"density");ui_appearance_popups_close_all();
  ui_appearance_popups_show_accent(changed);layout();snapshot(theme,large,"accent");ui_appearance_popups_close_all();
 }
}
static lv_obj_t *sections(bool reordered){lv_obj_t*body=lv_obj_create(lv_screen_active());lv_obj_set_size(body,300,160);lv_obj_set_style_pad_all(body,0,0);for(unsigned i=0;i<3;i++){lv_obj_t*s=lv_obj_create(body);lv_obj_set_pos(s,0,i*200);lv_obj_set_size(s,260,190);lv_obj_t*l=lv_label_create(s);lv_label_set_text(l,(const char*[]){"Display","Connections","About"}[reordered?(i+1)%3:i]);}lv_obj_update_layout(body);return body;}
static void rebuild_sections(void){lv_obj_delete(s_settings_content);s_settings_content=NULL;s_settings_content=sections(true);}
int main(void){lv_init();lv_display_t*d=lv_display_create(1024,600);static uint8_t pixels[1024*40*2];lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);lv_obj_t*background=lv_obj_create(lv_screen_active());lv_obj_set_size(background,LV_PCT(100),LV_PCT(100));lv_obj_add_event_cb(background,background_draw,LV_EVENT_DRAW_MAIN,NULL);
for(unsigned theme=0;theme<5;theme++)for(unsigned density=0;density<3;density++)for(unsigned large=0;large<2;large++)for(unsigned size=0;size<3;size++){
 lv_display_set_resolution(d,(int[]){1024,640,480}[size],(int[]){600,480,320}[size]);ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});ui_theme_set_accent(UI_ACCENT_DEFAULT);
 unsigned objs=objects(lv_screen_active())+objects(lv_layer_top()),ts=timers(),before=changes;
 ui_appearance_popups_show_accessibility(changed);layout();lv_obj_t*popup=s_popup;ui_appearance_popups_show_accessibility(other_changed);assert(s_popup==popup);
 lv_obj_t*row=lv_obj_get_child(s_body,1);click(row);click(row);assert(s_change_pending && theme_manager_accessibility().large_text!=large);pump();assert(changes==before+1&&s_popup&&s_kind==APPEARANCE_ACCESSIBILITY);layout();assert(!strcmp(lv_label_get_text(lv_obj_get_child(lv_obj_get_child(s_body,1),1)),large?"Off":"On"));
 click(lv_obj_get_child(s_body,2));pump();assert(theme_manager_accessibility().high_contrast);layout();
 click(lv_obj_get_child(s_body,3));pump();assert(theme_manager_accessibility().reduced_transparency);layout();
 click(lv_obj_get_child(s_body,4));pump();assert(theme_manager_accessibility().reduced_motion);layout();
 lv_obj_delete(s_popup);assert(!s_popup&&!s_body&&!s_changed_cb);ui_appearance_popups_show_density(changed);layout();unsigned next=(density+1)%3;click(lv_obj_get_child(s_body,next+1));pump();assert(ui_theme_get_density()==next&&s_kind==APPEARANCE_DENSITY);layout();before=changes;click(lv_obj_get_child(s_body,next+1));pump();assert(changes==before);ui_appearance_popups_close_all();
 ui_appearance_popups_show_accent(changed);layout();fail_save=true;before=saves;click(lv_obj_get_child(s_body,2));pump();assert(saves==before+1&&!s_change_pending&&ui_theme_get_accent()==UI_ACCENT_DEFAULT&&strstr(lv_label_get_text(s_feedback),"Could not save"));fail_save=false;
 fail_async=true;before=saves;click(lv_obj_get_child(s_body,2));assert(saves==before&&!s_change_pending&&strstr(lv_label_get_text(s_feedback),"not saved"));fail_async=false;click(lv_obj_get_child(s_body,2));pump();assert(ui_theme_get_accent()==1&&s_kind==APPEARANCE_ACCENT);layout();
 click(lv_obj_get_child(lv_obj_get_child(s_popup,-1),0));pump();assert(!s_popup&&!s_body&&!s_change_pending&&objects(lv_screen_active())+objects(lv_layer_top())==objs&&timers()==ts);
}
/* Deleting a chooser while its change is queued must not revive it or
 * accidentally retire a newly requested chooser. */
ui_appearance_popups_show_accessibility(changed);unsigned before=changes;
click(lv_obj_get_child(s_body,1));lv_obj_delete(s_popup);ui_appearance_popups_show_density(changed);assert(!s_popup);pump();assert(!s_popup&&!s_change_pending&&changes==before+1);
/* Callback-created UI must keep its own kind, callback and scroll position. */
ui_appearance_popups_show_accessibility(replacement_changed);before=changes;
click(s_rows[0]);pump();assert(changes==before+1&&s_popup&&s_kind==APPEARANCE_DENSITY&&s_changed_cb==changed&&lv_obj_get_scroll_y(s_body)==0);ui_appearance_popups_close_all();
ui_appearance_popups_show_accessibility(transient_changed);before=changes;
click(s_rows[0]);pump();assert(changes==before+1&&!s_popup&&!s_body);
/* Deletion and stale action delivery cannot persist a control of another kind. */
ui_appearance_popups_show_accent(changed);ui_appearance_popups_show_density(changed);assert(s_kind==APPEARANCE_DENSITY);
before=saves;lv_obj_t *target=s_rows[ui_theme_get_density()];lv_obj_add_event_cb(target,accent_select_cb,LV_EVENT_CLICKED,(void *)(uintptr_t)1);
click(target);assert(saves==before);pump();ui_appearance_popups_close_all();
ui_appearance_popups_show_accessibility(changed);target=s_rows[0];lv_obj_delete(target);assert(!s_rows[0]);before=saves;accessibility_toggle_cb(NULL);assert(saves==before);
lv_obj_delete(s_body);assert(!s_body&&!s_rows[1]&&!s_rows[2]&&!s_rows[3]&&!s_feedback);appearance_apply_async(NULL);assert(saves==before);ui_appearance_popups_close_all();
ui_appearance_popups_show_accent(changed);ui_theme_set_accent(UI_ACCENT_DEFAULT);before=saves;appearance_save_hook=nested_during_save;click(s_rows[1]);pump();assert(saves==before+1);ui_appearance_popups_close_all();
ui_appearance_popups_show_density(changed);unsigned requested=(ui_theme_get_density()+1)%UI_DENSITY_COUNT;before=changes;appearance_save_hook=close_during_save;click(s_rows[requested]);pump();assert(changes==before+1&&!s_popup&&!s_change_pending);
ui_appearance_popups_show_accessibility(changed);before=changes;click(s_rows[0]);lv_obj_delete(s_body);pump();assert(changes==before+1&&!s_popup&&!s_body&&!s_change_pending);
ui_appearance_popups_show_density(changed);requested=(ui_theme_get_density()+1)%UI_DENSITY_COUNT;before=changes;fail_save=true;appearance_save_hook=close_during_save;click(s_rows[requested]);pump();assert(changes==before&&!s_popup&&!s_change_pending);fail_save=false;
/* Preserve section identity when Studio and other themes order cards differently. */
lv_obj_t*body=sections(false);lv_obj_scroll_to_y(body,230,LV_ANIM_OFF);ui_scroll_anchor_t saved=ui_scroll_anchor_capture(body);assert(!strcmp(saved.title,"Connections")&&saved.offset==30);lv_obj_delete(body);body=sections(true);ui_scroll_anchor_restore(body,saved);assert(lv_obj_get_scroll_y(body)==30);lv_obj_delete(body);
s_settings_content=sections(false);lv_obj_scroll_to_y(s_settings_content,230,LV_ANIM_OFF);s_theme_rebuild_cb=rebuild_sections;settings_rebuild_preserving_scroll();assert(lv_obj_get_scroll_y(s_settings_content)==30);lv_obj_delete(s_settings_content);s_settings_content=NULL;s_theme_rebuild_cb=NULL;settings_rebuild_preserving_scroll();
body=sections(false);lv_obj_scroll_to_y(body,9999,LV_ANIM_OFF);saved=ui_scroll_anchor_capture(body);saved.title[0]=0;ui_scroll_anchor_restore(body,saved);assert(lv_obj_get_scroll_y(body)<=lv_obj_get_scroll_y(body)+lv_obj_get_scroll_bottom(body));lv_obj_delete(body);
render_dialogs(d);
lv_display_delete(d);lv_deinit();puts("PASS: live appearance refresh/reopen, large text/contrast/opacity/motion, all five themes/densities/viewports, pinned Close, rapid taps, save/queue failure, no-op selection, external deletion, scroll anchor reorder and object/timer cleanup");}
