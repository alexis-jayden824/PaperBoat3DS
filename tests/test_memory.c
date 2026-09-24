#include "pb3ds/memory.h"

#include <stdio.h>
#include <stdlib.h>

static uint32_t fake_application_free;
static uint32_t fake_linear_free;
static unsigned int checks_run;

#define CHECK(expression)                                                      \
    do {                                                                       \
        checks_run++;                                                          \
        if (!(expression)) {                                                   \
            fprintf(stderr, "memory policy check failed at %s:%d: %s\n",     \
                    __FILE__, __LINE__, #expression);                          \
            return false;                                                      \
        }                                                                      \
    } while (0)

uint32_t osGetMemRegionFree(int region) {
    (void)region;
    return fake_application_free;
}

uint32_t linearSpaceFree(void) {
    return fake_linear_free;
}

void *linearAlloc(size_t size) {
    return malloc(size);
}

void linearFree(void *memory) {
    free(memory);
}

static void reset_monitor(PBMemoryMonitor *monitor) {
    fake_application_free = (uint32_t)PB_MIB(32);
    fake_linear_free = (uint32_t)PB_MIB(16);
    pb_memory_monitor_init(monitor, (uintptr_t)PB_MIB(16));
}

static bool test_class_limits(void) {
    PBMemoryMonitor monitor;
    reset_monitor(&monitor);

    CHECK(pb_memory_class_limit(PB_MEMORY_ARCHIVE) == PB_MIB(2));
    CHECK(pb_memory_class_limit(PB_MEMORY_SCENE) == PB_MIB(12));
    CHECK(pb_memory_class_limit(PB_MEMORY_TRANSIENT) == PB_MIB(2));
    CHECK(pb_memory_class_limit(PB_MEMORY_LINEAR) == PB_MIB(6));
    CHECK(pb_memory_class_limit((PBMemoryClass)-1) == 0);
    CHECK(!pb_memory_can_allocate(&monitor, (PBMemoryClass)-1, 1));
    CHECK(!pb_memory_can_allocate(&monitor, PB_MEMORY_ARCHIVE, 0));
    CHECK(pb_memory_can_allocate(&monitor, PB_MEMORY_ARCHIVE,
                                 PB_MEMORY_ARCHIVE_LIMIT));
    CHECK(!pb_memory_can_allocate(&monitor, PB_MEMORY_ARCHIVE,
                                  PB_MEMORY_ARCHIVE_LIMIT + 1));

    monitor.snapshot.class_used[PB_MEMORY_ARCHIVE] = SIZE_MAX - 1;
    CHECK(!pb_memory_can_allocate(&monitor, PB_MEMORY_ARCHIVE, 2));
    return true;
}

static bool test_accounting_and_failure_state(void) {
    PBMemoryMonitor monitor;
    reset_monitor(&monitor);

    void *archive = pb_memory_alloc(&monitor, PB_MEMORY_ARCHIVE, PB_KIB(4));
    CHECK(archive != NULL);
    CHECK(monitor.snapshot.class_used[PB_MEMORY_ARCHIVE] == PB_KIB(4));
    CHECK(monitor.snapshot.class_peak[PB_MEMORY_ARCHIVE] == PB_KIB(4));
    CHECK(monitor.snapshot.allocation_failures == 0);

    pb_memory_free(&monitor, PB_MEMORY_ARCHIVE, archive, PB_KIB(4));
    CHECK(monitor.snapshot.class_used[PB_MEMORY_ARCHIVE] == 0);
    CHECK(monitor.snapshot.class_peak[PB_MEMORY_ARCHIVE] == PB_KIB(4));

    CHECK(pb_memory_alloc(&monitor, PB_MEMORY_ARCHIVE,
                          PB_MEMORY_ARCHIVE_LIMIT + 1) == NULL);
    CHECK(monitor.snapshot.allocation_failures == 1);
    CHECK(monitor.snapshot.pressure);

    CHECK(pb_memory_alloc(&monitor, (PBMemoryClass)-1, 1) == NULL);
    CHECK(monitor.snapshot.allocation_failures == 2);
    return true;
}

static bool test_reserves_and_sticky_pressure(void) {
    PBMemoryMonitor monitor;
    reset_monitor(&monitor);

    fake_application_free =
        (uint32_t)PB_MEMORY_APPLICATION_RESERVE + 1024U;
    fake_linear_free = (uint32_t)PB_MEMORY_LINEAR_RESERVE + 1024U;
    pb_memory_monitor_sample(&monitor, monitor.stack_anchor);
    CHECK(pb_memory_can_allocate(&monitor, PB_MEMORY_SCENE, 1024));
    CHECK(!pb_memory_can_allocate(&monitor, PB_MEMORY_SCENE, 1025));
    CHECK(pb_memory_can_allocate(&monitor, PB_MEMORY_LINEAR, 1024));
    CHECK(!pb_memory_can_allocate(&monitor, PB_MEMORY_LINEAR, 1025));
    CHECK(!monitor.snapshot.pressure);

    fake_application_free =
        (uint32_t)PB_MEMORY_APPLICATION_RESERVE - 1U;
    pb_memory_monitor_sample(&monitor, monitor.stack_anchor);
    CHECK(monitor.snapshot.pressure);

    fake_application_free = (uint32_t)PB_MIB(32);
    pb_memory_monitor_sample(&monitor, monitor.stack_anchor);
    CHECK(monitor.snapshot.pressure);
    return true;
}

static bool test_stack_and_folium_measurement(void) {
    PBMemoryMonitor monitor;
    reset_monitor(&monitor);

    pb_memory_monitor_sample(
        &monitor, monitor.stack_anchor - PB_MEMORY_STACK_LIMIT - 1U);
    CHECK(monitor.snapshot.peak_stack_used == PB_MEMORY_STACK_LIMIT + 1U);
    CHECK(monitor.snapshot.pressure);

    fake_application_free = 0;
    fake_linear_free = (uint32_t)PB_MIB(16);
    pb_memory_monitor_init(&monitor, (uintptr_t)PB_MIB(16));
    CHECK(!monitor.snapshot.application_measurement_available);
    CHECK(pb_memory_can_allocate(&monitor, PB_MEMORY_TRANSIENT, 1));
    CHECK(!monitor.snapshot.pressure);
    return true;
}

int main(void) {
    if (!test_class_limits() || !test_accounting_and_failure_state() ||
        !test_reserves_and_sticky_pressure() ||
        !test_stack_and_folium_measurement()) {
        return EXIT_FAILURE;
    }

    printf("M6 memory policy: %u checks passed\n", checks_run);
    return EXIT_SUCCESS;
}
