#include "ui_bed_mesh.h"
#include "printer_connection_policy.h"
#include "moonraker_config_controller.h"
#include "ui_value_update.h"
#include "ui_text.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bed_mesh_controller.h"
#include "ui_bed_mesh_gestures.h"
#include "ui_bed_mesh_renderer.h"
#include "ui_bed_mesh_profiles.h"
#include "esp_heap_caps.h"
#include "ui_button.h"
#include "ui_popup.h"
#include "ui_form_dialog.h"
#include "ui_widgets.h"
#include "ui_theme.h"
#include "ui_page_geometry.h"
#define CW (ui_theme_is_studio()?976:800)
#define CH (ui_theme_is_studio()?298:324)
#define VALUE_CAP ((size_t)BED_MESH_MAX_ROWS*BED_MESH_MAX_COLS)
typedef struct {
    lv_obj_t *popup;
    lv_obj_t *canvas;
    lv_obj_t *stats;
    lv_obj_t *metrics;
    lv_obj_t *surface_grid_button;
    lv_obj_t *surface_grid_label;
    lv_obj_t *calibrate_confirm;
    lv_obj_t *calibrate_button;
    lv_timer_t *connection_timer;
    uint32_t owner_generation;
    uint16_t *buf;
    float *values;
    bed_mesh_profile_name_t *profile_names;
    bed_mesh_snapshot_t mesh;
    ui_bed_mesh_view_transform_t view;
    float zscale;
    bool surface_grid_visible;
    ui_bed_mesh_command_cb_t command;
} ui_t;
static ui_t s;

void printerhmi_bed_mesh_multitouch_update(
    uint8_t count,
    int32_t x0,
    int32_t y0,
    int32_t x1,
    int32_t y1)
{
    if (!s.popup) {
        return;
    }

    ui_bed_mesh_gestures_multitouch_update(
        count,
        x0,
        y0,
        x1,
        y1);
}

static void *alloc_psram_first(size_t n,size_t z){void*p=heap_caps_calloc(n,z,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!p)p=heap_caps_calloc(n,z,MALLOC_CAP_8BIT);return p;}
static void render(void)
{
    ui_bed_mesh_renderer_config_t config = {
        .canvas = s.canvas,
        .buf = s.buf,
        .values = s.values,
        .mesh = s.mesh,
        .view = s.view,
        .zscale = s.zscale,
        .surface_grid_visible =
            s.surface_grid_visible,
        .interaction_active =
            ui_bed_mesh_gestures_is_active(),
    };

    ui_bed_mesh_renderer_render(&config);
}

