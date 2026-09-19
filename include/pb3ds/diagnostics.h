#pragma once

#include <3ds.h>

#include "pb3ds/log.h"
#include "pb3ds/memory.h"

PBMemorySnapshot pb_memory_snapshot(void);
void pb_show_fatal(PrintConsole *console, PBLog *log, const char *component,
                   Result result, const char *message);
