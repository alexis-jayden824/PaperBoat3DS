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
    size_t count;
    size_t hits;
    PBO2RResult archive_error;
    const char *error;
} PBRuntimeResources;

void pb_runtime_resources_init(PBRuntimeResources *resources,
                               PBArchive *archive, PBMemoryMonitor *memory);
void pb_runtime_resources_clear(PBRuntimeResources *resources);
/* Single game instance, like upstream's engine singleton. NULL unbinds. */
void pb_runtime_resources_bind(PBRuntimeResources *resources);

void *ResourceGetDataByName(const char *name);
size_t ResourceGetSizeByName(const char *name);
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
