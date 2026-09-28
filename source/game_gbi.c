#ifdef PB3DS_GAME_OBJECTS

#include "pb3ds/gbi_resolve.h"
#include "pb3ds/log.h"

#include "ultra64.h"
#include "gbi_custom.h"
#include <string.h>

uint8_t GameEngine_OTRSigCheck(const char *data);
void *ResourceGetDataByName(const char *name);

_Static_assert(sizeof(Gfx) == sizeof(PBGbiPacket),
               "PaperBoat Gfx packet must match the ARM11 GBI walker");

void gSPVertexOTR(Gfx *packet, uintptr_t vertices, int count, int first) {
    if (GameEngine_OTRSigCheck((const char *)vertices)) {
        vertices = (uintptr_t)ResourceGetDataByName((const char *)vertices);
    }
    if (vertices == 0U) {
        pb_log(PB_LOG_WARNING, "gbi", "missing vertex resource");
        memset(packet, 0, sizeof(*packet));
        return;
    }
    __gSPVertex(packet, vertices, count, first);
}

void gSPDisplayListOTR(Gfx *packet, const void *display_list) {
    if (GameEngine_OTRSigCheck((const char *)display_list)) {
        display_list = ResourceGetDataByName((const char *)display_list);
    }
    if (display_list == NULL) {
        pb_log(PB_LOG_WARNING, "gbi", "missing display-list resource");
        memset(packet, 0, sizeof(*packet));
        return;
    }
    __gSPDisplayList(packet, display_list);
}

void gDPSetTextureImageOTR(Gfx *packet, int fmt, int siz, int width,
                           uintptr_t image) {
    if (GameEngine_OTRSigCheck((const char *)image)) {
        image = (uintptr_t)ResourceGetDataByName((const char *)image);
    }
    if (image == 0U) {
        pb_log(PB_LOG_WARNING, "gbi", "missing texture resource");
        memset(packet, 0, sizeof(*packet));
        return;
    }
    gSetImage(packet, G_SETTIMG, fmt, siz, width, image);
}

void gbi_resolve_vtx_in_static_dl(Gfx *display_list) {
    pb_gbi_resolve_vtx_in_static_dl((PBGbiPacket *)display_list,
                                   GameEngine_OTRSigCheck,
                                   ResourceGetDataByName);
}

#endif
