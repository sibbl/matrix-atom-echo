#include "app_config.h"

#include <ctype.h>
#include <string.h>

#ifndef CONFIG_MATRIX_ATOM_ECHO_BRIDGE_BASE_URL
#define CONFIG_MATRIX_ATOM_ECHO_BRIDGE_BASE_URL "http://localhost:3000"
#endif

#ifndef CONFIG_MATRIX_ATOM_ECHO_DEVICE_ID
#define CONFIG_MATRIX_ATOM_ECHO_DEVICE_ID "default"
#endif

#ifndef CONFIG_MATRIX_ATOM_ECHO_WIFI_SSID
#define CONFIG_MATRIX_ATOM_ECHO_WIFI_SSID ""
#endif

#ifndef CONFIG_MATRIX_ATOM_ECHO_WIFI_PASSWORD
#define CONFIG_MATRIX_ATOM_ECHO_WIFI_PASSWORD ""
#endif

#ifndef CONFIG_MATRIX_ATOM_ECHO_POLL_INTERVAL_MS
#define CONFIG_MATRIX_ATOM_ECHO_POLL_INTERVAL_MS 3000
#endif

#ifndef CONFIG_MATRIX_ATOM_ECHO_HEALTH_INTERVAL_MS
#define CONFIG_MATRIX_ATOM_ECHO_HEALTH_INTERVAL_MS 15000
#endif

#ifndef CONFIG_MATRIX_ATOM_ECHO_HTTP_TIMEOUT_MS
#define CONFIG_MATRIX_ATOM_ECHO_HTTP_TIMEOUT_MS 5000
#endif

#ifndef CONFIG_MATRIX_ATOM_ECHO_BUTTON_POLL_MS
#define CONFIG_MATRIX_ATOM_ECHO_BUTTON_POLL_MS 25
#endif

#ifndef CONFIG_MATRIX_ATOM_ECHO_MAX_AUDIO_BYTES
#define CONFIG_MATRIX_ATOM_ECHO_MAX_AUDIO_BYTES 65536
#endif

#ifndef CONFIG_MATRIX_ATOM_ECHO_MAX_RECORDING_MS
#define CONFIG_MATRIX_ATOM_ECHO_MAX_RECORDING_MS 3000
#endif

#ifndef CONFIG_MATRIX_ATOM_ECHO_LONG_PRESS_MS
#define CONFIG_MATRIX_ATOM_ECHO_LONG_PRESS_MS 700
#endif

#ifndef CONFIG_MATRIX_ATOM_ECHO_DOUBLE_PRESS_GAP_MS
#define CONFIG_MATRIX_ATOM_ECHO_DOUBLE_PRESS_GAP_MS 250
#endif

#ifndef CONFIG_MATRIX_ATOM_ECHO_WIFI_CONNECT_TIMEOUT_MS
#define CONFIG_MATRIX_ATOM_ECHO_WIFI_CONNECT_TIMEOUT_MS 20000
#endif

#ifndef CONFIG_MATRIX_ATOM_ECHO_WIFI_MAX_RETRIES
#define CONFIG_MATRIX_ATOM_ECHO_WIFI_MAX_RETRIES 5
#endif

#ifndef CONFIG_MATRIX_ATOM_ECHO_BUTTON_GPIO
#define CONFIG_MATRIX_ATOM_ECHO_BUTTON_GPIO 0
#endif

static app_config_status_t copy_trimmed(
    char *destination,
    size_t destination_size,
    const char *source,
    bool allow_empty)
{
    size_t start_index = 0U;
    size_t end_index = 0U;
    size_t source_length = 0U;
    size_t output_length = 0U;

    if (destination == NULL || destination_size == 0U || source == NULL) {
        return APP_CONFIG_STATUS_INVALID_ARGUMENT;
    }

    source_length = strlen(source);
    end_index = source_length;

    while (start_index < source_length && isspace((unsigned char) source[start_index])) {
        start_index += 1U;
    }

    while (end_index > start_index && isspace((unsigned char) source[end_index - 1U])) {
        end_index -= 1U;
    }

    output_length = end_index - start_index;

    if (output_length == 0U && !allow_empty) {
        return APP_CONFIG_STATUS_INVALID_ARGUMENT;
    }

    if (output_length >= destination_size) {
        return APP_CONFIG_STATUS_OVERFLOW;
    }

    if (output_length > 0U) {
        memcpy(destination, source + start_index, output_length);
    }
    destination[output_length] = '\0';

    return APP_CONFIG_STATUS_OK;
}

static bool is_valid_url_scheme(const char *value)
{
    return strncmp(value, "http://", 7U) == 0 || strncmp(value, "https://", 8U) == 0;
}

static void strip_trailing_slash(char *value)
{
    size_t length = strlen(value);

    while (length > 0U && value[length - 1U] == '/') {
        value[length - 1U] = '\0';
        length -= 1U;
    }
}

static bool is_valid_device_id_char(char value)
{
    return isalnum((unsigned char) value) || value == '-' || value == '_' || value == '.';
}

