#include "pb3ds/runtime_flash.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

static bool flash_range_valid(uint32_t offset, size_t length) {
    return length <= PB3DS_FLASH_TOTAL_BYTES &&
           offset <= PB3DS_FLASH_TOTAL_BYTES - length;
}

static bool flash_finish_write(FILE *file, bool success) {
    if (success && fflush(file) != 0) success = false;
    if (fclose(file) != 0) success = false;
    return success;
}

static FILE *flash_open_update(const char *path) {
    static const unsigned char zeros[PB3DS_FLASH_PAGE_BYTES] = {0};
    FILE *file = fopen(path, "r+b");
    if (file == NULL) file = fopen(path, "w+b");
    if (file == NULL) return NULL;

    if (fseek(file, 0L, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    const long current_length = ftell(file);
    if (current_length < 0L ||
        (unsigned long)current_length > PB3DS_FLASH_TOTAL_BYTES) {
        fclose(file);
        return NULL;
    }

    size_t remaining = PB3DS_FLASH_TOTAL_BYTES - (size_t)current_length;
    while (remaining != 0U) {
        const size_t chunk = remaining < sizeof(zeros) ? remaining
                                                       : sizeof(zeros);
        if (fwrite(zeros, 1U, chunk, file) != chunk) {
            fclose(file);
            return NULL;
        }
        remaining -= chunk;
    }
    if (fflush(file) != 0) {
        fclose(file);
        return NULL;
    }
    return file;
}

bool pb_flash_store_read(const char *path, uint32_t offset, void *destination,
                         size_t length) {
    if (path == NULL || path[0] == '\0' || destination == NULL ||
        !flash_range_valid(offset, length)) return false;

    memset(destination, 0, length);
    FILE *file = fopen(path, "rb");
    if (file == NULL) return errno == ENOENT;
    if (fseek(file, (long)offset, SEEK_SET) != 0) {
        fclose(file);
        return false;
    }

    const size_t received = fread(destination, 1U, length, file);
    const bool success = received == length || !ferror(file);
    return fclose(file) == 0 && success;
}

bool pb_flash_store_write_page(const char *path, uint32_t offset,
                                const void *source) {
    if (path == NULL || path[0] == '\0' || source == NULL ||
        offset % PB3DS_FLASH_PAGE_BYTES != 0U ||
        !flash_range_valid(offset, PB3DS_FLASH_PAGE_BYTES)) return false;

    FILE *file = flash_open_update(path);
    if (file == NULL) return false;
    bool success = fseek(file, (long)offset, SEEK_SET) == 0;
    if (success) {
        success = fwrite(source, 1U, PB3DS_FLASH_PAGE_BYTES, file) ==
                  PB3DS_FLASH_PAGE_BYTES;
    }
    return flash_finish_write(file, success);
}

bool pb_flash_store_erase_sector(const char *path, uint32_t offset) {
    static const unsigned char zeros[PB3DS_FLASH_PAGE_BYTES] = {0};
    if (path == NULL || path[0] == '\0' ||
        offset % PB3DS_FLASH_SECTOR_BYTES != 0U ||
        !flash_range_valid(offset, PB3DS_FLASH_SECTOR_BYTES)) return false;

    FILE *file = flash_open_update(path);
    if (file == NULL) return false;
    bool success = fseek(file, (long)offset, SEEK_SET) == 0;
    size_t remaining = PB3DS_FLASH_SECTOR_BYTES;
    while (success && remaining != 0U) {
        const size_t chunk = remaining < sizeof(zeros) ? remaining
                                                       : sizeof(zeros);
        success = fwrite(zeros, 1U, chunk, file) == chunk;
        remaining -= chunk;
    }
    return flash_finish_write(file, success);
}
