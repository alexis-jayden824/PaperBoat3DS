#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* libctru KEY_* layout, defined here so callers never include 3ds.h. */
#define PB_KEY_A (1U << 0)
#define PB_KEY_B (1U << 1)
#define PB_KEY_SELECT (1U << 2)
#define PB_KEY_START (1U << 3)
#define PB_KEY_DRIGHT (1U << 4)
#define PB_KEY_DLEFT (1U << 5)
#define PB_KEY_DUP (1U << 6)
#define PB_KEY_DDOWN (1U << 7)
#define PB_KEY_R (1U << 8)
#define PB_KEY_L (1U << 9)
#define PB_KEY_X (1U << 10)
#define PB_KEY_Y (1U << 11)

typedef struct {
    uint32_t held;
    uint32_t down;
    int16_t stick_x;
    int16_t stick_y;
} PBInputSample;

void pb_input_poll(PBInputSample *sample);
#ifndef __3DS__
void pb_input_host_set(uint32_t held, uint32_t down);
#endif

#ifdef __cplusplus
}
#endif
