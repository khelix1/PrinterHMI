#include "ui_dashboard.h"
#include "ui_text.h"
#include "ui_value_update.h"
#include "misc/cache/instance/lv_image_cache.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include <stdio.h>
#include <string.h>
#include "ui_theme.h"
#include "ui_widgets.h"
#include "ui_status_banner.h"
#include "ui_active_print.h"
#include "ui_machine_status.h"
#include "ui_command_bar.h"
#include "ui_dashboard_page.h"
#include "ui_button.h"
#include "ui_popup.h"
#include "ui_form_dialog.h"
#include "ui_camera.h"
#include "camera_stream_controller.h"
#include "camera_catalog_controller.h"
#include "moonraker_config_controller.h"
#include "esp_heap_caps.h"

static lv_obj_t *dash32_root = NULL;

/* Dashboard construction delegates to the shared theme-aware page layout. */
static ui_dashboard_page_t dash32_page = {0};


lv_obj_t **ui_dashboard_thumb_canvas_ref(void)
{
    return ui_active_print_thumb_canvas_ref();
}

uint16_t **ui_dashboard_thumb_canvas_buf_ref(void)
{
    return ui_active_print_thumb_canvas_buf_ref();
}

char *ui_dashboard_thumb_canvas_file(void)
{
    return ui_active_print_thumb_canvas_file();
}

size_t ui_dashboard_thumb_canvas_file_size(void)
{
    return ui_active_print_thumb_canvas_file_size();
}

static lv_obj_t *dash32_banner = NULL;
static lv_obj_t *dash32_machine = NULL;
static lv_obj_t *dash32_active_print = NULL;
static lv_obj_t *dash32_thumb_box = NULL;
static lv_obj_t *dash32_thumb_label = NULL;
static lv_obj_t *dash32_camera_image = NULL;
static lv_obj_t *dash32_camera_toggle = NULL;
static lv_obj_t *dash32_camera_status = NULL;
static lv_timer_t *dash32_camera_timer = NULL;
static uint8_t *dash32_camera_frame = NULL;
static lv_image_dsc_t dash32_camera_dsc;
static bool dash32_camera_mode = false;
static bool dash32_camera_quiesced = false;
static bool dash32_camera_restore_after_ota = false;
static uint32_t dash32_camera_epoch,dash32_camera_generation;
static bool dash32_camera_polling,dash32_camera_dispatching;

