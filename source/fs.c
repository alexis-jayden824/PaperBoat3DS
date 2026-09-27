#include "pb3ds/fs.h"
#include "pb3ds/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PB_FS_CACHE 12U
#define PB_FS_EOCD_SIG 0x06054b50u
#define PB_FS_CD_SIG 0x02014b50u
#define PB_FS_LOC_SIG 0x04034b50u

typedef struct {
    char name[PB_FS_NAME_MAX];
    uint8_t *data;
    size_t size;
    void *block;
} PBFsEntry;

static bool g_fs_ready;
static PBFsMountInfo g_mount;
static PBFsEntry g_cache[PB_FS_CACHE];
static size_t g_cache_count;
#ifndef __3DS__
static char g_host_root[256];
#endif

static uint16_t u16le(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8));
}

static uint32_t u32le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static bool leaf_ok(const char *leaf) {
    return leaf != NULL && leaf[0] != '\0' && leaf[0] != '/' &&
           strchr(leaf, ':') == NULL && strstr(leaf, "..") == NULL;
}

static int native_path(char *out, size_t out_size, const char *logical) {
    const char *leaf = logical;
    size_t prefix = strlen(PB_FS_SDMC_ROOT);

    if (logical == NULL || out == NULL || out_size == 0U) {
        return -1;
    }
    if (strncmp(logical, PB_FS_SDMC_ROOT, prefix) == 0) {
        leaf = logical + prefix;
    }
#ifdef __3DS__
    if (snprintf(out, out_size, "%s", logical) < 0) {
        return -1;
    }
    (void)leaf;
#else
    if (g_host_root[0] != '\0') {
        if (snprintf(out, out_size, "%s/%s", g_host_root, leaf) < 0) {
            return -1;
        }
    } else if (snprintf(out, out_size, "%s", logical) < 0) {
        return -1;
    }
#endif
    return 0;
}

static void free_entry(PBFsEntry *entry) {
    if (entry == NULL) {
        return;
    }
    free(entry->block);
    memset(entry, 0, sizeof(*entry));
}

static void *aligned_copy(const void *src, size_t size, void **block_out) {
    uint8_t *block;
    uintptr_t addr;
    uintptr_t aligned;

    if (src == NULL || size == 0U || size > PB_FS_MAX_BLOB) {
        return NULL;
    }
    block = (uint8_t *)malloc(size + PB_FS_ALIGN + sizeof(void *));
    if (block == NULL) {
        return NULL;
    }
    addr = (uintptr_t)block + sizeof(void *);
    aligned = (addr + (uintptr_t)(PB_FS_ALIGN - 1U)) &
              ~(uintptr_t)(PB_FS_ALIGN - 1U);
    *(void **)(aligned - sizeof(void *)) = block;
    memcpy((void *)aligned, src, size);
    if (block_out != NULL) {
        *block_out = block;
    }
    return (void *)aligned;
}

static PBFsEntry *cache_find(const char *name) {
    size_t index;

    for (index = 0; index < g_cache_count; index++) {
        if (strcmp(g_cache[index].name, name) == 0) {
            return &g_cache[index];
        }
    }
    return NULL;
}

static PBFsEntry *cache_insert(const char *name, const void *src, size_t size) {
    PBFsEntry *slot;
    void *block = NULL;
    void *data;

    if (name == NULL || strlen(name) >= PB_FS_NAME_MAX) {
        return NULL;
    }
    slot = cache_find(name);
    if (slot == NULL) {
        if (g_cache_count >= PB_FS_CACHE) {
            return NULL;
        }
        slot = &g_cache[g_cache_count++];
        memset(slot, 0, sizeof(*slot));
        snprintf(slot->name, sizeof(slot->name), "%s", name);
    } else {
        free_entry(slot);
        snprintf(slot->name, sizeof(slot->name), "%s", name);
    }
    data = aligned_copy(src, size, &block);
    if (data == NULL) {
        return NULL;
    }
    slot->data = (uint8_t *)data;
    slot->size = size;
    slot->block = block;
    return slot;
}

