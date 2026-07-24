#include "ui_status_banner_v32.h"

#include "ui_theme.h"
#include "ui_widgets.h"

#include <stdlib.h>
#include <string.h>

/*
 * TEST19_DARK_STATE_BANNER
 *
 * Dashboard banner design language:
 *
 *   - banner interior remains dark in every machine state
 *   - machine state is communicated by the accent strip, border,
 *     state text, percentage, and progress indicator
 *   - the primary message or active filename is large and bright
 *   - ETA remains secondary information
 */
typedef struct {
    lv_obj_t *accent;
    lv_obj_t *state;
    lv_obj_t *file;
    lv_obj_t *eta;
    lv_obj_t *progress;
    lv_obj_t *bar;
} status_banner_ctx_t;

static void status_banner_delete_cb(lv_event_t *event)
{
    status_banner_ctx_t *ctx =
        (status_banner_ctx_t *)lv_event_get_user_data(event);

    if (ctx) lv_free(ctx);
}

static void set_optional_label(lv_obj_t *label, const char *text)
{
    if (!label) return;

    if (text && text[0]) {
        lv_label_set_text(label, text);
        lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text(label, "");
        lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
    }
}

static ui_status_kind_t banner_state_kind(
    const char *state_text)
{
    if (!state_text) {
        return UI_STATUS_NEUTRAL;
    }

    /*
     * ERROR / OFFLINE = red
     */
    if (strstr(state_text, "ERROR") ||
        strstr(state_text, "FAULT") ||
        strstr(state_text, "CANCEL") ||
        strstr(state_text, "OFFLINE") ||
        strstr(state_text, "DISCONNECTED") ||
        strstr(state_text, "NO CONNECTION")) {
        return UI_STATUS_DANGER;
    }

    /*
     * PAUSED / HEATING = amber
     */
    if (strstr(state_text, "PAUSED") ||
        strstr(state_text, "PAUSE") ||
        strstr(state_text, "HEATING")) {
        return UI_STATUS_WARNING;
    }

    /*
     * ACTIVE / PRINTING / DRYING = green
     */
    if (strstr(state_text, "PRINTING") ||
        strstr(state_text, "PRINT") ||
        strstr(state_text, "COMPLETE") ||
        strstr(state_text, "ACTIVE") ||
        strstr(state_text, "DRYING")) {
        return UI_STATUS_OK;
    }

    /*
     * READY / IDLE / CONNECTED = blue
     */
    if (strstr(state_text, "READY") ||
        strstr(state_text, "CONNECTED") ||
        strstr(state_text, "LINKED") ||
        strstr(state_text, "WIFI") ||
        strstr(state_text, "MONITORING") ||
        strstr(state_text, "STANDBY") ||
        strstr(state_text, "IDLE")) {
        return UI_STATUS_INFO;
    }

    return UI_STATUS_NEUTRAL;
}

