#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Extra OS threads are not used. PaperBoat is stepped from the APT main
 * loop so suspend/resume stay on one ARM11 context (M10).
 */
bool pb_thread_extra_workers_allowed(void);

#ifdef __cplusplus
}
#endif
