#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

static inline bool thumbnail_path_is_relative(const char *path)
{
    if (!path || !*path) return false;
    const char *part = path;
    for (const char *p = path;; ++p) {
        unsigned char c = (unsigned char)*p;
        if (!c || c == '/') {
            size_t n = (size_t)(p - part);
            if (!n || (part[0] == '.' &&
                (n == 1 || (n == 2 && part[1] == '.')))) return false;
            if (!c) return true;
            part = p + 1;
        } else if (c < 32 || c == 127) return false;
    }
}

/* Moonraker thumbnail paths are relative to the G-code parent. */
static inline size_t thumbnail_path_prefix_length(const char *file)
{
    const char *slash = strrchr(file, '/');
    return slash ? (size_t)(slash - file) + 1 : 0;
}

static inline bool thumbnail_path_fits(const char *file, const char *relative_path, size_t size)
{
    if (!thumbnail_path_is_relative(file) || !thumbnail_path_is_relative(relative_path))
        return false;
    size_t prefix = thumbnail_path_prefix_length(file);
    return prefix < size && strlen(relative_path) < size - prefix;
}

/* Resolve before URL encoding; output may alias relative_path. */
static inline bool thumbnail_path_resolve(const char *file,
    const char *relative_path, char *output, size_t size)
{
    if (!output || !size) return false;
    if (!thumbnail_path_fits(file, relative_path, size)) {
        output[0] = 0;
        return false;
    }
    size_t prefix = thumbnail_path_prefix_length(file);
    memmove(output + prefix, relative_path, strlen(relative_path) + 1);
    if (prefix) memcpy(output, file, prefix);
    return true;
}
