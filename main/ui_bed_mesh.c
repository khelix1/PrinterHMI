#include "ui_bed_mesh.h"
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
#include "ui_theme.h"
#include "ui_widgets.h"
#include "ui_page_geometry.h"
#define CW 620
#define CH 340
#define VALUE_CAP ((size_t)BED_MESH_MAX_ROWS*BED_MESH_MAX_COLS)
typedef struct {
    lv_obj_t *popup;
    lv_obj_t *canvas;
    lv_obj_t *stats;
    lv_obj_t *surface_grid_button;
    lv_obj_t *surface_grid_label;
    lv_obj_t *calibrate_confirm;
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

static void stats(void){if(!s.stats)return;if(!s.mesh.valid){lv_label_set_text(s.stats,ui_text("No active bed mesh. Calibrate or load a profile."));return;}char b[256];snprintf(
        b,
        sizeof(b),
        "PROFILE %s   GRID %u x %u   Z %.0fx%s\n"
        "MIN %+.3f mm   MAX %+.3f mm   RANGE %.3f mm",
        s.mesh.profile_name,
        s.mesh.cols,
        s.mesh.rows,
        s.zscale,
        s.mesh.truncated ? "   TRUNCATED" : "",
        s.mesh.minimum,
        s.mesh.maximum,
        s.mesh.range);lv_label_set_text(s.stats,b);}
void ui_bed_mesh_refresh(void)
{
    if (!s.popup) {
        return;
    }

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

    ui_bed_mesh_profiles_update(&s.mesh);
    stats();
    render();
}

static void update_surface_grid_button(void)
{
    if (s.surface_grid_label) {
        lv_label_set_text(
            s.surface_grid_label,
            s.surface_grid_visible
                ? ui_text("SURFACE GRID ON")
                : ui_text("SURFACE GRID OFF"));
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

    if (s.calibrate_confirm) {
        lv_obj_move_foreground(s.calibrate_confirm);
        return;
    }

    /*
     * Calibration belongs to the bed-mesh viewer, while all modal geometry
     * and styling comes from the shared popup subsystem.
     */
    s.calibrate_confirm =
        ui_popup_create(
            s.popup ? s.popup : lv_screen_active(),
            500,
            280,
            UI_POPUP_DANGER);

    if (!s.calibrate_confirm) {
        return;
    }

    ui_popup_add_title(
        s.calibrate_confirm,
        ui_text("START BED MESH?"),
        true,
        4);
    ui_popup_add_header_divider(
        s.calibrate_confirm,
        44);
    ui_popup_add_body(
        s.calibrate_confirm,
        "This will move the toolhead across the bed and probe multiple "
        "points.\n\nMake sure the bed is clear and the printer is ready.",
        24,
        70,
        452);
    ui_popup_add_standard_footer_divider(
        s.calibrate_confirm);

    ui_popup_add_footer_action(
        s.calibrate_confirm,
        UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_LEFT " BACK",
        160,
        UI_POPUP_FOOTER_LEFT,
        close_calibrate_confirm_cb,
        NULL,
        NULL);

    ui_popup_add_footer_action(
        s.calibrate_confirm,
        UI_POPUP_ACTION_DANGER,
        LV_SYMBOL_PLAY " START",
        190,
        UI_POPUP_FOOTER_RIGHT,
        confirm_calibrate_cb,
        NULL,
        NULL);
}

bool ui_bed_mesh_is_open(void)
{
    return s.popup != NULL;
}

void ui_bed_mesh_close(void)
{
    /*
     * The shared confirmation is a top-layer sibling, not a child of the
     * mesh viewer. Close it first so no modal surface can outlive its owner.
     */
    close_calibrate_confirm_cb(NULL);
    ui_bed_mesh_profiles_close();

    ui_bed_mesh_gestures_close();

    if (s.popup) {
        lv_obj_delete(s.popup);
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

void ui_bed_mesh_show(ui_bed_mesh_command_cb_t command)
{
    ui_bed_mesh_close();
    s.command = command;
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

    ui_popup_add_title(
        s.popup,
        ui_text("BED MESH 3D"),
        false,
        8);
    ui_popup_add_header_divider(
        s.popup,
        52);

    s.stats =
        ui_popup_add_caption(
            s.popup,
            ui_text(""),
            20,
            58,
            814);

    s.buf =
        alloc_psram_first(
            (size_t)CW * CH,
            sizeof(uint16_t));

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
            20,
            98);
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
            "SURFACE GRID OFF",
            174,
            46,
            LV_ALIGN_TOP_RIGHT,
            -20,
            98,
            surface_grid_cb,
            NULL,
            &s.surface_grid_label);
    update_surface_grid_button();

    ui_popup_add_standard_footer_divider(
        s.popup);
    ui_popup_add_caption(
        s.popup,
        ui_text("DRAG ROTATE  •  2-FINGER PAN  •  PINCH ZOOM"),
        20,
        493,
        272);

    ui_popup_add_action_aligned(
        s.popup,
        UI_POPUP_ACTION_SECONDARY,
        "RESET VIEW",
        132,
        48,
        LV_ALIGN_BOTTOM_RIGHT,
        -20,
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
        -162,
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
        -224,
        -12,
        minus_cb,
        NULL,
        NULL);
    ui_popup_add_action_aligned(
        s.popup,
        UI_POPUP_ACTION_PRIMARY,
        "CALIBRATE",
        130,
        48,
        LV_ALIGN_BOTTOM_RIGHT,
        -286,
        -12,
        calibrate_cb,
        NULL,
        NULL);
    ui_popup_add_action_aligned(
        s.popup,
        UI_POPUP_ACTION_SECONDARY,
        "PROFILES",
        140,
        48,
        LV_ALIGN_BOTTOM_RIGHT,
        -426,
        -12,
        ui_bed_mesh_profiles_show_cb,
        NULL,
        NULL);

    ui_bed_mesh_refresh();
}
