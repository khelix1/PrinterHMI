#!/usr/bin/env python3

from pathlib import Path

path = Path("main/ui_telemetry_charts.c")

if not path.is_file():
    raise RuntimeError(f"missing file: {path}")

text = path.read_text()

old_comment = """    /*
     * Four independent instruments. No chart shares an axis or scale.
     */"""

new_comment = """    /*
     * Two combined chart surfaces.
     *
     * Each instrument retains its own series, adaptive range, target,
     * statistics and newest-sample marker.
     */"""

if text.count(old_comment) != 1:
    raise RuntimeError(
        f"expected one stale chart-layout comment, "
        f"found {text.count(old_comment)}"
    )

text = text.replace(
    old_comment,
    new_comment,
    1,
)

old_history_refresh = """    lv_chart_refresh(s_nozzle_chart.chart);
    lv_chart_refresh(s_bed_chart.chart);
    lv_chart_refresh(s_chamber_chart.chart);
    lv_chart_refresh(s_humidity_chart.chart);"""

new_history_refresh = """    /*
     * Nozzle/Bed and Chamber/Humidity each share one LVGL chart.
     */
    lv_chart_refresh(s_nozzle_chart.chart);
    lv_chart_refresh(s_chamber_chart.chart);"""

if text.count(old_history_refresh) != 1:
    raise RuntimeError(
        f"expected one four-chart history refresh block, "
        f"found {text.count(old_history_refresh)}"
    )

text = text.replace(
    old_history_refresh,
    new_history_refresh,
    1,
)

old_live_refresh = """    if (s_nozzle_chart.chart) {
        lv_chart_refresh(s_nozzle_chart.chart);
    }

    if (s_bed_chart.chart) {
        lv_chart_refresh(s_bed_chart.chart);
    }

    if (s_chamber_chart.chart) {
        lv_chart_refresh(s_chamber_chart.chart);
    }

    if (s_humidity_chart.chart) {
        lv_chart_refresh(s_humidity_chart.chart);
    }"""

new_live_refresh = """    if (s_nozzle_chart.chart) {
        lv_chart_refresh(s_nozzle_chart.chart);
    }

    if (s_chamber_chart.chart) {
        lv_chart_refresh(s_chamber_chart.chart);
    }"""

if text.count(old_live_refresh) != 1:
    raise RuntimeError(
        f"expected one four-chart live refresh block, "
        f"found {text.count(old_live_refresh)}"
    )

text = text.replace(
    old_live_refresh,
    new_live_refresh,
    1,
)

path.write_text(text)

print("PASS: combined telemetry refresh ownership cleaned")
print("Each shared chart now refreshes exactly once")
print("Changed:")
print("  main/ui_telemetry_charts.c")
