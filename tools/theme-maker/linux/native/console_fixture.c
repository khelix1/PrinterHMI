/* Bounded, process-local console fixtures. No printer transport. */
#include "console_controller.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
static console_entry_t entries[CONSOLE_LOG_CAPACITY];
static char history[CONSOLE_HISTORY_CAPACITY][CONSOLE_COMMAND_MAX+1];
static size_t count,history_count;
static uint32_t sequence;
bool console_controller_init(void){console_controller_add(CONSOLE_ENTRY_SYSTEM,"Offline tablet simulation; commands stay on this computer.");console_controller_add(CONSOLE_ENTRY_RESPONSE,"Klipper ready (simulated)");console_controller_add(CONSOLE_ENTRY_RESPONSE,"T:210.0 /210.0 B:60.0 /60.0");return true;}
void console_controller_add(console_entry_type_t type,const char *format,...){
 if(count<CONSOLE_LOG_CAPACITY)count++;
 memmove(entries+1,entries,(count-1)*sizeof(*entries));
 entries[0]=(console_entry_t){.sequence=++sequence,.uptime_seconds=sequence,.type=type};
 va_list args;va_start(args,format);vsnprintf(entries[0].message,sizeof(entries[0].message),format,args);va_end(args);
}
void console_controller_add_command(const char *command){
 if(history_count<CONSOLE_HISTORY_CAPACITY)history_count++;
 memmove(history+1,history,(history_count-1)*sizeof(*history));snprintf(history[0],sizeof(history[0]),"%s",command);
 console_controller_add(CONSOLE_ENTRY_COMMAND,"%s",command);
}
size_t console_controller_count(void){return count;}
uint32_t console_controller_latest_sequence(void){return sequence;}
bool console_controller_get(size_t i,console_entry_t *out){if(i>=count||!out)return false;*out=entries[i];return true;}
size_t console_controller_history_count(void){return history_count;}
bool console_controller_history_get(size_t i,char *out,size_t size){if(i>=history_count||!out||!size)return false;snprintf(out,size,"%s",history[i]);return true;}
void console_controller_clear(void){count=0;sequence++;}
