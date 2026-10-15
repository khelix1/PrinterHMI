/* Actual LVGL objects, including independently deleted form fields. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "custom_theme.h"
static bool fail_binding_alloc;
static void *test_calloc(size_t count, size_t size)
{
    if (fail_binding_alloc) { fail_binding_alloc = false; return NULL; }
    return calloc(count, size);
}
#define calloc test_calloc
#include "ui_popup.c"
#undef calloc
#include "ui_form_dialog.h"
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t *b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t *a){(void)a;return false;}

static void resize(lv_obj_t *popup)
{
    lv_obj_set_width(popup, lv_obj_get_width(popup) == 400 ? 420 : 400);
    lv_obj_update_layout(popup);
}
static void footer_checks(void)
{
    unsigned before = lv_obj_get_child_count(lv_layer_top());
    lv_obj_t *popup = ui_popup_create(lv_screen_active(),400,300,UI_POPUP_STANDARD);
    ui_popup_modal_ctx_t *ctx = modal_context(popup);
    lv_obj_t *left = ui_popup_add_footer_action(popup,UI_POPUP_ACTION_CANCEL,"Cancel",140,
        UI_POPUP_FOOTER_LEFT,NULL,NULL,NULL);
    lv_obj_t *right = ui_popup_add_footer_action(popup,UI_POPUP_ACTION_CONFIRM,"Confirm",140,
        UI_POPUP_FOOTER_RIGHT,NULL,NULL,NULL);
    lv_obj_delete(left);
    assert(!ctx->footer_buttons[UI_POPUP_FOOTER_LEFT]);
    resize(popup);
    left = ui_popup_add_footer_action(popup,UI_POPUP_ACTION_CANCEL,"Cancel",140,
        UI_POPUP_FOOTER_LEFT,NULL,NULL,NULL);
    assert(ctx->footer_buttons[UI_POPUP_FOOTER_LEFT] == left);
    lv_obj_delete(lv_obj_get_child(right,0));
    resize(popup); /* Missing button text must not be dereferenced. */
    lv_obj_delete(ctx->footer);
    assert(!ctx->footer && !ctx->footer_buttons[0] && !ctx->footer_buttons[2]);
    resize(popup);
    ui_popup_add_footer_action(popup,UI_POPUP_ACTION_CLOSE,"Close",140,
        UI_POPUP_FOOTER_RIGHT,NULL,NULL,NULL);
    unsigned size_hooks = 0;
    for (uint32_t i=0;i<lv_obj_get_event_count(popup);++i)
        if (lv_event_dsc_get_cb(lv_obj_get_event_dsc(popup,i)) == popup_size_cb) ++size_hooks;
    assert(size_hooks == 1);
    lv_obj_delete(popup);
    assert(lv_obj_get_child_count(lv_layer_top()) == before);

    popup=ui_popup_create(lv_screen_active(),400,300,UI_POPUP_STANDARD);
    ui_popup_add_footer_action(popup,UI_POPUP_ACTION_CLOSE,"Close",140,
        UI_POPUP_FOOTER_RIGHT,NULL,NULL,NULL);
    lv_obj_t *blocker=modal_context(popup)->blocker;
    lv_obj_delete(blocker);
    assert(lv_obj_get_child_count(lv_layer_top()) == before);
    lv_obj_t *owner=lv_obj_create(lv_screen_active());
    popup=ui_popup_create(lv_screen_active(),400,300,UI_POPUP_STANDARD);
    ui_popup_set_owner(popup,owner);
    ui_popup_add_footer_action(popup,UI_POPUP_ACTION_CLOSE,"Close",140,
        UI_POPUP_FOOTER_RIGHT,NULL,NULL,NULL);
    lv_obj_delete(owner);
    assert(lv_obj_get_child_count(lv_layer_top()) == before);
}
static lv_obj_t *add_keyboard(lv_obj_t *popup,lv_obj_t *input,bool form)
{
    return form ? ui_form_keyboard(popup,input,LV_KEYBOARD_MODE_NUMBER) :
        ui_popup_add_keyboard(popup,input,300,140,LV_ALIGN_BOTTOM_MID,0,0,LV_KEYBOARD_MODE_NUMBER);
}
static void keyboard_checks(bool form)
{
    unsigned before=lv_obj_get_child_count(lv_layer_top());
    for (unsigned order=0;order<4;++order) {
        lv_obj_t *popup=ui_form_dialog("INPUT",420,400,UI_POPUP_STANDARD);
        lv_obj_t *input=ui_form_input(ui_form_body(popup),"Value","12",false,8);
        lv_obj_t *keyboard=add_keyboard(popup,input,form);
        assert(keyboard && lv_keyboard_get_textarea(keyboard)==input);
        if (order==0) {
            lv_obj_delete(input);
            assert(lv_keyboard_get_textarea(keyboard)==NULL);
            lv_buttonmatrix_set_selected_button(keyboard,0);
            lv_obj_send_event(keyboard,LV_EVENT_VALUE_CHANGED,NULL);
        } else if (order==1) {
            lv_obj_delete(keyboard);
            lv_textarea_set_text(input,"34");
            lv_obj_delete(input);
        } else if (order==2) {
            lv_obj_t *replacement=ui_form_input(ui_form_body(popup),"Value","56",false,8);
            lv_keyboard_set_textarea(keyboard,replacement);
            lv_obj_delete(input);
            assert(lv_keyboard_get_textarea(keyboard)==replacement);
            lv_obj_delete(keyboard); /* Direct LVGL rebinding remains caller-owned. */
        } else {
            ui_popup_pin_keyboard(popup,keyboard);
        }
        lv_obj_delete(popup);
        assert(lv_obj_get_child_count(lv_layer_top())==before);
    }
    lv_obj_t *popup=ui_form_dialog("INPUT",420,400,UI_POPUP_STANDARD);
    lv_obj_t *input=ui_form_input(ui_form_body(popup),"Value","12",false,8);
    unsigned count=lv_obj_get_child_count(popup);
    fail_binding_alloc=true;
    assert(!add_keyboard(popup,input,form));
    assert(!fail_binding_alloc && lv_obj_get_child_count(popup)==count);
    lv_obj_delete(popup);
    assert(lv_obj_get_child_count(lv_layer_top())==before);
}
int main(void)
{
    lv_init();lv_display_t *display=lv_display_create(1024,600);
    const int widths[]={1024,640,480};
    for(unsigned w=0;w<3;++w)for(int theme=0;theme<5;++theme)for(int large=0;large<2;++large) {
        lv_display_set_resolution(display,widths[w],600);
        ui_theme_set_active(theme);
        ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});
        for(unsigned repeat=0;repeat<3;++repeat) {
            footer_checks();keyboard_checks(false);keyboard_checks(true);
        }
    }
    lv_deinit();
    puts("PASS: footer field retirement/recreation, owner/blocker cleanup, both keyboard helpers, allocation failure and deletion orders across themes/text/viewports");
    return 0;
}
