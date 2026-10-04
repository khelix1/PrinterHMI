#include "macro_parameter_utils.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static bool identifier(const char *s)
{
    if (!s || !(isalpha((unsigned char)*s) || *s == '_')) return false;
    for (++s; *s; ++s) if (!(isalnum((unsigned char)*s) || *s == '_')) return false;
    return true;
}

static void add_parameter(macro_parameter_catalog_t *out, const char *start, size_t length)
{
    if (!length) return;
    if (length >= MACRO_PARAMETER_NAME_MAX) { out->truncated = true; return; }
    char name[MACRO_PARAMETER_NAME_MAX];
    for (size_t i = 0; i < length; ++i) name[i] = toupper((unsigned char)start[i]);
    name[length] = 0;
    if (!identifier(name)) return;
    for (size_t i = 0; i < out->count; ++i) if (!strcmp(out->names[i], name)) return;
    if (out->count == MACRO_PARAMETER_MAX) { out->truncated = true; return; }
    snprintf(out->names[out->count++], MACRO_PARAMETER_NAME_MAX, "%s", name);
}

void macro_parameter_detect(const char *gcode, macro_parameter_catalog_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!gcode) return;
    const char *cursor = gcode;
    while ((cursor = strstr(cursor, "params"))) {
        const char *found = cursor;
        cursor += 6;
        if (found != gcode && (isalnum((unsigned char)found[-1]) || found[-1] == '_')) continue;
        const char *p = cursor;
        char closing = 0;
        if (!strncmp(p, ".get(", 5)) {
            p += 5;
            while (isspace((unsigned char)*p)) ++p;
            if (*p != '\'' && *p != '"') continue;
            closing = *p++;
        } else if (*p == '[') {
            ++p;
            while (isspace((unsigned char)*p)) ++p;
            if (*p != '\'' && *p != '"') continue;
            closing = *p++;
        } else if (*p == '.') {
            ++p;
            if (!strncmp(p, "get", 3) && !(isalnum((unsigned char)p[3]) || p[3] == '_')) continue;
        } else continue;
        const char *start = p;
        while (isalnum((unsigned char)*p) || *p == '_') ++p;
        if (closing && *p != closing) continue;
        add_parameter(out, start, (size_t)(p - start));
    }
}

bool macro_parameter_matches(const char *name, const char *query)
{
    if (!name || !query) return false;
    if (!*query) return true;
    for (; *name; ++name) {
        const char *a = name, *b = query;
        while (*a && *b && toupper((unsigned char)*a) == toupper((unsigned char)*b)) { ++a; ++b; }
        if (!*b) return true;
    }
    return false;
}

bool macro_parameter_build_command(const char *macro,
    const macro_parameter_value_t *values, size_t count,
    char *output, size_t size, const char **error)
{
    if (error) *error = "Invalid macro or parameter name.";
    if (!output || !size) return false;
    output[0] = 0;
    if (!identifier(macro) || (count && !values)) return false;
    int written = snprintf(output, size, "%s", macro);
    if (written < 0 || (size_t)written >= size) goto overflow;
    size_t used = (size_t)written;
    for (size_t i = 0; i < count; ++i) {
        const char *value = values[i].value;
        const char *name = values[i].name;
        if (!value || !*value) continue; /* Omitted, letting Klipper apply any defaults. */
        if (!identifier(name) || strlen(name) >= MACRO_PARAMETER_NAME_MAX) goto invalid;
        for (size_t j = 0; j < i; ++j) {
            if (values[j].value && *values[j].value &&
                strlen(values[j].name) == strlen(name) &&
                macro_parameter_matches(name, values[j].name)) {
                if (error) *error = "Duplicate parameter names.";
                goto invalid;
            }
        }
        bool quote = false;
        for (const unsigned char *p = (const unsigned char *)value; *p; ++p) {
            if (*p < 32 || *p == 127 || *p == ';' || *p == '"' || *p == '\'' || *p == '\\') {
                if (error) *error = "Values cannot contain quotes, backslashes, semicolons or line breaks.";
                goto invalid;
            }
            if (isspace(*p)) quote = true;
        }
        char upper[MACRO_PARAMETER_NAME_MAX];
        size_t length = strlen(name);
        for (size_t k = 0; k < length; ++k) upper[k] = toupper((unsigned char)name[k]);
        upper[length] = 0;
        written = snprintf(output + used, size - used, " %s=%s%s%s",
                           upper, quote ? "\"" : "", value, quote ? "\"" : "");
        if (written < 0 || (size_t)written >= size - used) goto overflow;
        used += (size_t)written;
    }
    if (error) *error = NULL;
    return true;
overflow:
    if (error) *error = "Command is too long. Shorten the parameter values.";
invalid:
    output[0] = 0;
    return false;
}
