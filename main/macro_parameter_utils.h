#pragma once
#include <stdbool.h>
#include <stddef.h>
#define MACRO_PARAMETER_MAX 8
#define MACRO_PARAMETER_NAME_MAX 32
#define MACRO_PARAMETER_VALUE_MAX 64
#define MACRO_PARAMETER_EXTRA 2
#define MACRO_COMMAND_MAX 640

typedef struct {
    size_t count;
    bool truncated;
    char names[MACRO_PARAMETER_MAX][MACRO_PARAMETER_NAME_MAX];
} macro_parameter_catalog_t;
typedef struct { const char *name; const char *value; } macro_parameter_value_t;
/* Recognizes params.NAME, params['NAME'], and params.get('NAME', ...).
 * Dynamic names and rawparams cannot be inferred; extra fields remain available. */
void macro_parameter_detect(const char *gcode, macro_parameter_catalog_t *out);
bool macro_parameter_build_command(const char *macro,
    const macro_parameter_value_t *values, size_t count,
    char *output, size_t size, const char **error);
bool macro_parameter_matches(const char *name, const char *query);
