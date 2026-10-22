#pragma once
#include <stdio.h>
#define ESP_LOGI(t,...) do { (void)(t); if(0) fprintf(stderr,__VA_ARGS__); } while(0)
#define ESP_LOGW ESP_LOGI
#define ESP_LOGE ESP_LOGI
