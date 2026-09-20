#include "pb3ds/first_frame.h"

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
            fprintf(stderr, "M11 frame check failed at %s:%d: %s\n",      \
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

uint64_t osGetTime(void) {
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
                         PBFirstFrame *frame, PBMemoryMonitor *monitor) {
    char path[512];
    CHECK(make_path(path, sizeof(path), directory, name));
    PBArchive archive = { 0 };
    CHECK(pb_archive_open(&archive, path));
    (void)pb_first_frame_load(frame, &archive, monitor);
    pb_archive_close(&archive);
    return true;
}

static bool check_pixel(const PBFirstFrame *frame, uint16_t x, uint16_t y,
                        uint8_t red, uint8_t green, uint8_t blue,
                        uint8_t alpha) {
    const size_t offset =
        ((size_t)y * frame->texture_width + x) * 4U;
    CHECK(frame->rgba[offset + 0U] == red);
    CHECK(frame->rgba[offset + 1U] == green);
    CHECK(frame->rgba[offset + 2U] == blue);
    CHECK(frame->rgba[offset + 3U] == alpha);
    return true;
}

static bool test_valid_fixture(const char *directory, const char *name) {
    PBMemoryMonitor monitor;
    reset_monitor(&monitor);
    PBFirstFrame frame;
    CHECK(load_fixture(directory, name, &frame, &monitor));
    CHECK(frame.result == PB_FIRST_FRAME_READY);
    CHECK(frame.archive_result == PB_O2R_OK);
    CHECK(frame.source_width == 296);
    CHECK(frame.source_height == 200);
    CHECK(frame.texture_width == 512);
    CHECK(frame.texture_height == 256);
    CHECK(frame.rgba_size == 512U * 256U * 4U);
    CHECK(frame.rgba != NULL);
    CHECK(frame.archive_stats.entries_scanned == 3);
    CHECK(frame.archive_stats.compressed_bytes > 0);
    CHECK(frame.archive_stats.uncompressed_bytes == 59280U + 592U);
    CHECK(check_pixel(&frame, 0, 0, 255, 0, 0, 255));
    CHECK(check_pixel(&frame, 1, 0, 0, 255, 0, 255));
    CHECK(check_pixel(&frame, 2, 0, 0, 0, 255, 255));
    CHECK(check_pixel(&frame, 3, 0, 255, 255, 255, 255));
    CHECK(check_pixel(&frame, 296, 0, 0, 0, 0, 0));
    CHECK(check_pixel(&frame, 0, 200, 0, 0, 0, 0));
    CHECK(monitor.snapshot.class_used[PB_MEMORY_ARCHIVE] == 0);
    CHECK(monitor.snapshot.class_used[PB_MEMORY_TRANSIENT] == 0);
    CHECK(monitor.snapshot.class_used[PB_MEMORY_SCENE] == frame.rgba_size);
    pb_first_frame_release_pixels(&frame, &monitor);
    CHECK(frame.rgba == NULL);
    CHECK(monitor.snapshot.class_used[PB_MEMORY_SCENE] == 0);
    CHECK(monitor.snapshot.allocation_failures == 0);
    return true;
}

static bool test_failure(const char *directory, const char *name,
                         PBFirstFrameResult expected_frame,
                         PBO2RResult expected_archive) {
    PBMemoryMonitor monitor;
    reset_monitor(&monitor);
    PBFirstFrame frame;
    CHECK(load_fixture(directory, name, &frame, &monitor));
    CHECK(frame.result == expected_frame);
    CHECK(frame.archive_result == expected_archive);
    CHECK(frame.rgba == NULL);
    CHECK(monitor.snapshot.class_used[PB_MEMORY_ARCHIVE] == 0);
    CHECK(monitor.snapshot.class_used[PB_MEMORY_TRANSIENT] == 0);
    CHECK(monitor.snapshot.class_used[PB_MEMORY_SCENE] == 0);
    return true;
}

static bool test_missing_archive(void) {
    PBMemoryMonitor monitor;
    reset_monitor(&monitor);
    PBArchive missing = { 0 };
    PBFirstFrame frame;
    CHECK(pb_first_frame_load(&frame, &missing, &monitor) ==
          PB_FIRST_FRAME_ARCHIVE_MISSING);
    CHECK(strcmp(pb_first_frame_result_name(frame.result),
                 "pm64.o2r missing") == 0);
    CHECK(strcmp(pb_o2r_result_name(PB_O2R_CHECKSUM_MISMATCH),
                 "checksum mismatch") == 0);
    return true;
}

static bool test_private_archive(const char *path) {
    PBMemoryMonitor monitor;
    reset_monitor(&monitor);
    PBArchive archive = { 0 };
    CHECK(pb_archive_open(&archive, path));
    PBFirstFrame frame;
    CHECK(pb_first_frame_load(&frame, &archive, &monitor) ==
          PB_FIRST_FRAME_READY);
    CHECK(frame.archive_result == PB_O2R_OK);
    CHECK(frame.source_width == 296);
    CHECK(frame.source_height == 200);
    CHECK(frame.rgba != NULL);
    CHECK(frame.archive_stats.entries_scanned > 0);
    CHECK(frame.archive_stats.entries_scanned <
          frame.archive_stats.directory_entries);
    pb_first_frame_release_pixels(&frame, &monitor);
    pb_archive_close(&archive);
    CHECK(monitor.snapshot.class_used[PB_MEMORY_ARCHIVE] == 0);
    CHECK(monitor.snapshot.class_used[PB_MEMORY_TRANSIENT] == 0);
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
        !test_failure(argv[1], "missing-palette.o2r",
                      PB_FIRST_FRAME_RESOURCE_MISSING,
                      PB_O2R_ENTRY_NOT_FOUND) ||
        !test_failure(argv[1], "invalid-texture.o2r",
                      PB_FIRST_FRAME_TEXTURE_INVALID, PB_O2R_OK) ||
        !test_failure(argv[1], "bad-crc.o2r",
                      PB_FIRST_FRAME_ARCHIVE_ERROR,
                      PB_O2R_CHECKSUM_MISMATCH) ||
        !test_failure(argv[1], "unsupported-method.o2r",
                      PB_FIRST_FRAME_ARCHIVE_ERROR,
                      PB_O2R_UNSUPPORTED_METHOD) ||
        !test_failure(argv[1], "encrypted.o2r",
                      PB_FIRST_FRAME_ARCHIVE_ERROR,
                      PB_O2R_ENTRY_ENCRYPTED) ||
        !test_failure(argv[1], "oversized.o2r",
                      PB_FIRST_FRAME_ARCHIVE_ERROR,
                      PB_O2R_ENTRY_TOO_LARGE) ||
        !test_failure(argv[1], "bad-local-name.o2r",
                      PB_FIRST_FRAME_ARCHIVE_ERROR,
                      PB_O2R_INVALID_ZIP) ||
        !test_missing_archive() ||
        (argc == 3 && !test_private_archive(argv[2]))) {
        return EXIT_FAILURE;
    }
    printf("M11 archive-backed frame: %u checks passed\n", checks_run);
    return EXIT_SUCCESS;
}
