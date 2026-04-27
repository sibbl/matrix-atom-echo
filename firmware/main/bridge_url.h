#ifndef BRIDGE_URL_H
#define BRIDGE_URL_H

#include <stddef.h>

#include "app_config.h"

typedef enum {
    BRIDGE_ROUTE_HEALTH = 0,
    BRIDGE_ROUTE_RECORDING,
    BRIDGE_ROUTE_NEXT_AUDIO,
    BRIDGE_ROUTE_REPLAY_LAST_AUDIO,
    BRIDGE_ROUTE_THREAD_RESET
} bridge_route_t;

typedef enum {
    BRIDGE_URL_STATUS_OK = 0,
    BRIDGE_URL_STATUS_INVALID_ARGUMENT,
    BRIDGE_URL_STATUS_OVERFLOW
} bridge_url_status_t;

bridge_url_status_t bridge_url_build(
    char *buffer,
    size_t buffer_size,
    const app_config_t *config,
    bridge_route_t route);

#endif
