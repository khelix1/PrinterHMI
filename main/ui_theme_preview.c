#include "ui_theme_preview.h"

#include <stddef.h>

typedef struct {
    uint32_t background;
    uint32_t surface;
    uint32_t card;
    uint32_t control;
    uint32_t border;
    uint32_t accent;
    uint32_t text;
    uint32_t muted;
    uint32_t success;
    uint32_t danger;
    int32_t radius;
    lv_opa_t surface_opa;
    const char *name;
    const char *code;
    const char *description;
} ui_theme_preview_palette_t;

static ui_theme_preview_palette_t preview_palette(ui_theme_id_t theme)
{
    switch (theme) {
        case UI_THEME_CLASSIC:
            return (ui_theme_preview_palette_t){
                .background = 0x18130F,
                .surface = 0x2B211B,
                .card = 0x35271E,
                .control = 0x49362B,
                .border = 0xB87545,
                .accent = 0xE07A3F,
                .text = 0xF4E8DA,
                .muted = 0xC5A88E,
                .success = 0x6DD39E,
                .danger = 0xF06A66,
                .radius = 18,
                .surface_opa = LV_OPA_COVER,
                .name = "FOUNDRY",
                .code = "THEME A",
                .description = "Warm workshop",
            };

        case UI_THEME_GLASS:
            return (ui_theme_preview_palette_t){
                .background = 0x03040A,
                .surface = 0x0A1020,
                .card = 0x111D36,
                .control = 0x17142E,
                .border = 0x3A67A0,
                .accent = 0x4C9FBA,
                .text = 0xEAF3FF,
                .muted = 0x91A6D8,
                .success = 0x35FFC6,
                .danger = 0xFF4FA3,
                .radius = 22,
                .surface_opa = (lv_opa_t)230,
                .name = "DARK GLASS",
                .code = "THEME C",
                .description = "Smoked layers",
            };

        case UI_THEME_OPERATOR_SHELL:
            return (ui_theme_preview_palette_t){
                .background = 0x09121E,
                .surface = 0x101B2A,
                .card = 0x162235,
                .control = 0x1A2633,
                .border = 0x3D6F99,
                .accent = 0x33C7FF,
                .text = 0xE8F1FF,
                .muted = 0x8FA7C2,
                .success = 0x70E000,
                .danger = 0xFF4D4D,
                .radius = 10,
                .surface_opa = LV_OPA_COVER,
                .name = "OPERATOR SHELL",
                .code = "LAYOUT THEME",
                .description = "Eight-item rail",
            };

        case UI_THEME_OPERATOR:
        default:
            return (ui_theme_preview_palette_t){
                .background = 0x0B1118,
                .surface = 0x101B2A,
                .card = 0x162235,
                .control = 0x1A2633,
                .border = 0x3D6F99,
                .accent = 0x19C7E8,
                .text = 0xE8F1FF,
                .muted = 0x8FA7C2,
                .success = 0x70E000,
                .danger = 0xFF4D4D,
                .radius = 10,
                .surface_opa = LV_OPA_COVER,
                .name = "OPERATOR",
                .code = "THEME B",
                .description = "Print-cell flat",
            };
    }
}

static lv_obj_t *preview_rect(lv_obj_t *parent,
                              int32_t x,
                              int32_t y,
                              int32_t width,
                              int32_t height,
                              uint32_t background,
                              uint32_t border,
                              int32_t radius,
                              lv_opa_t opacity)
{
    lv_obj_t *object = lv_obj_create(parent);

    if (!object) return NULL;

    lv_obj_set_size(object, width, height);
    lv_obj_set_pos(object, x, y);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(object, 0, 0);
    lv_obj_set_style_bg_color(object, lv_color_hex(background), 0);
    lv_obj_set_style_bg_opa(object, opacity, 0);
    lv_obj_set_style_border_color(object, lv_color_hex(border), 0);
    lv_obj_set_style_border_width(object, border ? 1 : 0, 0);
    lv_obj_set_style_radius(object, radius, 0);

    return object;
}

static lv_obj_t *preview_label(lv_obj_t *parent,
                               const char *text,
                               const lv_font_t *font,
                               uint32_t color,
                               int32_t x,
                               int32_t y)
{
    lv_obj_t *label = lv_label_create(parent);

    if (!label) return NULL;

    lv_label_set_text(label, text ? text : "");
    lv_obj_set_pos(label, x, y);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);

    return label;
}

