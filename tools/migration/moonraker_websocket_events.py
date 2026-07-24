#!/usr/bin/env python3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MAIN = ROOT / "main" / "main.c"
MOON_H = ROOT / "main" / "moonraker.h"
MOON_C = ROOT / "main" / "moonraker.c"
WS_H = ROOT / "main" / "moonraker_live_websocket.h"
WS_C = ROOT / "main" / "moonraker_live_websocket.c"


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected one {label}, found {count}")
    return text.replace(old, new, 1)


for required in (MAIN, MOON_H, MOON_C, WS_H, WS_C):
    if not required.exists():
        raise RuntimeError(f"missing required file: {required}")

main = MAIN.read_text()
moon_h = MOON_H.read_text()
moon_c = MOON_C.read_text()
ws_h = WS_H.read_text()
ws_c = WS_C.read_text()

marker = "MOONRAKER_WEBSOCKET_PUSH_EVENTS"

if marker in main:
    print("PASS: Moonraker WebSocket push events already installed")
    raise SystemExit(0)

if "moonraker_live_websocket_fresh" not in ws_c:
    raise RuntimeError(
        "WebSocket Phase 2 is required before installing push events"
    )

if "WS_REBIND_CLIENT_IDENTITY_GUARD" not in ws_c:
    raise RuntimeError(
        "run tools/migration/moonraker_websocket_rebind_race_fix.py first"
    )


# -------------------------------------------------------------------------
# State owner: classify complete JSON-RPC messages and apply Klippy state.
# -------------------------------------------------------------------------

moon_h = replace_once(
    moon_h,
    '''/*
 * Merge a complete Moonraker WebSocket JSON-RPC message. Both the initial
 * subscription result and incremental notify_status_update messages are
 * accepted. Missing fields retain their previous values.
 */
bool moonraker_state_merge_websocket_json(
    const char *json,
    size_t length);
''',
    '''typedef enum {
    MOONRAKER_WEBSOCKET_MESSAGE_IGNORED = 0,
    MOONRAKER_WEBSOCKET_MESSAGE_STATUS,
    MOONRAKER_WEBSOCKET_MESSAGE_FILELIST_CHANGED,
    MOONRAKER_WEBSOCKET_MESSAGE_KLIPPY_READY,
    MOONRAKER_WEBSOCKET_MESSAGE_KLIPPY_SHUTDOWN,
    MOONRAKER_WEBSOCKET_MESSAGE_KLIPPY_DISCONNECTED
} moonraker_websocket_message_t;

/*
 * Merge or classify one complete Moonraker WebSocket JSON-RPC message.
 * Status messages update the synchronized live-state snapshot. File-list
 * and Klippy lifecycle notifications are returned to the transport owner.
 */
moonraker_websocket_message_t moonraker_state_merge_websocket_json(
    const char *json,
    size_t length);
''',
    "Moonraker WebSocket message API",
)

