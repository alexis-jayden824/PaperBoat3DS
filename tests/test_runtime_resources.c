#include "pb3ds/runtime_resources.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint32_t osGetMemRegionFree(int region) { (void)region; return PB_MIB(32); }
uint32_t linearSpaceFree(void) { return PB_MIB(16); }
void *linearAlloc(size_t size) { return malloc(size); }
void linearFree(void *p) { free(p); }
uint64_t osGetTime(void) { return 0; }
extern int test_upstream_resource_consumer(void);
size_t Sprite_GetPlayerSize(int32_t index);
void *Sprite_LoadPlayer(int32_t index, void *destination, size_t size);
size_t Sprite_GetNPCSize(int32_t index);
void *Sprite_LoadNPC(int32_t index, void *destination, size_t size);
void GameEngine_InvalidateTextureCache(const void *address) { (void)address; }

typedef struct {
    void *image;
    uint8_t width;
    uint8_t height;
    int8_t palette;
    int8_t quad_cache_index;
} TestNativeSpriteRaster;

static uint64_t test_path_crc64(const char *text) {
    uint64_t crc = UINT64_MAX;
    while (*text != '\0') {
        crc ^= (uint64_t)(uint8_t)*text++ << 56U;
        for (unsigned int bit = 0; bit < 8; bit++) {
            crc = (crc & (UINT64_C(1) << 63U)) != 0U
                      ? (crc << 1U) ^ UINT64_C(0x42F0E1EBA9EA3693)
                      : crc << 1U;
        }
    }
    return crc;
}

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
    assert(pb_runtime_resources_prepare(&resources));
    assert(resources.index != NULL && resources.index_count != 0);
    const size_t index_used = memory.snapshot.class_used[PB_MEMORY_SCENE];
    assert(pb_runtime_resource_exists("__OTR__le/blob"));
    assert(!pb_runtime_resource_exists("absent"));
    assert(resources.count == 0);
    assert(memory.snapshot.class_used[PB_MEMORY_SCENE] == index_used);
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

    /* Player image offsets can point outside the sprite blob because their
     * fallback data lives in player_raster_image_data.  An indexed external
     * raster path must be retained without decoding the texture early. */
    const size_t player_size = Sprite_GetPlayerSize(1);
    assert(player_size != 0);
    void *player = malloc(player_size);
    assert(player != NULL);
    const size_t player_resource_count = resources.count;
    assert(Sprite_LoadPlayer(1, player, player_size) == player);
    assert(resources.count == player_resource_count);
    void **rasters = ((void ***)player)[0];
    TestNativeSpriteRaster *raster = rasters[0];
    assert(raster != NULL && raster->width == 8 && raster->height == 8);
    assert(strcmp(raster->image,
                  "__OTR__sprites/player_sprite_1_raster_0") == 0);
    free(player);

    const char *raster_name = "sprites/player_sprite_1_raster_0";
    const uint64_t raster_hash = test_path_crc64(raster_name);
    uint8_t *raster_data = ResourceGetDataByCrc(raster_hash);
    assert(raster_data != NULL && raster_data[0] == 'x');
    assert(strcmp(ResourceGetNameByCrc(raster_hash), raster_name) == 0);
    assert(resources.count == player_resource_count + 1);

    /* The same out-of-range image offset remains invalid for an NPC sprite,
     * whose fallback pixels must be local to its own blob. */
    const size_t npc_size = Sprite_GetNPCSize(1);
    assert(npc_size != 0);
    void *npc = malloc(npc_size);
    assert(npc != NULL && Sprite_LoadNPC(1, npc, npc_size) == NULL);
    free(npc);
    pb_runtime_resources_clear(&resources);
    assert(resources.count == 0 && resources.head == NULL &&
           resources.index == NULL && resources.index_count == 0);
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
    puts("Runtime resources: index, endian, pointer lifetime, sparse sprites, "
         "malformed input, failure/retry, and upstream shape consumer passed");
    return 0;
}
