#pragma once
#include "esp_netif_ip_addr.h"
#include "esp_err.h"
typedef struct {int placeholder;} esp_netif_t;
typedef struct {esp_ip4_addr_t ip;} esp_netif_ip_info_t;
esp_netif_t *esp_netif_get_handle_from_ifkey(const char *key);
esp_err_t esp_netif_get_ip_info(esp_netif_t *,esp_netif_ip_info_t *);
