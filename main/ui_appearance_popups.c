#include "ui_widgets.h"
#include "ui_appearance_popups.h"

#include "theme_manager.h"
#include "ui_popup.h"
#include "ui_theme.h"
#include "ui_scroll_anchor.h"
#include "ui_value_update.h"
#include "ui_form_dialog.h"

#include "esp_log.h"

#include <stddef.h>
#include <stdint.h>

static const char *TAG = "ui_appearance";
static lv_obj_t *s_popup = NULL;
static ui_appearance_changed_cb_t s_changed_cb = NULL;
static ui_appearance_changed_cb_t s_pending_cb = NULL;
static bool s_change_pending = false;
typedef enum {APPEARANCE_NONE,APPEARANCE_ACCENT,APPEARANCE_DENSITY,APPEARANCE_ACCESSIBILITY} appearance_kind_t;
static appearance_kind_t s_kind;
static lv_obj_t *s_body, *s_feedback;
static lv_obj_t *s_rows[UI_ACCENT_COUNT > 4 ? UI_ACCENT_COUNT : 4];
static uint32_t s_appearance_epoch;
static bool s_apply_dispatching;
static void appearance_field_deleted(lv_event_t *event)
{
    lv_obj_t *target=lv_event_get_target_obj(event);
    if(target==s_body){s_body=NULL;++s_appearance_epoch;}
    if(target==s_feedback)s_feedback=NULL;
    for(size_t i=0;i<sizeof(s_rows)/sizeof(s_rows[0]);i++)if(s_rows[i]==target)s_rows[i]=NULL;
}
static void appearance_field_own(lv_obj_t *field)
{if(field)lv_obj_add_event_cb(field,appearance_field_deleted,LV_EVENT_DELETE,NULL);}
static void appearance_feedback(const char *text)
{if(s_feedback){ui_value_set_text(s_feedback,text);lv_obj_scroll_to_view(s_feedback,LV_ANIM_OFF);}}
static bool appearance_target(lv_event_t *event,appearance_kind_t kind,unsigned index,unsigned count)
{
    return event && lv_event_get_code(event)==LV_EVENT_CLICKED &&
        !s_change_pending && !s_apply_dispatching && s_popup && s_body &&
        s_kind==kind && index<count && s_rows[index] &&
        lv_event_get_target_obj(event)==s_rows[index] &&
        !lv_obj_has_state(s_rows[index],LV_STATE_DISABLED);
}
static void appearance_deleted(lv_event_t *event)
{
    if(lv_event_get_target_obj(event)!=s_popup)return;
    ++s_appearance_epoch;
    s_popup=s_body=s_feedback=NULL;s_kind=APPEARANCE_NONE;s_changed_cb=NULL;
    memset(s_rows,0,sizeof(s_rows));
}

static void appearance_close(void)
{
    lv_obj_t *popup=s_popup;
    if(popup)++s_appearance_epoch;
    s_popup=s_body=s_feedback=NULL;s_changed_cb=NULL;s_kind=APPEARANCE_NONE;
    memset(s_rows,0,sizeof(s_rows));
    if(popup)lv_obj_delete(popup);
}

static void appearance_close_cb(lv_event_t *event)
{
    (void)event;
    appearance_close();
}

static void appearance_apply_async(void *user_data)
{
    (void)user_data;
    if(!s_change_pending || s_apply_dispatching)return;
    s_apply_dispatching=true;
    ui_appearance_changed_cb_t callback=s_pending_cb;
    appearance_kind_t kind=s_kind;
    bool reopen=s_popup && s_body && kind!=APPEARANCE_NONE;
    ui_scroll_anchor_t saved=ui_scroll_anchor_capture(s_body);
    s_pending_cb=NULL;s_change_pending=false;
    uint32_t expected=s_appearance_epoch+(s_popup?1U:0U);
    appearance_close();
    /* Deletion callbacks may create a replacement editor. */
    if(s_appearance_epoch!=expected || s_popup)reopen=false;
    uint32_t epoch=s_appearance_epoch;
    if(callback)callback();
    /* Notify an accepted setting once, but leave callback-created UI alone. */
    if(reopen && epoch==s_appearance_epoch && !s_popup){
        if(kind==APPEARANCE_ACCENT)ui_appearance_popups_show_accent(callback);
        else if(kind==APPEARANCE_DENSITY)ui_appearance_popups_show_density(callback);
        else if(kind==APPEARANCE_ACCESSIBILITY)ui_appearance_popups_show_accessibility(callback);
        if(s_popup && s_body && s_kind==kind)ui_scroll_anchor_restore(s_body,saved);
    }
    s_apply_dispatching=false;
}

