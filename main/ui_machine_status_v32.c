#include "ui_machine_status_v32.h"
#include "ui_theme.h"
#include "ui_widgets.h"

typedef struct {
    lv_obj_t *nozzle;
    lv_obj_t *bed;
    lv_obj_t *air;
    lv_obj_t *humidity;
    lv_obj_t *speed;
    lv_obj_t *flow;
    lv_obj_t *fan;
} machine_status_ctx_t;

lv_obj_t *ui_machine_status_v32_create(
    lv_obj_t *parent,
    int x,
    int y,
    int w,
    int h)
{
    if (!parent) {
        return NULL;
    }

    lv_obj_t *panel =
        ui_create_operator_card(
            parent,
            x,
            y,
            w,
            h);

    if (!panel) {
        return NULL;
    }

    machine_status_ctx_t *ctx =
        lv_malloc(sizeof(machine_status_ctx_t));

    if (!ctx) {
        return panel;
    }

    ui_create_operator_card_heading(
        panel,
        "MACHINE STATUS",
        18,
        12);

    lv_obj_t *live = lv_label_create(panel);

    lv_label_set_text(
        live,
        LV_SYMBOL_OK " LIVE");

    ui_apply_text_caption(live);
    ui_apply_label_success(live);

    lv_obj_set_width(live, 90);

    lv_obj_set_style_text_align(
        live,
        LV_TEXT_ALIGN_RIGHT,
        0);

    lv_obj_set_pos(
        live,
        w - 108,
        16);

    ui_create_operator_card_divider(
        panel,
        18,
        45,
        w - 36);

    const int label_x = 20;
    const int value_x = 188;
    const int value_w = w - value_x - 20;

    const char *names[] = {
        "NOZZLE",
        "BED",
        "AIR TEMPERATURE",
        "HUMIDITY",
        "SPEED FACTOR",
        "FLOW FACTOR",
        "PART FAN",
    };

    const char *defaults[] = {
        "-- / -- C",
        "-- / -- C",
        "-- C",
        "-- %RH",
        "100%",
        "100%",
        "--%",
    };

    const int row_y[] = {
        56,
        86,
        124,
        154,
        192,
        222,
        248,
    };

    lv_obj_t **values[] = {
        &ctx->nozzle,
        &ctx->bed,
        &ctx->air,
        &ctx->humidity,
        &ctx->speed,
        &ctx->flow,
        &ctx->fan,
    };

    for (int index = 0; index < 7; ++index) {
        lv_obj_t *name =
            lv_label_create(panel);

        lv_label_set_text(
            name,
            names[index]);

        ui_apply_text_caption(name);
        ui_apply_label_dim(name);

        lv_obj_set_pos(
            name,
            label_x,
            row_y[index] + 5);

        *values[index] =
            lv_label_create(panel);

        lv_label_set_text(
            *values[index],
            defaults[index]);

        lv_obj_set_width(
            *values[index],
            value_w);

        ui_apply_text_value_small(
            *values[index]);
        ui_apply_label_bright(
            *values[index]);

        lv_obj_set_style_text_align(
            *values[index],
            LV_TEXT_ALIGN_RIGHT,
            0);

        lv_obj_set_pos(
            *values[index],
            value_x,
            row_y[index]);

        if (index == 1 ||
            index == 3 ||
            index == 5) {
            ui_create_operator_card_divider(
                panel,
                18,
                row_y[index] + 28,
                w - 36);
        }
    }

    lv_obj_set_user_data(panel, ctx);

    return panel;
}


void ui_machine_status_v32_set(
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

    lv_label_set_text(ctx->nozzle, nozzle ? nozzle : "-- / -- C");
    lv_label_set_text(ctx->bed, bed ? bed : "-- / -- C");
    lv_label_set_text(ctx->air, chamber ? chamber : "-- C");
    lv_label_set_text(ctx->humidity, humidity ? humidity : "-- %RH");
    lv_label_set_text(ctx->speed, speed ? speed : "100%");
    lv_label_set_text(ctx->flow, flow ? flow : "100%");
    lv_label_set_text(ctx->fan, fan ? fan : "--%");
}
