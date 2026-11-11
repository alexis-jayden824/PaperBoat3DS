#ifndef EX3_BOOTSTRAP_H
#define EX3_BOOTSTRAP_H

#include "ex3/plat.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    Ex3Plat *plat;
    uint32_t frames_drawn;
    bool ready;
    bool quit_requested;
} Ex3Bootstrap;

Ex3PlatResult ex3_bootstrap_init(Ex3Bootstrap *boot, Ex3Plat *plat);
void ex3_bootstrap_shutdown(Ex3Bootstrap *boot);

/* One dual-screen diagnostic frame. Returns false when the host should stop. */
bool ex3_bootstrap_frame(Ex3Bootstrap *boot);

#ifdef __cplusplus
}
#endif

#endif /* EX3_BOOTSTRAP_H */
