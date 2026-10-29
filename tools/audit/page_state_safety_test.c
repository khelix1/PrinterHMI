/* Real LVGL status panels; the text-update hook models owner callbacks after
 * a widget update and checks metadata lifetime while Show is unwinding. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "custom_theme.h"
#include "ui_value_update.h"
#include "ui_page_state.h"
static unsigned metadata_live;
static bool fail_metadata;
static void (*update_hook)(void);
static void *audit_calloc(size_t n,size_t size)
{
    if(fail_metadata){fail_metadata=false;return NULL;}
    void *p=lv_calloc(n,size);if(p)++metadata_live;return p;
}
static void audit_free(void *p){assert(p&&metadata_live);--metadata_live;lv_free(p);}
static bool audit_set_text(lv_obj_t *label,const char *text)
{
    bool changed=ui_value_set_text(label,text);
    if(update_hook){void(*hook)(void)=update_hook;update_hook=NULL;hook();}
    return changed;
}
#define lv_calloc audit_calloc
#define lv_free audit_free
#define ui_value_set_text audit_set_text
#include "ui_page_state.c"
#undef ui_value_set_text
#undef lv_free
#undef lv_calloc
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t*b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t*a){(void)a;return false;}
static ui_page_state_t *owner;
static lv_obj_t *parent;
static ui_page_state_t *create(void){return ui_page_state_create_owned(parent,0,0,400,300,&owner);}
static void retire(void){lv_obj_delete(owner->root);}
static void replace(void){retire();assert(!owner);create();ui_page_state_show(owner,UI_PAGE_STATE_EMPTY,"Replacement","Keep this panel");}
static void nested_show(void){ui_page_state_show(owner,UI_PAGE_STATE_OFFLINE,"Nested","Latest detail");}
static void nested_hide(void){ui_page_state_hide(owner);}
static void retire_title(void){lv_obj_delete(owner->title);}
static void check(void)
{
    assert(!metadata_live&&!owner);parent=lv_obj_create(lv_screen_active());
    ui_page_state_t *old=create();assert(owner==old&&metadata_live==1);
    for(int kind=0;kind<4;++kind){
        ui_page_state_show(owner,kind,"Panel title","Panel detail");lv_obj_update_layout(parent);
        assert(!lv_obj_has_flag(owner->root,LV_OBJ_FLAG_HIDDEN));
        assert(!strcmp(lv_label_get_text(owner->title),"Panel title"));
        assert(lv_obj_has_flag(owner->spinner,LV_OBJ_FLAG_HIDDEN)==(kind!=UI_PAGE_STATE_LOADING));
        assert(lv_obj_has_flag(owner->icon,LV_OBJ_FLAG_HIDDEN)==(kind==UI_PAGE_STATE_LOADING));
        if(kind!=UI_PAGE_STATE_LOADING){
            lv_font_glyph_dsc_t glyph;
            const uint32_t codepoints[]={0,0xF07B,0xF1EB,0xF071};
            assert(lv_font_get_glyph_dsc(lv_obj_get_style_text_font(owner->icon,0),&glyph,codepoints[kind],0));
            assert(!glyph.is_placeholder && glyph.box_w>0 && glyph.box_h>0);
        }
        ui_page_state_hide(owner);assert(lv_obj_has_flag(owner->root,LV_OBJ_FLAG_HIDDEN));
    }
    fail_metadata=true;assert(!create()&&owner==old&&metadata_live==1);
    ui_page_state_t *replacement=create();assert(owner==replacement&&metadata_live==2);
    lv_obj_delete(old->root);assert(owner==replacement&&metadata_live==1);
    retire();assert(!owner&&!metadata_live);
    ui_page_state_show(owner,UI_PAGE_STATE_ERROR,"Late","Ignored");ui_page_state_hide(owner);
    for(unsigned field=0;field<5;++field){
        create();lv_obj_t *object=(lv_obj_t *[]){owner->icon,owner->title,owner->detail,owner->spinner,
            lv_obj_get_parent(owner->icon)}[field];
        lv_obj_delete(object);
        for(int kind=0;kind<4;++kind){ui_page_state_show(owner,kind,"Retired field","Safe");ui_page_state_hide(owner);}
        retire();assert(!owner&&!metadata_live);
    }
    create();update_hook=nested_show;ui_page_state_show(owner,UI_PAGE_STATE_ERROR,"Outer","Discard");
    assert(!strcmp(lv_label_get_text(owner->title),"Nested")&&!strcmp(lv_label_get_text(owner->detail),"Latest detail"));
    update_hook=nested_hide;ui_page_state_show(owner,UI_PAGE_STATE_EMPTY,"Outer","Discard");
    assert(lv_obj_has_flag(owner->root,LV_OBJ_FLAG_HIDDEN));
    update_hook=retire_title;ui_page_state_show(owner,UI_PAGE_STATE_ERROR,"Outer","Discard");
    assert(!owner->title);ui_page_state_show(owner,UI_PAGE_STATE_EMPTY,NULL,NULL);
    assert(!lv_label_get_text(owner->detail)[0]);
    update_hook=replace;ui_page_state_show(owner,UI_PAGE_STATE_ERROR,"Old","Discard");
    assert(owner&&metadata_live==1&&!strcmp(lv_label_get_text(owner->title),"Replacement"));
    update_hook=retire;ui_page_state_show(owner,UI_PAGE_STATE_ERROR,"Old","Discard");
    assert(!owner&&!metadata_live);
    create();lv_obj_delete(parent);assert(!owner&&!metadata_live);
    /* Optional owner slots remain valid while the object tree lives. */
    parent=lv_obj_create(lv_screen_active());old=ui_page_state_create_owned(parent,0,0,400,300,NULL);
    ui_page_state_show(old,UI_PAGE_STATE_EMPTY,"Legacy","Alive");lv_obj_delete(parent);assert(!metadata_live);
}
int main(void)
{
    lv_init();lv_display_t *display=lv_display_create(1024,600);
    for(int theme=0;theme<5;++theme)for(int density=0;density<3;++density)for(int large=0;large<2;++large)
        for(unsigned w=0;w<3;++w){
            lv_display_set_resolution(display,(int[]){1024,640,480}[w],600);
            ui_theme_set_active(theme);ui_theme_set_density(density);
            ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});check();
        }
    lv_deinit();puts("PASS: owned page-state root/parent/field deletion, replacement slots, interrupted/nested Show/Hide, allocation cleanup and compatibility across all themes/densities/text/viewports");
    return 0;
}
