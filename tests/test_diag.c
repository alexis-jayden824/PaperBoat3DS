#include "pb3ds/platform.h"
#include "pb3ds/version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks_run;

#define CHECK(expression)                                                      \
    do {                                                                       \
        checks_run++;                                                          \
        if (!(expression)) {                                                   \
            fprintf(stderr, "M4 diagnostics check failed at %s:%d: %s\n",      \
                    __FILE__, __LINE__, #expression);                          \
            return false;                                                      \
        }                                                                      \
    } while (0)

static bool test_memory_pressure_and_hw(void) {
    PBMemoryStatus status;
    PBRuntimeStatus runtime;

    CHECK(strcmp(PB3DS_VERSION, "0.6.0-m6") == 0);
    CHECK(strstr(PB3DS_ROADMAP_STAGE, "M6") != NULL);
    CHECK(PB_MEMORY_APPLICATION_RESERVE == PB_MIB(8));
    CHECK(PB_MEMORY_LINEAR_RESERVE == PB_MIB(4));
    CHECK(!pb_hw_new_3ds_features_enabled());
    CHECK(strcmp(pb_hw_model_name(PB_HW_OLD_3DS), "old3ds") == 0);
    CHECK(strcmp(pb_hw_model_name(PB_HW_NEW_3DS), "new3ds") == 0);

    pb_hw_host_set(PB_HW_NEW_3DS);
    CHECK(pb_hw_model() == PB_HW_NEW_3DS);
    CHECK(!pb_hw_new_3ds_features_enabled());

    pb_memory_host_set((uint32_t)PB_MIB(16), (uint32_t)PB_MIB(8),
                       (uint32_t)PB_MIB(32), true);
    pb_memory_query(&status);
    CHECK(status.measured);
    CHECK(status.pressure == PB_MEMORY_OK);
    CHECK(strcmp(pb_memory_pressure_name(status.pressure), "ok") == 0);

    pb_memory_host_set((uint32_t)PB_MIB(6), (uint32_t)PB_MIB(8),
                       (uint32_t)PB_MIB(32), true);
    pb_memory_query(&status);
    CHECK(status.pressure == PB_MEMORY_WARNING);

    pb_memory_host_set((uint32_t)PB_KIB(100), (uint32_t)PB_MIB(8),
                       (uint32_t)PB_MIB(32), true);
    pb_memory_query(&status);
    CHECK(status.pressure == PB_MEMORY_CRITICAL);

    pb_memory_host_set(0U, 0U, 0U, false);
    pb_memory_query(&status);
    CHECK(!status.measured);
    CHECK(status.pressure == PB_MEMORY_UNMEASURED);

    pb_runtime_query(&runtime, 12U, true);
    CHECK(runtime.hardware == PB_HW_NEW_3DS);
    CHECK(!runtime.new_3ds_features_enabled);
    CHECK(runtime.frames == 12U);
    CHECK(runtime.gfx_ready);
    return true;
}

static bool test_log_assert_and_breadcrumbs(void) {
    size_t index;
    const PBBreadcrumb *crumb;
    PBBootstrap bootstrap;

    pb_diag_reset();
    CHECK(pb_log_count() == 0U);
    CHECK(pb_breadcrumb_count() == 0U);
    CHECK(!pb_assert_failed());

    pb_log(PB_LOG_WARNING, "mem", "pressure");
    CHECK(pb_log_count() == 1U);
    CHECK(strstr(pb_log_line(0U), "pressure") != NULL);
    CHECK(strstr(pb_log_line(0U), "warning") != NULL);

    pb_breadcrumb("first");
    pb_breadcrumb("second");
    CHECK(pb_breadcrumb_count() == 2U);
    crumb = pb_breadcrumb_at(0U);
    CHECK(crumb != NULL);
    CHECK(strcmp(crumb->text, "first") == 0);
    crumb = pb_breadcrumb_at(1U);
    CHECK(crumb != NULL);
    CHECK(strcmp(crumb->text, "second") == 0);

    for (index = 0U; index < 20U; index++) {
        char text[16];
        snprintf(text, sizeof(text), "c%u", (unsigned)index);
        pb_breadcrumb(text);
    }
    CHECK(pb_breadcrumb_count() == PB_BREADCRUMB_CAPACITY);
    crumb = pb_breadcrumb_at(PB_BREADCRUMB_CAPACITY - 1U);
    CHECK(crumb != NULL);
    CHECK(strcmp(crumb->text, "c19") == 0);

    PB_ASSERT(1 == 1);
    CHECK(!pb_assert_failed());
    PB_ASSERT(1 == 0);
    CHECK(pb_assert_failed());
    CHECK(strcmp(pb_assert_expression(), "1 == 0") == 0);
    CHECK(pb_assert_line() > 0);
    CHECK(pb_log_count() >= 1U);

    pb_diag_reset();
    CHECK(!pb_assert_failed());
    pb_bootstrap_init(&bootstrap);
    CHECK(pb_system_init(&bootstrap));
    CHECK(pb_breadcrumb_count() >= 2U);
    pb_system_shutdown(&bootstrap);
    return true;
}

int main(void) {
    if (!test_memory_pressure_and_hw() || !test_log_assert_and_breadcrumbs()) {
        return EXIT_FAILURE;
    }
    printf("M4 diagnostics contract: %u checks passed\n", checks_run);
    return EXIT_SUCCESS;
}
