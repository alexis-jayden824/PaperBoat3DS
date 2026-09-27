#include "pb3ds/compat.h"
#include "pb3ds/input.h"

#include <string.h>

bool pb_input_select_reserved(void) {
    return true;
}

bool pb_input_select_pressed(const PBInputSample *sample) {
    return sample != NULL && (sample->down & PB_KEY_SELECT) != 0U;
}

int8_t pb_input_scale_stick(int16_t axis) {
    int32_t scaled;

    if (axis > -PB_STICK_DEADZONE && axis < PB_STICK_DEADZONE) {
        return 0;
    }
    scaled = ((int32_t)axis * PB_STICK_N64_MAX) / PB_STICK_3DS_MAX;
    if (scaled > PB_STICK_N64_MAX) {
        scaled = PB_STICK_N64_MAX;
    }
    if (scaled < -PB_STICK_N64_MAX) {
        scaled = -PB_STICK_N64_MAX;
    }
    return (int8_t)scaled;
}

void pb_input_map_n64(const PBInputSample *sample, PBOSContPad *pad) {
    uint32_t keys;

    if (pad == NULL) {
        return;
    }
    memset(pad, 0, sizeof(*pad));
    if (sample == NULL) {
        return;
    }

    /* SELECT is platform-reserved and must never reach the game pad. */
    keys = sample->held & ~PB_KEY_SELECT;

    if ((keys & PB_KEY_A) != 0U) {
        pad->button |= (uint16_t)PB_CONT_A;
    }
    if ((keys & PB_KEY_B) != 0U) {
        pad->button |= (uint16_t)PB_CONT_B;
    }
    if ((keys & PB_KEY_START) != 0U) {
        pad->button |= (uint16_t)PB_CONT_START;
    }
    if ((keys & PB_KEY_DUP) != 0U) {
        pad->button |= (uint16_t)PB_CONT_UP;
    }
    if ((keys & PB_KEY_DDOWN) != 0U) {
        pad->button |= (uint16_t)PB_CONT_DOWN;
    }
    if ((keys & PB_KEY_DLEFT) != 0U) {
        pad->button |= (uint16_t)PB_CONT_LEFT;
    }
    if ((keys & PB_KEY_DRIGHT) != 0U) {
        pad->button |= (uint16_t)PB_CONT_RIGHT;
    }
    /* Old 3DS has no ZL; Paper Mario uses Z constantly, so L is Z. */
    if ((keys & (PB_KEY_L | PB_KEY_ZL)) != 0U) {
        pad->button |= (uint16_t)PB_CONT_Z;
    }
    if ((keys & PB_KEY_R) != 0U) {
        pad->button |= (uint16_t)PB_CONT_R;
    }
    if ((keys & (PB_KEY_X | PB_KEY_CSTICK_UP)) != 0U) {
        pad->button |= (uint16_t)PB_CONT_C_UP;
    }
    if ((keys & (PB_KEY_ZR | PB_KEY_CSTICK_DOWN)) != 0U) {
        pad->button |= (uint16_t)PB_CONT_C_DOWN;
    }
    if ((keys & (PB_KEY_Y | PB_KEY_CSTICK_LEFT)) != 0U) {
        pad->button |= (uint16_t)PB_CONT_C_LEFT;
    }
    if ((keys & PB_KEY_CSTICK_RIGHT) != 0U) {
        pad->button |= (uint16_t)PB_CONT_C_RIGHT;
    }

    pad->stick_x = pb_input_scale_stick(sample->stick_x);
    pad->stick_y = pb_input_scale_stick(sample->stick_y);
    pad->errnum = 0U;
}
