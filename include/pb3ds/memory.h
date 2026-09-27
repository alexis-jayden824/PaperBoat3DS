#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PB_KIB(value) ((size_t)(value) * 1024U)
#define PB_MIB(value) (PB_KIB(value) * 1024U)

/* Provisional Old 3DS budgets. M4 will add accounting and pressure. */
#define PB_MEMORY_APPLICATION_RESERVE PB_MIB(8)
#define PB_MEMORY_LINEAR_RESERVE PB_MIB(4)

typedef struct {
    uint32_t application_free;
    uint32_t linear_free;
    bool measured;
} PBMemoryStatus;

void pb_memory_query(PBMemoryStatus *status);

#ifdef __cplusplus
}
#endif
