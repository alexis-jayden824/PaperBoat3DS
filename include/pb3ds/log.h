#pragma once

#include <stdbool.h>
#include <stdio.h>

typedef enum {
    PB_LOG_INFO,
    PB_LOG_WARNING,
    PB_LOG_ERROR,
} PBLogLevel;

typedef struct {
    FILE *file;
    unsigned int entries_written;
    unsigned int entries_dropped;
} PBLog;

bool pb_log_init(PBLog *log);
bool pb_log_is_persistent(const PBLog *log);
void pb_log_write(PBLog *log, PBLogLevel level, const char *component,
                  const char *format, ...);
void pb_log_close(PBLog *log);
