#!/usr/bin/env python3
"""Exercise actual TLS owners, restore validation, print JSON and size guards.

NVS/HTTP are fault-injected host fixtures. libcJSON is real. The size-guard
checks simulate the 32-bit target limit; they do not exercise PSA encryption.
"""
from pathlib import Path
import ctypes.util
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def function(source, name):
    pos = source.index(name + '(')
    return source[source.rfind('\n', 0, pos) + 1:source.index('\n}\n', pos) + 3]


with tempfile.TemporaryDirectory(prefix='printerhmi-security-') as tmp:
    folder = Path(tmp)
    headers = {
        'esp_heap_caps.h': '''#pragma once
#include <stdlib.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_INTERNAL 2
#define MALLOC_CAP_8BIT 4
#define heap_caps_calloc(n,s,c) calloc(n,s)
''',
        'esp_log.h': '''#pragma once
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
''',
        'nvs.h': '''#pragma once
#include <stddef.h>
typedef int nvs_handle_t;
typedef int esp_err_t;
#define ESP_OK 0
#define NVS_READONLY 0
#define NVS_READWRITE 1
int nvs_open(const char *,int,nvs_handle_t *);
int nvs_get_str(nvs_handle_t,const char *,char *,size_t *);
int nvs_set_str(nvs_handle_t,const char *,const char *);
int nvs_commit(nvs_handle_t);
void nvs_close(nvs_handle_t);
''',
        'compat.h': '''#include <stdio.h>
#include <string.h>
static inline size_t strlcpy(char *d,const char *s,size_t n) {
 size_t len=strlen(s);if(n){size_t copy=len<n-1?len:n-1;memcpy(d,s,copy);d[copy]=0;}return len;
}
''',
    }
    for name, text in headers.items():
        (folder / name).write_text(text)
    config = (ROOT / 'main/moonraker_config_controller.c').read_text()
    validators = '\n'.join(function(config, name) for name in
                           ['valid_port', 'valid_host', 'valid_api_key',
                            'valid_camera_stream_url', 'profile_valid'])
    (folder / 'validators.c').write_text(validators)
    http = (ROOT / 'main/moonraker.c').read_text()
    # All five authenticated HTTP configuration sites must confine redirects.
    for name, expected in [('moonraker.c', 3), ('moonraker_live_transport.c', 1),
                           ('moonraker_probe.c', 1)]:
        source = (ROOT / 'main' / name).read_text()
        configs = source.split('esp_http_client_config_t config = {')[1:]
        assert len(configs) == expected, name
        for config in configs:
            assert '.disable_auto_redirect = true,' in config.split('};', 1)[0], name
    (folder / 'print.c').write_text(function(http, 'moonraker_start_print_file'))
    backup = (ROOT / 'main/settings_backup.c').read_text()
    assert 'parse_profile_transport_security(profile_json, profile)' in function(backup, 'parse_snapshot')
    start = backup.index('typedef struct {')
    end = backup.index('} settings_backup_snapshot_t;', start) + len('} settings_backup_snapshot_t;')
    (folder / 'backup.c').write_text('''#include <time.h>
typedef int ui_theme_id_t,ui_accent_id_t,ui_density_id_t,ui_notification_level_t;
typedef struct {bool large_text,high_contrast,reduced_transparency,reduced_motion;} ui_accessibility_t;
#define CUSTOM_THEME_ID_MAX 32
#define UI_NOTIFY_DETAILED 2
#define SETTINGS_BACKUP_PRODUCT "PrinterHMI"
#define SETTINGS_BACKUP_SCHEMA 1
''' + backup[start:end] + '\n' + function(backup, 'snapshot_to_json') + '\n' +
        function(backup, 'parse_profile_transport_security'))
    crypto = (ROOT / 'main/settings_backup_crypto.c').read_text()
    encrypt = function(crypto, 'settings_backup_crypto_encrypt')
    guard = encrypt[encrypt.index('    if (!encrypted'):encrypt.index('    size_t total_size')]
    (folder / 'size.c').write_text('''#undef SIZE_MAX
#define SIZE_MAX UINT32_MAX
typedef struct {unsigned char bytes[41];} backup_crypto_header_t;
#define CRYPTO_TAG_BYTES 16
static void set_status(char *s,size_t n,const char *m){if(s&&n)snprintf(s,n,"%s",m);}
static bool size_guard(const uint8_t *plaintext,size_t plaintext_size,
 uint8_t **encrypted,size_t *encrypted_size,char *status,size_t status_size) {
''' + guard + '\nreturn true;\n}\n')
    library = ctypes.util.find_library('cjson')
    if not library:
        raise SystemExit('Real libcJSON is required for this security regression.')
    executable = folder / 'test'
    subprocess.run(['cc', '-D_POSIX_C_SOURCE=200809L', '-std=c11', '-O1', '-g',
                    '-Wall', '-Wextra', '-Werror', '-fsanitize=undefined',
                    '-include', str(folder / 'compat.h'), '-I', str(folder),
                    '-I', str(ROOT / 'main'),
                    str(ROOT / 'tools/audit/security_boundaries_test.c'),
                    str(ROOT / 'main/moonraker_tls_trust_store.c'),
                    str(ROOT / 'main/moonraker_transport_security_controller.c'),
                    '-Wl,-l:' + library, '-o', str(executable)], check=True)
    subprocess.run([str(executable), str(folder / 'ca.pem')], check=True)