static bool file_stat(const char *logical, uint32_t *bytes, bool *is_zip) {
    char native[320];
    FILE *file;
    uint8_t magic[4];
    long end;

    if (bytes != NULL) {
        *bytes = 0U;
    }
    if (is_zip != NULL) {
        *is_zip = false;
    }
    if (native_path(native, sizeof(native), logical) != 0) {
        return false;
    }
    file = fopen(native, "rb");
    if (file == NULL) {
        return false;
    }
    if (fseek(file, 0, SEEK_END) == 0) {
        end = ftell(file);
        if (end > 0 && bytes != NULL) {
            *bytes = (uint32_t)end;
        }
    }
    if (fseek(file, 0, SEEK_SET) == 0 && fread(magic, 1, 4, file) == 4 &&
        is_zip != NULL) {
        *is_zip = magic[0] == 'P' && magic[1] == 'K' && magic[2] == 3 &&
                  magic[3] == 4;
    }
    fclose(file);
    return true;
}

static int names_equal(const char *want, const char *have, size_t have_len) {
    if (have_len > 0U && have[0] == '/') {
        have++;
        have_len--;
    }
    if (strlen(want) != have_len) {
        return 0;
    }
    return strncmp(want, have, have_len) == 0;
}

static int zip_load_store(const char *logical, const char *name, uint8_t **out,
                          size_t *out_size) {
    char native[320];
    FILE *file;
    long file_size;
    uint8_t eocd[22];
    uint8_t *cd = NULL;
    uint32_t cd_off;
    uint32_t cd_size;
    uint32_t cursor;
    int result = -1;

    if (native_path(native, sizeof(native), logical) != 0) {
        return -1;
    }
    file = fopen(native, "rb");
    if (file == NULL) {
        return -1;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return -1;
    }
    file_size = ftell(file);
    if (file_size < 22) {
        fclose(file);
        return -1;
    }
    if (fseek(file, file_size - 22, SEEK_SET) != 0 ||
        fread(eocd, 1, 22, file) != 22U) {
        fclose(file);
        return -1;
    }
    if (u32le(eocd) != PB_FS_EOCD_SIG) {
        fclose(file);
        return -1;
    }
    cd_size = u32le(eocd + 12);
    cd_off = u32le(eocd + 16);
    if (cd_size == 0U || cd_size > (1024U * 1024U) ||
        cd_off > (uint32_t)file_size) {
        fclose(file);
        return -1;
    }
    cd = (uint8_t *)malloc(cd_size);
    if (cd == NULL || fseek(file, (long)cd_off, SEEK_SET) != 0 ||
        fread(cd, 1, cd_size, file) != cd_size) {
        free(cd);
        fclose(file);
        return -1;
    }

    cursor = 0U;
    while (cursor + 46U <= cd_size) {
        uint16_t method;
        uint32_t uncomp;
        uint32_t comp;
        uint16_t name_len;
        uint16_t extra_len;
        uint16_t comment_len;
        uint32_t local_off;
        const uint8_t *entry = cd + cursor;

        if (u32le(entry) != PB_FS_CD_SIG) {
            break;
        }
        method = u16le(entry + 10);
        comp = u32le(entry + 20);
        uncomp = u32le(entry + 24);
        name_len = u16le(entry + 28);
        extra_len = u16le(entry + 30);
        comment_len = u16le(entry + 32);
        local_off = u32le(entry + 42);
        if (cursor + 46U + name_len > cd_size) {
            break;
        }
        if (names_equal(name, (const char *)(entry + 46), name_len)) {
            uint8_t local[30];
            uint16_t loc_name;
            uint16_t loc_extra;
            uint8_t *payload;

            if (method != 0U || uncomp == 0U || uncomp > PB_FS_MAX_BLOB ||
                uncomp != comp) {
                pb_log(PB_LOG_WARNING, "fs", "zip entry not STORE or too large");
                break;
            }
            if (fseek(file, (long)local_off, SEEK_SET) != 0 ||
                fread(local, 1, 30, file) != 30U ||
                u32le(local) != PB_FS_LOC_SIG) {
                break;
            }
            loc_name = u16le(local + 26);
            loc_extra = u16le(local + 28);
            if (fseek(file, (long)local_off + 30 + loc_name + loc_extra,
                      SEEK_SET) != 0) {
                break;
            }
            payload = (uint8_t *)malloc(uncomp);
            if (payload == NULL || fread(payload, 1, uncomp, file) != uncomp) {
                free(payload);
                break;
            }
            *out = payload;
            *out_size = uncomp;
            result = 0;
            break;
        }
        cursor += 46U + name_len + extra_len + comment_len;
    }

    free(cd);
    fclose(file);
    return result;
}

