#pragma once

/*
 * PaperBoat-facing 3DS platform boundary (master roadmap M3).
 *
 * Game and compatibility code should go through these headers rather than
 * scattering libctru calls. Audio is declared but deferred until M14.
 * Extra OS threads are not used: PaperBoat is stepped from the APT main
 * loop (M10) so suspend/resume stay on one ARM11 context.
 *
 * Graphics live in pb3ds/renderer.h and pb3ds/gfx_rendering_api_3ds.h.
 * Diagnostics on-device live in pb3ds/diagnostics.h (libctru).
 */

#include "pb3ds/compat.h"
#include "pb3ds/input.h"
#include "pb3ds/log.h"
#include "pb3ds/memory.h"
#include "pb3ds/o2r.h"
#include "pb3ds/renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PB_AUDIO_DEFERRED_M14 = 0,
} PBAudioStatus;

PBAudioStatus pb_platform_audio_status(void);

#ifdef __cplusplus
}
#endif
