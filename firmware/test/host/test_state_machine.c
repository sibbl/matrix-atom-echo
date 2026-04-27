#include <assert.h>

#include "state_machine.h"

static void assert_step(
    state_machine_state_t current_state,
    state_machine_event_t event,
    state_machine_state_t expected_state,
    state_machine_action_t expected_action)
{
    const state_machine_result_t result = state_machine_step(current_state, event);

    assert(result.next_state == expected_state);
    assert(result.action == expected_action);
}

int main(void)
{
    assert_step(
        STATE_MACHINE_STATE_IDLE,
        STATE_MACHINE_EVENT_SINGLE_PRESS,
        STATE_MACHINE_STATE_RECORDING,
        STATE_MACHINE_ACTION_START_RECORDING);
    assert_step(
        STATE_MACHINE_STATE_RECORDING,
        STATE_MACHINE_EVENT_SINGLE_PRESS,
        STATE_MACHINE_STATE_WAITING_FOR_REPLY,
        STATE_MACHINE_ACTION_STOP_RECORDING);
    assert_step(
        STATE_MACHINE_STATE_RECORDING,
        STATE_MACHINE_EVENT_DOUBLE_PRESS,
        STATE_MACHINE_STATE_IDLE,
        STATE_MACHINE_ACTION_CANCEL_RECORDING);
    assert_step(
        STATE_MACHINE_STATE_IDLE,
        STATE_MACHINE_EVENT_DOUBLE_PRESS,
        STATE_MACHINE_STATE_IDLE,
        STATE_MACHINE_ACTION_START_NEW_THREAD);
    assert_step(
        STATE_MACHINE_STATE_IDLE,
        STATE_MACHINE_EVENT_LONG_PRESS,
        STATE_MACHINE_STATE_IDLE,
        STATE_MACHINE_ACTION_REPLAY_LAST_AUDIO);
    assert_step(
        STATE_MACHINE_STATE_IDLE,
        STATE_MACHINE_EVENT_AUDIO_AVAILABLE,
        STATE_MACHINE_STATE_PLAYING,
        STATE_MACHINE_ACTION_START_PLAYBACK);
    assert_step(
        STATE_MACHINE_STATE_WAITING_FOR_REPLY,
        STATE_MACHINE_EVENT_AUDIO_AVAILABLE,
        STATE_MACHINE_STATE_PLAYING,
        STATE_MACHINE_ACTION_START_PLAYBACK);
    assert_step(
        STATE_MACHINE_STATE_PLAYING,
        STATE_MACHINE_EVENT_PLAYBACK_DONE,
        STATE_MACHINE_STATE_IDLE,
        STATE_MACHINE_ACTION_NONE);

    return 0;
}
