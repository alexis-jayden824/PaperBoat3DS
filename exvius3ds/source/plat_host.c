/* Host / unit-test implementation of ex3_plat (no libctru). */
#include "plat_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Ex3Plat *ex3_plat_create(void)
{
    Ex3Plat *plat = (Ex3Plat *)calloc(1, sizeof(Ex3Plat));
    if (plat != NULL) {
        ex3_plat_common_reset(plat);
    }
    return plat;
}

void ex3_plat_destroy(Ex3Plat *plat)
{
    if (plat == NULL) {
        return;
    }
    ex3_plat_shutdown(plat);
    free(plat);
}

Ex3PlatResult ex3_plat_init(Ex3Plat *plat)
{
    if (plat == NULL) {
        return EX3_PLAT_ERR_INIT;
    }
    if (plat->initialized) {
        return EX3_PLAT_OK;
    }

    /* Host assumes New 3DS XL profile for recomp bring-up tests. */
    plat->is_new_3ds = true;
    plat->boost_enabled = false;
    plat->lifecycle = EX3_LIFECYCLE_ACTIVE;
    plat->exit_requested = false;
    memset(&plat->frame, 0, sizeof(plat->frame));
    plat->frame.top_rgba = EX3_CLEAR_TOP_RGBA;
    plat->frame.bottom_rgba = EX3_CLEAR_BOTTOM_RGBA;

    snprintf(plat->log_path, sizeof(plat->log_path), "%s", "exvius3ds-host.log");
    plat->log_file = fopen(plat->log_path, "w");
    if (plat->log_file == NULL) {
        return EX3_PLAT_ERR_IO;
    }

    plat->initialized = true;
    return EX3_PLAT_OK;
}

void ex3_plat_shutdown(Ex3Plat *plat)
{
    if (plat == NULL || !plat->initialized) {
        return;
    }
    if (plat->log_file != NULL) {
        fclose(plat->log_file);
        plat->log_file = NULL;
    }
    plat->initialized = false;
}

Ex3PlatResult ex3_plat_enable_new3ds_boost(Ex3Plat *plat)
{
    if (plat == NULL || !plat->initialized) {
        return EX3_PLAT_ERR_STATE;
    }
    if (!plat->is_new_3ds) {
        return EX3_PLAT_ERR_STATE;
    }
    plat->boost_enabled = true;
    return EX3_PLAT_OK;
}

void ex3_plat_poll(Ex3Plat *plat)
{
    (void)plat;
}

void ex3_plat_begin_frame(Ex3Plat *plat)
{
    if (plat == NULL || !plat->initialized) {
        return;
    }
    plat->frame.screens_cleared = false;
}

void ex3_plat_clear_screens(Ex3Plat *plat, uint32_t top_rgba, uint32_t bottom_rgba)
{
    if (plat == NULL || !plat->initialized) {
        return;
    }
    plat->frame.top_rgba = top_rgba;
    plat->frame.bottom_rgba = bottom_rgba;
    plat->frame.screens_cleared = true;
}

void ex3_plat_end_frame(Ex3Plat *plat)
{
    if (plat == NULL || !plat->initialized) {
        return;
    }
    if (plat->frame.screens_cleared) {
        plat->frame.frame_count++;
    }
}
