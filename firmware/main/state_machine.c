#include "state_machine.h"

state_machine_result_t state_machine_step(
    state_machine_state_t current_state,
    state_machine_event_t event)
{
    switch (current_state) {
    case STATE_MACHINE_STATE_IDLE:
        switch (event) {
        case STATE_MACHINE_EVENT_SINGLE_PRESS:
            return (state_machine_result_t) {
                .next_state = STATE_MACHINE_STATE_RECORDING,
                .action = STATE_MACHINE_ACTION_START_RECORDING,
            };
        case STATE_MACHINE_EVENT_DOUBLE_PRESS:
            return (state_machine_result_t) {
                .next_state = STATE_MACHINE_STATE_IDLE,
                .action = STATE_MACHINE_ACTION_START_NEW_THREAD,
            };
        case STATE_MACHINE_EVENT_LONG_PRESS:
            return (state_machine_result_t) {
                .next_state = STATE_MACHINE_STATE_IDLE,
                .action = STATE_MACHINE_ACTION_REPLAY_LAST_AUDIO,
            };
        case STATE_MACHINE_EVENT_AUDIO_AVAILABLE:
            return (state_machine_result_t) {
                .next_state = STATE_MACHINE_STATE_PLAYING,
                .action = STATE_MACHINE_ACTION_START_PLAYBACK,
            };
        default:
            return (state_machine_result_t) {
                .next_state = STATE_MACHINE_STATE_IDLE,
                .action = STATE_MACHINE_ACTION_NONE,
            };
        }

    case STATE_MACHINE_STATE_RECORDING:
        switch (event) {
        case STATE_MACHINE_EVENT_SINGLE_PRESS:
            return (state_machine_result_t) {
                .next_state = STATE_MACHINE_STATE_WAITING_FOR_REPLY,
                .action = STATE_MACHINE_ACTION_STOP_RECORDING,
            };
        case STATE_MACHINE_EVENT_DOUBLE_PRESS:
            return (state_machine_result_t) {
                .next_state = STATE_MACHINE_STATE_IDLE,
                .action = STATE_MACHINE_ACTION_CANCEL_RECORDING,
            };
        default:
            return (state_machine_result_t) {
                .next_state = STATE_MACHINE_STATE_RECORDING,
                .action = STATE_MACHINE_ACTION_NONE,
            };
        }

    case STATE_MACHINE_STATE_WAITING_FOR_REPLY:
        if (event == STATE_MACHINE_EVENT_AUDIO_AVAILABLE) {
            return (state_machine_result_t) {
                .next_state = STATE_MACHINE_STATE_PLAYING,
                .action = STATE_MACHINE_ACTION_START_PLAYBACK,
            };
        }

        return (state_machine_result_t) {
            .next_state = STATE_MACHINE_STATE_WAITING_FOR_REPLY,
            .action = STATE_MACHINE_ACTION_NONE,
        };

    case STATE_MACHINE_STATE_PLAYING:
        if (event == STATE_MACHINE_EVENT_PLAYBACK_DONE) {
            return (state_machine_result_t) {
                .next_state = STATE_MACHINE_STATE_IDLE,
                .action = STATE_MACHINE_ACTION_NONE,
            };
        }

        return (state_machine_result_t) {
            .next_state = STATE_MACHINE_STATE_PLAYING,
            .action = STATE_MACHINE_ACTION_NONE,
        };
    }

    return (state_machine_result_t) {
        .next_state = STATE_MACHINE_STATE_IDLE,
        .action = STATE_MACHINE_ACTION_NONE,
    };
}

const char *state_machine_state_name(state_machine_state_t state)
{
    switch (state) {
    case STATE_MACHINE_STATE_IDLE:
        return "IDLE";
    case STATE_MACHINE_STATE_RECORDING:
        return "RECORDING";
    case STATE_MACHINE_STATE_WAITING_FOR_REPLY:
        return "WAITING_FOR_REPLY";
    case STATE_MACHINE_STATE_PLAYING:
        return "PLAYING";
    default:
        return "UNKNOWN";
    }
}

const char *state_machine_event_name(state_machine_event_t event)
{
    switch (event) {
    case STATE_MACHINE_EVENT_SINGLE_PRESS:
        return "single_press";
    case STATE_MACHINE_EVENT_DOUBLE_PRESS:
        return "double_press";
    case STATE_MACHINE_EVENT_LONG_PRESS:
        return "long_press";
    case STATE_MACHINE_EVENT_AUDIO_AVAILABLE:
        return "audio_available";
    case STATE_MACHINE_EVENT_PLAYBACK_DONE:
        return "playback_done";
    default:
        return "unknown";
    }
}

const char *state_machine_action_name(state_machine_action_t action)
{
    switch (action) {
    case STATE_MACHINE_ACTION_NONE:
        return "none";
    case STATE_MACHINE_ACTION_START_RECORDING:
        return "start_recording";
    case STATE_MACHINE_ACTION_STOP_RECORDING:
        return "stop_recording";
    case STATE_MACHINE_ACTION_CANCEL_RECORDING:
        return "cancel_recording";
    case STATE_MACHINE_ACTION_START_NEW_THREAD:
        return "start_new_thread";
    case STATE_MACHINE_ACTION_REPLAY_LAST_AUDIO:
        return "replay_last_audio";
    case STATE_MACHINE_ACTION_START_PLAYBACK:
        return "start_playback";
    default:
        return "unknown";
    }
}
