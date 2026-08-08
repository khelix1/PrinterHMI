#include "settings_backup_crypto.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mbedtls/platform_util.h"
#include "psa/crypto.h"

#define CRYPTO_MAGIC "PHMBK01"
#define CRYPTO_MAGIC_BYTES 8
#define CRYPTO_SALT_BYTES 16
#define CRYPTO_NONCE_BYTES 12
#define CRYPTO_TAG_BYTES 16
#define CRYPTO_KEY_BYTES 32
#define CRYPTO_PBKDF2_ITERATIONS 150000
#define BACKUP_CRYPTO_YIELD_INTERVAL 2048

typedef struct __attribute__((packed)) {
    uint8_t magic[CRYPTO_MAGIC_BYTES];
    uint8_t version;
    uint8_t salt[CRYPTO_SALT_BYTES];
    uint8_t nonce[CRYPTO_NONCE_BYTES];
    uint32_t plaintext_size;
} backup_crypto_header_t;

static void set_status(char *status, size_t size, const char *message)
{
    if (status && size) {
        strlcpy(status, message ? message : "", size);
    }
}

static bool derive_key(
    const char *passphrase,
    const uint8_t salt[CRYPTO_SALT_BYTES],
    uint8_t key[CRYPTO_KEY_BYTES])
{
    size_t passphrase_length = passphrase ? strnlen(passphrase, 4096) : 0;
    if (passphrase_length < SETTINGS_BACKUP_CRYPTO_MIN_PASSPHRASE_LENGTH ||
        passphrase_length == 4096) {
        return false;
    }

    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attributes, PSA_KEY_TYPE_HMAC);
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&attributes, PSA_ALG_HMAC(PSA_ALG_SHA_256));

    psa_key_id_t password_key = PSA_KEY_ID_NULL;
    psa_status_t result = psa_import_key(
        &attributes,
        (const uint8_t *)passphrase,
        passphrase_length,
        &password_key);
    psa_reset_key_attributes(&attributes);
    if (result != PSA_SUCCESS) {
        return false;
    }

    uint8_t initial[CRYPTO_SALT_BYTES + 4];
    uint8_t u[CRYPTO_KEY_BYTES] = {0};
    uint8_t next_u[CRYPTO_KEY_BYTES] = {0};
    uint8_t block[CRYPTO_KEY_BYTES] = {0};
    size_t mac_size = 0;
    memcpy(initial, salt, CRYPTO_SALT_BYTES);
    initial[CRYPTO_SALT_BYTES + 0] = 0;
    initial[CRYPTO_SALT_BYTES + 1] = 0;
    initial[CRYPTO_SALT_BYTES + 2] = 0;
    initial[CRYPTO_SALT_BYTES + 3] = 1;

    result = psa_mac_compute(
        password_key,
        PSA_ALG_HMAC(PSA_ALG_SHA_256),
        initial,
        sizeof(initial),
        u,
        sizeof(u),
        &mac_size);
    if (result == PSA_SUCCESS && mac_size == sizeof(u)) {
        memcpy(block, u, sizeof(block));
        for (uint32_t iteration = 1;
             iteration < CRYPTO_PBKDF2_ITERATIONS;
             ++iteration) {
            result = psa_mac_compute(
                password_key,
                PSA_ALG_HMAC(PSA_ALG_SHA_256),
                u,
                sizeof(u),
                next_u,
                sizeof(next_u),
                &mac_size);
            if (result != PSA_SUCCESS || mac_size != sizeof(u)) {
                break;
            }
            for (size_t index = 0; index < sizeof(block); ++index) {
                u[index] = next_u[index];
                block[index] ^= u[index];
            }

            /* Keep LVGL, display DMA, and network tasks responsive. */
            if ((iteration % BACKUP_CRYPTO_YIELD_INTERVAL) == 0) {
                vTaskDelay(1);
            }
        }
    }

    bool ok = result == PSA_SUCCESS && mac_size == sizeof(u);
    if (ok) {
        memcpy(key, block, CRYPTO_KEY_BYTES);
    }
    (void)psa_destroy_key(password_key);
    mbedtls_platform_zeroize(initial, sizeof(initial));
    mbedtls_platform_zeroize(u, sizeof(u));
    mbedtls_platform_zeroize(next_u, sizeof(next_u));
    mbedtls_platform_zeroize(block, sizeof(block));
    return ok;
}

static bool import_aes_key(
    const uint8_t key[CRYPTO_KEY_BYTES],
    psa_key_usage_t usage,
    psa_key_id_t *key_id)
{
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attributes, CRYPTO_KEY_BYTES * CHAR_BIT);
    psa_set_key_usage_flags(&attributes, usage);
    psa_set_key_algorithm(&attributes, PSA_ALG_GCM);
    psa_status_t result = psa_import_key(
        &attributes, key, CRYPTO_KEY_BYTES, key_id);
    psa_reset_key_attributes(&attributes);
    return result == PSA_SUCCESS;
}

void settings_backup_crypto_free(void *buffer, size_t size)
{
    if (buffer) {
        mbedtls_platform_zeroize(buffer, size);
        free(buffer);
    }
}

