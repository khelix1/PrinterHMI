#include "theme_background_asset.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static uint32_t little32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static bool valid_header(const uint8_t *h)
{
    return memcmp(h, "PHMIBG01", 8) == 0 && h[8] == 0 && h[9] == 4 &&
        h[10] == 88 && h[11] == 2 && little32(h + 12) == THEME_BACKGROUND_BYTES;
}
bool theme_background_asset_name_valid(const char *name)
{
    if (!name || strlen(name) != 74 || strncmp(name, "bg-", 3) || strcmp(name + 67, ".phmibg")) return false;
    for (unsigned i = 3; i < 67; ++i) if (!((name[i] >= '0' && name[i] <= '9') || (name[i] >= 'a' && name[i] <= 'f'))) return false;
    return true;
}
bool theme_background_asset_header_valid(const char *path)
{
    struct stat st;
    if (!path || stat(path, &st) || !S_ISREG(st.st_mode) || st.st_size != THEME_BACKGROUND_HEADER_BYTES + THEME_BACKGROUND_BYTES) return false;
    FILE *f = fopen(path, "rb"); if (!f) return false;
    uint8_t header[THEME_BACKGROUND_HEADER_BYTES];
    bool ok = fread(header, 1, sizeof(header), f) == sizeof(header) && valid_header(header) && !ferror(f);
    if (fclose(f)) ok = false;
    return ok;
}
bool theme_background_asset_read(const char *path, uint8_t *pixels, size_t capacity)
{
    if (!pixels || capacity < THEME_BACKGROUND_BYTES || !theme_background_asset_header_valid(path)) return false;
    FILE *f = fopen(path, "rb"); if (!f) return false;
    uint8_t header[THEME_BACKGROUND_HEADER_BYTES];
    bool ok = fread(header, 1, sizeof(header), f) == sizeof(header) && valid_header(header);
    if (ok) ok = fread(pixels, 1, THEME_BACKGROUND_BYTES, f) == THEME_BACKGROUND_BYTES && fgetc(f) == EOF && !ferror(f);
    if (fclose(f)) ok = false;
    if (!ok) return false;
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < THEME_BACKGROUND_BYTES; ++i) {
        crc ^= pixels[i];
        for (unsigned bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return (crc ^ UINT32_MAX) == little32(header + 16);
}
