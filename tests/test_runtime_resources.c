#include "pb3ds/runtime_resources.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

u32 osGetMemRegionFree(int region) { (void)region; return PB_MIB(32); }
u32 linearSpaceFree(void) { return PB_MIB(16); }
void *linearAlloc(size_t size) { return malloc(size); }
void linearFree(void *p) { free(p); }
u64 osGetTime(void) { return 0; }
extern int test_upstream_resource_consumer(void);

int main(int argc, char **argv) {
    assert(argc == 2);
    PBArchive archive;
    assert(pb_archive_open(&archive, argv[1]));
    PBMemoryMonitor memory;
    pb_memory_monitor_init(&memory, (uintptr_t)&memory);
    PBRuntimeResources resources;
    pb_runtime_resources_init(&resources, &archive, &memory);
    assert(ResourceGetDataByName("le/blob") == NULL);
    pb_runtime_resources_bind(&resources);
    for (unsigned i = 0; i < 2; i++) {
        const char *tag = i ? "be" : "le";
        char name[96];
        snprintf(name, sizeof(name), "%s/blob", tag);
        unsigned char *blob = ResourceGetDataByName(name);
        assert(blob && memcmp(blob, "abc\0", 4) == 0);
        assert(ResourceGetSizeByName(name) == 20); /* upstream includes padding */
        for (unsigned j = 4; j < 20; j++) assert(blob[j] == 0);
        blob[0] = 'Z';
        snprintf(name, sizeof(name), "__OTR__%s/blob", tag);
        assert(GameEngine_GetDataExact(name) == blob && blob[0] == 'Z');
        snprintf(name, sizeof(name), "%s/vertex", tag);
        int16_t *vertex = ResourceGetDataByName(name);
        assert(vertex && vertex[0] == -7 && vertex[1] == 300 && vertex[2] == -123);
        assert(vertex[3] == 9 && vertex[4] == -32 && vertex[5] == 96);
        assert(((uint8_t*)vertex)[15] == 255);
        assert(ResourceGetSizeByName(name) == 16);
        snprintf(name, sizeof(name), "%s/texture", tag);
        uint8_t *texture = GameEngine_GetDataExact(name);
        assert(texture && texture[0] == 0xF8 && texture[1] == 1);
        assert(GameEngine_GetTexWidthExact(name) == 1 && GameEngine_GetTexHeightExact(name) == 1);
        assert(GameEngine_GetSizeExact(name) == 2);
        snprintf(name, sizeof(name), "%s/dl", tag);
        PBRuntimeGfx *dl = ResourceGetDataByName(name);
        assert(dl && dl[0].words.w0 == 0x33000000 && dl[1].words.w0 == 0xDF123456);
        assert(dl[1].words.w1 == 0xCAFEBABE && dl[2].words.w0 == 0xDF000000);
        assert(ResourceGetSizeByName(name) == 3 * sizeof(*dl));
    }
    const size_t count = resources.count;
    const size_t used = memory.snapshot.class_used[PB_MEMORY_SCENE];
    const char *bad[] = {"bad/version", "bad/type", "bad/blob", "bad/vertex", "bad/texture", "bad/dl", "absent", "", NULL};
    for (unsigned i = 0; i < sizeof(bad)/sizeof(bad[0]); i++) {
        assert(ResourceGetDataByName(bad[i]) == NULL && resources.error != NULL);
        assert(resources.count == count && memory.snapshot.class_used[PB_MEMORY_SCENE] == used);
        assert(memory.snapshot.class_used[PB_MEMORY_TRANSIENT] == 0);
    }
    /* Calls the pinned Shape_LoadFromRawData, not a local reimplementation. */
    assert(test_upstream_resource_consumer() == 0);
    pb_runtime_resources_clear(&resources);
    assert(resources.count == 0 && resources.head == NULL);
    assert(memory.snapshot.class_used[PB_MEMORY_SCENE] == 0);
    assert(ResourceGetDataByName("le/blob") == NULL);
    pb_runtime_resources_bind(&resources);
    memory.snapshot.class_used[PB_MEMORY_SCENE] = PB_MEMORY_SCENE_LIMIT;
    assert(ResourceGetDataByName("le/blob") == NULL);
    assert(memory.snapshot.class_used[PB_MEMORY_TRANSIENT] == 0 && resources.count == 0);
    memory.snapshot.class_used[PB_MEMORY_SCENE] = 0;
    assert(ResourceGetDataByName("le/blob") != NULL);
    pb_runtime_resources_clear(&resources);
    pb_archive_close(&archive);
    puts("Runtime resources: endian, pointer lifetime, malformed input, failure/retry, and upstream shape consumer passed");
    return 0;
}
