#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "pb3ds/compat.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * libctru KEY_* layout, defined here so callers never include 3ds.h.
 * SELECT is reserved for the future PaperBoat menu (M16) and is never
 * written into an N64 OSContPad.
 */
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
#define PB_KEY_ZL (1U << 14)
#define PB_KEY_ZR (1U << 15)
#define PB_KEY_CSTICK_RIGHT (1U << 24)
#define PB_KEY_CSTICK_LEFT (1U << 25)
#define PB_KEY_CSTICK_UP (1U << 26)
#define PB_KEY_CSTICK_DOWN (1U << 27)

/* PaperBoat / libultra OSContPad button bits (include/PR/os_cont.h). */
#define PB_CONT_A 0x8000U
#define PB_CONT_B 0x4000U
#define PB_CONT_Z 0x2000U
#define PB_CONT_START 0x1000U
#define PB_CONT_UP 0x0800U
#define PB_CONT_DOWN 0x0400U
#define PB_CONT_LEFT 0x0200U
#define PB_CONT_RIGHT 0x0100U
#define PB_CONT_L 0x0020U
#define PB_CONT_R 0x0010U
#define PB_CONT_C_UP 0x0008U
#define PB_CONT_C_DOWN 0x0004U
#define PB_CONT_C_LEFT 0x0002U
#define PB_CONT_C_RIGHT 0x0001U

#define PB_STICK_DEADZONE 15
#define PB_STICK_3DS_MAX 156
#define PB_STICK_N64_MAX 80

typedef struct {
    uint32_t held;
    uint32_t down;
    int16_t stick_x;
    int16_t stick_y;
    int16_t cstick_x;
    int16_t cstick_y;
} PBInputSample;

void pb_input_poll(PBInputSample *sample);
void pb_input_last(PBInputSample *sample);
void pb_input_map_n64(const PBInputSample *sample, PBOSContPad *pad);
int8_t pb_input_scale_stick(int16_t axis);
bool pb_input_select_reserved(void);
bool pb_input_select_pressed(const PBInputSample *sample);

#ifndef __3DS__
void pb_input_host_set(uint32_t held, uint32_t down);
void pb_input_host_set_stick(int16_t x, int16_t y);
void pb_input_host_set_cstick(int16_t x, int16_t y);
#endif

#ifdef __cplusplus
}
#endif
