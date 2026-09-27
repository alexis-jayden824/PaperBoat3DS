#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint64_t pb_time_ms(void);
void pb_time_wait_vblank(void);

#ifdef __cplusplus
}
#endif
