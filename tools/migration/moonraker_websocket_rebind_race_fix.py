#!/usr/bin/env python3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "main" / "moonraker_live_websocket.c"


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected one {label}, found {count}")
    return text.replace(old, new, 1)


if not SOURCE.exists():
    raise RuntimeError(f"missing required file: {SOURCE}")

text = SOURCE.read_text()
marker = "WS_REBIND_CLIENT_IDENTITY_GUARD"

if marker in text:
    print("PASS: WebSocket rebind race already fixed")
    raise SystemExit(0)

text = replace_once(
    text,
    '''    case WEBSOCKET_EVENT_DATA: {
        esp_websocket_event_data_t *data =
            (esp_websocket_event_data_t *)event_data;

        handle_websocket_data(data);
''',
    '''    case WEBSOCKET_EVENT_DATA: {
        esp_websocket_event_data_t *data =
            (esp_websocket_event_data_t *)event_data;

        /* WS_REBIND_CLIENT_IDENTITY_GUARD
         * stop() may deliver a final queued frame from the retiring client.
         * Generation alone cannot reject it until the replacement endpoint
         * has been installed, so require exact client ownership as well.
         */
        if (!data || data->client != s_client) {
            ESP_LOGW(TAG, "WS_STALE_CLIENT_EVENT discarded");
            break;
        }

        handle_websocket_data(data);
''',
    "WebSocket data event handler",
)

text = replace_once(
    text,
    '''    copy_text(s_host, sizeof(s_host), host);
    copy_text(s_api_key, sizeof(s_api_key), api_key);
    s_generation = generation;

    esp_websocket_client_config_t config = {
''',
    '''    copy_text(s_host, sizeof(s_host), host);
    copy_text(s_api_key, sizeof(s_api_key), api_key);
    s_generation = generation;

    /* A replacement connection is not fresh until it merges its own first
     * subscription status message.
     */
    s_last_status_update_us = 0;
    s_status_merge_count = 0;
    s_receive_count = 0;
    s_message_generation = 0;

    esp_websocket_client_config_t config = {
''',
    "WebSocket client creation identity reset",
)

SOURCE.write_text(text)

print("PASS: WebSocket profile-rebind race fixed")
print("  - retiring-client data events are rejected by handle identity")
print("  - replacement connection starts with no inherited freshness")
print("  - HTTP remains authoritative until the new profile merges status")
print("Next: idf.py build")