lv_obj_t *ui_status_banner_v32_create(
    lv_obj_t *parent,
    int x,
    int y,
    int w,
    int h)
{
    if (!parent) {
        return NULL;
    }

    /*
     * Dashboard uses the shared Operator banner shell.
     * READY defaults to blue.
     */
    lv_obj_t *banner =
        ui_create_operator_banner(
            parent,
            x,
            y,
            w,
            h,
            UI_STATUS_INFO);

    if (!banner) {
        return NULL;
    }

    status_banner_ctx_t *ctx =
        lv_malloc(sizeof(status_banner_ctx_t));

    if (!ctx) {
        return banner;
    }

    memset(
        ctx,
        0,
        sizeof(*ctx));

    /*
     * Narrow state strip. This carries the state palette without
     * overpowering the message area.
     */
    ctx->accent = lv_obj_create(banner);

    lv_obj_clear_flag(
        ctx->accent,
        LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_size(
        ctx->accent,
        7,
        h - 4);

    lv_obj_set_pos(
        ctx->accent,
        2,
        2);

    lv_obj_set_style_bg_color(
        ctx->accent,
        UI_OK_BRIGHT,
        0);

    lv_obj_set_style_bg_opa(
        ctx->accent,
        LV_OPA_COVER,
        0);

    lv_obj_set_style_border_width(
        ctx->accent,
        UI_BORDER_NONE,
        0);

    lv_obj_set_style_radius(
        ctx->accent,
        UI_RADIUS_BAR,
        0);

    lv_obj_set_style_pad_all(
        ctx->accent,
        0,
        0);

    /*
     * State label: compact and strongly colored.
     */
    ctx->state = lv_label_create(banner);

    lv_label_set_text(
        ctx->state,
        LV_SYMBOL_OK " READY");

    lv_obj_set_width(ctx->state, 170);

    ui_apply_text_title(
        ctx->state);

    ui_apply_label_success(
        ctx->state);

    lv_obj_set_pos(
        ctx->state,
        22,
        11);

    /*
     * Primary operator message / active filename.
     *
     * This is now the visual focus of the banner.
     */
    ctx->file = lv_label_create(banner);

    lv_label_set_text(
        ctx->file,
        "No active print");

    lv_obj_set_width(ctx->file, w - 480);

    lv_label_set_long_mode(
        ctx->file,
        LV_LABEL_LONG_DOT);

    ui_apply_text_value_small(
        ctx->file);

    ui_apply_label_bright(
        ctx->file);

    lv_obj_set_pos(
        ctx->file,
        196,
        10);

    /*
     * ETA and percentage remain right-aligned supporting data.
     */
    ctx->eta = lv_label_create(banner);

    lv_label_set_text(
        ctx->eta,
        "ETA --:--");

    lv_obj_set_width(
        ctx->eta,
        145);

    ui_apply_text_body(
        ctx->eta);

    ui_apply_label_dim(
        ctx->eta);

    lv_obj_set_style_text_align(
        ctx->eta,
        LV_TEXT_ALIGN_RIGHT,
        0);

    lv_obj_set_pos(
        ctx->eta,
        w - 265,
        15);

    ctx->progress = lv_label_create(banner);

    lv_label_set_text(
        ctx->progress,
        "--%");

    lv_obj_set_width(
        ctx->progress,
        90);

    ui_apply_text_title(
        ctx->progress);

    ui_apply_label_success(
        ctx->progress);

    lv_obj_set_style_text_align(
        ctx->progress,
        LV_TEXT_ALIGN_RIGHT,
        0);

    lv_obj_set_pos(
        ctx->progress,
        w - 105,
        10);

    /*
     * Thin state-colored progress strip along the bottom.
     */
    ctx->bar = lv_bar_create(banner);

    lv_obj_set_size(
        ctx->bar,
        w - 36,
        5);

    lv_obj_set_pos(
        ctx->bar,
        18,
        h - 7);

    lv_bar_set_range(
        ctx->bar,
        0,
        100);

    lv_bar_set_value(
        ctx->bar,
        0,
        LV_ANIM_OFF);

    lv_obj_set_style_bg_color(
        ctx->bar,
        UI_PANEL,
        LV_PART_MAIN);

    lv_obj_set_style_bg_opa(
        ctx->bar,
        LV_OPA_COVER,
        LV_PART_MAIN);

    lv_obj_set_style_bg_color(
        ctx->bar,
        UI_OK_BRIGHT,
        LV_PART_INDICATOR);

    lv_obj_set_style_radius(
        ctx->bar,
        UI_RADIUS_BAR,
        LV_PART_MAIN);

    lv_obj_set_style_radius(
        ctx->bar,
        UI_RADIUS_BAR,
        LV_PART_INDICATOR);

    lv_obj_set_user_data(
        banner,
        ctx);

    lv_obj_add_event_cb(
        banner,
        status_banner_delete_cb,
        LV_EVENT_DELETE,
        ctx);

    return banner;
}

void ui_status_banner_v32_set(
    lv_obj_t *banner,
    const char *state,
    const char *file,
    const char *eta,
    const char *progress)
{
    if (!banner) {
        return;
    }

    status_banner_ctx_t *ctx =
        (status_banner_ctx_t *)
            lv_obj_get_user_data(banner);

    if (!ctx) {
        return;
    }

    const char *state_text =
        state ? state : "--";

    lv_label_set_text(
        ctx->state,
        state_text);

    set_optional_label(ctx->file, file);
    set_optional_label(ctx->eta, eta);
    set_optional_label(ctx->progress, progress);

    ui_status_kind_t state_kind =
        banner_state_kind(state_text);

    lv_color_t state_color =
        ui_status_color(state_kind);

    ui_operator_banner_set_status(
        banner,
        state_kind);

    if (ctx->accent) {
        lv_obj_set_style_bg_color(
            ctx->accent,
            state_color,
            0);
    }

    lv_obj_set_style_text_color(
        ctx->state,
        state_color,
        0);

    lv_obj_set_style_text_color(
        ctx->file,
        UI_TEXT_BRIGHT,
        0);

    lv_obj_set_style_text_color(
        ctx->eta,
        UI_TEXT_DIM,
        0);

    lv_obj_set_style_text_color(
        ctx->progress,
        state_color,
        0);

    if (ctx->bar) {
        lv_obj_set_style_bg_color(
            ctx->bar,
            state_color,
            LV_PART_INDICATOR);
    }

    int pct = 0;

    if (progress &&
        strstr(progress, "%") &&
        strstr(progress, "--") == NULL) {
        pct = atoi(progress);

        if (pct < 0) {
            pct = 0;
        }

        if (pct > 100) {
            pct = 100;
        }
    }

    if (ctx->bar) {
        lv_bar_set_value(
            ctx->bar,
            pct,
            ui_theme_motion_enabled()
                ? LV_ANIM_ON
                : LV_ANIM_OFF);
    }
}

void ui_status_banner_v32_set_simple(
    lv_obj_t *banner,
    const char *state,
    const char *message)
{
    ui_status_banner_v32_set(
        banner,
        state,
        message,
        NULL,
        NULL);

    if (!banner) return;

    status_banner_ctx_t *ctx =
        (status_banner_ctx_t *)lv_obj_get_user_data(banner);

    if (!ctx || !ctx->file) return;

    /* Use the space normally reserved for ETA and progress. */
    int32_t message_width = lv_obj_get_width(banner) - 218;
    if (message_width < 1) message_width = 1;

    lv_obj_set_width(ctx->file, message_width);
    lv_obj_set_style_text_align(ctx->file, LV_TEXT_ALIGN_LEFT, 0);
}

lv_obj_t *ui_status_banner_v32_state_label(lv_obj_t *banner)
{
    if (!banner) return NULL;

    status_banner_ctx_t *ctx =
        (status_banner_ctx_t *)lv_obj_get_user_data(banner);

    return ctx ? ctx->state : NULL;
}
