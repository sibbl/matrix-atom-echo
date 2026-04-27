#include "bridge_url.h"

#include <stdio.h>

bridge_url_status_t bridge_url_build(
    char *buffer,
    size_t buffer_size,
    const app_config_t *config,
    bridge_route_t route)
{
    int written = 0;

    if (buffer == NULL || buffer_size == 0U || config == NULL) {
        return BRIDGE_URL_STATUS_INVALID_ARGUMENT;
    }

    switch (route) {
    case BRIDGE_ROUTE_HEALTH:
        written = snprintf(buffer, buffer_size, "%s/health", config->bridge_base_url);
        break;
    case BRIDGE_ROUTE_RECORDING:
        written = snprintf(
            buffer,
            buffer_size,
            "%s/devices/%s/recording",
            config->bridge_base_url,
            config->device_id);
        break;
    case BRIDGE_ROUTE_NEXT_AUDIO:
        written = snprintf(
            buffer,
            buffer_size,
            "%s/devices/%s/next-audio",
            config->bridge_base_url,
            config->device_id);
        break;
    case BRIDGE_ROUTE_REPLAY_LAST_AUDIO:
        written = snprintf(
            buffer,
            buffer_size,
            "%s/devices/%s/replay-last-audio",
            config->bridge_base_url,
            config->device_id);
        break;
    case BRIDGE_ROUTE_THREAD_RESET:
        written = snprintf(
            buffer,
            buffer_size,
            "%s/devices/%s/thread/reset",
            config->bridge_base_url,
            config->device_id);
        break;
    default:
        return BRIDGE_URL_STATUS_INVALID_ARGUMENT;
    }

    if (written < 0) {
        return BRIDGE_URL_STATUS_INVALID_ARGUMENT;
    }

    if ((size_t) written >= buffer_size) {
        return BRIDGE_URL_STATUS_OVERFLOW;
    }

    return BRIDGE_URL_STATUS_OK;
}
