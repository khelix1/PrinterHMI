#include "case_lighting.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FIRST_REQUEST 700000000U
static case_lighting_snapshot_t s_light;
static uint32_t s_request=FIRST_REQUEST;
static portMUX_TYPE s_light_lock=portMUX_INITIALIZER_UNLOCKED;

void case_lighting_reset(uint32_t generation)
{
    portENTER_CRITICAL(&s_light_lock);
    memset(&s_light,0,sizeof(s_light));s_light.generation=generation;
    portEXIT_CRITICAL(&s_light_lock);
}
void case_lighting_objects(const cJSON *objects,uint32_t generation)
{
    if(!cJSON_IsArray(objects))return;
    bool found=false;const cJSON *entry;
    cJSON_ArrayForEach(entry,objects)
        if(cJSON_IsString(entry)&&!strcmp(entry->valuestring,"output_pin case_led"))found=true;
    portENTER_CRITICAL(&s_light_lock);
    if(s_light.generation==generation){s_light.discovered=true;s_light.available=found;}
    portEXIT_CRITICAL(&s_light_lock);
}
static bool config_number(const cJSON *entry,double *number)
{
    if(cJSON_IsNumber(entry))*number=entry->valuedouble;
    else if(cJSON_IsString(entry)){
        char *end;*number=strtod(entry->valuestring,&end);
        if(end==entry->valuestring)return false;
        while(isspace((unsigned char)*end))++end;
        if(*end)return false;
    }else return false;
    return isfinite(*number)&&*number>0;
}
void case_lighting_config(const cJSON *config,uint32_t generation)
{
    if(!cJSON_IsObject(config))return;
    const cJSON *pin=cJSON_GetObjectItemCaseSensitive(config,"output_pin case_led");
    const cJSON *pwm=cJSON_GetObjectItemCaseSensitive(pin,"pwm");
    bool valid=cJSON_IsObject(pin),is_pwm=false;
    if(pwm){
        if(cJSON_IsBool(pwm))is_pwm=cJSON_IsTrue(pwm);
        else if(cJSON_IsString(pwm)){
            char text[8];size_t n=strlen(pwm->valuestring);
            if(n>=sizeof(text))valid=false;
            else {
                for(size_t i=0;i<=n;i++)text[i]=(char)tolower((unsigned char)pwm->valuestring[i]);
                if(!strcmp(text,"true")||!strcmp(text,"yes")||!strcmp(text,"on")||!strcmp(text,"1"))is_pwm=true;
                else if(strcmp(text,"false")&&strcmp(text,"no")&&strcmp(text,"off")&&strcmp(text,"0"))valid=false;
            }
        }else valid=false;
    }
    double scale=1;
    const cJSON *value=cJSON_GetObjectItemCaseSensitive(pin,"scale");
    if(is_pwm&&value&&!config_number(value,&scale))valid=false;
    portENTER_CRITICAL(&s_light_lock);
    if(s_light.generation==generation){s_light.configured=valid;s_light.pwm=is_pwm;s_light.scale=scale;}
    portEXIT_CRITICAL(&s_light_lock);
}
void case_lighting_status_json(const char *json,size_t length,uint32_t generation)
{
    /* Avoid reparsing unrelated high-rate temperature/position messages. */
    static const char key[]="output_pin case_led";
    if(!json||length<sizeof(key)-1)return;
    bool mentions=false;
    for(size_t i=0;i<=length-(sizeof(key)-1);i++)
        if(!memcmp(json+i,key,sizeof(key)-1)){mentions=true;break;}
    if(!mentions)return;
    cJSON *root=cJSON_ParseWithLength(json,length);if(!root)return;
    const cJSON *result=cJSON_GetObjectItemCaseSensitive(root,"result");
    const cJSON *status=cJSON_GetObjectItemCaseSensitive(result,"status");
    const cJSON *method=cJSON_GetObjectItemCaseSensitive(root,"method");
    if(cJSON_IsString(method)&&!strcmp(method->valuestring,"notify_status_update"))
        status=cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(root,"params"),0);
    const cJSON *pin=cJSON_GetObjectItemCaseSensitive(status,"output_pin case_led");
    const cJSON *value=cJSON_GetObjectItemCaseSensitive(pin,"value");
    if(value){
        bool valid=cJSON_IsNumber(value)&&isfinite(value->valuedouble)&&value->valuedouble>=0&&value->valuedouble<=1;
        portENTER_CRITICAL(&s_light_lock);
        if(s_light.generation==generation){
            s_light.known=valid;
            if(valid){
                s_light.value=value->valuedouble;
                if(s_light.pending&&fabs(s_light.value-s_light.target/100.0)<0.0051){
                    s_light.pending=false;s_light.error[0]=0;
                }
            }
        }
        portEXIT_CRITICAL(&s_light_lock);
    }
    cJSON_Delete(root);
}
bool case_lighting_response(const cJSON *root,uint32_t generation)
{
    const cJSON *id=cJSON_GetObjectItemCaseSensitive(root,"id");
    if(!cJSON_IsNumber(id)||id->valuedouble<FIRST_REQUEST||id->valuedouble>=800000000||floor(id->valuedouble)!=id->valuedouble)return false;
    uint32_t request=(uint32_t)id->valuedouble;
    if(cJSON_GetObjectItemCaseSensitive(root,"error"))case_lighting_failed(request,generation);
    return true;
}
void case_lighting_snapshot(case_lighting_snapshot_t *out,int64_t now_us)
{
    if(!out)return;
    portENTER_CRITICAL(&s_light_lock);
    if(s_light.pending&&now_us-s_light.started_us>=5000000LL){
        s_light.pending=false;snprintf(s_light.error,sizeof(s_light.error),"No live confirmation. Check the printer and retry.");
    }
    *out=s_light;portEXIT_CRITICAL(&s_light_lock);
}
uint32_t case_lighting_begin(uint32_t generation,int percent,int64_t now_us,char *command,size_t size)
{
    if(!command||!size)return 0;
    command[0]=0;uint32_t request=0;
    portENTER_CRITICAL(&s_light_lock);
    if(s_light.generation==generation&&s_light.discovered&&s_light.available&&s_light.configured&&s_light.known&&!s_light.pending&&percent>=0&&percent<=100&&(s_light.pwm||percent==0||percent==100)){
        int n=snprintf(command,size,"SET_PIN PIN=case_led VALUE=%.12g",percent/100.0*s_light.scale);
        if(n>0&&(size_t)n<size){
            if(++s_request>=800000000U)s_request=FIRST_REQUEST+1;
            request=s_request;s_light.request=request;s_light.pending=true;s_light.target=percent;
            s_light.started_us=now_us;s_light.error[0]=0;
        }else command[0]=0;
    }
    portEXIT_CRITICAL(&s_light_lock);return request;
}
void case_lighting_failed(uint32_t request,uint32_t generation)
{
    portENTER_CRITICAL(&s_light_lock);
    if(s_light.generation==generation&&s_light.pending&&s_light.request==request){
        s_light.pending=false;snprintf(s_light.error,sizeof(s_light.error),"Lighting command failed. Retry when connected.");
    }
    portEXIT_CRITICAL(&s_light_lock);
}
