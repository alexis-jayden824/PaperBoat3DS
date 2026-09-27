#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PB_KIB(value) ((size_t)(value) * 1024U)
#define PB_MIB(value) (PB_KIB(value) * 1024U)

/* Old 3DS is the budget even when New 3DS extra RAM is present. */
#define PB_MEMORY_APPLICATION_RESERVE PB_MIB(8)
#define PB_MEMORY_LINEAR_RESERVE PB_MIB(4)

typedef enum {
    PB_MEMORY_UNMEASURED = 0,
    PB_MEMORY_OK,
    PB_MEMORY_WARNING,
    PB_MEMORY_CRITICAL,
} PBMemoryPressure;

typedef struct {
    uint32_t application_size;
    uint32_t application_free;
    uint32_t linear_free;
    PBMemoryPressure pressure;
    bool measured;
} PBMemoryStatus;

void pb_memory_query(PBMemoryStatus *status);
const char *pb_memory_pressure_name(PBMemoryPressure pressure);
#ifndef __3DS__
void pb_memory_host_set(uint32_t application_free, uint32_t linear_free,
                        uint32_t application_size, bool measured);
#endif

static inline PBMemoryPressure pb_memory_classify(const PBMemoryStatus *status) {
    if (status == NULL || !status->measured) {
        return PB_MEMORY_UNMEASURED;
    }
    if (status->application_free < (uint32_t)(PB_MEMORY_APPLICATION_RESERVE / 4U) ||
        status->linear_free < (uint32_t)(PB_MEMORY_LINEAR_RESERVE / 4U)) {
        return PB_MEMORY_CRITICAL;
    }
    if (status->application_free < (uint32_t)PB_MEMORY_APPLICATION_RESERVE ||
        status->linear_free < (uint32_t)PB_MEMORY_LINEAR_RESERVE) {
        return PB_MEMORY_WARNING;
    }
    return PB_MEMORY_OK;
}

#ifdef __cplusplus
}
#endif
