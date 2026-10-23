/* All controller and UI calls execute on the host's single LVGL thread. */
#pragma once
#include "FreeRTOS.h"
#ifndef portMAX_DELAY
#define portMAX_DELAY UINT32_MAX
#endif
typedef void *SemaphoreHandle_t;
static inline SemaphoreHandle_t xSemaphoreCreateMutex(void){return (void *)1;}
static inline int xSemaphoreTake(SemaphoreHandle_t lock,uint32_t wait){(void)lock;(void)wait;return 1;}
static inline int xSemaphoreGive(SemaphoreHandle_t lock){(void)lock;return 1;}
