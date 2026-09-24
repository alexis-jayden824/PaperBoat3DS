#include "pb3ds/fast3d_semantics.h"

#include <string.h>

enum {
    PB_CCMUX_COMBINED = 0,
    PB_CCMUX_TEXEL0 = 1,
    PB_CCMUX_TEXEL1 = 2,
    PB_CCMUX_PRIMITIVE = 3,
    PB_CCMUX_SHADE = 4,
    PB_CCMUX_ENVIRONMENT = 5,
    PB_CCMUX_CENTER = 6,
    PB_CCMUX_SCALE = 6,
    PB_CCMUX_COMBINED_ALPHA = 7,
    PB_CCMUX_NOISE = 7,
    PB_CCMUX_K4 = 7,
    PB_CCMUX_TEXEL0_ALPHA = 8,
    PB_CCMUX_TEXEL1_ALPHA = 9,
    PB_CCMUX_PRIMITIVE_ALPHA = 10,
    PB_CCMUX_SHADE_ALPHA = 11,
    PB_CCMUX_ENV_ALPHA = 12,
    PB_CCMUX_LOD_FRACTION = 13,
    PB_CCMUX_PRIM_LOD_FRAC = 14,
    PB_CCMUX_K5 = 15,
    PB_CCMUX_ONE = 6,
    PB_CCMUX_ZERO = 31,
    PB_CCMUX_KEY_CENTER = 20,
    PB_CCMUX_KEY_SCALE = 21,
    PB_CCMUX_CONVERT_K4 = 22,
    PB_CCMUX_CONVERT_K5 = 23,
};

enum {
    PB_ACMUX_COMBINED = 0,
    PB_ACMUX_LOD_FRACTION = 0,
    PB_ACMUX_TEXEL0 = 1,
    PB_ACMUX_TEXEL1 = 2,
    PB_ACMUX_PRIMITIVE = 3,
    PB_ACMUX_SHADE = 4,
    PB_ACMUX_ENVIRONMENT = 5,
    PB_ACMUX_ONE = 6,
    PB_ACMUX_PRIM_LOD_FRAC = 6,
    PB_ACMUX_ZERO = 7,
};

static bool has_option(uint64_t options, PBGfxShaderOption option) {
    return (options & pb_gfx_shader_option(option)) != 0;
}

static void decode_cycles(uint8_t cycles[2][2][4], uint32_t word0,
                          uint32_t word1, bool two_cycle) {
    cycles[0][0][0] = (uint8_t)((word0 >> 20U) & 0xFU);
    cycles[0][0][1] = (uint8_t)((word1 >> 28U) & 0xFU);
    cycles[0][0][2] = (uint8_t)((word0 >> 15U) & 0x1FU);
    cycles[0][0][3] = (uint8_t)((word1 >> 15U) & 0x7U);
    cycles[0][1][0] = (uint8_t)((word0 >> 12U) & 0x7U);
    cycles[0][1][1] = (uint8_t)((word1 >> 12U) & 0x7U);
    cycles[0][1][2] = (uint8_t)((word0 >> 9U) & 0x7U);
    cycles[0][1][3] = (uint8_t)((word1 >> 9U) & 0x7U);

    cycles[1][0][0] = (uint8_t)((word0 >> 5U) & 0xFU);
    cycles[1][0][1] = (uint8_t)((word1 >> 24U) & 0xFU);
    cycles[1][0][2] = (uint8_t)(word0 & 0x1FU);
    cycles[1][0][3] = (uint8_t)((word1 >> 6U) & 0x7U);
    cycles[1][1][0] = (uint8_t)((word1 >> 21U) & 0x7U);
    cycles[1][1][1] = (uint8_t)((word1 >> 3U) & 0x7U);
    cycles[1][1][2] = (uint8_t)((word1 >> 18U) & 0x7U);
    cycles[1][1][3] = (uint8_t)(word1 & 0x7U);

    const unsigned int cycle_count = two_cycle ? 2U : 1U;
    for (unsigned int cycle = 0; cycle < cycle_count; cycle++) {
        uint8_t *rgb = cycles[cycle][0];
        uint8_t *alpha = cycles[cycle][1];
        if (rgb[0] >= 8U) rgb[0] = PB_CCMUX_ZERO;
        if (rgb[1] >= 8U) rgb[1] = PB_CCMUX_ZERO;
        if (rgb[2] >= 16U) rgb[2] = PB_CCMUX_ZERO;
        if (rgb[3] == 7U) rgb[3] = PB_CCMUX_ZERO;
        if (rgb[0] == rgb[1] || rgb[2] == PB_CCMUX_ZERO) {
            rgb[0] = rgb[1] = rgb[2] = PB_CCMUX_ZERO;
        }
        if (alpha[0] == alpha[1] || alpha[2] == PB_ACMUX_ZERO) {
            alpha[0] = alpha[1] = alpha[2] = PB_ACMUX_ZERO;
        }

        if (cycle == 1U) {
            if (rgb[0] != PB_CCMUX_COMBINED &&
                rgb[1] != PB_CCMUX_COMBINED &&
                rgb[2] != PB_CCMUX_COMBINED &&
                rgb[3] != PB_CCMUX_COMBINED) {
                memset(cycles[0][0], PB_CCMUX_ZERO,
                       sizeof(cycles[0][0]));
            }
            if (rgb[2] != PB_CCMUX_COMBINED_ALPHA &&
                alpha[0] != PB_ACMUX_COMBINED &&
                alpha[1] != PB_ACMUX_COMBINED &&
                alpha[3] != PB_ACMUX_COMBINED) {
                memset(cycles[0][1], PB_ACMUX_ZERO,
                       sizeof(cycles[0][1]));
            }
        }
    }

    if (!two_cycle) {
        memset(cycles[1][0], PB_CCMUX_ZERO, sizeof(cycles[1][0]));
        memset(cycles[1][1], PB_ACMUX_ZERO, sizeof(cycles[1][1]));
        for (unsigned int slot = 0; slot < 4U; slot++) {
            if (cycles[0][0][slot] == PB_CCMUX_TEXEL1) {
                cycles[0][0][slot] = PB_CCMUX_TEXEL0;
            }
            if (cycles[0][0][slot] == PB_CCMUX_TEXEL1_ALPHA) {
                cycles[0][0][slot] = PB_CCMUX_TEXEL0_ALPHA;
            }
            if (cycles[0][1][slot] == PB_ACMUX_TEXEL1) {
                cycles[0][1][slot] = PB_ACMUX_TEXEL0;
            }
        }
    }
}

