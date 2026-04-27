#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define APP_CONFIG_BRIDGE_URL_MAX_LEN 192U
#define APP_CONFIG_DEVICE_ID_MAX_LEN 64U
#define APP_CONFIG_WIFI_SSID_MAX_LEN 33U
#define APP_CONFIG_WIFI_PASSWORD_MAX_LEN 65U
#define APP_CONFIG_DEVICE_AUTH_TOKEN_MAX_LEN 129U

typedef enum {
    APP_CONFIG_STATUS_OK = 0,
    APP_CONFIG_STATUS_INVALID_ARGUMENT,
    APP_CONFIG_STATUS_OVERFLOW
} app_config_status_t;

typedef struct {
    char bridge_base_url[APP_CONFIG_BRIDGE_URL_MAX_LEN];
    char device_id[APP_CONFIG_DEVICE_ID_MAX_LEN];
    char wifi_ssid[APP_CONFIG_WIFI_SSID_MAX_LEN];
    char wifi_password[APP_CONFIG_WIFI_PASSWORD_MAX_LEN];
    char device_auth_token[APP_CONFIG_DEVICE_AUTH_TOKEN_MAX_LEN];
    uint32_t poll_interval_ms;
    uint32_t health_interval_ms;
    uint32_t http_timeout_ms;
    uint32_t button_poll_ms;
    uint32_t max_audio_bytes;
    uint32_t max_recording_ms;
    uint32_t long_press_threshold_ms;
    uint32_t double_press_gap_ms;
    uint32_t wifi_connect_timeout_ms;
    uint32_t wifi_max_retries;
    int button_gpio;
} app_config_t;

app_config_status_t app_config_load_defaults(app_config_t *config);
app_config_status_t app_config_set_bridge_base_url(app_config_t *config, const char *value);
app_config_status_t app_config_set_device_id(app_config_t *config, const char *value);
app_config_status_t app_config_set_wifi_ssid(app_config_t *config, const char *value);
app_config_status_t app_config_set_wifi_password(app_config_t *config, const char *value);
app_config_status_t app_config_set_device_auth_token(app_config_t *config, const char *value);
bool app_config_has_wifi_credentials(const app_config_t *config);
bool app_config_has_device_auth_token(const app_config_t *config);

#endif
