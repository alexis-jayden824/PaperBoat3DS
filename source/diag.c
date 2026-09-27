#include "pb3ds/assert.h"
#include "pb3ds/diag.h"
#include "pb3ds/log.h"
#include "pb3ds/time.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    char lines[PB_LOG_RING_CAPACITY][PB_LOG_RING_LINE];
    size_t count;
    size_t next;
} PBLogRing;

typedef struct {
    PBBreadcrumb items[PB_BREADCRUMB_CAPACITY];
    size_t count;
    size_t next;
} PBBreadcrumbRing;

static PBLogRing g_logs;
static PBBreadcrumbRing g_crumbs;
static bool g_assert_failed;
static char g_assert_expression[PB_BREADCRUMB_LINE];
static int g_assert_line;
static bool g_diag_ready;

static void ring_push_line(char (*lines)[PB_LOG_RING_LINE], size_t capacity,
                           size_t *count, size_t *next, const char *text) {
    size_t index = *next;

    snprintf(lines[index], PB_LOG_RING_LINE, "%s", text != NULL ? text : "");
    *next = (index + 1U) % capacity;
    if (*count < capacity) {
        (*count)++;
    }
}

void pb_diag_reset(void) {
    memset(&g_logs, 0, sizeof(g_logs));
    memset(&g_crumbs, 0, sizeof(g_crumbs));
    memset(g_assert_expression, 0, sizeof(g_assert_expression));
    g_assert_failed = false;
    g_assert_line = 0;
    g_diag_ready = true;
}

void pb_diag_init(void) {
    pb_diag_reset();
    pb_breadcrumb("diag init");
}

const char *pb_log_level_name(PBLogLevel level) {
    switch (level) {
        case PB_LOG_WARNING:
            return "warning";
        case PB_LOG_ERROR:
            return "error";
        case PB_LOG_INFO:
        default:
            return "info";
    }
}

void pb_log(PBLogLevel level, const char *component, const char *message) {
    char formatted[PB_LOG_RING_LINE];
    const char *safe_component = component != NULL ? component : "platform";
    const char *safe_message = message != NULL ? message : "";

    if (!g_diag_ready) {
        pb_diag_reset();
    }
    snprintf(formatted, sizeof(formatted), "[%s] %s: %s",
             pb_log_level_name(level), safe_component, safe_message);
    ring_push_line(g_logs.lines, PB_LOG_RING_CAPACITY, &g_logs.count,
                   &g_logs.next, formatted);
#ifdef __3DS__
    printf("%s\n", formatted);
#else
    fprintf(stderr, "%s\n", formatted);
#endif
}

size_t pb_log_count(void) {
    return g_logs.count;
}

const char *pb_log_line(size_t oldest_index) {
    size_t start;

    if (oldest_index >= g_logs.count) {
        return "";
    }
    start = g_logs.count < PB_LOG_RING_CAPACITY ? 0U : g_logs.next;
    return g_logs.lines[(start + oldest_index) % PB_LOG_RING_CAPACITY];
}

void pb_breadcrumb(const char *text) {
    size_t index;

    if (!g_diag_ready) {
        pb_diag_reset();
    }
    index = g_crumbs.next;
    memset(&g_crumbs.items[index], 0, sizeof(g_crumbs.items[index]));
    g_crumbs.items[index].time_ms = pb_time_ms();
    snprintf(g_crumbs.items[index].text, PB_BREADCRUMB_LINE, "%s",
             text != NULL ? text : "");
    g_crumbs.next = (index + 1U) % PB_BREADCRUMB_CAPACITY;
    if (g_crumbs.count < PB_BREADCRUMB_CAPACITY) {
        g_crumbs.count++;
    }
}

size_t pb_breadcrumb_count(void) {
    return g_crumbs.count;
}

const PBBreadcrumb *pb_breadcrumb_at(size_t oldest_index) {
    size_t start;

    if (oldest_index >= g_crumbs.count) {
        return NULL;
    }
    start = g_crumbs.count < PB_BREADCRUMB_CAPACITY ? 0U : g_crumbs.next;
    return &g_crumbs.items[(start + oldest_index) % PB_BREADCRUMB_CAPACITY];
}

void pb_assert_fail(const char *file, int line, const char *expression) {
    const char *safe_file = file != NULL ? file : "unknown";
    const char *safe_expr = expression != NULL ? expression : "false";
    char crumb[PB_BREADCRUMB_LINE];

    g_assert_failed = true;
    g_assert_line = line;
    snprintf(g_assert_expression, sizeof(g_assert_expression), "%s", safe_expr);
    snprintf(crumb, sizeof(crumb), "assert %s:%d", safe_file, line);
    pb_breadcrumb(crumb);
    pb_log(PB_LOG_ERROR, "assert", safe_expr);
}

bool pb_assert_failed(void) {
    return g_assert_failed;
}

const char *pb_assert_expression(void) {
    return g_assert_expression;
}

int pb_assert_line(void) {
    return g_assert_line;
}

const char *pb_memory_pressure_name(PBMemoryPressure pressure) {
    switch (pressure) {
        case PB_MEMORY_OK:
            return "ok";
        case PB_MEMORY_WARNING:
            return "warning";
        case PB_MEMORY_CRITICAL:
            return "critical";
        case PB_MEMORY_UNMEASURED:
        default:
            return "unmeasured";
    }
}

void pb_runtime_query(PBRuntimeStatus *status, uint32_t frames, bool gfx_ready) {
    if (status == NULL) {
        return;
    }
    memset(status, 0, sizeof(*status));
    status->hardware = pb_hw_model();
    status->new_3ds_features_enabled = pb_hw_new_3ds_features_enabled();
    pb_memory_query(&status->memory);
    status->assert_failed = g_assert_failed;
    status->gfx_ready = gfx_ready;
    status->frames = frames;
    status->breadcrumb_count = g_crumbs.count;
    status->log_count = g_logs.count;
}
