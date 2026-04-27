#ifndef BUTTON_CONTROLLER_H
#define BUTTON_CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BUTTON_EVENT_NONE = 0,
    BUTTON_EVENT_SINGLE_PRESS,
    BUTTON_EVENT_DOUBLE_PRESS,
    BUTTON_EVENT_LONG_PRESS
} button_event_t;

typedef struct {
    uint32_t long_press_threshold_ms;
    uint32_t double_press_gap_ms;
    bool awaiting_second_press;
    uint64_t last_release_time_ms;
} button_controller_t;

void button_controller_init(
    button_controller_t *controller,
    uint32_t long_press_threshold_ms,
    uint32_t double_press_gap_ms);

button_event_t button_controller_on_release(
    button_controller_t *controller,
    uint64_t release_time_ms,
    uint32_t press_duration_ms);

button_event_t button_controller_flush(
    button_controller_t *controller,
    uint64_t now_ms);

const char *button_event_name(button_event_t event);

#endif
