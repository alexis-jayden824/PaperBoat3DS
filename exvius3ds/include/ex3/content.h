#ifndef EX3_CONTENT_H
#define EX3_CONTENT_H

#include "ex3/mst.h"
#include "ex3/plat.h"
#include "ex3/texpack.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    Ex3MstTable mst;
    Ex3TexPack tex;
    int mst_ok;
    int tex_ok;
} Ex3Content;

/* Load S3 diagnostic fixtures from RomFS. */
int ex3_content_load_diag(Ex3Content *content);

/* Draw bottom-screen status chrome from loaded content. */
void ex3_content_draw_bottom_status(const Ex3Content *content, Ex3Plat *plat);

#ifdef __cplusplus
}
#endif

#endif /* EX3_CONTENT_H */
