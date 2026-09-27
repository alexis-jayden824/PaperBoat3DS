#include "pb3ds/platform.h"
#include "pb3ds/version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks_run;

#define CHECK(expression)                                                      \
    do {                                                                       \
        checks_run++;                                                          \
        if (!(expression)) {                                                   \
            fprintf(stderr, "M8 input check failed at %s:%d: %s\n",            \
                    __FILE__, __LINE__, #expression);                          \
            return false;                                                      \
        }                                                                      \
    } while (0)

static bool test_select_reserved_and_map(void) {
    PBInputSample sample;
    PBOSContPad pad;
    PBCompatState state;

    CHECK(strcmp(PB3DS_VERSION, "0.13.0-m13") == 0);
    CHECK(strstr(PB3DS_ROADMAP_STAGE, "M13") != NULL);
    CHECK(pb_input_select_reserved());
    CHECK(PB_KEY_SELECT == (1U << 2));
    CHECK(PB_CONT_A == 0x8000U);
    CHECK(PB_CONT_Z == 0x2000U);

    memset(&sample, 0, sizeof(sample));
    sample.held = PB_KEY_SELECT | PB_KEY_A;
    sample.down = PB_KEY_SELECT;
    CHECK(pb_input_select_pressed(&sample));
    pb_input_map_n64(&sample, &pad);
    CHECK((pad.button & (uint16_t)PB_CONT_A) != 0U);
    CHECK((pad.button & (uint16_t)PB_CONT_Z) == 0U);
    CHECK(pad.button == (uint16_t)PB_CONT_A);

    memset(&sample, 0, sizeof(sample));
    sample.held = PB_KEY_SELECT;
    pb_input_map_n64(&sample, &pad);
    CHECK(pad.button == 0U);
    CHECK(pad.stick_x == 0);
    CHECK(pad.stick_y == 0);

    memset(&sample, 0, sizeof(sample));
    sample.held = PB_KEY_L | PB_KEY_R | PB_KEY_X | PB_KEY_Y | PB_KEY_START |
                  PB_KEY_B | PB_KEY_DUP | PB_KEY_CSTICK_RIGHT;
    pb_input_map_n64(&sample, &pad);
    CHECK((pad.button & (uint16_t)PB_CONT_Z) != 0U);
    CHECK((pad.button & (uint16_t)PB_CONT_R) != 0U);
    CHECK((pad.button & (uint16_t)PB_CONT_C_UP) != 0U);
    CHECK((pad.button & (uint16_t)PB_CONT_C_LEFT) != 0U);
    CHECK((pad.button & (uint16_t)PB_CONT_START) != 0U);
    CHECK((pad.button & (uint16_t)PB_CONT_B) != 0U);
    CHECK((pad.button & (uint16_t)PB_CONT_UP) != 0U);
    CHECK((pad.button & (uint16_t)PB_CONT_C_RIGHT) != 0U);

    CHECK(pb_input_scale_stick(0) == 0);
    CHECK(pb_input_scale_stick(5) == 0);
    CHECK(pb_input_scale_stick(-5) == 0);
    CHECK(pb_input_scale_stick(156) == 80);
    CHECK(pb_input_scale_stick(-156) == -80);

    pb_input_host_set(PB_KEY_A, PB_KEY_A);
    pb_input_host_set_stick(156, -156);
    pb_input_poll(&sample);
    CHECK((sample.held & PB_KEY_A) != 0U);
    CHECK(sample.stick_x == 156);
    pb_compat_init();
    pb_compat_query(&state);
    CHECK(state.controller == PB_COMPAT_READY);
    pb_compat_poll_controller(&pad);
    CHECK((pad.button & (uint16_t)PB_CONT_A) != 0U);
    CHECK(pad.stick_x == 80);
    CHECK(pad.stick_y == -80);
    return true;
}

int main(void) {
    if (!test_select_reserved_and_map()) {
        return EXIT_FAILURE;
    }
    printf("M8 input contract: %u checks passed\n", checks_run);
    return EXIT_SUCCESS;
}
