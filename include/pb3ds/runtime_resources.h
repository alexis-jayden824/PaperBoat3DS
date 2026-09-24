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
} PBRuntimeResources;

void pb_runtime_resources_init(PBRuntimeResources *resources,
                               PBArchive *archive, PBMemoryMonitor *memory);
bool pb_runtime_resources_prepare(PBRuntimeResources *resources);
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
