#pragma once

#include <3ds.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define PB_CONFIG_MAX_ENTRIES 24
#define PB_CONFIG_KEY_CAPACITY 40
#define PB_CONFIG_VALUE_CAPACITY 96

typedef struct {
    char key[PB_CONFIG_KEY_CAPACITY];
    char value[PB_CONFIG_VALUE_CAPACITY];
} PBConfigEntry;

typedef struct {
    PBConfigEntry entries[PB_CONFIG_MAX_ENTRIES];
    size_t count;
} PBConfig;

typedef struct {
    FILE *file;
    size_t size;
} PBArchive;

void pb_config_init(PBConfig *config);
bool pb_config_load(PBConfig *config, const char *path);
const char *pb_config_get(const PBConfig *config, const char *key,
                          const char *fallback);

bool pb_archive_open(PBArchive *archive, const char *path);
size_t pb_archive_read(PBArchive *archive, size_t offset, void *buffer,
                       size_t size);
void pb_archive_close(PBArchive *archive);

u64 pb_platform_time_ms(void);
