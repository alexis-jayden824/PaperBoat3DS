#ifndef PB3DS_RUNTIME_FLASH_H
#define PB3DS_RUNTIME_FLASH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PB3DS_FLASH_PAGE_BYTES 128U
#define PB3DS_FLASH_SECTOR_BYTES 0x4000U
#define PB3DS_FLASH_TOTAL_BYTES 0x20000U

bool pb_flash_store_read(const char *path, uint32_t offset, void *destination,
                         size_t length);
bool pb_flash_store_write_page(const char *path, uint32_t offset,
                                const void *source);
bool pb_flash_store_erase_sector(const char *path, uint32_t offset);

#endif
