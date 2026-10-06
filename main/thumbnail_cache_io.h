#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define THUMBNAIL_CACHE_MAX_PNG_BYTES (512U * 1024U)
#define THUMBNAIL_CACHE_MAX_FILES 64U
#define THUMBNAIL_CACHE_MAX_BYTES (16U * 1024U * 1024U)

/* Caller serializes cache I/O. Only file-preview directories are pruned. */
bool thumbnail_cache_write(const char *path, const uint8_t *data, size_t size);
