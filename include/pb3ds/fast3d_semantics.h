#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "pb3ds/gfx_bridge.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PB_FAST3D_COMBINER_INPUTS 6U

/* Raw RDP mux values retained in input_mapping, matching libultraship. */
typedef enum {
    PB_FAST3D_INPUT_PRIMITIVE = 3,
    PB_FAST3D_INPUT_ENVIRONMENT = 5,
    PB_FAST3D_INPUT_ALPHA_PRIM_LOD_FRACTION = 6,
    PB_FAST3D_INPUT_PRIMITIVE_ALPHA = 10,
    PB_FAST3D_INPUT_ENVIRONMENT_ALPHA = 12,
    PB_FAST3D_INPUT_PRIM_LOD_FRACTION = 14,
    PB_FAST3D_INPUT_KEY_CENTER = 20,
    PB_FAST3D_INPUT_KEY_SCALE = 21,
    PB_FAST3D_INPUT_CONVERT_K4 = 22,
    PB_FAST3D_INPUT_CONVERT_K5 = 23,
} PBFast3DInput;

/*
 * Canonical result of Fast3D's color-combiner key generation.  The shader
 * IDs and input mapping intentionally mirror the pinned libultraship
 * Interpreter::GenerateCC contract.  Keeping this representation outside the
 * compatibility display-list walker lets the PICA backend consume the same
 * semantics as the eventual upstream Fast3D interpreter.
 */
typedef struct {
    uint64_t shader_id0;
    uint64_t shader_id1;
    uint8_t input_mapping[2][PB_FAST3D_COMBINER_INPUTS];
    bool used_textures[PB_GFX_TEXTURE_UNITS];
    bool uses_shade;
    bool two_cycle;
} PBFast3DCombiner;

bool pb_fast3d_generate_combiner(PBFast3DCombiner *combiner,
                                 uint32_t combine_word0,
                                 uint32_t combine_word1,
                                 uint64_t shader_options);

#ifdef __cplusplus
}
#endif
