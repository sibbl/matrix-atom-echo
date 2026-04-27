#include <assert.h>
#include <string.h>

#include "app_config.h"

int main(void)
{
    app_config_t config;

    assert(app_config_load_defaults(&config) == APP_CONFIG_STATUS_OK);
    assert(strcmp(config.bridge_base_url, "http://localhost:3000") == 0);
    assert(strcmp(config.device_id, "default") == 0);
    assert(!app_config_has_wifi_credentials(&config));
    assert(!app_config_has_device_auth_token(&config));

    assert(app_config_set_bridge_base_url(&config, " https://bridge.local:3000/ ") == APP_CONFIG_STATUS_OK);
    assert(strcmp(config.bridge_base_url, "https://bridge.local:3000") == 0);
    assert(app_config_set_bridge_base_url(&config, "not-a-url") == APP_CONFIG_STATUS_INVALID_ARGUMENT);

    assert(app_config_set_device_id(&config, "device-1") == APP_CONFIG_STATUS_OK);
    assert(strcmp(config.device_id, "device-1") == 0);
    assert(app_config_set_device_id(&config, "bad id") == APP_CONFIG_STATUS_INVALID_ARGUMENT);

    assert(app_config_set_wifi_ssid(&config, "MyWifi") == APP_CONFIG_STATUS_OK);
    assert(app_config_set_wifi_password(&config, "secret") == APP_CONFIG_STATUS_OK);
    assert(app_config_has_wifi_credentials(&config));
    assert(app_config_set_device_auth_token(&config, "device-token") == APP_CONFIG_STATUS_OK);
    assert(app_config_has_device_auth_token(&config));

    return 0;
}
