#include "ex3/plat.h"

#include <stdio.h>
#include <string.h>

static int g_failures;

static void expect_true(int cond, const char *name)
{
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", name);
        g_failures++;
    } else {
        printf("ok: %s\n", name);
    }
}

int main(void)
{
    Ex3Plat *plat = ex3_plat_create();
    const Ex3PlatFrameInfo *info;

    expect_true(plat != NULL, "plat create");
    expect_true(ex3_plat_init(plat) == EX3_PLAT_OK, "plat init");
    expect_true(ex3_plat_is_new_3ds(plat), "host assumes new3ds");
    expect_true(ex3_plat_enable_new3ds_boost(plat) == EX3_PLAT_OK, "boost");
    expect_true(ex3_plat_boost_enabled(plat), "boost enabled");
    expect_true(ex3_plat_should_run(plat), "should run");

    ex3_plat_begin_frame(plat);
    ex3_plat_clear_screens(plat, EX3_CLEAR_TOP_RGBA, EX3_CLEAR_BOTTOM_RGBA);
    ex3_plat_end_frame(plat);

    info = ex3_plat_frame_info(plat);
    expect_true(info != NULL, "frame info");
    expect_true(info->screens_cleared, "screens cleared");
    expect_true(info->frame_count == 1, "frame count");
    expect_true(info->top_rgba == EX3_CLEAR_TOP_RGBA, "top color");
    expect_true(info->bottom_rgba == EX3_CLEAR_BOTTOM_RGBA, "bottom color");

    expect_true(ex3_plat_log_line(plat, "s1-host") == EX3_PLAT_OK, "log line");

    ex3_plat_debug_set_lifecycle(plat, EX3_LIFECYCLE_SUSPENDED);
    expect_true(ex3_plat_lifecycle(plat) == EX3_LIFECYCLE_SUSPENDED, "suspend");
    expect_true(ex3_plat_should_run(plat), "run while suspended");

    ex3_plat_debug_request_exit(plat);
    expect_true(!ex3_plat_should_run(plat), "stop on exit");

    ex3_plat_destroy(plat);

    if (g_failures != 0) {
        fprintf(stderr, "%d failure(s)\n", g_failures);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
