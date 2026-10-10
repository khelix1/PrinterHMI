#pragma once
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
    /* Firmware recovery also has a dedicated endpoint usable after shutdown.
     * Only redirect a standalone restart; preserve ordinary multi-line scripts. */
    if(moonraker_command_token_is(script,"FIRMWARE_RESTART")) {
        while(*script==' ' || *script=='\t' || *script=='\r')script++;
        script+=strlen("FIRMWARE_RESTART");
        while(isspace((unsigned char)*script))script++;
        if(!*script || (*script==';' && !strchr(script,'\n')))return "printer.firmware_restart";
    }
    return NULL;
}
