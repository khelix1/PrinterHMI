#include <stdio.h>

#include "ui_dashboard_status.h"
#include "printer_controller.h"

#include "ui_theme.h"
#include "ui_widgets.h"

static ui_dashboard_status_v32_t s_dash_status = {0};

ui_dashboard_status_v32_t ui_dashboard_status_v32_create(lv_obj_t *parent)
{
    ui_dashboard_status_v32_t out = {0};

    lv_obj_t *right_panel = lv_obj_create(parent);
    out.root = right_panel;
    lv_obj_clear_flag(right_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(right_panel, 370, 320);
    lv_obj_set_pos(right_panel, 575, 170);
    ui_apply_panel_style(right_panel);
    lv_obj_set_style_bg_color(right_panel, UI_PANEL_ALT, 0);
    lv_obj_set_style_border_color(right_panel, UI_BORDER_SOFT, 0);
    lv_obj_set_style_border_width(
        right_panel,
        UI_BORDER_THIN,
        0);
    lv_obj_set_style_pad_all(
        right_panel,
        UI_PAD_PANEL,
        0);

    lv_obj_t *rt = lv_label_create(right_panel);
    lv_label_set_text(rt, "PRINT STATUS");
    ui_apply_text_body(rt);
    ui_apply_label_dim(rt);
    lv_obj_set_pos(rt, 0, 0);

    out.state = lv_label_create(right_panel);
    lv_label_set_text(out.state, "--");
    ui_apply_text_heading(out.state);
    ui_apply_label_primary(out.state);
    lv_obj_set_pos(out.state, 0, 30);

    out.progress = lv_label_create(right_panel);
    lv_label_set_text(out.progress, "-- %");
    ui_apply_text_heading(out.progress);
    ui_apply_label_primary(out.progress);
    lv_obj_set_pos(out.progress, 300, 30);

    out.progress_bar = lv_bar_create(right_panel);
    lv_obj_set_size(out.progress_bar, 390, 22);
    lv_obj_set_pos(out.progress_bar, 0, 84);
    lv_bar_set_range(out.progress_bar, 0, 100);
    lv_bar_set_value(out.progress_bar, 0, LV_ANIM_OFF);

    lv_obj_t *el = lv_label_create(right_panel);
    lv_label_set_text(el, "ELAPSED");
    ui_apply_text_caption(el);
    ui_apply_label_dim(el);
    lv_obj_set_pos(el, 0, 125);

    out.elapsed = lv_label_create(right_panel);
    lv_label_set_text(out.elapsed, "--:--");
    ui_apply_text_title(out.elapsed);
    ui_apply_label_primary(out.elapsed);
    lv_obj_set_pos(out.elapsed, 0, 150);

    lv_obj_t *rl = lv_label_create(right_panel);
    lv_label_set_text(rl, "REMAINING");
    ui_apply_text_caption(rl);
    ui_apply_label_dim(rl);
    lv_obj_set_pos(rl, 205, 125);

    out.remaining = lv_label_create(right_panel);
    lv_label_set_text(out.remaining, "--:--");
    ui_apply_text_title(out.remaining);
    ui_apply_label_primary(out.remaining);
    lv_obj_set_pos(out.remaining, 205, 150);

    lv_obj_t *eta_l = lv_label_create(right_panel);
    lv_label_set_text(eta_l, "EST FINISH");
    ui_apply_text_caption(eta_l);
    ui_apply_label_dim(eta_l);
    lv_obj_set_pos(eta_l, 205, 186);

    out.eta = lv_label_create(right_panel);
    lv_label_set_text(out.eta, "--:--");
    ui_apply_text_title(out.eta);
    ui_apply_label_primary(out.eta);
    lv_obj_set_pos(out.eta, 205, 204);

    lv_obj_t *nt = lv_label_create(right_panel);
    lv_label_set_text(nt, "NOZZLE");
    ui_apply_text_caption(nt);
    ui_apply_label_dim(nt);
    lv_obj_set_pos(nt, 0, 186);

    out.nozzle = lv_label_create(right_panel);
    lv_label_set_text(out.nozzle, "-- / -- C");
    ui_apply_text_value_small(out.nozzle);
    ui_apply_label_primary(out.nozzle);
    lv_obj_set_pos(out.nozzle, 0, 204);

    lv_obj_t *bt = lv_label_create(right_panel);
    lv_label_set_text(bt, "BED");
    ui_apply_text_caption(bt);
    ui_apply_label_dim(bt);
    lv_obj_set_pos(bt, 0, 244);

    out.bed = lv_label_create(right_panel);
    lv_label_set_text(out.bed, "-- / -- C");
    ui_apply_text_value_small(out.bed);
    ui_apply_label_primary(out.bed);
    lv_obj_set_pos(out.bed, 0, 262);

    s_dash_status = out;
    return out;
}

void ui_dashboard_status_v32_set_print_state(const char *state)
{
    if (s_dash_status.state) {
        lv_label_set_text(s_dash_status.state, state ? state : "--");
    }
}

lv_color_t ui_dashboard_status_v32_progress_color(double progress)
{
    if (progress < 0.0) return UI_TEXT;
    if (progress < 0.25) return UI_ACCENT_INFO;
    if (progress < 0.75) return UI_ACCENT_PURPLE;
    return UI_OK_BRIGHT;
}

void ui_dashboard_status_v32_refresh(double progress,
                                     double print_duration_seconds)
{
    char progress_buf[32];
    char elapsed_buf[32];
    char remaining_buf[32];
    char eta_buf[32];

    if (progress >= 0.0) {
        snprintf(progress_buf,
                 sizeof(progress_buf),
                 "%.0f %%",
                 progress * 100.0);
    } else {
        snprintf(progress_buf, sizeof(progress_buf), "-- %%");
    }

    int progress_pct = 0;
    if (progress >= 0.0) {
        progress_pct = (int)(progress * 100.0);
    }

    printer_controller_format_hhmm(elapsed_buf,
                                   sizeof(elapsed_buf),
                                   print_duration_seconds);

    printer_controller_format_remaining(remaining_buf,
                                        sizeof(remaining_buf),
                                        progress,
                                        print_duration_seconds);

    printer_controller_format_eta_clock(eta_buf,
                                        sizeof(eta_buf),
                                        progress,
                                        print_duration_seconds);

    ui_dashboard_status_v32_set_progress(
        progress_buf,
        progress_pct,
        ui_dashboard_status_v32_progress_color(progress));

    ui_dashboard_status_v32_set_times(elapsed_buf,
                                      remaining_buf,
                                      eta_buf);
}

void ui_dashboard_status_v32_set_progress(const char *progress_text,
                                          int progress_pct,
                                          lv_color_t progress_color)
{
    if (s_dash_status.progress) {
        lv_label_set_text(s_dash_status.progress, progress_text ? progress_text : "-- %");
        lv_obj_set_style_text_color(s_dash_status.progress, progress_color, 0);
    }

    if (s_dash_status.progress_bar) {
        if (progress_pct < 0) progress_pct = 0;
        if (progress_pct > 100) progress_pct = 100;
        lv_bar_set_value(s_dash_status.progress_bar, progress_pct, LV_ANIM_OFF);
    }
}

void ui_dashboard_status_v32_set_times(const char *elapsed,
                                       const char *remaining,
                                       const char *eta)
{
    if (s_dash_status.elapsed) {
        lv_label_set_text(s_dash_status.elapsed, elapsed ? elapsed : "--:--");
    }

    if (s_dash_status.remaining) {
        lv_label_set_text(s_dash_status.remaining, remaining ? remaining : "--:--");
    }

    if (s_dash_status.eta) {
        lv_label_set_text(s_dash_status.eta, eta ? eta : "--:--");
    }
}
