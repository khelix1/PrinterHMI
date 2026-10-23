#pragma once
#include <stdint.h>
typedef struct {uint8_t ssid[33];int8_t rssi;unsigned authmode;} wifi_ap_record_t;
typedef struct {int unused;} wifi_scan_config_t;
typedef struct {struct {uint8_t ssid[32],password[64];} sta;} wifi_config_t;
#define WIFI_IF_STA 0
