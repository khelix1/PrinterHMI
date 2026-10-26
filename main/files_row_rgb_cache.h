#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define FILES_ROW_RGB_EDGE 96u
#define FILES_ROW_RGB_BUCKETS 256u
/* Zero means the endpoint/file revision is not reliable enough to cache. */
uint64_t files_row_rgb_key(const char *host,int port,const char *file,double size,double modified);
/* One worker serializes I/O. Fixed buckets bound disk use; full key/checksums
 * prevent a bucket collision, changed file or corrupt cache from being reused.
 * Caller owns capacity pixels; failure never produces a publishable image. */
bool files_row_rgb_load(uint64_t key,uint16_t *pixels,size_t capacity,int *width,int *height);
bool files_row_rgb_store(uint64_t key,const uint16_t *pixels,int width,int height);
