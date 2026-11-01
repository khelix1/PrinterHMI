/* The status callback is assembled in main.c; compile that function verbatim
 * with an explicit local address instead of the embedded Wi-Fi address. */
#include <stdio.h>
#include "moonraker.h"
#include "ui_dashboard.h"
#define IPSTR "%d.%d.%d.%d"
#define IP2STR(address) 127,0,0,1
#include "drybox_status.inc"
void host_drybox_status(lv_event_t *event){dashboard_dry_status_event_cb(event);}
