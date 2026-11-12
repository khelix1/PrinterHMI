#include "moonraker_probe.h"
#include "moonraker_transport_security_controller.h"
#include "network_activity_controller.h"

#include "esp_err.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "json_document_parse.h"
#include "esp_heap_caps.h"

#include "lwip/inet.h"
#include "lwip/sockets.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


typedef struct {
    char *buf;
    size_t len;
    size_t cap;
    bool invalid;
} moonraker_probe_capture_t;


static esp_err_t probe_http_event_handler(
    esp_http_client_event_t *event)
{
    if (!event || !event->user_data) {
        return ESP_OK;
    }

    moonraker_probe_capture_t *capture =
        (moonraker_probe_capture_t *)event->user_data;

    if (event->event_id == HTTP_EVENT_ON_DATA &&
        event->data && event->data_len > 0 &&
        capture->buf && capture->cap > 1) {
        if (memchr(event->data, '\0', (size_t)event->data_len)) capture->invalid = true;
        size_t available = capture->cap - capture->len - 1;
        if ((size_t)event->data_len > available) capture->invalid = true;
        size_t copy_len = (size_t)event->data_len < available
            ? (size_t)event->data_len
            : available;

        if (copy_len > 0) {
            memcpy(capture->buf + capture->len, event->data, copy_len);
            capture->len += copy_len;
            capture->buf[capture->len] = '\0';
        }
    }

    return ESP_OK;
}


size_t moonraker_probe_open_ports(
    const char *host,
    const int *ports,
    size_t port_count,
    int timeout_ms,
    bool *open_ports,
    size_t open_ports_count)
{
    if (!host || !host[0] || !ports || !open_ports ||
        port_count == 0 || open_ports_count < port_count ||
        timeout_ms <= 0) {
        return 0;
    }

    memset(open_ports, 0, open_ports_count * sizeof(*open_ports));

    struct in_addr address;
    if (inet_aton(host, &address) == 0) {
        return 0;
    }

    /*
     * This is a single shared-lane operation. It emits at most four TCP SYNs
     * for one address and contains no HTTP payloads.
     */
    if (!network_activity_controller_acquire_shared(timeout_ms + 150)) {
        return 0;
    }

    int sockets[8];
    if (port_count > sizeof(sockets) / sizeof(sockets[0])) {
        network_activity_controller_end_shared();
        return 0;
    }

    for (size_t index = 0; index < port_count; ++index) {
        sockets[index] = -1;
    }

    fd_set write_fds;
    FD_ZERO(&write_fds);
    int max_fd = -1;
    size_t open_count = 0;

    for (size_t index = 0; index < port_count; ++index) {
        if (ports[index] <= 0 || ports[index] >= 65536) {
            continue;
        }

        int socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (socket_fd < 0) {
            continue;
        }

        int flags = fcntl(socket_fd, F_GETFL, 0);
        if (flags < 0 || fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
            close(socket_fd);
            continue;
        }

        struct sockaddr_in destination = {
            .sin_family = AF_INET,
            .sin_port = htons((uint16_t)ports[index]),
            .sin_addr = address,
        };

        int connect_result = connect(
            socket_fd,
            (const struct sockaddr *)&destination,
            sizeof(destination));

        if (connect_result == 0) {
            open_ports[index] = true;
            open_count++;
            close(socket_fd);
            continue;
        }

        if (errno != EINPROGRESS && errno != EALREADY) {
            close(socket_fd);
            continue;
        }

        sockets[index] = socket_fd;
        FD_SET(socket_fd, &write_fds);
        if (socket_fd > max_fd) {
            max_fd = socket_fd;
        }
    }

    if (max_fd >= 0) {
        struct timeval timeout = {
            .tv_sec = timeout_ms / 1000,
            .tv_usec = (timeout_ms % 1000) * 1000,
        };

        int selected = select(
            max_fd + 1, NULL, &write_fds, NULL, &timeout);

        if (selected > 0) {
            for (size_t index = 0; index < port_count; ++index) {
                int socket_fd = sockets[index];
                if (socket_fd < 0 || !FD_ISSET(socket_fd, &write_fds)) {
                    continue;
                }

                int socket_error = 0;
                socklen_t socket_error_size = sizeof(socket_error);
                if (getsockopt(
                        socket_fd,
                        SOL_SOCKET,
                        SO_ERROR,
                        &socket_error,
                        &socket_error_size) == 0 &&
                    socket_error == 0) {
                    open_ports[index] = true;
                    open_count++;
                }
            }
        }
    }

    for (size_t index = 0; index < port_count; ++index) {
        if (sockets[index] >= 0) {
            close(sockets[index]);
        }
    }

    network_activity_controller_end_shared();
    return open_count;
}


