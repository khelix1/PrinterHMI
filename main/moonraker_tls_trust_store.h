#pragma once
#include <stdbool.h>

bool moonraker_tls_trust_store_init(void);
bool moonraker_tls_trust_store_set_pem_for_profile(int profile_index, const char *pem);
bool moonraker_tls_trust_store_import_file_for_profile(int profile_index, const char *path);
const char *moonraker_tls_trust_store_pem_for_profile(int profile_index);
