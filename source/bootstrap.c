#include "pb3ds/bootstrap.h"

#include <stdio.h>
#include <string.h>

static const char *kNotPaperBoat =
    "M0 bootstrap only. PaperBoat is not running.";

void pb_bootstrap_init(PBBootstrap *state) {
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->lifecycle = PB_LIFECYCLE_ACTIVE;
    pb_bootstrap_log(state, "init");
    pb_bootstrap_log(state, kNotPaperBoat);
}

void pb_bootstrap_log(PBBootstrap *state, const char *message) {
    size_t index;

    if (state == NULL || message == NULL) {
        return;
    }
    index = state->log.next;
    snprintf(state->log.lines[index], PB_BOOTSTRAP_LOG_LINE, "%s", message);
    state->log.next = (index + 1U) % PB_BOOTSTRAP_LOG_CAPACITY;
    if (state->log.count < PB_BOOTSTRAP_LOG_CAPACITY) {
        state->log.count++;
    }
}

void pb_bootstrap_apt_event(PBBootstrap *state, PBAptEvent event) {
    if (state == NULL) {
        return;
    }
    switch (event) {
        case PB_APT_SUSPEND:
            state->lifecycle = PB_LIFECYCLE_SUSPENDED;
            pb_bootstrap_log(state, "apt suspend");
            break;
        case PB_APT_SLEEP:
            state->lifecycle = PB_LIFECYCLE_SLEEPING;
            pb_bootstrap_log(state, "apt sleep");
            break;
        case PB_APT_RESTORE:
        case PB_APT_WAKEUP:
            state->lifecycle = PB_LIFECYCLE_ACTIVE;
            pb_bootstrap_log(state, "apt resume");
            break;
        case PB_APT_EXIT:
            state->lifecycle = PB_LIFECYCLE_EXITING;
            state->should_exit = true;
            pb_bootstrap_log(state, "apt exit");
            break;
        default:
            break;
    }
}

void pb_bootstrap_on_start(PBBootstrap *state) {
    if (state == NULL) {
        return;
    }
    state->should_exit = true;
    state->lifecycle = PB_LIFECYCLE_EXITING;
    pb_bootstrap_log(state, "START requested clean shutdown");
}

void pb_bootstrap_tick(PBBootstrap *state) {
    if (state == NULL || !pb_bootstrap_is_running(state)) {
        return;
    }
    state->frames++;
}

bool pb_bootstrap_is_running(const PBBootstrap *state) {
    return state != NULL && !state->should_exit &&
           state->lifecycle != PB_LIFECYCLE_EXITING;
}

const char *pb_bootstrap_lifecycle_name(PBLifecycle lifecycle) {
    switch (lifecycle) {
        case PB_LIFECYCLE_SUSPENDED:
            return "suspended";
        case PB_LIFECYCLE_SLEEPING:
            return "sleeping";
        case PB_LIFECYCLE_EXITING:
            return "exiting";
        case PB_LIFECYCLE_ACTIVE:
        default:
            return "active";
    }
}

const char *pb_bootstrap_status_line(const PBBootstrap *state) {
    if (state == NULL) {
        return "uninitialized";
    }
    if (!state->gfx_ready) {
        return "waiting for gfx";
    }
    if (!state->top_ready || !state->bottom_ready) {
        return "waiting for framebuffers";
    }
    return pb_bootstrap_lifecycle_name(state->lifecycle);
}
