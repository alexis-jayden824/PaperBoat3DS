#include "pb3ds/memory.h"

#include <stdlib.h>
#include <string.h>

static size_t size_delta(u32 baseline, u32 current) {
    return current < baseline ? (size_t)(baseline - current) : 0;
}

static size_t stack_delta(uintptr_t anchor, uintptr_t current) {
    return anchor > current ? (size_t)(anchor - current)
                            : (size_t)(current - anchor);
}

static bool add_would_overflow(size_t left, size_t right) {
    return right > SIZE_MAX - left;
}

static bool memory_class_is_valid(PBMemoryClass memory_class) {
    return (unsigned int)memory_class < (unsigned int)PB_MEMORY_CLASS_COUNT;
}

size_t pb_memory_class_limit(PBMemoryClass memory_class) {
    switch (memory_class) {
        case PB_MEMORY_ARCHIVE:
            return PB_MEMORY_ARCHIVE_LIMIT;
        case PB_MEMORY_SCENE:
            return PB_MEMORY_SCENE_LIMIT;
        case PB_MEMORY_TRANSIENT:
            return PB_MEMORY_TRANSIENT_LIMIT;
        case PB_MEMORY_LINEAR:
            return PB_MEMORY_LINEAR_LIMIT;
        case PB_MEMORY_CLASS_COUNT:
        default:
            return 0;
    }
}

const char *pb_memory_class_name(PBMemoryClass memory_class) {
    switch (memory_class) {
        case PB_MEMORY_ARCHIVE:
            return "archive";
        case PB_MEMORY_SCENE:
            return "scene";
        case PB_MEMORY_TRANSIENT:
            return "transient";
        case PB_MEMORY_LINEAR:
            return "linear";
        case PB_MEMORY_CLASS_COUNT:
        default:
            return "invalid";
    }
}

void pb_memory_monitor_init(PBMemoryMonitor *monitor, uintptr_t stack_anchor) {
    memset(monitor, 0, sizeof(*monitor));
    monitor->stack_anchor = stack_anchor;
    monitor->baseline_application_free = osGetMemRegionFree(MEMREGION_APPLICATION);
    monitor->baseline_linear_free = linearSpaceFree();
    monitor->snapshot.application_measurement_available =
        monitor->baseline_application_free != 0;
    pb_memory_monitor_sample(monitor, stack_anchor);
}

void pb_memory_monitor_sample(PBMemoryMonitor *monitor, uintptr_t stack_pointer) {
    PBMemorySnapshot *snapshot = &monitor->snapshot;
    snapshot->application_free = osGetMemRegionFree(MEMREGION_APPLICATION);
    snapshot->linear_free = linearSpaceFree();
    snapshot->application_measurement_available =
        monitor->baseline_application_free != 0 && snapshot->application_free != 0;

    if (snapshot->application_measurement_available) {
        snapshot->application_used = size_delta(monitor->baseline_application_free,
                                                snapshot->application_free);
        if (snapshot->application_used > snapshot->peak_application_used) {
            snapshot->peak_application_used = snapshot->application_used;
        }
    }

    snapshot->linear_used = size_delta(monitor->baseline_linear_free,
                                       snapshot->linear_free);
    if (snapshot->linear_used > snapshot->peak_linear_used) {
        snapshot->peak_linear_used = snapshot->linear_used;
    }

    snapshot->stack_used = stack_delta(monitor->stack_anchor, stack_pointer);
    if (snapshot->stack_used > snapshot->peak_stack_used) {
        snapshot->peak_stack_used = snapshot->stack_used;
    }

    const bool under_pressure =
        (snapshot->application_measurement_available &&
         snapshot->application_free < PB_MEMORY_APPLICATION_RESERVE) ||
        snapshot->linear_free < PB_MEMORY_LINEAR_RESERVE ||
        snapshot->peak_stack_used > PB_MEMORY_STACK_LIMIT;
    snapshot->pressure = snapshot->pressure || under_pressure;
}

const PBMemorySnapshot *pb_memory_monitor_snapshot(const PBMemoryMonitor *monitor) {
    return &monitor->snapshot;
}

bool pb_memory_can_allocate(const PBMemoryMonitor *monitor,
                            PBMemoryClass memory_class, size_t size) {
    if (!memory_class_is_valid(memory_class) || size == 0) {
        return false;
    }

    const PBMemorySnapshot *snapshot = &monitor->snapshot;
    const size_t used = snapshot->class_used[memory_class];
    const size_t limit = pb_memory_class_limit(memory_class);
    if (add_would_overflow(used, size) || used + size > limit) {
        return false;
    }

    if (memory_class == PB_MEMORY_LINEAR) {
        return snapshot->linear_free >= PB_MEMORY_LINEAR_RESERVE &&
               size <= snapshot->linear_free - PB_MEMORY_LINEAR_RESERVE;
    }

    if (!snapshot->application_measurement_available) {
        return true;
    }
    return snapshot->application_free >= PB_MEMORY_APPLICATION_RESERVE &&
           size <= snapshot->application_free - PB_MEMORY_APPLICATION_RESERVE;
}

void *pb_memory_alloc(PBMemoryMonitor *monitor, PBMemoryClass memory_class,
                      size_t size) {
    const uintptr_t stack_marker = (uintptr_t)&monitor;
    pb_memory_monitor_sample(monitor, stack_marker);
    if (!pb_memory_can_allocate(monitor, memory_class, size)) {
        monitor->snapshot.allocation_failures++;
        monitor->snapshot.pressure = true;
        return NULL;
    }

    void *memory = memory_class == PB_MEMORY_LINEAR ? linearAlloc(size)
                                                    : malloc(size);
    if (memory == NULL) {
        monitor->snapshot.allocation_failures++;
        monitor->snapshot.pressure = true;
        return NULL;
    }

    PBMemorySnapshot *snapshot = &monitor->snapshot;
    snapshot->class_used[memory_class] += size;
    if (snapshot->class_used[memory_class] >
        snapshot->class_peak[memory_class]) {
        snapshot->class_peak[memory_class] = snapshot->class_used[memory_class];
    }
    pb_memory_monitor_sample(monitor, stack_marker);
    return memory;
}

void pb_memory_free(PBMemoryMonitor *monitor, PBMemoryClass memory_class,
                    void *memory, size_t size) {
    if (memory == NULL || !memory_class_is_valid(memory_class)) {
        return;
    }

    if (memory_class == PB_MEMORY_LINEAR) {
        linearFree(memory);
    } else {
        free(memory);
    }

    PBMemorySnapshot *snapshot = &monitor->snapshot;
    snapshot->class_used[memory_class] =
        size < snapshot->class_used[memory_class]
            ? snapshot->class_used[memory_class] - size
            : 0;
    const uintptr_t stack_marker = (uintptr_t)&monitor;
    pb_memory_monitor_sample(monitor, stack_marker);
}
