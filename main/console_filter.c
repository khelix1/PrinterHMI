#include "console_filter.h"
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static bool contains(const char *text, const char *query)
{
    if (!query || !*query) return true;
    for (; *text; ++text) {
        const unsigned char *a = (const unsigned char *)text;
        const unsigned char *b = (const unsigned char *)query;
        while (*a && *b && tolower(*a) == tolower(*b)) { ++a; ++b; }
        if (!*b) return true;
    }
    return false;
}

static void spaces(const char **text)
{
    while (isspace((unsigned char)**text)) ++*text;
}

static bool numeric(const char **text)
{
    char *end;
    double value = strtod(*text, &end);
    if (end == *text || !isfinite(value)) return false;
    *text = end;
    return true;
}

/* Recognize complete numeric M105-style reports, not arbitrary heater messages. */
static bool temperature_report(const char *text)
{
    spaces(&text);
    if (!strncmp(text, "//", 2)) { text += 2; spaces(&text); }
    if (!strncmp(text, "ok", 2) && isspace((unsigned char)text[2])) {
        text += 2; spaces(&text);
    }
    bool temperature = false;
    while (*text) {
        bool heater = *text == 'T' || *text == 'B';
        if (heater) {
            ++text;
            while (isdigit((unsigned char)*text)) ++text;
        } else if (*text != '@') return false;
        bool power = *text == '@';
        if (power) ++text;
        if (*text++ != ':') return false;
        spaces(&text);
        if (!numeric(&text)) return false;
        if (heater && !power) temperature = true;
        const char *end = text;
        spaces(&text);
        if (*text == '/') {
            ++text; spaces(&text);
            if (!numeric(&text)) return false;
            end = text; spaces(&text);
        }
        if (*text && end == text) return false;
    }
    return temperature;
}

bool console_filter_matches(const console_entry_t *entry,
    console_filter_kind_t kind, const char *query, bool hide_temperatures)
{
    if (!entry) return false;
    bool type;
    switch (kind) {
    case CONSOLE_FILTER_ALL: type = true; break;
    case CONSOLE_FILTER_ALERTS: type = entry->type == CONSOLE_ENTRY_ERROR || entry->type == CONSOLE_ENTRY_WARNING; break;
    case CONSOLE_FILTER_ERRORS: type = entry->type == CONSOLE_ENTRY_ERROR; break;
    case CONSOLE_FILTER_WARNINGS: type = entry->type == CONSOLE_ENTRY_WARNING; break;
    case CONSOLE_FILTER_COMMANDS: type = entry->type == CONSOLE_ENTRY_COMMAND; break;
    case CONSOLE_FILTER_RESPONSES: type = entry->type == CONSOLE_ENTRY_RESPONSE; break;
    case CONSOLE_FILTER_SYSTEM: type = entry->type == CONSOLE_ENTRY_SYSTEM; break;
    default: return false;
    }
    return type && contains(entry->message, query) &&
        !(hide_temperatures && entry->type == CONSOLE_ENTRY_RESPONSE && temperature_report(entry->message));
}