app_config_status_t app_config_set_bridge_base_url(app_config_t *config, const char *value)
{
    app_config_status_t status;
    size_t index = 0U;

    if (config == NULL) {
        return APP_CONFIG_STATUS_INVALID_ARGUMENT;
    }

    status = copy_trimmed(config->bridge_base_url, sizeof(config->bridge_base_url), value, false);
    if (status != APP_CONFIG_STATUS_OK) {
        return status;
    }

    strip_trailing_slash(config->bridge_base_url);

    if (!is_valid_url_scheme(config->bridge_base_url)) {
        return APP_CONFIG_STATUS_INVALID_ARGUMENT;
    }

    for (index = 0U; config->bridge_base_url[index] != '\0'; index += 1U) {
        if (isspace((unsigned char) config->bridge_base_url[index])) {
            return APP_CONFIG_STATUS_INVALID_ARGUMENT;
        }
    }

    return APP_CONFIG_STATUS_OK;
}

app_config_status_t app_config_set_device_id(app_config_t *config, const char *value)
{
    app_config_status_t status;
    size_t index = 0U;

    if (config == NULL) {
        return APP_CONFIG_STATUS_INVALID_ARGUMENT;
    }

    status = copy_trimmed(config->device_id, sizeof(config->device_id), value, false);
    if (status != APP_CONFIG_STATUS_OK) {
        return status;
    }

    for (index = 0U; config->device_id[index] != '\0'; index += 1U) {
        if (!is_valid_device_id_char(config->device_id[index])) {
            return APP_CONFIG_STATUS_INVALID_ARGUMENT;
        }
    }

    return APP_CONFIG_STATUS_OK;
}

app_config_status_t app_config_set_wifi_ssid(app_config_t *config, const char *value)
{
    if (config == NULL) {
        return APP_CONFIG_STATUS_INVALID_ARGUMENT;
    }

    return copy_trimmed(config->wifi_ssid, sizeof(config->wifi_ssid), value, true);
}

app_config_status_t app_config_set_wifi_password(app_config_t *config, const char *value)
{
    if (config == NULL) {
        return APP_CONFIG_STATUS_INVALID_ARGUMENT;
    }

    return copy_trimmed(config->wifi_password, sizeof(config->wifi_password), value, true);
}

app_config_status_t app_config_set_device_auth_token(app_config_t *config, const char *value)
{
    if (config == NULL) {
        return APP_CONFIG_STATUS_INVALID_ARGUMENT;
    }

    return copy_trimmed(config->device_auth_token, sizeof(config->device_auth_token), value, true);
}

bool app_config_has_wifi_credentials(const app_config_t *config)
{
    return config != NULL && config->wifi_ssid[0] != '\0';
}

bool app_config_has_device_auth_token(const app_config_t *config)
{
    return config != NULL && config->device_auth_token[0] != '\0';
}

app_config_status_t app_config_load_defaults(app_config_t *config)
{
    app_config_status_t status;

    if (config == NULL) {
        return APP_CONFIG_STATUS_INVALID_ARGUMENT;
    }

    memset(config, 0, sizeof(*config));

    config->poll_interval_ms = CONFIG_MATRIX_ATOM_ECHO_POLL_INTERVAL_MS;
    config->health_interval_ms = CONFIG_MATRIX_ATOM_ECHO_HEALTH_INTERVAL_MS;
    config->http_timeout_ms = CONFIG_MATRIX_ATOM_ECHO_HTTP_TIMEOUT_MS;
    config->button_poll_ms = CONFIG_MATRIX_ATOM_ECHO_BUTTON_POLL_MS;
    config->max_audio_bytes = CONFIG_MATRIX_ATOM_ECHO_MAX_AUDIO_BYTES;
    config->max_recording_ms = CONFIG_MATRIX_ATOM_ECHO_MAX_RECORDING_MS;
    config->long_press_threshold_ms = CONFIG_MATRIX_ATOM_ECHO_LONG_PRESS_MS;
    config->double_press_gap_ms = CONFIG_MATRIX_ATOM_ECHO_DOUBLE_PRESS_GAP_MS;
    config->wifi_connect_timeout_ms = CONFIG_MATRIX_ATOM_ECHO_WIFI_CONNECT_TIMEOUT_MS;
    config->wifi_max_retries = CONFIG_MATRIX_ATOM_ECHO_WIFI_MAX_RETRIES;
    config->button_gpio = CONFIG_MATRIX_ATOM_ECHO_BUTTON_GPIO;

    status = app_config_set_bridge_base_url(config, CONFIG_MATRIX_ATOM_ECHO_BRIDGE_BASE_URL);
    if (status != APP_CONFIG_STATUS_OK) {
        return status;
    }

    status = app_config_set_device_id(config, CONFIG_MATRIX_ATOM_ECHO_DEVICE_ID);
    if (status != APP_CONFIG_STATUS_OK) {
        return status;
    }

    status = app_config_set_wifi_ssid(config, CONFIG_MATRIX_ATOM_ECHO_WIFI_SSID);
    if (status != APP_CONFIG_STATUS_OK) {
        return status;
    }

    status = app_config_set_wifi_password(config, CONFIG_MATRIX_ATOM_ECHO_WIFI_PASSWORD);
    if (status != APP_CONFIG_STATUS_OK) {
        return status;
    }

    return app_config_set_device_auth_token(config, "");
}
