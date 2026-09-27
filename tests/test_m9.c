#include "pb3ds/platform.h"
#include "pb3ds/version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static unsigned int checks_run;

#define CHECK(expression)                                                      \
    do {                                                                       \
        checks_run++;                                                          \
        if (!(expression)) {                                                   \
            fprintf(stderr, "M9 fs check failed at %s:%d: %s\n", __FILE__,     \
                    __LINE__, #expression);                                    \
            return false;                                                      \
        }                                                                      \
    } while (0)

static void wr16(FILE *file, uint16_t value) {
    uint8_t bytes[2];

    bytes[0] = (uint8_t)(value & 0xFFU);
    bytes[1] = (uint8_t)((value >> 8) & 0xFFU);
    fwrite(bytes, 1, 2, file);
}

static void wr32(FILE *file, uint32_t value) {
    uint8_t bytes[4];

    bytes[0] = (uint8_t)(value & 0xFFU);
    bytes[1] = (uint8_t)((value >> 8) & 0xFFU);
    bytes[2] = (uint8_t)((value >> 16) & 0xFFU);
    bytes[3] = (uint8_t)((value >> 24) & 0xFFU);
    fwrite(bytes, 1, 4, file);
}

static bool write_store_zip(const char *path, const char *name, const void *data,
                            size_t size) {
    FILE *file;
    uint16_t name_len;
    long cd_start;

    file = fopen(path, "wb");
    if (file == NULL) {
        return false;
    }
    name_len = (uint16_t)strlen(name);
    wr32(file, 0x04034b50U);
    wr16(file, 20);
    wr16(file, 0);
    wr16(file, 0);
    wr16(file, 0);
    wr16(file, 0);
    wr32(file, 0);
    wr32(file, (uint32_t)size);
    wr32(file, (uint32_t)size);
    wr16(file, name_len);
    wr16(file, 0);
    fwrite(name, 1, name_len, file);
    fwrite(data, 1, size, file);
    cd_start = ftell(file);
    wr32(file, 0x02014b50U);
    wr16(file, 20);
    wr16(file, 20);
    wr16(file, 0);
    wr16(file, 0);
    wr16(file, 0);
    wr16(file, 0);
    wr32(file, 0);
    wr32(file, (uint32_t)size);
    wr32(file, (uint32_t)size);
    wr16(file, name_len);
    wr16(file, 0);
    wr16(file, 0);
    wr16(file, 0);
    wr16(file, 0);
    wr32(file, 0);
    wr32(file, 0);
    fwrite(name, 1, name_len, file);
    {
        uint32_t cd_size = (uint32_t)(ftell(file) - cd_start);
        wr32(file, 0x06054b50U);
        wr16(file, 0);
        wr16(file, 0);
        wr16(file, 1);
        wr16(file, 1);
        wr32(file, cd_size);
        wr32(file, (uint32_t)cd_start);
        wr16(file, 0);
    }
    fclose(file);
    return true;
}

static bool test_paths_register_and_zip(const char *root) {
    char joined[96];
    char zip_path[320];
    const char payload[] = "PB3DS-M9";
    const unsigned char registered[] = {0x11, 0x22, 0x33, 0x44};
    size_t size = 0;
    void *data;
    PBFsMountInfo mount;
    PBCompatState state;

    CHECK(strcmp(PB3DS_VERSION, "0.11.0-m11") == 0);
    CHECK(strstr(PB3DS_ROADMAP_STAGE, "M11") != NULL);
    CHECK(pb_fs_status() == PB_FS_READY);
    CHECK(PB_FS_ALIGN == 16U);
    CHECK(pb_fs_join(joined, sizeof(joined), "../secret") == -1);
    CHECK(pb_fs_join(joined, sizeof(joined), "../x") == -1);
    CHECK(pb_fs_join(joined, sizeof(joined), "paperboat.o2r") == 0);
    CHECK(strcmp(joined, PB_FS_PAPERBOAT_O2R) == 0);

    pb_fs_host_set_root(root);
    pb_fs_shutdown();
    mkdir(root, 0755);
    snprintf(zip_path, sizeof(zip_path), "%s/paperboat.o2r", root);
    CHECK(write_store_zip(zip_path, "shapes/mac_00", payload, strlen(payload)));

    pb_fs_init();
    pb_fs_query_mount(&mount);
    CHECK(mount.paperboat_present);
    CHECK(mount.paperboat_zip);
    CHECK(!mount.pm64_present);
    CHECK(pb_fs_exists(PB_FS_PAPERBOAT_O2R));
    CHECK(!pb_fs_exists(PB_FS_PM64_O2R));

    CHECK(pb_fs_register("boot/mark", registered, sizeof(registered)) == 0);
    data = pb_fs_lookup("boot/mark", &size);
    CHECK(data != NULL);
    CHECK(size == sizeof(registered));
    CHECK(pb_fs_is_aligned(data));
    CHECK(memcmp(data, registered, sizeof(registered)) == 0);

    data = ResourceGetDataByName("shapes/mac_00");
    CHECK(data != NULL);
    CHECK(pb_fs_is_aligned(data));
    CHECK(memcmp(data, payload, strlen(payload)) == 0);
    CHECK(ResourceGetDataByName("missing/never") == NULL);

    pb_compat_init();
    pb_compat_query(&state);
    CHECK(state.resources == PB_COMPAT_READY);

    pb_fs_shutdown();
    CHECK(ResourceGetDataByName("shapes/mac_00") == NULL);
    return true;
}

int main(int argc, char **argv) {
    const char *root;

    if (argc < 2) {
        fprintf(stderr, "usage: test_m9 <host-root>\n");
        return EXIT_FAILURE;
    }
    root = argv[1];
    if (!test_paths_register_and_zip(root)) {
        return EXIT_FAILURE;
    }
    printf("M9 filesystem contract: %u checks passed\n", checks_run);
    return EXIT_SUCCESS;
}
