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
void is_debug_panic(const char *message) { (void)message; abort(); }
extern int test_upstream_resource_consumer(void);
size_t Sprite_GetPlayerSize(int32_t index);
void *Sprite_LoadPlayer(int32_t index, void *destination, size_t size);
size_t Sprite_GetNPCSize(int32_t index);
void *Sprite_LoadNPC(int32_t index, void *destination, size_t size);
bool PB3DS_RuntimeValidatePlayerRasterTables(void);
int32_t Sprite_GetPlayerRasterHeader(int32_t *output);
int32_t Sprite_GetPlayerRasterSets(int32_t *output, int32_t maximum);
int32_t Sprite_GetPlayerRasterLoadDescriptors(int32_t sprite_index,
                                               int32_t start,
                                               int32_t *output,
                                               int32_t count);
void *Sprite_GetPlayerRasterPath(int32_t sprite_index,
                                 int32_t raster_index);
int32_t Sprite_LoadPlayerRaster(int32_t offset, void *destination,
                                int32_t size);
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
    assert(resources.loaded_buckets != NULL &&
           resources.loaded_bucket_count != 0);
    const size_t index_used = memory.snapshot.class_used[PB_MEMORY_SCENE];
    assert(pb_runtime_resource_exists("__OTR__le/blob"));
    assert(!pb_runtime_resource_exists("absent"));
    assert(strcmp(resources.failed_name, "absent") == 0);
    assert(resources.count == 0);
    assert(memory.snapshot.class_used[PB_MEMORY_SCENE] == index_used);
    const PBRuntimeResourceRequirement valid_requirements[] = {
        { "le/blob", 0x4F424C42U, 0U, 0U, 0U, 4U },
        { "le/texture", 0x4F544558U, 2U, 1U, 1U, 2U },
    };
    assert(pb_runtime_resources_validate(
        &resources, valid_requirements,
        sizeof(valid_requirements) / sizeof(valid_requirements[0])));
    const PBRuntimeResourceRequirement wrong_type = {
        "le/blob", 0x4F565458U, 0U, 0U, 0U, 1U
    };
    assert(!pb_runtime_resources_validate(&resources, &wrong_type, 1U));
    assert(strcmp(resources.error, "required resource contract mismatch") == 0);
    assert(strcmp(resources.failed_name, "le/blob") == 0);
    const PBRuntimeResourceRequirement wrong_dimensions = {
        "le/texture", 0x4F544558U, 2U, 2U, 1U, 2U
    };
    assert(!pb_runtime_resources_validate(&resources, &wrong_dimensions, 1U));
    assert(strcmp(resources.failed_name, "le/texture") == 0);
    const PBRuntimeResourceRequirement missing_requirement = {
        "required/missing", 0U, 0U, 0U, 0U, 0U
    };
    assert(!pb_runtime_resources_validate(&resources, &missing_requirement, 1U));
    assert(strcmp(resources.error, "required resource missing") == 0);
    assert(strcmp(resources.failed_name, "required/missing") == 0);
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
        snprintf(name, sizeof(name), "%s/matrix", tag);
        uint32_t *matrix = ResourceGetDataByName(name);
        assert(matrix && matrix[0] == UINT32_C(0x00010000) &&
               matrix[5] == UINT32_C(0x00010000) &&
               matrix[10] == UINT32_C(0x00010000) &&
               matrix[15] == UINT32_C(0x00010000));
        assert(ResourceGetSizeByName(name) == 16 * sizeof(*matrix));
        snprintf(name, sizeof(name), "%s/vec3s", tag);
        int16_t *vectors = ResourceGetDataByName(name);
        assert(vectors && vectors[0] == -1 && vectors[1] == 2 &&
               vectors[2] == -300 && vectors[3] == 32767 &&
               vectors[4] == -32768 && vectors[5] == 1234);
        assert(ResourceGetSizeByName(name) == 6 * sizeof(*vectors));
        snprintf(name, sizeof(name), "%s/lights", tag);
        uint8_t *lights = ResourceGetDataByName(name);
        assert(lights && lights[0] == 0 && lights[7] == 7 &&
               lights[8] == 8 && lights[23] == 23);
        assert(ResourceGetSizeByName(name) == 24);
        snprintf(name, sizeof(name), "%s/viewport", tag);
        int16_t *viewport = ResourceGetDataByName(name);
        assert(viewport && viewport[0] == 640 && viewport[1] == 480 &&
               viewport[2] == 511 && viewport[4] == 640 &&
               viewport[5] == 480);
        assert(ResourceGetSizeByName(name) == 16);
        snprintf(name, sizeof(name), "%s/dl", tag);
        PBRuntimeGfx *dl = ResourceGetDataByName(name);
        assert(dl && dl[0].words.w0 == 0x33000000 && dl[1].words.w0 == 0xDF123456);
        assert(dl[1].words.w1 == 0xCAFEBABE && dl[2].words.w0 == 0xDF000000);
        assert(ResourceGetSizeByName(name) == 3 * sizeof(*dl));
        snprintf(name, sizeof(name), "%s/span-dl", tag);
        dl = ResourceGetDataByName(name);
        assert(dl && dl[0].words.w0 == 0xE4000000 &&
               dl[1].words.w0 == 0xDF111111 &&
               dl[3].words.w0 == 0x3E000001 &&
               dl[4].words.w0 == 0xDF000000 &&
               dl[5].words.w0 == 0xDF000000);
        assert(ResourceGetSizeByName(name) == 6 * sizeof(*dl));
    }
    const size_t count = resources.count;
    const size_t used = memory.snapshot.class_used[PB_MEMORY_SCENE];
    const char *bad[] = {
        "bad/version", "bad/type", "bad/blob", "bad/vertex",
        "bad/texture", "bad/matrix", "bad/lights", "bad/viewport",
        "bad/vec3s",
        "bad/dl", "bad/truncated-span-dl",
        "absent", "", NULL
    };
    for (unsigned i = 0; i < sizeof(bad)/sizeof(bad[0]); i++) {
        assert(ResourceGetDataByName(bad[i]) == NULL && resources.error != NULL);
        assert(resources.count == count && memory.snapshot.class_used[PB_MEMORY_SCENE] == used);
        assert(memory.snapshot.class_used[PB_MEMORY_TRANSIENT] == 0);
    }
    /* Calls the pinned Shape_LoadFromRawData, not a local reimplementation. */
    assert(test_upstream_resource_consumer() == 0);

    /* Startup walks the actual shape tree and recursively decodes every
     * Fast3D hash plus map texture/palette before mutating game state. */
    assert(pb_runtime_resources_validate_shape_closure(
        &resources, "shapes/closure_shape", "runtime_tex"));
    assert(!pb_runtime_resources_validate_shape_closure(
        &resources, "shapes/missing_shape", "runtime_tex"));
    assert(strcmp(resources.error, "shape closure resource missing") == 0);
    assert(strcmp(resources.failed_name,
                  "shapes/missing_shape/dlist_20") == 0);
    assert(!pb_runtime_resources_validate_shape_closure(
        &resources, "shapes/wrong_shape", "runtime_tex"));
    assert(strcmp(resources.error,
                  "display-list hash resource type mismatch") == 0);
    assert(strcmp(resources.failed_name, "le/blob") == 0);
    assert(!pb_runtime_resources_validate_shape_closure(
        &resources, "shapes/hash_missing_shape", "runtime_tex"));
    assert(strcmp(resources.error, "display-list hash resource missing") == 0);
    assert(strncmp(resources.failed_name, "crc64:", 6) == 0);
    assert(!pb_runtime_resources_validate_shape_closure(
        &resources, "shapes/closure_shape", "missing_tex"));
    assert(strcmp(resources.error, "shape closure resource missing") == 0);
    assert(strcmp(resources.failed_name,
                  "textures/missing_tex/test_tex") == 0);

    /* Player image offsets can point outside the sprite blob because their
     * pixels live in indexed companion resources. The size/preflight pass
     * decodes and validates that companion, and retains its cache-owned name. */
    assert(PB3DS_RuntimeValidatePlayerRasterTables());
    int32_t raster_header[3] = { -1, -1, -1 };
    assert(Sprite_GetPlayerRasterHeader(raster_header) == 1);
    assert(raster_header[0] == 0 && raster_header[1] == 0 &&
           raster_header[2] == 0x100);
    int32_t raster_sets[4] = { -1, -1, -1, -1 };
    assert(Sprite_GetPlayerRasterSets(raster_sets, 4) == 4);
    assert(raster_sets[0] == 0 && raster_sets[1] == 1 &&
           raster_sets[2] == 2 && raster_sets[3] == 3);
    int32_t descriptor = 0;
    assert(Sprite_GetPlayerRasterLoadDescriptors(1, raster_sets[1],
                                                  &descriptor, 1) == 1);
    assert((uint32_t)descriptor == UINT32_C(0x00200120));
    assert(Sprite_GetPlayerRasterLoadDescriptors(1, raster_sets[1] + 1,
                                                  &descriptor, 1) == 0);
    assert(Sprite_GetPlayerRasterLoadDescriptors(1, raster_sets[1],
                                                  &descriptor, 2) == 0);
    uint8_t raster_copy[32];
    memset(raster_copy, 0xFF, sizeof(raster_copy));
    assert(Sprite_LoadPlayerRaster(0x20, raster_copy,
                                   sizeof(raster_copy)) == 1);
    for (size_t index = 0U; index < sizeof(raster_copy); index++) {
        assert(raster_copy[index] == 0U);
    }
    assert(Sprite_LoadPlayerRaster(-1, raster_copy,
                                   sizeof(raster_copy)) == 0);
    assert(Sprite_LoadPlayerRaster(13 * 0x20 - 16, raster_copy,
                                   sizeof(raster_copy)) == 0);
    assert(Sprite_GetPlayerRasterPath(1, 0) ==
           (void *)pb_runtime_resource_otr_name(
               "sprites/player_sprite_1_raster_0"));
    assert(Sprite_GetPlayerRasterPath(2, 0) == NULL);
    uint32_t *player_sets =
        ResourceGetDataByName("sprites/player_raster_sets");
    assert(player_sets != NULL);
    const uint32_t saved_player_set = player_sets[2];
    player_sets[2] = 0;
    assert(!PB3DS_RuntimeValidatePlayerRasterTables());
    player_sets[2] = saved_player_set;
    uint32_t *player_descriptors =
        ResourceGetDataByName("sprites/player_raster_load_descriptors");
    assert(player_descriptors != NULL);
    const uint32_t saved_descriptor = player_descriptors[0];
    player_descriptors[0] = 0;
    assert(!PB3DS_RuntimeValidatePlayerRasterTables());
    player_descriptors[0] = saved_descriptor;
    assert(PB3DS_RuntimeValidatePlayerRasterTables());
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
    assert(raster->image ==
           (void *)pb_runtime_resource_otr_name(
               "sprites/player_sprite_1_raster_0"));
    free(player);

    /* A normal player raster whose companion texture is missing must fail
     * conversion instead of retaining its out-of-allocation ROM offset. */
    assert(Sprite_GetPlayerSize(2) == 0);

    /* Torch deliberately omits 255x255 placeholder rasters. Keep those as a
     * null, non-drawable entry without inventing pixels or an invalid pointer. */
    const size_t placeholder_player_size = Sprite_GetPlayerSize(3);
    assert(placeholder_player_size != 0);
    player = malloc(placeholder_player_size);
    assert(player != NULL);
    assert(Sprite_LoadPlayer(3, player, placeholder_player_size) == player);
    rasters = ((void ***)player)[0];
    raster = rasters[0];
    assert(raster != NULL && raster->width == UINT8_MAX &&
           raster->height == UINT8_MAX && raster->image == NULL);
    free(player);

    /* A valid texture envelope with dimensions that disagree with the sprite
     * raster metadata is also rejected during conversion. */
    assert(Sprite_GetPlayerSize(4) == 0);

    const char *raster_name = "sprites/player_sprite_1_raster_0";
    const uint64_t raster_hash = test_path_crc64(raster_name);
    const size_t resource_count_before_crc = resources.count;
    uint8_t *raster_data = ResourceGetDataByCrc(raster_hash);
    assert(raster_data != NULL && raster_data[0] == 0x12);
    const size_t probes_before_cached_crc = resources.lookup_probes;
    const size_t hits_before_cached_crc = resources.hits;
    assert(strcmp(ResourceGetNameByCrc(raster_hash), raster_name) == 0);
    assert(resources.hits == hits_before_cached_crc + 1);
    assert(resources.lookup_probes - probes_before_cached_crc < 8);
    assert(resources.count == resource_count_before_crc);
    assert(ResourceGetDataByCrc(UINT64_C(0x123456789ABCDEF0)) == NULL);
    assert(strcmp(resources.error, "resource hash lookup failed") == 0);
    assert(strcmp(resources.failed_name,
                  "crc64:123456789abcdef0") == 0);

    /* The same out-of-range image offset remains invalid for an NPC sprite,
     * whose fallback pixels must be local to its own blob. */
    assert(Sprite_GetNPCSize(1) == 0);

    /* Component command byte ranges are validated before exposing pointers. */
    assert(Sprite_GetNPCSize(2) == 0);
    const size_t component_size = Sprite_GetNPCSize(3);
    assert(component_size != 0);
    void *npc = malloc(component_size);
    assert(npc != NULL && Sprite_LoadNPC(3, npc, component_size) == npc);
    free(npc);
    assert(Sprite_GetNPCSize(4) == 0); /* truncated embedded CI4 raster */
    assert(Sprite_GetNPCSize(5) == 0); /* truncated embedded RGBA16 palette */
    pb_runtime_resources_clear(&resources);
    assert(resources.count == 0 && resources.head == NULL &&
           resources.index == NULL && resources.index_count == 0 &&
           resources.loaded_buckets == NULL &&
           resources.loaded_bucket_count == 0);
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
