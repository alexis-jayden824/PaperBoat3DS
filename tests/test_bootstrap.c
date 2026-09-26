#include "pb3ds/bootstrap.h"
#include "pb3ds/version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks_run;

#define CHECK(expression)                                                      \
    do {                                                                       \
        checks_run++;                                                          \
        if (!(expression)) {                                                   \
            fprintf(stderr, "M0 bootstrap check failed at %s:%d: %s\n",        \
                    __FILE__, __LINE__, #expression);                          \
            return false;                                                      \
        }                                                                      \
    } while (0)

static bool test_init_does_not_claim_paperboat(void) {
    PBBootstrap state;
    size_t index;
    bool saw_disclaimer = false;

    pb_bootstrap_init(&state);
    CHECK(state.lifecycle == PB_LIFECYCLE_ACTIVE);
    CHECK(!state.should_exit);
    CHECK(state.frames == 0);
    CHECK(!state.gfx_ready);
    CHECK(strcmp(PB3DS_PROJECT_NAME, "PaperBoat3DS Refolded") == 0);
    CHECK(strstr(PB3DS_ROADMAP_STAGE, "M2") != NULL);
    CHECK(strcmp(PB3DS_VERSION, "0.2.0-m2") == 0);
    CHECK(pb_bootstrap_is_running(&state));
    for (index = 0; index < state.log.count; index++) {
        if (strstr(state.log.lines[index], "PaperBoat is not running") !=
            NULL) {
            saw_disclaimer = true;
        }
    }
    CHECK(saw_disclaimer);
    return true;
}

static bool test_lifecycle_and_start_exit(void) {
    PBBootstrap state;

    pb_bootstrap_init(&state);
    pb_bootstrap_apt_event(&state, PB_APT_SUSPEND);
    CHECK(state.lifecycle == PB_LIFECYCLE_SUSPENDED);
    CHECK(strcmp(pb_bootstrap_lifecycle_name(state.lifecycle), "suspended") ==
          0);
    pb_bootstrap_apt_event(&state, PB_APT_RESTORE);
    CHECK(state.lifecycle == PB_LIFECYCLE_ACTIVE);

    pb_bootstrap_apt_event(&state, PB_APT_SLEEP);
    CHECK(state.lifecycle == PB_LIFECYCLE_SLEEPING);
    pb_bootstrap_apt_event(&state, PB_APT_WAKEUP);
    CHECK(state.lifecycle == PB_LIFECYCLE_ACTIVE);

    pb_bootstrap_tick(&state);
    pb_bootstrap_tick(&state);
    CHECK(state.frames == 2);

    pb_bootstrap_on_start(&state);
    CHECK(state.should_exit);
    CHECK(state.lifecycle == PB_LIFECYCLE_EXITING);
    CHECK(!pb_bootstrap_is_running(&state));
    pb_bootstrap_tick(&state);
    CHECK(state.frames == 2);
    return true;
}

static bool test_apt_exit_and_status(void) {
    PBBootstrap state;

    pb_bootstrap_init(&state);
    CHECK(strcmp(pb_bootstrap_status_line(&state), "waiting for gfx") == 0);
    state.gfx_ready = true;
    CHECK(strcmp(pb_bootstrap_status_line(&state), "waiting for framebuffers") ==
          0);
    state.top_ready = true;
    state.bottom_ready = true;
    CHECK(strcmp(pb_bootstrap_status_line(&state), "active") == 0);

    pb_bootstrap_apt_event(&state, PB_APT_EXIT);
    CHECK(state.should_exit);
    CHECK(!pb_bootstrap_is_running(&state));
    pb_bootstrap_init(NULL);
    pb_bootstrap_log(NULL, "ignored");
    pb_bootstrap_on_start(NULL);
    pb_bootstrap_tick(NULL);
    return true;
}

int main(void) {
    if (!test_init_does_not_claim_paperboat() ||
        !test_lifecycle_and_start_exit() || !test_apt_exit_and_status()) {
        return EXIT_FAILURE;
    }
    printf("M0 bootstrap contract: %u checks passed\n", checks_run);
    return EXIT_SUCCESS;
}
