#include "ui_machine_status.h"
#include "ui_text.h"
#include "ui_hotend_readings.h"
#include "ui_studio_temperature.h"
#include "ui_theme.h"
#include "ui_widgets.h"
#include "ui_text_fit.h"
#include "ui_studio_layout.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    lv_obj_t *live;
    lv_obj_t *filament;
    lv_obj_t *nozzle_name;
    lv_obj_t *nozzle;
    lv_obj_t *bed;
    lv_obj_t *nozzle_setpoint;
    lv_obj_t *bed_setpoint;
    lv_obj_t *air;
    lv_obj_t *humidity;
    lv_obj_t *speed;
    lv_obj_t *flow;
    lv_obj_t *fan;
    bool studio;
    lv_obj_t *hotend_readings[MOONRAKER_MAX_HOTENDS];
    ui_hotend_layout_t hotend_layout;
    size_t hotend_layout_count;
    lv_obj_t *thermal_names[3];
    int thermal_y[3];
    lv_obj_t *thermal_divider;
    bool split;
    bool multi;
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
    } else if (index <= 3) {
        ctx->thermal_names[index - 1] = name;
        ctx->thermal_y[index - 1] = y;
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


static lv_obj_t *studio_label(lv_obj_t *p,const char *t,int x,int y,int w,const lv_font_t *f,lv_color_t c)
{
    lv_obj_t *o=lv_label_create(p); lv_label_set_text(o,t);
    lv_obj_set_pos(o,x,y); lv_obj_set_width(o,w);
    lv_label_set_long_mode(o,LV_LABEL_LONG_MODE_DOTS);
    ui_apply_custom_label_style(o,f,c);
    ui_text_fit_single_line(o,f); return o;
}
static lv_obj_t *studio_machine_create(lv_obj_t *parent,const ui_dashboard_rect_t *rect,machine_status_ctx_t *ctx)
{
    lv_obj_t *host=studio_plane(parent,rect->x,rect->y,rect->width,rect->height);
    ctx->studio = true;
    studio_rule(host,736,0,1,330);
    ctx->nozzle_name=studio_label(host,"Nozzle",756,4,220,UI_FONT_BODY_LARGE,UI_TEXT_DIM);
    ctx->nozzle=ui_studio_temperature_reading(host,756,34,220);
    ctx->nozzle_setpoint=ui_studio_temperature_setpoint(host,756,94,220);
    ctx->hotend_layout = ui_studio_hotend_layout(756, 34);
    ui_hotend_create_readings(host, ctx->hotend_readings);
    studio_rule(host,756,134,220,1);
    studio_label(host,"Bed",756,154,220,UI_FONT_BODY_LARGE,UI_TEXT_DIM);
    ctx->bed=ui_studio_temperature_reading(host,756,184,220);
    ctx->bed_setpoint=ui_studio_temperature_setpoint(host,756,244,220);
    ctx->live=studio_label(host,"OFFLINE",756,298,96,UI_FONT_CAPTION,UI_DANGER_BRIGHT);
    ctx->filament=studio_label(host,"--",856,298,120,UI_FONT_CAPTION,UI_TEXT_DIM);
    studio_rule(host,0,346,976,1);
    const char *names[]={"Speed","Flow","Fan","Drybox air","Humidity"};
    lv_obj_t **values[]={&ctx->speed,&ctx->flow,&ctx->fan,&ctx->air,&ctx->humidity};
    for(int i=0;i<5;i++) {
        int x=i*196;
        studio_label(host,names[i],x,356,184,UI_FONT_CAPTION,UI_TEXT_DIM);
        *values[i]=studio_label(host,"--",x,386,184,UI_FONT_BODY_LARGE,UI_TEXT);
        if(i)studio_rule(host,x-10,356,1,60);
    }
    lv_obj_set_user_data(host,ctx);
    lv_obj_add_event_cb(host,machine_status_delete_cb,LV_EVENT_DELETE,ctx);
    return host;
}

lv_obj_t *ui_machine_status_create_profile(
    lv_obj_t *parent,
    const ui_dashboard_rect_t *rect,
    const ui_dashboard_machine_layout_t *layout)
{
    if (!parent || !rect || !layout) return NULL;

    const char *names[] = {
        "Nozzle",
        "Bed",
        "Air temp",
        "Humidity",
        "Speed",
        "Flow",
        "Part fan",
        "Filament",
    };

    const char *defaults[] = {
        UI_TEMPERATURE_PAIR_UNKNOWN,
        UI_TEMPERATURE_PAIR_UNKNOWN,
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

    if (ui_theme_is_studio()) return studio_machine_create(parent,rect,ctx);

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
            "Thermal",
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
            "Process",
            16,
            8);
        ui_create_operator_card_divider(
            process,
            16,
            36,
            rect->width - 32);

        ctx->split = true;
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

        ctx->hotend_layout = (ui_hotend_layout_t){layout->label_x, 38,
            rect->width - layout->label_x - 16, 34, 2, 2,
            &lv_font_montserrat_22, &lv_font_montserrat_16};
        ui_hotend_create_readings(thermal, ctx->hotend_readings);

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
            "Machine status",
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
                if (index == 1)
                    ctx->thermal_divider = lv_obj_get_child(host, -1);
            }
        }
        ctx->hotend_layout = (ui_hotend_layout_t){layout->label_x, 50,
            rect->width - layout->label_x - 20, 46, 2, 2,
            &lv_font_montserrat_22, &lv_font_montserrat_18};
        ui_hotend_create_readings(host, ctx->hotend_readings);
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


