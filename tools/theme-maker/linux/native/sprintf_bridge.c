/* Linux libc formatting includes the floating-point readings used by shared UI.
 * Avoid enabling LV_USE_FLOAT against an existing LVGL integer-coordinate ABI. */
#include <stdarg.h>
#include <stdio.h>
#include "lvgl.h"
int lv_vsnprintf(char *buffer,size_t size,const char *format,va_list args){return vsnprintf(buffer,size,format,args);}
int lv_snprintf(char *buffer,size_t size,const char *format,...){va_list args;va_start(args,format);int count=vsnprintf(buffer,size,format,args);va_end(args);return count;}
