#include <stdbool.h>
static bool bsp_display_lock(unsigned timeout){(void)timeout;return true;}
static void bsp_display_unlock(void){}
/* Discovery UI/request ownership is shared; only its network worker is local. */
int host_discovery_mode(void);
#include "discovery_local.inc"
