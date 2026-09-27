#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PB_LOG_INFO = 0,
    PB_LOG_WARNING,
    PB_LOG_ERROR,
} PBLogLevel;

void pb_log(PBLogLevel level, const char *component, const char *message);
const char *pb_log_level_name(PBLogLevel level);

#ifdef __cplusplus
}
#endif
