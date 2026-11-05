#pragma once

#include "print_estimates.h"

/* One background request at a time. Failures are retried while the job lives. */
void print_estimate_metadata_poll(const char *file, uint32_t job_revision, uint32_t generation, int64_t now_us);
bool print_estimate_metadata_get(const char *file, uint32_t job_revision,
    print_estimate_metadata_t *out);
