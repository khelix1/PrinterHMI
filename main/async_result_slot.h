#pragma once

#include <stdbool.h>

/* One worker and one result consumer. Starting a new job may discard an unread
 * result, but must not overwrite a result while the consumer is copying it.
 * No task handle is used as a busy flag: a task can finish before creation returns. */
typedef enum {
    ASYNC_RESULT_IDLE, ASYNC_RESULT_WORKING, ASYNC_RESULT_READY, ASYNC_RESULT_READING
} async_result_slot_t;

static inline bool async_result_start(async_result_slot_t *slot)
{
    async_result_slot_t state = __atomic_load_n(slot, __ATOMIC_ACQUIRE);
    while (state == ASYNC_RESULT_IDLE || state == ASYNC_RESULT_READY) {
        if (__atomic_compare_exchange_n(slot, &state, ASYNC_RESULT_WORKING, false,
                                        __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) return true;
    }
    return false;
}

static inline void async_result_publish(async_result_slot_t *slot)
{
    __atomic_store_n(slot, ASYNC_RESULT_READY, __ATOMIC_RELEASE);
}

static inline bool async_result_take(async_result_slot_t *slot)
{
    async_result_slot_t expected = ASYNC_RESULT_READY;
    return __atomic_compare_exchange_n(slot, &expected, ASYNC_RESULT_READING, false,
                                       __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
}

static inline void async_result_release(async_result_slot_t *slot)
{
    __atomic_store_n(slot, ASYNC_RESULT_IDLE, __ATOMIC_RELEASE);
}

static inline bool async_result_busy(async_result_slot_t *slot)
{
    async_result_slot_t state = __atomic_load_n(slot, __ATOMIC_ACQUIRE);
    return state == ASYNC_RESULT_WORKING || state == ASYNC_RESULT_READING;
}
