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
            fprintf(stderr, "M3 platform check failed at %s:%d: %s\n",         \
                    __FILE__, __LINE__, #expression);                          \
            return false;                                                      \
        }                                                                      \
    } while (0)

static bool test_deferred_and_layout(void) {
    CHECK(strcmp(PB3DS_VERSION, "0.7.0-m7") == 0);
    CHECK(strstr(PB3DS_ROADMAP_STAGE, "M7") != NULL);
    CHECK(pb_assets_status() == PB_ASSETS_HOST_ONLY);
    CHECK(!pb_assets_extraction_on_device());
    CHECK(PB_GFX_TOP_WIDTH == 400U);
    CHECK(PB_GFX_TOP_HEIGHT == 240U);
    CHECK(PB_GFX_BOTTOM_WIDTH == 320U);
    CHECK(PB_GFX_BOTTOM_HEIGHT == 240U);
    CHECK(PB_KEY_START == (1U << 3));
    CHECK(PB_KEY_A == (1U << 0));
    CHECK(pb_audio_status() == PB_AUDIO_DEFERRED_M14);
    CHECK(!pb_thread_extra_workers_allowed());
    CHECK(pb_fs_status() == PB_FS_DEFERRED_M9);
    CHECK(strcmp(pb_fs_sdmc_root(), "sdmc:/3ds/PaperBoat3DS/") == 0);
    CHECK(strcmp(PB_FS_PAPERBOAT_O2R, "sdmc:/3ds/PaperBoat3DS/paperboat.o2r") ==
          0);
    CHECK(strcmp(pb_log_level_name(PB_LOG_ERROR), "error") == 0);
    return true;
}

static bool test_host_lifecycle_and_input(void) {
    PBBootstrap bootstrap;
    PBInputSample sample;
    PBMemoryStatus memory;
    uint64_t first_ms;

    pb_bootstrap_init(&bootstrap);
    CHECK(pb_system_init(&bootstrap));
    CHECK(pb_gfx_ready());
    CHECK(bootstrap.gfx_ready);
    CHECK(bootstrap.top_ready);
    CHECK(bootstrap.bottom_ready);
    CHECK(pb_system_pump());

    pb_memory_query(&memory);
    CHECK(!memory.measured);

    first_ms = pb_time_ms();
    CHECK(pb_time_ms() == first_ms + 1U);

    pb_input_host_set(PB_KEY_START, PB_KEY_START);
    pb_input_poll(&sample);
    CHECK(sample.held == PB_KEY_START);
    CHECK((sample.down & PB_KEY_START) != 0U);
    pb_input_poll(&sample);
    CHECK(sample.down == 0U);
    CHECK(sample.held == PB_KEY_START);

    pb_gfx_clear_top(26U, 51U, 68U);
    pb_console_clear();
    pb_console_print("host");
    pb_gfx_present();
    pb_time_wait_vblank();
    pb_log(PB_LOG_INFO, "test", "m3 host");

    pb_system_shutdown(&bootstrap);
    CHECK(!pb_gfx_ready());
    CHECK(!pb_system_pump());
    CHECK(!bootstrap.gfx_ready);
    return true;
}

int main(void) {
    if (!test_deferred_and_layout() || !test_host_lifecycle_and_input()) {
        return EXIT_FAILURE;
    }
    printf("M3 platform contract: %u checks passed\n", checks_run);
    return EXIT_SUCCESS;
}
