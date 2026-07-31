#include "network_activity_controller.h"

#include <stdint.h>

#define NETWORK_EXCLUSIVE_BIT UINT32_C(0x80000000)
#define NETWORK_SHARED_MASK   UINT32_C(0x7fffffff)

static uint32_t s_activity_state;
static bool s_persistent_quiet;


void network_activity_controller_request_exclusive(void)
{
    __atomic_store_n(&s_persistent_quiet, false, __ATOMIC_RELEASE);
    __atomic_fetch_or(
        &s_activity_state,
        NETWORK_EXCLUSIVE_BIT,
        __ATOMIC_ACQ_REL);
}


void network_activity_controller_release_exclusive(void)
{
    __atomic_fetch_and(
        &s_activity_state,
        NETWORK_SHARED_MASK,
        __ATOMIC_ACQ_REL);
    __atomic_store_n(&s_persistent_quiet, false, __ATOMIC_RELEASE);
}


bool network_activity_controller_exclusive_requested(void)
{
    return (__atomic_load_n(
        &s_activity_state,
        __ATOMIC_ACQUIRE) & NETWORK_EXCLUSIVE_BIT) != 0;
}


bool network_activity_controller_try_begin_shared(void)
{
    uint32_t current = __atomic_load_n(
        &s_activity_state,
        __ATOMIC_ACQUIRE);

    while ((current & NETWORK_EXCLUSIVE_BIT) == 0 &&
           (current & NETWORK_SHARED_MASK) != NETWORK_SHARED_MASK) {
        if (__atomic_compare_exchange_n(
                &s_activity_state,
                &current,
                current + 1,
                false,
                __ATOMIC_ACQ_REL,
                __ATOMIC_ACQUIRE)) {
            return true;
        }
    }

    return false;
}


void network_activity_controller_end_shared(void)
{
    __atomic_fetch_sub(&s_activity_state, 1, __ATOMIC_ACQ_REL);
}


void network_activity_controller_set_persistent_quiet(bool quiet)
{
    __atomic_store_n(&s_persistent_quiet, quiet, __ATOMIC_RELEASE);
}


bool network_activity_controller_exclusive_ready(void)
{
    uint32_t state = __atomic_load_n(
        &s_activity_state,
        __ATOMIC_ACQUIRE);

    return (state & NETWORK_EXCLUSIVE_BIT) != 0 &&
        (state & NETWORK_SHARED_MASK) == 0 &&
        __atomic_load_n(&s_persistent_quiet, __ATOMIC_ACQUIRE);
}
