#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* Moonraker metadata thumbnail paths are relative to the G-code parent.
 * Resolve once before URL encoding; output may alias relative_path. */
static inline bool thumbnail_path_resolve(const char *file,
    const char *relative_path, char *output, size_t size)
{
    if (!output || !size) return false;
    if (!file || !file[0] || !relative_path || !relative_path[0] ||
        relative_path[0] == '/') goto invalid;
    for (const char *part = relative_path; *part;) {
        const char *end = strchr(part, '/');
        size_t length = end ? (size_t)(end - part) : strlen(part);
        if (length == 2 && part[0] == '.' && part[1] == '.') goto invalid;
        if (!end) break;
        part = end + 1;
    }
    const char *slash = strrchr(file, '/');
    size_t prefix = slash ? (size_t)(slash - file) + 1 : 0;
    size_t length = strlen(relative_path);
    if (prefix >= size || length >= size - prefix) goto invalid;
    memmove(output + prefix, relative_path, length + 1);
    if (prefix) memcpy(output, file, prefix);
    return true;
invalid:
    output[0] = '\0';
    return false;
}
