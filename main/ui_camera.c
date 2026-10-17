#include "ui_camera.h"
#include "ui_text.h"
#include "ui_value_update.h"

#include "camera_stream_controller.h"
#include "camera_catalog_controller.h"
#include "moonraker_config_controller.h"
#include "ui_page_geometry.h"
#include "ui_button.h"
#include "ui_printer_profiles.h"
#include "ui_page_title.h"
#include "ui_popup.h"
#include "ui_form_dialog.h"
#include "ui_shell.h"
#include "ui_theme.h"

#include "esp_heap_caps.h"

#include "lvgl.h"
#include "misc/cache/instance/lv_image_cache.h"

#include <stdio.h>
#include <string.h>

static lv_obj_t *s_root = NULL;
static lv_obj_t *s_image = NULL;
static lv_obj_t *s_status = NULL;
static lv_obj_t *s_card = NULL;
static lv_obj_t *s_fullscreen_button = NULL;
static lv_obj_t *s_configure_button = NULL;
static lv_obj_t *s_camera_selector = NULL;
static lv_obj_t *s_selector_label;
static char s_selector_text[64], s_selector_rendered[64];
static lv_obj_t *s_view_button = NULL;
static lv_obj_t *s_retry_button = NULL;
static uint32_t s_camera_generation;
static void camera_update_selector(void);
static lv_obj_t *s_view_popup = NULL;
static lv_obj_t *s_camera_picker_popup = NULL;
static lv_obj_t *s_view_feedback, *s_picker_feedback;
static uint32_t s_view_owner, s_picker_owner, s_camera_dialog_epoch;
static size_t s_view_camera;
static bool s_camera_dialog_dispatching;
static lv_obj_t *s_view_actions[7], *s_picker_actions[CAMERA_CATALOG_MAX_CAMERAS];
static lv_obj_t *s_view_close, *s_picker_close;
static uint32_t s_camera_ui_epoch;
static char s_view_url[192], s_picker_urls[CAMERA_CATALOG_MAX_CAMERAS][192];

static void camera_dialog_deleted(lv_event_t *event)
{
    lv_obj_t *target = lv_event_get_target_obj(event);
    if (target == s_view_popup) {
        ++s_camera_dialog_epoch;
        s_view_popup = s_view_feedback = s_view_close = NULL;
        memset(s_view_actions, 0, sizeof(s_view_actions));
        s_view_owner = 0;
        s_view_url[0] = 0;
    } else if (target == s_camera_picker_popup) {
        ++s_camera_dialog_epoch;
        s_camera_picker_popup = s_picker_feedback = s_picker_close = NULL;
        memset(s_picker_actions, 0, sizeof(s_picker_actions));
        s_picker_owner = 0;
        memset(s_picker_urls, 0, sizeof(s_picker_urls));
    }
}

static void camera_close_popups(void)
{
    ++s_camera_dialog_epoch;
    lv_obj_t *view = s_view_popup, *picker = s_camera_picker_popup;
    s_view_popup = s_camera_picker_popup = s_view_feedback = s_picker_feedback = NULL;
    s_view_close = s_picker_close = NULL;
    memset(s_view_actions, 0, sizeof(s_view_actions));
    memset(s_picker_actions, 0, sizeof(s_picker_actions));
    s_view_owner = s_picker_owner = 0;
    s_view_url[0] = 0;
    memset(s_picker_urls, 0, sizeof(s_picker_urls));
    if (view) lv_obj_delete(view);
    if (picker) lv_obj_delete(picker);
}
static size_t s_camera_index = 0;
static bool s_fullscreen = false;
static bool s_setup_active = false;
static bool s_camera_retiring;
static bool s_camera_polling;
static unsigned s_camera_show_depth;
static int s_view_width = UI_PAGE_ROOT_WIDTH;
static int s_view_height = UI_PAGE_ROOT_HEIGHT;
static lv_timer_t *s_refresh_timer = NULL;
static uint8_t *s_frame = NULL;
static lv_image_dsc_t s_frame_dsc;
static uint32_t s_camera_last_frame_tick = 0;
static uint32_t s_camera_stream_started_tick = 0;
static uint32_t s_camera_window_tick = 0;
static unsigned s_camera_frame_count = 0;


