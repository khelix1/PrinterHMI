#include "print_estimates.h"
#include <limits.h>
#include <math.h>

static bool fraction_valid(double value)
{
    return isfinite(value) && value >= 0 && value <= 1;
}

static double progress_remaining(double fraction, double elapsed)
{
    return fraction > 0 && fraction < 1 ? elapsed * (1 - fraction) / fraction : -1;
}

static double combine(unsigned mask, const double values[3])
{
    double sum = 0;
    unsigned count = 0;
    for (unsigned i = 0; i < 3; ++i) {
        if ((mask & (1u << i)) && isfinite(values[i]) && values[i] > 0 && values[i] <= INT_MAX) {
            sum += round(values[i]);
            ++count;
        }
    }
    return count && sum > 0 ? sum / count : -1;
}

print_estimate_result_t print_estimate_calculate(print_estimate_options_t options,
    const print_estimate_sample_t *sample, const print_estimate_metadata_t *metadata,
    double elapsed)
{
    print_estimate_result_t result = {-1, -1, -1};
    if (!sample) return result;
    double absolute = sample->file_valid && fraction_valid(sample->file_progress) ? sample->file_progress : -1;
    double relative = absolute;
    double filament = -1;
    if (metadata) {
        if (sample->position_valid && isfinite(sample->file_position) && sample->file_position >= 0 &&
            isfinite(metadata->start_byte) && isfinite(metadata->end_byte) &&
            metadata->start_byte >= 0 && metadata->end_byte > metadata->start_byte) {
            relative = fmin(1, fmax(0, (sample->file_position - metadata->start_byte) /
                (metadata->end_byte - metadata->start_byte)));
        }
        if (sample->filament_valid && isfinite(sample->filament_used) && sample->filament_used >= 0 &&
            isfinite(metadata->filament_total) && metadata->filament_total > 0) {
            filament = fmin(1, sample->filament_used / metadata->filament_total);
        }
    }
    switch (options.progress) {
    case PRINT_PROGRESS_FILE_ABSOLUTE: result.progress = absolute; break;
    case PRINT_PROGRESS_SLICER:
        if (sample->slicer_valid && fraction_valid(sample->slicer_progress))
            result.progress = sample->slicer_progress;
        break;
    case PRINT_PROGRESS_FILAMENT: result.progress = filament; break;
    default: result.progress = relative; break;
    }

    if (!isfinite(elapsed) || elapsed <= 0 || elapsed > INT_MAX) return result;
    /* Mainsail's File estimate extrapolates from the selected progress mode. */
    double values[] = {
        progress_remaining(result.progress, elapsed),
        progress_remaining(filament, elapsed),
        metadata ? metadata->estimated_time - elapsed : -1,
    };
    result.remaining = combine(options.remaining_sources, values);
    result.eta_remaining = combine(options.eta_sources, values);
    return result;
}