static void preview_add_sample_ui(
    lv_obj_t *button,
    const ui_theme_preview_palette_t *palette,
    int32_t preview_width)
{
    int32_t inner_width = preview_width - 28;
    int32_t action_width = (inner_width - 12) / 2;
    lv_obj_t *topbar = preview_rect(
        button, 14, 76, inner_width, 34,
        palette->surface,
        palette->border,
        palette->radius / 2,
        palette->surface_opa);

    if (topbar) {
        lv_obj_t *dot = preview_rect(
            topbar, 10, 10, 12, 12,
            palette->success,
            0,
            LV_RADIUS_CIRCLE,
            LV_OPA_COVER);
        (void)dot;
        preview_label(topbar, "READY", UI_FONT_CAPTION,
                      palette->text, 30, 7);
        if (inner_width >= 220) {
            preview_label(topbar, "12:42 PM", UI_FONT_CAPTION,
                          palette->muted, inner_width - 98, 7);
        }
    }

    lv_obj_t *sample_card = preview_rect(
        button, 14, 118, inner_width, 64,
        palette->card,
        palette->border,
        palette->radius,
        palette->surface_opa);

    if (sample_card) {
        preview_label(sample_card, "NOZZLE", UI_FONT_CAPTION,
                      palette->muted, 12, 8);
        preview_label(sample_card, "215 / 220 C", UI_FONT_BODY,
                      palette->text, 12, 29);

        lv_obj_t *track = preview_rect(
            sample_card, inner_width - 100, 39, 88, 8,
            palette->background,
            0,
            4,
            LV_OPA_COVER);

        if (track) {
            preview_rect(track, 0, 0, 67, 8,
                         palette->accent, 0, 4, LV_OPA_COVER);
        }
    }

    lv_obj_t *action = preview_rect(
        button, 14, 192, action_width, 36,
        palette->control,
        palette->accent,
        palette->radius / 2,
        palette->surface_opa);

    if (action) {
        lv_obj_t *text = preview_label(
            action, "ACTION", UI_FONT_CAPTION,
            palette->text, 0, 0);
        if (text) lv_obj_center(text);
    }

    lv_obj_t *stop = preview_rect(
        button, 14 + action_width + 12, 192, action_width, 36,
        palette->control,
        palette->danger,
        palette->radius / 2,
        palette->surface_opa);

    if (stop) {
        lv_obj_t *text = preview_label(
            stop, "STOP", UI_FONT_CAPTION,
            palette->danger, 0, 0);
        if (text) lv_obj_center(text);
    }
}

lv_obj_t *ui_theme_preview_create(
    lv_obj_t *parent,
    ui_theme_id_t theme,
    bool selected,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    lv_event_cb_t event_cb,
    void *user_data)
{
    if (!parent) return NULL;

    ui_theme_preview_palette_t palette = preview_palette(theme);
    lv_obj_t *button = lv_button_create(parent);

    if (!button) return NULL;

    lv_obj_set_size(button, width, height);
    lv_obj_set_pos(button, x, y);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(button, 0, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(palette.background), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(
        button,
        lv_color_hex(selected ? palette.accent : palette.border),
        0);
    lv_obj_set_style_border_width(button, selected ? 4 : 2, 0);
    lv_obj_set_style_radius(button, palette.radius, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_outline_width(button, 0, 0);

    preview_label(button, palette.name,
                  width <= 220 ? UI_FONT_BODY_LARGE : UI_FONT_TITLE,
                  palette.text, 14, 10);
    preview_label(button, palette.code, UI_FONT_CAPTION,
                  palette.accent, 14, 39);
    preview_label(button, palette.description, UI_FONT_CAPTION,
                  palette.muted, 96, 39);

    if (selected) {
        lv_obj_t *check = preview_label(
            button, LV_SYMBOL_OK, UI_FONT_BODY_LARGE,
            palette.success, width - 38, 13);
        (void)check;
    }

    preview_add_sample_ui(button, &palette, width);

    if (event_cb) {
        lv_obj_add_event_cb(
            button,
            event_cb,
            LV_EVENT_CLICKED,
            user_data);
    }

    return button;
}