static void camera_set_status(const char *text)
{
    if (s_retry_button) {
        bool retry = text && strstr(text, "Camera unavailable") && !s_fullscreen;
        if (retry) lv_obj_remove_flag(s_retry_button, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_retry_button, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_status) {
        ui_value_set_text(s_status, text ? text : ui_text(""));
    }
}


static void camera_release_frame(void)
{
    /* Retire ownership before detaching; that update can replace the page. */
    uint8_t *frame=s_frame;
    lv_obj_t *image=s_image;
    bool attached=image && lv_image_get_src(image)==&s_frame_dsc;
    s_frame=NULL;
    if(frame)lv_image_cache_drop(&s_frame_dsc);
    memset(&s_frame_dsc,0,sizeof(s_frame_dsc));
    if(attached)lv_image_set_src(image,NULL);
    heap_caps_free(frame);
}

static bool camera_page_active(void)
{
    return s_root && s_card && s_image && !s_setup_active && !s_camera_retiring &&
        !lv_obj_has_flag(s_root, LV_OBJ_FLAG_HIDDEN);
}

typedef struct {lv_obj_t *root;uint32_t epoch,generation;int profile;size_t camera;} camera_ui_context_t;
static camera_ui_context_t camera_ui_context(void)
{
    return (camera_ui_context_t){s_root,s_camera_ui_epoch,moonraker_config_generation(),
        moonraker_config_active_profile_index(),s_camera_index};
}
/* Lifecycle operations also own a hidden or setup-suspended page. */
static bool camera_ui_same_page(const camera_ui_context_t *context)
{
    return context->root && !s_camera_retiring && context->root==s_root &&
        context->epoch==s_camera_ui_epoch && context->generation==moonraker_config_generation() &&
        context->profile==moonraker_config_active_profile_index();
}
static bool camera_ui_owned(const camera_ui_context_t *context)
{
    return camera_ui_same_page(context) && context->camera==s_camera_index;
}
static bool camera_ui_current(const camera_ui_context_t *context)
{
    return camera_page_active() && camera_ui_owned(context);
}
static bool camera_review_current(const camera_ui_context_t *context,lv_obj_t *popup,uint32_t epoch,bool view)
{
    return camera_ui_current(context) && epoch==s_camera_dialog_epoch &&
        popup==(view?s_view_popup:s_camera_picker_popup);
}
static bool camera_owned_action(lv_event_t *event,lv_obj_t *button)
{
    return camera_page_active() && !s_camera_dialog_dispatching && button &&
        !lv_obj_has_state(button,LV_STATE_DISABLED) &&
        (!event || (lv_event_get_code(event)==LV_EVENT_CLICKED && lv_event_get_target_obj(event)==button &&
            lv_event_get_current_target_obj(event)==button));
}
static void camera_field_deleted(lv_event_t *event)
{
    lv_obj_t *target=lv_event_get_target_obj(event);bool retired=false;
    lv_obj_t **fields[]={&s_status,&s_fullscreen_button,&s_configure_button,&s_camera_selector,&s_selector_label,
        &s_view_button,&s_retry_button,&s_view_feedback,&s_picker_feedback,&s_view_close,&s_picker_close};
    for(unsigned i=0;i<sizeof(fields)/sizeof(fields[0]);++i)if(*fields[i]==target){*fields[i]=NULL;retired=true;}
    for(unsigned i=0;i<7;++i)if(s_view_actions[i]==target){s_view_actions[i]=NULL;retired=true;}
    for(unsigned i=0;i<CAMERA_CATALOG_MAX_CAMERAS;++i)if(s_picker_actions[i]==target){s_picker_actions[i]=NULL;retired=true;}
    if(retired){++s_camera_ui_epoch;++s_camera_dialog_epoch;}
}
static void camera_watch(lv_obj_t *object)
{
    if(object)lv_obj_add_event_cb(object,camera_field_deleted,LV_EVENT_DELETE,NULL);
}

static void camera_reset_timing(void)
{
    s_camera_last_frame_tick = s_camera_stream_started_tick = 0;
    s_camera_window_tick = 0;
    s_camera_frame_count = 0;
}

static void camera_retire_page(bool root_is_deleting)
{
    if (s_camera_retiring) return;
    s_camera_retiring = true;
    ++s_camera_ui_epoch;
    lv_obj_t *root = s_root;
    lv_obj_t *detached = s_card && lv_obj_get_parent(s_card) != root ? s_card : NULL;
    lv_timer_t *timer = s_refresh_timer;
    s_refresh_timer = NULL;
    camera_close_popups();
    camera_stream_request_stop();
    /* Retire the image/cache while its descriptor and pixels are still valid. */
    camera_release_frame();
    s_root = s_image = s_card = s_status = NULL;
    s_fullscreen_button = s_configure_button = s_camera_selector = NULL;
    s_view_button = s_retry_button = s_selector_label = NULL;
    s_selector_text[0] = s_selector_rendered[0] = 0;
    s_fullscreen = false;
    camera_reset_timing();
    if (timer) lv_timer_delete(timer);
    if (detached) lv_obj_delete(detached);
    if (root && !root_is_deleting) lv_obj_delete(root);
    s_camera_retiring = false;
}

static void camera_page_deleted(lv_event_t *event)
{
    if (lv_event_get_target_obj(event) == s_root) camera_retire_page(true);
}

static void camera_surface_deleted(lv_event_t *event)
{
    lv_obj_t *target = lv_event_get_target_obj(event);
    if (target != s_card && target != s_image) return;
    ++s_camera_ui_epoch;
    camera_close_popups();
    lv_timer_t *timer = s_refresh_timer;
    s_refresh_timer = NULL;
    camera_stream_request_stop();
    camera_release_frame();
    if (target == s_card) { s_card = NULL; s_fullscreen = false; }
    else if (s_card && s_root && lv_obj_get_parent(s_card) != s_root) {
        /* A retired fullscreen image must not leave an overlay above navigation. */
        lv_obj_set_parent(s_card, s_root);
        lv_obj_add_flag(s_card, LV_OBJ_FLAG_HIDDEN);
        s_fullscreen = false;
    }
    s_image = NULL;
    camera_reset_timing();
    if (timer) lv_timer_delete(timer);
}

static void camera_mark_unavailable(void)
{
    camera_ui_context_t context=camera_ui_context();
    camera_release_frame();
    if(!camera_ui_current(&context))return;
    s_camera_last_frame_tick = 0;
    camera_set_status("Camera unavailable | reconnecting");
}





static void camera_apply_pixel_mirror(const camera_catalog_entry_t *entry)
{
    if (!entry || !s_frame || !s_frame_dsc.header.w || !s_frame_dsc.header.h) return;
    uint16_t *pixels = (uint16_t *)s_frame;
    int width = (int)s_frame_dsc.header.w;
    int height = (int)s_frame_dsc.header.h;

    if (entry->mirror_horizontal) {
        for (int y = 0; y < height; ++y) {
            uint16_t *row = pixels + (y * width);
            for (int left = 0, right = width - 1; left < right; ++left, --right) {
                uint16_t swap = row[left];
                row[left] = row[right];
                row[right] = swap;
            }
        }
    }
    if (entry->mirror_vertical) {
        for (int top = 0, bottom = height - 1; top < bottom; ++top, --bottom) {
            uint16_t *upper = pixels + (top * width);
            uint16_t *lower = pixels + (bottom * width);
            for (int x = 0; x < width; ++x) {
                uint16_t swap = upper[x];
                upper[x] = lower[x];
                lower[x] = swap;
            }
        }
    }
}

static void camera_apply_view_transform(void)
{
    if (!s_image || !s_frame_dsc.header.w || !s_frame_dsc.header.h) return;
    camera_ui_context_t context=camera_ui_context();
    lv_obj_t *image=s_image;uint8_t *frame=s_frame;
    camera_catalog_entry_t entry = {0};
    (void)camera_catalog_get(moonraker_config_active_profile_index(),
                             s_camera_index, &entry);
    if(!camera_ui_current(&context) || image!=s_image || frame!=s_frame)return;
    int image_width = (int)s_frame_dsc.header.w;
    int image_height = (int)s_frame_dsc.header.h;
    if (entry.rotation == 90U || entry.rotation == 270U) {
        int swap = image_width; image_width = image_height; image_height = swap;
    }
    int scale_x = (s_view_width * 256) / image_width;
    int scale_y = (s_view_height * 256) / image_height;
    int scale = scale_x < scale_y ? scale_x : scale_y;
    if (scale < 1) scale = 1;
    if (lv_image_get_scale_x(s_image) != scale)
        lv_image_set_scale_x(s_image, scale);
    if(!camera_ui_current(&context) || image!=s_image || frame!=s_frame)return;
    if (lv_image_get_scale_y(s_image) != scale)
        lv_image_set_scale_y(s_image, scale);
    if(!camera_ui_current(&context) || image!=s_image || frame!=s_frame)return;
    int32_t rotation = (int32_t)(entry.rotation * 10U);
    if (lv_image_get_rotation(s_image) != rotation)
        lv_image_set_rotation(s_image, rotation);
    if(!camera_ui_current(&context) || image!=s_image || frame!=s_frame)return;
    if (lv_obj_get_style_align(s_image, 0) != LV_ALIGN_CENTER ||
        lv_obj_get_style_x(s_image, 0) != 0 || lv_obj_get_style_y(s_image, 0) != 0)
        lv_obj_center(s_image);
}

static void camera_view_close_cb(lv_event_t *event)
{
    if (!s_view_popup || (event && !camera_owned_action(event,s_view_close))) return;
    ++s_camera_dialog_epoch;
    lv_obj_t *popup = s_view_popup;
    s_view_popup = s_view_feedback = s_view_close = NULL;
    memset(s_view_actions, 0, sizeof(s_view_actions));
    s_view_owner = 0;
    s_view_url[0] = 0;
    if (popup) lv_obj_delete(popup);
}

static void camera_view_set_cb(lv_event_t *event)
{
    if (!event || !s_view_popup) return;
    unsigned action=(unsigned)(uintptr_t)lv_event_get_user_data(event);
    if(action>6 || !camera_owned_action(event,s_view_actions[action]))return;
    if(s_view_owner!=moonraker_config_generation() || s_view_camera!=s_camera_index){
        ui_value_set_text(s_view_feedback,"Printer or camera changed. Close and reopen View.");return;
    }
    ++s_camera_ui_epoch;
    camera_ui_context_t context=camera_ui_context();
    uint32_t epoch=s_camera_dialog_epoch;lv_obj_t *popup=s_view_popup;
    char url[sizeof(s_view_url)];memcpy(url,s_view_url,sizeof(url));
    s_camera_dialog_dispatching=true;
    camera_catalog_entry_t entry={0};
    bool configured=camera_catalog_get(context.profile,context.camera,&entry);
    if(!camera_review_current(&context,popup,epoch,true))goto finished;
    if(!configured || !entry.configured || strcmp(entry.stream_url,url)){
        ui_value_set_text(s_view_feedback,"Camera configuration changed. Close and reopen View.");goto finished;
    }
    unsigned rotation=entry.rotation;
    bool mirror_h=entry.mirror_horizontal,mirror_v=entry.mirror_vertical;
    if(action<=3U)rotation=action*90U;
    else if(action==4U)mirror_h=!mirror_h;
    else if(action==5U)mirror_v=!mirror_v;
    else {rotation=0;mirror_h=false;mirror_v=false;}
    bool saved=camera_catalog_set_view(context.profile,context.camera,rotation,mirror_h,mirror_v);
    if(!camera_review_current(&context,popup,epoch,true))goto finished;
    if(!saved){ui_value_set_text(s_view_feedback,"Could not save camera view. Retry or Close.");goto finished;}
    camera_stream_request_stop();
    if(!camera_review_current(&context,popup,epoch,true))goto finished;
    camera_release_frame();
    if(!camera_review_current(&context,popup,epoch,true))goto finished;
    camera_reset_timing();
    camera_set_status("Updating camera view...");
    if(camera_review_current(&context,popup,epoch,true))camera_view_close_cb(NULL);
finished:
    s_camera_dialog_dispatching=false;
}

static lv_obj_t *camera_view_group(lv_obj_t *body, bool rotations)
{
    lv_obj_t *group = lv_obj_create(body);
    ui_responsive_column(group);
    static const int32_t columns[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static const int32_t rotation_rows[] = {48, 48, LV_GRID_TEMPLATE_LAST};
    static const int32_t mirror_rows[] = {48, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(group, columns, rotations ? rotation_rows : mirror_rows);
    lv_obj_set_style_pad_column(group, 8, 0);
    lv_obj_set_style_pad_row(group, 8, 0);
    return group;
}

static void camera_view_option(lv_obj_t *group, const char *text, unsigned action, bool active)
{
    lv_obj_t *button = ui_button_create(group, active ? UI_BUTTON_PRIMARY : UI_BUTTON_SECONDARY, text);
    s_view_actions[action] = button;camera_watch(button);
    lv_obj_set_height(button, 48);
    unsigned index = action < 4 ? action : action - 4;
    lv_obj_set_grid_cell(button, LV_GRID_ALIGN_STRETCH, index%2, 1, LV_GRID_ALIGN_STRETCH, index/2, 1);
    lv_obj_add_event_cb(button, ui_form_action_fit, LV_EVENT_SIZE_CHANGED, NULL);
    lv_obj_add_event_cb(button, camera_view_set_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)action);
    if (active) lv_obj_add_state(button, LV_STATE_CHECKED);
}

static void camera_view_open_cb(lv_event_t *event)
{
    if (!camera_owned_action(event,s_view_button) || s_camera_generation != moonraker_config_generation()) return;
    if (s_view_popup) { lv_obj_move_foreground(s_view_popup); return; }
    camera_ui_context_t context=camera_ui_context();
    uint32_t epoch=s_camera_dialog_epoch;
    camera_catalog_entry_t entry;
    bool configured=camera_catalog_get(context.profile,context.camera,&entry);
    if(!camera_ui_current(&context) || epoch!=s_camera_dialog_epoch || s_view_popup)return;
    if (!configured || !entry.configured) {
        camera_set_status("No configured camera to adjust.");
        return;
    }
    s_view_owner = moonraker_config_generation();
    s_view_camera = s_camera_index;
    memcpy(s_view_url, entry.stream_url, sizeof(s_view_url));
    s_view_url[sizeof(s_view_url)-1] = 0;
    s_view_popup = ui_form_dialog(ui_text("Camera view"), 680, 460, UI_POPUP_STANDARD);
    if (!s_view_popup) return;
    lv_obj_add_event_cb(s_view_popup, camera_dialog_deleted, LV_EVENT_DELETE, NULL);
    lv_obj_t *body = ui_form_body(s_view_popup);
    ui_form_text(body, entry.name[0] ? entry.name : "Selected camera", UI_FONT_BODY, UI_TEXT);
    ui_form_text(body, ui_text("Rotation"), UI_FONT_CAPTION, UI_TEXT_DIM);
    lv_obj_t *rotations = camera_view_group(body, true);
    const char *labels[] = {"0 deg", "90 deg", "180 deg", "270 deg"};
    for (unsigned i = 0; i < 4; ++i) camera_view_option(rotations, ui_text(labels[i]), i, entry.rotation == i*90);
    ui_form_text(body, ui_text("Mirror"), UI_FONT_CAPTION, UI_TEXT_DIM);
    lv_obj_t *mirrors = camera_view_group(body, false);
    camera_view_option(mirrors, ui_text("Horizontal"), 4, entry.mirror_horizontal);
    camera_view_option(mirrors, ui_text("Vertical"), 5, entry.mirror_vertical);
    s_view_feedback = ui_form_text(body, "", UI_FONT_CAPTION, UI_TEXT_DIM);camera_watch(s_view_feedback);
    s_view_close=ui_form_action(s_view_popup, UI_BUTTON_CLOSE, LV_SYMBOL_CLOSE " Close", camera_view_close_cb, NULL);camera_watch(s_view_close);
    s_view_actions[6]=ui_form_action(s_view_popup, UI_BUTTON_SECONDARY, LV_SYMBOL_REFRESH " Reset", camera_view_set_cb, (void *)6);camera_watch(s_view_actions[6]);
    ui_form_finish(s_view_popup);
}

static void camera_set_viewport(bool fullscreen)
{
    if (!s_root || !s_card || !s_image) return;
    s_fullscreen = fullscreen;
    if (fullscreen) {
        /* Reparent above the shell for a genuine 1024 x 600 display takeover. */
        lv_obj_set_parent(s_card, lv_layer_top());
        lv_obj_set_pos(s_card, 0, 0);
        lv_obj_set_size(s_card, 1024, 600);
        lv_obj_set_style_pad_all(s_card, 0, 0);
        lv_obj_set_style_radius(s_card, 0, 0);
        lv_obj_set_style_border_width(s_card, 0, 0);
        s_view_width = 1024;
        s_view_height = 600;
        lv_obj_set_pos(s_image, 0, 0);
        lv_obj_set_size(s_image, s_view_width, s_view_height);
        if(s_configure_button)lv_obj_add_flag(s_configure_button, LV_OBJ_FLAG_HIDDEN);
        if(s_camera_selector)lv_obj_add_flag(s_camera_selector, LV_OBJ_FLAG_HIDDEN);
        if(s_status)lv_obj_add_flag(s_status, LV_OBJ_FLAG_HIDDEN);
        if(s_fullscreen_button)lv_obj_set_pos(s_fullscreen_button, 728, 536);
        if(s_fullscreen_button)lv_obj_set_size(s_fullscreen_button, 280, 48);
        if(s_fullscreen_button && lv_obj_get_child(s_fullscreen_button,0))lv_label_set_text(lv_obj_get_child(s_fullscreen_button, 0), ui_text(LV_SYMBOL_CLOSE " Exit fullscreen"));
        lv_obj_move_foreground(s_card);
    } else {
        lv_obj_set_parent(s_card, s_root);
        lv_obj_set_style_pad_all(s_card, 8, 0);
        lv_obj_set_style_radius(s_card, UI_RADIUS_CARD, 0);
        lv_obj_set_style_border_width(s_card, UI_BORDER_THIN, 0);
        if(s_configure_button)lv_obj_clear_flag(s_configure_button, LV_OBJ_FLAG_HIDDEN);
        if(s_camera_selector)lv_obj_clear_flag(s_camera_selector, LV_OBJ_FLAG_HIDDEN);
        if(s_status)lv_obj_clear_flag(s_status, LV_OBJ_FLAG_HIDDEN);
        if(s_fullscreen_button)lv_obj_set_size(s_fullscreen_button, 210, 48);
        lv_obj_set_pos(s_card, 0, 0);
        lv_obj_set_size(s_card, ui_theme_is_studio() ? 976 : UI_PAGE_ROOT_WIDTH,
            ui_theme_is_studio() ? 424 : UI_PAGE_ROOT_HEIGHT);
        s_view_width = ui_theme_is_studio() ? 976 : UI_PAGE_ROOT_WIDTH;
        s_view_height = ui_theme_is_studio() ? 424 : UI_PAGE_ROOT_HEIGHT;
        lv_obj_set_pos(s_image, 0, 0);
        lv_obj_set_size(s_image, s_view_width, s_view_height);
        if(s_configure_button)lv_obj_set_pos(s_configure_button, 20, ui_theme_is_studio() ? 358 : 464);
        if(s_view_button)lv_obj_set_pos(s_view_button, 226, ui_theme_is_studio() ? 358 : 464);
        if(s_camera_selector)lv_obj_set_pos(s_camera_selector, 356, ui_theme_is_studio() ? 358 : 464);
        if(s_fullscreen_button)lv_obj_set_pos(s_fullscreen_button, 624, ui_theme_is_studio() ? 358 : 464);
        if(s_status)lv_obj_set_pos(s_status, 20, 106);
        if(s_status)lv_obj_set_width(s_status, UI_PAGE_ROOT_WIDTH - 40);
        if(s_fullscreen_button && lv_obj_get_child(s_fullscreen_button,0))lv_label_set_text(lv_obj_get_child(s_fullscreen_button, 0), ui_text(LV_SYMBOL_IMAGE " Fullscreen"));
        if(ui_theme_is_studio()) {
            lv_obj_set_style_pad_all(s_card,0,0);lv_obj_set_style_radius(s_card,0,0);lv_obj_set_style_border_width(s_card,0,0);
            if(s_status)lv_obj_set_pos(s_status,16,16);
            if(s_status)lv_obj_set_width(s_status,944);
            if(s_status)lv_obj_set_style_bg_color(s_status,UI_BG,0);
            if(s_status)lv_obj_set_style_bg_opa(s_status,LV_OPA_80,0);
            if(s_status)lv_obj_set_style_pad_all(s_status,6,0);
            if(s_configure_button)lv_obj_set_pos(s_configure_button,100,356);
            if(s_view_button)lv_obj_set_pos(s_view_button,308,356);
            if(s_camera_selector)lv_obj_set_pos(s_camera_selector,448,356);
            if(s_fullscreen_button)lv_obj_set_pos(s_fullscreen_button,718,356);
        }
        lv_obj_move_background(s_card);
    }
    if (s_retry_button) {
        if (fullscreen) lv_obj_add_flag(s_retry_button, LV_OBJ_FLAG_HIDDEN);
        else if(s_status)camera_set_status(lv_label_get_text(s_status));
    }
    camera_apply_view_transform();
}


static void camera_image_tap_cb(lv_event_t *event)
{
    if (event && camera_owned_action(event,s_image) && s_fullscreen) {
        ++s_camera_ui_epoch;
        camera_set_viewport(false);
    }
}


static void camera_fullscreen_cb(lv_event_t *event)
{
    if (camera_owned_action(event,s_fullscreen_button)) {++s_camera_ui_epoch;camera_set_viewport(!s_fullscreen);}
}


static void camera_configure_cb(lv_event_t *event)
{
    if (!camera_owned_action(event,s_configure_button)) return;
    ++s_camera_ui_epoch;
    s_camera_dialog_dispatching = true;
    camera_ui_context_t context = camera_ui_context();
    camera_close_popups();
    if (camera_ui_current(&context))
        ui_printer_profiles_open_camera_setup(context.profile, NULL, NULL);
    s_camera_dialog_dispatching = false;
}


static void camera_poll_cb(lv_timer_t *timer)
{
    if(s_camera_show_depth || s_camera_polling || !camera_page_active() || (timer && timer!=s_refresh_timer))return;
    s_camera_polling=true;
    camera_ui_context_t context=camera_ui_context();
    uint32_t now=lv_tick_get();
    uint8_t *pixels=NULL;
    if(s_camera_generation!=context.generation){
        s_camera_generation=context.generation;
        camera_close_popups();
        if(!camera_ui_current(&context))goto finished;
        camera_stream_request_stop();
        if(!camera_ui_current(&context))goto finished;
        camera_release_frame();
        if(!camera_ui_current(&context))goto finished;
        camera_reset_timing();
        size_t index=camera_catalog_default(context.profile);
        if(!camera_ui_current(&context))goto finished;
        s_camera_index=index;context.camera=index;
        camera_update_selector();
        if(!camera_ui_current(&context))goto finished;
        camera_set_status("Connecting | active printer changed");
        if(!camera_ui_current(&context))goto finished;
    }
    size_t pixel_size=0;int width=0,height=0;bool ok=false;
    bool taken=camera_stream_take_result(&pixels,&pixel_size,&width,&height,&ok);
    if(!camera_ui_current(&context))goto finished;
    if(taken){
        if(!ok || !pixels || width<=0 || height<=0 || width>65535 || height>65535 ||
            (size_t)width>SIZE_MAX/sizeof(uint16_t)/(size_t)height ||
            pixel_size<(size_t)width*(size_t)height*sizeof(uint16_t)){
            heap_caps_free(pixels);pixels=NULL;
            camera_mark_unavailable();
        }else{
            camera_catalog_entry_t view={0};
            (void)camera_catalog_get(context.profile,context.camera,&view);
            if(!camera_ui_current(&context))goto finished;
            camera_release_frame();
            if(!camera_ui_current(&context))goto finished;
            s_frame=pixels;pixels=NULL;
            memset(&s_frame_dsc,0,sizeof(s_frame_dsc));
            s_frame_dsc.header.magic=LV_IMAGE_HEADER_MAGIC;
            s_frame_dsc.header.cf=LV_COLOR_FORMAT_RGB565;
            s_frame_dsc.header.w=width;s_frame_dsc.header.h=height;
            s_frame_dsc.header.stride=width*sizeof(uint16_t);
            s_frame_dsc.data=s_frame;s_frame_dsc.data_size=pixel_size;
            camera_apply_pixel_mirror(&view);
            lv_image_set_src(s_image,&s_frame_dsc);
            if(!camera_ui_current(&context))goto finished;
            camera_apply_view_transform();
            if(!camera_ui_current(&context))goto finished;
            if(!s_camera_last_frame_tick)camera_set_status("Live | receiving camera frames");
            if(!camera_ui_current(&context))goto finished;
            s_camera_last_frame_tick=now;
            if(!s_camera_window_tick)s_camera_window_tick=now;
            ++s_camera_frame_count;
            uint32_t elapsed=now-s_camera_window_tick;
            if(elapsed>=1000){
                char status[48];unsigned fps=(s_camera_frame_count*1000U+elapsed/2U)/elapsed;
                snprintf(status,sizeof(status),"Live  |  %u FPS",fps);
                camera_set_status(status);
                if(!camera_ui_current(&context))goto finished;
                s_camera_window_tick=now;s_camera_frame_count=0;
            }
        }
        if(!camera_ui_current(&context))goto finished;
    }
    uint32_t latest=s_camera_last_frame_tick?s_camera_last_frame_tick:s_camera_stream_started_tick;
    if(latest && now-latest>2000){
        camera_mark_unavailable();
        if(!camera_ui_current(&context))goto finished;
    }
    bool busy=camera_stream_busy();
    if(!camera_ui_current(&context) || busy)goto finished;
    camera_catalog_entry_t selected={0};
    bool configured=camera_catalog_get(context.profile,context.camera,&selected);
    if(!camera_ui_current(&context))goto finished;
    if(!configured || !selected.configured){
        size_t index=camera_catalog_default(context.profile);
        if(!camera_ui_current(&context))goto finished;
        s_camera_index=index;context.camera=index;
        configured=camera_catalog_get(context.profile,index,&selected);
        if(!camera_ui_current(&context))goto finished;
        if(!configured || !selected.configured){
            camera_release_frame();
            if(!camera_ui_current(&context))goto finished;
            s_camera_stream_started_tick=0;
            camera_set_status("No camera is configured for the active printer.");goto finished;
        }
    }
    bool started=camera_stream_start(selected.stream_url);
    if(!camera_ui_current(&context))goto finished;
    if(!started)camera_set_status("Camera unavailable | retrying");
    else{
        s_camera_stream_started_tick=now;
        camera_set_status("Connecting | waiting for camera stream");
    }
finished:
    heap_caps_free(pixels);
    s_camera_polling=false;
}


static void camera_retry_cb(lv_event_t *event)
{
    if(!camera_owned_action(event,s_retry_button))return;
    ++s_camera_ui_epoch;
    camera_ui_context_t context=camera_ui_context();
    s_camera_dialog_dispatching=true;
    camera_stream_request_stop();
    if(!camera_ui_current(&context))goto finished;
    camera_release_frame();
    if(!camera_ui_current(&context))goto finished;
    camera_reset_timing();
    camera_set_status("Connecting | retry requested");
    if(camera_ui_current(&context))camera_poll_cb(NULL);
finished:
    s_camera_dialog_dispatching=false;
}

static void camera_start(void)
{
    if(!camera_page_active() || !s_refresh_timer)return;
    camera_ui_context_t context=camera_ui_context();
    lv_timer_t *timer=s_refresh_timer;
    camera_set_status("Connecting | waiting for camera stream");
    if(camera_ui_current(&context) && timer==s_refresh_timer)camera_poll_cb(NULL);
}

static void camera_update_selector(void)
{
    if (!s_camera_selector) return;
    camera_ui_context_t context=camera_ui_context();
    lv_obj_t *selector=s_camera_selector;
    int profile_index = context.profile;
    camera_catalog_entry_t entry = {0};
    bool configured = camera_catalog_get(profile_index, s_camera_index, &entry) && entry.configured;
    if(!camera_ui_owned(&context) || selector!=s_camera_selector)return;
    if (!configured) {
        s_camera_index = 0;context.camera=0;
        configured = camera_catalog_get(profile_index, s_camera_index, &entry) && entry.configured;
        if(!camera_ui_owned(&context) || selector!=s_camera_selector)return;
    }

    lv_obj_t *selector_label = lv_obj_get_child(s_camera_selector, 0);
    if (selector_label) {
        char text[64];
        if (configured && entry.name[0]) {
            snprintf(text, sizeof(text), LV_SYMBOL_IMAGE " %s", entry.name);
        } else {
            snprintf(text, sizeof(text), LV_SYMBOL_IMAGE " Camera %u", (unsigned)(s_camera_index + 1));
        }
        if (s_selector_label != selector_label) {
            s_selector_label = selector_label;
            s_selector_text[0] = s_selector_rendered[0] = 0;
            camera_watch(selector_label);
        }
        if (lv_label_get_long_mode(selector_label) != LV_LABEL_LONG_DOT)
            lv_label_set_long_mode(selector_label, LV_LABEL_LONG_DOT);
        if(!camera_ui_owned(&context) || selector!=s_camera_selector || selector_label!=s_selector_label)return;
        int32_t line_height = lv_font_get_line_height(lv_obj_get_style_text_font(selector_label, 0));
        if (lv_obj_get_style_height(selector_label, 0) != line_height)
            lv_obj_set_height(selector_label, line_height);
        if(!camera_ui_owned(&context) || selector!=s_camera_selector || selector_label!=s_selector_label)return;
        /* LVGL can replace the displayed buffer with dots. Keep both the
         * logical caption and last rendered text to avoid refreshing it. */
        if (strcmp(s_selector_text, text) ||
            strcmp(s_selector_rendered, lv_label_get_text(selector_label))) {
            snprintf(s_selector_text, sizeof(s_selector_text), "%s", text);
            ui_value_set_text(selector_label, text);
            if(!camera_ui_owned(&context) || selector!=s_camera_selector || selector_label!=s_selector_label)return;
            lv_obj_update_layout(selector_label);
            if(!camera_ui_owned(&context) || selector!=s_camera_selector || selector_label!=s_selector_label)return;
            snprintf(s_selector_rendered, sizeof(s_selector_rendered), "%s", lv_label_get_text(selector_label));
        }
    }
    if(!camera_ui_owned(&context) || selector!=s_camera_selector)return;
    lv_obj_clear_flag(s_camera_selector, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_state(s_camera_selector, LV_STATE_DISABLED);
}

void ui_camera_set_setup_active(bool active)
{
    ++s_camera_ui_epoch;
    s_setup_active=active;
    camera_ui_context_t context=camera_ui_context();
    if(active){
        lv_timer_t *timer=s_refresh_timer;s_refresh_timer=NULL;
        if(timer)lv_timer_delete(timer);
        camera_close_popups();
        if(context.epoch!=s_camera_ui_epoch || s_setup_active!=active)return;
        /* Workers retain transport ownership until teardown completes. */
        camera_stream_request_stop();
        if(context.epoch!=s_camera_ui_epoch || s_setup_active!=active)return;
        if(context.root && !camera_ui_owned(&context))return;
        camera_release_frame();
        if(context.epoch!=s_camera_ui_epoch || s_setup_active!=active)return;
        camera_reset_timing();return;
    }
    if(camera_page_active() && !s_refresh_timer){
        s_refresh_timer=lv_timer_create(camera_poll_cb,100,NULL);
        camera_start();
    }
}

void ui_camera_refresh_catalog(void)
{
    if(!s_root || !s_camera_selector)return;
    camera_ui_context_t context=camera_ui_context();
    lv_obj_t *selector=s_camera_selector;
    camera_catalog_entry_t entry={0};
    bool configured=camera_catalog_get(context.profile,context.camera,&entry);
    if(!camera_ui_owned(&context) || selector!=s_camera_selector)return;
    if(!configured || !entry.configured)s_camera_index=0;
    camera_update_selector();
}

static void camera_picker_close_cb(lv_event_t *event)
{
    if (!s_camera_picker_popup || (event && !camera_owned_action(event,s_picker_close))) return;
    ++s_camera_dialog_epoch;
    lv_obj_t *popup = s_camera_picker_popup;
    s_camera_picker_popup = s_picker_feedback = s_picker_close = NULL;
    memset(s_picker_actions, 0, sizeof(s_picker_actions));
    s_picker_owner = 0;
    memset(s_picker_urls, 0, sizeof(s_picker_urls));
    if (popup) lv_obj_delete(popup);
}

static void camera_picker_select_cb(lv_event_t *event)
{
    if(!event || !s_camera_picker_popup)return;
    size_t index=(size_t)(uintptr_t)lv_event_get_user_data(event);
    if(index>=CAMERA_CATALOG_MAX_CAMERAS || !camera_owned_action(event,s_picker_actions[index]))return;
    if(s_picker_owner!=moonraker_config_generation()){
        ui_value_set_text(s_picker_feedback,"Printer changed. Close and reopen the camera picker.");return;
    }
    ++s_camera_ui_epoch;
    camera_ui_context_t context=camera_ui_context();
    uint32_t epoch=s_camera_dialog_epoch;lv_obj_t *popup=s_camera_picker_popup;
    char url[sizeof(s_picker_urls[index])];memcpy(url,s_picker_urls[index],sizeof(url));
    s_camera_dialog_dispatching=true;
    camera_catalog_entry_t entry={0};
    bool configured=camera_catalog_get(context.profile,index,&entry);
    if(!camera_review_current(&context,popup,epoch,false))goto finished;
    if(!configured || !entry.configured || strcmp(entry.stream_url,url)){
        ui_value_set_text(s_picker_feedback,"Camera configuration changed. Close and reopen the picker.");goto finished;
    }
    bool saved=camera_catalog_set_default(context.profile,index);
    if(!camera_review_current(&context,popup,epoch,false))goto finished;
    if(!saved){ui_value_set_text(s_picker_feedback,"Could not save the default camera. Retry or Close.");goto finished;}
    s_camera_index=index;context.camera=index;
    camera_stream_request_stop();
    if(!camera_review_current(&context,popup,epoch,false))goto finished;
    camera_release_frame();
    if(!camera_review_current(&context,popup,epoch,false))goto finished;
    camera_reset_timing();
    camera_update_selector();
    if(!camera_review_current(&context,popup,epoch,false))goto finished;
    camera_set_status("Connecting | switching camera");
    if(!camera_review_current(&context,popup,epoch,false))goto finished;
    camera_picker_close_cb(NULL);
    if(camera_ui_current(&context))camera_poll_cb(NULL);
finished:
    s_camera_dialog_dispatching=false;
}

static void camera_picker_open_cb(lv_event_t *event)
{
    if (!camera_owned_action(event,s_camera_selector) || s_camera_generation != moonraker_config_generation()) return;
    if (s_camera_picker_popup) { lv_obj_move_foreground(s_camera_picker_popup); return; }
    camera_ui_context_t context=camera_ui_context();
    int profile = context.profile;
    s_picker_owner = moonraker_config_generation();
    memset(s_picker_urls, 0, sizeof(s_picker_urls));
    s_camera_picker_popup = ui_form_dialog(ui_text("Select camera"), 680, 440, UI_POPUP_STANDARD);
    if (!s_camera_picker_popup) return;
    lv_obj_add_event_cb(s_camera_picker_popup, camera_dialog_deleted, LV_EVENT_DELETE, NULL);
    lv_obj_t *body = ui_form_body(s_camera_picker_popup);
    lv_obj_t *popup=s_camera_picker_popup;uint32_t epoch=s_camera_dialog_epoch;
    s_picker_close=ui_form_action(popup, UI_BUTTON_CLOSE, LV_SYMBOL_CLOSE " Close", camera_picker_close_cb, NULL);camera_watch(s_picker_close);
    size_t default_index = camera_catalog_default(profile);
    if(!camera_review_current(&context,popup,epoch,false))return;
    unsigned rows = 0;
    for (size_t index = 0; index < CAMERA_CATALOG_MAX_CAMERAS; ++index) {
        camera_catalog_entry_t entry;
        bool configured=camera_catalog_get(profile,index,&entry);
        if(!camera_review_current(&context,popup,epoch,false))return;
        if(!configured || !entry.configured)continue;
        memcpy(s_picker_urls[index], entry.stream_url, sizeof(s_picker_urls[index]));
        s_picker_urls[index][sizeof(s_picker_urls[index])-1] = 0;
        char label[64];
        snprintf(label, sizeof(label), "%s%s", entry.name[0] ? entry.name : "Camera", index == default_index ? " (default)" : "");
        lv_obj_t *button = ui_button_create(body, index == s_camera_index ? UI_BUTTON_PRIMARY : UI_BUTTON_SECONDARY, label);
        s_picker_actions[index]=button;camera_watch(button);
        lv_obj_set_size(button, LV_PCT(100), 56);
        lv_obj_add_event_cb(button, ui_form_action_fit, LV_EVENT_SIZE_CHANGED, NULL);
        lv_obj_add_event_cb(button, camera_picker_select_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)index);
        if (index == s_camera_index) lv_obj_add_state(button, LV_STATE_CHECKED);
        ++rows;
    }
    if (!rows) ui_form_text(body, "No configured cameras. Use Configure to add a camera.", UI_FONT_BODY, UI_TEXT_DIM);
    s_picker_feedback = ui_form_text(body, "", UI_FONT_CAPTION, UI_TEXT_DIM);camera_watch(s_picker_feedback);
    ui_form_finish(s_camera_picker_popup);
}

static void camera_select_next_cb(lv_event_t *event)
{
    camera_picker_open_cb(event);
}



static bool camera_show_ready(void)
{
    ++s_camera_show_depth;
    /* Install the page's timer before callbacks, but poll only after its
     * persisted default has been chosen and the outer Show has unwound. */
    if(!s_setup_active && !s_refresh_timer)s_refresh_timer=lv_timer_create(camera_poll_cb,100,NULL);
    camera_ui_context_t context=camera_ui_context();
    size_t index=camera_catalog_default(context.profile);
    if(!camera_ui_owned(&context))goto interrupted;
    s_camera_index=index;context.camera=index;
    camera_update_selector();
    if(!camera_ui_same_page(&context))goto interrupted;
    context.camera=s_camera_index;
    --s_camera_show_depth;
    camera_start();
    return camera_ui_owned(&context);
interrupted:
    --s_camera_show_depth;
    return false;
}

static bool camera_show_page(void)
{
    if(s_camera_retiring)return false;
    if(s_root && (!s_card || !s_image))camera_retire_page(false);
    if(s_root){
        ++s_camera_ui_epoch;
        lv_obj_clear_flag(s_root,LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_root);
        return camera_show_ready();
    }
    s_camera_generation=moonraker_config_generation();
    s_camera_index=0;
    ++s_camera_ui_epoch;
    s_root = lv_obj_create(lv_screen_active());
    lv_obj_add_event_cb(s_root, camera_page_deleted, LV_EVENT_DELETE, NULL);
    lv_obj_set_size(s_root, UI_PAGE_ROOT_WIDTH, UI_PAGE_ROOT_HEIGHT);
    lv_obj_set_pos(s_root, UI_PAGE_ROOT_X, UI_PAGE_ROOT_Y);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    ui_apply_root_style(s_root);

    s_card = lv_obj_create(s_root);
    lv_obj_add_event_cb(s_card, camera_surface_deleted, LV_EVENT_DELETE, NULL);
    lv_obj_t *card = s_card;
    lv_obj_set_size(card, UI_PAGE_ROOT_WIDTH, UI_PAGE_ROOT_HEIGHT);
    lv_obj_set_pos(card, 0, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(card, UI_CARD_DARK, 0);
    lv_obj_set_style_border_color(card, UI_BORDER_SOFT, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 0, 0);
    lv_obj_set_style_pad_all(card, 0, 0);

    s_image = lv_image_create(card);
    lv_obj_add_event_cb(s_image, camera_surface_deleted, LV_EVENT_DELETE, NULL);
    lv_obj_add_flag(s_image, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_image, camera_image_tap_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_size(s_image, UI_PAGE_ROOT_WIDTH, UI_PAGE_ROOT_HEIGHT);
    lv_obj_align(s_image, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(s_image, UI_BG_DEEP, 0);
    lv_obj_set_style_bg_opa(s_image, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_image, 0, 0);

    /* The title is created after the viewport and therefore stays readable. */
    if(!ui_theme_is_studio())ui_page_title_create(s_root, LV_SYMBOL_IMAGE " Camera", "Active printer live view");

    s_configure_button = ui_button_create(
        s_root, UI_BUTTON_SECONDARY, LV_SYMBOL_SETTINGS " Configure");
    lv_obj_set_size(s_configure_button, 196, 48);
    lv_obj_set_pos(s_configure_button, 20, ui_theme_is_studio() ? 358 : 464);
    ui_button_expand_touch_target(s_configure_button);camera_watch(s_configure_button);
    lv_obj_add_event_cb(s_configure_button, camera_configure_cb, LV_EVENT_CLICKED, NULL);

    s_view_button = ui_button_create(
        s_root, UI_BUTTON_SECONDARY, LV_SYMBOL_SETTINGS " View");
    lv_obj_set_size(s_view_button, 120, 48);
    lv_obj_set_pos(s_view_button, 226, ui_theme_is_studio() ? 358 : 464);
    ui_button_expand_touch_target(s_view_button);camera_watch(s_view_button);
    lv_obj_add_event_cb(s_view_button, camera_view_open_cb, LV_EVENT_CLICKED, NULL);

    s_camera_selector = ui_button_create(
        s_root, UI_BUTTON_SECONDARY, LV_SYMBOL_IMAGE " Camera 1");
    lv_obj_set_size(s_camera_selector, 180, 48);
    lv_obj_set_pos(s_camera_selector, 356, ui_theme_is_studio() ? 358 : 464);
    ui_button_expand_touch_target(s_camera_selector);camera_watch(s_camera_selector);
    lv_obj_add_event_cb(s_camera_selector, camera_select_next_cb, LV_EVENT_CLICKED, NULL);
    /* Catalog callbacks run only after every page field has been created. */

    s_fullscreen_button = ui_button_create(
        s_root, UI_BUTTON_OUTLINED, LV_SYMBOL_IMAGE " Fullscreen");
    lv_obj_set_size(s_fullscreen_button, 210, 48);
    lv_obj_set_pos(s_fullscreen_button, 624, ui_theme_is_studio() ? 358 : 464);
    ui_button_expand_touch_target(s_fullscreen_button);camera_watch(s_fullscreen_button);
    lv_obj_add_event_cb(s_fullscreen_button, camera_fullscreen_cb, LV_EVENT_CLICKED, NULL);

    s_retry_button = ui_button_create(s_root, UI_BUTTON_SECONDARY, "Retry");camera_watch(s_retry_button);
    lv_obj_set_size(s_retry_button, 180, 44);
    lv_obj_set_pos(s_retry_button, 20, ui_theme_is_studio() ? 74 : 410);
    lv_obj_add_flag(s_retry_button, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_retry_button, camera_retry_cb, LV_EVENT_CLICKED, NULL);

    s_status = lv_label_create(s_root);camera_watch(s_status);
    ui_apply_text_body(s_status);
    ui_apply_label_dim(s_status);
    lv_obj_set_width(s_status, UI_PAGE_ROOT_WIDTH - 40);
    lv_obj_set_pos(s_status, 20, 106);
    lv_label_set_long_mode(s_status, LV_LABEL_LONG_WRAP);
    lv_label_set_text(s_status, ui_text("Connecting to configured camera..."));

    if(ui_theme_is_studio())camera_set_viewport(false);
    return camera_show_ready();
}

void ui_camera_show(void)
{
    (void)camera_show_page();
}

void ui_camera_show_fullscreen(void)
{
    if(camera_show_page() && camera_page_active() && !s_fullscreen)camera_set_viewport(true);
}


void ui_camera_destroy(void)
{
    camera_retire_page(false);
}


void ui_camera_hide(void)
{
    ++s_camera_ui_epoch;
    camera_ui_context_t context=camera_ui_context();
    if(s_fullscreen)camera_set_viewport(false);
    if(context.root && !camera_ui_owned(&context))return;
    if(s_root)lv_obj_add_flag(s_root,LV_OBJ_FLAG_HIDDEN);
    lv_timer_t *timer=s_refresh_timer;s_refresh_timer=NULL;
    if(timer)lv_timer_delete(timer);
    camera_close_popups();
    if(context.epoch!=s_camera_ui_epoch)return;
    camera_stream_request_stop();
    if(context.epoch!=s_camera_ui_epoch)return;
    if(context.root && !camera_ui_owned(&context))return;
    camera_release_frame();
    if(context.epoch!=s_camera_ui_epoch)return;
    camera_reset_timing();
    ui_shell_raise_topbar();
    ui_shell_raise_nav();
}