moon_c = replace_once(
    moon_c,
    '''bool moonraker_state_merge_websocket_json(
    const char *json,
    size_t length)
{
    if (!json || length == 0) return false;

    cJSON *root = cJSON_ParseWithLength(json, length);
    if (!root) return false;

    cJSON *status = json_status_object(root);
    if (!cJSON_IsObject(status)) {
        cJSON_Delete(root);
        return false;
    }
''',
    '''moonraker_websocket_message_t moonraker_state_merge_websocket_json(
    const char *json,
    size_t length)
{
    if (!json || length == 0) {
        return MOONRAKER_WEBSOCKET_MESSAGE_IGNORED;
    }

    cJSON *root = cJSON_ParseWithLength(json, length);
    if (!root) return MOONRAKER_WEBSOCKET_MESSAGE_IGNORED;

    cJSON *method = cJSON_GetObjectItemCaseSensitive(root, "method");
    const char *method_name =
        cJSON_IsString(method) ? method->valuestring : NULL;

    if (method_name &&
        strcmp(method_name, "notify_filelist_changed") == 0) {
        cJSON_Delete(root);
        return MOONRAKER_WEBSOCKET_MESSAGE_FILELIST_CHANGED;
    }

    moonraker_websocket_message_t lifecycle =
        MOONRAKER_WEBSOCKET_MESSAGE_IGNORED;

    if (method_name && strcmp(method_name, "notify_klippy_ready") == 0) {
        lifecycle = MOONRAKER_WEBSOCKET_MESSAGE_KLIPPY_READY;
    } else if (method_name &&
               strcmp(method_name, "notify_klippy_shutdown") == 0) {
        lifecycle = MOONRAKER_WEBSOCKET_MESSAGE_KLIPPY_SHUTDOWN;
    } else if (method_name &&
               strcmp(method_name, "notify_klippy_disconnected") == 0) {
        lifecycle = MOONRAKER_WEBSOCKET_MESSAGE_KLIPPY_DISCONNECTED;
    }

    if (lifecycle != MOONRAKER_WEBSOCKET_MESSAGE_IGNORED) {
        state_lock();

        if (lifecycle == MOONRAKER_WEBSOCKET_MESSAGE_KLIPPY_READY) {
            g_moonraker_state.live_data_ok = false;
            g_moonraker_state.moonraker_ok = true;
            mr_safe_copy(
                g_moonraker_state.printer_state,
                sizeof(g_moonraker_state.printer_state),
                "--");
        } else if (lifecycle ==
                   MOONRAKER_WEBSOCKET_MESSAGE_KLIPPY_SHUTDOWN) {
            g_moonraker_state.live_data_ok = false;
            g_moonraker_state.moonraker_ok = true;
            mr_safe_copy(
                g_moonraker_state.printer_state,
                sizeof(g_moonraker_state.printer_state),
                "error");
        } else {
            g_moonraker_state.live_data_ok = false;
            g_moonraker_state.moonraker_ok = false;
            mr_safe_copy(
                g_moonraker_state.printer_state,
                sizeof(g_moonraker_state.printer_state),
                "--");
        }

        state_unlock();
        cJSON_Delete(root);
        return lifecycle;
    }

    cJSON *status = json_status_object(root);
    if (!cJSON_IsObject(status)) {
        cJSON_Delete(root);
        return MOONRAKER_WEBSOCKET_MESSAGE_IGNORED;
    }
''',
    "Moonraker WebSocket merge entry",
)

moon_c = replace_once(
    moon_c,
    '''    state_unlock();
    cJSON_Delete(root);
    return updates > 0;
}


const char *json_find_string''',
    '''    state_unlock();
    cJSON_Delete(root);
    return updates > 0
        ? MOONRAKER_WEBSOCKET_MESSAGE_STATUS
        : MOONRAKER_WEBSOCKET_MESSAGE_IGNORED;
}


const char *json_find_string''',
    "Moonraker WebSocket merge result",
)


# -------------------------------------------------------------------------
# Transport owner: coalesced Files invalidation and Klippy recovery.
# -------------------------------------------------------------------------

ws_h = replace_once(
    ws_h,
    '''bool moonraker_live_websocket_connected(void);
bool moonraker_live_websocket_subscribed(void);
bool moonraker_live_websocket_fresh(int64_t maximum_age_us);
void moonraker_live_websocket_stop(void);
''',
    '''bool moonraker_live_websocket_connected(void);
bool moonraker_live_websocket_subscribed(void);
bool moonraker_live_websocket_fresh(int64_t maximum_age_us);

/* A file-list notification is coalesced until the LVGL owner consumes it. */
bool moonraker_live_websocket_file_change_pending(void);
bool moonraker_live_websocket_take_file_change(void);

void moonraker_live_websocket_stop(void);
''',
    "WebSocket file-change API",
)

ws_c = replace_once(
    ws_c,
    '''static volatile uint32_t s_status_merge_count = 0;
static volatile int64_t s_last_status_update_us = 0;
''',
    '''static volatile uint32_t s_status_merge_count = 0;
static volatile int64_t s_last_status_update_us = 0;
static bool s_file_change_pending = false;
''',
    "WebSocket push-event state",
)