static const char *mesh_action_reason(void){
    moonraker_state_t live;moonraker_state_snapshot(&live);
    return s.owner_generation!=moonraker_config_generation()?"Printer changed - reopen Bed Mesh.":printer_idle_reason(&live);
}
static void mesh_caption_deleted(lv_event_t *event)
{
    lv_obj_t *object=lv_event_get_target_obj(event);
    if(object==s.stats)s.stats=NULL;
    if(object==s.metrics)s.metrics=NULL;
}
static void stats(void)
{
    if(!s.stats)return;
    moonraker_state_t live;moonraker_state_snapshot(&live);
    const char *reason=s.owner_generation!=moonraker_config_generation()?"Printer changed - reopen Bed Mesh.":printer_connection_reason(&live);
    if(s.owner_generation!=moonraker_config_generation()||!s.mesh.valid){
        ui_value_set_text(s.stats,reason[0]?reason:"No active bed mesh. Calibrate or load a profile.");
        if(s.metrics)ui_value_set_text(s.metrics,"");
        return;
    }
    char summary[160];
    snprintf(summary,sizeof(summary),"Profile %s | %u x %u | Z %.0fx%s",
        s.mesh.profile_name[0]?s.mesh.profile_name:"Current mesh",s.mesh.cols,s.mesh.rows,s.zscale,
        reason[0]?" | Last reported mesh":s.mesh.truncated?" | Truncated":"");
    ui_value_set_text(s.stats,summary);
    if(s.metrics){
        char height[128];
        snprintf(height,sizeof(height),"Min %+.3f mm  |  Max %+.3f mm  |  Range %.3f mm",
            s.mesh.minimum,s.mesh.maximum,s.mesh.range);
        ui_value_set_text(s.metrics,height);
    }
}
static void mesh_connection_refresh(lv_timer_t *timer){
    (void)timer;if(!s.popup)return;
    if(s.calibrate_button){if(mesh_action_reason()[0])lv_obj_add_state(s.calibrate_button,LV_STATE_DISABLED);else lv_obj_remove_state(s.calibrate_button,LV_STATE_DISABLED);}
    ui_bed_mesh_profiles_refresh_availability();
    if(s.owner_generation==moonraker_config_generation())ui_bed_mesh_refresh();
    stats();
}
static uint32_t mesh_content_hash(void){
    uint32_t hash=2166136261u;const unsigned char *bytes=(const unsigned char *)s.values;size_t count=s.mesh.valid?(size_t)s.mesh.rows*s.mesh.cols*sizeof(float):0;
    for(size_t i=0;i<count;i++){hash^=bytes[i];hash*=16777619u;}
    bytes=(const unsigned char *)s.profile_names;count=s.mesh.profile_count*sizeof(bed_mesh_profile_name_t);for(size_t i=0;bytes&&i<count;i++){hash^=bytes[i];hash*=16777619u;}return hash;
}
void ui_bed_mesh_refresh(void)
{
    if (!s.popup) {
        return;
    }

    bed_mesh_snapshot_t previous=s.mesh;uint32_t previous_hash=mesh_content_hash();
    if (!s.values) {
        s.values =
            alloc_psram_first(
                VALUE_CAP,
                sizeof(float));
    }

    if (!s.profile_names) {
        s.profile_names =
            alloc_psram_first(
                BED_MESH_MAX_PROFILES,
                sizeof(*s.profile_names));
    }

    if (!s.values ||
        !bed_mesh_controller_snapshot(
            &s.mesh,
            s.values,
            VALUE_CAP,
            s.profile_names,
            s.profile_names
                ? BED_MESH_MAX_PROFILES
                : 0)) {
        memset(&s.mesh, 0, sizeof(s.mesh));
        if(!memcmp(&previous,&s.mesh,sizeof(s.mesh))&&previous_hash==mesh_content_hash()){stats();return;}
        ui_bed_mesh_profiles_update(&s.mesh);
        stats();

        if (s.canvas) {
            lv_canvas_fill_bg(
                s.canvas,
                UI_BED_MESH_BG,
                LV_OPA_COVER);
        }

        return;
    }

    if(!memcmp(&previous,&s.mesh,sizeof(s.mesh))&&previous_hash==mesh_content_hash()){stats();return;}
    ui_bed_mesh_profiles_update(&s.mesh);
    stats();
    render();
}

static void update_surface_grid_button(void)
{
    if (s.surface_grid_label) {
        lv_label_set_text(
            s.surface_grid_label,
            ui_text("Grid"));
    }

    if (s.surface_grid_button) {
        ui_button_apply_kind(
            s.surface_grid_button,
            s.surface_grid_visible
                ? UI_BUTTON_PRIMARY
                : UI_BUTTON_SECONDARY);
    }
}

static void surface_grid_cb(lv_event_t *e)
{
    (void)e;
    s.surface_grid_visible = !s.surface_grid_visible;
    update_surface_grid_button();
    render();
}

static void reset_cb(lv_event_t *e)
{
    (void)e;
    s.zscale = 20.0f;
    stats();
    ui_bed_mesh_gestures_reset();
}

static void plus_cb(lv_event_t *e)
{
    (void)e;
    ui_bed_mesh_gestures_zoom_in();
}

static void minus_cb(lv_event_t *e)
{
    (void)e;
    ui_bed_mesh_gestures_zoom_out();
}

static void close_calibrate_confirm_cb(lv_event_t *e)
{
    (void)e;

    if (s.calibrate_confirm) {
        lv_obj_t *popup = s.calibrate_confirm;
        s.calibrate_confirm = NULL;
        lv_obj_delete(popup);
    }
}

static void confirm_calibrate_cb(lv_event_t *e)
{
    (void)e;

    if(mesh_action_reason()[0]){ui_cal_dialog_notice(UI_STATUS_WARNING,"BED MESH NOT READY",mesh_action_reason());return;}
    /*
     * Preserve the command callback before closing the modal. The mesh
     * viewer remains open so its live snapshot can refresh after probing.
     */
    ui_bed_mesh_command_cb_t command = s.command;
    close_calibrate_confirm_cb(NULL);

    if (command) {
        command("BED_MESH_CALIBRATE");
    }
}

