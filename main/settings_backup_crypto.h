#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Portable passphrase-encrypted configuration backup format. */
#define SETTINGS_BACKUP_CRYPTO_FORMAT_VERSION 1
#define SETTINGS_BACKUP_CRYPTO_MIN_PASSPHRASE_LENGTH 12

/* Returned buffers are heap-owned; release with settings_backup_crypto_free(). */
bool settings_backup_crypto_encrypt(
    const char *passphrase,
    const uint8_t *plaintext,
    size_t plaintext_size,
    uint8_t **encrypted,
    size_t *encrypted_size,
    char *status,
    size_t status_size);

bool settings_backup_crypto_decrypt(
    const char *passphrase,
    const uint8_t *encrypted,
    size_t encrypted_size,
    uint8_t **plaintext,
    size_t *plaintext_size,
    char *status,
    size_t status_size);

void settings_backup_crypto_free(void *buffer, size_t size);