ws_c = replace_once(
    ws_c,
    '''    if (moonraker_state_merge_websocket_json(
            s_message_buffer,
            (size_t)payload_length)) {
        s_last_status_update_us = esp_timer_get_time();

        uint32_t count = ++s_status_merge_count;
        if (count <= 5 || count % 100 == 0) {
            ESP_LOGI(TAG, "WS_STATUS_MERGED count=%u generation=%u",
                     (unsigned)count,
                     (unsigned)s_generation);
        }
    }
''',
    '''    moonraker_websocket_message_t message =
        moonraker_state_merge_websocket_json(
            s_message_buffer,
            (size_t)payload_length);

    switch (message) {
    case MOONRAKER_WEBSOCKET_MESSAGE_STATUS: {
        s_last_status_update_us = esp_timer_get_time();

        uint32_t count = ++s_status_merge_count;
        if (count <= 5 || count % 100 == 0) {
            ESP_LOGI(TAG, "WS_STATUS_MERGED count=%u generation=%u",
                     (unsigned)count,
                     (unsigned)s_generation);
        }
        break;
    }

    case MOONRAKER_WEBSOCKET_MESSAGE_FILELIST_CHANGED:
        __atomic_store_n(&s_file_change_pending, true, __ATOMIC_RELEASE);
        ESP_LOGI(TAG, "WS_FILELIST_CHANGED generation=%u",
                 (unsigned)s_generation);
        break;

    case MOONRAKER_WEBSOCKET_MESSAGE_KLIPPY_READY:
        /* A Klippy restart invalidates the old object subscription. */
        s_last_status_update_us = 0;
        s_subscribed = false;
        s_subscribe_pending = true;
        ESP_LOGI(TAG, "WS_KLIPPY_READY resubscribe generation=%u",
                 (unsigned)s_generation);
        break;

    case MOONRAKER_WEBSOCKET_MESSAGE_KLIPPY_SHUTDOWN:
        s_last_status_update_us = esp_timer_get_time();
        ESP_LOGW(TAG, "WS_KLIPPY_SHUTDOWN generation=%u",
                 (unsigned)s_generation);
        break;

    case MOONRAKER_WEBSOCKET_MESSAGE_KLIPPY_DISCONNECTED:
        s_last_status_update_us = 0;
        s_subscribed = false;
        ESP_LOGW(TAG, "WS_KLIPPY_DISCONNECTED generation=%u",
                 (unsigned)s_generation);
        break;

    case MOONRAKER_WEBSOCKET_MESSAGE_IGNORED:
    default:
        break;
    }
''',
    "WebSocket completed-message dispatch",
)

ws_c = replace_once(
    ws_c,
    '''    s_last_status_update_us = 0;
    s_status_merge_count = 0;
    s_message_generation = 0;

    if (!client) return;
''',
    '''    s_last_status_update_us = 0;
    s_status_merge_count = 0;
    s_message_generation = 0;
    __atomic_store_n(&s_file_change_pending, false, __ATOMIC_RELEASE);

    if (!client) return;
''',
    "WebSocket destroy event reset",
)

ws_c = replace_once(
    ws_c,
    '''bool moonraker_live_websocket_fresh(int64_t maximum_age_us)
{
    int64_t updated = s_last_status_update_us;
    if (!s_connected || !s_subscribed || updated <= 0 || maximum_age_us <= 0) {
        return false;
    }

    int64_t age = esp_timer_get_time() - updated;
    return age >= 0 && age <= maximum_age_us;
}
''',
    '''bool moonraker_live_websocket_fresh(int64_t maximum_age_us)
{
    int64_t updated = s_last_status_update_us;
    if (!s_connected || !s_subscribed || updated <= 0 || maximum_age_us <= 0) {
        return false;
    }

    int64_t age = esp_timer_get_time() - updated;
    return age >= 0 && age <= maximum_age_us;
}


bool moonraker_live_websocket_file_change_pending(void)
{
    return __atomic_load_n(&s_file_change_pending, __ATOMIC_ACQUIRE);
}


bool moonraker_live_websocket_take_file_change(void)
{
    return __atomic_exchange_n(
        &s_file_change_pending,
        false,
        __ATOMIC_ACQ_REL);
}
''',
    "WebSocket file-change implementation",
)

