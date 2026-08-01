#include "network_activity_controller.h"

#include <stdint.h>

#define NETWORK_EXCLUSIVE_BIT UINT32_C(0x80000000)
#define NETWORK_SHARED_MASK   UINT32_C(0x7fffffff)

static uint32_t s_activity_state;
static bool s_persistent_quiet;


bool network_activity_controller_request_exclusive(void)
{
    uint32_t current = __atomic_load_n(
        &s_activity_state,
        __ATOMIC_ACQUIRE);

    while ((current & NETWORK_EXCLUSIVE_BIT) == 0) {
        uint32_t requested = current | NETWORK_EXCLUSIVE_BIT;

        if (__atomic_compare_exchange_n(
                &s_activity_state,
                &current,
                requested,
                false,
                __ATOMIC_ACQ_REL,
                __ATOMIC_ACQUIRE)) {
            return true;
        }
    }

    return false;
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

    /* ESP-Hosted SDIO is most reliable when short HTTP transactions do not
     * overlap. A shared owner may coexist with the persistent WebSocket, but
     * never with another shared HTTP owner.
     */
    while (current == 0) {
        if (__atomic_compare_exchange_n(
                &s_activity_state,
                &current,
                UINT32_C(1),
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
