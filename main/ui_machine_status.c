#include "ui_machine_status.h"
#include "ui_text.h"
#include "ui_theme.h"
#include "ui_widgets.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    lv_obj_t *live;
    lv_obj_t *filament;
    lv_obj_t *nozzle_name;
    lv_obj_t *nozzle;
    lv_obj_t *bed;
    lv_obj_t *air;
    lv_obj_t *humidity;
    lv_obj_t *speed;
    lv_obj_t *flow;
    lv_obj_t *fan;
} machine_status_ctx_t;

static lv_obj_t **machine_status_value_slot(
    machine_status_ctx_t *ctx,
    int index)
{
    switch (index) {
        case 0: return &ctx->nozzle;
        case 1: return &ctx->bed;
        case 2: return &ctx->air;
        case 3: return &ctx->humidity;
        case 4: return &ctx->speed;
        case 5: return &ctx->flow;
        case 6: return &ctx->fan;
        case 7: return &ctx->filament;
        default: return NULL;
    }
}


static void machine_status_delete_cb(lv_event_t *event)
{
    machine_status_ctx_t *ctx =
        (machine_status_ctx_t *)lv_event_get_user_data(event);
    if (ctx) lv_free(ctx);
}


static void machine_status_add_row(
    machine_status_ctx_t *ctx,
    lv_obj_t *card,
    int index,
    const char *name_text,
    const char *default_text,
    int label_x,
    int value_x,
    int value_width,
    int y)
{
    lv_obj_t **slot =
        machine_status_value_slot(ctx, index);
    if (!slot) return;

    lv_obj_t *name = lv_label_create(card);
    lv_label_set_text(name, name_text);
    ui_apply_text_caption(name);
    ui_apply_label_dim(name);
    lv_obj_set_pos(name, label_x, y + 5);

    if (index == 0) {
        ctx->nozzle_name = name;
    }

    *slot = lv_label_create(card);
    lv_label_set_text(*slot, default_text);
    lv_obj_set_width(*slot, value_width);
    ui_apply_text_value_small(*slot);
    ui_apply_label_bright(*slot);
    lv_obj_set_style_text_align(
        *slot,
        LV_TEXT_ALIGN_RIGHT,
        0);
    lv_obj_set_pos(*slot, value_x, y);
}


static void machine_status_add_live_badge(
    machine_status_ctx_t *ctx,
    lv_obj_t *card,
    int width,
    int y)
{
    ctx->live = lv_label_create(card);
    lv_label_set_text(
        ctx->live,
        ui_text(LV_SYMBOL_CLOSE " OFFLINE"));
    ui_apply_text_caption(ctx->live);
    ui_apply_label_error(ctx->live);
    lv_obj_set_width(ctx->live, 110);
    lv_obj_set_style_text_align(
        ctx->live,
        LV_TEXT_ALIGN_RIGHT,
        0);
    lv_obj_set_pos(ctx->live, width - 128, y);
}


lv_obj_t *ui_machine_status_create(
    lv_obj_t *parent,
    int x,
    int y,
    int w,
    int h)
{
    const ui_dashboard_rect_t rect = {x, y, w, h};
    const ui_dashboard_machine_layout_t layout = {
        .composition = UI_DASHBOARD_MACHINE_SINGLE_CARD,
        .label_x = 20,
        .value_x = 188,
        .split_gap = 12,
    };

    return ui_machine_status_create_profile(
        parent,
        &rect,
        &layout);
}


