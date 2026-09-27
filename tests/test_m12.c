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
            fprintf(stderr, "M12 title check failed at %s:%d: %s\n",           \
                    __FILE__, __LINE__, #expression);                          \
            return false;                                                      \
        }                                                                      \
    } while (0)

static bool nearly(float value, float expected) {
    float delta = value - expected;
    if (delta < 0.0f) {
        delta = -delta;
    }
    return delta < 0.01f;
}

static unsigned char *zeros(size_t bytes) {
    unsigned char *block = (unsigned char *)calloc(1U, bytes);
    return block;
}

static bool test_bind_orientation_and_scene(void) {
    PBBootstrap bootstrap;
    PBTitleState state;
    unsigned char *logo;
    unsigned char *copyright;
    unsigned char *press;
    unsigned char tiny[4];
    float s0 = 0.0f;
    float t0 = 0.0f;
    float s1 = 0.0f;
    float t1 = 0.0f;
    float top_x = 0.0f;
    float top_y = 0.0f;
    unsigned i;

    CHECK(strcmp(PB3DS_VERSION, "0.13.0-m13") == 0);
    CHECK(strstr(PB3DS_ROADMAP_STAGE, "M13") != NULL);
    CHECK(PB_TITLE_LOGO_WIDTH == 200U);
    CHECK(PB_TITLE_LOGO_HEIGHT == 112U);
    CHECK(PB_TITLE_LOGO_BYTES == 89600U);
    CHECK(PB_TITLE_COPYRIGHT_BYTES == 4608U);
    CHECK(PB_TITLE_PRESS_BYTES == 4096U);
    CHECK(PB_TITLE_LOGO_SOURCE_X == 60U);
    CHECK(PB_TITLE_LOGO_SOURCE_Y == 15U);
    CHECK(strcmp(PB_TITLE_OTR_LOGO, "__OTR__title_screen/title_logo") == 0);
    CHECK(pb_gfx_upright_t(0.0f) == 1.0f);
    CHECK(pb_gfx_upright_t(1.0f) == 0.0f);
    pb_gfx_map_source_to_top((float)PB_TITLE_LOGO_SOURCE_X,
                             (float)PB_TITLE_LOGO_SOURCE_Y, &top_x, &top_y);
    CHECK(nearly(top_x, 100.0f));
    CHECK(nearly(top_y, 15.0f));

    pb_bootstrap_init(&bootstrap);
    CHECK(pb_system_init(&bootstrap));
    pb_compat_init();
    pb_title_init();
    CHECK(!pb_title_resources_bound());
    CHECK(!pb_title_presented());
    pb_title_query(&state);
    CHECK(state.phase == PB_TITLE_WAITING_ASSETS);

    memset(tiny, 0x11, sizeof(tiny));
    CHECK(pb_fs_register(PB_TITLE_OTR_LOGO, tiny, sizeof(tiny)) == 0);
    pb_title_step();
    CHECK(!pb_title_resources_bound());

    logo = zeros(PB_TITLE_LOGO_BYTES);
    copyright = zeros(PB_TITLE_COPYRIGHT_BYTES);
    press = zeros(PB_TITLE_PRESS_BYTES);
    CHECK(logo != NULL && copyright != NULL && press != NULL);
    CHECK(pb_fs_register(PB_TITLE_OTR_LOGO, logo, PB_TITLE_LOGO_BYTES) == 0);
    CHECK(pb_fs_register(PB_TITLE_OTR_COPYRIGHT, copyright,
                         PB_TITLE_COPYRIGHT_BYTES) == 0);
    CHECK(pb_fs_register(PB_TITLE_OTR_PRESS_START, press,
                         PB_TITLE_PRESS_BYTES) == 0);
    pb_title_step();
    CHECK(pb_title_resources_bound());
    pb_title_query(&state);
    CHECK(state.logo_bytes == PB_TITLE_LOGO_BYTES);
    CHECK(state.copyright_bytes == PB_TITLE_COPYRIGHT_BYTES);
    CHECK(state.press_start_bytes == PB_TITLE_PRESS_BYTES);
    CHECK(state.phase == PB_TITLE_INIT || state.phase == PB_TITLE_APPEAR);
    CHECK(!pb_title_presented());

    pb_title_logo_uv(&s0, &t0, &s1, &t1);
    CHECK(nearly(s0, 0.0f));
    CHECK(nearly(s1, 1.0f));
    CHECK(nearly(t0, 1.0f));
    CHECK(nearly(t1, 0.0f));

    for (i = 0U; i < 8U; i++) {
        pb_title_step();
    }
    pb_title_query(&state);
    CHECK(state.phase == PB_TITLE_HOLD);
    CHECK(strcmp(pb_title_phase_name(PB_TITLE_HOLD), "hold") == 0);
    CHECK(!state.presented);
    CHECK(!pb_title_presented());

    pb_title_shutdown();
    CHECK(!pb_title_resources_bound());
    pb_system_shutdown(&bootstrap);
    free(logo);
    free(copyright);
    free(press);
    return true;
}

int main(void) {
    if (!test_bind_orientation_and_scene()) {
        return EXIT_FAILURE;
    }
    printf("M12 title contract: %u checks passed\n", checks_run);
    return EXIT_SUCCESS;
}
