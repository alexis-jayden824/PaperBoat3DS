#include "ex3/fs.h"
#include "ex3/mst.h"
#include "ex3/texpack.h"

#include <stdio.h>
#include <stdlib.h>
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
    const char *root = getenv("EX3_HOST_ROOT");
    Ex3MstTable plain;
    Ex3MstTable enc;
    Ex3TexPack tex;

    if (root != NULL && root[0] != '\0') {
        ex3_fs_set_host_root(root);
    }

    expect_true(ex3_mst_load_path("romfs:/fixtures/diag.mst", &plain) == EX3_MST_OK,
                "load plain mst");
    expect_true(plain.record_count == 3, "3 records");
    expect_true(strcmp(plain.records[0].name, "Rain") == 0, "Rain");

    expect_true(ex3_mst_load_path("romfs:/fixtures/diag_enc.mst", &enc) == EX3_MST_OK,
                "load enc mst");
    expect_true(enc.record_count == 3, "enc 3 records");
    expect_true(strcmp(enc.records[2].name, "Fina") == 0, "Fina");
    expect_true((enc.flags & EX3_MST_FLAG_ENCRYPTED) != 0, "encrypted flag");

    expect_true(ex3_texpack_load_path("romfs:/fixtures/diag.tex", &tex) == EX3_TEXPACK_OK,
                "load tex");
    expect_true(tex.width == 16 && tex.height == 16, "tex size");
    expect_true(tex.plan.valid && tex.plan.gpu_width == 16, "tex plan");

    if (g_failures != 0) {
        fprintf(stderr, "%d failure(s)\n", g_failures);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
