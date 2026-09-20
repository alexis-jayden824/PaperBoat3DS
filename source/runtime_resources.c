#include "pb3ds/runtime_resources.h"
#include "pb3ds/texture.h"

#include <string.h>

#define RESOURCE_LIMIT 1024U
#define ENTRY_LIMIT PB_MIB(2)
#define TYPE_BLOB 0x4F424C42U
#define TYPE_VERTEX 0x4F565458U
#define TYPE_TEXTURE 0x4F544558U
#define TYPE_DL 0x4F444C54U

struct PBRuntimeResource {
    PBRuntimeResource *next;
    char name[PB_O2R_NAME_CAPACITY];
    void *data;
    size_t size, allocation;
    uint16_t width, height;
};

static PBRuntimeResources *bound_resources;

static uint32_t word(const uint8_t *p, bool big) {
    return big ? ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
                     ((uint32_t)p[2] << 8) | p[3]
               : ((uint32_t)p[3] << 24) | ((uint32_t)p[2] << 16) |
                     ((uint32_t)p[1] << 8) | p[0];
}

/* The second word-pair of these commands is payload, never an opcode. */
static bool expanded(uint8_t op) {
    return op == 0x20 || op == 0x31 || op == 0x32 || op == 0x33 ||
           op == 0x35 || op == 0x36 || op == 0x42;
}

void pb_runtime_resources_init(PBRuntimeResources *r, PBArchive *archive,
                               PBMemoryMonitor *memory) {
    memset(r, 0, sizeof(*r));
    r->archive = archive;
    r->memory = memory;
}

void pb_runtime_resources_bind(PBRuntimeResources *r) { bound_resources = r; }

void pb_runtime_resources_clear(PBRuntimeResources *r) {
    if (r == NULL) return;
    if (bound_resources == r) bound_resources = NULL;
    while (r->head != NULL) {
        PBRuntimeResource *entry = r->head;
        r->head = entry->next;
        pb_memory_free(r->memory, PB_MEMORY_SCENE, entry->data, entry->allocation);
        pb_memory_free(r->memory, PB_MEMORY_SCENE, entry, sizeof(*entry));
    }
    r->count = 0;
}

uint8_t GameEngine_OTRSigCheck(const char *data) {
    /* Match upstream's small-integer guard; other inputs must be valid readable
     * game pointers, as required by the Engine.h API. */
    return (uintptr_t)data >= 0x10000U && strncmp(data, "__OTR__", 7) == 0;
}

static bool decode(PBRuntimeResources *r, PBRuntimeResource *entry,
                   const uint8_t *raw, size_t size) {
    if (size < 64 || raw[0] > 1) return false;
    const bool big = raw[0] != 0;
    if (word(raw + 8, big) != 0) {
        r->error = "unsupported resource version";
        return false;
    }
    const uint32_t type = word(raw + 4, big);
    const uint8_t *payload = raw + 64;
    size_t bytes = size - 64;
    size_t allocation = 0;
    if (type == TYPE_TEXTURE) {
        PBTextureResourceView view;
        if (!pb_texture_resource_parse(raw, size, &view) ||
            view.width == 0 || view.height == 0 ||
            view.width > UINT16_MAX || view.height > UINT16_MAX ||
            !pb_texture_resource_matches(&view, (PBResourceTextureType)view.type,
                                         view.width, view.height)) return false;
        payload = view.image;
        bytes = view.image_size;
        entry->width = (uint16_t)view.width;
        entry->height = (uint16_t)view.height;
        allocation = bytes;
    } else if (type == TYPE_BLOB || type == TYPE_VERTEX) {
        if (bytes < 4) return false;
        const uint32_t count = word(payload, big);
        payload += 4;
        bytes -= 4;
        if (type == TYPE_BLOB) {
            if (count != bytes || bytes > SIZE_MAX - 16) return false;
            allocation = bytes + 16; /* upstream BlobFactory overread padding */
        } else {
            if (bytes % 16 != 0 || count != bytes / 16 || count == 0) return false;
            allocation = bytes;
        }
    } else if (type == TYPE_DL) {
        if (bytes < 16 || payload[0] != 4 || (bytes - 8) % 8 != 0) {
            r->error = "invalid or unsupported display-list microcode";
            return false;
        }
        payload += 8;
        bytes -= 8;
        bool ended = false;
        for (size_t offset = 0; offset < bytes; offset += 8) {
            const uint8_t op = (uint8_t)(word(payload + offset, big) >> 24);
            if (expanded(op)) {
                if (bytes - offset < 16) return false;
                offset += 8;
            } else if (op == 0xDF) {
                ended = offset + 8 == bytes;
                break;
            }
        }
        if (!ended || bytes / 8 > SIZE_MAX / sizeof(PBRuntimeGfx)) return false;
        allocation = bytes / 8 * sizeof(PBRuntimeGfx);
    } else {
        r->error = "unsupported resource type";
        return false;
    }

    entry->data = pb_memory_alloc(r->memory, PB_MEMORY_SCENE, allocation);
    if (entry->data == NULL) { r->error = "resource memory budget"; return false; }
    entry->allocation = allocation;
    entry->size = (type == TYPE_DL || type == TYPE_BLOB) ? allocation : bytes;
    memset(entry->data, 0, allocation);
    if (type == TYPE_DL) {
        PBRuntimeGfx *dl = entry->data;
        for (size_t i = 0; i < bytes / 8; i++) {
            dl[i].words.w0 = word(payload + i * 8, big);
            dl[i].words.w1 = word(payload + i * 8 + 4, big);
        }
    } else {
        memcpy(entry->data, payload, bytes);
        if (type == TYPE_VERTEX) {
            /* VertexFactory converts six 16-bit fields, but preserves RGBA. */
            uint8_t *v = entry->data;
            for (size_t i = 0; i < bytes; i += 16) {
                for (size_t j = 0; j < 12; j += 2) {
                    uint16_t value = big ? ((uint16_t)payload[i+j] << 8) | payload[i+j+1]
                                        : ((uint16_t)payload[i+j+1] << 8) | payload[i+j];
                    memcpy(v + i + j, &value, sizeof(value));
                }
            }
        }
    }
    return true;
}

