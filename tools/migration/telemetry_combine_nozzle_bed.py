#!/usr/bin/env python3

from pathlib import Path

path = Path("main/ui_telemetry_charts.c")

if not path.is_file():
    raise RuntimeError(f"missing file: {path}")

text = path.read_text()

if "telemetry_create_overlay_series(" in text:
    raise RuntimeError("overlay-series helper already exists")

history_anchor = """
static void telemetry_chart_load_history(void)
"""

if text.count(history_anchor) != 1:
    raise RuntimeError(
        f"expected one history-function anchor, "
        f"found {text.count(history_anchor)}"
    )

overlay_helper = r'''
static void telemetry_create_overlay_series(
    telemetry_chart_t *base,
    telemetry_chart_t *out,
    const char *title,
    lv_color_t actual_color,
    lv_color_t target_color,
    lv_chart_axis_t axis,
    double minimum_span_c,
    double fallback_min_c,
    double fallback_max_c)
{
    if (!base ||
        !base->chart ||
        !out) {
        return;
    }

    lv_obj_t *card =
        lv_obj_get_parent(base->chart);

    if (!card) {
        return;
    }

    *out = (telemetry_chart_t){0};

    out->chart = base->chart;
    out->axis = axis;

    out->minimum_span_c = minimum_span_c;
    out->fallback_min_c = fallback_min_c;
    out->fallback_max_c = fallback_max_c;

    out->axis_locked = false;
    out->axis_anchor = NAN;
    out->axis_edge_samples = 0;

    out->actual_color = actual_color;
    out->target_color = target_color;

    out->axis_min =
        (int32_t)lround(fallback_min_c * 10.0);

    out->axis_max =
        (int32_t)lround(fallback_max_c * 10.0);

    /*
     * The combined card has two instrumentation rows.
     * Keep the shared plot below both rows.
     */
    lv_obj_set_height(base->chart, 48);
    lv_obj_set_y(base->chart, 38);

    out->stats_label = telemetry_make_label(
        card,
        title,
        &lv_font_montserrat_12,
        actual_color);

    lv_obj_set_pos(
        out->stats_label,
        12,
        21);

    out->target_label = telemetry_make_label(
        card,
        "TARGET -- C",
        &lv_font_montserrat_12,
        target_color);

    lv_obj_align(
        out->target_label,
        LV_ALIGN_TOP_RIGHT,
        -12,
        21);

    lv_chart_set_range(
        out->chart,
        out->axis,
        out->axis_min,
        out->axis_max);

    out->actual_series = lv_chart_add_series(
        out->chart,
        actual_color,
        out->axis);

    /*
     * Independent newest-sample marker for the overlay trace.
     */
    out->newest_dot =
        lv_obj_create(out->chart);

    lv_obj_set_size(
        out->newest_dot,
        8,
        8);

    lv_obj_set_pos(
        out->newest_dot,
        0,
        0);

    lv_obj_clear_flag(
        out->newest_dot,
        LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_radius(
        out->newest_dot,
        4,
        0);

    lv_obj_set_style_bg_color(
        out->newest_dot,
        actual_color,
        0);

    lv_obj_set_style_bg_opa(
        out->newest_dot,
        LV_OPA_COVER,
        0);

    lv_obj_set_style_border_color(
        out->newest_dot,
        UI_TEXT_BRIGHT,
        0);

    lv_obj_set_style_border_width(
        out->newest_dot,
        1,
        0);

    lv_obj_set_style_shadow_color(
        out->newest_dot,
        actual_color,
        0);

    lv_obj_set_style_shadow_width(
        out->newest_dot,
        8,
        0);

    lv_obj_set_style_shadow_opa(
        out->newest_dot,
        LV_OPA_70,
        0);

    lv_obj_set_style_pad_all(
        out->newest_dot,
        0,
        0);

    lv_obj_add_flag(
        out->newest_dot,
        LV_OBJ_FLAG_HIDDEN);

    /*
     * Independent target reference for the Bed series.
     */
    out->target_line =
        lv_obj_create(out->chart);

    lv_obj_set_size(
        out->target_line,
        lv_pct(100),
        2);

    lv_obj_set_pos(
        out->target_line,
        0,
        0);

    lv_obj_clear_flag(
        out->target_line,
        LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_radius(
        out->target_line,
        1,
        0);

    lv_obj_set_style_bg_color(
        out->target_line,
        target_color,
        0);

    lv_obj_set_style_bg_opa(
        out->target_line,
        LV_OPA_70,
        0);

    lv_obj_set_style_border_width(
        out->target_line,
        0,
        0);

    lv_obj_set_style_pad_all(
        out->target_line,
        0,
        0);

    lv_obj_add_flag(
        out->target_line,
        LV_OBJ_FLAG_HIDDEN);

    lv_obj_move_foreground(
        out->newest_dot);
}
'''

