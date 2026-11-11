#ifndef EX3_PLAT_COMMON_H
#define EX3_PLAT_COMMON_H

#include "ex3/plat.h"

#include <stdio.h>

struct Ex3Plat {
    bool initialized;
    bool is_new_3ds;
    bool boost_enabled;
    bool exit_requested;
    Ex3PlatLifecycle lifecycle;
    Ex3PlatFrameInfo frame;
    FILE *log_file;
    char log_path[256];
};

void ex3_plat_common_reset(Ex3Plat *plat);

#endif /* EX3_PLAT_COMMON_H */
