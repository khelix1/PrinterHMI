#include "printer_layer_resolver.h"

#include <math.h>
#include <limits.h>

printer_layer_result_t printer_layer_resolver_resolve(
    int reported_current,
    int reported_total,
    double current_z,
    const printer_layer_metadata_t *metadata,
    double progress,
    bool print_started)
{
    printer_layer_result_t result = {
        .current = reported_current,
        .total = reported_total,
        .valid = false,
    };

    double object_height = metadata ? metadata->object_height : 0.0;
    double layer_height = metadata ? metadata->layer_height : 0.0;
    double first = metadata ? metadata->first_layer_height : 0.0;
    if (!isfinite(first) || first <= 0.0) first = layer_height;
    if (result.total <= 0 && metadata && metadata->total_layers > 0)
        result.total = metadata->total_layers;
    if (result.total <= 0 && isfinite(object_height) && object_height > 0.0 &&
        isfinite(layer_height) && layer_height > 0.0 && isfinite(first) && first > 0.0) {
        double count = 1.0 + ceil(fmax(0.0, (object_height - first) / layer_height) - 0.000001);
        if (isfinite(count) && count >= 1.0 && count <= INT_MAX)
            result.total = (int)count;
    }

    /* File bytes and Z motion also advance during START_PRINT. Until actual
     * printing begins, a missing/zero report means layer 0, not probing Z/h.
     * Positive Klipper reports always retain priority. */
    bool estimate_current = print_started && result.current <= 0;
    if (result.current <= 0 && !print_started) result.current = 0;

    if (estimate_current &&
        result.total > 0 &&
        isfinite(layer_height) && layer_height > 0.0 &&
        isfinite(current_z) && current_z > 0.0 &&
        isfinite(first) && first > 0.0) {
        double estimated = 1.0 + floor(fmax(0.0, (current_z - first) / layer_height) + 0.001);
        if (isfinite(estimated) && estimated >= 1.0 && estimated <= INT_MAX)
            result.current = (int)estimated;
    }

    if (estimate_current && result.current <= 0 &&
        result.total > 0 &&
        isfinite(progress) && progress <= 1.0 &&
        (result.current < 0 ? progress >= 0.0 : progress > 0.0)) {
        result.current = (int)floor(progress * (result.total - 1)) + 1;
    }

    if (result.total > 0 &&
        result.current > result.total) {
        if (reported_current > 0 && reported_total <= 0) {
            /* A derived total must not overwrite a reported current layer. */
            result.total = -1;
        } else {
            result.current = result.total;
        }
    }

    result.valid =
        result.current >= 0 &&
        result.total > 0;

    return result;
}

bool printer_layer_resolver_print_started(
    double print_duration, bool filament_valid, double filament_used)
{
    if (!isfinite(print_duration) || print_duration <= 0.0) return false;
    return !filament_valid || (isfinite(filament_used) && filament_used > 0.0);
}
