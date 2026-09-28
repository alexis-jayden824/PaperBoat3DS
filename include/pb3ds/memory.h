#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PB_KIB(value) ((size_t)(value) * 1024U)
#define PB_MIB(value) (PB_KIB(value) * 1024U)

/* Provisional Old 3DS budgets. Hardware telemetry will calibrate these. */
#define PB_MEMORY_APPLICATION_RESERVE PB_MIB(8)
#define PB_MEMORY_LINEAR_RESERVE PB_MIB(4)
#define PB_MEMORY_ARCHIVE_LIMIT PB_MIB(2)
#define PB_MEMORY_SCENE_LIMIT PB_MIB(12)
#define PB_MEMORY_TRANSIENT_LIMIT PB_MIB(2)
#define PB_MEMORY_LINEAR_LIMIT PB_MIB(6)
#define PB_MEMORY_STACK_LIMIT PB_KIB(512)
#define PB_ARCHIVE_STREAM_CHUNK PB_KIB(64)

typedef enum {
    PB_MEMORY_ARCHIVE,
    PB_MEMORY_SCENE,
    PB_MEMORY_TRANSIENT,
    PB_MEMORY_LINEAR,
    PB_MEMORY_CLASS_COUNT,
} PBMemoryClass;

typedef struct {
    uint32_t application_free;
    uint32_t linear_free;
    size_t application_used;
    size_t linear_used;
    size_t stack_used;
    size_t peak_application_used;
    size_t peak_linear_used;
    size_t peak_stack_used;
    size_t class_used[PB_MEMORY_CLASS_COUNT];
    size_t class_peak[PB_MEMORY_CLASS_COUNT];
    uint32_t allocation_failures;
    bool application_measurement_available;
    bool pressure;
} PBMemorySnapshot;

typedef struct {
    PBMemorySnapshot snapshot;
    uint32_t baseline_application_free;
    uint32_t baseline_linear_free;
    uintptr_t stack_anchor;
} PBMemoryMonitor;

#ifdef __cplusplus
extern "C" {
#endif

void pb_memory_monitor_init(PBMemoryMonitor *monitor, uintptr_t stack_anchor);
void pb_memory_monitor_sample(PBMemoryMonitor *monitor, uintptr_t stack_pointer);
const PBMemorySnapshot *pb_memory_monitor_snapshot(const PBMemoryMonitor *monitor);

size_t pb_memory_class_limit(PBMemoryClass memory_class);
bool pb_memory_can_allocate(const PBMemoryMonitor *monitor,
                            PBMemoryClass memory_class, size_t size);
void *pb_memory_alloc(PBMemoryMonitor *monitor, PBMemoryClass memory_class,
                      size_t size);
void pb_memory_free(PBMemoryMonitor *monitor, PBMemoryClass memory_class,
                    void *memory, size_t size);

const char *pb_memory_class_name(PBMemoryClass memory_class);

#ifdef __cplusplus
}
#endif
