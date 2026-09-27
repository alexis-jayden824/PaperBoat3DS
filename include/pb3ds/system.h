#pragma once

#include <stdbool.h>

#include "pb3ds/bootstrap.h"

#ifdef __cplusplus
extern "C" {
#endif

bool pb_system_init(PBBootstrap *bootstrap);
void pb_system_shutdown(PBBootstrap *bootstrap);
bool pb_system_pump(void);
void pb_console_clear(void);
void pb_console_print(const char *text);

#ifdef __cplusplus
}
#endif
