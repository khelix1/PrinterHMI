#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "case_lighting.h"
#include "cJSON.h"
static void objects(const char *json,unsigned gen){cJSON*r=cJSON_Parse(json);assert(r);case_lighting_objects(r,gen);cJSON_Delete(r);}
static void config(const char *json,unsigned gen){cJSON*r=cJSON_Parse(json);assert(r);case_lighting_config(r,gen);cJSON_Delete(r);}
static void status(const char *json,unsigned gen){case_lighting_status_json(json,strlen(json),gen);}
static void setup(bool pwm,double scale,unsigned gen)
{
 case_lighting_reset(gen);objects("[\"output_pin other\",\"output_pin case_led\"]",gen);
 char json[180];snprintf(json,sizeof(json),"{\"output_pin case_led\":{\"pwm\":%s,\"scale\":%g}}",pwm?"true":"false",scale);config(json,gen);
 status("{\"result\":{\"status\":{\"output_pin case_led\":{\"value\":0.25}}}}",gen);
}
int main(void)
{
 char cmd[96];case_lighting_snapshot_t s;
 case_lighting_reset(1);assert(!case_lighting_begin(1,100,0,cmd,sizeof(cmd)));
 objects("[\"output_pin case_led2\"]",1);case_lighting_snapshot(&s,0);assert(s.discovered&&!s.available);
 setup(true,255,1);case_lighting_snapshot(&s,0);assert(s.known&&s.pwm&&s.scale==255&&s.value==.25);
 assert(!case_lighting_begin(2,50,0,cmd,sizeof(cmd)));assert(!case_lighting_begin(1,-1,0,cmd,sizeof(cmd)));assert(!case_lighting_begin(1,101,0,cmd,sizeof(cmd)));
 assert(!case_lighting_begin(1,50,0,cmd,4));
 unsigned id=case_lighting_begin(1,50,0,cmd,sizeof(cmd));assert(id&&!strcmp(cmd,"SET_PIN PIN=case_led VALUE=127.5"));assert(!case_lighting_begin(1,0,1,cmd,sizeof(cmd)));
 status("{\"method\":\"notify_status_update\",\"params\":[{\"output_pin case_led\":{\"value\":0.5}},1]}",2);case_lighting_snapshot(&s,1);assert(s.pending&&s.value==.25);
 status("{\"method\":\"notify_status_update\",\"params\":[{\"output_pin case_led\":{\"value\":0.5}},1]}",1);case_lighting_snapshot(&s,1);assert(!s.pending&&s.value==.5);
 id=case_lighting_begin(1,100,10,cmd,sizeof(cmd));case_lighting_failed(id-1,1);case_lighting_snapshot(&s,11);assert(s.pending);
 char response[100];snprintf(response,sizeof(response),"{\"id\":%u,\"error\":{\"message\":\"no\"}}",id);cJSON*r=cJSON_Parse(response);assert(case_lighting_response(r,1));cJSON_Delete(r);case_lighting_snapshot(&s,12);assert(!s.pending&&s.error[0]);
 id=case_lighting_begin(1,0,20,cmd,sizeof(cmd));assert(id);case_lighting_snapshot(&s,5000020);assert(!s.pending&&strstr(s.error,"confirmation"));
 setup(false,255,3);assert(!case_lighting_begin(3,50,0,cmd,sizeof(cmd)));assert(case_lighting_begin(3,100,0,cmd,sizeof(cmd))&&!strcmp(cmd,"SET_PIN PIN=case_led VALUE=1"));
 case_lighting_reset(3);case_lighting_snapshot(&s,0);assert(!s.known&&!s.configured&&!s.discovered&&!s.pending);
 setup(true,1,4);config("{\"output_pin case_led\":{\"pwm\":\"True\",\"scale\":\"100\"}}",4);assert(case_lighting_begin(4,1,0,cmd,sizeof(cmd))&&!strcmp(cmd,"SET_PIN PIN=case_led VALUE=1"));
 setup(true,1,5);config("{\"output_pin case_led\":{\"pwm\":\"nope\"}}",5);assert(!case_lighting_begin(5,1,0,cmd,sizeof(cmd)));
 setup(true,1,6);config("{\"output_pin case_led\":{\"pwm\":true,\"scale\":\"nan\"}}",6);assert(!case_lighting_begin(6,1,0,cmd,sizeof(cmd)));
 setup(true,1,7);status("{\"result\":{\"status\":{\"output_pin case_led\":{\"value\":2}}}}",7);case_lighting_snapshot(&s,0);assert(!s.known&&!case_lighting_begin(7,0,0,cmd,sizeof(cmd)));
 setup(true,0.000001,8);assert(case_lighting_begin(8,100,0,cmd,sizeof(cmd))&&!strcmp(cmd,"SET_PIN PIN=case_led VALUE=1e-06"));
 setup(true,1,9);status("{\"result\":{\"status\":{\"output_pin case_led\":{\"value\":0.004}}}}",9);
 assert(case_lighting_begin(9,0,0,cmd,sizeof(cmd)));status("{\"result\":{\"status\":{\"output_pin case_led\":{\"value\":0.004}}}}",9);case_lighting_snapshot(&s,1);assert(s.pending);
 status("{\"result\":{\"status\":{\"output_pin case_led\":{\"value\":0}}}}",9);case_lighting_snapshot(&s,2);assert(!s.pending);
 puts("PASS: exact output discovery, normalized/scaled PWM, digital guards, malformed config/status, pending serialization, stale generation/error rejection, timeout/retry and reset");
}
