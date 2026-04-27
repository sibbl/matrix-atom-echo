#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    STATE_MACHINE_STATE_IDLE = 0,
    STATE_MACHINE_STATE_RECORDING,
    STATE_MACHINE_STATE_WAITING_FOR_REPLY,
    STATE_MACHINE_STATE_PLAYING
} state_machine_state_t;

typedef enum {
    STATE_MACHINE_EVENT_SINGLE_PRESS = 0,
    STATE_MACHINE_EVENT_DOUBLE_PRESS,
    STATE_MACHINE_EVENT_LONG_PRESS,
    STATE_MACHINE_EVENT_AUDIO_AVAILABLE,
    STATE_MACHINE_EVENT_PLAYBACK_DONE
} state_machine_event_t;

typedef enum {
    STATE_MACHINE_ACTION_NONE = 0,
    STATE_MACHINE_ACTION_START_RECORDING,
    STATE_MACHINE_ACTION_STOP_RECORDING,
    STATE_MACHINE_ACTION_CANCEL_RECORDING,
    STATE_MACHINE_ACTION_START_NEW_THREAD,
    STATE_MACHINE_ACTION_REPLAY_LAST_AUDIO,
    STATE_MACHINE_ACTION_START_PLAYBACK
} state_machine_action_t;

typedef struct {
    state_machine_state_t next_state;
    state_machine_action_t action;
} state_machine_result_t;

state_machine_result_t state_machine_step(
    state_machine_state_t current_state,
    state_machine_event_t event);

const char *state_machine_state_name(state_machine_state_t state);
const char *state_machine_event_name(state_machine_event_t event);
const char *state_machine_action_name(state_machine_action_t action);

#ifdef __cplusplus
}
#endif

#endif
