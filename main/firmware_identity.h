#pragma once
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    FIRMWARE_CHANNEL_CUSTOM = 0,
    FIRMWARE_CHANNEL_STABLE = 1,
    FIRMWARE_CHANNEL_NIGHTLY = 2,
    FIRMWARE_CHANNEL_DEVELOPMENT = 3,
} firmware_channel_t;

typedef struct {
    firmware_channel_t channel;
    char version[40];
    char revision[16];
    bool modified;
} firmware_identity_t;

void firmware_identity_get(firmware_identity_t *identity);
const char *firmware_identity_channel_name(firmware_channel_t channel);
void firmware_identity_format(char *out, size_t capacity);
/* Order releases only within a channel. Cross-channel versions are incomparable. */
int firmware_identity_compare(const firmware_identity_t *installed,
                             firmware_channel_t candidate_channel,
                             const char *candidate_version);
#define FIRMWARE_RELATION_OTHER_BUILD 2
#define FIRMWARE_RELATION_SWITCH_CHANNEL 3
