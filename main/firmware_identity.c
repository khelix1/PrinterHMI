#include "firmware_identity.h"
#include "esp_app_desc.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef PRINTERHMI_FIRMWARE_CHANNEL
#define PRINTERHMI_FIRMWARE_CHANNEL 0
#endif
#ifndef PRINTERHMI_SOURCE_REVISION
#define PRINTERHMI_SOURCE_REVISION "unknown"
#endif
#ifndef PRINTERHMI_SOURCE_MODIFIED
#define PRINTERHMI_SOURCE_MODIFIED 1
#endif
void firmware_identity_get(firmware_identity_t *identity)
{
    if (!identity) return;
    const esp_app_desc_t *app = esp_app_get_description();
    *identity = (firmware_identity_t){.channel=PRINTERHMI_FIRMWARE_CHANNEL,
                                     .modified=PRINTERHMI_SOURCE_MODIFIED};
    snprintf(identity->version, sizeof(identity->version), "%.*s",
             (int)sizeof(app->version) - 1,
             app && app->version[0] ? app->version : "unknown");
    snprintf(identity->revision, sizeof(identity->revision), "%s", PRINTERHMI_SOURCE_REVISION);
}
const char *firmware_identity_channel_name(firmware_channel_t channel)
{
    return channel == FIRMWARE_CHANNEL_STABLE ? "Stable" :
           channel == FIRMWARE_CHANNEL_NIGHTLY ? "Nightly" :
           channel == FIRMWARE_CHANNEL_DEVELOPMENT ? "Development" : "Custom";
}
void firmware_identity_format(char *out, size_t capacity)
{
    if (!out || !capacity) return;
    firmware_identity_t identity;
    firmware_identity_get(&identity);
    snprintf(out, capacity, "Installed: %s | %s | %s%s",
             firmware_identity_channel_name(identity.channel), identity.version,
             identity.revision, identity.modified ? " (modified)" : "");
}
static bool semver(const char *version, unsigned long parts[3], bool *development)
{
    if (!version || !development) return false;
    *development = false;
    if (*version=='v') ++version;
    for (unsigned i=0;i<3;i++) {
        if (!isdigit((unsigned char)*version)) return false;
        char *end;
        parts[i]=strtoul(version,&end,10);
        if (parts[i]>2147483647UL) return false;
        if (i<2) {if (*end!='.') return false;version=end+1;}
        else if (*end) {
            if (strcmp(end,"-dev")) return false;
            *development = true;
        }
    }
    return true;
}
static bool nightly_date(const char *version, char out[11])
{
    if (!version || strncmp(version,"nightly-",8) || strlen(version)<20) return false;
    const char *date=version+8;
    for (unsigned i=0;i<10;i++) {
        if (i==4 || i==7) {if(date[i]!='-') return false;}
        else if(!isdigit((unsigned char)date[i])) return false;
    }
    if(date[10]!='-') return false;
    for(const char *p=date+11;*p;p++)if(!isxdigit((unsigned char)*p))return false;
    memcpy(out,date,10);out[10]=0;return true;
}
int firmware_identity_compare(const firmware_identity_t *installed,
                             firmware_channel_t channel,const char *candidate)
{
    if(!installed || !candidate)return FIRMWARE_RELATION_OTHER_BUILD;
    if(channel==FIRMWARE_CHANNEL_STABLE) {
        unsigned long a[3],b[3];
        bool candidate_dev, installed_dev;
        if(installed->channel!=FIRMWARE_CHANNEL_STABLE)
            return FIRMWARE_RELATION_SWITCH_CHANNEL;
        if(semver(candidate,a,&candidate_dev) && !candidate_dev &&
           semver(installed->version,b,&installed_dev) && !installed_dev) {
            for(unsigned i=0;i<3;i++)if(a[i]!=b[i])return a[i]>b[i]?1:-1;
            return 0;
        }
    }
    if(channel==FIRMWARE_CHANNEL_NIGHTLY && installed->channel==channel) {
        char a[11],b[11];
        if(nightly_date(candidate,a) && nightly_date(installed->version,b)) {
            if(!strcmp(installed->version,candidate))return 0;
            int difference=strcmp(a,b);
            if(difference)return difference>0?1:-1;
            return FIRMWARE_RELATION_OTHER_BUILD; /* Same-day hashes have no ordering. */
        }
    }
    return FIRMWARE_RELATION_SWITCH_CHANNEL;
}
