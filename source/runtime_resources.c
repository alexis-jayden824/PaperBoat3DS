#include "pb3ds/runtime_resources.h"
#include "pb3ds/gbi_command_span.h"
#include "pb3ds/gbi_resolve.h"
#include "pb3ds/texture.h"
#include "pb3ds/world_boot.h"

#include <stdio.h>
#include <string.h>

#define RESOURCE_LIMIT 4096U
#define LOADED_BUCKET_COUNT 1024U
#define ENTRY_LIMIT PB_MIB(2)
#define TYPE_BLOB 0x4F424C42U
#define TYPE_VERTEX 0x4F565458U
#define TYPE_TEXTURE 0x4F544558U
#define TYPE_DL 0x4F444C54U
#define TYPE_MATRIX 0x4F4D5458U
#define TYPE_LIGHTS 0x46669697U
#define TYPE_VEC3S 0x56433353U
#define TYPE_VIEWPORT 0x4F565054U

#define SHAPE_NODE_LIMIT 512U
#define SHAPE_DEPTH_LIMIT 32U
#define DL_DEPTH_LIMIT 32U
#define DL_COMMAND_LIMIT 65536U

#define REQUIRE(_name, _type, _minimum) \
    { (_name), (_type), 0U, 0U, 0U, (_minimum) }
#define REQUIRE_TEXTURE(_name, _format, _width, _height) \
    { (_name), TYPE_TEXTURE, (_format), (_width), (_height), 1U }
#define REQUIRE_CI4_PAIR(_name, _width, _height) \
    REQUIRE_TEXTURE(_name, PB_RESOURCE_TEXTURE_CI4, (_width), (_height)), \
    REQUIRE_TEXTURE(_name ".pal", PB_RESOURCE_TEXTURE_RGBA16, 16U, 1U)

struct PBRuntimeResource {
    PBRuntimeResource *next;
    PBRuntimeResource *next_loaded;
    /* One allocation serves normalized lookup and retained tagged pointers. */
    char tagged_name[PB_O2R_NAME_CAPACITY + 7U];
    uint64_t name_hash;
    void *data;
    size_t size, payload_size, allocation;
    uint32_t type, texture_type;
    uint16_t width, height;
};

static const char *resource_name(const PBRuntimeResource *entry) {
    return entry != NULL ? entry->tagged_name + 7U : NULL;
}

static PBRuntimeResources *bound_resources;

static PBO2RResult find_archive_entry(PBRuntimeResources *r,
                                      const char *name,
                                      PBO2REntry *entry);
static PBRuntimeResource *get_by_crc(uint64_t crc);

