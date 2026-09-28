/* Deliberately compiled against the pinned game's real types. */
#include "port/shape_loader.h"
#include "port/Engine.h"
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include "pb3ds/runtime_resource_types.h"

_Static_assert(sizeof(PBRuntimeGfx) == sizeof(Gfx), "native packet stride");
_Static_assert(_Alignof(PBRuntimeGfx) == _Alignof(Gfx), "native packet alignment");
_Static_assert(offsetof(PBRuntimeGfx, words.w1) == offsetof(Gfx, words.w1), "packet word offset");
_Static_assert(sizeof(Vtx) == 16, "native vertex stride");

void GameEngine_LogInfo(const char *fmt, ...) { (void)fmt; }

int test_upstream_resource_consumer(void) {
    ShapeFile *shape = calloc(1, sizeof(*shape));
    assert(shape != NULL);
    void *raw = ResourceGetDataByName("__OTR__shapes/mac_00_shape");
    assert(raw != NULL);
    Shape_LoadFromRawData(shape, raw, ResourceGetSizeByName("shapes/mac_00_shape"), "mac_00_shape");
    assert(shape->header.root != NULL);
    ModelNode *leaf = shape->header.root->groupData->childList[0];
    assert(leaf->displayData != NULL);
    Gfx *dl = leaf->displayData->displayList;
    assert(dl == ResourceGetDataByName("shapes/mac_00_shape/dlist_20"));
    assert(dl[0].words.w0 == 0xE7000000 && dl[1].words.w0 == 0xDF000000);
    Vtx *v = ResourceGetDataByName("be/vertex");
    assert(v[0].v.ob[0] == -7 && v[0].v.tc[1] == 96 && v[0].v.cn[3] == 255);
    free(shape);
    return 0;
}