static void probe_archives(void) {
    memset(&g_mount, 0, sizeof(g_mount));
    g_mount.paperboat_present =
        file_stat(PB_FS_PAPERBOAT_O2R, &g_mount.paperboat_bytes,
                  &g_mount.paperboat_zip);
    g_mount.pm64_present =
        file_stat(PB_FS_PM64_O2R, &g_mount.pm64_bytes, &g_mount.pm64_zip);
}

void pb_fs_init(void) {
    if (g_fs_ready) {
        probe_archives();
        return;
    }
    memset(g_cache, 0, sizeof(g_cache));
    g_cache_count = 0U;
    probe_archives();
    g_fs_ready = true;
    pb_log(PB_LOG_INFO, "fs", g_mount.paperboat_present ? "paperboat.o2r" :
                                                         "paperboat.o2r missing");
}

void pb_fs_shutdown(void) {
    size_t index;

    for (index = 0; index < g_cache_count; index++) {
        free_entry(&g_cache[index]);
    }
    g_cache_count = 0U;
    memset(&g_mount, 0, sizeof(g_mount));
    g_fs_ready = false;
}

PBFsStatus pb_fs_status(void) {
    return PB_FS_READY;
}

const char *pb_fs_sdmc_root(void) {
    return PB_FS_SDMC_ROOT;
}

void pb_fs_query_mount(PBFsMountInfo *info) {
    if (!g_fs_ready) {
        pb_fs_init();
    }
    if (info != NULL) {
        *info = g_mount;
    }
}

bool pb_fs_exists(const char *logical_path) {
    uint32_t bytes;
    bool zip;

    return file_stat(logical_path, &bytes, &zip);
}

int pb_fs_join(char *dst, size_t dst_size, const char *leaf) {
    int written;

    if (dst == NULL || dst_size == 0U || !leaf_ok(leaf)) {
        return -1;
    }
    written = snprintf(dst, dst_size, "%s%s", PB_FS_SDMC_ROOT, leaf);
    if (written < 0 || (size_t)written >= dst_size) {
        return -1;
    }
    return 0;
}

int pb_fs_register(const char *name, const void *data, size_t size) {
    if (!g_fs_ready) {
        pb_fs_init();
    }
    if (name == NULL || data == NULL || size == 0U) {
        return -1;
    }
    return cache_insert(name, data, size) != NULL ? 0 : -1;
}

void *pb_fs_lookup(const char *name, size_t *size_out) {
    PBFsEntry *hit;
    uint8_t *payload = NULL;
    size_t payload_size = 0U;
    const char *archives[2];
    size_t index;

    if (size_out != NULL) {
        *size_out = 0U;
    }
    if (name == NULL || name[0] == '\0' || !g_fs_ready) {
        return NULL;
    }
    hit = cache_find(name);
    if (hit != NULL) {
        if (size_out != NULL) {
            *size_out = hit->size;
        }
        return hit->data;
    }

    archives[0] = PB_FS_PAPERBOAT_O2R;
    archives[1] = PB_FS_PM64_O2R;
    for (index = 0; index < 2U; index++) {
        if (zip_load_store(archives[index], name, &payload, &payload_size) ==
            0) {
            hit = cache_insert(name, payload, payload_size);
            free(payload);
            if (hit == NULL) {
                return NULL;
            }
            if (size_out != NULL) {
                *size_out = hit->size;
            }
            return hit->data;
        }
    }
    return NULL;
}

bool pb_fs_is_aligned(const void *pointer) {
    return pointer != NULL &&
           ((uintptr_t)pointer & (uintptr_t)(PB_FS_ALIGN - 1U)) == 0U;
}

#ifndef __3DS__
void pb_fs_host_set_root(const char *native_root) {
    if (native_root == NULL) {
        g_host_root[0] = '\0';
        return;
    }
    snprintf(g_host_root, sizeof(g_host_root), "%s", native_root);
}
#endif
