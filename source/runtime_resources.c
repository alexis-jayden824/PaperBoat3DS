#include "pb3ds/runtime_resources.h"
#include "pb3ds/texture.h"

#include <string.h>

#define RESOURCE_LIMIT 4096U
#define LOADED_BUCKET_COUNT 1024U
#define ENTRY_LIMIT PB_MIB(2)
#define TYPE_BLOB 0x4F424C42U
#define TYPE_VERTEX 0x4F565458U
#define TYPE_TEXTURE 0x4F544558U
#define TYPE_DL 0x4F444C54U

struct PBRuntimeResource {
    PBRuntimeResource *next;
    PBRuntimeResource *next_loaded;
    char name[PB_O2R_NAME_CAPACITY];
    uint64_t name_hash;
    void *data;
    size_t size, payload_size, allocation;
    uint32_t type, texture_type;
    uint16_t width, height;
};

static PBRuntimeResources *bound_resources;

static uint32_t word(const uint8_t *p, bool big) {
    return big ? ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
                     ((uint32_t)p[2] << 8) | p[3]
               : ((uint32_t)p[3] << 24) | ((uint32_t)p[2] << 16) |
                     ((uint32_t)p[1] << 8) | p[0];
}

static uint64_t resource_name_hash(const char *name) {
    uint64_t hash = UINT64_MAX;
    for (const uint8_t *p = (const uint8_t *)name; *p != 0U; p++) {
        hash ^= (uint64_t)*p << 56U;
        for (unsigned int bit = 0U; bit < 8U; bit++) {
            hash = (hash & (UINT64_C(1) << 63U)) != 0U
                       ? (hash << 1U) ^ UINT64_C(0x42F0E1EBA9EA3693)
                       : hash << 1U;
        }
    }
    return hash;
}

