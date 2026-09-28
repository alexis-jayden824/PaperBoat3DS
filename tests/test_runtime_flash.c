#include "pb3ds/runtime_flash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;

#define CHECK(condition) do { \
    checks++; \
    if (!(condition)) { \
        fprintf(stderr, "flash check %u failed: %s\n", checks, #condition); \
        return 1; \
    } \
} while (0)

static int all_value(const unsigned char *data, size_t length,
                     unsigned char expected) {
    for (size_t i = 0; i < length; i++) {
        if (data[i] != expected) return 0;
    }
    return 1;
}

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    const char *path = argv[1];
    unsigned char page[PB3DS_FLASH_PAGE_BYTES];
    unsigned char readback[PB3DS_FLASH_SECTOR_BYTES];
    remove(path);

    memset(readback, 0xA5, sizeof(readback));
    CHECK(pb_flash_store_read(path, 0U, readback, sizeof(readback)));
    CHECK(all_value(readback, sizeof(readback), 0U));

    memset(page, 0x31, sizeof(page));
    CHECK(pb_flash_store_write_page(path, 9U * PB3DS_FLASH_PAGE_BYTES,
                                    page));
    memset(readback, 0xA5, sizeof(readback));
    CHECK(pb_flash_store_read(path, 0U, readback, sizeof(readback)));
    CHECK(all_value(readback, 9U * PB3DS_FLASH_PAGE_BYTES, 0U));
    CHECK(all_value(readback + 9U * PB3DS_FLASH_PAGE_BYTES,
                    PB3DS_FLASH_PAGE_BYTES, 0x31));
    CHECK(all_value(readback + 10U * PB3DS_FLASH_PAGE_BYTES,
                    sizeof(readback) - 10U * PB3DS_FLASH_PAGE_BYTES, 0U));

    memset(page, 0x72, sizeof(page));
    CHECK(pb_flash_store_write_page(path, PB3DS_FLASH_SECTOR_BYTES, page));
    CHECK(pb_flash_store_erase_sector(path, 0U));
    memset(readback, 0xA5, sizeof(readback));
    CHECK(pb_flash_store_read(path, 0U, readback, sizeof(readback)));
    CHECK(all_value(readback, sizeof(readback), 0U));
    memset(page, 0, sizeof(page));
    CHECK(pb_flash_store_read(path, PB3DS_FLASH_SECTOR_BYTES, page,
                              sizeof(page)));
    CHECK(all_value(page, sizeof(page), 0x72));

    CHECK(!pb_flash_store_write_page(path, 1U, page));
    CHECK(!pb_flash_store_write_page(path, PB3DS_FLASH_TOTAL_BYTES, page));
    CHECK(!pb_flash_store_erase_sector(path, PB3DS_FLASH_PAGE_BYTES));
    CHECK(!pb_flash_store_erase_sector(path, PB3DS_FLASH_TOTAL_BYTES));
    CHECK(!pb_flash_store_read(path, PB3DS_FLASH_TOTAL_BYTES, page, 1U));
    CHECK(!pb_flash_store_read(path, 0U, NULL, sizeof(page)));

    FILE *truncated = fopen(path, "wb");
    CHECK(truncated != NULL);
    const unsigned char prefix[] = {0x12, 0x34, 0x56};
    CHECK(fwrite(prefix, 1U, sizeof(prefix), truncated) == sizeof(prefix));
    CHECK(fclose(truncated) == 0);
    memset(page, 0xA5, sizeof(page));
    CHECK(pb_flash_store_read(path, 0U, page, sizeof(page)));
    CHECK(memcmp(page, prefix, sizeof(prefix)) == 0);
    CHECK(all_value(page + sizeof(prefix), sizeof(page) - sizeof(prefix), 0U));

    CHECK(remove(path) == 0);
    printf("runtime flash: %u checks passed\n", checks);
    return 0;
}
