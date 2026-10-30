#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include "case_lighting.h"
#include "cJSON.h"
static unsigned lock_depth;
static void (*format_hook)(void);
void lighting_test_enter(void){assert(!lock_depth);lock_depth++;}
void lighting_test_exit(void){assert(lock_depth==1);lock_depth--;}
static int model_snprintf(char *buffer,size_t size,const char *format,...)
{
 assert(!lock_depth);
 va_list args;va_start(args,format);int n=vsnprintf(buffer,size,format,args);va_end(args);
 if(format_hook){void(*hook)(void)=format_hook;format_hook=NULL;hook();}
 return n;
}
#define snprintf model_snprintf
#include "case_lighting.c"
#undef snprintf
static void setup(void)
{
 case_lighting_reset(1);cJSON *r=cJSON_Parse("[\"output_pin case_led\"]");case_lighting_objects(r,1);cJSON_Delete(r);
 r=cJSON_Parse("{\"output_pin case_led\":{\"pwm\":true}}");case_lighting_config(r,1);cJSON_Delete(r);
 const char *s="{\"result\":{\"status\":{\"output_pin case_led\":{\"value\":0}}}}";case_lighting_status_json(s,strlen(s),1);
}
static void reset_during_format(void){case_lighting_reset(1);}
static void config_during_format(void){cJSON*r=cJSON_Parse("{\"output_pin case_led\":{\"pwm\":true,\"scale\":255}}");case_lighting_config(r,1);cJSON_Delete(r);}
static void competing_command(void){char cmd[96];assert(case_lighting_begin(1,30,1,cmd,sizeof(cmd)));}
int main(void)
{
 char cmd[96];case_lighting_snapshot_t s;
 setup();assert(case_lighting_begin(1,50,0,cmd,sizeof(cmd))&&!lock_depth);
 setup();format_hook=reset_during_format;assert(!case_lighting_begin(1,50,0,cmd,sizeof(cmd))&&!cmd[0]);
 setup();format_hook=config_during_format;assert(!case_lighting_begin(1,50,0,cmd,sizeof(cmd))&&!cmd[0]);
 setup();format_hook=competing_command;assert(!case_lighting_begin(1,50,0,cmd,sizeof(cmd))&&!cmd[0]);case_lighting_snapshot(&s,2);assert(s.pending&&s.target==30&&!lock_depth);
 puts("PASS: formatting outside critical sections, reset/config revision fences and competing command serialization");
}
