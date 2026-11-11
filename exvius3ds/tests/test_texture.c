#include "ex3/texture.h"

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
    Ex3TextureInfo ok = ex3_texture_plan(300, 200);
    Ex3TextureInfo too_big = ex3_texture_plan(2048, 64);
    Ex3TextureInfo zero = ex3_texture_plan(0, 16);

    expect_true(ex3_texture_next_pot(1) == 1, "pot 1");
    expect_true(ex3_texture_next_pot(300) == 512, "pot 300");
    expect_true(ex3_texture_next_pot(1024) == 1024, "pot 1024");

    expect_true(ok.valid, "300x200 valid");
    expect_true(ok.gpu_width == 512 && ok.gpu_height == 256, "300x200 gpu pad");
    expect_true(!too_big.valid, "2048 rejected");
    expect_true(!zero.valid, "zero rejected");

    if (g_failures != 0) {
        fprintf(stderr, "%d failure(s)\n", g_failures);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
