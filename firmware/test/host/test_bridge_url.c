#include <assert.h>
#include <string.h>

#include "app_config.h"
#include "bridge_url.h"

int main(void)
{
    app_config_t config;
    char url[256];

    assert(app_config_load_defaults(&config) == APP_CONFIG_STATUS_OK);
    assert(app_config_set_bridge_base_url(&config, "http://bridge.local:3000") == APP_CONFIG_STATUS_OK);
    assert(app_config_set_device_id(&config, "demo-device") == APP_CONFIG_STATUS_OK);

    assert(bridge_url_build(url, sizeof(url), &config, BRIDGE_ROUTE_HEALTH) == BRIDGE_URL_STATUS_OK);
    assert(strcmp(url, "http://bridge.local:3000/health") == 0);

    assert(bridge_url_build(url, sizeof(url), &config, BRIDGE_ROUTE_RECORDING) == BRIDGE_URL_STATUS_OK);
    assert(strcmp(url, "http://bridge.local:3000/devices/demo-device/recording") == 0);

    assert(bridge_url_build(url, sizeof(url), &config, BRIDGE_ROUTE_NEXT_AUDIO) == BRIDGE_URL_STATUS_OK);
    assert(strcmp(url, "http://bridge.local:3000/devices/demo-device/next-audio") == 0);

    assert(bridge_url_build(url, sizeof(url), &config, BRIDGE_ROUTE_REPLAY_LAST_AUDIO) == BRIDGE_URL_STATUS_OK);
    assert(strcmp(url, "http://bridge.local:3000/devices/demo-device/replay-last-audio") == 0);

    assert(bridge_url_build(url, sizeof(url), &config, BRIDGE_ROUTE_THREAD_RESET) == BRIDGE_URL_STATUS_OK);
    assert(strcmp(url, "http://bridge.local:3000/devices/demo-device/thread/reset") == 0);

    return 0;
}
