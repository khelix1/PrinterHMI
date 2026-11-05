#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* An SSID is at most 32 bytes, independent of the record's spare byte. */
static inline bool wifi_scan_copy_ssid(char out[33], const uint8_t *ssid)
{
    size_t len = 0;
    while (len < 32 && ssid[len]) {
        out[len] = (char)ssid[len];
        len++;
    }
    out[len] = '\0';
    return len != 0;
}

static inline void wifi_scan_format_page_status(char *out, size_t size,
                                                 unsigned count, unsigned total)
{
    if (!count) snprintf(out, size, "No networks found");
    else if (total > count) snprintf(out, size, "%u found - showing %u", total, count);
    else snprintf(out, size, "%u network%s found", count, count == 1 ? "" : "s");
}
