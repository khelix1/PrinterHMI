#pragma once

#include <stdbool.h>

/*
 * Coordinates short-lived exclusive network operations such as OTA.
 * Network producers remain responsible for observing this state from their
 * existing owner task; this module owns no task, timer, or transport handle.
 */
/* Returns false when another exclusive operation already owns the network. */
bool network_activity_controller_request_exclusive(void);
void network_activity_controller_release_exclusive(void);
bool network_activity_controller_exclusive_requested(void);

/* Shared HTTP operations drain before an exclusive owner may begin. */
bool network_activity_controller_try_begin_shared(void);
void network_activity_controller_end_shared(void);

/* The runtime owner reports when its persistent transport is fully retired. */
void network_activity_controller_set_persistent_quiet(bool quiet);
bool network_activity_controller_exclusive_ready(void);
