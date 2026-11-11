/* ex3_plat — New 3DS platform ABI for the Exvius3DS recomp host (S1).
 *
 * Recovered client code will call into this surface instead of Android/JNI/GLES.
 * S1 ships the skeleton: init/shutdown, dual-screen clears, lifecycle, logging.
 */
#ifndef EX3_PLAT_H
#define EX3_PLAT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EX3_PLAT_OK = 0,
    EX3_PLAT_ERR_INIT = 1,
    EX3_PLAT_ERR_IO = 2,
    EX3_PLAT_ERR_STATE = 3
} Ex3PlatResult;

typedef enum {
    EX3_LIFECYCLE_ACTIVE = 0,
    EX3_LIFECYCLE_SUSPENDED,
    EX3_LIFECYCLE_SLEEPING,
    EX3_LIFECYCLE_EXITING
} Ex3PlatLifecycle;

typedef struct {
    uint32_t top_rgba;
    uint32_t bottom_rgba;
    uint32_t frame_count;
    bool screens_cleared;
} Ex3PlatFrameInfo;

typedef struct Ex3Plat Ex3Plat;

/* Packed RGBA8 colors used by the S1 diagnostic clear. */
enum {
    EX3_CLEAR_TOP_RGBA = 0x0B1E3AFF,    /* deep FFBE-ish navy */
    EX3_CLEAR_BOTTOM_RGBA = 0x163A6BFF  /* panel blue for touch chrome */
};

Ex3Plat *ex3_plat_create(void);
void ex3_plat_destroy(Ex3Plat *plat);

Ex3PlatResult ex3_plat_init(Ex3Plat *plat);
void ex3_plat_shutdown(Ex3Plat *plat);

/* New 3DS exclusive: request 804 MHz + L2 when hardware supports it. */
Ex3PlatResult ex3_plat_enable_new3ds_boost(Ex3Plat *plat);

bool ex3_plat_is_new_3ds(const Ex3Plat *plat);
bool ex3_plat_boost_enabled(const Ex3Plat *plat);
Ex3PlatLifecycle ex3_plat_lifecycle(const Ex3Plat *plat);
bool ex3_plat_should_run(const Ex3Plat *plat);

void ex3_plat_poll(Ex3Plat *plat);
void ex3_plat_begin_frame(Ex3Plat *plat);
void ex3_plat_clear_screens(Ex3Plat *plat, uint32_t top_rgba, uint32_t bottom_rgba);
void ex3_plat_end_frame(Ex3Plat *plat);

const Ex3PlatFrameInfo *ex3_plat_frame_info(const Ex3Plat *plat);

/* Append one line to the SDMC (or host) diagnostic log. */
Ex3PlatResult ex3_plat_log_line(Ex3Plat *plat, const char *line);

/* Host/test seam: inject lifecycle transitions without APT. */
void ex3_plat_debug_set_lifecycle(Ex3Plat *plat, Ex3PlatLifecycle life);
void ex3_plat_debug_request_exit(Ex3Plat *plat);

#ifdef __cplusplus
}
#endif

#endif /* EX3_PLAT_H */