static bool appearance_schedule_apply(void)
{
    if (s_change_pending || s_apply_dispatching || !s_popup || !s_body) return false;
    s_pending_cb = s_changed_cb;
    s_change_pending = true;
    if(lv_async_call(appearance_apply_async, NULL)!=LV_RESULT_OK) {
        /* Do not persist a change without a queued safe rebuild. */
        s_change_pending=false;s_pending_cb=NULL;
        appearance_feedback("Could not queue refresh. Your choice was not saved; try again.");
        ESP_LOGE(TAG,"Could not schedule appearance refresh");
        return false;
    }
    return true;
}

static void appearance_cancel_apply(void)
{
    lv_async_call_cancel(appearance_apply_async,NULL);
    s_change_pending=false;s_pending_cb=NULL;
}

static lv_obj_t *appearance_popup(const char *title,
                                  const char *message,
                                  int32_t width,
                                  int32_t height)
{
    int32_t available=lv_display_get_horizontal_resolution(NULL)-32;
    if(width>available)width=available;
    available=lv_display_get_vertical_resolution(NULL)-32;
    if(height>available)height=available;
    s_popup=ui_popup_create_on_screen(width,height,UI_POPUP_STANDARD);
    if(!s_popup){s_changed_cb=NULL;s_kind=APPEARANCE_NONE;return NULL;}
    ++s_appearance_epoch;
    lv_obj_set_style_bg_opa(s_popup,LV_OPA_COVER,0);
    lv_obj_set_style_shadow_width(s_popup,0,0);
    lv_obj_add_event_cb(s_popup,appearance_deleted,LV_EVENT_DELETE,NULL);
    lv_obj_set_flex_flow(s_popup,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_popup,UI_PAD_POPUP,0);
    lv_obj_set_style_pad_row(s_popup,UI_GAP_ROW,0);
    lv_obj_t *heading=lv_label_create(s_popup);lv_label_set_text(heading,title);
    ui_apply_custom_label_style(heading,UI_FONT_TITLE,UI_TEXT);
    lv_obj_set_width(heading,LV_PCT(100));lv_label_set_long_mode(heading,LV_LABEL_LONG_WRAP);
    ui_form_divide_title(s_popup);
    s_body=lv_obj_create(s_popup);ui_apply_surface_role(s_body,UI_SURFACE_TRANSPARENT);
    lv_obj_set_style_bg_color(s_body,lv_obj_get_style_bg_color(s_popup,0),0);
    lv_obj_set_style_bg_opa(s_body,LV_OPA_COVER,0);
    lv_obj_set_size(s_body,LV_PCT(100),0);lv_obj_set_flex_grow(s_body,1);
    lv_obj_set_flex_flow(s_body,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_all(s_body,0,0);
    lv_obj_set_style_pad_row(s_body,UI_GAP_ROW,0);lv_obj_set_scroll_dir(s_body,LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_body,LV_SCROLLBAR_MODE_OFF);
    appearance_field_own(s_body);
    lv_obj_t *hint=lv_label_create(s_body);lv_label_set_text(hint,message);
    s_feedback=hint;appearance_field_own(hint);
    ui_apply_custom_label_style(hint,UI_FONT_CAPTION,UI_TEXT_DIM);
    lv_obj_set_width(hint,LV_PCT(100));lv_label_set_long_mode(hint,LV_LABEL_LONG_WRAP);
    lv_obj_t *footer=lv_obj_create(s_popup);lv_obj_remove_style_all(footer);
    ui_apply_divider_border(footer,LV_BORDER_SIDE_TOP);lv_obj_set_style_pad_top(footer,8,0);
    lv_obj_set_size(footer,LV_PCT(100),58);lv_obj_clear_flag(footer,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *close=ui_popup_add_action_at(footer,UI_POPUP_ACTION_CLOSE,LV_SYMBOL_CLOSE " Close",0,0,160,48,appearance_close_cb,NULL,NULL);
    lv_obj_center(close);
    return s_popup;
}

static void accent_select_cb(lv_event_t *event)
{
    if(!event)return;
    ui_accent_id_t accent=(ui_accent_id_t)(uintptr_t)lv_event_get_user_data(event);
    if(!appearance_target(event,APPEARANCE_ACCENT,(unsigned)accent,UI_ACCENT_COUNT))return;
    if(accent==theme_manager_accent())return;
    if(!appearance_schedule_apply())return;
    uint32_t epoch=s_appearance_epoch;
    s_apply_dispatching=true;
    bool accepted=theme_manager_select_accent(accent);
    s_apply_dispatching=false;
    if (!accepted) {
        appearance_cancel_apply();
        if(epoch==s_appearance_epoch)appearance_feedback("Could not save this choice. Try again or Close.");
        ESP_LOGE(TAG, "Could not select accent %d", (int)accent);
        return;
    }
}

void ui_appearance_popups_show_accent(ui_appearance_changed_cb_t changed_cb)
{
    if(s_change_pending)return;
    if(s_popup && s_kind==APPEARANCE_ACCENT){lv_obj_move_foreground(s_popup);return;}
    if(s_popup){uint32_t expected=s_appearance_epoch+1;appearance_close();if(s_appearance_epoch!=expected || s_popup)return;}
    s_changed_cb=changed_cb;
    s_kind=APPEARANCE_ACCENT;
    const int32_t row_height = ui_theme_density_metric(38, 44, 50);
    const int32_t row_step = ui_theme_density_metric(45, 51, 57);
    const int32_t list_height = 16 + (UI_ACCENT_COUNT - 1) * row_step + row_height;
    const int32_t popup_height = list_height + 170;
    if (!appearance_popup(
            "Accent color",
            "Choose an accent. Status, warning, and danger colors remain semantic.",
            720, popup_height)) return;

    lv_obj_t *list = s_body;
    if (!list) { appearance_close(); return; }

    ui_accent_id_t active = theme_manager_accent();
    for (int accent = 0; accent < UI_ACCENT_COUNT; ++accent) {
        lv_obj_t *row = ui_popup_add_selectable_row(
            list,
            theme_manager_accent_name((ui_accent_id_t)accent),
            8, 8 + accent * row_step, 640, row_height,
            accent_select_cb, (void *)(uintptr_t)accent);
        s_rows[accent]=row;appearance_field_own(row);
        ui_popup_set_selectable_row_selected(row, (ui_accent_id_t)accent == active);
        lv_obj_set_size(row,LV_PCT(100),LV_SIZE_CONTENT);
        lv_obj_set_style_min_height(row,row_height<48?48:row_height,0);
        lv_obj_set_style_pad_hor(row,UI_PAD_CARD,0);
        lv_obj_set_style_pad_ver(row,10,0);
        lv_obj_set_style_pad_row(row,4,0);
        lv_obj_set_flex_flow(row,LV_FLEX_FLOW_COLUMN);
        lv_obj_t *label=lv_obj_get_child(row,0);
        lv_obj_set_width(label,LV_PCT(100));lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_LEFT,0);
    }
}

static void density_select_cb(lv_event_t *event)
{
    if(!event)return;
    ui_density_id_t density=(ui_density_id_t)(uintptr_t)lv_event_get_user_data(event);
    if(!appearance_target(event,APPEARANCE_DENSITY,(unsigned)density,UI_DENSITY_COUNT))return;
    if(density==theme_manager_density())return;
    if(!appearance_schedule_apply())return;
    uint32_t epoch=s_appearance_epoch;
    s_apply_dispatching=true;
    bool accepted=theme_manager_select_density(density);
    s_apply_dispatching=false;
    if (!accepted) {
        appearance_cancel_apply();
        if(epoch==s_appearance_epoch)appearance_feedback("Could not save this choice. Try again or Close.");
        ESP_LOGE(TAG, "Could not select density %d", (int)density);
        return;
    }
}

void ui_appearance_popups_show_density(ui_appearance_changed_cb_t changed_cb)
{
    if(s_change_pending)return;
    if(s_popup && s_kind==APPEARANCE_DENSITY){lv_obj_move_foreground(s_popup);return;}
    if(s_popup){uint32_t expected=s_appearance_epoch+1;appearance_close();if(s_appearance_epoch!=expected || s_popup)return;}
    s_changed_cb=changed_cb;
    s_kind=APPEARANCE_DENSITY;
    const int32_t row_height = ui_theme_density_metric(40, 48, 56);
    const int32_t row_step = ui_theme_density_metric(47, 56, 64);
    const int32_t list_height = 16 + 2 * row_step + row_height;
    const int32_t popup_height = list_height + 170;
    if (!appearance_popup(
            "Display density",
            "Choose typography and spacing independently of the theme.",
            640, popup_height)) return;

    lv_obj_t *list = s_body;
    if (!list) { appearance_close(); return; }

    ui_density_id_t active = theme_manager_density();
    for (int density = 0; density < UI_DENSITY_COUNT; ++density) {
        lv_obj_t *row = ui_popup_add_selectable_row(
            list,
            theme_manager_density_name((ui_density_id_t)density),
            8, 8 + density * row_step, 560, row_height,
            density_select_cb, (void *)(uintptr_t)density);
        s_rows[density]=row;appearance_field_own(row);
        ui_popup_set_selectable_row_selected(row, (ui_density_id_t)density == active);
        lv_obj_set_size(row,LV_PCT(100),LV_SIZE_CONTENT);
        lv_obj_set_style_min_height(row,row_height<48?48:row_height,0);
        lv_obj_set_style_pad_hor(row,UI_PAD_CARD,0);
        lv_obj_set_style_pad_ver(row,10,0);
        lv_obj_set_style_pad_row(row,4,0);
        lv_obj_set_flex_flow(row,LV_FLEX_FLOW_COLUMN);
        lv_obj_t *label=lv_obj_get_child(row,0);
        lv_obj_set_width(label,LV_PCT(100));lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_LEFT,0);
    }
}

