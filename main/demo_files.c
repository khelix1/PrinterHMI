#include "demo_files.h"
#include "demo_preview_assets.inc"
#include <stdio.h>
#include <string.h>
static const demo_file_t library[] = {
    {"Spool holder.gcode", 1.4, 30, false, 0},
    {"Tool tray.gcode", 2.2, 30, true, 1},
    {"Cable organizer.gcode", .8, 30, false, 1},
    {"Calibration cube.gcode", .2, 30, false, 2},
    {"Planter.gcode", 3.1, 30, true, 0},
    {"Bracket.gcode", 1.1, 30, false, 1},
    {"Calibration/Calibration cube.gcode", .2, 30, false, 2}};
size_t demo_files_count(void) { return sizeof(library) / sizeof(library[0]); }
const demo_file_t *demo_files_at(size_t index) {
  return index < demo_files_count() ? &library[index] : NULL;
}
const demo_file_t *demo_files_find(const char *path) {
  if (path)
    for (size_t i = 0; i < demo_files_count(); i++)
      if (!strcmp(path, library[i].path))
        return &library[i];
  return NULL;
}
/* Strict URL decoding, including output-size checks. Unknown paths never fall
 * through to a real endpoint. '+' is accepted as a query-space encoding. */
static int hex(char c) {
  return c >= '0' && c <= '9'   ? c - '0'
         : c >= 'A' && c <= 'F' ? c - 'A' + 10
         : c >= 'a' && c <= 'f' ? c - 'a' + 10
                                : -1;
}
static bool decode(const char *input, char *out, size_t size) {
  if (!input || !out || !size)
    return false;
  size_t used = 0;
  for (; *input; input++) {
    unsigned char c = (unsigned char)*input;
    if (c == '%') {
      if (!input[1] || !input[2])
        return false;
      int a = hex(input[1]), b = hex(input[2]);
      if (a < 0 || b < 0)
        return false;
      c = (unsigned char)(a * 16 + b);
      input += 2;
    } else if (c == '+')
      c = ' ';
    if (c < 32 || c == 127 || used + 1 >= size)
      return false;
    out[used++] = (char)c;
  }
  out[used] = 0;
  return used > 0;
}
const uint16_t *demo_files_pixels(const char *path, int *width, int *height) {
  const demo_file_t *file = demo_files_find(path);
  if (!file || !width || !height)
    return NULL;
  *width = 286;
  *height = 165;
  return file->artwork == 0   ? preview_0_rgb
         : file->artwork == 1 ? preview_1_rgb
                              : preview_2_rgb;
}
const uint8_t *demo_files_png(const char *encoded, size_t *size) {
  if (size)
    *size = 0;
  if (!size)
    return NULL;
  char path[192];
  if (!decode(encoded, path, sizeof(path)))
    return NULL;
  const uint8_t *images[] = {preview_0_png, preview_1_png, preview_2_png};
  const size_t sizes[] = {sizeof(preview_0_png), sizeof(preview_1_png),
                          sizeof(preview_2_png)};
  for (unsigned i = 0; i < 3; i++) {
    char name[64];
    snprintf(name, sizeof(name), ".demo/previews/%u.png", i);
    if (!strcmp(path, name)) {
      *size = sizes[i];
      return images[i];
    }
  }
  return NULL;
}
bool demo_files_metadata(const char *encoded, char *out, size_t size) {
  if (out && size)
    out[0] = 0;
  if (!out || !size)
    return false;
  char path[192];
  if (!decode(encoded, path, sizeof(path)))
    return false;
  const demo_file_t *file = demo_files_find(path);
  if (!file)
    return false;
  int n = snprintf(
      out, size,
      "{\"result\":{\"filename\":\"%s\",\"size\":%.0f,\"modified\":1793448000,"
      "\"slicer\":\"PrinterHMI "
      "demo\",\"estimated_time\":1800,\"filament_total\":5200,\"layer_height\":"
      "0.2,\"object_height\":20,\"thumbnails\":[{\"width\":900,\"height\":520,"
      "\"relative_path\":\".demo/previews/%u.png\"}]}}",
      file->path, file->mib * 1024 * 1024, file->artwork);
  if (n < 0 || (size_t)n >= size) {
    out[0] = 0;
    return false;
  }
  return true;
}
bool demo_files_metadata_text(const char *path, char *out, size_t size) {
  if (out && size)
    out[0] = 0;
  const demo_file_t *file = demo_files_find(path);
  if (!file || !out || !size)
    return false;
  int n = snprintf(
      out, size,
      "Layer height: 0.20 mm\nObject height: 20.0 mm\nFilament: PLA\nEstimated "
      "time: %d min\nSize: %.1f MB%s",
      file->minutes, file->mib,
      file->extras
          ? "\nSlicer: PrinterHMI demo\nPerimeters: 3\nInfill: 20% "
            "gyroid\nNozzle diameter: 0.4 mm\nFilament diameter: 1.75 mm\nBed "
            "temperature: 60 C\nNozzle temperature: 210 C\nSupport material: "
            "off\nFirst layer height: 0.24 mm\nNotes: Local sample metadata. "
            "Images are demonstration artwork; no file is downloaded."
          : "");
  if (n < 0 || (size_t)n >= size) {
    out[0] = 0;
    return false;
  }
  return true;
}