static bool probe_get(
    const char *host,
    int port,
    const char *path,
    const char *api_key,
    char *body,
    size_t body_size)
{
    if (!host || !host[0] || !path || !body || body_size < 2) {
        return false;
    }

    char url[128];
    const char *ca_pem = NULL;
    int written = moonraker_transport_security_build_http_url_for_endpoint(
        host, port, path, url, sizeof(url), &ca_pem) ? (int)strlen(url) : -1;
    if (written < 0 || written >= (int)sizeof(url)) {
        return false;
    }

    body[0] = '\0';
    moonraker_probe_capture_t capture = {
        .buf = body,
        .len = 0,
        .cap = body_size,
    };

    esp_http_client_config_t config = {
        /* API credentials and commands must stay on the selected endpoint. */
        .disable_auto_redirect = true,
        .url = url,
        .cert_pem = ca_pem,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 800,
        .event_handler = probe_http_event_handler,
        .user_data = &capture,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        return false;
    }

    if (api_key && api_key[0]) {
        if (esp_http_client_set_header(client, "X-Api-Key", api_key) != ESP_OK) {
            esp_http_client_cleanup(client);
            return false;
        }
    }

    if (!network_activity_controller_acquire_shared(900)) {
        esp_http_client_cleanup(client);
        return false;
    }

    esp_err_t err = esp_http_client_perform(client);
    network_activity_controller_end_shared();
    int code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    return err == ESP_OK && code == 200 && !capture.invalid;
}


static bool normalize_webcam_url(
    const char *host,
    const char *source,
    char *destination,
    size_t destination_size)
{
    if (!host || !host[0] || !source || !source[0] ||
        !destination || destination_size == 0) {
        return false;
    }

    if (strncmp(source, "http://", 7) == 0 ||
        strncmp(source, "https://", 8) == 0) {
        /* Never persist a different address produced by truncating a URL. */
        if (strlen(source) >= destination_size) return false;
        memcpy(destination, source, strlen(source) + 1);
        return true;
    }

    /* A scheme belongs before the first path/query/fragment delimiter.
     * Unsupported schemes must not turn into apparent HTTP camera paths. */
    const char *colon = strchr(source, ':');
    const char *delimiter = strpbrk(source, "/?#");
    if (colon && (!delimiter || colon < delimiter)) return false;

    /* Relative webcam paths are served by the host web proxy on port 80,
     * not by Moonraker's API port or its secured HTTPS proxy port. */
    int written = snprintf(
        destination,
        destination_size,
        "http://%s/%s",
        host,
        source[0] == '/' ? source + 1 : source);
    if (written <= 0 || (size_t)written >= destination_size) {
        destination[0] = '\0';
        return false;
    }
    return true;
}