lv_obj_t *ui_machine_status_create_profile(
    lv_obj_t *parent,
    const ui_dashboard_rect_t *rect,
    const ui_dashboard_machine_layout_t *layout)
{
    if (!parent || !rect || !layout) return NULL;

    const char *names[] = {
        "NOZZLE",
        "BED",
        "AIR TEMPERATURE",
        "HUMIDITY",
        "SPEED",
        "FLOW",
        "PART FAN",
        "FILAMENT",
    };

    const char *defaults[] = {
        "-- / -- C",
        "-- / -- C",
        "-- C",
        "-- %RH",
        "-- mm/s",
        "-- mm3/s",
        "--%",
        "N/A",
    };

    machine_status_ctx_t *ctx =
        lv_malloc(sizeof(machine_status_ctx_t));
    if (!ctx) return NULL;
    memset(ctx, 0, sizeof(*ctx));

    lv_obj_t *host = NULL;

    if (layout->composition ==
        UI_DASHBOARD_MACHINE_SPLIT_CARDS) {
        host = lv_obj_create(parent);
        if (!host) {
            lv_free(ctx);
            return NULL;
        }

        lv_obj_set_pos(host, rect->x, rect->y);
        lv_obj_set_size(host, rect->width, rect->height);
        lv_obj_clear_flag(host, LV_OBJ_FLAG_SCROLLABLE);
        ui_apply_surface_role(
            host,
            UI_SURFACE_TRANSPARENT);
        lv_obj_set_style_pad_all(host, 0, 0);
        lv_obj_set_style_border_width(host, 0, 0);

        int card_height =
            (rect->height - layout->split_gap) / 2;

        lv_obj_t *thermal =
            ui_create_operator_card(
                host,
                0,
                0,
                rect->width,
                card_height);

        lv_obj_t *process =
            ui_create_operator_card(
                host,
                0,
                card_height + layout->split_gap,
                rect->width,
                card_height);

        if (!thermal || !process) {
            lv_obj_delete(host);
            lv_free(ctx);
            return NULL;
        }

        ui_create_operator_card_heading(
            thermal,
            "THERMAL",
            16,
            8);
        machine_status_add_live_badge(
            ctx,
            thermal,
            rect->width,
            10);
        ui_create_operator_card_divider(
            thermal,
            16,
            36,
            rect->width - 32);

        ui_create_operator_card_heading(
            process,
            "PROCESS",
            16,
            8);
        ui_create_operator_card_divider(
            process,
            16,
            36,
            rect->width - 32);

        const int compact_y[] = {42, 66, 90, 114};
        int value_width =
            rect->width - layout->value_x - 16;

        for (int index = 0; index < 4; ++index) {
            machine_status_add_row(
                ctx,
                thermal,
                index,
                names[index],
                defaults[index],
                layout->label_x,
                layout->value_x,
                value_width,
                compact_y[index]);
        }

        for (int index = 4; index < 8; ++index) {
            machine_status_add_row(
                ctx,
                process,
                index,
                names[index],
                defaults[index],
                layout->label_x,
                layout->value_x,
                value_width,
                compact_y[index - 4]);
        }
    } else {
        host =
            ui_create_operator_card(
                parent,
                rect->x,
                rect->y,
                rect->width,
                rect->height);

        if (!host) {
            lv_free(ctx);
            return NULL;
        }

        ui_create_operator_card_heading(
            host,
            "MACHINE STATUS",
            18,
            12);
        machine_status_add_live_badge(
            ctx,
            host,
            rect->width,
            16);
        ui_create_operator_card_divider(
            host,
            18,
            45,
            rect->width - 36);

        const int row_y[] = {
            56, 84, 120, 148, 184, 212, 248, 276,
        };
        int value_width =
            rect->width - layout->value_x - 20;

        for (int index = 0; index < 8; ++index) {
            machine_status_add_row(
                ctx,
                host,
                index,
                names[index],
                defaults[index],
                layout->label_x,
                layout->value_x,
                value_width,
                row_y[index]);

            if (index == 1 ||
                index == 3 ||
                index == 5) {
                ui_create_operator_card_divider(
                    host,
                    18,
                    row_y[index] + 28,
                    rect->width - 36);
            }
        }
    }

    lv_obj_set_user_data(host, ctx);
    lv_obj_add_event_cb(
        host,
        machine_status_delete_cb,
        LV_EVENT_DELETE,
        ctx);

    return host;
}

