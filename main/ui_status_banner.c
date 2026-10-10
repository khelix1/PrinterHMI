#include "ui_status_banner.h"
#include "ui_value_update.h"
#include "ui_text.h"

#include "ui_theme.h"
#include "ui_widgets.h"
#include "ui_font_fallback.h"

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
    lv_subject_t state_subject;
    lv_subject_t message_subject;
    char state_value[64];
    char state_previous[64];
    char message_value[256];
    char message_previous[256];
    bool state_bound;
    bool message_bound;
    bool studio_dashboard;
} status_banner_ctx_t;

static void status_banner_delete_cb(lv_event_t *event)
{
    status_banner_ctx_t *ctx =
        (status_banner_ctx_t *)lv_event_get_user_data(event);

    if (ctx) {
        /* Parent DELETE precedes child deletion. Deinit removes the label's
         * observer callbacks before freeing the subject's backing storage.
         */
        lv_subject_deinit(&ctx->state_subject);
        lv_subject_deinit(&ctx->message_subject);
        lv_free(ctx);
    }
}

static void set_observed_text(lv_obj_t *label, lv_subject_t *subject,
                              size_t capacity, bool bound, const char *text)
{
    if (!label) return;
    if (!text) text = "";
    /* Preserve the public setter's full-text behavior for unusually long
     * values or a failed observer allocation. No silent truncation.
     */
    if (!bound || strlen(text) >= capacity) {
        ui_value_set_text(label, text);
        return;
    }
    bool unchanged = strcmp(lv_subject_get_string(subject), text) == 0;
    lv_subject_copy_string(subject, text);
    /* Restore the widget after long-text fallback or a compatibility writer,
     * even when the bounded subject already contains this value.
     */
    if (unchanged) ui_value_set_text(label, text);
}

