#include "plat_common.h"

#include <string.h>

void ex3_plat_common_reset(Ex3Plat *plat)
{
    if (plat == NULL) {
        return;
    }
    memset(plat, 0, sizeof(*plat));
    plat->lifecycle = EX3_LIFECYCLE_ACTIVE;
    plat->frame.top_rgba = EX3_CLEAR_TOP_RGBA;
    plat->frame.bottom_rgba = EX3_CLEAR_BOTTOM_RGBA;
}

bool ex3_plat_is_new_3ds(const Ex3Plat *plat)
{
    return plat != NULL && plat->is_new_3ds;
}

bool ex3_plat_boost_enabled(const Ex3Plat *plat)
{
    return plat != NULL && plat->boost_enabled;
}

Ex3PlatLifecycle ex3_plat_lifecycle(const Ex3Plat *plat)
{
    if (plat == NULL) {
        return EX3_LIFECYCLE_EXITING;
    }
    return plat->lifecycle;
}

bool ex3_plat_should_run(const Ex3Plat *plat)
{
    if (plat == NULL || !plat->initialized) {
        return false;
    }
    if (plat->exit_requested || plat->lifecycle == EX3_LIFECYCLE_EXITING) {
        return false;
    }
    return true;
}

const Ex3PlatFrameInfo *ex3_plat_frame_info(const Ex3Plat *plat)
{
    if (plat == NULL) {
        return NULL;
    }
    return &plat->frame;
}

void ex3_plat_debug_set_lifecycle(Ex3Plat *plat, Ex3PlatLifecycle life)
{
    if (plat == NULL) {
        return;
    }
    plat->lifecycle = life;
    if (life == EX3_LIFECYCLE_EXITING) {
        plat->exit_requested = true;
    }
}

void ex3_plat_debug_request_exit(Ex3Plat *plat)
{
    if (plat == NULL) {
        return;
    }
    plat->exit_requested = true;
    plat->lifecycle = EX3_LIFECYCLE_EXITING;
}

Ex3PlatResult ex3_plat_log_line(Ex3Plat *plat, const char *line)
{
    if (plat == NULL || line == NULL || !plat->initialized) {
        return EX3_PLAT_ERR_STATE;
    }
    if (plat->log_file == NULL) {
        return EX3_PLAT_ERR_IO;
    }
    if (fprintf(plat->log_file, "%s\n", line) < 0) {
        return EX3_PLAT_ERR_IO;
    }
    fflush(plat->log_file);
    return EX3_PLAT_OK;
}
