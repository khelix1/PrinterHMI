#pragma once

#include <stdbool.h>

void demo_mode_controller_init(void);
bool demo_mode_controller_enabled(void);
void demo_mode_controller_tick(void);
bool demo_mode_controller_handle_command(const char *command);
void demo_mode_controller_start_file(const char *file);
