#include "button_controller.h"

void button_controller_init(
    button_controller_t *controller,
    uint32_t long_press_threshold_ms,
    uint32_t double_press_gap_ms)
{
    controller->long_press_threshold_ms = long_press_threshold_ms;
    controller->double_press_gap_ms = double_press_gap_ms;
    controller->awaiting_second_press = false;
    controller->last_release_time_ms = 0U;
}

button_event_t button_controller_on_release(
    button_controller_t *controller,
    uint64_t release_time_ms,
    uint32_t press_duration_ms)
{
    if (press_duration_ms >= controller->long_press_threshold_ms) {
        controller->awaiting_second_press = false;
        controller->last_release_time_ms = release_time_ms;
        return BUTTON_EVENT_LONG_PRESS;
    }

    if (controller->awaiting_second_press &&
        (release_time_ms - controller->last_release_time_ms) <= controller->double_press_gap_ms) {
        controller->awaiting_second_press = false;
        controller->last_release_time_ms = release_time_ms;
        return BUTTON_EVENT_DOUBLE_PRESS;
    }

    controller->awaiting_second_press = true;
    controller->last_release_time_ms = release_time_ms;
    return BUTTON_EVENT_NONE;
}

button_event_t button_controller_flush(
    button_controller_t *controller,
    uint64_t now_ms)
{
    if (!controller->awaiting_second_press) {
        return BUTTON_EVENT_NONE;
    }

    if ((now_ms - controller->last_release_time_ms) <= controller->double_press_gap_ms) {
        return BUTTON_EVENT_NONE;
    }

    controller->awaiting_second_press = false;
    return BUTTON_EVENT_SINGLE_PRESS;
}

const char *button_event_name(button_event_t event)
{
    switch (event) {
    case BUTTON_EVENT_NONE:
        return "none";
    case BUTTON_EVENT_SINGLE_PRESS:
        return "single_press";
    case BUTTON_EVENT_DOUBLE_PRESS:
        return "double_press";
    case BUTTON_EVENT_LONG_PRESS:
        return "long_press";
    default:
        return "unknown";
    }
}
