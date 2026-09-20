#include "pb3ds/input.h"

#include <3ds.h>
#include <stddef.h>
#include <string.h>

static uint16_t clamp_touch_coordinate(uint16_t value, uint16_t limit) {
    if (value >= limit) {
        return (uint16_t)(limit - 1U);
    }
    return value;
}

static void clear_live_state(PBInputState *state) {
    state->native_held = 0;
    state->native_pressed = 0;
    state->native_released = 0;
    state->n64_held = 0;
    state->n64_pressed = 0;
    state->n64_released = 0;
    state->stick_x = 0;
    state->stick_y = 0;
    state->touch_held = false;
    state->touch_pressed = false;
    state->touch_released = false;
    state->menu_requested = false;
}

static void normalized_dpad(uint32_t keys, bool *up, bool *down,
                            bool *left, bool *right) {
    *up = (keys & KEY_DUP) != 0;
    *down = (keys & KEY_DDOWN) != 0;
    *left = (keys & KEY_DLEFT) != 0;
    *right = (keys & KEY_DRIGHT) != 0;

    if (*up && *down) {
        *up = false;
        *down = false;
    }
    if (*left && *right) {
        *left = false;
        *right = false;
    }
}

uint16_t pb_input_map_buttons(uint32_t keys_held) {
    uint16_t buttons = 0;
    bool up;
    bool down;
    bool left;
    bool right;

    if ((keys_held & KEY_A) != 0) {
        buttons |= PB_N64_A;
    }
    if ((keys_held & KEY_B) != 0) {
        buttons |= PB_N64_B;
    }
    if ((keys_held & (KEY_X | KEY_ZL)) != 0) {
        buttons |= PB_N64_Z;
    }
    if ((keys_held & KEY_Y) != 0) {
        buttons |= PB_N64_C_DOWN;
    }
    if ((keys_held & KEY_START) != 0) {
        buttons |= PB_N64_START;
    }
    if ((keys_held & (KEY_R | KEY_ZR)) != 0) {
        buttons |= PB_N64_R;
    }

    normalized_dpad(keys_held, &up, &down, &left, &right);
    const bool dpad_active = up || down || left || right;

    /*
     * Paper Mario uses the C buttons substantially more often than the N64
     * D-pad. The physical D-pad therefore supplies C by default. Holding L
     * shifts it to the N64 D-pad and suppresses L for that chord.
     */
    if ((keys_held & KEY_L) != 0 && dpad_active) {
        if (up) {
            buttons |= PB_N64_D_UP;
        }
        if (down) {
            buttons |= PB_N64_D_DOWN;
        }
        if (left) {
            buttons |= PB_N64_D_LEFT;
        }
        if (right) {
            buttons |= PB_N64_D_RIGHT;
        }
    } else {
        if ((keys_held & KEY_L) != 0) {
            buttons |= PB_N64_L;
        }
        if (up) {
            buttons |= PB_N64_C_UP;
        }
        if (down) {
            buttons |= PB_N64_C_DOWN;
        }
        if (left) {
            buttons |= PB_N64_C_LEFT;
        }
        if (right) {
            buttons |= PB_N64_C_RIGHT;
        }
    }

    if ((keys_held & KEY_CSTICK_UP) != 0) {
        buttons |= PB_N64_C_UP;
    }
    if ((keys_held & KEY_CSTICK_DOWN) != 0) {
        buttons |= PB_N64_C_DOWN;
    }
    if ((keys_held & KEY_CSTICK_LEFT) != 0) {
        buttons |= PB_N64_C_LEFT;
    }
    if ((keys_held & KEY_CSTICK_RIGHT) != 0) {
        buttons |= PB_N64_C_RIGHT;
    }

    return buttons;
}

int8_t pb_input_scale_circle_axis(int16_t value) {
    int32_t magnitude = value;
    int32_t sign = 1;

    if (magnitude < 0) {
        sign = -1;
        magnitude = -magnitude;
    }
    if (magnitude <= PB_INPUT_CIRCLE_DEADZONE) {
        return 0;
    }
    if (magnitude >= PB_INPUT_CIRCLE_MAX) {
        return (int8_t)(sign * PB_INPUT_STICK_LIMIT);
    }

    const int32_t usable_range = PB_INPUT_CIRCLE_MAX - PB_INPUT_CIRCLE_DEADZONE;
    const int32_t scaled =
        ((magnitude - PB_INPUT_CIRCLE_DEADZONE) * PB_INPUT_STICK_LIMIT +
         usable_range / 2) /
        usable_range;
    return (int8_t)(sign * scaled);
}

void pb_input_init(PBInputState *state) {
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
}

void pb_input_update(PBInputState *state, const PBInputSample *sample) {
    if (state == NULL || sample == NULL) {
        return;
    }

    state->frame_index++;
    if (state->suspended) {
        clear_live_state(state);
        return;
    }

    const int8_t next_stick_x = pb_input_scale_circle_axis(sample->circle_x);
    const int8_t next_stick_y = pb_input_scale_circle_axis(sample->circle_y);
    if (state->waiting_for_neutral) {
        const bool neutral = sample->keys_held == 0 && !sample->touch_active &&
                             next_stick_x == 0 && next_stick_y == 0;
        clear_live_state(state);
        if (neutral) {
            state->waiting_for_neutral = false;
        }
        return;
    }

    const uint32_t previous_native = state->native_held;
    const uint16_t previous_n64 = state->n64_held;
    const bool previous_touch = state->touch_held;

    state->native_held = sample->keys_held;
    state->native_pressed = sample->keys_held & ~previous_native;
    state->native_released = previous_native & ~sample->keys_held;

    state->n64_held = pb_input_map_buttons(sample->keys_held);
    state->n64_pressed = (uint16_t)(state->n64_held & ~previous_n64);
    state->n64_released = (uint16_t)(previous_n64 & ~state->n64_held);
    state->stick_x = next_stick_x;
    state->stick_y = next_stick_y;

    state->touch_held = sample->touch_active;
    state->touch_pressed = sample->touch_active && !previous_touch;
    state->touch_released = !sample->touch_active && previous_touch;
    if (sample->touch_active) {
        state->touch_x = clamp_touch_coordinate(sample->touch_x,
                                                PB_INPUT_TOUCH_WIDTH);
        state->touch_y = clamp_touch_coordinate(sample->touch_y,
                                                PB_INPUT_TOUCH_HEIGHT);
    }

    state->menu_requested = (state->native_pressed & KEY_SELECT) != 0;
}

void pb_input_poll(PBInputState *state) {
    if (state == NULL) {
        return;
    }

    PBInputSample sample = { 0 };
    circlePosition circle = { 0 };
    touchPosition touch = { 0 };

    hidScanInput();
    sample.keys_held = hidKeysHeld();
    hidCircleRead(&circle);
    sample.circle_x = circle.dx;
    sample.circle_y = circle.dy;
    sample.touch_active = (sample.keys_held & KEY_TOUCH) != 0;
    if (sample.touch_active) {
        hidTouchRead(&touch);
        sample.touch_x = touch.px;
        sample.touch_y = touch.py;
    }

    pb_input_update(state, &sample);
}

void pb_input_suspend(PBInputState *state) {
    if (state == NULL) {
        return;
    }
    clear_live_state(state);
    state->suspended = true;
    state->waiting_for_neutral = true;
}

void pb_input_resume(PBInputState *state) {
    if (state == NULL) {
        return;
    }
    clear_live_state(state);
    state->suspended = false;
    state->waiting_for_neutral = true;
}