static void set_optional_label(lv_obj_t *label, const char *text)
{
    if (!label) return;

    if (text && text[0]) {
        ui_value_set_text(label, text);
        lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
    } else {
        ui_value_set_text(label, ui_text(""));
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

lv_obj_t *ui_status_banner_create(
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
        ui_text(LV_SYMBOL_OK " READY"));

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
        ui_text("No active print"));

    lv_obj_set_width(ctx->file, w - 480);

    lv_label_set_long_mode(
        ctx->file,
        LV_LABEL_LONG_SCROLL_CIRCULAR);

    /*
     * Slow the active filename marquee so long names remain readable.
     * LVGL uses this duration for one complete circular-scroll cycle.
     */
    lv_obj_set_style_anim_duration(
        ctx->file,
        14000,
        0);

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
        ui_text("ETA --:--"));

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

    ctx->studio_dashboard = ui_theme_is_studio() && w == 720 && h == 338;
    if (ctx->studio_dashboard) {
        lv_obj_clear_flag(banner,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_opa(banner,LV_OPA_TRANSP,0);
        lv_obj_set_style_border_width(banner,0,0);
        lv_obj_set_style_pad_all(banner,0,0);
        lv_obj_add_flag(ctx->accent,LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(ctx->state,0,0); lv_obj_set_width(ctx->state,316);
        lv_label_set_long_mode(ctx->state,LV_LABEL_LONG_MODE_DOTS);
        ui_apply_custom_label_style(ctx->state,UI_FONT_BODY_LARGE,UI_TEXT);
        lv_obj_set_pos(ctx->file,0,32); lv_obj_set_width(ctx->file,316);
        lv_obj_set_height(ctx->file,ui_font_with_fallback(UI_FONT_VALUE_SMALL)->line_height);
        lv_obj_set_height(ctx->state,ui_font_with_fallback(UI_FONT_BODY_LARGE)->line_height);
        lv_label_set_long_mode(ctx->file,LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_pos(ctx->progress,340,54); lv_obj_set_width(ctx->progress,360);
        ui_apply_text_percent(ctx->progress);
        lv_obj_set_style_text_align(ctx->progress,LV_TEXT_ALIGN_LEFT,0);
        lv_obj_set_pos(ctx->bar,340,176); lv_obj_set_size(ctx->bar,360,12);
        ui_apply_progress_bar_style(ctx->bar);
        lv_obj_set_pos(ctx->eta,340,194); lv_obj_set_width(ctx->eta,360);
        lv_obj_set_style_text_align(ctx->eta,LV_TEXT_ALIGN_LEFT,0);
    }

    lv_subject_init_string(&ctx->state_subject, ctx->state_value,
        ctx->state_previous, sizeof(ctx->state_value), lv_label_get_text(ctx->state));
    lv_subject_init_string(&ctx->message_subject, ctx->message_value,
        ctx->message_previous, sizeof(ctx->message_value), lv_label_get_text(ctx->file));
    ctx->state_bound = lv_label_bind_text(ctx->state, &ctx->state_subject, NULL) != NULL;
    ctx->message_bound = lv_label_bind_text(ctx->file, &ctx->message_subject, NULL) != NULL;

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

static void status_banner_set(
    lv_obj_t *banner,
    const char *state,
    const char *file,
    const char *eta,
    const char *progress,
    const ui_status_kind_t *shell_kind)
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

    set_observed_text(ctx->state, &ctx->state_subject,
        sizeof(ctx->state_value), ctx->state_bound, state_text);
    set_observed_text(ctx->file, &ctx->message_subject,
        sizeof(ctx->message_value), ctx->message_bound, file);
    if (file && file[0]) {
        lv_obj_remove_flag(ctx->file, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(ctx->file, LV_OBJ_FLAG_HIDDEN);
    }
    set_optional_label(ctx->eta, eta);
    set_optional_label(ctx->progress, progress);

    ui_status_kind_t state_kind =
        banner_state_kind(state_text);

    lv_color_t state_color =
        ui_status_color(state_kind);

    if (!ctx->studio_dashboard) ui_operator_banner_set_status(
        banner,
        shell_kind ? *shell_kind : state_kind);

    if (ctx->accent) {
        ui_value_set_bg_color(
            ctx->accent,
            state_color,
            0);
    }

    ui_value_set_color(
        ctx->state,
        ctx->studio_dashboard && state_kind != UI_STATUS_DANGER && state_kind != UI_STATUS_WARNING ? UI_TEXT : state_color,
        0);

    ui_value_set_color(
        ctx->file,
        UI_TEXT_BRIGHT,
        0);

    ui_value_set_color(
        ctx->eta,
        UI_TEXT_DIM,
        0);

    ui_value_set_color(
        ctx->progress,
        ctx->studio_dashboard ? UI_TEXT : state_color,
        0);

    if (ctx->bar) {
        ui_value_set_bg_color(
            ctx->bar,
            ctx->studio_dashboard ? UI_OK_BRIGHT : state_color,
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

void ui_status_banner_set(lv_obj_t *banner, const char *state,
    const char *file, const char *eta, const char *progress)
{
    status_banner_set(banner, state, file, eta, progress, NULL);
}

static void simple_message_layout(lv_obj_t *banner)
{
    if (!banner) return;
    status_banner_ctx_t *ctx = lv_obj_get_user_data(banner);
    if (!ctx || !ctx->file) return;
    int32_t width = lv_obj_get_width(banner) - 218;
    if (width < 1) width = 1;
    ui_value_set_style_num(ctx->file, LV_STYLE_WIDTH, width, 0);
    ui_value_set_style_num(ctx->file, LV_STYLE_TEXT_ALIGN, LV_TEXT_ALIGN_LEFT, 0);
}

void ui_status_banner_set_simple(lv_obj_t *banner,
    const char *state, const char *message)
{
    status_banner_set(banner, state, message, NULL, NULL, NULL);
    simple_message_layout(banner);
}

void ui_status_banner_set_simple_kind(lv_obj_t *banner,
    const char *state, const char *message, ui_status_kind_t shell_kind)
{
    status_banner_set(banner, state, message, NULL, NULL, &shell_kind);
    simple_message_layout(banner);
}

lv_obj_t *ui_status_banner_state_label(lv_obj_t *banner)
{
    if (!banner) return NULL;

    status_banner_ctx_t *ctx =
        (status_banner_ctx_t *)lv_obj_get_user_data(banner);

    return ctx ? ctx->state : NULL;
}
