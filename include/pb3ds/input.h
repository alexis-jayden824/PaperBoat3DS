#pragma once

#include <stdbool.h>
#include <stdint.h>

#define PB_INPUT_STICK_LIMIT 80
#define PB_INPUT_CIRCLE_DEADZONE 15
#define PB_INPUT_CIRCLE_MAX 156
#define PB_INPUT_TOUCH_WIDTH 320
#define PB_INPUT_TOUCH_HEIGHT 240

typedef enum {
    PB_N64_C_RIGHT = 0x0001,
    PB_N64_C_LEFT = 0x0002,
    PB_N64_C_DOWN = 0x0004,
    PB_N64_C_UP = 0x0008,
    PB_N64_R = 0x0010,
    PB_N64_L = 0x0020,
    PB_N64_D_RIGHT = 0x0100,
    PB_N64_D_LEFT = 0x0200,
    PB_N64_D_DOWN = 0x0400,
    PB_N64_D_UP = 0x0800,
    PB_N64_START = 0x1000,
    PB_N64_Z = 0x2000,
    PB_N64_B = 0x4000,
    PB_N64_A = 0x8000,
} PBN64Button;

typedef struct {
    uint32_t keys_held;
    int16_t circle_x;
    int16_t circle_y;
    uint16_t touch_x;
    uint16_t touch_y;
    bool touch_active;
} PBInputSample;

typedef struct {
    uint32_t native_held;
    uint32_t native_pressed;
    uint32_t native_released;
    uint16_t n64_held;
    uint16_t n64_pressed;
    uint16_t n64_released;
    int8_t stick_x;
    int8_t stick_y;
    uint16_t touch_x;
    uint16_t touch_y;
    bool touch_held;
    bool touch_pressed;
    bool touch_released;
    bool menu_requested;
    bool suspended;
    bool waiting_for_neutral;
    uint64_t frame_index;
} PBInputState;

void pb_input_init(PBInputState *state);
void pb_input_update(PBInputState *state, const PBInputSample *sample);
void pb_input_poll(PBInputState *state);
void pb_input_suspend(PBInputState *state);
void pb_input_resume(PBInputState *state);

uint16_t pb_input_map_buttons(uint32_t keys_held);
int8_t pb_input_scale_circle_axis(int16_t value);