static const PBRuntimeResourceRequirement m13_requirements[] = {
    REQUIRE("title_screen/title_logo", TYPE_BLOB, 89600U),
    REQUIRE_TEXTURE("title_screen/title_logo_img",
                    PB_RESOURCE_TEXTURE_RGBA32, 200U, 112U),
    REQUIRE("title_screen/title_copyright", TYPE_BLOB, 4608U),
    REQUIRE_TEXTURE("title_screen/title_copyright_img",
                    PB_RESOURCE_TEXTURE_IA8, 144U, 32U),
    REQUIRE("title_screen/title_press_start", TYPE_BLOB, 4096U),
    REQUIRE_TEXTURE("title_screen/title_press_start_img",
                    PB_RESOURCE_TEXTURE_IA8, 128U, 32U),
    REQUIRE_TEXTURE("backgrounds/title_bg", PB_RESOURCE_TEXTURE_CI8,
                    296U, 200U),
    REQUIRE_TEXTURE("backgrounds/title_bg_pal0", PB_RESOURCE_TEXTURE_RGBA16,
                    256U, 1U),

    REQUIRE("charset/charset_standard", TYPE_BLOB, 0x5100U),
    REQUIRE("charset/charset_standard_palette", TYPE_BLOB, 0x500U),
    REQUIRE("charset/charset_title", TYPE_BLOB, 0xF60U),
    REQUIRE("charset/charset_subtitle", TYPE_BLOB, 0xB88U),
    REQUIRE("charset/charset_subtitle_palette", TYPE_BLOB, 0x80U),
    REQUIRE_TEXTURE("ui/filemenu/copyarrow", PB_RESOURCE_TEXTURE_IA4,
                    64U, 16U),
    REQUIRE_TEXTURE("ui/filemenu/corners_yellow", PB_RESOURCE_TEXTURE_RGBA32,
                    16U, 64U),
    REQUIRE_TEXTURE("ui/filemenu/corners_gray", PB_RESOURCE_TEXTURE_IA8,
                    16U, 32U),

    /* File-select constructs these HUD scripts eagerly, including icons for
     * empty files. Keep both indexed pixels and TLUTs in the startup gate. */
    REQUIRE_CI4_PAIR("ui/pause/cursor_hand", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/files/filename_caret", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/files/filename_space", 8U, 8U),
    REQUIRE_CI4_PAIR("ui/pause/unused_bubble", 56U, 16U),
    REQUIRE_CI4_PAIR("ui/pause/label_jp_file", 32U, 16U),
    REQUIRE_CI4_PAIR("ui/pause/label_jp_file_disabled", 32U, 16U),
    REQUIRE_CI4_PAIR("ui/files/option_mono_on", 64U, 16U),
    REQUIRE_CI4_PAIR("ui/files/option_mono_off", 64U, 16U),
    REQUIRE_CI4_PAIR("ui/files/option_stereo_on", 64U, 16U),
    REQUIRE_CI4_PAIR("ui/files/option_stereo_off", 64U, 16U),
    REQUIRE_CI4_PAIR("ui/files/eldstar", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/files/eldstar_silhouette", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/files/mamar", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/files/mamar_silhouette", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/files/skolar", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/files/skolar_silhouette", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/files/muskular", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/files/muskular_silhouette", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/files/misstar", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/files/misstar_silhouette", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/files/klevar", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/files/klevar_silhouette", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/files/kalmar", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/files/kalmar_silhouette", 16U, 16U),

    /* The first world frame creates the status bar and swaps number scripts
     * during drawing. Random-branch shimmer scripts load lazily, so their
     * complete animation is preflighted here as well. */
    REQUIRE_CI4_PAIR("ui/status/text_times", 8U, 8U),
    REQUIRE_CI4_PAIR("ui/status/text_slash", 8U, 16U),
    REQUIRE_CI4_PAIR("ui/status/text_0", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/status/text_1", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/status/text_2", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/status/text_3", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/status/text_4", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/status/text_5", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/status/text_6", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/status/text_7", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/status/text_8", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/status/text_9", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/status/text_hp", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/status/text_fp", 16U, 16U),
    REQUIRE_TEXTURE("ui/stat_heart", PB_RESOURCE_TEXTURE_RGBA32, 16U, 16U),
    REQUIRE_TEXTURE("ui/stat_flower", PB_RESOURCE_TEXTURE_RGBA32, 16U, 16U),
    REQUIRE_CI4_PAIR("ui/status/coin_0", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/status/coin_1", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/status/coin_2", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/status/coin_3", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/status/coin_4", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/status/coin_5", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/status/coin_6", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/status/coin_7", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/status/coin_8", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/status/coin_9", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/status/star_point_0", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/status/star_point_1", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/status/star_point_2", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/status/star_point_3", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/status/star_point_4", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/status/star_point_5", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/status/star_point_6", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/status/star_point_7", 16U, 16U),
    REQUIRE_TEXTURE("ui/status/star_point_shine", PB_RESOURCE_TEXTURE_IA8,
                    24U, 24U),
    REQUIRE_CI4_PAIR("ui/status/pow_star_1", 8U, 8U),
    REQUIRE_CI4_PAIR("ui/status/star_piece_0", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/status/star_piece_1", 16U, 16U),
    REQUIRE_CI4_PAIR("ui/status/star_piece_2", 16U, 16U),
    REQUIRE_CI4_PAIR("icons/anim/shimmer_0", 8U, 8U),
    REQUIRE_CI4_PAIR("icons/anim/shimmer_1", 8U, 8U),
    REQUIRE_CI4_PAIR("icons/anim/shimmer_2", 8U, 8U),
    REQUIRE_CI4_PAIR("icons/anim/shimmer_3", 8U, 8U),
    REQUIRE_CI4_PAIR("icons/anim/shimmer_4", 8U, 8U),
    REQUIRE_CI4_PAIR("icons/anim/shimmer_5", 8U, 8U),
    REQUIRE_CI4_PAIR("icons/anim/shimmer_6", 8U, 8U),
    REQUIRE_CI4_PAIR("icons/anim/star_piece_0", 32U, 32U),
    REQUIRE_CI4_PAIR("icons/anim/star_piece_1", 32U, 32U),
    REQUIRE_CI4_PAIR("icons/anim/star_piece_2", 32U, 32U),
    REQUIRE_TEXTURE("ui/box/bg_flat", PB_RESOURCE_TEXTURE_I4, 16U, 1U),
    REQUIRE_TEXTURE("ui/box/corners6", PB_RESOURCE_TEXTURE_IA8, 16U, 40U),
    REQUIRE_TEXTURE("ui/box/corners7", PB_RESOURCE_TEXTURE_IA8, 16U, 32U),

    REQUIRE("sprites/sprite_data_header", TYPE_BLOB, 12U),
    REQUIRE("sprites/player_raster_header", TYPE_BLOB, 12U),
    REQUIRE("sprites/player_raster_sets", TYPE_BLOB, 4U),
    REQUIRE("sprites/player_raster_load_descriptors", TYPE_BLOB, 4U),
    REQUIRE("sprites/player_raster_image_data", TYPE_BLOB, 4U),
    REQUIRE("sprites/player_sprite_index", TYPE_BLOB, 8U),
    REQUIRE("sprites/player_sprite_0", TYPE_BLOB, 1U),
    REQUIRE("sprites/player_sprite_1", TYPE_BLOB, 1U),
    REQUIRE("sprites/player_sprite_5", TYPE_BLOB, 1U),
    REQUIRE("sprites/player_sprite_6", TYPE_BLOB, 1U),
    REQUIRE("sprites/player_sprite_7", TYPE_BLOB, 1U),
    REQUIRE("sprites/player_sprite_8", TYPE_BLOB, 1U),

    REQUIRE("shapes/mac_00_shape", TYPE_BLOB, 1U),
    REQUIRE("collisions/mac_00_hit", TYPE_BLOB, 1U),
    REQUIRE("shapes/mac_00_shape/vtx", TYPE_VERTEX, 16U),
    REQUIRE("shapes/mac_00_shape/dlist_20", TYPE_DL,
            sizeof(PBRuntimeGfx)),
    REQUIRE("shapes/mac_01_shape", TYPE_BLOB, 1U),
    REQUIRE("collisions/mac_01_hit", TYPE_BLOB, 1U),
    REQUIRE("shapes/mac_01_shape/vtx", TYPE_VERTEX, 16U),
    REQUIRE("shapes/mac_01_shape/dlist_20", TYPE_DL,
            sizeof(PBRuntimeGfx)),
    REQUIRE_TEXTURE("backgrounds/nok_bg", PB_RESOURCE_TEXTURE_CI8,
                    296U, 200U),
    REQUIRE_TEXTURE("backgrounds/nok_bg_pal0", PB_RESOURCE_TEXTURE_RGBA16,
                    256U, 1U),
    REQUIRE_TEXTURE("textures/mac_tex/mac_frametif", PB_RESOURCE_TEXTURE_CI4,
                    64U, 64U),
    REQUIRE_TEXTURE("textures/mac_tex/mac_frametif_tlut",
                    PB_RESOURCE_TEXTURE_RGBA16, 16U, 1U),

    /* Top-level entity lists are recursively checked below. */
    REQUIRE("entities/HiddenPanel/dlist_180", TYPE_DL, sizeof(PBRuntimeGfx)),
    REQUIRE("entities/HiddenPanel/dlist_1B0", TYPE_DL, sizeof(PBRuntimeGfx)),
    REQUIRE("entities/HiddenPanel/dlist_230", TYPE_DL, sizeof(PBRuntimeGfx)),
    REQUIRE("entities/HiddenPanel/dlist_280", TYPE_DL, sizeof(PBRuntimeGfx)),
    REQUIRE("entities/HiddenPanel/dlist_2A0", TYPE_DL, sizeof(PBRuntimeGfx)),
    REQUIRE("entities/SaveBlock/mtx_3260", TYPE_MATRIX, 64U),
    REQUIRE("entities/SaveBlock/dlist_32A0", TYPE_DL, sizeof(PBRuntimeGfx)),
    REQUIRE("entities/SaveBlock/dlist_3360", TYPE_DL, sizeof(PBRuntimeGfx)),
    REQUIRE("entities/SaveBlock/dlist_3468", TYPE_DL, sizeof(PBRuntimeGfx)),
};

#undef REQUIRE_CI4_PAIR

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

void pb_runtime_resources_init(PBRuntimeResources *r, PBArchive *archive,
                               PBMemoryMonitor *memory) {
    memset(r, 0, sizeof(*r));
    r->archive = archive;
    r->memory = memory;
}

static void remember_failed_name(PBRuntimeResources *r, const char *name) {
    if (r == NULL) return;
    r->failed_name[0] = '\0';
    if (name == NULL) return;
    const size_t length = strlen(name);
    const size_t copied = length < sizeof(r->failed_name) - 1U
                              ? length
                              : sizeof(r->failed_name) - 1U;
    memcpy(r->failed_name, name, copied);
    r->failed_name[copied] = '\0';
}

void pb_runtime_resources_bind(PBRuntimeResources *r) { bound_resources = r; }

bool pb_runtime_resources_prepare(PBRuntimeResources *r) {
    if (r == NULL || r->archive == NULL || r->memory == NULL) return false;
    if (r->index != NULL) return true;
    r->error = NULL;
    r->failed_name[0] = '\0';
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

static bool validate_metadata(PBRuntimeResources *r, const char *name,
                              const uint8_t *expected, size_t expected_size,
                              bool version_entry) {
    PBO2REntry entry;
    r->archive_error = find_archive_entry(r, name, &entry);
    if (r->archive_error != PB_O2R_OK) {
        r->error = "required archive metadata missing";
        remember_failed_name(r, name);
        return false;
    }
    uint8_t *data = NULL;
    size_t size = 0U;
    r->archive_error = pb_o2r_extract_entry(
        r->archive, &entry, 16U, r->memory, PB_MEMORY_TRANSIENT,
        &data, &size, NULL);
    bool valid = r->archive_error == PB_O2R_OK;
    if (valid && version_entry) {
        valid = size == 5U && data[0] == 1U;
    } else if (valid) {
        valid = size == expected_size &&
                memcmp(data, expected, expected_size) == 0;
    }
    if (data != NULL) {
        pb_memory_free(r->memory, PB_MEMORY_TRANSIENT, data, size);
    }
    if (!valid) {
        r->error = "required archive metadata malformed";
        remember_failed_name(r, name);
    }
    return valid;
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
    r->failed_name[0] = '\0';
}

uint8_t GameEngine_OTRSigCheck(const char *data) {
    /* Match PaperBoat's small-integer guard. OTR paths are byte strings and
     * need not be naturally aligned; display-list walkers reject tagged odd
     * addresses before calling this function. */
    const uintptr_t address = (uintptr_t)data;
    if (data == NULL || !pb_gbi_host_pointer_ok(address)) {
        return 0U;
    }
    return strncmp(data, "__OTR__", 7) == 0 ? 1U : 0U;
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
    } else if (type == TYPE_BLOB || type == TYPE_VERTEX ||
               type == TYPE_VEC3S) {
        if (bytes < 4) return false;
        const uint32_t count = word(payload, big);
        payload += 4;
        bytes -= 4;
        if (type == TYPE_BLOB) {
            if (count != bytes || bytes > SIZE_MAX - 16) return false;
            allocation = bytes + 16; /* upstream BlobFactory overread padding */
        } else if (type == TYPE_VERTEX) {
            if (bytes % 16 != 0 || count != bytes / 16 || count == 0) return false;
            allocation = bytes;
        } else {
            if (bytes % 6U != 0U || count != bytes / 6U || count == 0U) {
                return false;
            }
            allocation = bytes;
        }
    } else if (type == TYPE_MATRIX) {
        if (bytes != 16U * sizeof(uint32_t)) return false;
        allocation = bytes;
    } else if (type == TYPE_VIEWPORT) {
        /* Torch OVPT is eight signed 16-bit values (vscale then vtrans). */
        if (bytes != 8U * sizeof(uint16_t)) return false;
        allocation = bytes;
    } else if (type == TYPE_LIGHTS) {
        /* Fast::LightEntry is one 8-byte ambient plus one 16-byte light. */
        if (bytes != 24U) return false;
        allocation = bytes;
    } else if (type == TYPE_DL) {
        if (bytes < 16 || payload[0] != 4 || (bytes - 8) % 8 != 0) {
            r->error = "invalid or unsupported display-list microcode";
            return false;
        }
        payload += 8;
        bytes -= 8;
        bool ended = false;
        for (size_t offset = 0; offset < bytes;) {
            const uint8_t op = (uint8_t)(word(payload + offset, big) >> 24);
            const size_t span = pb_gbi_command_span(op);
            if (span == 0U || span > (bytes - offset) / 8U) return false;
            const size_t command_bytes = span * 8U;
            if (op == 0xDF) {
                ended = offset + command_bytes == bytes;
                break;
            }
            offset += command_bytes;
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
    } else if (type == TYPE_MATRIX) {
        uint32_t *matrix = entry->data;
        for (size_t i = 0U; i < 16U; i++) {
            matrix[i] = word(payload + i * sizeof(uint32_t), big);
        }
    } else if (type == TYPE_VEC3S || type == TYPE_VIEWPORT) {
        uint16_t *vectors = entry->data;
        for (size_t i = 0U; i < bytes / sizeof(uint16_t); i++) {
            const uint8_t *value = payload + i * sizeof(uint16_t);
            vectors[i] = big ? ((uint16_t)value[0] << 8U) | value[1]
                             : ((uint16_t)value[1] << 8U) | value[0];
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
            if (entry->name_hash == hash &&
                strcmp(name, resource_name(entry)) == 0) {
                return entry;
            }
        }
        return NULL;
    }
    for (PBRuntimeResource *entry = r->head; entry != NULL;
         entry = entry->next) {
        r->lookup_probes++;
        if (strcmp(name, resource_name(entry)) == 0) return entry;
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
    memcpy(entry->tagged_name, "__OTR__", 7U);
    memcpy(entry->tagged_name + 7U, name, length + 1U);
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
    r->failed_name[0] = '\0';
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
        remember_failed_name(r, name);
        return NULL;
    }
    PBRuntimeResource *entry = load_entry(r, name, &archive_entry);
    if (entry == NULL) remember_failed_name(r, name);
    return entry;
}

bool pb_runtime_resources_validate(
    PBRuntimeResources *r,
    const PBRuntimeResourceRequirement *requirements,
    size_t requirement_count) {
    if (r == NULL || requirements == NULL || requirement_count == 0U ||
        bound_resources != r || r->index == NULL) {
        if (r != NULL) r->error = "invalid resource validation request";
        return false;
    }
    r->error = NULL;
    r->failed_name[0] = '\0';
    for (size_t index = 0U; index < requirement_count; index++) {
        const PBRuntimeResourceRequirement *required = &requirements[index];
        PBRuntimeResource *entry = get(required->name);
        if (entry == NULL) {
            remember_failed_name(r, required->name);
            r->error = r->archive_error == PB_O2R_ENTRY_NOT_FOUND
                           ? "required resource missing"
                           : "required resource malformed";
            return false;
        }
        if ((required->type != 0U && entry->type != required->type) ||
            (required->texture_type != 0U &&
             entry->texture_type != required->texture_type) ||
            (required->width != 0U && entry->width != required->width) ||
            (required->height != 0U && entry->height != required->height) ||
            entry->payload_size < required->minimum_payload_size) {
            remember_failed_name(r, required->name);
            r->error = "required resource contract mismatch";
            return false;
        }
    }
    return true;
}

static uint32_t shape_u32(const uint8_t *data) {
    /* Pinned PM64 shape blobs preserve the little-endian port layout. */
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8U) |
           ((uint32_t)data[2] << 16U) | ((uint32_t)data[3] << 24U);
}

static bool shape_span(size_t size, uint32_t offset, size_t length) {
    return (size_t)offset <= size && length <= size - (size_t)offset;
}

typedef struct {
    PBRuntimeResources *resources;
    const char *shape_resource;
    const char *texture_archive;
    const uint8_t *shape;
    size_t shape_size;
    uint32_t nodes;
    size_t commands;
} PBShapeClosure;

static bool closure_fail(PBShapeClosure *closure, const char *error,
                         const char *name) {
    closure->resources->error = error;
    remember_failed_name(closure->resources, name);
    return false;
}

static bool closure_require_name(PBShapeClosure *closure, const char *name,
                                 uint32_t type,
                                 PBRuntimeResource **result) {
    PBRuntimeResource *entry = get(name);
    if (entry == NULL) {
        /* get() retains the exact missing/malformed name and archive result. */
        closure->resources->error =
            closure->resources->archive_error == PB_O2R_ENTRY_NOT_FOUND
                ? "shape closure resource missing"
                : "shape closure resource malformed";
        return false;
    }
    if (entry->type != type) {
        return closure_fail(closure, "shape closure resource type mismatch",
                            resource_name(entry));
    }
    if (result != NULL) *result = entry;
    return true;
}

static bool closure_require_hash(PBShapeClosure *closure, uint64_t hash,
                                 uint32_t type,
                                 PBRuntimeResource **result) {
    PBRuntimeResource *entry = get_by_crc(hash);
    if (entry == NULL) {
        closure->resources->error =
            closure->resources->archive_error == PB_O2R_ENTRY_NOT_FOUND
                ? "display-list hash resource missing"
                : "display-list hash resource malformed";
        return false;
    }
    if (entry->type != type) {
        return closure_fail(closure,
                            "display-list hash resource type mismatch",
                            resource_name(entry));
    }
    if (result != NULL) *result = entry;
    return true;
}

static bool closure_display_list(PBShapeClosure *closure,
                                 PBRuntimeResource *display_list,
                                 unsigned int depth) {
    if (depth > DL_DEPTH_LIMIT || display_list == NULL ||
        display_list->type != TYPE_DL ||
        display_list->payload_size % 8U != 0U) {
        return closure_fail(closure, "display-list closure depth or layout",
                            resource_name(display_list));
    }
    const PBRuntimeGfx *commands = display_list->data;
    const size_t count = display_list->payload_size / 8U;
    for (size_t index = 0U; index < count;) {
        if (++closure->commands > DL_COMMAND_LIMIT) {
            return closure_fail(closure, "display-list closure command limit",
                                resource_name(display_list));
        }
        const uint8_t opcode =
            (uint8_t)((uint32_t)commands[index].words.w0 >> 24U);
        const size_t span = pb_gbi_command_span(opcode);
        if (span == 0U || span > count - index) {
            return closure_fail(closure, "display-list closure malformed",
                                resource_name(display_list));
        }
        if (opcode == 0x20U || opcode == 0x31U || opcode == 0x32U ||
            opcode == 0x35U || opcode == 0x36U || opcode == 0x42U) {
            if (span != 2U) {
                return closure_fail(closure, "display-list hash span mismatch",
                                    resource_name(display_list));
            }
            const uint64_t hash =
                ((uint64_t)(uint32_t)commands[index + 1U].words.w0 << 32U) |
                (uint32_t)commands[index + 1U].words.w1;
            uint32_t expected = TYPE_LIGHTS;
            if (opcode == 0x20U) expected = TYPE_TEXTURE;
            if (opcode == 0x31U || opcode == 0x35U) expected = TYPE_DL;
            if (opcode == 0x32U) expected = TYPE_VERTEX;
            if (opcode == 0x36U) expected = TYPE_MATRIX;
            if (opcode == 0x42U) {
                const uint32_t metadata =
                    (uint32_t)commands[index].words.w1;
                const uint8_t move_type =
                    (uint8_t)(metadata >> 24U);
                const uint8_t move_offset =
                    (uint8_t)(metadata >> 16U);
                const uint8_t has_offset = (uint8_t)(metadata >> 8U);
                if (has_offset > 1U || (metadata & 0xFFU) != 0U) {
                    return closure_fail(
                        closure, "malformed hashed movemem metadata",
                        resource_name(display_list));
                }
                if (move_type == 8U) {
                    if (has_offset != 0U) {
                        return closure_fail(
                            closure, "viewport movemem offset is invalid",
                            resource_name(display_list));
                    }
                    expected = TYPE_VIEWPORT;
                } else if (move_type != 10U) {
                    return closure_fail(
                        closure, "unsupported hashed movemem resource",
                        resource_name(display_list));
                } else if (move_offset < 48U || move_offset % 24U != 0U ||
                           move_offset > 240U) {
                    return closure_fail(
                        closure, "unsupported hashed light offset",
                        resource_name(display_list));
                }
            }
            PBRuntimeResource *referenced = NULL;
            if (!closure_require_hash(closure, hash, expected, &referenced)) {
                return false;
            }
            if ((opcode == 0x31U || opcode == 0x35U) &&
                !closure_display_list(closure, referenced, depth + 1U)) {
                return false;
            }
        }
        if (opcode == 0xDFU) return index + span == count;
        index += span;
    }
    return closure_fail(closure, "display-list closure lacks terminator",
                        resource_name(display_list));
}

static bool closure_texture(PBShapeClosure *closure,
                            const char *texture_name) {
    char path[PB_O2R_NAME_CAPACITY];
    const int written = snprintf(path, sizeof(path), "textures/%s/%s",
                                 closure->texture_archive, texture_name);
    if (written < 0 || (size_t)written >= sizeof(path)) {
        return closure_fail(closure, "map texture path too long",
                            texture_name);
    }
    PBRuntimeResource *texture = NULL;
    if (!closure_require_name(closure, path, TYPE_TEXTURE, &texture)) {
        return false;
    }
    if (texture->texture_type != PB_RESOURCE_TEXTURE_CI4 &&
        texture->texture_type != PB_RESOURCE_TEXTURE_CI8) {
        return true;
    }
    const uint16_t colors =
        texture->texture_type == PB_RESOURCE_TEXTURE_CI4 ? 16U : 256U;
    const size_t base_length = (size_t)written;
    if (base_length + sizeof("_tlut") > sizeof(path)) {
        return closure_fail(closure, "map palette path too long", path);
    }
    memcpy(path + base_length, "_tlut", sizeof("_tlut"));
    PBRuntimeResource *palette = NULL;
    if (!closure_require_name(closure, path, TYPE_TEXTURE, &palette)) {
        return false;
    }
    if (palette->texture_type != PB_RESOURCE_TEXTURE_RGBA16 ||
        palette->width != colors || palette->height != 1U) {
        return closure_fail(closure, "map palette contract mismatch", path);
    }
    return true;
}

static bool closure_shape_node(PBShapeClosure *closure, uint32_t offset,
                               unsigned int depth) {
    if (depth > SHAPE_DEPTH_LIMIT ||
        ++closure->nodes > SHAPE_NODE_LIMIT ||
        !shape_span(closure->shape_size, offset, 20U)) {
        return closure_fail(closure, "shape closure node bounds",
                            closure->shape_resource);
    }
    const uint8_t *node = closure->shape + offset;
    const uint32_t display_offset = shape_u32(node + 4U);
    if (display_offset != 0U) {
        if (!shape_span(closure->shape_size, display_offset, 8U)) {
            return closure_fail(closure, "shape display metadata bounds",
                                closure->shape_resource);
        }
        const uint32_t list_offset =
            shape_u32(closure->shape + display_offset);
        char path[PB_O2R_NAME_CAPACITY];
        const int written = snprintf(path, sizeof(path), "%s/dlist_%X",
                                     closure->shape_resource,
                                     (unsigned int)list_offset);
        if (list_offset == 0U || written < 0 ||
            (size_t)written >= sizeof(path)) {
            return closure_fail(closure, "shape display-list path invalid",
                                closure->shape_resource);
        }
        PBRuntimeResource *display_list = NULL;
        if (!closure_require_name(closure, path, TYPE_DL, &display_list) ||
            !closure_display_list(closure, display_list, 0U)) {
            return false;
        }
    }

    const int32_t property_count = (int32_t)shape_u32(node + 8U);
    const uint32_t properties = shape_u32(node + 12U);
    if (property_count < 0 || property_count > 1024 ||
        (property_count > 0 &&
         !shape_span(closure->shape_size, properties,
                     (size_t)property_count * 12U))) {
        return closure_fail(closure, "shape property closure bounds",
                            closure->shape_resource);
    }
    for (int32_t index = 0; index < property_count; index++) {
        const uint8_t *property =
            closure->shape + properties + (size_t)index * 12U;
        if ((int32_t)shape_u32(property) == 0x5E) {
            const uint32_t name_offset = shape_u32(property + 8U);
            if (name_offset == 0U) continue;
            if (name_offset >= closure->shape_size) {
                return closure_fail(closure, "shape texture name bounds",
                                    closure->shape_resource);
            }
            const char *name = (const char *)closure->shape + name_offset;
            if (memchr(name, '\0', closure->shape_size - name_offset) == NULL ||
                !closure_texture(closure, name)) {
                return false;
            }
        }
    }

    const uint32_t group_offset = shape_u32(node + 16U);
    if (group_offset == 0U) return true;
    if (!shape_span(closure->shape_size, group_offset, 20U)) {
        return closure_fail(closure, "shape group closure bounds",
                            closure->shape_resource);
    }
    const uint8_t *group = closure->shape + group_offset;
    const int32_t child_count = (int32_t)shape_u32(group + 12U);
    const uint32_t children = shape_u32(group + 16U);
    if (child_count < 0 || child_count > (int32_t)SHAPE_NODE_LIMIT ||
        (child_count > 0 &&
         !shape_span(closure->shape_size, children,
                     (size_t)child_count * sizeof(uint32_t)))) {
        return closure_fail(closure, "shape child closure bounds",
                            closure->shape_resource);
    }
    for (int32_t index = 0; index < child_count; index++) {
        const uint32_t child = shape_u32(
            closure->shape + children + (size_t)index * sizeof(uint32_t));
        if (child == 0U ||
            !closure_shape_node(closure, child, depth + 1U)) {
            return false;
        }
    }
    return true;
}

bool pb_runtime_resources_validate_shape_closure(
    PBRuntimeResources *r, const char *shape_resource,
    const char *texture_archive) {
    if (r == NULL || shape_resource == NULL || texture_archive == NULL ||
        bound_resources != r || r->index == NULL) {
        if (r != NULL) r->error = "invalid shape closure request";
        return false;
    }
    PBRuntimeResource *shape = get(shape_resource);
    PBWorldBootStats stats;
    memset(&stats, 0, sizeof(stats));
    if (shape == NULL || shape->type != TYPE_BLOB ||
        !pb_world_validate_shape_payload(shape->data, shape->payload_size,
                                         &stats)) {
        remember_failed_name(r, shape_resource);
        r->error = "shape closure payload malformed";
        return false;
    }
    PBShapeClosure closure = {
        .resources = r,
        .shape_resource = shape_resource,
        .texture_archive = texture_archive,
        .shape = shape->data,
        .shape_size = shape->payload_size,
    };
    return closure_shape_node(&closure, shape_u32(closure.shape), 0U);
}

bool pb_runtime_resources_validate_m13(PBRuntimeResources *r) {
    static const uint8_t port_version[] = { 0U, 1U, 0U, 0U, 0U, 1U };
    if (r == NULL || bound_resources != r || r->index == NULL) {
        if (r != NULL) r->error = "resource index not ready for preflight";
        return false;
    }
    if (!validate_metadata(r, "portVersion", port_version,
                           sizeof(port_version), false) ||
        !validate_metadata(r, "version", NULL, 0U, true)) {
        return false;
    }
    if (!pb_runtime_resources_validate(
            r, m13_requirements,
            sizeof(m13_requirements) / sizeof(m13_requirements[0]))) {
        return false;
    }

    static const struct {
        const char *shape;
        const char *collision;
        const char *texture_archive;
    } maps[] = {
        { "shapes/mac_00_shape", "collisions/mac_00_hit", "mac_tex" },
        { "shapes/mac_01_shape", "collisions/mac_01_hit", "mac_tex" },
    };
    for (size_t index = 0U; index < sizeof(maps) / sizeof(maps[0]); index++) {
        PBRuntimeResource *shape = get(maps[index].shape);
        PBWorldBootStats stats;
        memset(&stats, 0, sizeof(stats));
        if (shape == NULL ||
            !pb_world_validate_shape_payload(shape->data,
                                             shape->payload_size, &stats)) {
            remember_failed_name(r, maps[index].shape);
            r->error = "required map shape malformed";
            return false;
        }
        if (!pb_runtime_resources_validate_shape_closure(
                r, maps[index].shape, maps[index].texture_archive)) {
            return false;
        }
        PBRuntimeResource *collision = get(maps[index].collision);
        if (collision == NULL ||
            !pb_world_validate_collision_payload(
                collision->data, collision->payload_size, &stats)) {
            remember_failed_name(r, maps[index].collision);
            r->error = "required map collision malformed";
            return false;
        }
    }

    static const char *entity_display_lists[] = {
        "entities/HiddenPanel/dlist_180",
        "entities/HiddenPanel/dlist_1B0",
        "entities/HiddenPanel/dlist_230",
        "entities/HiddenPanel/dlist_280",
        "entities/HiddenPanel/dlist_2A0",
        "entities/SaveBlock/dlist_32A0",
        "entities/SaveBlock/dlist_3360",
        "entities/SaveBlock/dlist_3468",
    };
    PBShapeClosure entity_closure = { .resources = r };
    for (size_t index = 0U;
         index < sizeof(entity_display_lists) /
                     sizeof(entity_display_lists[0]);
         index++) {
        PBRuntimeResource *display_list = get(entity_display_lists[index]);
        entity_closure.commands = 0U;
        if (display_list == NULL || display_list->type != TYPE_DL ||
            !closure_display_list(&entity_closure, display_list, 0U)) {
            if (display_list == NULL) {
                r->error = r->archive_error == PB_O2R_ENTRY_NOT_FOUND
                               ? "entity display-list resource missing"
                               : "entity display-list resource malformed";
            } else if (display_list->type != TYPE_DL) {
                remember_failed_name(r, entity_display_lists[index]);
                r->error = "entity display-list resource type mismatch";
            }
            return false;
        }
    }
    return true;
}

void *ResourceGetDataByName(const char *name) { PBRuntimeResource *e = get(name); return e ? e->data : NULL; }
static PBRuntimeResource *get_by_crc(uint64_t crc) {
    PBRuntimeResources *r = bound_resources;
    if (r == NULL || r->archive == NULL) return NULL;
    r->error = NULL;
    r->failed_name[0] = '\0';
    r->archive_error = PB_O2R_OK;
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
        (void)snprintf(r->failed_name, sizeof(r->failed_name),
                       "crc64:%016llx", (unsigned long long)crc);
        return NULL;
    }
    PBRuntimeResource *entry =
        load_entry(r, archive_entry.name, &archive_entry);
    if (entry == NULL) remember_failed_name(r, archive_entry.name);
    return entry;
}
void *ResourceGetDataByCrc(uint64_t crc) { PBRuntimeResource *e = get_by_crc(crc); return e ? e->data : NULL; }
const char *ResourceGetNameByCrc(uint64_t crc) { PBRuntimeResource *e = get_by_crc(crc); return resource_name(e); }
size_t ResourceGetSizeByName(const char *name) { PBRuntimeResource *e = get(name); return e ? e->size : 0; }
size_t pb_runtime_resource_payload_size(const char *name) { PBRuntimeResource *e = get(name); return e ? e->payload_size : 0; }
uint32_t pb_runtime_resource_type(const char *name) { PBRuntimeResource *e = get(name); return e ? e->type : 0; }
uint32_t pb_runtime_resource_texture_type(const char *name) { PBRuntimeResource *e = get(name); return e ? e->texture_type : 0; }
bool pb_runtime_resource_exists(const char *name) {
    PBRuntimeResources *r = bound_resources;
    if (r == NULL || r->archive == NULL) return false;
    r->error = NULL;
    r->failed_name[0] = '\0';
    r->archive_error = PB_O2R_OK;
    name = normalize_name(r, name);
    if (name == NULL) return false;
    if (find_loaded(r, name) != NULL) return true;
    if (r->index != NULL && r->index_count != 0U) {
        const bool found =
            pb_o2r_index_contains(r->index, r->index_count, name);
        r->archive_error = found ? PB_O2R_OK : PB_O2R_ENTRY_NOT_FOUND;
        if (!found) remember_failed_name(r, name);
        return found;
    }
    PBO2REntry archive_entry;
    r->archive_error = find_archive_entry(r, name, &archive_entry);
    if (r->archive_error != PB_O2R_OK) remember_failed_name(r, name);
    return r->archive_error == PB_O2R_OK;
}
const char *pb_runtime_resource_otr_name(const char *name) {
    PBRuntimeResource *entry = get(name);
    return entry != NULL ? entry->tagged_name : NULL;
}
uint16_t ResourceGetTexWidthByName(const char *name) { PBRuntimeResource *e = get(name); return e ? e->width : 0; }
uint16_t ResourceGetTexHeightByName(const char *name) { PBRuntimeResource *e = get(name); return e ? e->height : 0; }
void *GameEngine_GetDataExact(const char *name) { return ResourceGetDataByName(name); }
size_t GameEngine_GetSizeExact(const char *name) { return ResourceGetSizeByName(name); }
uint16_t GameEngine_GetTexWidthExact(const char *name) { return ResourceGetTexWidthByName(name); }
uint16_t GameEngine_GetTexHeightExact(const char *name) { return ResourceGetTexHeightByName(name); }
