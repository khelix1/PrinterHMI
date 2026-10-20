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

static uint16_t raster[1024*600];
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *data){
 uint16_t *p=(uint16_t*)data;int width=lv_display_get_horizontal_resolution(d);
 for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)raster[y*width+x]=*p++;
 lv_display_flush_ready(d);
}
static void keyboard_key(lv_obj_t *keyboard,const char *key){
 for(unsigned i=0;;i++){const char *text=lv_buttonmatrix_get_button_text(keyboard,i);assert(text);if(!strcmp(text,key)){lv_buttonmatrix_set_selected_button(keyboard,i);lv_obj_send_event(keyboard,LV_EVENT_VALUE_CHANGED,NULL);return;}}
}
static void cursor_checks(void){
 for(unsigned reduced=0;reduced<2;reduced++)for(unsigned factory=0;factory<3;factory++)for(unsigned password=0;password<2;password++){
  ui_accessibility_t options=ui_theme_get_accessibility();options.reduced_motion=reduced;ui_theme_set_accessibility(options);
  lv_obj_t *popup=ui_form_dialog("Input cursor",420,480,UI_POPUP_STANDARD),*body=ui_form_body(popup),*input;
  if(factory==0)input=ui_form_input(body,"Value","1234",password,16);
  else if(factory==1)input=ui_popup_add_textarea(body,LV_PCT(100),56,LV_ALIGN_TOP_LEFT,0,0,true,password,16,"Value","1234",NULL);
  else {input=lv_textarea_create(body);lv_obj_set_size(input,LV_PCT(100),56);lv_textarea_set_one_line(input,true);lv_textarea_set_password_mode(input,password);lv_textarea_set_text(input,"1234");ui_apply_custom_label_style(input,UI_FONT_BODY,UI_TEXT);ui_apply_surface_role(input,UI_SURFACE_TEXT_INPUT);}
  assert(lv_obj_get_style_border_width(input,LV_PART_CURSOR)==0);
  lv_obj_t *keyboard=ui_form_keyboard(popup,input,LV_KEYBOARD_MODE_NUMBER);assert(keyboard&&lv_obj_has_state(input,LV_STATE_FOCUSED));
  assert(lv_obj_get_style_border_width(input,LV_PART_CURSOR)==2&&lv_obj_get_style_border_side(input,LV_PART_CURSOR)==LV_BORDER_SIDE_LEFT);
  assert(lv_color_eq(lv_obj_get_style_border_color(input,LV_PART_CURSOR),UI_TEXT_BRIGHT));
  assert(lv_obj_get_style_anim_duration(input,LV_PART_CURSOR)==(reduced?0:400));
  lv_textarea_set_cursor_pos(input,2);keyboard_key(keyboard,"5");assert(!strcmp(lv_textarea_get_text(input),"12534")&&lv_textarea_get_cursor_pos(input)==3);
  keyboard_key(keyboard,LV_SYMBOL_BACKSPACE);assert(!strcmp(lv_textarea_get_text(input),"1234")&&lv_textarea_get_cursor_pos(input)==2);
  uint32_t key=LV_KEY_RIGHT;lv_obj_send_event(input,LV_EVENT_KEY,&key);assert(lv_textarea_get_cursor_pos(input)==3);key=LV_KEY_LEFT;lv_obj_send_event(input,LV_EVENT_KEY,&key);assert(lv_textarea_get_cursor_pos(input)==2);
  assert(lv_textarea_get_password_mode(input)==(bool)password);
  if(!password)for(unsigned empty=0;empty<2;empty++){
   lv_keyboard_set_textarea(keyboard,input);if(empty)lv_textarea_set_text(input,"");
   lv_obj_update_layout(popup);lv_refr_now(NULL);lv_obj_t *label=lv_textarea_get_label(input);lv_point_t pos;lv_label_get_letter_pos(label,empty?0:2,&pos);lv_area_t area;lv_obj_get_coords(label,&area);
   int x=area.x1+pos.x,y=area.y1+pos.y,h=lv_obj_get_style_text_font(input,0)->line_height,width=lv_display_get_horizontal_resolution(NULL);assert(h<128&&x>=3&&x+3<width&&y+h<600);
   uint16_t focused[128*7];for(int row=0;row<h;row++)for(int col=0;col<7;col++)focused[row*7+col]=raster[(y+row)*width+x-3+col];
   lv_keyboard_set_textarea(keyboard,NULL);lv_refr_now(NULL);unsigned changed=0;uint16_t bright=lv_color_to_u16(UI_TEXT_BRIGHT);
   for(int row=0;row<h;row++)for(int col=0;col<7;col++)if(focused[row*7+col]==bright&&raster[(y+row)*width+x-3+col]!=bright)changed++;
   assert(changed>=(unsigned)h); /* Actual bright caret pixels disappear on blur. */
  }
  /* Long text must keep scrolling to both ends with no visible bars. */
  lv_keyboard_set_textarea(keyboard,input);lv_textarea_set_max_length(input,128);
  char long_value[97];memset(long_value,'W',96);long_value[96]=0;
  lv_textarea_set_text(input,long_value);lv_textarea_set_cursor_pos(input,LV_TEXTAREA_CURSOR_LAST);
  keyboard_key(keyboard,"5");assert(strlen(lv_textarea_get_text(input))==97&&lv_textarea_get_text(input)[96]=='5');
  lv_tick_inc(500);lv_timer_handler();lv_obj_update_layout(popup);assert(lv_obj_get_scroll_x(input)>0);
  assert(lv_obj_get_scrollbar_mode(input)==LV_SCROLLBAR_MODE_OFF);
  lv_obj_t *text_label=lv_textarea_get_label(input);lv_area_t text_area,viewport;lv_point_t caret;
  lv_label_get_letter_pos(text_label,lv_textarea_get_cursor_pos(input),&caret);lv_obj_get_coords(text_label,&text_area);lv_obj_get_content_coords(input,&viewport);
  lv_obj_get_coords(input,&viewport);
  int cursor_width=lv_obj_get_style_border_width(input,LV_PART_CURSOR);
  int cursor_left=text_area.x1+caret.x-lv_obj_get_style_pad_left(input,LV_PART_CURSOR)-cursor_width;
  int border=lv_obj_get_style_border_width(input,0);
  assert(cursor_left>=viewport.x1+border&&cursor_left+cursor_width-1<=viewport.x2-border);
  lv_textarea_set_cursor_pos(input,0);keyboard_key(keyboard,"5");assert(lv_textarea_get_text(input)[0]=='5'&&strlen(lv_textarea_get_text(input))==98);
  keyboard_key(keyboard,LV_SYMBOL_BACKSPACE);assert(!strcmp(lv_textarea_get_text(input)+96,"5"));
  lv_textarea_set_cursor_pos(input,0);lv_tick_inc(500);lv_timer_handler();lv_obj_update_layout(popup);assert(lv_obj_get_scroll_x(input)==0);
  assert(lv_textarea_get_password_mode(input)==(bool)password);
  lv_keyboard_set_textarea(keyboard,input);lv_obj_add_state(input,LV_STATE_DISABLED);assert(lv_obj_get_style_border_width(input,LV_PART_CURSOR)==0);lv_obj_remove_state(input,LV_STATE_DISABLED);
  lv_obj_delete(popup);
 }
 ui_accessibility_t options=ui_theme_get_accessibility();options.reduced_motion=false;ui_theme_set_accessibility(options);
}
static void feedback_checks(void)
{
 lv_obj_t *popup=ui_form_dialog("Feedback",420,400,UI_POPUP_STANDARD),*body=ui_form_body(popup);
 lv_obj_t *ordinary=ui_form_text(body,"Keep ordinary guidance",UI_FONT_BODY,UI_TEXT);
 unsigned before=lv_obj_get_child_count(body);ui_popup_clear_feedback(body);assert(lv_obj_get_child_count(body)==before);
 lv_obj_t *feedback=ui_popup_feedback(body,UI_STATUS_WARNING,"Invalid value","Retain the draft");
 assert(strstr(lv_label_get_text(feedback),"Invalid value"));before=lv_obj_get_child_count(body);
 ui_popup_clear_feedback(body);ui_popup_clear_feedback(body);
 assert(!lv_label_get_text(feedback)[0]&&lv_obj_get_child_count(body)==before&&!strcmp(lv_label_get_text(ordinary),"Keep ordinary guidance"));
 assert(ui_popup_feedback(body,UI_STATUS_DANGER,"Try again","Retry remains available")==feedback);
 assert(strstr(lv_label_get_text(feedback),"Try again"));
 lv_obj_delete(feedback);ui_popup_clear_feedback(body);assert(!strcmp(lv_label_get_text(ordinary),"Keep ordinary guidance"));
 lv_obj_delete(popup);ui_popup_clear_feedback(NULL);
}
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
    static uint8_t pixels[1024*40*2];lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(display,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(display,flush);
    const int widths[]={1024,640,480};
    for(unsigned w=0;w<3;++w)for(int theme=0;theme<5;++theme)for(int large=0;large<2;++large) {
        lv_display_set_resolution(display,widths[w],600);
        ui_theme_set_active(theme);
        ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});
        cursor_checks();feedback_checks();
        for(unsigned repeat=0;repeat<3;++repeat) {
            footer_checks();keyboard_checks(false);keyboard_checks(true);
        }
    }
    lv_deinit();
    puts("PASS: footer field retirement/recreation, owner/blocker cleanup, both keyboard helpers, three input factories and password modes, rendered focused caret pixels, insertion/backspace/arrows, reduced-motion cursor, allocation failure and deletion orders across themes/text/viewports");
    return 0;
}
