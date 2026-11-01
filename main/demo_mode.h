#pragma once
#include <stdbool.h>
#include <stdint.h>
bool demo_mode_enabled(void);
bool demo_mode_select(bool enabled);
bool demo_printer_init(void);
void demo_printer_tick(void);
bool demo_printer_command(const char *command);
bool demo_printer_start_file(const char *path);
bool demo_printer_scenario(const char *scenario);
bool demo_printer_light(uint32_t generation, int percent);
bool demo_printer_endstops(uint32_t request);
bool demo_printer_calibration_step(double amount);
void ui_demo_mode_show(void);
