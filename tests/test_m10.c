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
            fprintf(stderr, "M10 loop check failed at %s:%d: %s\n",            \
                    __FILE__, __LINE__, #expression);                          \
            return false;                                                      \
        }                                                                      \
    } while (0)

static bool test_timing_and_pause(void) {
    PBBootstrap bootstrap;
    PBLoopStatus status;
    PBCompatState compat;
    uint64_t frozen;

    CHECK(strcmp(PB3DS_VERSION, "0.12.0-m12") == 0);
    CHECK(strstr(PB3DS_ROADMAP_STAGE, "M12") != NULL);
    CHECK(PB_LOOP_TICK_HZ == 30U);
    CHECK(!pb_thread_extra_workers_allowed());
    CHECK(!pb_loop_desktop_window_allowed());

    pb_time_host_set(0);
    pb_bootstrap_init(&bootstrap);
    CHECK(pb_system_init(&bootstrap));
    CHECK(pb_time_ms() == 0U);
    CHECK(!pb_loop_paused());

    CHECK(pb_loop_begin_frame() == 0U);
    pb_time_host_advance(PB_LOOP_TICK_MS);
    CHECK(pb_loop_begin_frame() == 1U);
    pb_time_host_advance(PB_LOOP_TICK_MS * 3U);
    CHECK(pb_loop_begin_frame() == PB_LOOP_MAX_CATCHUP);

    pb_compat_init();
    pb_compat_query(&compat);
    CHECK(compat.time == PB_COMPAT_READY);
    GameEngine_HoldFrame();

    frozen = pb_time_ms();
    pb_loop_on_apt(PB_APT_SUSPEND);
    CHECK(pb_loop_paused());
    pb_time_host_advance(5000);
    CHECK(pb_time_ms() == frozen);
    CHECK(pb_loop_begin_frame() == 0U);

    pb_loop_on_apt(PB_APT_RESTORE);
    CHECK(!pb_loop_paused());
    CHECK(pb_time_ms() == frozen);
    CHECK(pb_loop_begin_frame() == 0U);
    pb_time_host_advance(PB_LOOP_TICK_MS);
    CHECK(pb_loop_begin_frame() == 1U);

    pb_loop_query(&status);
    CHECK(status.game_ticks >= 1U);
    CHECK(!status.paused);

    pb_system_shutdown(&bootstrap);
    return true;
}

int main(void) {
    if (!test_timing_and_pause()) {
        return EXIT_FAILURE;
    }
    printf("M10 loop contract: %u checks passed\n", checks_run);
    return EXIT_SUCCESS;
}
