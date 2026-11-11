#include "ex3/cocos.h"
#include "ex3/plat.h"

#include <stdio.h>

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
    Ex3Director *director = ex3_director_create();
    Ex3Scene *scene = ex3_diag_scene_create();
    const Ex3PlatFrameInfo *info;

    expect_true(plat != NULL && director != NULL && scene != NULL, "alloc");
    expect_true(ex3_plat_init(plat) == EX3_PLAT_OK, "plat init");

    ex3_director_run_with_scene(director, scene);
    expect_true(ex3_director_running_scene(director) == scene, "running scene");

    ex3_plat_begin_frame(plat);
    ex3_plat_clear_screens(plat, EX3_CLEAR_TOP_RGBA, EX3_CLEAR_BOTTOM_RGBA);
    ex3_director_draw(director, plat);
    ex3_plat_end_frame(plat);

    info = ex3_plat_frame_info(plat);
    expect_true(info->draw_rect_count == 2, "sprite+label rects");
    expect_true(info->last_draw_rgba == EX3_DIAG_LABEL_RGBA, "label drawn last");
    expect_true(info->top_rgba == EX3_CLEAR_TOP_RGBA, "top clear retained");

    ex3_director_destroy(director);
    ex3_plat_destroy(plat);

    if (g_failures != 0) {
        fprintf(stderr, "%d failure(s)\n", g_failures);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
