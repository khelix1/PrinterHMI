/* In-memory Moonraker adapter. Real lighting parser and widgets stay shared. */
#include "case_lighting.h"
#include "moonraker.h"
#include "moonraker_config_controller.h"
#include "esp_timer.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>

static uint32_t owner;
static bool initialized;
static int fixture; /* 0 PWM, 1 absent, 2 digital, 3 send failure, 4 delayed reply */
static double values[MOONRAKER_CONFIG_MAX_PROFILES];
static uint32_t pending_owner;
static int pending_percent;
static int64_t confirm_at;
void host_lighting_command(const char *command);

static void status(double value)
{
    char json[160];
    int n=snprintf(json,sizeof(json),"{\"result\":{\"status\":{\"output_pin case_led\":{\"value\":%.8f}}}}",value);
    case_lighting_status_json(json,(size_t)n,owner);
}
void host_lighting_refresh(void)
{
    uint32_t generation=moonraker_config_generation();
    int index=moonraker_config_active_profile_index();
    if(!initialized||owner!=generation){
        initialized=true;owner=generation;confirm_at=0;case_lighting_reset(owner);
        /* The second sample printer has no case light. */
        bool present=fixture!=1&&index==0;
        cJSON *objects=cJSON_Parse(present?"[\"output_pin case_led\"]":"[]");
        case_lighting_objects(objects,owner);cJSON_Delete(objects);
        if(present){
            cJSON *config=cJSON_Parse(fixture==2?"{\"output_pin case_led\":{\"pwm\":false}}":"{\"output_pin case_led\":{\"pwm\":true}}");
            case_lighting_config(config,owner);cJSON_Delete(config);
            status(values[index]);
        }
    }
    if(confirm_at&&esp_timer_get_time()>=confirm_at){
        confirm_at=0;
        if(pending_owner==owner&&index>=0&&index<MOONRAKER_CONFIG_MAX_PROFILES){values[index]=pending_percent/100.0;status(values[index]);}
    }
}
void host_lighting_fixture(int mode){fixture=mode>=0&&mode<=4?mode:0;initialized=false;host_lighting_refresh();}
bool moonraker_live_websocket_subscribed(void)
{
    moonraker_state_t state;moonraker_state_snapshot(&state);
    return state.moonraker_ok&&state.live_data_ok;
}
bool moonraker_live_websocket_set_case_led(uint32_t generation,int percent)
{
    host_lighting_refresh();
    if(generation!=moonraker_config_generation()||!moonraker_live_websocket_subscribed())return false;
    char command[160];uint32_t request=case_lighting_begin(generation,percent,esp_timer_get_time(),command,sizeof(command));
    if(!request)return false;
    if(fixture==3){case_lighting_failed(request,generation);return false;}
    host_lighting_command(command);
    pending_owner=generation;pending_percent=percent;
    confirm_at=esp_timer_get_time()+(fixture==4?1000000:1);
    host_lighting_refresh();return true;
}

double host_lighting_value(void){int index=moonraker_config_active_profile_index();return index>=0 && index<MOONRAKER_CONFIG_MAX_PROFILES?values[index]:0;}