static uint8_t map_rgb_constant(uint8_t mux, uint8_t input_number[32],
                                uint8_t mapping[PB_FAST3D_COMBINER_INPUTS],
                                uint8_t *next_input) {
    if (input_number[mux] == 0U) {
        if (*next_input > PB_GFX_SHADER_INPUT_6) {
            *next_input = PB_GFX_SHADER_INPUT_6;
        }
        mapping[*next_input - 1U] = mux;
        input_number[mux] = (*next_input)++;
    }
    return input_number[mux];
}

static uint8_t map_alpha_constant(
    uint8_t mux, uint8_t input_number[16],
    uint8_t mapping[PB_FAST3D_COMBINER_INPUTS], uint8_t *next_input) {
    if (input_number[mux] == 0U) {
        if (*next_input > PB_GFX_SHADER_INPUT_6) {
            *next_input = PB_GFX_SHADER_INPUT_6;
        }
        mapping[*next_input - 1U] = mux;
        input_number[mux] = (*next_input)++;
    }
    return input_number[mux];
}

bool pb_fast3d_generate_combiner(PBFast3DCombiner *combiner,
                                 uint32_t combine_word0,
                                 uint32_t combine_word1,
                                 uint64_t shader_options) {
    if (combiner == NULL) {
        return false;
    }
    memset(combiner, 0, sizeof(*combiner));
    combiner->shader_id1 = shader_options;
    combiner->two_cycle =
        has_option(shader_options, PB_GFX_OPT_TWO_CYCLE);

    uint8_t cycles[2][2][4] = { 0 };
    decode_cycles(cycles, combine_word0, combine_word1,
                  combiner->two_cycle);

    uint8_t rgb_inputs[32] = { 0 };
    uint8_t next_rgb_input = PB_GFX_SHADER_INPUT_1;
    const unsigned int cycle_count = combiner->two_cycle ? 2U : 1U;
    for (unsigned int cycle = 0; cycle < cycle_count; cycle++) {
        for (unsigned int slot = 0; slot < 4U; slot++) {
            uint8_t mux = cycles[cycle][0][slot];
            if (slot == 1U && mux == PB_CCMUX_CENTER) {
                mux = PB_CCMUX_KEY_CENTER;
            } else if (slot == 1U && mux == PB_CCMUX_K4) {
                mux = PB_CCMUX_CONVERT_K4;
            } else if (slot == 2U && mux == PB_CCMUX_SCALE) {
                mux = PB_CCMUX_KEY_SCALE;
            } else if (slot == 2U && mux == PB_CCMUX_K5) {
                mux = PB_CCMUX_CONVERT_K5;
            }

            uint8_t value = PB_GFX_SHADER_ZERO;
            switch (mux) {
                case PB_CCMUX_ZERO:
                    value = PB_GFX_SHADER_ZERO;
                    break;
                case PB_CCMUX_ONE:
                    value = PB_GFX_SHADER_ONE;
                    break;
                case PB_CCMUX_TEXEL0:
                    value = PB_GFX_SHADER_TEXEL0;
                    combiner->used_textures[cycle == 0U ? 0U : 1U] = true;
                    break;
                case PB_CCMUX_TEXEL1:
                    value = PB_GFX_SHADER_TEXEL1;
                    combiner->used_textures[cycle == 0U ? 1U : 0U] = true;
                    break;
                case PB_CCMUX_TEXEL0_ALPHA:
                    value = PB_GFX_SHADER_TEXEL0_ALPHA;
                    combiner->used_textures[cycle == 0U ? 0U : 1U] = true;
                    break;
                case PB_CCMUX_TEXEL1_ALPHA:
                    value = PB_GFX_SHADER_TEXEL1_ALPHA;
                    combiner->used_textures[cycle == 0U ? 1U : 0U] = true;
                    break;
                case PB_CCMUX_NOISE:
                    value = PB_GFX_SHADER_NOISE;
                    break;
                case PB_CCMUX_LOD_FRACTION:
                    value = has_option(shader_options, PB_GFX_OPT_TEX_LOD)
                                ? PB_GFX_SHADER_LOD_FRACTION
                                : PB_GFX_SHADER_ONE;
                    break;
                case PB_CCMUX_SHADE:
                    value = PB_GFX_SHADER_SHADE;
                    combiner->uses_shade = true;
                    break;
                case PB_CCMUX_PRIMITIVE:
                case PB_CCMUX_PRIMITIVE_ALPHA:
                case PB_CCMUX_PRIM_LOD_FRAC:
                case PB_CCMUX_ENVIRONMENT:
                case PB_CCMUX_ENV_ALPHA:
                case PB_CCMUX_KEY_CENTER:
                case PB_CCMUX_KEY_SCALE:
                case PB_CCMUX_CONVERT_K4:
                case PB_CCMUX_CONVERT_K5:
                    value = map_rgb_constant(
                        mux, rgb_inputs, combiner->input_mapping[0],
                        &next_rgb_input);
                    break;
                case PB_CCMUX_COMBINED:
                    value = PB_GFX_SHADER_COMBINED;
                    break;
                default:
                    return false;
            }
            combiner->shader_id0 |=
                (uint64_t)value << (cycle * 32U + slot * 4U);
        }
    }

    uint8_t alpha_inputs[16] = { 0 };
    uint8_t next_alpha_input = PB_GFX_SHADER_INPUT_1;
    for (unsigned int cycle = 0; cycle < 2U; cycle++) {
        for (unsigned int slot = 0; slot < 4U; slot++) {
            const uint8_t mux = cycles[cycle][1][slot];
            uint8_t value = PB_GFX_SHADER_ZERO;
            switch (mux) {
                case PB_ACMUX_ZERO:
                    value = PB_GFX_SHADER_ZERO;
                    break;
                case PB_ACMUX_TEXEL0:
                    value = PB_GFX_SHADER_TEXEL0;
                    combiner->used_textures[cycle == 0U ? 0U : 1U] = true;
                    break;
                case PB_ACMUX_TEXEL1:
                    value = PB_GFX_SHADER_TEXEL1;
                    combiner->used_textures[cycle == 0U ? 1U : 0U] = true;
                    break;
                case PB_ACMUX_COMBINED:
                    if (slot != 2U) {
                        value = PB_GFX_SHADER_COMBINED;
                    } else {
                        value = has_option(shader_options,
                                           PB_GFX_OPT_TEX_LOD)
                                    ? PB_GFX_SHADER_LOD_FRACTION
                                    : PB_GFX_SHADER_ONE;
                    }
                    break;
                case PB_ACMUX_SHADE:
                    value = PB_GFX_SHADER_SHADE;
                    combiner->uses_shade = true;
                    break;
                case PB_ACMUX_ONE:
                    if (slot != 2U) {
                        value = PB_GFX_SHADER_ONE;
                    } else {
                        value = map_alpha_constant(
                            mux, alpha_inputs,
                            combiner->input_mapping[1], &next_alpha_input);
                    }
                    break;
                case PB_ACMUX_PRIMITIVE:
                case PB_ACMUX_ENVIRONMENT:
                    value = map_alpha_constant(
                        mux, alpha_inputs, combiner->input_mapping[1],
                        &next_alpha_input);
                    break;
                default:
                    return false;
            }
            combiner->shader_id0 |=
                (uint64_t)value <<
                (cycle * 32U + 16U + slot * 4U);
        }
    }

    if (has_option(shader_options, PB_GFX_OPT_MIP_LOD) &&
        combiner->used_textures[1]) {
        combiner->used_textures[0] = true;
        combiner->used_textures[1] = false;
    }
    return true;
}
