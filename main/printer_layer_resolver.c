#include "printer_layer_resolver.h"

#include <math.h>
#include <limits.h>

printer_layer_result_t printer_layer_resolver_resolve(
    int reported_current,
    int reported_total,
    double current_z,
    double object_height,
    double layer_height,
    double progress)
{
    printer_layer_result_t result = {
        .current = reported_current,
        .total = reported_total,
        .valid = false,
    };

    if (result.total <= 0 &&
        isfinite(object_height) && isfinite(layer_height) &&
        object_height > 0.0 && layer_height > 0.0 &&
        object_height / layer_height < INT_MAX) {
        result.total = (int)floor(
            (object_height / layer_height) + 0.001);
    }

    /* Some external print starts leave Klipper's current layer at zero while
     * progress advances. Preserve zero at the beginning, but allow estimates
     * once the print has made measurable progress. */
    bool estimate_current = result.current < 0 ||
        (result.current == 0 && isfinite(progress) && progress > 0.0);

    if (estimate_current &&
        result.total > 0 &&
        isfinite(layer_height) && layer_height > 0.0 &&
        isfinite(current_z) && current_z > 0.0 &&
        current_z / layer_height < INT_MAX) {
        int estimated = (int)floor((current_z / layer_height) + 0.001);
        if (estimated > 0) result.current = estimated;
    }

    if (estimate_current && result.current <= 0 &&
        result.total > 0 &&
        isfinite(progress) && progress >= 0.0 && progress <= 1.0) {
        result.current = (int)floor(progress * (result.total - 1)) + 1;
    }

    if (result.current < 0 && result.total > 0) {
        result.current = 1;
    }

    if (result.total > 0 &&
        result.current > result.total) {
        result.current = result.total;
    }

    result.valid =
        result.current >= 0 &&
        result.total > 0;

    return result;
}
