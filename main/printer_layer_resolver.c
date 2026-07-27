#include "printer_layer_resolver.h"

#include <math.h>

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
        object_height > 0.0 &&
        layer_height > 0.0) {
        result.total = (int)floor(
            (object_height / layer_height) + 0.001);
    }

    if (result.current <= 0 &&
        result.total > 0 &&
        layer_height > 0.0 &&
        current_z > 0.0) {
        result.current = (int)floor(
            (current_z / layer_height) + 0.001);
    }

    if (result.current <= 0 &&
        result.total > 0 &&
        progress >= 0.0) {
        result.current =
            (int)floor(progress * result.total) + 1;
    }

    if (result.current < 1 && result.total > 0) {
        result.current = 1;
    }

    if (result.total > 0 &&
        result.current > result.total) {
        result.current = result.total;
    }

    result.valid =
        result.current > 0 &&
        result.total > 0;

    return result;
}
