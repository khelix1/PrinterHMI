#include "ui_telemetry_charts.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "telemetry_history.h"
#include "ui_telemetry_components.h"
#include "ui_theme.h"
#include "ui_page_geometry_v32.h"

typedef struct {
    lv_obj_t *chart;
    lv_chart_series_t *actual_series;
    lv_chart_axis_t axis;

    lv_obj_t *stats_label;
    lv_obj_t *target_label;
    lv_obj_t *target_line;
    lv_obj_t *newest_dot;

    lv_obj_t *oldest_time_label;
    lv_obj_t *newest_time_label;

    int32_t axis_min;
    int32_t axis_max;

    /*
     * The axis is deliberately stable. Live samples move inside the
     * instrument instead of continuously moving the grid itself.
     */
    bool axis_locked;
    double axis_anchor;

    /*
     * Number of consecutive samples outside the stable inner window.
     * The axis only recenters after several edge samples.
     */
    uint8_t axis_edge_samples;

    double minimum_span_c;
    double fallback_min_c;
    double fallback_max_c;

    lv_color_t actual_color;
    lv_color_t target_color;
} telemetry_chart_t;

typedef struct {
    bool valid;
    double current;
    double minimum;
    double maximum;
} telemetry_range_stats_t;

static telemetry_chart_t s_nozzle_chart = {0};
static telemetry_chart_t s_bed_chart = {0};
static telemetry_chart_t s_chamber_chart = {0};
static telemetry_chart_t s_humidity_chart = {0};

static void telemetry_range_add(
    telemetry_range_stats_t *stats,
    double value)
{
    if (!stats || !isfinite(value) || value < -100.0) {
        return;
    }

    if (!stats->valid) {
        stats->valid = true;
        stats->current = value;
        stats->minimum = value;
        stats->maximum = value;
        return;
    }

    stats->current = value;

    if (value < stats->minimum) {
        stats->minimum = value;
    }

    if (value > stats->maximum) {
        stats->maximum = value;
    }
}

static void telemetry_calculate_axis(
    const telemetry_range_stats_t *stats,
    double fallback_min_c,
    double fallback_max_c,
    double minimum_span_c,
    int32_t *axis_min,
    int32_t *axis_max)
{
    if (!axis_min || !axis_max) {
        return;
    }

    if (!stats || !stats->valid) {
        *axis_min = (int32_t)lround(fallback_min_c * 10.0);
        *axis_max = (int32_t)lround(fallback_max_c * 10.0);
        return;
    }

    double low = stats->minimum;
    double high = stats->maximum;
    double span = high - low;

    if (span < minimum_span_c) {
        double center = (low + high) * 0.5;
        double half_span = minimum_span_c * 0.5;

        low = center - half_span;
        high = center + half_span;
    } else {
        double padding = span * 0.20;

        if (padding < 0.4) {
            padding = 0.4;
        }

        low -= padding;
        high += padding;
    }

    low = floor(low * 2.0) / 2.0;
    high = ceil(high * 2.0) / 2.0;

    if (low < 0.0) {
        low = 0.0;
    }

    if (high > 320.0) {
        high = 320.0;
    }

    if (high - low < minimum_span_c) {
        high = low + minimum_span_c;
    }

    *axis_min = (int32_t)lround(low * 10.0);
    *axis_max = (int32_t)lround(high * 10.0);
}

static int32_t telemetry_chart_value(double value)
{
    if (!isfinite(value) || value < -100.0) {
        return LV_CHART_POINT_NONE;
    }

    if (value > 320.0) {
        value = 320.0;
    }

    if (value < 0.0) {
        value = 0.0;
    }

    return (int32_t)lround(value * 10.0);
}

