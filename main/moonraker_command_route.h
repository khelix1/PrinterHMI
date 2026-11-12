#pragma once
#include "printer_command_limits.h"
#include <ctype.h>
#include <stddef.h>
#include <string.h>

/* Emergency commands must bypass Klipper's ordinary G-code queue, including
 * when entered through Console. Never match a token inside a quoted argument. */
static inline int moonraker_command_token_is(const char *line,const char *token)
{
    while(*line==' ' || *line=='\t' || *line=='\r')line++;
    size_t n=strlen(token);
    for(size_t i=0;i<n;i++) {
        if(!line[i] || toupper((unsigned char)line[i])!=token[i])return 0;
    }
    return !line[n] || isspace((unsigned char)line[n]) || line[n]==';';
}

static inline const char *moonraker_command_admin_method(const char *script)
{
    if(!script)return NULL;
    for(const char *line=script;*line;) {
        if(moonraker_command_token_is(line,"M112"))return "printer.emergency_stop";
        const char *next=strchr(line,'\n');
        if(!next)break;
        line=next+1;
    }
    /* Recovery has dedicated endpoints that handle the expected disconnect.
     * Redirect only one restart command, allowing surrounding blank/comment
     * lines; preserve scripts containing parameters or another real command. */
    const char *method = NULL;
    for (const char *line = script; *line;) {
        while (*line == ' ' || *line == '\t' || *line == '\r') ++line;
        if (*line && *line != '\n' && *line != ';') {
            if (method) return NULL;
            size_t length;
            if (moonraker_command_token_is(line, "FIRMWARE_RESTART")) {
                method = "printer.firmware_restart";
                length = strlen("FIRMWARE_RESTART");
            } else if (moonraker_command_token_is(line, "RESTART")) {
                method = "printer.restart";
                length = strlen("RESTART");
            } else return NULL;
            const char *tail = line + length;
            while (*tail == ' ' || *tail == '\t' || *tail == '\r') ++tail;
            if (*tail && *tail != '\n' && *tail != ';') return NULL;
        }
        const char *next = strchr(line, '\n');
        if (!next) break;
        line = next + 1;
    }
    return method;
}
