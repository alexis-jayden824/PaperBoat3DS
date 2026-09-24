#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * M0 is a native 3DS application shell only. It must not claim that PaperBoat
 * or Paper Mario is running.
 */
#define PB_BOOTSTRAP_TOP_WIDTH 400U
#define PB_BOOTSTRAP_TOP_HEIGHT 240U
#define PB_BOOTSTRAP_BOTTOM_WIDTH 320U
#define PB_BOOTSTRAP_BOTTOM_HEIGHT 240U
#define PB_BOOTSTRAP_LOG_CAPACITY 12U
#define PB_BOOTSTRAP_LOG_LINE 80U

typedef enum {
    PB_LIFECYCLE_ACTIVE = 0,
    PB_LIFECYCLE_SUSPENDED,
    PB_LIFECYCLE_SLEEPING,
    PB_LIFECYCLE_EXITING,
} PBLifecycle;

typedef enum {
    PB_APT_SUSPEND = 0,
    PB_APT_SLEEP,
    PB_APT_RESTORE,
    PB_APT_WAKEUP,
    PB_APT_EXIT,
} PBAptEvent;

typedef struct {
    char lines[PB_BOOTSTRAP_LOG_CAPACITY][PB_BOOTSTRAP_LOG_LINE];
    size_t count;
    size_t next;
} PBBootstrapLog;

typedef struct {
    PBLifecycle lifecycle;
    PBBootstrapLog log;
    uint32_t frames;
    bool top_ready;
    bool bottom_ready;
    bool gfx_ready;
    bool should_exit;
} PBBootstrap;

void pb_bootstrap_init(PBBootstrap *state);
void pb_bootstrap_log(PBBootstrap *state, const char *message);
void pb_bootstrap_apt_event(PBBootstrap *state, PBAptEvent event);
void pb_bootstrap_on_start(PBBootstrap *state);
void pb_bootstrap_tick(PBBootstrap *state);
bool pb_bootstrap_is_running(const PBBootstrap *state);
const char *pb_bootstrap_lifecycle_name(PBLifecycle lifecycle);
const char *pb_bootstrap_status_line(const PBBootstrap *state);

#ifdef __cplusplus
}
#endif
