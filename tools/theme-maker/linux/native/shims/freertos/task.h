#pragma once
static inline void vTaskDelay(unsigned ticks){(void)ticks;}
/* Setup scans run synchronously in this single-threaded fixture. Results are
 * still consumed by the real UI polling timer. No worker survives teardown. */
#ifndef pdPASS
#define pdPASS 1
#endif
static inline int xTaskCreate(void (*entry)(void *),const char *name,unsigned stack,void *data,unsigned priority,void *handle){(void)name;(void)stack;(void)priority;(void)handle;entry(data);return pdPASS;}
static inline void vTaskDelete(void *task){(void)task;}
