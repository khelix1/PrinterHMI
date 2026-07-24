#!/usr/bin/env python3

from pathlib import Path

path = Path("main/ui_telemetry_charts.c")

if not path.is_file():
    raise RuntimeError(f"missing file: {path}")

text = path.read_text()

struct_anchor = """    lv_obj_t *chart;
    lv_chart_series_t *actual_series;

    lv_obj_t *stats_label;"""

struct_replacement = """    lv_obj_t *chart;
    lv_chart_series_t *actual_series;
    lv_chart_axis_t axis;

    lv_obj_t *stats_label;"""

if text.count(struct_anchor) != 1:
    raise RuntimeError(
        f"expected one chart-structure anchor, "
        f"found {text.count(struct_anchor)}"
    )

text = text.replace(
    struct_anchor,
    struct_replacement,
    1,
)

fallback_axis = """            lv_chart_set_range(
                chart->chart,
                LV_CHART_AXIS_PRIMARY_Y,
                chart->axis_min,
                chart->axis_max);"""

fallback_replacement = """            lv_chart_set_range(
                chart->chart,
                chart->axis,
                chart->axis_min,
                chart->axis_max);"""

if text.count(fallback_axis) != 1:
    raise RuntimeError(
        f"expected one fallback-axis assignment, "
        f"found {text.count(fallback_axis)}"
    )

text = text.replace(
    fallback_axis,
    fallback_replacement,
    1,
)

adaptive_axis = """    lv_chart_set_range(
        chart->chart,
        LV_CHART_AXIS_PRIMARY_Y,
        chart->axis_min,
        chart->axis_max);"""

adaptive_replacement = """    lv_chart_set_range(
        chart->chart,
        chart->axis,
        chart->axis_min,
        chart->axis_max);"""

if text.count(adaptive_axis) != 1:
    raise RuntimeError(
        f"expected one adaptive-axis assignment, "
        f"found {text.count(adaptive_axis)}"
    )

text = text.replace(
    adaptive_axis,
    adaptive_replacement,
    1,
)

config_anchor = """    out->actual_color = actual_color;
    out->target_color = target_color;

    out->stats_label = telemetry_make_label("""

config_replacement = """    out->actual_color = actual_color;
    out->target_color = target_color;
    out->axis = LV_CHART_AXIS_PRIMARY_Y;

    out->stats_label = telemetry_make_label("""

if text.count(config_anchor) != 1:
    raise RuntimeError(
        f"expected one chart configuration anchor, "
        f"found {text.count(config_anchor)}"
    )

text = text.replace(
    config_anchor,
    config_replacement,
    1,
)

initial_range = """    lv_chart_set_range(
        out->chart,
        LV_CHART_AXIS_PRIMARY_Y,
        out->axis_min,
        out->axis_max);"""

initial_replacement = """    lv_chart_set_range(
        out->chart,
        out->axis,
        out->axis_min,
        out->axis_max);"""

if text.count(initial_range) != 1:
    raise RuntimeError(
        f"expected one initial chart range, "
        f"found {text.count(initial_range)}"
    )

text = text.replace(
    initial_range,
    initial_replacement,
    1,
)

series_axis = """    out->actual_series = lv_chart_add_series(
        out->chart,
        actual_color,
        LV_CHART_AXIS_PRIMARY_Y);"""

series_replacement = """    out->actual_series = lv_chart_add_series(
        out->chart,
        actual_color,
        out->axis);"""

if text.count(series_axis) != 1:
    raise RuntimeError(
        f"expected one primary series creation, "
        f"found {text.count(series_axis)}"
    )

text = text.replace(
    series_axis,
    series_replacement,
    1,
)

path.write_text(text)

updated = path.read_text()

if updated.count("lv_chart_axis_t axis;") != 1:
    raise RuntimeError("chart axis field missing after migration")

if updated.count("chart->axis") != 2:
    raise RuntimeError(
        "expected two runtime chart-axis uses, found "
        f"{updated.count('chart->axis')}"
    )

if updated.count("out->axis") != 3:
    raise RuntimeError(
        "expected three chart-construction axis uses, found "
        f"{updated.count('out->axis')}"
    )

print("PASS: telemetry instruments now own their LVGL axis")
print("No visual or sampling behavior changed")
print("Changed:")
print("  main/ui_telemetry_charts.c")
