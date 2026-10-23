#pragma once
#include <stdlib.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define MALLOC_CAP_INTERNAL 4
#define heap_caps_malloc(n,c) malloc(n)
#define heap_caps_calloc(n,s,c) calloc(n,s)
#define heap_caps_free(p) free(p)

#define heap_caps_check_integrity(c,p) true

static inline size_t heap_caps_get_free_size(unsigned caps){return caps==MALLOC_CAP_SPIRAM?28U*1024*1024:280U*1024;}
static inline size_t heap_caps_get_minimum_free_size(unsigned caps){return caps==MALLOC_CAP_SPIRAM?26U*1024*1024:240U*1024;}
static inline size_t heap_caps_get_largest_free_block(unsigned caps){return caps==MALLOC_CAP_SPIRAM?24U*1024*1024:220U*1024;}
