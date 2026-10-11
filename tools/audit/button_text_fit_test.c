#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "custom_theme.h"
#include "ui_button.h"
#include "ui_text_fit.h"
#include "ui_printer_info_cards.h"
#include "ui_theme.h"
#include "ui_widgets.h"
#include "ui_popup.h"
#include "ui_settings_components.h"
#include "firmware_identity.h"
#include "ui_selector_style.h"
#include "ui_printer_actions.h"
#include "ui_command_bar.h"
#include "ui_calibration_layout.h"
#include "ui_studio_layout.h"
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t*b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t*a){(void)a;return false;}
void ui_command_bar_action(const char*s){(void)s;}
static int theme,density,large;static unsigned checks;
static uint16_t raster[1024*600];
static void flush(lv_display_t*d,const lv_area_t*a,uint8_t*p){uint16_t*v=(uint16_t*)p;for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)raster[y*1024+x]=*v++;lv_display_flush_ready(d);}
static void snapshot(const char *name){const char*folder=getenv("BUTTON_TEXT_SCREENSHOTS");if(!folder)return;lv_refr_now(NULL);char path[512];snprintf(path,sizeof(path),"%s/%s-theme%d.ppm",folder,name,theme);FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n1024 600\n255\n");for(int i=0;i<1024*600;i++){uint16_t v=raster[i];unsigned char c[3]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(c,1,3,f);}fclose(f);}
static bool overlap(lv_obj_t*a,lv_obj_t*b){lv_area_t x,y;lv_obj_get_coords(a,&x);lv_obj_get_coords(b,&y);return x.x1<=y.x2&&y.x1<=x.x2&&x.y1<=y.y2&&y.y1<=x.y2;}
static void inside(lv_obj_t*c,lv_obj_t*p,const char*where){lv_area_t x,y;lv_obj_get_coords(c,&x);lv_obj_get_coords(p,&y);if(x.x1<y.x1||x.x2>y.x2||x.y1<y.y1||x.y2>y.y2){fprintf(stderr,"OUTSIDE %s theme%d density%d large%d [%d,%d,%d,%d] vs [%d,%d,%d,%d]\n",where,theme,density,large,x.x1,x.y1,x.x2,x.y2,y.x1,y.y1,y.x2,y.y2);abort();}}
static void pressed_frame(lv_obj_t *button)
{
    lv_state_t original=lv_obj_get_state(button);
    int border=lv_obj_get_style_border_width(button,0),radius=lv_obj_get_style_radius(button,0);
    lv_area_t before,after;lv_obj_get_coords(button,&before);
    lv_obj_add_state(button,LV_STATE_PRESSED|LV_STATE_FOCUSED);
    lv_obj_update_layout(button);lv_obj_get_coords(button,&after);
    assert(!memcmp(&before,&after,sizeof(before)));
    assert(lv_obj_get_style_border_width(button,0)==border);
    assert(lv_obj_get_style_radius(button,0)==radius);
    assert(!lv_obj_get_style_transform_width(button,0));
    assert(!lv_obj_get_style_transform_height(button,0));
    assert(!lv_obj_get_style_translate_y(button,0));
    assert(!lv_obj_get_style_outline_width(button,0));
    lv_obj_remove_state(button,(LV_STATE_PRESSED|LV_STATE_FOCUSED)&~original);
}
static void centered(lv_obj_t *button,const char *where)
{
    unsigned count=lv_obj_get_child_count(button);
    if(!count)return;
    lv_area_t surface,group;lv_obj_get_coords(button,&surface);
    lv_obj_get_coords(lv_obj_get_child(button,0),&group);
    if(count==1 && lv_obj_check_type(lv_obj_get_child(button,0),&lv_label_class)) {
        if(lv_obj_get_style_text_align(lv_obj_get_child(button,0),0)!=LV_TEXT_ALIGN_CENTER){fprintf(stderr,"LEFT ALIGNED %s theme%d density%d large%d\n",where,theme,density,large);abort();}
    } else if(count==2 && (lv_obj_get_style_flex_flow(button,0)==LV_FLEX_FLOW_ROW ||
                          lv_obj_get_style_flex_flow(button,0)==LV_FLEX_FLOW_ROW_REVERSE)) {
        lv_area_t second;lv_obj_get_coords(lv_obj_get_child(button,1),&second);
        if(second.x1<group.x1)group.x1=second.x1;
        if(second.x2>group.x2)group.x2=second.x2;
        if(second.y1<group.y1)group.y1=second.y1;
        if(second.y2>group.y2)group.y2=second.y2;
        assert(lv_obj_get_style_text_align(lv_obj_get_child(button,1),0)==LV_TEXT_ALIGN_CENTER);
    } else return;
    if(abs((group.x1+group.x2)-(surface.x1+surface.x2))>3 ||
       abs((group.y1+group.y2)-(surface.y1+surface.y2))>3) {
        fprintf(stderr,"OFF CENTER %s theme%d density%d large%d group[%d,%d,%d,%d] surface[%d,%d,%d,%d]\n",where,theme,density,large,group.x1,group.y1,group.x2,group.y2,surface.x1,surface.y1,surface.x2,surface.y2);abort();
    }
}
static void fit(lv_obj_t*b,const char*where){lv_obj_update_layout(b);centered(b,where);pressed_frame(b);lv_obj_update_layout(b);for(unsigned i=0;i<lv_obj_get_child_count(b);i++){lv_obj_t*l=lv_obj_get_child(b,i);if(!lv_obj_check_type(l,&lv_label_class))continue;inside(l,b,where);assert(lv_label_get_long_mode(l)!=LV_LABEL_LONG_DOT);const char *text=lv_label_get_text(l);bool atomic=true;unsigned words=0;bool in_word=false;for(const unsigned char*q=(const unsigned char*)text;*q;q++){bool word=(*q>='A'&&*q<='Z')||(*q>='a'&&*q<='z')||(*q>='0'&&*q<='9')||*q=='.'||*q=='-'||*q=='+';if(word&&!in_word)words++;in_word=word;}atomic=words<=1||!strcmp(text,"PROBE / Z")||!strcmp(text,"1 mm")||!strcmp(text,"10 mm")||!strcmp(text,"50 mm");if(atomic){lv_point_t natural;lv_text_get_size(&natural,text,lv_obj_get_style_text_font(l,0),lv_obj_get_style_text_letter_space(l,0),lv_obj_get_style_text_line_space(l,0),LV_COORD_MAX,LV_TEXT_FLAG_NONE);if(lv_obj_get_height(l)>lv_obj_get_style_text_font(l,0)->line_height||natural.x>lv_obj_get_width(l)){fprintf(stderr,"UNWANTED WRAP/CLIP %s [%s] theme%d density%d large%d natural%d available%d\n",where,text,theme,density,large,natural.x,lv_obj_get_width(l));abort();}}if(i==lv_obj_get_child_count(b)-1)assert(lv_obj_get_style_text_font(l,0)->line_height>=UI_FONT_BODY->line_height);}if(lv_obj_get_child_count(b)==2)assert(!overlap(lv_obj_get_child(b,0),lv_obj_get_child(b,1)));checks++;}
static void row(const char*t,const char*d,const char*text){lv_obj_t*s=ui_settings_section_create(lv_screen_active(),"SETTINGS",190,48+UI_SETTINGS_ACTION_HEIGHT+6);lv_obj_t*b=ui_settings_section_add_action_row(s,t,d,text,48,NULL,false);lv_obj_update_layout(s);lv_obj_t*r=lv_obj_get_parent(b),*copy=lv_obj_get_child(r,0);fit(b,text);inside(b,r,text);inside(copy,r,t);assert(!overlap(copy,b));for(unsigned i=0;i<lv_obj_get_child_count(copy);i++)inside(lv_obj_get_child(copy,i),copy,t);if(density==1&&large&&!strcmp(text,"BACKUP / RESTORE"))snapshot("settings");lv_obj_delete(s);}
static void footer(const char*text,int32_t width){lv_obj_t*p=ui_popup_create(lv_layer_top(),560,320,UI_POPUP_STANDARD);lv_obj_t*a=ui_popup_add_footer_action(p,UI_POPUP_ACTION_CANCEL,"BACK",160,UI_POPUP_FOOTER_LEFT,NULL,NULL,NULL);lv_obj_t*b=ui_popup_add_footer_action(p,UI_POPUP_ACTION_PRIMARY,text,width,UI_POPUP_FOOTER_RIGHT,NULL,NULL,NULL);lv_obj_update_layout(p);fit(a,"footer back");fit(b,text);inside(a,p,text);inside(b,p,text);assert(!overlap(a,b));lv_obj_delete(p);}
typedef struct {bool bed_mesh,screws_tilt,z_tilt,quad_gantry_level,axis_twist;} calibration_capabilities_t;
typedef struct {lv_obj_t*bed_mesh_button,*screws_tilt_button,*gantry_level_button,*axis_twist_button;ui_calibration_card_refs_t bed;} fixture_state_t;
static fixture_state_t state,*s_calibration=&state;
#include "bed_actions.inc"
static void bed(void){for(unsigned mask=1;mask<16;mask++){lv_obj_t*card=lv_obj_create(lv_screen_active());lv_obj_remove_style_all(card);lv_obj_set_size(card,390,190);memset(&state,0,sizeof(state));state.bed.summary=lv_label_create(card);lv_obj_set_size(state.bed.summary,358,24);lv_obj_set_pos(state.bed.summary,16,58);state.bed.status=lv_label_create(card);lv_obj_t*bs[4];const char*names[]={"BED MESH","SCREWS","LEVEL","TWIST"};for(int i=0;i<4;i++)bs[i]=ui_button_create(card,UI_BUTTON_OUTLINED,names[i]);state.bed_mesh_button=bs[0];state.screws_tilt_button=bs[1];state.gantry_level_button=bs[2];state.axis_twist_button=bs[3];calibration_capabilities_t caps={.bed_mesh=mask&1,.screws_tilt=mask&2,.z_tilt=mask&4,.axis_twist=mask&8};layout_bed_geometry_actions(&caps);lv_obj_update_layout(card);for(int i=0;i<4;i++)if(mask&(1<<i)){fit(bs[i],names[i]);inside(bs[i],card,names[i]);assert(!overlap(bs[i],state.bed.summary));for(int j=i+1;j<4;j++)if(mask&(1<<j))assert(!overlap(bs[i],bs[j]));}lv_obj_delete(card);}}
static void groups(void){
#include "settings_cases.inc"
lv_obj_t*p=lv_obj_create(lv_screen_active());lv_obj_remove_style_all(p);lv_obj_set_size(p,ui_theme_is_studio()?976:810,54);ui_printer_actions_t a;ui_printer_actions_create(p,&a,NULL,NULL);lv_obj_update_layout(p);for(unsigned i=0;i<lv_obj_get_child_count(p);i++){lv_obj_t*b=lv_obj_get_child(p,i);fit(b,"printer actions");inside(b,p,"printer actions");for(unsigned j=i+1;j<lv_obj_get_child_count(p);j++)assert(!overlap(b,lv_obj_get_child(p,j)));}lv_obj_delete(p);
p=ui_command_bar_create(lv_screen_active(),0,0,800,90);lv_obj_update_layout(p);for(unsigned i=0;i<lv_obj_get_child_count(p);i++){lv_obj_t*b=lv_obj_get_child(p,i);fit(b,"dashboard command");inside(b,p,"dashboard command");}lv_obj_delete(p);
const char*icons[]={LV_SYMBOL_HOME,LV_SYMBOL_LIST,LV_SYMBOL_FILE,LV_SYMBOL_IMAGE,LV_SYMBOL_SETTINGS,LV_SYMBOL_EDIT,LV_SYMBOL_LOOP,LV_SYMBOL_SETTINGS};const char*nav[]={"Dashboard","Printer","Files","Camera","Tools","Console","Drybox","Settings"};p=lv_obj_create(lv_screen_active());lv_obj_remove_style_all(p);lv_obj_set_size(p,170,528);for(int i=0;i<8;i++){lv_obj_t*b=ui_create_operator_nav_button(p,8,8+i*58,156,52,icons[i],nav[i]);fit(b,nav[i]);inside(b,p,nav[i]);lv_area_t surface,icon_area,text_area;lv_obj_get_coords(b,&surface);lv_obj_get_coords(lv_obj_get_child(b,0),&icon_area);lv_obj_get_coords(lv_obj_get_child(b,1),&text_area);assert(abs(surface.x1+surface.x2-icon_area.x1-text_area.x2)<=2);assert(lv_obj_get_style_text_align(lv_obj_get_child(b,1),0)==LV_TEXT_ALIGN_CENTER);}if(density==1&&large)snapshot("navigation");lv_obj_delete(p);bed();}
/* Construct the actual Settings firmware block with every channel and a
 * modified revision at the largest label lengths used by current builds. */
const char *firmware_identity_channel_name(firmware_channel_t c)
{
    return c == FIRMWARE_CHANNEL_STABLE ? "Stable" :
        c == FIRMWARE_CHANNEL_NIGHTLY ? "Nightly" : "Custom";
}
static void firmware_settings(void)
{
    for(int channel=0;channel<3;channel++) {
        firmware_identity_t identity={.channel=(firmware_channel_t)channel};
        const char *firmware_version="nightly-2026-10-11-abcdef123456";
        const char *image_build="Oct 11 2026  13:30:51";
        const char *source_revision="abcdef123456 (modified)";
        const int first_row_y=48,row_height=UI_SETTINGS_ROW_HEIGHT;
        const int action_height=UI_SETTINGS_ACTION_HEIGHT,section_y=0;
        lv_event_cb_t ota_cb=NULL;
        lv_obj_t *content=lv_obj_create(lv_screen_active());
        lv_obj_remove_style_all(content);lv_obj_set_size(content,1024,240);
#include "firmware_settings.inc"
        lv_obj_update_layout(content);
        assert(lv_obj_get_height(firmware)>=firmware_row_5+action_height);
        for(unsigned i=0;i<lv_obj_get_child_count(firmware);i++) {
            lv_obj_t *child=lv_obj_get_child(firmware,i);
            if(lv_obj_get_child_count(child)<3)continue;
            for(unsigned j=0;j<lv_obj_get_child_count(child);j++)
                inside(lv_obj_get_child(child,j),child,"Firmware identity row");
        }
        assert(lv_obj_get_scroll_bottom(content)>0);
        lv_obj_scroll_to_y(content,lv_obj_get_scroll_bottom(content),LV_ANIM_OFF);
        lv_obj_update_layout(content);assert(lv_obj_get_scroll_bottom(content)<=1);
        lv_obj_delete(content);
    }
}
static void value_rows(void){lv_obj_t*s=ui_settings_section_create(lv_screen_active(),"DISPLAY",0,160);lv_obj_t*v=ui_settings_section_add_row(s,"Display Density","Choose compact, comfortable, or spacious type and spacing","COMFORTABLE",48,NULL);lv_obj_update_layout(s);lv_obj_t*r=lv_obj_get_parent(v);for(unsigned i=0;i<lv_obj_get_child_count(r);i++)inside(lv_obj_get_child(r,i),r,"Settings value row");assert(!overlap(lv_obj_get_child(r,1),v));lv_obj_delete(s);}
static lv_point_t selector_touch;
static bool selector_down;
static unsigned selector_changes;
static void selector_read(lv_indev_t *input,lv_indev_data_t *data)
{
    (void)input;data->point=selector_touch;
    data->state=selector_down?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;
}
static void selector_changed(lv_event_t *event){(void)event;selector_changes++;}
static void selector_tap(lv_indev_t *input,int x,int y)
{
    selector_touch=(lv_point_t){x,y};selector_down=true;lv_indev_read(input);
    selector_down=false;lv_indev_read(input);
}
static void selector_controls(void)
{
    lv_indev_t *input=lv_indev_create();lv_indev_set_type(input,LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(input,selector_read);
    lv_obj_t *field=lv_dropdown_create(lv_screen_active());
    lv_obj_set_pos(field,80,70);lv_obj_set_size(field,260,48);
    lv_dropdown_set_options(field,"All entries\nErrors + warnings\nErrors\nWarnings\nCommands\nResponses\nSystem");
    ui_selector_style(field,UI_FONT_CAPTION);
    lv_obj_add_event_cb(field,selector_changed,LV_EVENT_VALUE_CHANGED,NULL);
    lv_obj_update_layout(field);assert(lv_obj_get_height(field)>=44);
    assert(lv_obj_get_width(field)==260 && lv_obj_get_x(field)==80 && lv_obj_get_y(field)==70);
    lv_area_t a,b;lv_obj_get_coords(field,&a);
    lv_obj_add_state(field,LV_STATE_PRESSED|LV_STATE_FOCUSED);
    lv_obj_update_layout(field);lv_obj_get_coords(field,&b);assert(!memcmp(&a,&b,sizeof(a)));
    assert(!lv_obj_get_style_outline_width(field,0));
    lv_obj_remove_state(field,LV_STATE_PRESSED|LV_STATE_FOCUSED);
    lv_obj_add_state(field,LV_STATE_FOCUS_KEY);
    assert(lv_obj_get_style_outline_width(field,0)==2);
    lv_obj_remove_state(field,LV_STATE_FOCUS_KEY);
    for(unsigned selected=0;selected<7;selected++) {
        selector_tap(input,(a.x1+a.x2)/2,(a.y1+a.y2)/2);
        assert(lv_dropdown_is_open(field));
        lv_obj_t *list=lv_dropdown_get_list(field);lv_obj_update_layout(list);
        inside(list,lv_screen_active(),"Opened selector");
        const lv_font_t *font=lv_obj_get_style_text_font(list,0);
        int pitch=font->line_height+lv_obj_get_style_text_line_space(list,0);
        assert(pitch>=44 && lv_obj_get_style_text_font(list,LV_PART_SELECTED)==font);
        assert(lv_obj_get_style_text_line_space(list,LV_PART_SELECTED)==pitch-font->line_height);
        assert(lv_obj_get_style_radius(list,0)==6 && !lv_obj_get_style_shadow_width(list,0));
        lv_obj_t *label=lv_obj_get_child(list,0);lv_area_t text;lv_obj_get_coords(label,&text);
        if(selected==0 && density==1 && large)snapshot("selectors");
        unsigned before=selector_changes;
        selector_tap(input,text.x1+20,text.y1+(int)selected*pitch+font->line_height/2);
        assert(!lv_dropdown_is_open(field) && lv_dropdown_get_selected(field)==selected);
        assert(selector_changes==before+1);
    }
    lv_obj_add_state(field,LV_STATE_DISABLED);
    selector_tap(input,(a.x1+a.x2)/2,(a.y1+a.y2)/2);
    assert(!lv_dropdown_is_open(field));
    lv_obj_delete(field);lv_indev_delete(input);
}
static void dynamic(void){lv_obj_t *reverse=ui_button_create_icon(lv_screen_active(),UI_BUTTON_OUTLINED,LV_SYMBOL_LEFT,"BACK",UI_TEXT,UI_BUTTON_ICON_HORIZONTAL_REVERSE);lv_obj_set_size(reverse,220,60);fit(reverse,"reverse icon action");lv_obj_delete(reverse);lv_obj_t*p=lv_obj_create(lv_screen_active());lv_obj_remove_style_all(p);lv_obj_set_size(p,800,100);lv_obj_set_flex_flow(p,LV_FLEX_FLOW_ROW_WRAP);lv_obj_t*b=ui_button_create_icon(p,UI_BUTTON_OUTLINED,LV_SYMBOL_REFRESH,"REFRESH",UI_TEXT,UI_BUTTON_ICON_HORIZONTAL);lv_obj_set_size(b,LV_SIZE_CONTENT,LV_SIZE_CONTENT);lv_obj_set_style_min_height(b,44,0);fit(b,"intrinsic toolbar");lv_obj_set_size(b,220,60);fit(b,"bounded toolbar");lv_label_set_text(lv_obj_get_child(b,1),"REFRESH FILES");fit(b,"changed icon label");lv_label_set_text(lv_obj_get_child(b,1),"REFRESH");fit(b,"restored icon label");lv_obj_set_size(b,LV_SIZE_CONTENT,LV_SIZE_CONTENT);fit(b,"intrinsic again");lv_obj_delete(p);
b=ui_button_create(lv_screen_active(),UI_BUTTON_OUTLINED,"TEMPS ON");lv_obj_set_size(b,156,40);fit(b,"TEMPS ON");lv_label_set_text(lv_obj_get_child(b,0),"TEMPS OFF");fit(b,"TEMPS OFF");lv_obj_delete(b);
b=ui_button_create(lv_screen_active(),UI_BUTTON_OUTLINED,LV_SYMBOL_CLOSE " EXIT FULLSCREEN");lv_obj_set_size(b,280,48);fit(b,"EXIT FULLSCREEN");lv_obj_delete(b);}

static void atomic_readings(void){
lv_obj_t*panel=lv_obj_create(lv_screen_active());lv_obj_remove_style_all(panel);lv_obj_set_size(panel,ui_theme_is_studio()?336:800,ui_theme_is_studio()?354:94);ui_printer_info_cards_t cards={0};ui_printer_info_cards_create(panel,&cards,NULL,NULL,NULL);
ui_printer_info_cards_refresh(panel,&cards,1.0,205.0,205.0,60.0,60.0,100,360000,"100:00",true);
lv_obj_t*readings[]={cards.progress,cards.nozzle,cards.bed,cards.part_fan,cards.elapsed,cards.remaining};for(unsigned i=0;i<6;i++){lv_obj_t*l=readings[i];assert(l);lv_obj_update_layout(l);assert(lv_obj_get_height(l)==lv_obj_get_style_text_font(l,0)->line_height);inside(l,lv_obj_get_parent(l),"live telemetry");checks++;}
lv_obj_delete(panel);

const char *names[]={"PROGRESS","REMAINING","NOZZLE","PART FAN"};
for(int width=115;width<=155;width+=8)for(unsigned i=0;i<4;i++){
lv_obj_t*v=ui_create_operator_info_card(lv_screen_active(),names[i],"205.0/205.0 C",0,0,width,94);
lv_obj_t*card=lv_obj_get_parent(v);lv_obj_update_layout(card);
for(unsigned j=0;j<2;j++){lv_obj_t*l=lv_obj_get_child(card,j);lv_point_t n;const lv_font_t*f=lv_obj_get_style_text_font(l,0);lv_text_get_size(&n,lv_label_get_text(l),f,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);assert(n.x<=lv_obj_get_width(l));assert(lv_obj_get_height(l)==f->line_height);inside(l,card,"atomic telemetry");checks++;}
lv_label_set_text(v,"999.9/999.9 C");ui_text_fit_single_line(v,UI_FONT_VALUE_SMALL);lv_obj_update_layout(v);assert(lv_obj_get_height(v)==lv_obj_get_style_text_font(v,0)->line_height);
lv_label_set_text(v,"60.0/60.0 C");ui_text_fit_single_line(v,UI_FONT_VALUE_SMALL);lv_obj_update_layout(v);assert(lv_obj_get_height(v)==lv_obj_get_style_text_font(v,0)->line_height);lv_obj_delete(card);}
lv_obj_t*b=ui_button_create(lv_screen_active(),UI_BUTTON_DANGER,LV_SYMBOL_WARNING " E-STOP");lv_obj_set_size(b,140,52);fit(b,"single-line E-STOP");assert(lv_obj_get_height(lv_obj_get_child(b,0))==lv_obj_get_style_text_font(lv_obj_get_child(b,0),0)->line_height);lv_obj_delete(b);
const char*status[]={"MOONRAKER LINKED","MOONRAKER OFFLINE","WAITING FOR PRINTER"};for(unsigned i=0;i<3;i++){lv_obj_t*l=lv_label_create(lv_screen_active());lv_label_set_text(l,status[i]);lv_obj_set_width(l,i==2?230:220);ui_apply_custom_label_style(l,UI_FONT_CAPTION,UI_TEXT);lv_obj_update_layout(l);assert(lv_obj_get_height(l)==UI_FONT_CAPTION->line_height);checks++;lv_obj_delete(l);}
}

int main(void){lv_init();lv_display_t*d=lv_display_create(1024,600);static uint8_t buf[1024*40*2];lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,buf,NULL,sizeof(buf),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);for(theme=0;theme<5;theme++)for(density=0;density<3;density++)for(large=0;large<2;large++){ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});lv_obj_t*b;
#include "button_cases.inc"
groups();dynamic();value_rows();atomic_readings();firmware_settings();selector_controls();}printf("PASS: %u full-label, pressed-frame and atomic single-line checks, themes/densities/text sizes, source-derived actions, actual Settings/Printer/Dashboard rows, navigation, bed capability combinations, native footers and dynamic labels\n",checks);return 0;}
