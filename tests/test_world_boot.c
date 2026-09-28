#include "pb3ds/world_boot.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t fake_application_free;
static uint32_t fake_linear_free;
static unsigned int checks_run;

#define CHECK(expression)                                                   \
    do {                                                                    \
        checks_run++;                                                       \
        if (!(expression)) {                                                \
            fprintf(stderr, "M13 world-boot check failed at %s:%d: %s\n", \
                    __FILE__, __LINE__, #expression);                       \
            return false;                                                   \
        }                                                                   \
    } while (0)

uint32_t osGetMemRegionFree(int region) {
    (void)region;
    return fake_application_free;
}

uint32_t linearSpaceFree(void) {
    return fake_linear_free;
}

void *linearAlloc(size_t size) {
    return malloc(size);
}

void linearFree(void *memory) {
    free(memory);
}

uint64_t osGetTime(void) {
    return 1234;
}

static void reset_monitor(PBMemoryMonitor *monitor) {
    fake_application_free = (uint32_t)PB_MIB(32);
    fake_linear_free = (uint32_t)PB_MIB(16);
    pb_memory_monitor_init(monitor, (uintptr_t)PB_MIB(16));
}

static bool make_path(char *path, size_t capacity, const char *directory,
                      const char *name) {
    const int length = snprintf(path, capacity, "%s/%s", directory, name);
    return length > 0 && (size_t)length < capacity;
}

static bool load(const char *path, PBWorldBoot *boot,
                 PBMemoryMonitor *monitor) {
    PBArchive archive = { 0 };
    CHECK(pb_archive_open(&archive, path));
    (void)pb_world_boot_load(boot, &archive, monitor);
    pb_archive_close(&archive);
    return true;
}

static bool load_fixture(const char *directory, const char *name,
                         PBWorldBoot *boot, PBMemoryMonitor *monitor) {
    char path[512];
    CHECK(make_path(path, sizeof(path), directory, name));
    return load(path, boot, monitor);
}

static bool test_valid_fixture(const char *directory, const char *name) {
    PBMemoryMonitor monitor;
    reset_monitor(&monitor);
    PBWorldBoot boot;
    CHECK(load_fixture(directory, name, &boot, &monitor));
    if (boot.result != PB_WORLD_BOOT_READY) {
        fprintf(stderr, "M13 fixture %s returned %s (%s)\n", name,
                pb_world_boot_result_name(boot.result),
                pb_o2r_result_name(boot.archive_result));
    }
    CHECK(boot.result == PB_WORLD_BOOT_READY);
    CHECK(boot.archive_result == PB_O2R_OK);
    CHECK(boot.world_stats.shape_nodes == 2);
    CHECK(boot.world_stats.shape_display_lists == 1);
    CHECK(boot.world_stats.vertex_count == 3);
    CHECK(boot.world_stats.collision_colliders == 1);
    CHECK(boot.world_stats.collision_vertices == 3);
    CHECK(boot.world_stats.collision_triangles == 1);
    CHECK(boot.world_stats.zone_colliders == 1);
    CHECK(boot.world_stats.zone_vertices == 3);
    CHECK(boot.world_stats.zone_triangles == 1);
    CHECK(boot.world_stats.display_list_commands == 2);
    CHECK(boot.background.source_width == 296);
    CHECK(boot.background.source_height == 200);
    CHECK(boot.background.texture_width == 512);
    CHECK(boot.background.texture_height == 256);
    CHECK(boot.background.rgba_size == 512U * 256U * 4U);
    CHECK(monitor.snapshot.class_used[PB_MEMORY_SCENE] ==
          boot.background.rgba_size);
    CHECK(monitor.snapshot.class_used[PB_MEMORY_ARCHIVE] == 0);
    CHECK(monitor.snapshot.class_used[PB_MEMORY_TRANSIENT] == 0);
    pb_world_boot_release(&boot, &monitor);
    CHECK(boot.background.rgba == NULL);
    CHECK(monitor.snapshot.class_used[PB_MEMORY_SCENE] == 0);
    return true;
}

static bool test_failure(const char *directory, const char *name,
                         PBWorldBootResult expected) {
    PBMemoryMonitor monitor;
    reset_monitor(&monitor);
    PBWorldBoot boot;
    CHECK(load_fixture(directory, name, &boot, &monitor));
    CHECK(boot.result == expected);
    CHECK(boot.background.rgba == NULL);
    CHECK(monitor.snapshot.class_used[PB_MEMORY_SCENE] == 0);
    CHECK(monitor.snapshot.class_used[PB_MEMORY_TRANSIENT] == 0);
    return true;
}

static bool test_missing_archive(void) {
    PBMemoryMonitor monitor;
    reset_monitor(&monitor);
    PBArchive missing = { 0 };
    PBWorldBoot boot;
    CHECK(pb_world_boot_load(&boot, &missing, &monitor) ==
          PB_WORLD_BOOT_ARCHIVE_MISSING);
    CHECK(strcmp(pb_world_boot_result_name(boot.result),
                 "pm64.o2r missing") == 0);
    return true;
}

static bool test_flow(void) {
    PBWorldFlow flow;
    pb_world_flow_init(&flow);
    CHECK(flow.state == PB_WORLD_FLOW_FILE_SELECT);
    CHECK(pb_world_flow_request(&flow, 4) == PB_WORLD_FLOW_EVENT_NONE);
    CHECK(pb_world_flow_request(&flow, 2) ==
          PB_WORLD_FLOW_EVENT_LOAD_REQUESTED);
    CHECK(flow.state == PB_WORLD_FLOW_LOADING);
    CHECK(flow.selected_slot == 2);
    CHECK(pb_world_flow_request(&flow, 1) == PB_WORLD_FLOW_EVENT_NONE);
    CHECK(pb_world_flow_finish(&flow, true) ==
          PB_WORLD_FLOW_EVENT_ENTERED_WORLD);
    CHECK(flow.state == PB_WORLD_FLOW_ACTIVE);

    PBInputState input;
    memset(&input, 0, sizeof(input));
    CHECK(pb_world_flow_update(&flow, &input) == PB_WORLD_FLOW_EVENT_NONE);
    input.n64_pressed = PB_N64_START;
    CHECK(pb_world_flow_update(&flow, &input) == PB_WORLD_FLOW_EVENT_PAUSED);
    CHECK(flow.state == PB_WORLD_FLOW_PAUSED);
    CHECK(flow.pause_count == 1);
    CHECK(pb_world_flow_update(&flow, &input) == PB_WORLD_FLOW_EVENT_RESUMED);
    CHECK(flow.state == PB_WORLD_FLOW_ACTIVE);
    CHECK(pb_world_flow_begin_transition(&flow) ==
          PB_WORLD_FLOW_EVENT_LOAD_REQUESTED);
    CHECK(flow.state == PB_WORLD_FLOW_LOADING);
    CHECK(pb_world_flow_finish(&flow, true) ==
          PB_WORLD_FLOW_EVENT_ENTERED_WORLD);
    CHECK(pb_world_flow_begin_transition(NULL) == PB_WORLD_FLOW_EVENT_NONE);

    pb_world_flow_init(&flow);
    CHECK(pb_world_flow_request(&flow, 0) ==
          PB_WORLD_FLOW_EVENT_LOAD_REQUESTED);
    CHECK(pb_world_flow_finish(&flow, false) ==
          PB_WORLD_FLOW_EVENT_LOAD_FAILED);
    CHECK(flow.state == PB_WORLD_FLOW_FAILED);
    CHECK(pb_world_flow_request(&flow, 1) ==
          PB_WORLD_FLOW_EVENT_LOAD_REQUESTED);
    CHECK(flow.selected_slot == 1);
    CHECK(pb_world_flow_finish(&flow, true) ==
          PB_WORLD_FLOW_EVENT_ENTERED_WORLD);
    return true;
}

static bool test_private_archive(const char *path) {
    PBMemoryMonitor monitor;
    reset_monitor(&monitor);
    PBWorldBoot boot;
    CHECK(load(path, &boot, &monitor));
    CHECK(boot.result == PB_WORLD_BOOT_READY);
    CHECK(boot.world_stats.shape_nodes == 223);
    CHECK(boot.world_stats.shape_display_lists == 223);
    CHECK(boot.world_stats.vertex_count == 3211);
    CHECK(boot.world_stats.collision_colliders == 110);
    CHECK(boot.world_stats.collision_vertices == 727);
    CHECK(boot.world_stats.collision_triangles == 873);
    CHECK(boot.world_stats.zone_colliders == 18);
    CHECK(boot.world_stats.zone_vertices == 76);
    CHECK(boot.world_stats.zone_triangles == 69);
    CHECK(boot.world_stats.display_list_commands == 19);
    printf("private mac_00: nodes=%lu dlrefs=%lu vertices=%lu "
           "collision=%lu/%lu/%lu zones=%lu/%lu/%lu sample_cmds=%lu\n",
           (unsigned long)boot.world_stats.shape_nodes,
           (unsigned long)boot.world_stats.shape_display_lists,
           (unsigned long)boot.world_stats.vertex_count,
           (unsigned long)boot.world_stats.collision_colliders,
           (unsigned long)boot.world_stats.collision_vertices,
           (unsigned long)boot.world_stats.collision_triangles,
           (unsigned long)boot.world_stats.zone_colliders,
           (unsigned long)boot.world_stats.zone_vertices,
           (unsigned long)boot.world_stats.zone_triangles,
           (unsigned long)boot.world_stats.display_list_commands);
    pb_world_boot_release(&boot, &monitor);
    CHECK(monitor.snapshot.class_used[PB_MEMORY_SCENE] == 0);
    return true;
}

int main(int argc, char **argv) {
    if (argc < 2 ||
        !test_valid_fixture(argv[1], "valid-deflate.o2r") ||
        !test_valid_fixture(argv[1], "valid-stored.o2r") ||
        !test_failure(argv[1], "missing-shape.o2r",
                      PB_WORLD_BOOT_RESOURCE_MISSING) ||
        !test_failure(argv[1], "invalid-shape.o2r",
                      PB_WORLD_BOOT_SHAPE_INVALID) ||
        !test_failure(argv[1], "invalid-collision.o2r",
                      PB_WORLD_BOOT_COLLISION_INVALID) ||
        !test_failure(argv[1], "invalid-vertices.o2r",
                      PB_WORLD_BOOT_VERTEX_INVALID) ||
        !test_failure(argv[1], "invalid-display-list.o2r",
                      PB_WORLD_BOOT_DISPLAY_LIST_INVALID) ||
        !test_failure(argv[1], "invalid-background.o2r",
                      PB_WORLD_BOOT_BACKGROUND_INVALID) ||
        !test_failure(argv[1], "invalid-palette.o2r",
                      PB_WORLD_BOOT_BACKGROUND_INVALID) ||
        !test_missing_archive() || !test_flow()) {
        return EXIT_FAILURE;
    }
    if (argc >= 3 && !test_private_archive(argv[2])) {
        return EXIT_FAILURE;
    }
    printf("M13 world boot: %u checks passed%s\n", checks_run,
           argc >= 3 ? " (private archive included)" : "");
    return EXIT_SUCCESS;
}