void ui_machine_status_set_hotends(lv_obj_t *panel,
    const moonraker_hotend_t *hotends, size_t count, bool online)
{
    if (!panel) return;
    machine_status_ctx_t *ctx = lv_obj_get_user_data(panel);
    if (!ctx) return;
    bool multi = hotends && count > 1;
    if (ctx->multi != multi) {
        ctx->multi = multi;
        ui_hotend_visible(ctx->nozzle, !multi);
        if (ctx->studio) {
            lv_label_set_text(ctx->nozzle_name, ui_text(multi ? "Tools" : "Nozzle"));
            ui_hotend_visible(ctx->nozzle_setpoint, !multi);
        } else {
            ui_hotend_visible(ctx->nozzle_name, !multi);
            static const int split_y[] = {73, 96, 119};
            static const int single_y[] = {97, 124, 149};
            for (int i = 0; i < 3; ++i) {
                int y = multi ? (ctx->split ? split_y[i] : single_y[i]) : ctx->thermal_y[i];
                lv_obj_set_y(ctx->thermal_names[i], y + 5);
                lv_obj_set_y(*machine_status_value_slot(ctx, i + 1), y);
            }
            ui_hotend_visible(ctx->thermal_divider, !multi);
        }
    }
    size_t layout_count = multi ? LV_MIN(count, MOONRAKER_MAX_HOTENDS) : 0;
    if (ctx->hotend_layout_count != layout_count) {
        ctx->hotend_layout_count = layout_count;
        ui_hotend_layout_readings(ctx->hotend_readings, &ctx->hotend_layout, layout_count);
    }
    ui_hotend_update_readings(ctx->hotend_readings, hotends, count, online);
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

    if (ctx->studio) {
        ui_studio_set_temperature(ctx->nozzle, ctx->nozzle_setpoint, nozzle);
        ui_studio_set_temperature(ctx->bed, ctx->bed_setpoint, bed);
    } else {
        ui_value_set_text(ctx->nozzle, nozzle ? nozzle : UI_TEMPERATURE_PAIR_UNKNOWN);
        ui_value_set_text(ctx->bed, bed ? bed : UI_TEMPERATURE_PAIR_UNKNOWN);
    }
    lv_label_set_text(ctx->air, chamber ? chamber : ui_text("-- C"));
    lv_label_set_text(ctx->humidity, humidity ? humidity : "-- %RH");
    lv_label_set_text(ctx->speed, speed ? speed : "100%");
    lv_label_set_text(ctx->flow, flow ? flow : "100%");
    lv_label_set_text(ctx->fan, fan ? fan : "--%");
    if (ctx->studio) {
        ui_text_fit_single_line(ctx->speed,UI_FONT_BODY_LARGE);
        ui_text_fit_single_line(ctx->flow,UI_FONT_BODY_LARGE);
        ui_text_fit_single_line(ctx->air,UI_FONT_VALUE);
        ui_text_fit_single_line(ctx->humidity,UI_FONT_VALUE);
    }
}
