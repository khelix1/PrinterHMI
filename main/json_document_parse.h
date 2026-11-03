#pragma once

#include "cJSON.h"
#include "json_document_limits.h"

/* Parse exactly one bounded JSON document; no trailing NUL is required. */
static inline cJSON *json_document_parse_bounded(
    const char *document, size_t size, size_t max_size, unsigned max_depth)
{
    if (!json_document_within_limits(document, size, max_size, max_depth)) return NULL;
    const char *end = NULL;
    cJSON *root = cJSON_ParseWithLengthOpts(document, size, &end, false);
    if (!root) return NULL;
    while (end < document + size &&
        (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) ++end;
    if (end != document + size) {
        cJSON_Delete(root);
        return NULL;
    }
    return root;
}