void ui_machine_status_set_filament(
    lv_obj_t *panel,
    bool moonraker_online,
    const moonraker_filament_state_t *state)
{
    if (!panel) return;

    machine_status_ctx_t *ctx =
        (machine_status_ctx_t *)lv_obj_get_user_data(panel);

    if (!ctx || !ctx->filament) return;

    if (state &&
        state->discovered &&
        state->total_count > 0 &&
        !moonraker_online) {
        lv_label_set_text(
            ctx->filament,
            ui_text("OFFLINE"));
        ui_apply_label_error(
            ctx->filament);
        return;
    }

    size_t present = 0;
    size_t enabled = 0;

    moonraker_filament_status_t status =
        moonraker_filament_state_status(
            state,
            &present,
            &enabled);

    char text[24];

    switch (status) {
        case MOONRAKER_FILAMENT_ABSENT:
            snprintf(text, sizeof(text), "N/A");
            ui_apply_label_dim(ctx->filament);
            break;

        case MOONRAKER_FILAMENT_CHECKING:
            snprintf(text, sizeof(text), "CHECKING");
            ui_apply_label_warning(ctx->filament);
            break;

        case MOONRAKER_FILAMENT_READY:
            if (enabled <= 1) {
                snprintf(
                    text,
                    sizeof(text),
                    "PRESENT");
            } else {
                snprintf(
                    text,
                    sizeof(text),
                    "%u/%u PRESENT",
                    (unsigned)present,
                    (unsigned)enabled);
            }
            ui_apply_label_success(ctx->filament);
            break;

        case MOONRAKER_FILAMENT_RUNOUT:
            if (enabled <= 1) {
                snprintf(
                    text,
                    sizeof(text),
                    "RUNOUT");
            } else {
                snprintf(
                    text,
                    sizeof(text),
                    "%u/%u RUNOUT",
                    (unsigned)present,
                    (unsigned)enabled);
            }
            ui_apply_label_error(ctx->filament);
            break;

        case MOONRAKER_FILAMENT_DISABLED:
            snprintf(text, sizeof(text), "DISABLED");
            ui_apply_label_dim(ctx->filament);
            break;

        case MOONRAKER_FILAMENT_UNKNOWN:
        default:
            snprintf(text, sizeof(text), "--");
            ui_apply_label_dim(ctx->filament);
            break;
    }

    lv_label_set_text(ctx->filament, text);
}


void ui_machine_status_set_connection(
    lv_obj_t *panel,
    bool online)
{
    if (!panel) return;

    machine_status_ctx_t *ctx =
        (machine_status_ctx_t *)lv_obj_get_user_data(panel);

    if (!ctx || !ctx->live) return;

    lv_label_set_text(
        ctx->live,
        online
            ? ui_text(LV_SYMBOL_OK " LIVE")
            : ui_text(LV_SYMBOL_CLOSE " OFFLINE"));

    if (online) {
        ui_apply_label_success(ctx->live);
    } else {
        ui_apply_label_error(ctx->live);
    }
}


void ui_machine_status_set_active_hotend(
    lv_obj_t *panel,
    const char *name,
    const char *value)
{
    if (!panel) return;

    machine_status_ctx_t *ctx =
        (machine_status_ctx_t *)lv_obj_get_user_data(panel);

    if (!ctx) return;

    if (ctx->nozzle_name) {
        lv_label_set_text(
            ctx->nozzle_name,
            name && name[0] ? name : ui_text("NOZZLE"));
    }

    if (ctx->nozzle) {
        lv_label_set_text(
            ctx->nozzle,
            value && value[0] ? value : ui_text("-- / -- C"));
    }
}


void ui_machine_status_set(
    lv_obj_t *panel,
    const char *nozzle,
    const char *bed,
    const char *chamber,
    const char *humidity,
    const char *speed,
    const char *flow,
    const char *fan
)
{
    if (!panel) return;
    machine_status_ctx_t *ctx = (machine_status_ctx_t *)lv_obj_get_user_data(panel);
    if (!ctx) return;

    lv_label_set_text(ctx->nozzle, nozzle ? nozzle : ui_text("-- / -- C"));
    lv_label_set_text(ctx->bed, bed ? bed : ui_text("-- / -- C"));
    lv_label_set_text(ctx->air, chamber ? chamber : ui_text("-- C"));
    lv_label_set_text(ctx->humidity, humidity ? humidity : "-- %RH");
    lv_label_set_text(ctx->speed, speed ? speed : "100%");
    lv_label_set_text(ctx->flow, flow ? flow : "100%");
    lv_label_set_text(ctx->fan, fan ? fan : "--%");
}
