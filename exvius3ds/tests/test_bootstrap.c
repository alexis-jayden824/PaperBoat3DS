#include "ex3/bootstrap.h"
#include "ex3/fs.h"
#include "ex3/plat.h"

#include <stdio.h>
#include <stdlib.h>

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
    const char *root = getenv("EX3_HOST_ROOT");
    Ex3Plat *plat = ex3_plat_create();
    Ex3Bootstrap boot;
    int i;

    if (root != NULL && root[0] != '\0') {
        ex3_fs_set_host_root(root);
    }

    expect_true(plat != NULL, "plat create");
    expect_true(ex3_bootstrap_init(&boot, plat) == EX3_PLAT_OK, "bootstrap init");
    expect_true(boot.ready, "ready");
    expect_true(boot.content.mst_ok, "mst loaded");
    expect_true(boot.content.tex_ok, "tex loaded");

    for (i = 0; i < 3; i++) {
        expect_true(ex3_bootstrap_frame(&boot), "frame runs");
    }
    expect_true(boot.frames_drawn == 3, "three frames");
    expect_true(ex3_plat_frame_info(plat)->top_rgba == EX3_CLEAR_TOP_RGBA,
                "top clear color");
    expect_true(ex3_plat_frame_info(plat)->bottom_rgba == EX3_CLEAR_BOTTOM_RGBA,
                "bottom clear color");
    /* top sprite+label (2) + bottom status strip + 3 name bars + tex chip (5) = 7 */
    expect_true(ex3_plat_frame_info(plat)->draw_rect_count == 7,
                "top scene + bottom content rects");
    expect_true(boot.director != NULL, "director installed");

    ex3_plat_debug_set_lifecycle(plat, EX3_LIFECYCLE_SUSPENDED);
    expect_true(ex3_bootstrap_frame(&boot), "frame while suspended");
    expect_true(boot.frames_drawn == 3, "no draw while suspended");

    ex3_plat_debug_request_exit(plat);
    expect_true(!ex3_bootstrap_frame(&boot), "stop after exit");

    ex3_bootstrap_shutdown(&boot);
    ex3_plat_destroy(plat);

    if (g_failures != 0) {
        fprintf(stderr, "%d failure(s)\n", g_failures);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
