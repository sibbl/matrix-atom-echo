#include <assert.h>
#include <string.h>

#include "button_controller.h"

int main(void)
{
    button_controller_t controller;

    button_controller_init(&controller, 700U, 250U);
    assert(button_controller_on_release(&controller, 1000U, 120U) == BUTTON_EVENT_NONE);
    assert(button_controller_flush(&controller, 1300U) == BUTTON_EVENT_SINGLE_PRESS);

    button_controller_init(&controller, 700U, 250U);
    assert(button_controller_on_release(&controller, 1000U, 120U) == BUTTON_EVENT_NONE);
    assert(button_controller_on_release(&controller, 1150U, 100U) == BUTTON_EVENT_DOUBLE_PRESS);

    button_controller_init(&controller, 700U, 250U);
    assert(button_controller_on_release(&controller, 2000U, 900U) == BUTTON_EVENT_LONG_PRESS);
    assert(strcmp(button_event_name(BUTTON_EVENT_LONG_PRESS), "long_press") == 0);

    return 0;
}