size_t moonraker_probe_webcams_with_api_key(
    const char *host, int port, const char *api_key,
    moonraker_webcam_t *webcams, size_t webcam_capacity)
{
    if (!host || !host[0] || port <= 0 || port >= 65536 ||
        !webcams || webcam_capacity == 0) return 0;

    char *body = heap_caps_malloc(4096, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!body) body = heap_caps_malloc(4096, MALLOC_CAP_8BIT);
    if (!body) return 0;

    size_t count = 0;
    if (probe_get(host, port, "/server/webcams/list", api_key, body, 4096)) {
        cJSON *root = json_document_parse_bounded(body, strlen(body), 4095, 16);
        cJSON *result = root ? cJSON_GetObjectItemCaseSensitive(root, "result") : NULL;
        cJSON *items = result ? cJSON_GetObjectItemCaseSensitive(result, "webcams") :
            (root ? cJSON_GetObjectItemCaseSensitive(root, "webcams") : NULL);
        cJSON *entry = NULL;
        if (!cJSON_GetObjectItemCaseSensitive(root, "error") && cJSON_IsArray(items)) cJSON_ArrayForEach(entry, items) {
            if (count >= webcam_capacity) break;
            moonraker_webcam_t candidate = {0};
            cJSON *enabled = cJSON_GetObjectItemCaseSensitive(entry, "enabled");
            cJSON *stream = cJSON_GetObjectItemCaseSensitive(entry, "stream_url");
            if (!cJSON_IsObject(entry) || (enabled && !cJSON_IsTrue(enabled)) ||
                !cJSON_IsString(stream) || !stream->valuestring ||
                !normalize_webcam_url(host, stream->valuestring,
                    candidate.stream_url, sizeof(candidate.stream_url))) continue;
            cJSON *name = cJSON_GetObjectItemCaseSensitive(entry, "name");
            strlcpy(candidate.name,
                cJSON_IsString(name) && name->valuestring ? name->valuestring : "Moonraker camera",
                sizeof(candidate.name));
            cJSON *snapshot = cJSON_GetObjectItemCaseSensitive(entry, "snapshot_url");
            if (cJSON_IsString(snapshot) && snapshot->valuestring)
                (void)normalize_webcam_url(host, snapshot->valuestring,
                    candidate.snapshot_url, sizeof(candidate.snapshot_url));
            webcams[count++] = candidate;
        }
        cJSON_Delete(root);
    }
    free(body);
    return count;
}


bool moonraker_probe_endpoint_with_api_key(
    const char *host,
    int port,
    const char *api_key,
    moonraker_probe_result_t *result)
{
    if (!host || !host[0] || port <= 0 || port >= 65536) {
        return false;
    }

    moonraker_probe_result_t local = {0};
    /* Parse complete bounded responses rather than inspecting a truncated
     * stack prefix for keywords. One buffer is reused for both requests. */
    const size_t capacity = 8192;
    char *body = heap_caps_malloc(capacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!body) body = heap_caps_malloc(capacity, MALLOC_CAP_8BIT);
    if (!body) return false;
    cJSON *root = NULL;
    if (probe_get(host, port, "/server/info", api_key, body, capacity)) {
        root = json_document_parse_bounded(body, strlen(body), capacity - 1, 16);
    }
    const cJSON *info = cJSON_GetObjectItemCaseSensitive(root, "result");
    const cJSON *state = cJSON_GetObjectItemCaseSensitive(info, "klippy_state");
    bool verified = cJSON_IsObject(info) && cJSON_IsString(state) && state->valuestring[0] &&
        !cJSON_GetObjectItemCaseSensitive(root, "error");
    if (!verified) {
        cJSON_Delete(root);
        free(body);
        return false;
    }
    local.reachable = true;
    local.klippy_ready = strcmp(state->valuestring, "ready") == 0;
    cJSON_Delete(root);

    if (probe_get(host, port, "/printer/info", api_key, body, capacity)) {
        root = json_document_parse_bounded(body, strlen(body), capacity - 1, 16);
        info = cJSON_GetObjectItemCaseSensitive(root, "result");
        if (cJSON_IsObject(info) && !cJSON_GetObjectItemCaseSensitive(root, "error")) {
            const cJSON *identity = cJSON_GetObjectItemCaseSensitive(info, "hostname");
            if (cJSON_IsString(identity)) strlcpy(local.identity, identity->valuestring, sizeof(local.identity));
            state = cJSON_GetObjectItemCaseSensitive(info, "state");
            if (cJSON_IsString(state)) local.klippy_ready = strcmp(state->valuestring, "ready") == 0;
        }
        cJSON_Delete(root);
    }
    free(body);

    if (!local.identity[0]) {
        strlcpy(local.identity, "Moonraker", sizeof(local.identity));
    }

    if (result) {
        *result = local;
    }
    return true;
}


bool moonraker_probe_endpoint(
    const char *host,
    int port,
    moonraker_probe_result_t *result)
{
    return moonraker_probe_endpoint_with_api_key(host, port, NULL, result);
}


bool moonraker_probe_host(const char *host, int port)
{
    return moonraker_probe_endpoint(host, port, NULL);
}
