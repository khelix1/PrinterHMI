#pragma once

#include <stdbool.h>
#include <stddef.h>

/* Validates one JPEG frame without changing the live Camera-page stream. */
bool camera_test_start(const char *url);
bool camera_test_busy(void);
bool camera_test_take_result(bool *ok, int *width, int *height, size_t *bytes);