text = text.replace(
    history_anchor,
    overlay_helper + history_anchor,
    1,
)

old_legend = """    telemetry_create_legend_item(
        panel,
        18,
        38,
        UI_ACCENT_CYAN,
        "Live trace");

    telemetry_create_legend_item(
        panel,
        112,
        38,
        UI_BORDER_BRIGHT,
        "Target reference");"""

new_legend = """    telemetry_create_legend_item(
        panel,
        18,
        38,
        UI_ACCENT_CYAN,
        "Nozzle");

    telemetry_create_legend_item(
        panel,
        104,
        38,
        UI_WARN,
        "Bed");

    telemetry_create_legend_item(
        panel,
        170,
        38,
        UI_BORDER_BRIGHT,
        "Target reference");"""

if text.count(old_legend) != 1:
    raise RuntimeError(
        f"expected one existing legend block, "
        f"found {text.count(old_legend)}"
    )

text = text.replace(
    old_legend,
    new_legend,
    1,
)

old_nozzle = """    telemetry_create_single_chart(
        panel,
        &s_nozzle_chart,
        18,
        58,
        372,
        "NOZZLE  -- C    MIN --    MAX --",
        UI_ACCENT_CYAN,
        UI_BORDER_BRIGHT,
        4.0,
        0.0,
        300.0);"""

new_nozzle = """    telemetry_create_single_chart(
        panel,
        &s_nozzle_chart,
        18,
        58,
        770,
        "NOZZLE  -- C    MIN --    MAX --",
        UI_ACCENT_CYAN,
        UI_BORDER_BRIGHT,
        4.0,
        0.0,
        300.0);"""

if text.count(old_nozzle) != 1:
    raise RuntimeError(
        f"expected one Nozzle chart creation, "
        f"found {text.count(old_nozzle)}"
    )

text = text.replace(
    old_nozzle,
    new_nozzle,
    1,
)

old_chamber = """    telemetry_create_single_chart(
        panel,
        &s_chamber_chart,
        416,
        58,
        372,
        "CHAMBER  -- C    MIN --    MAX --",
        UI_TELEMETRY_CHAMBER,
        UI_TELEMETRY_CHAMBER,
        2.0,
        0.0,
        80.0);"""

new_chamber = """    telemetry_create_single_chart(
        panel,
        &s_chamber_chart,
        18,
        174,
        372,
        "CHAMBER  -- C    MIN --    MAX --",
        UI_TELEMETRY_CHAMBER,
        UI_TELEMETRY_CHAMBER,
        2.0,
        0.0,
        80.0);"""

if text.count(old_chamber) != 1:
    raise RuntimeError(
        f"expected one Chamber chart creation, "
        f"found {text.count(old_chamber)}"
    )

text = text.replace(
    old_chamber,
    new_chamber,
    1,
)

old_bed = """    telemetry_create_single_chart(
        panel,
        &s_bed_chart,
        18,
        174,
        372,
        "BED  -- C    MIN --    MAX --",
        UI_WARN,
        UI_TELEMETRY_BED_TRACE,
        2.0,
        0.0,
        130.0);"""

new_bed = """    telemetry_create_overlay_series(
        &s_nozzle_chart,
        &s_bed_chart,
        "BED  -- C    MIN --    MAX --",
        UI_WARN,
        UI_TELEMETRY_BED_TRACE,
        LV_CHART_AXIS_SECONDARY_Y,
        2.0,
        0.0,
        130.0);"""

if text.count(old_bed) != 1:
    raise RuntimeError(
        f"expected one Bed chart creation, "
        f"found {text.count(old_bed)}"
    )

text = text.replace(
    old_bed,
    new_bed,
    1,
)

path.write_text(text)

updated = path.read_text()

checks = {
    "overlay helper": updated.count(
        "static void telemetry_create_overlay_series("
    ),
    "overlay call": updated.count(
        "    telemetry_create_overlay_series("
    ),
    "secondary axis": updated.count(
        "LV_CHART_AXIS_SECONDARY_Y"
    ),
    "full-width heater chart": updated.count(
        """        770,
        "NOZZLE"""
    ),
}

for description, count in checks.items():
    if count != 1:
        raise RuntimeError(
            f"{description}: expected 1, found {count}"
        )

if updated.count("&s_bed_chart,") != 6:
    raise RuntimeError(
        "unexpected Bed chart reference count after merge: "
        f"{updated.count('&s_bed_chart,')}"
    )

print("PASS: Nozzle and Bed now share one chart surface")
print("Nozzle uses the primary Y-axis")
print("Bed uses the secondary Y-axis")
print("Chamber and Humidity remain independent")
print("Changed:")
print("  main/ui_telemetry_charts.c")
