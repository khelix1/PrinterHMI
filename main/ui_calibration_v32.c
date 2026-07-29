#include "ui_calibration_v32.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lvgl.h"
#include <stdbool.h>

#include "ui_button.h"
#include "ui_page_geometry_v32.h"
#include "ui_theme.h"
#include "ui_widgets.h"

typedef struct {
    lv_obj_t *root;
    ui_calibration_open_bed_mesh_cb_t open_bed_mesh;
} ui_calibration_state_t;

static const char TAG[] = "ui_calibration";
static ui_calibration_state_t *s_calibration;


static bool calibration_state_init(void)
{
    if (s_calibration) {
        return true;
    }

    s_calibration = heap_caps_calloc(
        1,
        sizeof(*s_calibration),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (s_calibration) {
        ESP_LOGI(
            TAG,
            "Calibration page state allocated permanently in PSRAM: %u bytes",
            (unsigned)sizeof(*s_calibration));
        return true;
    }

    s_calibration = heap_caps_calloc(
        1,
        sizeof(*s_calibration),
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    if (!s_calibration) {
        ESP_LOGE(TAG, "Unable to allocate Calibration page state");
        return false;
    }

    ESP_LOGW(TAG, "Calibration page state using internal RAM fallback");
    return true;
}


static lv_obj_t *calibration_label(
    lv_obj_t *parent,
    const char *text,
    const lv_font_t *font,
    lv_color_t color,
    int x,
    int y,
    int width)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_label_set_text(label, text);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label, width);
    lv_obj_set_pos(label, x, y);
    ui_apply_custom_label_style(label, font, color);

    return label;
}


static lv_obj_t *calibration_card(
    lv_obj_t *parent,
    const char *title,
    const char *summary,
    const char *status,
    int x,
    int y)
{
    lv_obj_t *card = ui_create_operator_card(
        parent,
        x,
        y,
        390,
        176);

    if (!card) {
        return NULL;
    }

    ui_create_operator_card_heading(card, title, 16, 14);
    ui_create_operator_card_divider(card, 16, 45, 358);

    calibration_label(
        card,
        summary,
        UI_FONT_BODY,
        UI_TEXT_DIM,
        16,
        58,
        358);

    calibration_label(
        card,
        status,
        UI_FONT_CAPTION,
        UI_ACCENT_BRIGHT,
        16,
        142,
        180);

    return card;
}


static void calibration_open_bed_mesh_event_cb(lv_event_t *event)
{
    (void)event;

    if (s_calibration && s_calibration->open_bed_mesh) {
        s_calibration->open_bed_mesh();
    }
}


void ui_calibration_v32_show(
    ui_calibration_open_bed_mesh_cb_t open_bed_mesh_cb)
{
    if (!calibration_state_init()) {
        return;
    }

    s_calibration->open_bed_mesh = open_bed_mesh_cb;

    if (s_calibration->root) {
        lv_obj_move_foreground(s_calibration->root);
        return;
    }

    s_calibration->root = lv_obj_create(lv_screen_active());
    lv_obj_set_size(
        s_calibration->root,
        UI_PAGE_ROOT_WIDTH,
        UI_PAGE_ROOT_HEIGHT);
    lv_obj_set_pos(
        s_calibration->root,
        UI_PAGE_ROOT_X,
        UI_PAGE_ROOT_Y);
    lv_obj_clear_flag(s_calibration->root, LV_OBJ_FLAG_SCROLLABLE);
    ui_apply_surface_role(s_calibration->root, UI_SURFACE_PAGE_DEEP);

    lv_obj_t *banner = ui_create_operator_banner(
        s_calibration->root,
        20,
        20,
        800,
        86,
        UI_STATUS_INFO);

    calibration_label(
        banner,
        "CALIBRATION",
        UI_FONT_TITLE,
        UI_TEXT_BRIGHT,
        20,
        15,
        520);

    calibration_label(
        banner,
        "Guided workflows selected from the active printer's capabilities",
        UI_FONT_CAPTION,
        UI_TEXT_DIM,
        20,
        50,
        620);

    calibration_label(
        banner,
        "CAPABILITY DRIVEN",
        UI_FONT_CAPTION,
        UI_ACCENT_BRIGHT,
        630,
        32,
        150);

    lv_obj_t *bed = calibration_card(
        s_calibration->root,
        "BED GEOMETRY",
        "Bed Mesh is available now. Screws Tilt, Z Tilt, QGL and Axis Twist will appear only when detected.",
        "BED MESH AVAILABLE",
        20,
        126);

    if (bed) {
        lv_obj_t *button = ui_button_create(
            bed,
            UI_BUTTON_OUTLINED,
            "OPEN BED MESH");

        if (button) {
            lv_obj_set_size(button, 166, 38);
            lv_obj_align(button, LV_ALIGN_BOTTOM_RIGHT, -16, -12);
            lv_obj_add_event_cb(
                button,
                calibration_open_bed_mesh_event_cb,
                LV_EVENT_CLICKED,
                NULL);
        }
    }

    calibration_card(
        s_calibration->root,
        "MOTION",
        "Input Shaper, belt resonance and motion-limit workflows will share one guided session controller.",
        "AWAITING DISCOVERY",
        430,
        126);

    calibration_card(
        s_calibration->root,
        "TEMPERATURE & EXTRUSION",
        "PID tuning, Pressure Advance, rotation distance and firmware retraction will use explicit review and save steps.",
        "AWAITING DISCOVERY",
        20,
        322);

    calibration_card(
        s_calibration->root,
        "PROBE & Z",
        "Probe, Z-endstop and printer-specific leveling workflows will be shown only when their prerequisites are present.",
        "AWAITING DISCOVERY",
        430,
        322);
}


void ui_calibration_v32_hide(void)
{
    if (!s_calibration || !s_calibration->root) {
        return;
    }

    lv_obj_delete(s_calibration->root);
    s_calibration->root = NULL;
}