# Make transport loss visible immediately; HTTP may replace it on its next
# proven fallback poll.
ws_c = replace_once(
    ws_c,
    '''    case WEBSOCKET_EVENT_DISCONNECTED:
        s_connected = false;
        s_subscribed = false;
        s_subscribe_pending = false;
        ESP_LOGW(TAG, "WS_DISCONNECTED %s", s_uri);
''',
    '''    case WEBSOCKET_EVENT_DISCONNECTED:
        s_connected = false;
        s_subscribed = false;
        s_subscribe_pending = false;
        s_last_status_update_us = 0;
        moonraker_state_set_connection(false, false);
        ESP_LOGW(TAG, "WS_DISCONNECTED %s", s_uri);
''',
    "WebSocket disconnect state",
)

ws_c = replace_once(
    ws_c,
    '''    case WEBSOCKET_EVENT_CLOSED:
        s_connected = false;
        s_subscribed = false;
        s_subscribe_pending = false;
        ESP_LOGW(TAG, "WS_CLOSED %s", s_uri);
''',
    '''    case WEBSOCKET_EVENT_CLOSED:
        s_connected = false;
        s_subscribed = false;
        s_subscribe_pending = false;
        s_last_status_update_us = 0;
        moonraker_state_set_connection(false, false);
        ESP_LOGW(TAG, "WS_CLOSED %s", s_uri);
''',
    "WebSocket closed state",
)


# -------------------------------------------------------------------------
# LVGL owner: refresh only the visible Files page and never a detail popup.
# -------------------------------------------------------------------------

files_push_helper = r'''/* MOONRAKER_WEBSOCKET_PUSH_EVENTS
 * Moonraker keeps file payload transport on HTTP. WebSocket only invalidates
 * the active profile's visible Files page, coalescing bursts into one reload.
 */
static void moonraker_process_filelist_notification(void)
{
    if (!moonraker_live_websocket_file_change_pending()) return;

    if (!ui_files_v32_get_popup()) {
        /* Opening Files always performs a fresh HTTP reload. */
        (void)moonraker_live_websocket_take_file_change();
        return;
    }

    if (ui_files_v32_detail_is_open()) {
        /* Preserve the confirmation popup; consume after it closes. */
        return;
    }

    if (!moonraker_live_websocket_take_file_change()) return;

    ESP_LOGI(TAG, "WS_FILELIST_REFRESH visible Files page");
    ui_files_v32_refresh();
}


'''

main = replace_once(
    main,
    "static void ui_refresh_timer_cb(lv_timer_t *timer)\n",
    files_push_helper + "static void ui_refresh_timer_cb(lv_timer_t *timer)\n",
    "UI refresh callback anchor",
)

main = replace_once(
    main,
    '''    ota_manager_pump_ui();
    moonraker_sync_legacy_from_state();

    (void)timer;
''',
    '''    ota_manager_pump_ui();
    moonraker_sync_legacy_from_state();
    moonraker_process_filelist_notification();

    (void)timer;
''',
    "UI file-list notification pump",
)


MAIN.write_text(main)
MOON_H.write_text(moon_h)
MOON_C.write_text(moon_c)
WS_H.write_text(ws_h)
WS_C.write_text(ws_c)

print("PASS: Moonraker WebSocket push events installed")
print("  - Files page refreshes immediately after file-list notifications")
print("  - hidden Files pages still reload normally when opened")
print("  - file-detail popups are never destroyed by an automatic refresh")
print("  - Klippy ready forces a new full object subscription")
print("  - Klippy shutdown/disconnect updates operator state immediately")
print("  - file invalidation remains isolated to the active printer profile")
print("Next: idf.py build")

