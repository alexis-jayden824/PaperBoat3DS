#include "pb3ds/compat.h"
#include "pb3ds/platform.h"

#include <3ds.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>

static char *trim(char *text) {
    while (isspace((unsigned char)*text)) {
        text++;
    }

    char *end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) {
        end--;
    }
    *end = '\0';
    return text;
}

void pb_config_init(PBConfig *config) {
    memset(config, 0, sizeof(*config));
}

bool pb_config_load(PBConfig *config, const char *path) {
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return false;
    }

    char line[PB_CONFIG_KEY_CAPACITY + PB_CONFIG_VALUE_CAPACITY + 8];
    while (config->count < PB_CONFIG_MAX_ENTRIES &&
           fgets(line, sizeof(line), file) != NULL) {
        char *key = trim(line);
        if (*key == '\0' || *key == '#' || *key == ';') {
            continue;
        }

        char *separator = strchr(key, '=');
        if (separator == NULL) {
            continue;
        }
        *separator = '\0';

        key = trim(key);
        char *value = trim(separator + 1);
        if (*key == '\0') {
            continue;
        }

        PBConfigEntry *entry = &config->entries[config->count];
        (void)snprintf(entry->key, sizeof(entry->key), "%s", key);
        (void)snprintf(entry->value, sizeof(entry->value), "%s", value);
        config->count++;
    }

    fclose(file);
    return true;
}

const char *pb_config_get(const PBConfig *config, const char *key,
                          const char *fallback) {
    for (size_t index = 0; index < config->count; index++) {
        if (strcmp(config->entries[index].key, key) == 0) {
            return config->entries[index].value;
        }
    }
    return fallback;
}

bool pb_archive_open(PBArchive *archive, const char *path) {
    archive->file = fopen(path, "rb");
    archive->size = 0;
    if (archive->file == NULL) {
        return false;
    }

    if (fseek(archive->file, 0, SEEK_END) != 0) {
        pb_archive_close(archive);
        return false;
    }
    const long end = ftell(archive->file);
    if (end < 0 || fseek(archive->file, 0, SEEK_SET) != 0) {
        pb_archive_close(archive);
        return false;
    }

    archive->size = (size_t)end;
    return true;
}

size_t pb_archive_read(PBArchive *archive, size_t offset, void *buffer,
                       size_t size) {
    if (archive->file == NULL || offset > archive->size ||
        offset > (size_t)LONG_MAX) {
        return 0;
    }

    const size_t available = archive->size - offset;
    size_t requested = size < available ? size : available;
    if (requested > PB_ARCHIVE_MAX_READ) {
        requested = PB_ARCHIVE_MAX_READ;
    }
    if (fseek(archive->file, (long)offset, SEEK_SET) != 0) {
        return 0;
    }
    return fread(buffer, 1, requested, archive->file);
}

void pb_archive_close(PBArchive *archive) {
    if (archive->file != NULL) {
        fclose(archive->file);
        archive->file = NULL;
    }
    archive->size = 0;
}

uint64_t pb_platform_time_ms(void) {
    return osGetTime();
}

PBAudioStatus pb_platform_audio_status(void) {
    return PB_AUDIO_DEFERRED_M14;
}
