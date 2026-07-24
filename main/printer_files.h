#pragma once

#include <stddef.h>
#include <stdbool.h>

#define PRINTER_FILES_MAX_PATH 160

typedef struct {
    char path[PRINTER_FILES_MAX_PATH];
    double size;
    double modified;
} printer_file_entry_t;

void printer_files_seconds_to_hhmm(double sec, char *out, size_t out_sz);

bool printer_files_build_metadata_text(const char *file,
                                       const char *metadata_json,
                                       char *thumbnail_path,
                                       size_t thumbnail_path_sz,
                                       char *out,
                                       size_t out_sz);

int printer_files_for_each_path(const char *body,
                                void (*cb)(const char *path, void *user),
                                void *user);
int printer_files_parse_entries(const char *body,
                                printer_file_entry_t *entries,
                                size_t capacity);
