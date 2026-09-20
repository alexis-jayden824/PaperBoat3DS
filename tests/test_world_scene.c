#include "pb3ds/world_scene.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static u32 fake_application_free;
static u32 fake_linear_free;
static unsigned int checks_run;

#define CHECK(expression)                                                    \
    do {                                                                     \
        checks_run++;                                                        \
        if (!(expression)) {                                                 \
            fprintf(stderr, "M13 world-scene check failed at %s:%d: %s\n", \
                    __FILE__, __LINE__, #expression);                        \
            return false;                                                    \
        }                                                                    \
    } while (0)

u32 osGetMemRegionFree(int region) {
    (void)region;
    return fake_application_free;
}

u32 linearSpaceFree(void) {
    return fake_linear_free;
}

void *linearAlloc(size_t size) {
    return malloc(size);
}

void linearFree(void *memory) {
    free(memory);
}

u64 osGetTime(void) {
    return 1234U;
}

static void reset_monitor(PBMemoryMonitor *monitor) {
    fake_application_free = (u32)PB_MIB(32);
    fake_linear_free = (u32)PB_MIB(16);
    pb_memory_monitor_init(monitor, (uintptr_t)PB_MIB(16));
}

static bool make_path(char *path, size_t capacity, const char *directory,
                      const char *name) {
    const int length = snprintf(path, capacity, "%s/%s", directory, name);
    return length > 0 && (size_t)length < capacity;
}

static bool load_ready(PBWorldScene *scene, PBArchive *archive,
                       const char *map_id, uint8_t entry_id,
                       PBMemoryMonitor *monitor) {
    const PBWorldSceneResult result =
        pb_world_scene_load(scene, archive, map_id, entry_id, monitor);
    if (result != PB_WORLD_SCENE_READY) {
        fprintf(stderr, "%s entry %u returned %s (%s), scene=%lu KiB\n",
                map_id, entry_id, pb_world_scene_result_name(result),
                pb_o2r_result_name(scene->archive_result),
                (unsigned long)(monitor->snapshot
                                    .class_used[PB_MEMORY_SCENE] /
                                1024U));
    }
    CHECK(result == PB_WORLD_SCENE_READY);
    CHECK(monitor->snapshot.class_used[PB_MEMORY_TRANSIENT] == 0U);
    return true;
}

static bool test_decoder_formats(void) {
    PBMemoryMonitor monitor;
    reset_monitor(&monitor);
    uint8_t palette_bytes[32];
    memset(palette_bytes, 0, sizeof(palette_bytes));
    palette_bytes[2] = 0xF8U;
    palette_bytes[3] = 0x01U;
    PBTextureResourceView palette = {
        .type = PB_RESOURCE_TEXTURE_RGBA16,
        .width = 16U,
        .height = 1U,
        .image_size = sizeof(palette_bytes),
        .image = palette_bytes,
    };
    const uint8_t ci4_bytes[] = { 0x01U };
    const uint8_t i4_bytes[] = { 0x0FU };
    const uint8_t i8_bytes[] = { 17U, 238U };
    const uint8_t ia4_bytes[] = { 0x1FU };
    const uint8_t ia16_bytes[] = { 33U, 44U };
    const struct {
        uint32_t type;
        uint32_t width;
        const uint8_t *bytes;
        size_t size;
        const PBTextureResourceView *palette;
    } cases[] = {
        { PB_RESOURCE_TEXTURE_CI4, 2U, ci4_bytes, sizeof(ci4_bytes),
          &palette },
        { PB_RESOURCE_TEXTURE_I4, 2U, i4_bytes, sizeof(i4_bytes), NULL },
        { PB_RESOURCE_TEXTURE_I8, 2U, i8_bytes, sizeof(i8_bytes), NULL },
        { PB_RESOURCE_TEXTURE_IA4, 2U, ia4_bytes, sizeof(ia4_bytes), NULL },
        { PB_RESOURCE_TEXTURE_IA16, 1U, ia16_bytes, sizeof(ia16_bytes),
          NULL },
    };
    for (size_t index = 0U; index < sizeof(cases) / sizeof(cases[0]);
         index++) {
        PBTextureResourceView resource = {
            .type = cases[index].type,
            .width = cases[index].width,
            .height = 1U,
            .image_size = (uint32_t)cases[index].size,
            .image = cases[index].bytes,
        };
        PBDecodedTexture decoded;
        CHECK(pb_texture_decode_rgba8(&decoded, &resource,
                                      cases[index].palette, &monitor) ==
              PB_TEXTURE_DECODE_OK);
        CHECK(decoded.texture_width == 8U);
        CHECK(decoded.texture_height == 8U);
        pb_decoded_texture_release(&decoded, &monitor);
    }
    CHECK(monitor.snapshot.class_used[PB_MEMORY_SCENE] == 0U);
    return true;
}

static bool test_prefix_scan(PBArchive *archive) {
    PBO2REntry entries[4];
    size_t count = 0U;
    PBO2RStats stats = { 0 };
    CHECK(pb_o2r_find_entries_with_prefix(archive, "shapes/mac_00_shape/",
                                          entries, 4U, &count,
                                          &stats) == PB_O2R_OK);
    CHECK(count == 2U);
    CHECK(strcmp(entries[0].name, "shapes/mac_00_shape/vtx") == 0 ||
          strcmp(entries[1].name, "shapes/mac_00_shape/vtx") == 0);
    count = 0U;
    CHECK(pb_o2r_find_entries_with_prefix(archive, "sprites/", entries, 1U,
                                          &count, &stats) ==
          PB_O2R_CAPACITY_EXCEEDED);
    CHECK(strcmp(pb_o2r_result_name(PB_O2R_CAPACITY_EXCEEDED),
                 "entry capacity") == 0);
    return true;
}

static bool test_public_mac_00(PBArchive *archive) {
    PBMemoryMonitor monitor;
    reset_monitor(&monitor);
    PBWorldScene scene;
    pb_world_scene_init(&scene);
    CHECK(load_ready(&scene, archive, "mac_00", 6U, &monitor));
    CHECK(scene.stats.shape_nodes == 1U);
    CHECK(scene.stats.leaf_models == 1U);
    CHECK(scene.stats.display_lists == 1U);
    CHECK(scene.stats.display_list_commands == 4U);
    CHECK(scene.stats.source_vertices == 3U);
    CHECK(scene.stats.triangles == 1U);
    CHECK(scene.stats.textured_triangles == 1U);
    CHECK(scene.stats.textures == 1U);
    CHECK(scene.stats.unsupported_commands == 0U);
    CHECK(scene.stats.colliders == 3U);
    CHECK(scene.stats.collision_vertices == 8U);
    CHECK(scene.stats.collision_triangles == 4U);
    CHECK(scene.sign_collider == 2);
    CHECK(scene.exit_collider == 1);
    CHECK(scene.current_floor == 0);
    CHECK(scene.background.rgba != NULL);
    CHECK(scene.player_frames[0].source_width == 32U);
    CHECK(scene.player_frames[0].source_height == 56U);
    CHECK(scene.star_piece.source_width == 32U);

    PBInputState input;
    memset(&input, 0, sizeof(input));
    input.n64_pressed = PB_N64_A;
    CHECK(pb_world_scene_update(&scene, &input) ==
          PB_WORLD_SCENE_EVENT_SIGN_OPENED);
    CHECK(pb_world_scene_message_visible(&scene));
    CHECK(pb_world_scene_update(&scene, &input) ==
          PB_WORLD_SCENE_EVENT_SIGN_CLOSED);
    CHECK(!pb_world_scene_message_visible(&scene));

    memset(&input, 0, sizeof(input));
    input.stick_x = 80;
    const float previous_x = scene.player_position.x;
    CHECK(pb_world_scene_update(&scene, &input) ==
          PB_WORLD_SCENE_EVENT_NONE);
    CHECK(scene.player_position.x > previous_x);
    CHECK(scene.movement_frames > 0U);

    memset(&input, 0, sizeof(input));
    input.native_held = KEY_DRIGHT;
    const float previous_dpad_x = scene.player_position.x;
    const uint32_t previous_movement_frames = scene.movement_frames;
    CHECK(pb_world_scene_update(&scene, &input) ==
          PB_WORLD_SCENE_EVENT_NONE);
    CHECK(scene.player_position.x > previous_dpad_x);
    CHECK(scene.movement_frames == previous_movement_frames + 1U);

    scene.player_position.x = -420.0f;
    scene.player_position.y = 20.0f;
    scene.player_position.z = 410.0f;
    memset(&input, 0, sizeof(input));
    CHECK(pb_world_scene_update(&scene, &input) ==
          PB_WORLD_SCENE_EVENT_STAR_PIECE_COLLECTED);
    CHECK(scene.star_piece_collected);
    CHECK(!scene.star_piece_active);

    scene.transition_state = PB_WORLD_TRANSITION_NONE;
    scene.transition_cooldown = 0U;
    scene.current_floor = scene.exit_collider;
    CHECK(pb_world_scene_update(&scene, &input) ==
          PB_WORLD_SCENE_EVENT_TRANSITION_STARTED);
    for (unsigned int frame = 0U; frame < 29U; frame++) {
        CHECK(pb_world_scene_update(&scene, &input) ==
              PB_WORLD_SCENE_EVENT_NONE);
    }
    CHECK(pb_world_scene_update(&scene, &input) ==
          PB_WORLD_SCENE_EVENT_TRANSITION_REQUESTED);
    CHECK(strcmp(scene.requested_map, "mac_01") == 0);
    CHECK(scene.requested_entry == 0U);
    CHECK(fabsf(pb_world_scene_fade_alpha(&scene) - 1.0f) < 0.001f);

    const size_t geometry_bytes = scene.triangles_allocation +
                                  scene.collision_vertices_allocation +
                                  scene.collision_triangles_allocation +
                                  scene.colliders_allocation;
    pb_world_scene_release_pixels(&scene, &monitor);
    CHECK(scene.pixels_released);
    CHECK(monitor.snapshot.class_used[PB_MEMORY_SCENE] == geometry_bytes);
    pb_world_scene_release(&scene, &monitor);
    CHECK(monitor.snapshot.class_used[PB_MEMORY_SCENE] == 0U);
    return true;
}

static bool test_public_mac_01(PBArchive *archive) {
    PBMemoryMonitor monitor;
    reset_monitor(&monitor);
    PBWorldScene scene;
    pb_world_scene_init(&scene);
    CHECK(load_ready(&scene, archive, "mac_01", 0U, &monitor));
    CHECK(scene.current_floor == scene.exit_collider);
    CHECK(scene.entry_walk_frames == 45U);
    CHECK(strcmp(scene.exit_map, "mac_00") == 0);
    CHECK(scene.exit_entry == 1U);
    PBInputState input;
    memset(&input, 0, sizeof(input));
    PBWorldSceneEvent event = PB_WORLD_SCENE_EVENT_NONE;
    for (unsigned int frame = 0U; frame < 100U; frame++) {
        event = pb_world_scene_update(&scene, &input);
        CHECK(event != PB_WORLD_SCENE_EVENT_TRANSITION_STARTED);
        CHECK(event != PB_WORLD_SCENE_EVENT_TRANSITION_REQUESTED);
    }
    CHECK(scene.entry_walk_frames == 0U);
    CHECK(scene.player_position.x > -550.0f);
    CHECK(scene.current_floor != scene.exit_collider);
    pb_world_scene_release(&scene, &monitor);
    CHECK(monitor.snapshot.class_used[PB_MEMORY_SCENE] == 0U);
    return true;
}

static bool test_public_archive(const char *path) {
    PBArchive archive = { 0 };
    CHECK(pb_archive_open(&archive, path));
    const bool ok = test_prefix_scan(&archive) &&
                    test_public_mac_00(&archive) &&
                    test_public_mac_01(&archive);
    pb_archive_close(&archive);
    return ok;
}

static bool test_private_map(PBArchive *archive, const char *map_id,
                             uint8_t entry_id, uint32_t nodes,
                             uint32_t leaves, uint32_t display_lists,
                             uint32_t vertices, uint32_t triangles,
                             uint32_t textures, uint32_t colliders,
                             uint32_t collision_vertices,
                             uint32_t collision_triangles) {
    PBMemoryMonitor monitor;
    reset_monitor(&monitor);
    PBWorldScene scene;
    pb_world_scene_init(&scene);
    CHECK(load_ready(&scene, archive, map_id, entry_id, &monitor));
    CHECK(scene.stats.shape_nodes == nodes);
    CHECK(scene.stats.leaf_models == leaves);
    CHECK(scene.stats.display_lists == display_lists);
    CHECK(scene.stats.source_vertices == vertices);
    CHECK(scene.stats.triangles == triangles);
    CHECK(scene.stats.textures == textures);
    CHECK(scene.stats.colliders == colliders);
    CHECK(scene.stats.collision_vertices == collision_vertices);
    CHECK(scene.stats.collision_triangles == collision_triangles);
    CHECK(scene.stats.unsupported_commands == 0U);
    CHECK(scene.exit_collider >= 0);
    CHECK(scene.current_floor >= 0);
    CHECK(scene.background.rgba != NULL);
    CHECK(scene.player_frames[0].rgba != NULL);
    CHECK(scene.player_frames[1].rgba != NULL);
    printf("private %s:%u nodes=%lu leaves=%lu dl=%lu cmds=%lu "
           "vtx=%lu tris=%lu lit=%lu textured=%lu textures=%lu "
           "hit=%lu/%lu/%lu floor=%d exit=%d scene=%lu peak=%lu KiB\n",
           map_id, entry_id, (unsigned long)scene.stats.shape_nodes,
           (unsigned long)scene.stats.leaf_models,
           (unsigned long)scene.stats.display_lists,
           (unsigned long)scene.stats.display_list_commands,
           (unsigned long)scene.stats.source_vertices,
           (unsigned long)scene.stats.triangles,
           (unsigned long)scene.stats.lit_triangles,
           (unsigned long)scene.stats.textured_triangles,
           (unsigned long)scene.stats.textures,
           (unsigned long)scene.stats.colliders,
           (unsigned long)scene.stats.collision_vertices,
           (unsigned long)scene.stats.collision_triangles,
           scene.current_floor, scene.exit_collider,
           (unsigned long)(monitor.snapshot.class_used[PB_MEMORY_SCENE] /
                           1024U),
           (unsigned long)(monitor.snapshot.class_peak[PB_MEMORY_SCENE] /
                           1024U));
    pb_world_scene_release(&scene, &monitor);
    CHECK(monitor.snapshot.class_used[PB_MEMORY_SCENE] == 0U);
    return true;
}

static bool test_private_return_entry(PBArchive *archive) {
    PBMemoryMonitor monitor;
    reset_monitor(&monitor);
    PBWorldScene scene;
    pb_world_scene_init(&scene);
    CHECK(load_ready(&scene, archive, "mac_00", 1U, &monitor));
    CHECK(scene.entry_walk_frames == 45U);
    PBInputState input;
    memset(&input, 0, sizeof(input));
    for (unsigned int frame = 0U; frame < 100U; frame++) {
        const PBWorldSceneEvent event = pb_world_scene_update(&scene, &input);
        CHECK(event != PB_WORLD_SCENE_EVENT_TRANSITION_STARTED);
        CHECK(event != PB_WORLD_SCENE_EVENT_TRANSITION_REQUESTED);
    }
    CHECK(scene.entry_walk_frames == 0U);
    CHECK(scene.player_position.x < 550.0f);
    CHECK(scene.current_floor != scene.exit_collider);
    pb_world_scene_release(&scene, &monitor);
    CHECK(monitor.snapshot.class_used[PB_MEMORY_SCENE] == 0U);
    return true;
}

static bool test_private_archive(const char *path) {
    PBArchive archive = { 0 };
    CHECK(pb_archive_open(&archive, path));
    const bool ok = test_private_map(
                        &archive, "mac_00", 6U, 223U, 174U, 223U, 3211U,
                        2040U, 41U, 110U, 727U, 873U) &&
                    test_private_map(
                        &archive, "mac_01", 0U, 206U, 148U, 206U, 3624U,
                        2210U, 48U, 98U, 567U, 684U) &&
                    test_private_return_entry(&archive);
    pb_archive_close(&archive);
    return ok;
}

static bool test_failures(void) {
    PBMemoryMonitor monitor;
    reset_monitor(&monitor);
    PBArchive missing = { 0 };
    PBWorldScene scene;
    pb_world_scene_init(&scene);
    CHECK(pb_world_scene_load(&scene, &missing, "mac_00", 6U, &monitor) ==
          PB_WORLD_SCENE_INVALID_ARGUMENT);
    CHECK(pb_world_scene_load(&scene, &missing, "nok_01", 0U, &monitor) ==
          PB_WORLD_SCENE_INVALID_ARGUMENT);
    CHECK(strcmp(pb_world_scene_result_name(PB_WORLD_SCENE_CAPACITY),
                 "scene capacity") == 0);
    CHECK(strcmp(pb_world_scene_event_name(
                     PB_WORLD_SCENE_EVENT_STAR_PIECE_COLLECTED),
                 "star piece collected") == 0);
    return true;
}

int main(int argc, char **argv) {
    if (argc < 2 || !test_decoder_formats() || !test_failures()) {
        return EXIT_FAILURE;
    }
    char path[512];
    if (!make_path(path, sizeof(path), argv[1], "scene-deflate.o2r") ||
        !test_public_archive(path) ||
        !make_path(path, sizeof(path), argv[1], "scene-stored.o2r") ||
        !test_public_archive(path)) {
        return EXIT_FAILURE;
    }
    if (argc >= 3 && !test_private_archive(argv[2])) {
        return EXIT_FAILURE;
    }
    printf("M13 world scene: %u checks passed%s\n", checks_run,
           argc >= 3 ? " (private archive included)" : "");
    return EXIT_SUCCESS;
}
