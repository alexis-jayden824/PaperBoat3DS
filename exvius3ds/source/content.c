#include "ex3/content.h"

#include "ex3/log.h"

#include <stdio.h>
#include <string.h>

int ex3_content_load_diag(Ex3Content *content)
{
    Ex3MstResult mr;
    Ex3TexPackResult tr;

    if (content == NULL) {
        return -1;
    }
    memset(content, 0, sizeof(*content));

    mr = ex3_mst_load_path("romfs:/fixtures/diag_enc.mst", &content->mst);
    content->mst_ok = (mr == EX3_MST_OK && content->mst.loaded) ? 1 : 0;
    if (!content->mst_ok) {
        /* Fall back to plaintext fixture. */
        mr = ex3_mst_load_path("romfs:/fixtures/diag.mst", &content->mst);
        content->mst_ok = (mr == EX3_MST_OK && content->mst.loaded) ? 1 : 0;
    }

    tr = ex3_texpack_load_path("romfs:/fixtures/diag.tex", &content->tex);
    content->tex_ok = (tr == EX3_TEXPACK_OK && content->tex.loaded) ? 1 : 0;

    ex3_logf(EX3_LOG_INFO, "content mst_ok=%d records=%u tex_ok=%d %ux%u",
             content->mst_ok, content->mst.record_count, content->tex_ok,
             content->tex.width, content->tex.height);
    return (content->mst_ok && content->tex_ok) ? 0 : -1;
}

void ex3_content_draw_bottom_status(const Ex3Content *content, Ex3Plat *plat)
{
    uint32_t i;
    float y;

    if (content == NULL || plat == NULL) {
        return;
    }

    /* Status strip */
    ex3_plat_draw_rect(plat, EX3_SCREEN_BOTTOM, 8.0f, 8.0f, 304.0f, 28.0f,
                       content->mst_ok ? 0x2E7D32FF : 0xC62828FF);

    /* One bar per MST record name (visual stand-in for text). */
    y = 48.0f;
    if (content->mst_ok) {
        for (i = 0; i < content->mst.record_count && i < 8; i++) {
            float w = 40.0f + (float)(content->mst.records[i].id * 24U);
            if (w > 300.0f) {
                w = 300.0f;
            }
            ex3_plat_draw_rect(plat, EX3_SCREEN_BOTTOM, 12.0f, y, w, 18.0f,
                               0x90CAF9FF);
            y += 26.0f;
        }
    }

    /* Texpack status chip */
    ex3_plat_draw_rect(plat, EX3_SCREEN_BOTTOM, 8.0f, 200.0f, 64.0f, 28.0f,
                       content->tex_ok ? EX3_DIAG_SPRITE_RGBA : 0xC62828FF);
}
