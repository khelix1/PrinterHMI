#pragma once
#include <stdint.h>
typedef struct {uint32_t addr;} esp_ip4_addr_t;
#define IPSTR "%u.%u.%u.%u"
#define IP2STR(address) ((address)->addr&255),(((address)->addr>>8)&255),(((address)->addr>>16)&255),(((address)->addr>>24)&255)