static size_t loaded_bucket(const PBRuntimeResources *r, uint64_t hash) {
    return r->loaded_bucket_count != 0U
               ? (size_t)(hash % r->loaded_bucket_count)
               : 0U;
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

bool pb_runtime_resources_prepare(PBRuntimeResources *r) {
    if (r == NULL || r->archive == NULL || r->memory == NULL) return false;
    if (r->index != NULL) return true;
    r->error = NULL;
    r->archive_error = PB_O2R_OK;
    size_t capacity = 0U;
    r->archive_error = pb_o2r_index_capacity(r->archive, &capacity, NULL);
    if (r->archive_error != PB_O2R_OK || capacity == 0U ||
        capacity > SIZE_MAX / sizeof(*r->index)) {
        r->error = "resource index sizing failed";
        return false;
    }
    r->index_allocation = capacity * sizeof(*r->index);
    r->index = pb_memory_alloc(r->memory, PB_MEMORY_SCENE,
                               r->index_allocation);
    if (r->index == NULL) {
        r->index_allocation = 0U;
        r->error = "resource index memory budget";
        return false;
    }
    r->archive_error = pb_o2r_build_index(r->archive, r->index, capacity,
                                          &r->index_count, NULL);
    if (r->archive_error != PB_O2R_OK || r->index_count == 0U) {
        pb_memory_free(r->memory, PB_MEMORY_SCENE, r->index,
                       r->index_allocation);
        r->index = NULL;
        r->index_count = 0U;
        r->index_allocation = 0U;
        r->error = "resource index build failed";
        return false;
    }
    r->loaded_bucket_allocation =
        LOADED_BUCKET_COUNT * sizeof(*r->loaded_buckets);
    r->loaded_buckets = pb_memory_alloc(r->memory, PB_MEMORY_SCENE,
                                        r->loaded_bucket_allocation);
    if (r->loaded_buckets == NULL) {
        pb_memory_free(r->memory, PB_MEMORY_SCENE, r->index,
                       r->index_allocation);
        r->index = NULL;
        r->index_count = 0U;
        r->index_allocation = 0U;
        r->loaded_bucket_allocation = 0U;
        r->error = "loaded resource lookup memory budget";
        return false;
    }
    memset(r->loaded_buckets, 0, r->loaded_bucket_allocation);
    r->loaded_bucket_count = LOADED_BUCKET_COUNT;
    return true;
}

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
    if (r->loaded_buckets != NULL) {
        pb_memory_free(r->memory, PB_MEMORY_SCENE, r->loaded_buckets,
                       r->loaded_bucket_allocation);
        r->loaded_buckets = NULL;
    }
    r->loaded_bucket_count = 0U;
    r->loaded_bucket_allocation = 0U;
    if (r->index != NULL) {
        pb_memory_free(r->memory, PB_MEMORY_SCENE, r->index,
                       r->index_allocation);
        r->index = NULL;
    }
    r->index_count = 0U;
    r->index_allocation = 0U;
    r->hits = 0U;
    r->lookup_probes = 0U;
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
    entry->type = type;
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
        entry->texture_type = view.type;
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
    entry->payload_size = bytes;
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

static const char *normalize_name(PBRuntimeResources *r, const char *name) {
    if (name == NULL) {
        r->error = "null resource name";
        return NULL;
    }
    if (GameEngine_OTRSigCheck(name)) name += 7;
    const size_t length = strlen(name);
    if (length == 0U || length >= PB_O2R_NAME_CAPACITY) {
        r->error = "invalid resource name";
        return NULL;
    }
    return name;
}

static PBRuntimeResource *find_loaded(PBRuntimeResources *r,
                                      const char *name) {
    const uint64_t hash = resource_name_hash(name);
    if (r->loaded_buckets != NULL && r->loaded_bucket_count != 0U) {
        for (PBRuntimeResource *entry =
                 r->loaded_buckets[loaded_bucket(r, hash)];
             entry != NULL; entry = entry->next_loaded) {
            r->lookup_probes++;
            if (entry->name_hash == hash && strcmp(name, entry->name) == 0) {
                return entry;
            }
        }
        return NULL;
    }
    for (PBRuntimeResource *entry = r->head; entry != NULL;
         entry = entry->next) {
        r->lookup_probes++;
        if (strcmp(name, entry->name) == 0) return entry;
    }
    return NULL;
}

static PBO2RResult find_archive_entry(PBRuntimeResources *r,
                                      const char *name,
                                      PBO2REntry *entry) {
    if (r->index != NULL && r->index_count != 0U) {
        return pb_o2r_find_indexed(r->archive, r->index, r->index_count,
                                   name, entry, NULL);
    }
    PBO2RRequest request = { .name = name };
    const PBO2RResult result =
        pb_o2r_find_entries(r->archive, &request, 1U, NULL);
    if (result == PB_O2R_OK) *entry = request.entry;
    return result;
}

static PBRuntimeResource *load_entry(PBRuntimeResources *r, const char *name,
                                     const PBO2REntry *archive_entry) {
    if (r->count == RESOURCE_LIMIT) {
        r->error = "resource capacity";
        return NULL;
    }
    uint8_t *raw = NULL;
    size_t size = 0U;
    r->archive_error = pb_o2r_extract_entry(r->archive, archive_entry,
        ENTRY_LIMIT, r->memory, PB_MEMORY_TRANSIENT, &raw, &size, NULL);
    if (r->archive_error != PB_O2R_OK) {
        r->error = "resource extraction failed";
        return NULL;
    }
    PBRuntimeResource *entry =
        pb_memory_alloc(r->memory, PB_MEMORY_SCENE, sizeof(*entry));
    bool ready = false;
    if (entry != NULL) {
        memset(entry, 0, sizeof(*entry));
        ready = decode(r, entry, raw, size);
    } else {
        r->error = "resource memory budget";
    }
    pb_memory_free(r->memory, PB_MEMORY_TRANSIENT, raw, size);
    if (!ready) {
        if (entry != NULL) {
            pb_memory_free(r->memory, PB_MEMORY_SCENE, entry,
                           sizeof(*entry));
        }
        if (r->error == NULL) r->error = "malformed resource";
        return NULL;
    }
    const size_t length = strlen(name);
    memcpy(entry->name, name, length + 1U);
    entry->name_hash = resource_name_hash(name);
    entry->next = r->head;
    r->head = entry;
    if (r->loaded_buckets != NULL && r->loaded_bucket_count != 0U) {
        const size_t bucket = loaded_bucket(r, entry->name_hash);
        entry->next_loaded = r->loaded_buckets[bucket];
        r->loaded_buckets[bucket] = entry;
    }
    r->count++;
    return entry;
}

static PBRuntimeResource *get(const char *name) {
    PBRuntimeResources *r = bound_resources;
    if (r == NULL || r->memory == NULL || r->archive == NULL) return NULL;
    r->error = NULL;
    r->archive_error = PB_O2R_OK;
    name = normalize_name(r, name);
    if (name == NULL) return NULL;
    PBRuntimeResource *loaded = find_loaded(r, name);
    if (loaded != NULL) {
        r->hits++;
        return loaded;
    }
    PBO2REntry archive_entry;
    r->archive_error = find_archive_entry(r, name, &archive_entry);
    if (r->archive_error != PB_O2R_OK) {
        r->error = "resource lookup failed";
        return NULL;
    }
    return load_entry(r, name, &archive_entry);
}

void *ResourceGetDataByName(const char *name) { PBRuntimeResource *e = get(name); return e ? e->data : NULL; }
static PBRuntimeResource *get_by_crc(uint64_t crc) {
    PBRuntimeResources *r = bound_resources;
    if (r == NULL || r->archive == NULL) return NULL;
    if (r->loaded_buckets != NULL && r->loaded_bucket_count != 0U) {
        for (PBRuntimeResource *e =
                 r->loaded_buckets[loaded_bucket(r, crc)];
             e != NULL; e = e->next_loaded) {
            r->lookup_probes++;
            if (e->name_hash == crc) {
                r->hits++;
                return e;
            }
        }
    } else {
        for (PBRuntimeResource *e = r->head; e != NULL; e = e->next) {
            r->lookup_probes++;
            if (e->name_hash == crc) {
                r->hits++;
                return e;
            }
        }
    }
    PBO2REntry archive_entry;
    r->archive_error = r->index != NULL && r->index_count != 0U
        ? pb_o2r_find_indexed_by_hash(r->archive, r->index, r->index_count,
                                      crc, &archive_entry, NULL)
        : pb_o2r_find_entry_by_hash(r->archive, crc, &archive_entry, NULL);
    if (r->archive_error != PB_O2R_OK) {
        r->error = "resource hash lookup failed";
        return NULL;
    }
    return load_entry(r, archive_entry.name, &archive_entry);
}
void *ResourceGetDataByCrc(uint64_t crc) { PBRuntimeResource *e = get_by_crc(crc); return e ? e->data : NULL; }
const char *ResourceGetNameByCrc(uint64_t crc) { PBRuntimeResource *e = get_by_crc(crc); return e ? e->name : NULL; }
size_t ResourceGetSizeByName(const char *name) { PBRuntimeResource *e = get(name); return e ? e->size : 0; }
size_t pb_runtime_resource_payload_size(const char *name) { PBRuntimeResource *e = get(name); return e ? e->payload_size : 0; }
uint32_t pb_runtime_resource_type(const char *name) { PBRuntimeResource *e = get(name); return e ? e->type : 0; }
uint32_t pb_runtime_resource_texture_type(const char *name) { PBRuntimeResource *e = get(name); return e ? e->texture_type : 0; }
bool pb_runtime_resource_exists(const char *name) {
    PBRuntimeResources *r = bound_resources;
    if (r == NULL || r->archive == NULL) return false;
    r->error = NULL;
    r->archive_error = PB_O2R_OK;
    name = normalize_name(r, name);
    if (name == NULL) return false;
    if (find_loaded(r, name) != NULL) return true;
    if (r->index != NULL && r->index_count != 0U) {
        const bool found =
            pb_o2r_index_contains(r->index, r->index_count, name);
        r->archive_error = found ? PB_O2R_OK : PB_O2R_ENTRY_NOT_FOUND;
        return found;
    }
    PBO2REntry archive_entry;
    r->archive_error = find_archive_entry(r, name, &archive_entry);
    return r->archive_error == PB_O2R_OK;
}
uint16_t ResourceGetTexWidthByName(const char *name) { PBRuntimeResource *e = get(name); return e ? e->width : 0; }
uint16_t ResourceGetTexHeightByName(const char *name) { PBRuntimeResource *e = get(name); return e ? e->height : 0; }
void *GameEngine_GetDataExact(const char *name) { return ResourceGetDataByName(name); }
size_t GameEngine_GetSizeExact(const char *name) { return ResourceGetSizeByName(name); }
uint16_t GameEngine_GetTexWidthExact(const char *name) { return ResourceGetTexWidthByName(name); }
uint16_t GameEngine_GetTexHeightExact(const char *name) { return ResourceGetTexHeightByName(name); }
