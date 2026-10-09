#pragma once
#include <stdbool.h>

/* The printer owns all extrusion, heating and restoration behavior. */
void ui_filament_recovery_show(bool (*send)(const char *command));
void ui_filament_recovery_close(void);