static void calibrate_cb(lv_event_t *e)
{
    (void)e;

    if(mesh_action_reason()[0]){stats();return;}
    if (s.calibrate_confirm) {
        lv_obj_move_foreground(s.calibrate_confirm);
        return;
    }

    /* Keep the review owned by the viewer, with a scrolling warning and
     * pinned actions so larger text cannot overlap the controls. */
    lv_obj_t *parent = s.popup ? s.popup : lv_screen_active();
    int32_t width = lv_obj_get_width(parent) - 32;
    int32_t height = lv_obj_get_height(parent) - 32;
    if (width > 560) width = 560;
    if (height > 340) height = 340;
    s.calibrate_confirm = ui_popup_create(parent, width, height, UI_POPUP_DANGER);
    if (!s.calibrate_confirm) return;
    lv_obj_set_flex_flow(s.calibrate_confirm, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s.calibrate_confirm, UI_PAD_POPUP, 0);
    lv_obj_set_style_pad_row(s.calibrate_confirm, UI_GAP_ROW, 0);
    lv_obj_t *title = ui_form_text(s.calibrate_confirm,
        ui_text("Calibrate bed mesh?"), UI_FONT_TITLE, UI_TEXT_BRIGHT);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    ui_form_divide_title(s.calibrate_confirm);

    lv_obj_t *body = lv_obj_create(s.calibrate_confirm);
    lv_obj_remove_style_all(body);
    lv_obj_set_size(body, LV_PCT(100), 0);
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(body, UI_GAP_ROW, 0);
    lv_obj_set_scroll_dir(body, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_OFF);
    lv_obj_t *warning = ui_form_text(body,
        "This will move the toolhead across the bed and probe multiple points.",
        UI_FONT_BODY, UI_TEXT);
    lv_obj_set_style_text_line_space(warning, 4, 0);
    lv_obj_t *readiness = ui_form_text(body,
        "Make sure the bed is clear and the printer is ready.",
        UI_FONT_CAPTION, UI_TEXT_MUTED);
    lv_obj_set_style_text_line_space(readiness, 4, 0);

    lv_obj_t *footer = lv_obj_create(s.calibrate_confirm);
    ui_responsive_column(footer);
    ui_apply_divider_border(footer, LV_BORDER_SIDE_TOP);
    lv_obj_set_style_pad_top(footer, 8, 0);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_column(footer, UI_GAP_ROW, 0);
    ui_form_action(s.calibrate_confirm, UI_BUTTON_CANCEL, LV_SYMBOL_LEFT " Back",
        close_calibrate_confirm_cb, NULL);
    ui_form_action(s.calibrate_confirm, UI_BUTTON_DANGER, LV_SYMBOL_PLAY " Start probing",
        confirm_calibrate_cb, NULL);
    ui_form_finish(s.calibrate_confirm);
}

void ui_bed_mesh_close(void)
{
    /*
     * The shared confirmation is a top-layer sibling, not a child of the
     * mesh viewer. Close it first so no modal surface can outlive its owner.
     */
    if(s.connection_timer)lv_timer_delete(s.connection_timer);
    s.connection_timer=NULL;
    close_calibrate_confirm_cb(NULL);
    ui_bed_mesh_profiles_close();

    ui_bed_mesh_gestures_close();

    if (s.popup) {
        lv_obj_t *popup=s.popup;s.popup=NULL;lv_obj_delete(popup);
    }
    if (s.buf) {
        heap_caps_free(s.buf);
    }
    if (s.values) {
        heap_caps_free(s.values);
    }
    if (s.profile_names) {
        heap_caps_free(s.profile_names);
    }

    memset(&s, 0, sizeof(s));
}

static void mesh_deleted(lv_event_t *event){
    if(lv_event_get_target_obj(event)!=s.popup)return;
    s.popup=NULL;ui_bed_mesh_close();
}