static void telemetry_position_target_line(
    telemetry_chart_t *chart,
    double target)
{
    if (!chart || !chart->chart || !chart->target_line) {
        return;
    }

    if (!isfinite(target) || target <= 0.0) {
        lv_obj_add_flag(chart->target_line, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    int32_t target_value = telemetry_chart_value(target);

    if (target_value == LV_CHART_POINT_NONE ||
        target_value < chart->axis_min ||
        target_value > chart->axis_max ||
        chart->axis_max <= chart->axis_min) {
        /*
         * Do not widen the graph for a distant target during warm-up.
         */
        lv_obj_add_flag(chart->target_line, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    int chart_height = lv_obj_get_height(chart->chart);
    int usable_height = chart_height - 4;

    int32_t span = chart->axis_max - chart->axis_min;
    int32_t from_top = chart->axis_max - target_value;

    int y = 2 + (int)(
        ((int64_t)from_top * usable_height) /
        span);

    lv_obj_set_y(chart->target_line, y);
    lv_obj_remove_flag(chart->target_line, LV_OBJ_FLAG_HIDDEN);
}

static void telemetry_position_newest_dot(
    telemetry_chart_t *chart,
    const telemetry_range_stats_t *stats)
{
    if (!chart ||
        !chart->chart ||
        !chart->newest_dot ||
        !stats ||
        !stats->valid) {
        if (chart && chart->newest_dot) {
            lv_obj_add_flag(
                chart->newest_dot,
                LV_OBJ_FLAG_HIDDEN);
        }

        return;
    }

    int32_t value =
        telemetry_chart_value(stats->current);

    if (value == LV_CHART_POINT_NONE ||
        value < chart->axis_min ||
        value > chart->axis_max ||
        chart->axis_max <= chart->axis_min) {
        lv_obj_add_flag(
            chart->newest_dot,
            LV_OBJ_FLAG_HIDDEN);

        return;
    }

    int chart_width =
        lv_obj_get_width(chart->chart);

    int chart_height =
        lv_obj_get_height(chart->chart);

    /*
     * The newest LVGL shift-mode point is at the right edge.
     * Leave a small inset so the marker remains fully visible.
     */
    int x = chart_width - 9;

    int usable_height = chart_height - 8;
    int32_t span = chart->axis_max - chart->axis_min;
    int32_t from_top = chart->axis_max - value;

    int y = 4 + (int)(
        ((int64_t)from_top * usable_height) /
        span);

    lv_obj_set_pos(
        chart->newest_dot,
        x,
        y - 4);

    lv_obj_remove_flag(
        chart->newest_dot,
        LV_OBJ_FLAG_HIDDEN);

    lv_obj_move_foreground(chart->newest_dot);
}

static void telemetry_update_chart_stats(
    telemetry_chart_t *chart,
    const char *name,
    const char *unit,
    const telemetry_range_stats_t *stats,
    bool has_target,
    double target)
{
    if (!chart) {
        return;
    }

    if (chart->stats_label) {
        char buf[128];

        if (stats && stats->valid) {
            snprintf(
                buf,
                sizeof(buf),
                "%s  %.1f %s    MIN %.1f    MAX %.1f",
                name,
                stats->current,
                unit,
                stats->minimum,
                stats->maximum);
        } else {
            snprintf(
                buf,
                sizeof(buf),
                "%s  -- %s    MIN --    MAX --",
                name,
                unit);
        }

        lv_label_set_text(chart->stats_label, buf);
    }

    if (chart->target_label) {
        char buf[48];

        if (has_target && isfinite(target) && target > 0.0) {
            snprintf(
                buf,
                sizeof(buf),
                "TARGET %.1f %s",
                target,
                unit);
        } else {
            buf[0] = '\0';
        }

        lv_label_set_text(chart->target_label, buf);
    }
}

static void telemetry_chart_apply_axis(
    telemetry_chart_t *chart,
    const telemetry_range_stats_t *stats,
    bool follow_reference,
    double preferred_center)
{
    if (!chart || !chart->chart) {
        return;
    }

    if (!stats || !stats->valid) {
        if (!chart->axis_locked) {
            chart->axis_min =
                (int32_t)lround(chart->fallback_min_c * 10.0);

            chart->axis_max =
                (int32_t)lround(chart->fallback_max_c * 10.0);

            chart->axis_edge_samples = 0;

            lv_chart_set_range(
                chart->chart,
                chart->axis,
                chart->axis_min,
                chart->axis_max);
        }

        return;
    }

    double requested_center = preferred_center;

    if (!isfinite(requested_center)) {
        requested_center = stats->current;
    }

    bool reference_changed = false;

    if (chart->axis_locked && follow_reference) {
        double reference_threshold =
            chart->minimum_span_c * 0.5;

        if (reference_threshold < 0.5) {
            reference_threshold = 0.5;
        }

        reference_changed =
            fabs(requested_center - chart->axis_anchor) >=
            reference_threshold;
    }

    /*
     * Keep the Y-axis stationary while the newest sample remains inside
     * the middle 80 percent of the current range.
     *
     * The outer 10 percent at each edge is the hysteresis zone.
     */
    if (chart->axis_locked && !reference_changed) {
        const double low =
            (double)chart->axis_min / 10.0;

        const double high =
            (double)chart->axis_max / 10.0;

        const double span = high - low;
        const double inner_low = low + span * 0.10;
        const double inner_high = high - span * 0.10;

        if (stats->current >= inner_low &&
            stats->current <= inner_high) {
            chart->axis_edge_samples = 0;
            return;
        }

        if (chart->axis_edge_samples < UINT8_MAX) {
            chart->axis_edge_samples++;
        }

        /*
         * Ignore brief spikes. Recenter only after four consecutive
         * samples remain near or beyond an edge.
         */
        if (chart->axis_edge_samples < 4) {
            return;
        }
    }

    chart->axis_edge_samples = 0;

    /*
     * Center on the newest actual value when adapting. This allows a
     * heating or cooling trace to move through several stable windows
     * instead of jumping directly to the final target range.
     */
    double center = stats->current;

    if (!isfinite(center)) {
        center = requested_center;
    }

    double half_span = chart->minimum_span_c * 0.5;
    double low = center - half_span;
    double high = center + half_span;

    /*
     * Keep the new range on clean half-unit boundaries.
     */
    low = floor(low * 2.0) / 2.0;
    high = ceil(high * 2.0) / 2.0;

    if (low < 0.0) {
        high -= low;
        low = 0.0;
    }

    if (high > 320.0) {
        const double excess = high - 320.0;

        low -= excess;
        high = 320.0;

        if (low < 0.0) {
            low = 0.0;
        }
    }

    if (high - low < chart->minimum_span_c) {
        high = low + chart->minimum_span_c;
    }

    chart->axis_min = (int32_t)lround(low * 10.0);
    chart->axis_max = (int32_t)lround(high * 10.0);

    /*
     * Preserve the commanded target as the reference-change anchor for
     * heater graphs. Environment graphs use the actual center.
     */
    chart->axis_anchor =
        follow_reference
            ? requested_center
            : center;

    chart->axis_locked = true;

    lv_chart_set_range(
        chart->chart,
        chart->axis,
        chart->axis_min,
        chart->axis_max);
}

static void telemetry_collect_recent_stats(
    telemetry_range_stats_t *nozzle,
    telemetry_range_stats_t *bed,
    telemetry_range_stats_t *chamber,
    telemetry_range_stats_t *humidity,
    double *latest_nozzle_target,
    double *latest_bed_target)
{
    if (!nozzle || !bed || !chamber || !humidity) {
        return;
    }

    size_t count = telemetry_history_count();

    /*
     * Four-minute adaptive window at the current two-second sample rate.
     * All retained history remains plotted; only the scale uses this window.
     */
    size_t window = 120;
    size_t start = count > window ? count - window : 0;

    for (size_t i = start; i < count; i++) {
        telemetry_sample_t sample;

        if (!telemetry_history_get(i, &sample)) {
            continue;
        }

        telemetry_range_add(nozzle, sample.nozzle_temp);
        telemetry_range_add(bed, sample.bed_temp);
        telemetry_range_add(chamber, sample.air_temp);
        telemetry_range_add(humidity, sample.humidity);

        if (latest_nozzle_target) {
            *latest_nozzle_target = sample.nozzle_target;
        }

        if (latest_bed_target) {
            *latest_bed_target = sample.bed_target;
        }
    }
}

static void telemetry_update_chart_ranges_and_stats(void)
{
    telemetry_range_stats_t nozzle = {0};
    telemetry_range_stats_t bed = {0};
    telemetry_range_stats_t chamber = {0};
    telemetry_range_stats_t humidity = {0};

    double nozzle_target = 0.0;
    double bed_target = 0.0;

    telemetry_collect_recent_stats(
        &nozzle,
        &bed,
        &chamber,
        &humidity,
        &nozzle_target,
        &bed_target);

    double nozzle_center =
        nozzle_target > 0.0
            ? nozzle_target
            : nozzle.current;

    double bed_center =
        bed_target > 0.0
            ? bed_target
            : bed.current;

    telemetry_chart_apply_axis(
        &s_nozzle_chart,
        &nozzle,
        true,
        nozzle_center);

    telemetry_chart_apply_axis(
        &s_bed_chart,
        &bed,
        true,
        bed_center);

    telemetry_chart_apply_axis(
        &s_chamber_chart,
        &chamber,
        false,
        chamber.current);

    telemetry_chart_apply_axis(
        &s_humidity_chart,
        &humidity,
        false,
        humidity.current);

    telemetry_update_chart_stats(
        &s_nozzle_chart,
        "NOZZLE",
        "C",
        &nozzle,
        true,
        nozzle_target);

    telemetry_update_chart_stats(
        &s_bed_chart,
        "BED",
        "C",
        &bed,
        true,
        bed_target);

    telemetry_update_chart_stats(
        &s_chamber_chart,
        "CHAMBER",
        "C",
        &chamber,
        false,
        0.0);

    telemetry_update_chart_stats(
        &s_humidity_chart,
        "HUMIDITY",
        "%RH",
        &humidity,
        false,
        0.0);

    telemetry_position_target_line(
        &s_nozzle_chart,
        nozzle_target);

    telemetry_position_target_line(
        &s_bed_chart,
        bed_target);

    telemetry_position_newest_dot(
        &s_nozzle_chart,
        &nozzle);

    telemetry_position_newest_dot(
        &s_bed_chart,
        &bed);

    telemetry_position_newest_dot(
        &s_chamber_chart,
        &chamber);

    telemetry_position_newest_dot(
        &s_humidity_chart,
        &humidity);
}

static void telemetry_chart_push_sample(
    telemetry_chart_t *chart,
    double value)
{
    if (!chart || !chart->chart || !chart->actual_series) {
        return;
    }

    lv_chart_set_next_value(
        chart->chart,
        chart->actual_series,
        telemetry_chart_value(value));
}

static void telemetry_create_single_chart(
    lv_obj_t *parent,
    telemetry_chart_t *out,
    int x,
    int y,
    int width,
    const char *title,
    lv_color_t actual_color,
    lv_color_t target_color,
    double minimum_span_c,
    double fallback_min_c,
    double fallback_max_c)
{
    if (!out) {
        return;
    }

    lv_obj_t *card = lv_obj_create(parent);

    lv_obj_set_size(card, width, 108);
    lv_obj_set_pos(card, x, y);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    ui_apply_surface_role(card, UI_SURFACE_TELEMETRY_CHART);

    out->minimum_span_c = minimum_span_c;
    out->fallback_min_c = fallback_min_c;
    out->fallback_max_c = fallback_max_c;

    out->axis_locked = false;
    out->axis_anchor = NAN;
    out->axis_edge_samples = 0;

    out->actual_color = actual_color;
    out->target_color = target_color;
    out->axis = LV_CHART_AXIS_PRIMARY_Y;

    out->stats_label = telemetry_make_label(
        card,
        title,
        &lv_font_montserrat_12,
        actual_color);

    lv_obj_set_pos(out->stats_label, 12, 8);

    out->target_label = telemetry_make_label(
        card,
        "TARGET -- C",
        &lv_font_montserrat_12,
        target_color);

    lv_obj_align(
        out->target_label,
        LV_ALIGN_TOP_RIGHT,
        -12,
        8);

    out->chart = lv_chart_create(card);

    /*
     * Telemetry chart parent is a fixed instrument card.
     * Explicitly disable LVGL scrolling and scrollbar rendering.
     */
    lv_obj_t *instrument_card =
        lv_obj_get_parent(out->chart);

    if (instrument_card) {
        lv_obj_clear_flag(
            instrument_card,
            LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_set_scrollbar_mode(
            instrument_card,
            LV_SCROLLBAR_MODE_OFF);

        lv_obj_scroll_to(
            instrument_card,
            0,
            0,
            LV_ANIM_OFF);
    }


    /*
     * Leave a lower instrumentation strip for the time direction.
     */
    lv_obj_set_size(out->chart, width - 24, 54);
    lv_obj_set_pos(out->chart, 12, 32);

    ui_apply_telemetry_plot_style(out->chart);

    lv_chart_set_type(
        out->chart,
        LV_CHART_TYPE_LINE);

    lv_chart_set_update_mode(
        out->chart,
        LV_CHART_UPDATE_MODE_SHIFT);

    lv_chart_set_point_count(
        out->chart,
        TELEMETRY_HISTORY_CAPACITY);

    /*
     * Ten vertical divisions across ten minutes:
     * approximately one minute per division.
     */
    lv_chart_set_div_line_count(
        out->chart,
        4,
        10);

    out->axis_min = (int32_t)lround(fallback_min_c * 10.0);
    out->axis_max = (int32_t)lround(fallback_max_c * 10.0);

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
     * Bright marker identifying the newest live sample.
     */
    out->newest_dot = lv_obj_create(out->chart);

    lv_obj_set_size(out->newest_dot, 8, 8);
    lv_obj_set_pos(out->newest_dot, 0, 0);

    lv_obj_clear_flag(
        out->newest_dot,
        LV_OBJ_FLAG_SCROLLABLE);

    ui_apply_trace_marker_style(out->newest_dot, actual_color);

    lv_obj_add_flag(
        out->newest_dot,
        LV_OBJ_FLAG_HIDDEN);

    out->oldest_time_label = telemetry_make_label(
        card,
        "-10 MIN",
        &lv_font_montserrat_12,
        UI_TEXT_DIM);

    lv_obj_set_pos(
        out->oldest_time_label,
        12,
        89);

    out->newest_time_label = telemetry_make_label(
        card,
        "NOW",
        &lv_font_montserrat_12,
        actual_color);

    lv_obj_align(
        out->newest_time_label,
        LV_ALIGN_TOP_RIGHT,
        -12,
        89);

    /*
     * Target is drawn as a crisp reference line instead of another
     * scrolling series, so it cannot disappear beneath the actual trace.
     */
    out->target_line = lv_obj_create(out->chart);

    lv_obj_set_size(
        out->target_line,
        lv_pct(100),
        2);

    lv_obj_set_pos(out->target_line, 0, 0);
    lv_obj_clear_flag(
        out->target_line,
        LV_OBJ_FLAG_SCROLLABLE);

    ui_apply_reference_line_style(out->target_line, target_color);

    lv_obj_add_flag(
        out->target_line,
        LV_OBJ_FLAG_HIDDEN);

    /*
     * The current sample marker must remain above the target reference.
     */
    lv_obj_move_foreground(out->newest_dot);
}

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

    ui_apply_trace_marker_style(out->newest_dot, actual_color);

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

    ui_apply_reference_line_style(out->target_line, target_color);

    lv_obj_add_flag(
        out->target_line,
        LV_OBJ_FLAG_HIDDEN);

    lv_obj_move_foreground(
        out->newest_dot);
}

static void telemetry_chart_load_history(void)
{
    if (!s_nozzle_chart.chart ||
        !s_bed_chart.chart ||
        !s_chamber_chart.chart ||
        !s_humidity_chart.chart) {
        return;
    }

    lv_chart_set_all_value(
        s_nozzle_chart.chart,
        s_nozzle_chart.actual_series,
        LV_CHART_POINT_NONE);

    lv_chart_set_all_value(
        s_bed_chart.chart,
        s_bed_chart.actual_series,
        LV_CHART_POINT_NONE);

    lv_chart_set_all_value(
        s_chamber_chart.chart,
        s_chamber_chart.actual_series,
        LV_CHART_POINT_NONE);

    lv_chart_set_all_value(
        s_humidity_chart.chart,
        s_humidity_chart.actual_series,
        LV_CHART_POINT_NONE);

    size_t count = telemetry_history_count();

    for (size_t i = 0; i < count; i++) {
        telemetry_sample_t sample;

        if (!telemetry_history_get(i, &sample)) {
            continue;
        }

        telemetry_chart_push_sample(
            &s_nozzle_chart,
            sample.nozzle_temp);

        telemetry_chart_push_sample(
            &s_bed_chart,
            sample.bed_temp);

        telemetry_chart_push_sample(
            &s_chamber_chart,
            sample.air_temp);

        telemetry_chart_push_sample(
            &s_humidity_chart,
            sample.humidity);
    }

    telemetry_update_chart_ranges_and_stats();

    /*
     * Nozzle/Bed and Chamber/Humidity each share one LVGL chart.
     */
    lv_chart_refresh(s_nozzle_chart.chart);
    lv_chart_refresh(s_chamber_chart.chart);
}

static void telemetry_create_chart_panel(lv_obj_t *parent)
{
    lv_obj_t *panel = lv_obj_create(parent);

    lv_obj_set_size(panel, UI_PAGE_RAIL_WIDTH, 300);
    lv_obj_set_pos(panel, UI_PAGE_RAIL_X, 208);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    ui_apply_surface_role(panel, UI_SURFACE_TELEMETRY_PANEL);

    lv_obj_t *title = telemetry_make_label(
        panel,
        "LIVE HISTORY",
        &lv_font_montserrat_16,
        UI_TEXT_BRIGHT);

    lv_obj_set_pos(title, 18, 12);

    lv_obj_t *range = telemetry_make_label(
        panel,
        "10 MINUTES  /  0.1 UNIT RESOLUTION",
        &lv_font_montserrat_12,
        UI_TEXT_DIM);

    lv_obj_align(range, LV_ALIGN_TOP_RIGHT, -18, 15);

    telemetry_create_legend_item(
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
        "Target reference");

    /*
     * Two combined chart surfaces.
     *
     * Each instrument retains its own series, adaptive range, target,
     * statistics and newest-sample marker.
     */
    telemetry_create_single_chart(
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
        300.0);

    telemetry_create_single_chart(
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
        80.0);

    telemetry_create_overlay_series(
        &s_nozzle_chart,
        &s_bed_chart,
        "BED  -- C    MIN --    MAX --",
        UI_WARN,
        UI_TELEMETRY_BED_TRACE,
        LV_CHART_AXIS_SECONDARY_Y,
        2.0,
        0.0,
        130.0);

    telemetry_create_overlay_series(
        &s_chamber_chart,
        &s_humidity_chart,
        "HUMIDITY  -- %RH    MIN --    MAX --",
        UI_TELEMETRY_HUMIDITY,
        UI_TELEMETRY_HUMIDITY,
        LV_CHART_AXIS_SECONDARY_Y,
        4.0,
        0.0,
        100.0);

    telemetry_chart_load_history();
}

void ui_telemetry_charts_create(lv_obj_t *parent)
{
    telemetry_create_chart_panel(parent);
}

void ui_telemetry_charts_load_history(void)
{
    telemetry_chart_load_history();
}

void ui_telemetry_charts_push_sample(
    double nozzle_temp,
    double bed_temp,
    double chamber_temp,
    double humidity)
{
    telemetry_chart_push_sample(
        &s_nozzle_chart,
        nozzle_temp);

    telemetry_chart_push_sample(
        &s_bed_chart,
        bed_temp);

    telemetry_chart_push_sample(
        &s_chamber_chart,
        chamber_temp);

    telemetry_chart_push_sample(
        &s_humidity_chart,
        humidity);

    telemetry_update_chart_ranges_and_stats();

    if (s_nozzle_chart.chart) {
        lv_chart_refresh(s_nozzle_chart.chart);
    }

    if (s_chamber_chart.chart) {
        lv_chart_refresh(s_chamber_chart.chart);
    }
}

void ui_telemetry_charts_reset(void)
{
    s_nozzle_chart = (telemetry_chart_t){0};
    s_bed_chart = (telemetry_chart_t){0};
    s_chamber_chart = (telemetry_chart_t){0};
    s_humidity_chart = (telemetry_chart_t){0};
}
