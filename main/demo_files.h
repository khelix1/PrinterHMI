#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct {
  const char *path;
  double mib;
  int minutes;
  bool extras;
  unsigned artwork;
} demo_file_t;
size_t demo_files_count(void);
const demo_file_t *demo_files_at(size_t index);
const demo_file_t *demo_files_find(const char *path);
const uint16_t *demo_files_pixels(const char *path, int *width, int *height);
/* Immutable originals; the Moonraker adapter makes an owned copy for callers.
 */
const uint8_t *demo_files_png(const char *encoded_thumbnail, size_t *size);
bool demo_files_metadata(const char *encoded_filename, char *out, size_t size);
bool demo_files_metadata_text(const char *path, char *out, size_t size);