void ui_bed_mesh_show(ui_bed_mesh_command_cb_t command)
{
    ui_bed_mesh_close();
    s.command = command;
    s.owner_generation=moonraker_config_generation();
    s.zscale = 20.0f;
    ui_bed_mesh_gestures_init(
        &s.view,
        render,
        CW,
        CH);

    /*
     * Bed Mesh is a first-class shell destination. Keep the existing
     * viewer owner but host it in the standard application page frame.
     */
    s.popup = lv_obj_create(lv_screen_active());
    if (!s.popup) {
        return;
    }

    lv_obj_add_event_cb(s.popup,mesh_deleted,LV_EVENT_DELETE,NULL);
    ui_bed_mesh_profiles_init(
        s.popup,
        s.command);

    lv_obj_set_size(
        s.popup,
        UI_PAGE_ROOT_WIDTH,
        UI_PAGE_ROOT_HEIGHT);
    lv_obj_set_pos(
        s.popup,
        UI_PAGE_ROOT_X,
        UI_PAGE_ROOT_Y);
    lv_obj_clear_flag(
        s.popup,
        LV_OBJ_FLAG_SCROLLABLE);
    ui_apply_root_style(s.popup);

    if(ui_theme_is_studio()) {
        /* Use eight pixels of the gap above the navigation shelf. */
        lv_obj_set_height(s.popup,432);
        lv_obj_t *title=ui_popup_add_caption(s.popup,"Bed mesh",0,0,976);
        ui_apply_custom_label_style(title,UI_FONT_TITLE,UI_TEXT_BRIGHT);
        ui_popup_add_header_divider(s.popup,36);
        s.stats=ui_popup_add_caption(s.popup,ui_text(""),184,6,792);
    }else {
        lv_obj_t *header=ui_create_operator_page_header(s.popup,"Bed mesh","",UI_PAGE_RAIL_WIDTH);
        s.stats=lv_obj_get_child(header,1);
        /* A compact banner gives the interactive graph another twenty pixels. */
        lv_obj_set_height(header,66);
        lv_obj_set_y(lv_obj_get_child(header,0),8);
        lv_obj_set_y(s.stats,42);
    }

    ui_apply_custom_label_style(s.stats,UI_FONT_CAPTION,UI_TEXT_DIM);
    lv_label_set_long_mode(s.stats,LV_LABEL_LONG_CLIP);
    lv_obj_set_height(s.stats,UI_FONT_CAPTION->line_height);
    lv_obj_add_event_cb(s.stats,mesh_caption_deleted,LV_EVENT_DELETE,NULL);
    s.metrics=ui_popup_add_caption(s.popup,"",ui_theme_is_studio()?0:20,ui_theme_is_studio()?344:430,CW);
    ui_apply_custom_label_style(s.metrics,UI_FONT_CAPTION,UI_TEXT_DIM);
    lv_label_set_long_mode(s.metrics,LV_LABEL_LONG_CLIP);
    lv_obj_set_height(s.metrics,UI_FONT_CAPTION->line_height);
    lv_obj_add_event_cb(s.metrics,mesh_caption_deleted,LV_EVENT_DELETE,NULL);

    s.buf =
        alloc_psram_first(
            (size_t)lv_draw_buf_width_to_stride(CW,LV_COLOR_FORMAT_RGB565) * CH,
            1);

    if (s.buf) {
        s.canvas = lv_canvas_create(s.popup);
        lv_canvas_set_buffer(
            s.canvas,
            s.buf,
            CW,
            CH,
            LV_COLOR_FORMAT_RGB565);
        lv_obj_set_pos(
            s.canvas,
            ui_theme_is_studio()?0:20,
            ui_theme_is_studio()?40:94);
        lv_obj_add_flag(
            s.canvas,
            LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(
            s.canvas,
            ui_bed_mesh_gestures_event_cb,
            LV_EVENT_ALL,
            NULL);
    }

    s.surface_grid_button =
        ui_popup_add_action_aligned(
            s.popup,
            UI_POPUP_ACTION_SECONDARY,
            "Grid",
            160,
            48,
            LV_ALIGN_BOTTOM_RIGHT,
            -20,
            -12,
            surface_grid_cb,
            NULL,
            &s.surface_grid_label);
    update_surface_grid_button();

    if(ui_theme_is_studio())ui_popup_add_footer_divider(s.popup,368);
    else ui_popup_add_standard_footer_divider(s.popup);

    ui_popup_add_action_aligned(
        s.popup,
        UI_POPUP_ACTION_SECONDARY,
        "Reset view",
        160,
        48,
        LV_ALIGN_BOTTOM_RIGHT,
        -188,
        -12,
        reset_cb,
        NULL,
        NULL);
    ui_popup_add_action_aligned(
        s.popup,
        UI_POPUP_ACTION_SECONDARY,
        "+",
        52,
        48,
        LV_ALIGN_BOTTOM_RIGHT,
        -356,
        -12,
        plus_cb,
        NULL,
        NULL);
    ui_popup_add_action_aligned(
        s.popup,
        UI_POPUP_ACTION_SECONDARY,
        "-",
        52,
        48,
        LV_ALIGN_BOTTOM_RIGHT,
        -416,
        -12,
        minus_cb,
        NULL,
        NULL);
    s.calibrate_button=ui_popup_add_action_aligned(
        s.popup,
        UI_POPUP_ACTION_PRIMARY,
        "Calibrate",
        150,
        48,
        LV_ALIGN_BOTTOM_RIGHT,
        -476,
        -12,
        calibrate_cb,
        NULL,
        NULL);
    ui_popup_add_action_aligned(
        s.popup,
        UI_POPUP_ACTION_SECONDARY,
        "Profiles",
        140,
        48,
        LV_ALIGN_BOTTOM_RIGHT,
        -634,
        -12,
        ui_bed_mesh_profiles_show_cb,
        NULL,
        NULL);

    ui_bed_mesh_refresh();
    s.connection_timer=lv_timer_create(mesh_connection_refresh,250,NULL);mesh_connection_refresh(NULL);
}
