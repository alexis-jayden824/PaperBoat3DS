#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "pb3ds/hw.h"
#include "pb3ds/memory.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PB_BREADCRUMB_CAPACITY 16U
#define PB_BREADCRUMB_LINE 64U

typedef struct {
    uint64_t time_ms;
    char text[PB_BREADCRUMB_LINE];
} PBBreadcrumb;

typedef struct {
    PBHardwareModel hardware;
    bool new_3ds_features_enabled;
    PBMemoryStatus memory;
    bool assert_failed;
    bool gfx_ready;
    uint32_t frames;
    size_t breadcrumb_count;
    size_t log_count;
} PBRuntimeStatus;

void pb_diag_init(void);
void pb_diag_reset(void);
void pb_breadcrumb(const char *text);
size_t pb_breadcrumb_count(void);
const PBBreadcrumb *pb_breadcrumb_at(size_t oldest_index);
void pb_runtime_query(PBRuntimeStatus *status, uint32_t frames, bool gfx_ready);

#ifdef __cplusplus
}
#endif
