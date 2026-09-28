#ifdef PB3DS_GAME_OBJECTS

#include "pb3ds/fs.h"
#include "pb3ds/f3d.h"
#include "pb3ds/log.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define PB_GAME_RESOURCE_SLOTS 512U
#define PB_GAME_RESOURCE_BYTES (8U * 1024U * 1024U)
#define PB_OTR_HEADER 64U
#define PB_OTR_BLOB 0x4F424C42U
#define PB_OTR_VERTEX 0x4F565458U
#define PB_OTR_TEXTURE 0x4F544558U
#define PB_OTR_DL 0x4F444C54U

typedef struct {
    char name[PB_FS_NAME_MAX];
    uint8_t *data;
    size_t size;
    uint16_t width;
    uint16_t height;
} PBGameResource;

static PBGameResource g_resources[PB_GAME_RESOURCE_SLOTS];
static size_t g_resource_count;
static size_t g_resource_bytes;

static uint32_t read_word(const uint8_t *p, bool big) {
    return big ? ((uint32_t)p[0] << 24U) | ((uint32_t)p[1] << 16U) |
                     ((uint32_t)p[2] << 8U) | p[3]
               : ((uint32_t)p[3] << 24U) | ((uint32_t)p[2] << 16U) |
                     ((uint32_t)p[1] << 8U) | p[0];
}

static uint16_t read_half(const uint8_t *p, bool big) {
    return big ? (uint16_t)(((uint16_t)p[0] << 8U) | p[1])
               : (uint16_t)(((uint16_t)p[1] << 8U) | p[0]);
}

uint8_t GameEngine_OTRSigCheck(const char *data) {
    uintptr_t address = (uintptr_t)data;
    /* N64 segmented/KSEG addresses and small integer sentinels are not
     * dereferenceable on ARM11. Refuse them before inspecting the signature. */
    if (address < 0x10000U ||
        (address <= UINT32_MAX && address >= 0x80000000U)) {
        return 0U;
    }
    return memcmp(data, "__OTR__", 7U) == 0 ? 1U : 0U;
}

static const char *asset_name(const char *name) {
    if (name == NULL) {
        return NULL;
    }
    if (GameEngine_OTRSigCheck(name)) {
        name += 7;
    }
    if (name[0] == '\0' || strlen(name) >= PB_FS_NAME_MAX) {
        return NULL;
    }
    return name;
}

/* Convert an OTR factory payload into the exact bytes expected by the game.
 * Keep resources alive until shutdown because display lists retain pointers. */
static bool decode_resource(PBGameResource *entry, const uint8_t *raw,
                            size_t raw_size) {
    const uint8_t *payload;
    size_t size;
    uint32_t type;
    bool big;
    size_t i;

    if (raw == NULL || raw_size < PB_OTR_HEADER || raw[0] > 1U) {
        return false;
    }
    big = raw[0] != 0U;
    type = read_word(raw + 4U, big);
    if (read_word(raw + 8U, big) != 0U) {
        return false;
    }
    payload = raw + PB_OTR_HEADER;
    size = raw_size - PB_OTR_HEADER;
    if (type == PB_OTR_TEXTURE) {
        uint32_t width;
        uint32_t height;
        uint32_t image_size;
        if (size < 16U) return false;
        width = read_word(payload + 4U, big);
        height = read_word(payload + 8U, big);
        image_size = read_word(payload + 12U, big);
        if (width == 0U || height == 0U || width > UINT16_MAX ||
            height > UINT16_MAX || image_size != size - 16U) return false;
        entry->width = (uint16_t)width;
        entry->height = (uint16_t)height;
        payload += 16U;
        size = image_size;
    } else if (type == PB_OTR_BLOB || type == PB_OTR_VERTEX) {
        uint32_t count;
        if (size < 4U) return false;
        count = read_word(payload, big);
        payload += 4U;
        size -= 4U;
        if ((type == PB_OTR_BLOB && count != size) ||
            (type == PB_OTR_VERTEX &&
             (size % sizeof(PBVtx) != 0U || count != size / sizeof(PBVtx)))) {
            return false;
        }
    } else if (type == PB_OTR_DL) {
        if (size < 16U || payload[0] != 4U || (size - 8U) % sizeof(PBGfx)) {
            return false;
        }
        payload += 8U;
        size -= 8U;
    } else {
        return false;
    }

    if (size == 0U || size > PB_GAME_RESOURCE_BYTES - g_resource_bytes) {
        return false;
    }
    entry->data = (uint8_t *)malloc(size + (type == PB_OTR_BLOB ? 16U : 0U));
    if (entry->data == NULL) return false;
    entry->size = size;
    memcpy(entry->data, payload, size);
    if (type == PB_OTR_BLOB) {
        memset(entry->data + size, 0, 16U);
    } else if (type == PB_OTR_VERTEX) {
        /* N64 vertex coordinates and UVs are big/little endian 16-bit cells;
         * colors occupy the last four bytes and need no conversion. */
        for (i = 0U; i < size; i += sizeof(PBVtx)) {
            size_t j;
            for (j = 0U; j < 12U; j += 2U) {
                uint16_t value = read_half(payload + i + j, big);
                memcpy(entry->data + i + j, &value, sizeof(value));
            }
        }
    } else if (type == PB_OTR_DL) {
        for (i = 0U; i < size; i += sizeof(PBGfx)) {
            PBGfx *packet = (PBGfx *)(void *)(entry->data + i);
            packet->w0 = read_word(payload + i, big);
            packet->w1 = read_word(payload + i + 4U, big);
        }
    }
    g_resource_bytes += size;
    return true;
}

static PBGameResource *resource_get(const char *requested) {
    PBGameResource *entry;
    uint8_t *raw = NULL;
    size_t raw_size = 0U;
    const char *name = asset_name(requested);
    size_t i;

    if (name == NULL) return NULL;
    for (i = 0U; i < g_resource_count; i++) {
        if (strcmp(g_resources[i].name, name) == 0) return &g_resources[i];
    }
    if (g_resource_count >= PB_GAME_RESOURCE_SLOTS ||
        !pb_fs_load_raw(name, &raw, &raw_size)) return NULL;
    entry = &g_resources[g_resource_count];
    memset(entry, 0, sizeof(*entry));
    if (!decode_resource(entry, raw, raw_size)) {
        free(raw);
        pb_log(PB_LOG_WARNING, "resource", "invalid or oversized OTR asset");
        return NULL;
    }
    free(raw);
    memcpy(entry->name, name, strlen(name) + 1U);
    g_resource_count++;
    return entry;
}

void *ResourceGetDataByName(const char *name) {
    PBGameResource *entry = resource_get(name);
    return entry != NULL ? entry->data : NULL;
}

size_t ResourceGetSizeByName(const char *name) {
    PBGameResource *entry = resource_get(name);
    return entry != NULL ? entry->size : 0U;
}

uint16_t GameEngine_GetTexWidthExact(const char *name) {
    PBGameResource *entry = resource_get(name);
    return entry != NULL ? entry->width : 0U;
}

uint16_t GameEngine_GetTexHeightExact(const char *name) {
    PBGameResource *entry = resource_get(name);
    return entry != NULL ? entry->height : 0U;
}

void pb_game_resources_shutdown(void) {
    size_t i;
    for (i = 0U; i < g_resource_count; i++) {
        free(g_resources[i].data);
        memset(&g_resources[i], 0, sizeof(g_resources[i]));
    }
    g_resource_count = 0U;
    g_resource_bytes = 0U;
}

#endif
