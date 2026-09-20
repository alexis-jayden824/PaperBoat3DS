#include "pb3ds/input.h"

#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks_run;
static u32 fake_keys_held;
static circlePosition fake_circle;
static touchPosition fake_touch;
static unsigned int scan_count;

#define CHECK(expression)                                                      \
    do {                                                                       \
        checks_run++;                                                          \
        if (!(expression)) {                                                   \
            fprintf(stderr, "input backend check failed at %s:%d: %s\n",    \
                    __FILE__, __LINE__, #expression);                          \
            return false;                                                      \
        }                                                                      \
    } while (0)

void hidScanInput(void) {
    scan_count++;
}

u32 hidKeysHeld(void) {
    return fake_keys_held;
}

void hidCircleRead(circlePosition *position) {
    *position = fake_circle;
}

void hidTouchRead(touchPosition *position) {
    *position = fake_touch;
}

static PBInputSample sample_with(uint32_t keys) {
    PBInputSample sample = { 0 };
    sample.keys_held = keys;
    return sample;
}

static bool test_default_mapping(void) {
    CHECK(pb_input_map_buttons(KEY_A) == PB_N64_A);
    CHECK(pb_input_map_buttons(KEY_B) == PB_N64_B);
    CHECK(pb_input_map_buttons(KEY_X) == PB_N64_Z);
    CHECK(pb_input_map_buttons(KEY_Y) == PB_N64_C_DOWN);
    CHECK(pb_input_map_buttons(KEY_START) == PB_N64_START);
    CHECK(pb_input_map_buttons(KEY_L) == PB_N64_L);
    CHECK(pb_input_map_buttons(KEY_R) == PB_N64_R);
    CHECK(pb_input_map_buttons(KEY_ZL) == PB_N64_Z);
    CHECK(pb_input_map_buttons(KEY_ZR) == PB_N64_R);
    CHECK(pb_input_map_buttons(KEY_SELECT) == 0);
    CHECK(pb_input_map_buttons(KEY_CPAD_UP | KEY_CPAD_RIGHT) == 0);

    CHECK(pb_input_map_buttons(KEY_DUP) == PB_N64_C_UP);
    CHECK(pb_input_map_buttons(KEY_DDOWN) == PB_N64_C_DOWN);
    CHECK(pb_input_map_buttons(KEY_DLEFT) == PB_N64_C_LEFT);
    CHECK(pb_input_map_buttons(KEY_DRIGHT) == PB_N64_C_RIGHT);
    CHECK(pb_input_map_buttons(KEY_L | KEY_DUP) == PB_N64_D_UP);
    CHECK(pb_input_map_buttons(KEY_L | KEY_DDOWN) == PB_N64_D_DOWN);
    CHECK(pb_input_map_buttons(KEY_L | KEY_DLEFT) == PB_N64_D_LEFT);
    CHECK(pb_input_map_buttons(KEY_L | KEY_DRIGHT) == PB_N64_D_RIGHT);

    CHECK(pb_input_map_buttons(KEY_CSTICK_UP | KEY_CSTICK_LEFT) ==
          (PB_N64_C_UP | PB_N64_C_LEFT));
    CHECK(pb_input_map_buttons(KEY_DUP | KEY_DDOWN) == 0);
    CHECK(pb_input_map_buttons(KEY_DLEFT | KEY_DRIGHT) == 0);
    return true;
}

static bool test_old_3ds_button_coverage(void) {
    static const uint32_t physical_inputs[] = {
        KEY_A,
        KEY_B,
        KEY_X,
        KEY_START,
        KEY_L,
        KEY_R,
        KEY_DUP,
        KEY_DDOWN,
        KEY_DLEFT,
        KEY_DRIGHT,
        KEY_L | KEY_DUP,
        KEY_L | KEY_DDOWN,
        KEY_L | KEY_DLEFT,
        KEY_L | KEY_DRIGHT,
    };
    uint16_t reachable = 0;
    for (size_t i = 0; i < sizeof(physical_inputs) / sizeof(physical_inputs[0]);
         i++) {
        reachable |= pb_input_map_buttons(physical_inputs[i]);
    }

    const uint16_t complete_n64_set =
        PB_N64_A | PB_N64_B | PB_N64_Z | PB_N64_START | PB_N64_L | PB_N64_R |
        PB_N64_C_UP | PB_N64_C_DOWN | PB_N64_C_LEFT | PB_N64_C_RIGHT |
        PB_N64_D_UP | PB_N64_D_DOWN | PB_N64_D_LEFT | PB_N64_D_RIGHT;
    CHECK(reachable == complete_n64_set);
    return true;
}

static bool test_circle_scaling(void) {
    CHECK(pb_input_scale_circle_axis(0) == 0);
    CHECK(pb_input_scale_circle_axis(PB_INPUT_CIRCLE_DEADZONE) == 0);
    CHECK(pb_input_scale_circle_axis(-PB_INPUT_CIRCLE_DEADZONE) == 0);
    CHECK(pb_input_scale_circle_axis(PB_INPUT_CIRCLE_DEADZONE + 1) > 0);
    CHECK(pb_input_scale_circle_axis(-PB_INPUT_CIRCLE_DEADZONE - 1) < 0);
    CHECK(pb_input_scale_circle_axis(PB_INPUT_CIRCLE_MAX) ==
          PB_INPUT_STICK_LIMIT);
    CHECK(pb_input_scale_circle_axis(-PB_INPUT_CIRCLE_MAX) ==
          -PB_INPUT_STICK_LIMIT);
    CHECK(pb_input_scale_circle_axis(INT16_MAX) == PB_INPUT_STICK_LIMIT);
    CHECK(pb_input_scale_circle_axis(INT16_MIN) == -PB_INPUT_STICK_LIMIT);
    return true;
}

static bool test_edges_and_menu_reservation(void) {
    PBInputState state;
    pb_input_init(&state);
    CHECK(state.frame_index == 0);

    PBInputSample sample = sample_with(KEY_A | KEY_SELECT);
    pb_input_update(&state, &sample);
    CHECK(state.native_pressed == (KEY_A | KEY_SELECT));
    CHECK(state.n64_held == PB_N64_A);
    CHECK(state.n64_pressed == PB_N64_A);
    CHECK(state.n64_released == 0);
    CHECK(state.menu_requested);

    pb_input_update(&state, &sample);
    CHECK(state.native_pressed == 0);
    CHECK(state.n64_pressed == 0);
    CHECK(!state.menu_requested);

    sample = sample_with(0);
    pb_input_update(&state, &sample);
    CHECK(state.native_released == (KEY_A | KEY_SELECT));
    CHECK(state.n64_released == PB_N64_A);
    CHECK(state.n64_held == 0);
    CHECK(state.frame_index == 3);
    return true;
}

static bool test_touch_and_coordinates(void) {
    PBInputState state;
    pb_input_init(&state);

    PBInputSample sample = sample_with(KEY_TOUCH);
    sample.touch_active = true;
    sample.touch_x = 400;
    sample.touch_y = 300;
    pb_input_update(&state, &sample);
    CHECK(state.touch_held);
    CHECK(state.touch_pressed);
    CHECK(!state.touch_released);
    CHECK(state.touch_x == PB_INPUT_TOUCH_WIDTH - 1);
    CHECK(state.touch_y == PB_INPUT_TOUCH_HEIGHT - 1);

    sample.touch_x = 100;
    sample.touch_y = 50;
    pb_input_update(&state, &sample);
    CHECK(state.touch_held);
    CHECK(!state.touch_pressed);
    CHECK(state.touch_x == 100);
    CHECK(state.touch_y == 50);

    sample = sample_with(0);
    pb_input_update(&state, &sample);
    CHECK(!state.touch_held);
    CHECK(state.touch_released);
    CHECK(state.touch_x == 100);
    CHECK(state.touch_y == 50);
    return true;
}

static bool test_suspend_resume_neutral_gate(void) {
    PBInputState state;
    pb_input_init(&state);

    PBInputSample sample = sample_with(KEY_B);
    sample.circle_x = PB_INPUT_CIRCLE_MAX;
    pb_input_update(&state, &sample);
    CHECK(state.n64_held == PB_N64_B);
    CHECK(state.stick_x == PB_INPUT_STICK_LIMIT);

    pb_input_suspend(&state);
    CHECK(state.suspended);
    CHECK(state.waiting_for_neutral);
    CHECK(state.n64_held == 0);
    CHECK(state.stick_x == 0);

    pb_input_update(&state, &sample);
    CHECK(state.n64_held == 0);
    pb_input_resume(&state);
    CHECK(!state.suspended);
    CHECK(state.waiting_for_neutral);

    pb_input_update(&state, &sample);
    CHECK(state.n64_held == 0);
    CHECK(state.waiting_for_neutral);

    sample = sample_with(0);
    pb_input_update(&state, &sample);
    CHECK(!state.waiting_for_neutral);
    CHECK(state.n64_held == 0);

    sample = sample_with(KEY_B);
    pb_input_update(&state, &sample);
    CHECK(state.n64_pressed == PB_N64_B);
    return true;
}

static bool test_hardware_poll_adapter(void) {
    PBInputState state;
    pb_input_init(&state);
    fake_keys_held = KEY_A | KEY_TOUCH;
    fake_circle.dx = PB_INPUT_CIRCLE_MAX;
    fake_circle.dy = -PB_INPUT_CIRCLE_MAX;
    fake_touch.px = 123;
    fake_touch.py = 45;
    scan_count = 0;

    pb_input_poll(&state);
    CHECK(scan_count == 1);
    CHECK(state.n64_held == PB_N64_A);
    CHECK(state.stick_x == PB_INPUT_STICK_LIMIT);
    CHECK(state.stick_y == -PB_INPUT_STICK_LIMIT);
    CHECK(state.touch_held);
    CHECK(state.touch_x == 123);
    CHECK(state.touch_y == 45);
    return true;
}

int main(void) {
    if (!test_default_mapping() || !test_old_3ds_button_coverage() ||
        !test_circle_scaling() || !test_edges_and_menu_reservation() ||
        !test_touch_and_coordinates() || !test_suspend_resume_neutral_gate() ||
        !test_hardware_poll_adapter()) {
        return EXIT_FAILURE;
    }

    printf("M8 input backend: %u checks passed\n", checks_run);
    return EXIT_SUCCESS;
}
