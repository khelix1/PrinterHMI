#include "print_estimates.h"
#include "nvs.h"
#ifdef PRINTERHMI_DEMO_RUNTIME
#include "demo_mode.h"
#endif

static const print_estimate_options_t defaults = {
    PRINT_PROGRESS_FILE_RELATIVE, PRINT_ESTIMATE_FILE, PRINT_ESTIMATE_FILE,
};

#ifdef PRINTERHMI_DEMO_RUNTIME
static print_estimate_options_t demo = {
    PRINT_PROGRESS_FILE_RELATIVE, PRINT_ESTIMATE_FILE, PRINT_ESTIMATE_FILE,
};
#endif
static bool loaded;
static print_estimate_options_t selected;

print_estimate_options_t print_estimate_options_get(void)
{
#ifdef PRINTERHMI_DEMO_RUNTIME
    if (demo_mode_enabled()) return demo;
#endif
    if (!loaded) {
        loaded = true;
        selected = defaults;
        nvs_handle_t handle;
        uint8_t packed;
        if (nvs_open("hmi", NVS_READONLY, &handle) == ESP_OK) {
            if (nvs_get_u8(handle, "print_estimate", &packed) == ESP_OK)
                selected = (print_estimate_options_t){packed & 3, (packed >> 2) & 7, packed >> 5};
            nvs_close(handle);
        }
    }
    return selected;
}

bool print_estimate_options_save(print_estimate_options_t options)
{
    if ((unsigned)options.progress > PRINT_PROGRESS_FILAMENT ||
        options.remaining_sources > 7 || options.eta_sources > 7) return false;
#ifdef PRINTERHMI_DEMO_RUNTIME
    if (demo_mode_enabled()) { demo = options; return true; }
#endif
    nvs_handle_t handle;
    if (nvs_open("hmi", NVS_READWRITE, &handle) != ESP_OK) return false;
    uint8_t packed = options.progress | (options.remaining_sources << 2) | (options.eta_sources << 5);
    bool ok = nvs_set_u8(handle, "print_estimate", packed) == ESP_OK &&
              nvs_commit(handle) == ESP_OK;
    nvs_close(handle);
    if (ok) { selected = options; loaded = true; }
    return ok;
}
