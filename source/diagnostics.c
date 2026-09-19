#include "pb3ds/diagnostics.h"

#include <stdio.h>

PBMemorySnapshot pb_memory_snapshot(void) {
    PBMemorySnapshot snapshot = {
        .application_free = osGetMemRegionFree(MEMREGION_APPLICATION),
        .linear_free = linearSpaceFree(),
    };
    return snapshot;
}

void pb_show_fatal(PrintConsole *console, PBLog *log, const char *component,
                   Result result, const char *message) {
    pb_log_write(log, PB_LOG_ERROR, component, "result=%08lX %s",
                 (unsigned long)result, message);

    consoleSelect(console);
    printf("\x1b[2J\x1b[1;1H");
    printf("FATAL ERROR\n\n");
    printf("Component: %s\n", component);
    printf("Result:    %08lX\n\n", (unsigned long)result);
    printf("%s\n\n", message);
    printf("The application cannot continue.\n");
    printf("Press START to exit.\n");
}
