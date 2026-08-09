#include "ota_boot_validation.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_ota_ops.h"

static const char *TAG = "PrinterHMI";

void ota_boot_validation_confirm_running_image(void)
{
#if CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;

    if (esp_ota_get_state_partition(running, &state) == ESP_OK) {
        ESP_LOGI(TAG, "OTA: running partition=%s state=%d",
                 running ? running->label : "unknown", state);

        if (state == ESP_OTA_IMG_PENDING_VERIFY) {
            esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "OTA: image marked valid, rollback cancelled");
            } else {
                ESP_LOGE(TAG, "OTA: mark valid failed: %s", esp_err_to_name(err));
            }
        }
    }
#endif
}
