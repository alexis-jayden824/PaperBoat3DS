#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Detection only. New 3DS clock/L2/extra cores stay disabled until an
 * explicit later opt-in (not M4). Old 3DS remains the performance baseline.
 */
typedef enum {
    PB_HW_UNKNOWN = 0,
    PB_HW_OLD_3DS,
    PB_HW_NEW_3DS,
} PBHardwareModel;

PBHardwareModel pb_hw_model(void);
const char *pb_hw_model_name(PBHardwareModel model);
bool pb_hw_new_3ds_features_enabled(void);
#ifndef __3DS__
void pb_hw_host_set(PBHardwareModel model);
#endif

#ifdef __cplusplus
}
#endif
