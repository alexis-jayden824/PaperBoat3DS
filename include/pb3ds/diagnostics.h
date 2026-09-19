#pragma once

#include <3ds.h>

#include "pb3ds/log.h"

typedef struct {
    u32 application_free;
    u32 linear_free;
} PBMemorySnapshot;

PBMemorySnapshot pb_memory_snapshot(void);
void pb_show_fatal(PrintConsole *console, PBLog *log, const char *component,
                   Result result, const char *message);
