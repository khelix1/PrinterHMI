#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "case_lighting.h"
#include "cJSON.h"
static unsigned s_generation=1,calls;
static bool subscribed=true,send_full=true;
static void *s_client=(void *)1;
static char captured[224];
static int64_t esp_timer_get_time(void){return 0;}
static bool moonraker_live_websocket_subscribed(void){return subscribed;}
static int esp_websocket_client_send_text(void *client,const char *text,int length,unsigned timeout)
{assert(client==s_client&&!timeout&&length<(int)sizeof(captured));memcpy(captured,text,length);captured[length]=0;calls++;return send_full?length:length-1;}
#include "lighting_transport.inc"
int main(void)
{
 case_lighting_reset(1);cJSON *r=cJSON_Parse("[\"output_pin case_led\"]");case_lighting_objects(r,1);cJSON_Delete(r);
 r=cJSON_Parse("{\"output_pin case_led\":{\"pwm\":true,\"scale\":255}}");case_lighting_config(r,1);cJSON_Delete(r);
 const char *json="{\"result\":{\"status\":{\"output_pin case_led\":{\"value\":0}}}}";case_lighting_status_json(json,strlen(json),1);
 assert(!moonraker_live_websocket_set_case_led(2,50)&&!calls);subscribed=false;assert(!moonraker_live_websocket_set_case_led(1,50)&&!calls);subscribed=true;
 assert(moonraker_live_websocket_set_case_led(1,50)&&calls==1);
 r=cJSON_Parse(captured);assert(r);const cJSON *method=cJSON_GetObjectItem(r,"method"),*params=cJSON_GetObjectItem(r,"params"),*script=cJSON_GetObjectItem(params,"script");assert(!strcmp(method->valuestring,"printer.gcode.script")&&!strcmp(script->valuestring,"SET_PIN PIN=case_led VALUE=127.5"));cJSON_Delete(r);
 assert(!moonraker_live_websocket_set_case_led(1,0)&&calls==1);
 case_lighting_snapshot_t state;case_lighting_snapshot(&state,5000001);send_full=false;assert(!moonraker_live_websocket_set_case_led(1,0)&&calls==2);case_lighting_snapshot(&state,5000001);assert(!state.pending&&state.error[0]);
 puts("PASS: actual nonblocking lighting WS adapter, exact scaled JSON, profile/offline/pending guards and short-send failure");
}
