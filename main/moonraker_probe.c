#include "moonraker_probe.h"
#include "network_activity_controller.h"

#include "esp_err.h"
#include "esp_http_client.h"

#include "lwip/inet.h"
#include "lwip/sockets.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>


typedef struct {
    char *buf;
    size_t len;
    size_t cap;
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
        size_t available = capture->cap - capture->len - 1;
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
    char *body,
    size_t body_size)
{
    if (!host || !host[0] || !path || !body || body_size < 2) {
        return false;
    }

    char url[128];
    int written = snprintf(
        url, sizeof(url), "http://%s:%d%s", host, port, path);
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
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 800,
        .event_handler = probe_http_event_handler,
        .user_data = &capture,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        return false;
    }

    if (!network_activity_controller_acquire_shared(900)) {
        esp_http_client_cleanup(client);
        return false;
    }

    esp_err_t err = esp_http_client_perform(client);
    network_activity_controller_end_shared();
    int code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    return err == ESP_OK && code == 200;
}


static bool extract_json_string(
    const char *body,
    const char *key,
    char *out,
    size_t out_size)
{
    if (!body || !key || !out || out_size == 0) {
        return false;
    }

    out[0] = '\0';
    char needle[80];
    int written = snprintf(needle, sizeof(needle), "\"%s\"", key);
    if (written < 0 || written >= (int)sizeof(needle)) {
        return false;
    }

    const char *value = strstr(body, needle);
    if (!value || !(value = strchr(value + strlen(needle), ':'))) {
        return false;
    }

    while (*++value == ' ' || *value == '\t') {
    }
    if (*value != '\"') {
        return false;
    }

    ++value;
    size_t copied = 0;
    while (*value && *value != '\"' && copied + 1 < out_size) {
        if (*value == '\\' && value[1]) {
            ++value;
        }
        out[copied++] = *value++;
    }
    out[copied] = '\0';
    return copied > 0 && *value == '\"';
}


bool moonraker_probe_endpoint(
    const char *host,
    int port,
    moonraker_probe_result_t *result)
{
    if (!host || !host[0] || port <= 0 || port >= 65536) {
        return false;
    }

    moonraker_probe_result_t local = {0};
    char body[384];

    if (!probe_get(host, port, "/server/info", body, sizeof(body)) ||
        (!strstr(body, "klippy") &&
         !strstr(body, "moonraker") &&
         !strstr(body, "\"result\""))) {
        return false;
    }

    local.reachable = true;
    char klippy_state[24] = "";
    if (extract_json_string(
            body, "klippy_state", klippy_state, sizeof(klippy_state)) &&
        strcmp(klippy_state, "ready") == 0) {
        local.klippy_ready = true;
    }

    if (probe_get(host, port, "/printer/info", body, sizeof(body))) {
        (void)extract_json_string(
            body, "hostname", local.identity, sizeof(local.identity));
        if (extract_json_string(
                body, "state", klippy_state, sizeof(klippy_state)) &&
            strcmp(klippy_state, "ready") == 0) {
            local.klippy_ready = true;
        }
    }

    if (!local.identity[0]) {
        strlcpy(local.identity, "Moonraker", sizeof(local.identity));
    }

    if (result) {
        *result = local;
    }
    return true;
}


bool moonraker_probe_host(const char *host, int port)
{
    return moonraker_probe_endpoint(host, port, NULL);
}
