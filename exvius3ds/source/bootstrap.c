#include "ex3/bootstrap.h"

#include "ex3/log.h"
#include "ex3/version.h"

#include <stdio.h>
#include <string.h>

Ex3PlatResult ex3_bootstrap_init(Ex3Bootstrap *boot, Ex3Plat *plat)
{
    Ex3PlatResult rc;
    Ex3Scene *scene;

    if (boot == NULL || plat == NULL) {
        return EX3_PLAT_ERR_INIT;
    }
    memset(boot, 0, sizeof(*boot));
    boot->plat = plat;

    rc = ex3_plat_init(plat);
    if (rc != EX3_PLAT_OK) {
        return rc;
    }

    ex3_log_init(plat);
    (void)ex3_plat_enable_new3ds_boost(plat);

    boot->director = ex3_director_create();
    scene = ex3_diag_scene_create();
    if (boot->director == NULL || scene == NULL) {
        ex3_director_destroy(boot->director);
        boot->director = NULL;
        ex3_plat_shutdown(plat);
        return EX3_PLAT_ERR_INIT;
    }
    ex3_director_run_with_scene(boot->director, scene);

    ex3_logf(EX3_LOG_INFO, "%s %s sha=%s utc=%s", EX3_NAME, EX3_VERSION_STRING,
             EX3_BUILD_SHA, EX3_BUILD_UTC);
    ex3_logf(EX3_LOG_INFO, "new3ds=%d boost=%d scene=s2-diag",
             ex3_plat_is_new_3ds(plat) ? 1 : 0, ex3_plat_boost_enabled(plat) ? 1 : 0);

    boot->ready = true;
    return EX3_PLAT_OK;
}

void ex3_bootstrap_shutdown(Ex3Bootstrap *boot)
{
    if (boot == NULL) {
        return;
    }
    if (boot->plat != NULL) {
        ex3_logf(EX3_LOG_INFO, "shutdown frames=%u", boot->frames_drawn);
    }
    ex3_director_destroy(boot->director);
    boot->director = NULL;
    if (boot->plat != NULL) {
        ex3_plat_shutdown(boot->plat);
    }
    boot->ready = false;
}

bool ex3_bootstrap_frame(Ex3Bootstrap *boot)
{
    if (boot == NULL || !boot->ready || boot->plat == NULL) {
        return false;
    }

    ex3_plat_poll(boot->plat);
    if (!ex3_plat_should_run(boot->plat) || boot->quit_requested) {
        return false;
    }

    if (ex3_plat_lifecycle(boot->plat) != EX3_LIFECYCLE_ACTIVE) {
        return true;
    }

    ex3_plat_begin_frame(boot->plat);
    ex3_plat_clear_screens(boot->plat, EX3_CLEAR_TOP_RGBA, EX3_CLEAR_BOTTOM_RGBA);
    ex3_director_draw(boot->director, boot->plat);
    ex3_plat_end_frame(boot->plat);
    boot->frames_drawn++;
    return true;
}
