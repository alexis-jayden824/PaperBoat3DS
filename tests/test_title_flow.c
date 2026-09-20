#include "pb3ds/title_flow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static u32 fake_application_free;
static u32 fake_linear_free;
static unsigned int checks_run;

#define CHECK(expression)                                                     \
    do {                                                                      \
        checks_run++;                                                         \
        if (!(expression)) {                                                  \
            fprintf(stderr, "M12 title-flow check failed at %s:%d: %s\n",   \
                    __FILE__, __LINE__, #expression);                         \
            return false;                                                     \
        }                                                                     \
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
    return 1234;
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

static bool load_fixture(const char *directory, const char *name,
                         PBTitleAssets *assets, PBMemoryMonitor *monitor) {
    char path[512];
    CHECK(make_path(path, sizeof(path), directory, name));
    PBArchive archive = { 0 };
    CHECK(pb_archive_open(&archive, path));
    (void)pb_title_assets_load(assets, &archive, monitor);
    pb_archive_close(&archive);
    return true;
}

static bool check_pixel(const PBDecodedTexture *texture, uint16_t x,
                        uint16_t y, uint8_t red, uint8_t green, uint8_t blue,
                        uint8_t alpha) {
    const size_t offset = ((size_t)y * texture->texture_width + x) * 4U;
    CHECK(texture->rgba[offset + 0U] == red);
    CHECK(texture->rgba[offset + 1U] == green);
    CHECK(texture->rgba[offset + 2U] == blue);
    CHECK(texture->rgba[offset + 3U] == alpha);
    return true;
}

static bool test_valid_fixture(const char *directory, const char *name) {
    PBMemoryMonitor monitor;
    reset_monitor(&monitor);
    PBTitleAssets assets;
    CHECK(load_fixture(directory, name, &assets, &monitor));
    CHECK(assets.result == PB_TITLE_ASSETS_READY);
    CHECK(assets.archive_result == PB_O2R_OK);
    CHECK(assets.logo.source_width == PB_TITLE_LOGO_WIDTH);
    CHECK(assets.logo.source_height == PB_TITLE_LOGO_HEIGHT);
    CHECK(assets.logo.texture_width == 256);
    CHECK(assets.logo.texture_height == 128);
    CHECK(assets.prompt.texture_width == 128);
    CHECK(assets.prompt.texture_height == 32);
    CHECK(assets.copyright.texture_width == 256);
    CHECK(assets.copyright.texture_height == 32);
    CHECK(check_pixel(&assets.logo, 0, 0, 255, 0, 0, 255));
    CHECK(check_pixel(&assets.logo, 0, 111, 0, 0, 255, 255));
    CHECK(check_pixel(&assets.logo, 200, 0, 0, 0, 0, 0));
    CHECK(check_pixel(&assets.prompt, 0, 0, 255, 255, 255, 17));
    CHECK(check_pixel(&assets.prompt, 0, 31, 17, 17, 17, 255));
    CHECK(monitor.snapshot.class_used[PB_MEMORY_SCENE] ==
          256U * 128U * 4U + 128U * 32U * 4U + 256U * 32U * 4U);
    CHECK(monitor.snapshot.class_used[PB_MEMORY_ARCHIVE] == 0);
    CHECK(monitor.snapshot.class_used[PB_MEMORY_TRANSIENT] == 0);
    pb_title_assets_release_pixels(&assets, &monitor);
    CHECK(assets.logo.rgba == NULL);
    CHECK(assets.prompt.rgba == NULL);
    CHECK(assets.copyright.rgba == NULL);
    CHECK(monitor.snapshot.class_used[PB_MEMORY_SCENE] == 0);
    CHECK(monitor.snapshot.allocation_failures == 0);
    return true;
}

static bool test_failure(const char *directory, const char *name,
                         PBTitleAssetsResult expected,
                         PBO2RResult expected_archive) {
    PBMemoryMonitor monitor;
    reset_monitor(&monitor);
    PBTitleAssets assets;
    CHECK(load_fixture(directory, name, &assets, &monitor));
    CHECK(assets.result == expected);
    CHECK(assets.archive_result == expected_archive);
    CHECK(assets.logo.rgba == NULL);
    CHECK(assets.prompt.rgba == NULL);
    CHECK(assets.copyright.rgba == NULL);
    CHECK(monitor.snapshot.class_used[PB_MEMORY_ARCHIVE] == 0);
    CHECK(monitor.snapshot.class_used[PB_MEMORY_TRANSIENT] == 0);
    CHECK(monitor.snapshot.class_used[PB_MEMORY_SCENE] == 0);
    return true;
}

static bool test_missing_archive(void) {
    PBMemoryMonitor monitor;
    reset_monitor(&monitor);
    PBArchive missing = { 0 };
    PBTitleAssets assets;
    CHECK(pb_title_assets_load(&assets, &missing, &monitor) ==
          PB_TITLE_ASSETS_ARCHIVE_MISSING);
    CHECK(strcmp(pb_title_assets_result_name(assets.result),
                 "pm64.o2r missing") == 0);
    return true;
}

static bool update(PBTitleFlow *flow, uint16_t pressed, int8_t stick_x,
                   int8_t stick_y, PBTitleFlowEvent expected) {
    PBInputState input;
    memset(&input, 0, sizeof(input));
    input.n64_pressed = pressed;
    input.stick_x = stick_x;
    input.stick_y = stick_y;
    CHECK(pb_title_flow_update(flow, &input) == expected);
    return true;
}

static bool test_state_machine(void) {
    PBTitleFlow flow;
    pb_title_flow_init(&flow);
    CHECK(flow.screen == PB_TITLE_FLOW_TITLE);
    CHECK(flow.selected_slot == 0);
    CHECK(update(&flow, 0, 0, 0, PB_TITLE_FLOW_EVENT_NONE));
    CHECK(flow.prompt_alpha == 128);
    CHECK(update(&flow, PB_N64_A, 0, 0,
                 PB_TITLE_FLOW_EVENT_ENTER_FILE_SELECT));
    CHECK(flow.screen == PB_TITLE_FLOW_FILE_SELECT);
    CHECK(flow.transition_count == 1);
    CHECK(update(&flow, PB_N64_D_RIGHT, 0, 0,
                 PB_TITLE_FLOW_EVENT_MOVE_SLOT));
    CHECK(flow.selected_slot == 1);
    CHECK(update(&flow, PB_N64_D_DOWN, 0, 0,
                 PB_TITLE_FLOW_EVENT_MOVE_SLOT));
    CHECK(flow.selected_slot == 3);
    CHECK(update(&flow, PB_N64_D_RIGHT | PB_N64_D_DOWN, 0, 0,
                 PB_TITLE_FLOW_EVENT_NONE));
    CHECK(flow.selected_slot == 3);
    CHECK(update(&flow, PB_N64_START, 0, 0,
                 PB_TITLE_FLOW_EVENT_CONFIRM_SLOT));
    CHECK(flow.slot_confirmed);
    CHECK(flow.confirmation_count == 1);
    CHECK(update(&flow, PB_N64_D_LEFT, 0, 0,
                 PB_TITLE_FLOW_EVENT_MOVE_SLOT));
    CHECK(flow.selected_slot == 2);
    CHECK(!flow.slot_confirmed);
    CHECK(update(&flow, PB_N64_B, 0, 0,
                 PB_TITLE_FLOW_EVENT_RETURN_TITLE));
    CHECK(flow.screen == PB_TITLE_FLOW_TITLE);
    CHECK(flow.transition_count == 2);
    CHECK(update(&flow, PB_N64_START, 0, 0,
                 PB_TITLE_FLOW_EVENT_ENTER_FILE_SELECT));
    flow.selected_slot = 0;
    CHECK(update(&flow, 0, 50, 0, PB_TITLE_FLOW_EVENT_MOVE_SLOT));
    CHECK(flow.selected_slot == 1);
    CHECK(update(&flow, 0, 50, 0, PB_TITLE_FLOW_EVENT_NONE));
    CHECK(flow.selected_slot == 1);
    CHECK(update(&flow, 0, 0, 0, PB_TITLE_FLOW_EVENT_NONE));
    CHECK(update(&flow, 0, -50, 0, PB_TITLE_FLOW_EVENT_MOVE_SLOT));
    CHECK(flow.selected_slot == 0);
    CHECK(pb_title_flow_update(NULL, NULL) == PB_TITLE_FLOW_EVENT_NONE);
    CHECK(strcmp(pb_title_flow_screen_name(PB_TITLE_FLOW_FILE_SELECT),
                 "file select") == 0);
    CHECK(strcmp(pb_title_flow_event_name(PB_TITLE_FLOW_EVENT_CONFIRM_SLOT),
                 "slot selected") == 0);
    return true;
}

static bool test_private_archive(const char *path) {
    PBMemoryMonitor monitor;
    reset_monitor(&monitor);
    PBArchive archive = { 0 };
    CHECK(pb_archive_open(&archive, path));
    PBTitleAssets assets;
    CHECK(pb_title_assets_load(&assets, &archive, &monitor) ==
          PB_TITLE_ASSETS_READY);
    CHECK(assets.archive_stats.entries_scanned > 0);
    CHECK(assets.logo.rgba != NULL);
    CHECK(assets.prompt.rgba != NULL);
    CHECK(assets.copyright.rgba != NULL);
    pb_title_assets_release_pixels(&assets, &monitor);
    pb_archive_close(&archive);
    CHECK(monitor.snapshot.class_used[PB_MEMORY_SCENE] == 0);
    return true;
}

int main(int argc, char **argv) {
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "usage: %s FIXTURE_DIRECTORY [PRIVATE_PM64_O2R]\n",
                argv[0]);
        return EXIT_FAILURE;
    }
    if (!test_valid_fixture(argv[1], "valid-deflate.o2r") ||
        !test_valid_fixture(argv[1], "valid-stored.o2r") ||
        !test_failure(argv[1], "missing-prompt.o2r",
                      PB_TITLE_ASSETS_RESOURCE_MISSING,
                      PB_O2R_ENTRY_NOT_FOUND) ||
        !test_failure(argv[1], "invalid-logo.o2r",
                      PB_TITLE_ASSETS_TEXTURE_INVALID, PB_O2R_OK) ||
        !test_failure(argv[1], "invalid-prompt.o2r",
                      PB_TITLE_ASSETS_TEXTURE_INVALID, PB_O2R_OK) ||
        !test_failure(argv[1], "bad-crc.o2r",
                      PB_TITLE_ASSETS_ARCHIVE_ERROR,
                      PB_O2R_CHECKSUM_MISMATCH) ||
        !test_missing_archive() || !test_state_machine() ||
        (argc == 3 && !test_private_archive(argv[2]))) {
        return EXIT_FAILURE;
    }
    printf("M12 title and file-select flow: %u checks passed\n", checks_run);
    return EXIT_SUCCESS;
}
