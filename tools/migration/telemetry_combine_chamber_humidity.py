#!/usr/bin/env python3

from pathlib import Path
import re

path = Path("main/ui_telemetry_charts.c")

if not path.is_file():
    raise RuntimeError(f"missing file: {path}")

text = path.read_text()

old_legend = """    telemetry_create_legend_item(
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

new_legend = """    telemetry_create_legend_item(
        panel,
        18,
        38,
        UI_ACCENT_CYAN,
        "Nozzle");

    telemetry_create_legend_item(
        panel,
        100,
        38,
        UI_WARN,
        "Bed");

    telemetry_create_legend_item(
        panel,
        170,
        38,
        UI_TELEMETRY_CHAMBER,
        "Chamber");

    telemetry_create_legend_item(
        panel,
        280,
        38,
        UI_TELEMETRY_HUMIDITY,
        "Humidity");

    telemetry_create_legend_item(
        panel,
        390,
        38,
        UI_BORDER_BRIGHT,
        "Target reference");"""

if text.count(old_legend) != 1:
    raise RuntimeError(
        f"expected one three-item legend, "
        f"found {text.count(old_legend)}"
    )

text = text.replace(
    old_legend,
    new_legend,
    1,
)

old_chamber = """    telemetry_create_single_chart(
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

new_chamber = """    telemetry_create_single_chart(
        panel,
        &s_chamber_chart,
        18,
        174,
        770,
        "CHAMBER  -- C    MIN --    MAX --",
        UI_TELEMETRY_CHAMBER,
        UI_TELEMETRY_CHAMBER,
        2.0,
        0.0,
        80.0);"""

if text.count(old_chamber) != 1:
    raise RuntimeError(
        f"expected one lower Chamber chart, "
        f"found {text.count(old_chamber)}"
    )

text = text.replace(
    old_chamber,
    new_chamber,
    1,
)

old_humidity = """    telemetry_create_single_chart(
        panel,
        &s_humidity_chart,
        416,
        174,
        372,
        "HUMIDITY  -- %RH    MIN --    MAX --",
        UI_TELEMETRY_HUMIDITY,
        UI_TELEMETRY_HUMIDITY,
        4.0,
        0.0,
        100.0);"""

new_humidity = """    telemetry_create_overlay_series(
        &s_chamber_chart,
        &s_humidity_chart,
        "HUMIDITY  -- %RH    MIN --    MAX --",
        UI_TELEMETRY_HUMIDITY,
        UI_TELEMETRY_HUMIDITY,
        LV_CHART_AXIS_SECONDARY_Y,
        4.0,
        0.0,
        100.0);"""

if text.count(old_humidity) != 1:
    raise RuntimeError(
        f"expected one standalone Humidity chart, "
        f"found {text.count(old_humidity)}"
    )

text = text.replace(
    old_humidity,
    new_humidity,
    1,
)

path.write_text(text)

updated = path.read_text()

standalone_humidity = re.search(
    r"telemetry_create_single_chart\s*\(\s*"
    r"panel\s*,\s*&s_humidity_chart",
    updated,
    re.DOTALL,
)

if standalone_humidity:
    raise RuntimeError(
        "standalone Humidity chart creation remains"
    )

combined_chamber = re.search(
    r"telemetry_create_single_chart\s*\(\s*"
    r"panel\s*,\s*&s_chamber_chart\s*,\s*"
    r"18\s*,\s*174\s*,\s*770\s*,",
    updated,
    re.DOTALL,
)

if not combined_chamber:
    raise RuntimeError(
        "full-width Chamber/Humidity chart missing"
    )

overlay_calls = len(re.findall(
    r"^\s{4}telemetry_create_overlay_series\s*\(",
    updated,
    re.MULTILINE,
))

if overlay_calls != 2:
    raise RuntimeError(
        f"expected two overlay calls, found {overlay_calls}"
    )

secondary_axes = updated.count(
    "LV_CHART_AXIS_SECONDARY_Y"
)

if secondary_axes != 2:
    raise RuntimeError(
        f"expected two secondary axes, found {secondary_axes}"
    )

print("PASS: Chamber and Humidity now share one chart surface")
print("Telemetry page now contains two combined charts")
print("Changed:")
print("  main/ui_telemetry_charts.c")