bool settings_backup_crypto_encrypt(
    const char *passphrase,
    const uint8_t *plaintext,
    size_t plaintext_size,
    uint8_t **encrypted,
    size_t *encrypted_size,
    char *status,
    size_t status_size)
{
    if (!encrypted || !encrypted_size || !plaintext ||
        plaintext_size == 0 || plaintext_size > UINT32_MAX) {
        set_status(status, status_size, "Encrypted backup input is invalid.");
        return false;
    }
    *encrypted = NULL;
    *encrypted_size = 0;

    size_t total_size = sizeof(backup_crypto_header_t) +
        plaintext_size + CRYPTO_TAG_BYTES;
    uint8_t *output = calloc(1, total_size);
    if (!output) {
        set_status(status, status_size, "Not enough memory to encrypt the backup.");
        return false;
    }

    backup_crypto_header_t *header = (backup_crypto_header_t *)output;
    memcpy(header->magic, CRYPTO_MAGIC, CRYPTO_MAGIC_BYTES);
    header->version = SETTINGS_BACKUP_CRYPTO_FORMAT_VERSION;
    header->plaintext_size = (uint32_t)plaintext_size;
    esp_fill_random(header->salt, sizeof(header->salt));
    esp_fill_random(header->nonce, sizeof(header->nonce));

    uint8_t key[CRYPTO_KEY_BYTES] = {0};
    psa_key_id_t key_id = PSA_KEY_ID_NULL;
    size_t ciphertext_size = 0;
    bool ok = psa_crypto_init() == PSA_SUCCESS &&
        derive_key(passphrase, header->salt, key) &&
        import_aes_key(key, PSA_KEY_USAGE_ENCRYPT, &key_id) &&
        psa_aead_encrypt(
            key_id, PSA_ALG_GCM, header->nonce, sizeof(header->nonce),
            output, sizeof(*header), plaintext, plaintext_size,
            output + sizeof(*header), plaintext_size + CRYPTO_TAG_BYTES,
            &ciphertext_size) == PSA_SUCCESS &&
        ciphertext_size == plaintext_size + CRYPTO_TAG_BYTES;

    if (key_id != PSA_KEY_ID_NULL) {
        (void)psa_destroy_key(key_id);
    }
    mbedtls_platform_zeroize(key, sizeof(key));
    if (!ok) {
        settings_backup_crypto_free(output, total_size);
        set_status(status, status_size,
            "Encryption could not derive a key from this passphrase.");
        return false;
    }
    *encrypted = output;
    *encrypted_size = total_size;
    set_status(status, status_size, "Encrypted backup payload prepared.");
    return true;
}

bool settings_backup_crypto_decrypt(
    const char *passphrase,
    const uint8_t *encrypted,
    size_t encrypted_size,
    uint8_t **plaintext,
    size_t *plaintext_size,
    char *status,
    size_t status_size)
{
    if (!plaintext || !plaintext_size || !encrypted ||
        encrypted_size < sizeof(backup_crypto_header_t) + CRYPTO_TAG_BYTES) {
        set_status(status, status_size, "Encrypted backup format is invalid.");
        return false;
    }
    *plaintext = NULL;
    *plaintext_size = 0;

    const backup_crypto_header_t *header =
        (const backup_crypto_header_t *)encrypted;
    size_t size = header->plaintext_size;
    if (memcmp(header->magic, CRYPTO_MAGIC, CRYPTO_MAGIC_BYTES) != 0 ||
        header->version != SETTINGS_BACKUP_CRYPTO_FORMAT_VERSION ||
        size == 0 || encrypted_size != sizeof(*header) + size + CRYPTO_TAG_BYTES) {
        set_status(status, status_size, "Encrypted backup format is not supported.");
        return false;
    }

    uint8_t *output = calloc(1, size + 1);
    if (!output) {
        set_status(status, status_size, "Not enough memory to decrypt the backup.");
        return false;
    }

    uint8_t key[CRYPTO_KEY_BYTES] = {0};
    psa_key_id_t key_id = PSA_KEY_ID_NULL;
    size_t output_size = 0;
    bool ok = psa_crypto_init() == PSA_SUCCESS &&
        derive_key(passphrase, header->salt, key) &&
        import_aes_key(key, PSA_KEY_USAGE_DECRYPT, &key_id) &&
        psa_aead_decrypt(
            key_id, PSA_ALG_GCM, header->nonce, sizeof(header->nonce),
            encrypted, sizeof(*header), encrypted + sizeof(*header),
            size + CRYPTO_TAG_BYTES, output, size, &output_size) == PSA_SUCCESS &&
        output_size == size;

    if (key_id != PSA_KEY_ID_NULL) {
        (void)psa_destroy_key(key_id);
    }
    mbedtls_platform_zeroize(key, sizeof(key));
    if (!ok) {
        settings_backup_crypto_free(output, size + 1);
        set_status(status, status_size,
            "Passphrase is incorrect or the encrypted backup was changed.");
        return false;
    }
    *plaintext = output;
    *plaintext_size = size;
    set_status(status, status_size, "Encrypted backup authenticated.");
    return true;
}
