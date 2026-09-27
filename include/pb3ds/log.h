#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PB_LOG_RING_CAPACITY 16U
#define PB_LOG_RING_LINE 96U

typedef enum {
    PB_LOG_INFO = 0,
    PB_LOG_WARNING,
    PB_LOG_ERROR,
} PBLogLevel;

void pb_log(PBLogLevel level, const char *component, const char *message);
const char *pb_log_level_name(PBLogLevel level);
size_t pb_log_count(void);
const char *pb_log_line(size_t oldest_index);

#ifdef __cplusplus
}
#endif