static void accessibility_toggle_cb(lv_event_t *event)
{
    if(!event)return;
    int option=(int)(uintptr_t)lv_event_get_user_data(event);
    if(!appearance_target(event,APPEARANCE_ACCESSIBILITY,(unsigned)option,4))return;
    ui_accessibility_t value = theme_manager_accessibility();

    if (option == 0) value.large_text = !value.large_text;
    else if (option == 1) value.high_contrast = !value.high_contrast;
    else if (option == 2) value.reduced_transparency = !value.reduced_transparency;
    else if (option == 3) value.reduced_motion = !value.reduced_motion;
    else return;

    if(!appearance_schedule_apply())return;
    uint32_t epoch=s_appearance_epoch;
    s_apply_dispatching=true;
    bool accepted=theme_manager_set_accessibility(value);
    s_apply_dispatching=false;
    if (!accepted) {
        appearance_cancel_apply();
        if(epoch==s_appearance_epoch)appearance_feedback("Could not save this choice. Try again or Close.");
        ESP_LOGE(TAG, "Could not save accessibility option %d", option);
        return;
    }
}

void ui_appearance_popups_show_accessibility(
    ui_appearance_changed_cb_t changed_cb)
{
    if(s_change_pending)return;
    if(s_popup && s_kind==APPEARANCE_ACCESSIBILITY){lv_obj_move_foreground(s_popup);return;}
    if(s_popup){uint32_t expected=s_appearance_epoch+1;appearance_close();if(s_appearance_epoch!=expected || s_popup)return;}
    s_changed_cb=changed_cb;
    s_kind=APPEARANCE_ACCESSIBILITY;
    const int32_t row_height = ui_theme_density_metric(40, 48, 56);
    const int32_t row_step = ui_theme_density_metric(47, 56, 64);
    /* Reserve both the option name and state caption at the active text size. */
    const int32_t popup_height = 202 + 4 * (UI_FONT_BODY_LARGE->line_height +
        UI_FONT_CAPTION->line_height + 28) + 4 * UI_GAP_ROW;
    if (!appearance_popup(
            "Accessibility",
            "Tap an option to toggle it. The interface will refresh immediately.",
            720, popup_height)) return;

    lv_obj_t *list = s_body;
    if (!list) { appearance_close(); return; }

    ui_accessibility_t value = theme_manager_accessibility();
    const char *labels[] = {
        "Large text",
        "High contrast",
        "Reduced transparency",
        "Reduced motion",
    };
    const bool enabled[] = {
        value.large_text, value.high_contrast,
        value.reduced_transparency, value.reduced_motion,
    };

    for (int option = 0; option < 4; ++option) {
        lv_obj_t *row = ui_popup_add_selectable_row(
            list, labels[option], 8, 8 + option * row_step,
            640, row_height,
            accessibility_toggle_cb, (void *)(uintptr_t)option);
        s_rows[option]=row;appearance_field_own(row);
        ui_popup_set_selectable_row_selected(row, enabled[option]);
        lv_obj_set_size(row,LV_PCT(100),LV_SIZE_CONTENT);
        lv_obj_set_style_min_height(row,row_height<48?48:row_height,0);
        lv_obj_set_style_pad_hor(row,UI_PAD_CARD,0);
        lv_obj_set_style_pad_ver(row,10,0);
        lv_obj_set_style_pad_row(row,4,0);
        lv_obj_set_flex_flow(row,LV_FLEX_FLOW_COLUMN);
        lv_obj_t *label=lv_obj_get_child(row,0);
        lv_obj_set_width(label,LV_PCT(100));lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_LEFT,0);
        lv_obj_t *state=ui_form_text(row,enabled[option]?"On":"Off",UI_FONT_CAPTION,UI_TEXT_DIM);
        lv_obj_set_style_text_align(state,LV_TEXT_ALIGN_LEFT,0);
    }
}

void ui_appearance_popups_close_all(void)
{
    appearance_close();
}