static PBRuntimeResource *get(const char *name) {
    PBRuntimeResources *r = bound_resources;
    if (r == NULL || r->memory == NULL || r->archive == NULL) return NULL;
    r->error = NULL;
    r->archive_error = PB_O2R_OK;
    if (name == NULL) { r->error = "null resource name"; return NULL; }
    if (GameEngine_OTRSigCheck(name)) name += 7;
    const size_t length = strlen(name);
    if (length == 0 || length >= PB_O2R_NAME_CAPACITY) {
        r->error = "invalid resource name";
        return NULL;
    }
    for (PBRuntimeResource *e = r->head; e != NULL; e = e->next) {
        if (strcmp(name, e->name) == 0) { r->hits++; return e; }
    }
    if (r->count == RESOURCE_LIMIT) { r->error = "resource capacity"; return NULL; }
    PBO2RRequest request = { .name = name };
    r->archive_error = pb_o2r_find_entries(r->archive, &request, 1, NULL);
    if (r->archive_error != PB_O2R_OK) { r->error = "resource lookup failed"; return NULL; }
    uint8_t *raw = NULL;
    size_t size = 0;
    r->archive_error = pb_o2r_extract_entry(r->archive, &request.entry, ENTRY_LIMIT,
        r->memory, PB_MEMORY_TRANSIENT, &raw, &size, NULL);
    if (r->archive_error != PB_O2R_OK) { r->error = "resource extraction failed"; return NULL; }
    PBRuntimeResource *entry = pb_memory_alloc(r->memory, PB_MEMORY_SCENE, sizeof(*entry));
    bool ready = false;
    if (entry != NULL) {
        memset(entry, 0, sizeof(*entry));
        ready = decode(r, entry, raw, size);
    } else r->error = "resource memory budget";
    pb_memory_free(r->memory, PB_MEMORY_TRANSIENT, raw, size);
    if (!ready) {
        if (entry != NULL) pb_memory_free(r->memory, PB_MEMORY_SCENE, entry, sizeof(*entry));
        if (r->error == NULL) r->error = "malformed resource";
        return NULL;
    }
    memcpy(entry->name, name, length + 1);
    entry->next = r->head;
    r->head = entry;
    r->count++;
    return entry;
}

void *ResourceGetDataByName(const char *name) { PBRuntimeResource *e = get(name); return e ? e->data : NULL; }
size_t ResourceGetSizeByName(const char *name) { PBRuntimeResource *e = get(name); return e ? e->size : 0; }
uint16_t ResourceGetTexWidthByName(const char *name) { PBRuntimeResource *e = get(name); return e ? e->width : 0; }
uint16_t ResourceGetTexHeightByName(const char *name) { PBRuntimeResource *e = get(name); return e ? e->height : 0; }
void *GameEngine_GetDataExact(const char *name) { return ResourceGetDataByName(name); }
size_t GameEngine_GetSizeExact(const char *name) { return ResourceGetSizeByName(name); }
uint16_t GameEngine_GetTexWidthExact(const char *name) { return ResourceGetTexWidthByName(name); }
uint16_t GameEngine_GetTexHeightExact(const char *name) { return ResourceGetTexHeightByName(name); }
