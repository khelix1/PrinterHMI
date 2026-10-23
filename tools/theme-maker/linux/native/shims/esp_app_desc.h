#pragma once
typedef struct {char version[32],date[16],time[16],idf_ver[32];} esp_app_desc_t;
static inline const esp_app_desc_t *esp_app_get_description(void){static const esp_app_desc_t app={.version="7.0.0-simulation",.date="Local simulation",.time="",.idf_ver="6.0.2 (sample)"};return &app;}
