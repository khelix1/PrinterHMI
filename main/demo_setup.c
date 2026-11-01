#include "demo_setup.h"
#include <string.h>
static char sample_ssid[33];
bool demo_setup_connect(const char *ssid, const char *password)
{
    (void)password;
    if (!ssid || !ssid[0] || strlen(ssid) >= sizeof(sample_ssid)) return false;
    memcpy(sample_ssid, ssid, strlen(ssid)+1);
    return true;
}
bool demo_setup_network(char *ssid, size_t size)
{
    if (!ssid || size == 0 || !sample_ssid[0] || strlen(sample_ssid) >= size) return false;
    memcpy(ssid, sample_ssid, strlen(sample_ssid)+1);
    return true;
}
