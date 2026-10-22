#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define THEME_BACKGROUND_WIDTH 1024
#define THEME_BACKGROUND_HEIGHT 600
#define THEME_BACKGROUND_BYTES (1024u * 600u * 2u)
#define THEME_BACKGROUND_HEADER_BYTES 20u
bool theme_background_asset_name_valid(const char *name);
bool theme_background_asset_header_valid(const char *path);
bool theme_background_asset_read(const char *path, uint8_t *pixels, size_t capacity);
