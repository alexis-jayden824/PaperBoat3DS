#include "pb3ds/input.h"

#ifdef PB3DS_GAME_OBJECTS
#include <libultraship/libultra/controller.h>

#include <stddef.h>
#include <string.h>

/* PaperBoat's extended OSContPad includes gyro and right-stick fields.
 * Fill the complete upstream ABI instead of copying the smaller PBOSContPad
 * over an array of game pads. */
_Static_assert(offsetof(OSContPad, button) == 0U, "OSContPad button offset");
_Static_assert(offsetof(OSContPad, stick_x) == 2U, "OSContPad stick offset");

float GameEngine_GetAspectRatio(void) {
    /* Preserve Paper Mario's 320x240 world coordinates within the pillars. */
    return 4.0f / 3.0f;
}

void GameEngine_ReadController(void *opaque_pads) {
    OSContPad *pads = (OSContPad *)opaque_pads;
    PBInputSample sample;
    PBOSContPad mapped;

    if (pads == NULL) {
        return;
    }
    memset(pads, 0, sizeof(*pads) * 4U);
    pb_input_last(&sample);
    pb_input_map_n64(&sample, &mapped);
    pads[0].button = mapped.button;
    pads[0].stick_x = mapped.stick_x;
    pads[0].stick_y = mapped.stick_y;
    pads[0].right_stick_x = pb_input_scale_stick(sample.cstick_x);
    pads[0].right_stick_y = pb_input_scale_stick(sample.cstick_y);
}
#endif