typedef struct {lv_obj_t *root;uint32_t epoch,generation;int profile;} dashboard_camera_context_t;
static dashboard_camera_context_t dashboard_camera_context(void)
{
    return (dashboard_camera_context_t){dash32_root,dash32_camera_epoch,moonraker_config_generation(),moonraker_config_active_profile_index()};
}
static bool dashboard_camera_owned(const dashboard_camera_context_t *context)
{
    return context->root && context->root==dash32_root && context->epoch==dash32_camera_epoch &&
        context->generation==moonraker_config_generation() && context->profile==moonraker_config_active_profile_index();
}
static bool dashboard_camera_current(const dashboard_camera_context_t *context)
{
    return dashboard_camera_owned(context) && dash32_camera_mode && !dash32_camera_quiesced &&
        dash32_camera_image && !lv_obj_has_flag(dash32_root,LV_OBJ_FLAG_HIDDEN);
}
static bool dashboard_camera_url(const dashboard_camera_context_t *context,char *url,size_t size)
{
    size_t index=camera_catalog_default(context->profile);
    if(!dashboard_camera_owned(context))return false;
    camera_catalog_entry_t camera={0};
    bool configured=camera_catalog_get(context->profile,index,&camera);
    if(!dashboard_camera_owned(context) || !configured || !camera.configured || !camera.stream_url[0])return false;
    snprintf(url,size,"%s",camera.stream_url);return true;
}
static void dashboard_camera_release_owned(lv_obj_t *image,uint8_t *frame)
{
    bool attached=image && lv_image_get_src(image)==&dash32_camera_dsc;
    if(frame)lv_image_cache_drop(&dash32_camera_dsc);
    memset(&dash32_camera_dsc,0,sizeof(dash32_camera_dsc));
    if(attached)lv_image_set_src(image,NULL);
    heap_caps_free(frame);
}
static void dashboard_camera_release_frame(void)
{
    uint8_t *frame=dash32_camera_frame;dash32_camera_frame=NULL;
    dashboard_camera_release_owned(dash32_camera_image,frame);
}
static void dashboard_camera_set_status(const char *text)
{
    ui_value_set_text(dash32_camera_status,text?text:ui_text(""));
}
static void dashboard_camera_poll_cb(lv_timer_t *timer)
{
    dashboard_camera_context_t context=dashboard_camera_context();
    if(dash32_camera_polling || !dashboard_camera_current(&context) || (timer && timer!=dash32_camera_timer))return;
    dash32_camera_polling=true;uint8_t *pixels=NULL;
    if(dash32_camera_generation!=context.generation){
        dash32_camera_generation=context.generation;
        camera_stream_request_stop();
        if(!dashboard_camera_current(&context))goto finished;
        dashboard_camera_release_frame();
        if(!dashboard_camera_current(&context))goto finished;
    }
    size_t size=0;int width=0,height=0;bool ok=false;
    bool taken=camera_stream_take_result(&pixels,&size,&width,&height,&ok);
    if(!dashboard_camera_current(&context))goto finished;
    if(taken){
        if(!ok || !pixels || width<=0 || height<=0 || width>65535 || height>65535 ||
            (size_t)width>SIZE_MAX/sizeof(uint16_t)/(size_t)height || size<(size_t)width*height*sizeof(uint16_t)){
            heap_caps_free(pixels);pixels=NULL;
            dashboard_camera_release_frame();
            if(!dashboard_camera_current(&context))goto finished;
            dashboard_camera_set_status("Camera unavailable");
        }else{
            dashboard_camera_release_frame();
            if(!dashboard_camera_current(&context))goto finished;
            dash32_camera_frame=pixels;pixels=NULL;
            dash32_camera_dsc.header.magic=LV_IMAGE_HEADER_MAGIC;
            dash32_camera_dsc.header.cf=LV_COLOR_FORMAT_RGB565;
            dash32_camera_dsc.header.w=width;dash32_camera_dsc.header.h=height;
            dash32_camera_dsc.header.stride=width*sizeof(uint16_t);
            dash32_camera_dsc.data=dash32_camera_frame;dash32_camera_dsc.data_size=size;
            lv_image_set_src(dash32_camera_image,&dash32_camera_dsc);
            if(!dashboard_camera_current(&context))goto finished;
            lv_obj_t *box=lv_obj_get_parent(dash32_camera_image);
            int available_w=lv_obj_get_width(box)-12,available_h=lv_obj_get_height(box)-12;
            if(available_w<1)available_w=1;
            if(available_h<1)available_h=1;
            int scale_x=available_w*256/width,scale_y=available_h*256/height;
            int scale=scale_x<scale_y?scale_x:scale_y;if(scale<1)scale=1;
            lv_image_set_scale(dash32_camera_image,scale);
            if(!dashboard_camera_current(&context))goto finished;
            lv_obj_center(dash32_camera_image);
            if(!dashboard_camera_current(&context))goto finished;
            lv_obj_move_foreground(dash32_camera_image);
            dashboard_camera_set_status("LIVE CAMERA");
        }
        if(!dashboard_camera_current(&context))goto finished;
    }
    bool busy=camera_stream_busy();
    if(!dashboard_camera_current(&context) || busy)goto finished;
    char url[192];bool configured=dashboard_camera_url(&context,url,sizeof(url));
    if(!dashboard_camera_current(&context))goto finished;
    if(!configured){dashboard_camera_set_status("No camera configured");goto finished;}
    bool started=camera_stream_start(url);
    if(dashboard_camera_current(&context) && !started)dashboard_camera_set_status("Camera connecting...");
finished:
    heap_caps_free(pixels);dash32_camera_polling=false;
}
static void dashboard_camera_mode_set(bool enabled)
{
    if(enabled && (dash32_camera_quiesced || !dash32_root || !dash32_camera_image || !dash32_camera_toggle))return;
    ++dash32_camera_epoch;dash32_camera_mode=enabled;
    dashboard_camera_context_t context=dashboard_camera_context();
    if(dash32_camera_image){if(enabled)lv_obj_clear_flag(dash32_camera_image,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(dash32_camera_image,LV_OBJ_FLAG_HIDDEN);}
    if(dash32_camera_status){if(enabled)lv_obj_clear_flag(dash32_camera_status,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(dash32_camera_status,LV_OBJ_FLAG_HIDDEN);}
    if(dash32_camera_toggle){lv_obj_t *label=lv_obj_get_child(dash32_camera_toggle,0);if(label)ui_value_set_text(label,enabled?ui_text(LV_SYMBOL_IMAGE " Thumbnail"):ui_text(LV_SYMBOL_IMAGE " Camera"));}
    if(!enabled){
        lv_timer_t *timer=dash32_camera_timer;dash32_camera_timer=NULL;if(timer)lv_timer_delete(timer);
        camera_stream_request_stop();
        if(context.epoch!=dash32_camera_epoch)return;
        dashboard_camera_release_frame();return;
    }
    dash32_camera_generation=context.generation;
    if(!dash32_camera_timer)dash32_camera_timer=lv_timer_create(dashboard_camera_poll_cb,100,NULL);
    dashboard_camera_set_status("Connecting camera...");
    if(dashboard_camera_current(&context))dashboard_camera_poll_cb(NULL);
}
void ui_dashboard_set_camera_quiesced(bool quiesced)
{
    if(quiesced){
        if(dash32_camera_quiesced)return;
        dash32_camera_quiesced=true;dash32_camera_restore_after_ota=dash32_camera_mode;
        if(dash32_camera_mode)dashboard_camera_mode_set(false);
        return;
    }
    if(!dash32_camera_quiesced)return;
    dash32_camera_quiesced=false;
    bool restore=dash32_camera_restore_after_ota;dash32_camera_restore_after_ota=false;
    if(restore && dash32_camera_toggle && !lv_obj_has_flag(dash32_camera_toggle,LV_OBJ_FLAG_HIDDEN))dashboard_camera_mode_set(true);
}
void ui_dashboard_refresh_camera(void)
{
    if(!dash32_camera_toggle)return;
    dashboard_camera_context_t context=dashboard_camera_context();char url[192];
    bool configured=dashboard_camera_url(&context,url,sizeof(url));
    if(!dashboard_camera_owned(&context))return;
    if(!configured){
        uint32_t expected=context.epoch;
        if(dash32_camera_mode){dashboard_camera_mode_set(false);++expected;}
        context.epoch=expected;
        if(dash32_camera_toggle && dashboard_camera_owned(&context))lv_obj_add_flag(dash32_camera_toggle,LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(dash32_camera_toggle,LV_OBJ_FLAG_HIDDEN);
    if(dash32_camera_mode){
        dashboard_camera_mode_set(false);
        ++context.epoch;
        if(dashboard_camera_owned(&context) && !dash32_camera_mode && !dash32_camera_quiesced)dashboard_camera_mode_set(true);
    }
}
static bool dashboard_camera_action(lv_event_t *event,lv_obj_t *object)
{
    return event && dash32_root && !dash32_camera_dispatching && object &&
        !lv_obj_has_state(object,LV_STATE_DISABLED) && lv_event_get_code(event)==LV_EVENT_CLICKED &&
        lv_event_get_target_obj(event)==object && lv_event_get_current_target_obj(event)==object;
}
static void dashboard_camera_toggle_cb(lv_event_t *event)
{
    if(!dashboard_camera_action(event,dash32_camera_toggle))return;
    dash32_camera_dispatching=true;dashboard_camera_mode_set(!dash32_camera_mode);dash32_camera_dispatching=false;
}
static void dashboard_camera_open_fullscreen_cb(lv_event_t *event)
{
    if(!dashboard_camera_action(event,dash32_camera_image) || !dash32_camera_mode)return;
    dash32_camera_dispatching=true;
    lv_obj_t *root=dash32_root;uint32_t epoch=dash32_camera_epoch;
    dashboard_camera_mode_set(false);
    if(root==dash32_root && dash32_camera_epoch==epoch+1U && !dash32_camera_mode && !dash32_camera_quiesced){
        ui_camera_show_fullscreen();
    }
    dash32_camera_dispatching=false;
}
static void dashboard_camera_field_deleted(lv_event_t *event)
{
    lv_obj_t *object=lv_event_get_target_obj(event);
    if(object==dash32_camera_status){dash32_camera_status=NULL;++dash32_camera_epoch;return;}
    if(object==dash32_camera_image){
        ++dash32_camera_epoch;uint32_t epoch=dash32_camera_epoch;
        uint8_t *frame=dash32_camera_frame;dash32_camera_frame=NULL;dash32_camera_image=NULL;
        dashboard_camera_release_owned(object,frame);
        if(epoch==dash32_camera_epoch)dashboard_camera_mode_set(false);
    }else if(object==dash32_camera_toggle || object==dash32_thumb_box){
        if(object==dash32_camera_toggle)dash32_camera_toggle=NULL;
        if(object==dash32_thumb_box)dash32_thumb_box=NULL;
        dashboard_camera_mode_set(false);
    }
}
static void dashboard_camera_root_deleted(lv_event_t *event)
{
    if(lv_event_get_target_obj(event)!=dash32_root)return;
    ++dash32_camera_epoch;uint32_t epoch=dash32_camera_epoch;
    lv_timer_t *timer=dash32_camera_timer;lv_obj_t *image=dash32_camera_image;uint8_t *frame=dash32_camera_frame;
    dash32_camera_timer=NULL;dash32_camera_frame=NULL;dash32_camera_mode=false;
    dash32_camera_image=dash32_camera_toggle=dash32_camera_status=NULL;
    dash32_root=dash32_banner=dash32_machine=dash32_active_print=dash32_thumb_box=dash32_thumb_label=NULL;
    dash32_page=(ui_dashboard_page_t){0};
    if(timer)lv_timer_delete(timer);
    dashboard_camera_release_owned(image,frame);
    if(epoch==dash32_camera_epoch)camera_stream_request_stop();
}

void ui_dashboard_create(void)
{
    if (dash32_page.root) {
        lv_obj_move_foreground(dash32_page.root);
        return;
    }

    dash32_page =
        ui_dashboard_page_create(
            lv_screen_active());

    dash32_root = dash32_page.root;
    dash32_banner = dash32_page.banner_host;
    dash32_active_print = dash32_page.active_print_host;
    dash32_machine = dash32_page.machine_status_host;

    if (!dash32_root ||
        !dash32_banner ||
        !dash32_active_print ||
        !dash32_machine) {
        ui_dashboard_page_destroy(&dash32_page);

        dash32_root = NULL;
        dash32_banner = NULL;
        dash32_active_print = NULL;
        dash32_machine = NULL;
        dash32_thumb_box = NULL;

        return;
    }

    ++dash32_camera_epoch;
    lv_obj_add_event_cb(dash32_root,dashboard_camera_root_deleted,LV_EVENT_DELETE,NULL);

    ui_dashboard_set_active_print(
        "--/--",
        "--:--",
        "REM --:--");

    dash32_thumb_box =
        ui_active_print_thumb_box(
            dash32_active_print);

    ui_dashboard_thumb_set_placeholder(
        "Model preview\n\nNo preview loaded");

    lv_obj_t *preview_box = ui_dashboard_thumb_box();
    if (preview_box) {
        lv_obj_add_event_cb(preview_box,dashboard_camera_field_deleted,LV_EVENT_DELETE,NULL);
        dash32_camera_image = lv_image_create(preview_box);
        lv_obj_add_event_cb(dash32_camera_image,dashboard_camera_field_deleted,LV_EVENT_DELETE,NULL);
        lv_obj_add_flag(dash32_camera_image, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(dash32_camera_image,
                            dashboard_camera_open_fullscreen_cb,
                            LV_EVENT_CLICKED,
                            NULL);
        lv_obj_set_style_bg_color(dash32_camera_image, UI_CARD_DARK, 0);
        lv_obj_set_style_bg_opa(dash32_camera_image, LV_OPA_COVER, 0);
        lv_obj_add_flag(dash32_camera_image, LV_OBJ_FLAG_HIDDEN);
        dash32_camera_status = lv_label_create(preview_box);
        lv_obj_add_event_cb(dash32_camera_status,dashboard_camera_field_deleted,LV_EVENT_DELETE,NULL);
        ui_apply_text_caption(dash32_camera_status);
        ui_apply_label_bright(dash32_camera_status);
        lv_obj_align(dash32_camera_status, LV_ALIGN_BOTTOM_LEFT, 10, -8);
        lv_obj_add_flag(dash32_camera_status, LV_OBJ_FLAG_HIDDEN);
    }
    dash32_camera_toggle = ui_button_create(
        dash32_active_print, UI_BUTTON_OUTLINED, LV_SYMBOL_IMAGE " Camera");
    if (dash32_camera_toggle) {
        lv_obj_add_event_cb(dash32_camera_toggle,dashboard_camera_field_deleted,LV_EVENT_DELETE,NULL);
        lv_obj_set_size(dash32_camera_toggle, 172, ui_theme_is_studio() ? 44 : 32);
        /* Keep the Camera/Thumbnail toggle inside the active-print card
         * for every theme, including Operator Shell and custom themes. */
        if (ui_theme_is_studio()) {
            lv_obj_set_parent(dash32_camera_toggle,preview_box);
            lv_obj_align(dash32_camera_toggle,LV_ALIGN_TOP_RIGHT,-8,8);
        } else lv_obj_align(dash32_camera_toggle, LV_ALIGN_TOP_RIGHT, -8, 8);
        /* Bind the callback even when the initial printer has no camera.
         * Active-printer changes can make this same button visible later. */
        ui_button_expand_touch_target(dash32_camera_toggle);
        lv_obj_add_event_cb(dash32_camera_toggle, dashboard_camera_toggle_cb,
                            LV_EVENT_CLICKED, NULL);

    }

    ui_machine_status_set(
        dash32_machine,
        "-- / -- C",
        "-- / -- C",
        "-- C",
        "-- %RH",
        "100%",
        "100%",
        "--%");

    ui_dashboard_update();
    ui_dashboard_refresh_camera();
}

void ui_dashboard_set_active_print(const char *layer,
                                       const char *elapsed,
                                       const char *remaining)
{
    if (!dash32_active_print) return;
    ui_active_print_set(dash32_active_print, layer, elapsed, remaining);
}

void ui_dashboard_update(void)
{
    if (!dash32_banner) return;

    const esp_app_desc_t *app =
        esp_app_get_description();
    const char *version =
        app && app->version[0]
            ? app->version
            : "--";

    char console_name[64];
    snprintf(
        console_name,
        sizeof(console_name),
        "PrinterHMI %s%s Operator Console",
        version[0] == 'v' ? "" : "v",
        version);

    ui_status_banner_set(
        dash32_banner,
        LV_SYMBOL_OK " READY",
        console_name,
        "ETA --:--",
        "--%"
    );
}

void ui_dashboard_destroy(void)
{
    if(dash32_root)lv_obj_delete(dash32_root);
    else dashboard_camera_mode_set(false);
}

void ui_dashboard_set_filament(
    bool moonraker_online,
    const moonraker_filament_state_t *state)
{
    if (!dash32_machine) return;

    ui_machine_status_set_filament(
        dash32_machine,
        moonraker_online,
        state);
}


void ui_dashboard_set_machine_connection(
    bool online)
{
    if (!dash32_machine) return;

    ui_machine_status_set_connection(
        dash32_machine,
        online);
}


void ui_dashboard_set_active_hotend(
    const char *name,
    const char *value)
{
    if (!dash32_machine) return;

    ui_machine_status_set_active_hotend(
        dash32_machine,
        name,
        value);
}


void ui_dashboard_set_machine(
    const char *nozzle,
    const char *bed,
    const char *chamber,
    const char *humidity,
    const char *speed,
    const char *flow,
    const char *fan
)
{
    if (!dash32_machine) return;

    ui_machine_status_set(
        dash32_machine,
        nozzle,
        bed,
        chamber,
        humidity,
        speed,
        flow,
        fan
    );
}


void ui_dashboard_set_banner(
    const char *state,
    const char *file,
    const char *eta,
    const char *progress
)
{
    if (!dash32_banner) return;
    ui_status_banner_set(dash32_banner, state, file, eta, progress);
}





static lv_obj_t *s_dashboard_status_popup = NULL;

static void dashboard_status_close_cb(lv_event_t *e)
{
    (void)e;

    if (s_dashboard_status_popup) {
        lv_obj_delete(s_dashboard_status_popup);
        s_dashboard_status_popup = NULL;
    }
}

void ui_dashboard_status_popup_close(void)
{
    dashboard_status_close_cb(NULL);
}

static void dashboard_status_deleted_cb(lv_event_t *event)
{
    if (lv_event_get_target(event) == s_dashboard_status_popup) s_dashboard_status_popup = NULL;
}

void ui_dashboard_status_popup_show(const char *title_text, const char *body)
{
    ui_dashboard_status_popup_close();
    int32_t width = lv_display_get_horizontal_resolution(NULL) - 32;
    int32_t height = lv_display_get_vertical_resolution(NULL) - 32;
    if (width > 660) width = 660;
    if (height > 440) height = 440;
    s_dashboard_status_popup = ui_popup_create(lv_screen_active(), width, height, UI_POPUP_STANDARD);
    if (!s_dashboard_status_popup) return;
    lv_obj_add_event_cb(s_dashboard_status_popup, dashboard_status_deleted_cb, LV_EVENT_DELETE, NULL);
    lv_obj_set_flex_flow(s_dashboard_status_popup, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_dashboard_status_popup, UI_PAD_POPUP, 0);
    lv_obj_set_style_pad_row(s_dashboard_status_popup, UI_GAP_CARD, 0);
    lv_obj_t *title = lv_label_create(s_dashboard_status_popup);
    lv_label_set_text(title, title_text ? title_text : ui_text("Status"));
    ui_apply_custom_label_style(title, UI_FONT_TITLE, UI_TEXT_BRIGHT);
    lv_obj_set_width(title, LV_PCT(100));
    lv_label_set_long_mode(title,LV_LABEL_LONG_DOT);
    ui_form_divide_title(s_dashboard_status_popup);
    lv_obj_t *scroll = lv_obj_create(s_dashboard_status_popup);
    lv_obj_remove_style_all(scroll);
    lv_obj_set_size(scroll, LV_PCT(100), 0);
    lv_obj_set_flex_grow(scroll, 1);
    lv_obj_set_flex_flow(scroll, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(scroll, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(scroll, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(scroll,LV_SCROLLBAR_MODE_OFF);
    lv_obj_t *message = lv_label_create(scroll);
    lv_label_set_text(message, body ? body : "--");
    ui_apply_custom_label_style(message, UI_FONT_BODY, UI_TEXT);
    lv_obj_set_width(message, LV_PCT(100));
    lv_label_set_long_mode(message,LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_line_space(message,6,0);
    lv_obj_t *footer = lv_obj_create(s_dashboard_status_popup);
    lv_obj_remove_style_all(footer);
    lv_obj_set_size(footer, LV_PCT(100), LV_SIZE_CONTENT);
    ui_apply_divider_border(footer,LV_BORDER_SIDE_TOP);
    lv_obj_set_style_pad_top(footer,8,0);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    ui_popup_add_action_at(footer, UI_POPUP_ACTION_CLOSE, LV_SYMBOL_CLOSE " Close",
        0, 0, 160, 48, dashboard_status_close_cb, NULL, NULL);
}


lv_obj_t *ui_dashboard_thumb_box(void)
{
    if (dash32_active_print) {
        dash32_thumb_box = ui_active_print_thumb_box(dash32_active_print);
    }
    return dash32_thumb_box;
}

uint32_t ui_dashboard_thumb_generation(void){return ui_active_print_thumb_generation();}

bool ui_dashboard_thumb_ready(void)
{
    return ui_dashboard_thumb_box() != NULL;
}

void ui_dashboard_thumb_set_placeholder(const char *text)
{
    ui_active_print_thumb_set_placeholder(dash32_active_print, text);
}


bool ui_dashboard_thumb_canvas_matches(const char *file)
{
    return ui_active_print_thumb_canvas_matches(file);
}

bool ui_dashboard_thumb_ensure_canvas_buffer(size_t pixels)
{
    return ui_active_print_thumb_ensure_canvas_buffer(pixels);
}

void ui_dashboard_thumb_delete_canvas(void)
{
    ui_active_print_thumb_delete_canvas();
}

void ui_dashboard_thumb_show_canvas_from_buffer(int w, int h, const char *file)
{
    ui_active_print_thumb_show_canvas_from_buffer(dash32_active_print, w, h, file);
}

void ui_dashboard_thumb_apply_canvas_from_buffer(int w, int h, const char *file)
{
    ui_active_print_thumb_apply_canvas_from_buffer(dash32_active_print, w, h, file);
}


void ui_dashboard_thumb_clear_placeholder(void)
{
    ui_active_print_thumb_clear_placeholder(dash32_active_print);
}
