#pragma once

#include <stddef.h>
#include <string.h>
#include "cJSON.h"

/* Volatile stores keep cleanup of temporary credential copies observable. */
static inline void backup_sensitive_clear(void *buffer, size_t size)
{
    volatile unsigned char *bytes = buffer;
    if (bytes) while (size--) *bytes++ = 0;
}

static inline void backup_sensitive_json_clear(cJSON *node)
{
    for (; node; node = node->next) {
        if (!(node->type & cJSON_IsReference)) {
            backup_sensitive_json_clear(node->child);
            if (node->valuestring) backup_sensitive_clear(node->valuestring, strlen(node->valuestring));
        }
        if (node->string && !(node->type & cJSON_StringIsConst))
            backup_sensitive_clear(node->string, strlen(node->string));
    }
}

static inline void backup_sensitive_json_delete(cJSON *root)
{
    backup_sensitive_json_clear(root);
    cJSON_Delete(root);
}

static inline void backup_sensitive_json_free(char *document)
{
    if (document) backup_sensitive_clear(document, strlen(document));
    cJSON_free(document);
}
