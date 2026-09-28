#pragma once

#include "pb3ds/o2r.h"
#include "pb3ds/runtime_resource_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Stable, writable native resources, owned until explicit shutdown. Never
 * evict behind game pointers. Clear only after all game/GPU users are stopped. */
typedef struct PBRuntimeResource PBRuntimeResource;
typedef struct {
    const char *name;
    uint32_t type;
    uint32_t texture_type;
    uint16_t width;
    uint16_t height;
    size_t minimum_payload_size;
} PBRuntimeResourceRequirement;

typedef struct {
    PBArchive *archive;
    PBMemoryMonitor *memory;
    PBRuntimeResource *head;
    PBRuntimeResource **loaded_buckets;
    size_t loaded_bucket_count;
    size_t loaded_bucket_allocation;
    PBO2RIndexEntry *index;
    size_t index_count;
    size_t index_allocation;
    size_t count;
    size_t hits;
    size_t lookup_probes;
    PBO2RResult archive_error;
    const char *error;
    char failed_name[PB_O2R_NAME_CAPACITY];
} PBRuntimeResources;

void pb_runtime_resources_init(PBRuntimeResources *resources,
                               PBArchive *archive, PBMemoryMonitor *memory);
bool pb_runtime_resources_prepare(PBRuntimeResources *resources);
bool pb_runtime_resources_validate(
    PBRuntimeResources *resources,
    const PBRuntimeResourceRequirement *requirements,
    size_t requirement_count);
/* Validate the pinned PaperBoat 1.0.1 metadata and the resources that must be
 * usable before title/file select/mac_00 startup is allowed to mutate game
 * state. This intentionally decodes the entries instead of checking names. */
bool pb_runtime_resources_validate_m13(PBRuntimeResources *resources);
/* Decode a validated Torch shape tree and require every display list, hashed
 * Fast3D dependency, map texture, and CI palette reachable from it. */
bool pb_runtime_resources_validate_shape_closure(
    PBRuntimeResources *resources, const char *shape_resource,
    const char *texture_archive);
void pb_runtime_resources_clear(PBRuntimeResources *resources);
/* Single game instance, like upstream's engine singleton. NULL unbinds. */
void pb_runtime_resources_bind(PBRuntimeResources *resources);

void *ResourceGetDataByName(const char *name);
void *ResourceGetDataByCrc(uint64_t crc);
const char *ResourceGetNameByCrc(uint64_t crc);
size_t ResourceGetSizeByName(const char *name);
/* Exact serialized payload size, excluding BlobFactory's safety padding. */
size_t pb_runtime_resource_payload_size(const char *name);
uint32_t pb_runtime_resource_type(const char *name);
uint32_t pb_runtime_resource_texture_type(const char *name);
bool pb_runtime_resource_exists(const char *name);
/* Stable __OTR__-prefixed name owned by the loaded resource cache.  This is
 * suitable for upstream structures which retain a path until shutdown. */
const char *pb_runtime_resource_otr_name(const char *name);
uint16_t ResourceGetTexWidthByName(const char *name);
uint16_t ResourceGetTexHeightByName(const char *name);
void *GameEngine_GetDataExact(const char *name);
size_t GameEngine_GetSizeExact(const char *name);
uint16_t GameEngine_GetTexWidthExact(const char *name);
uint16_t GameEngine_GetTexHeightExact(const char *name);
uint8_t GameEngine_OTRSigCheck(const char *data);

#ifdef __cplusplus
}
#endif
