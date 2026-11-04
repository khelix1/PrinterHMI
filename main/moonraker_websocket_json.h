#pragma once

#include "json_document_parse.h"

#define MOONRAKER_WS_MESSAGE_CAPACITY (128U * 1024U)
#define MOONRAKER_WS_JSON_MAX_DEPTH 16U

/* Keep one byte free for the receive buffer's terminating NUL. */
static inline cJSON *moonraker_websocket_parse_object(const char *json, size_t length)
{
    cJSON *root = json_document_parse_bounded(json, length,
        MOONRAKER_WS_MESSAGE_CAPACITY - 1, MOONRAKER_WS_JSON_MAX_DEPTH);
    if (cJSON_IsObject(root)) return root;
    cJSON_Delete(root);
    return NULL;
}
