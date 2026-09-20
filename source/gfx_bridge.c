#include "pb3ds/gfx_bridge.h"

#include <limits.h>
#include <string.h>

#define PB_GFX_SUPPORTED_OPTIONS                                             \
    (PB_GFX_OPTION_MASK(PB_GFX_OPT_ALPHA) |                                 \
     PB_GFX_OPTION_MASK(PB_GFX_OPT_TEXEL0_CLAMP_S) |                        \
     PB_GFX_OPTION_MASK(PB_GFX_OPT_TEXEL0_CLAMP_T))
#define PB_GFX_OPTION_MASK(option) (UINT64_C(1) << (unsigned int)(option))

static bool operand_is_texture0(uint8_t operand) {
    return operand == PB_GFX_SHADER_TEXEL0 ||
           operand == PB_GFX_SHADER_TEXEL0_ALPHA;
}

static bool formula_is_replace(const uint8_t formula[4], uint8_t source) {
    return formula[2] == PB_GFX_SHADER_ZERO && formula[3] == source;
}

static bool formula_is_texture_shade(const uint8_t formula[4]) {
    if (formula[1] != PB_GFX_SHADER_ZERO ||
        formula[3] != PB_GFX_SHADER_ZERO) {
        return false;
    }
    return (operand_is_texture0(formula[0]) &&
            formula[2] == PB_GFX_SHADER_SHADE) ||
           (formula[0] == PB_GFX_SHADER_SHADE &&
            operand_is_texture0(formula[2]));
}

static PBGfxCombinerMode formula_mode(const uint8_t formula[4]) {
    if (formula_is_replace(formula, PB_GFX_SHADER_SHADE)) {
        return PB_GFX_COMBINER_SHADE;
    }
    if (formula_is_replace(formula, PB_GFX_SHADER_TEXEL0) ||
        formula_is_replace(formula, PB_GFX_SHADER_TEXEL0_ALPHA)) {
        return PB_GFX_COMBINER_TEXTURE0;
    }
    if (formula_is_texture_shade(formula)) {
        return PB_GFX_COMBINER_TEXTURE0_SHADE;
    }
    return PB_GFX_COMBINER_FALLBACK;
}

uint64_t pb_gfx_shader_option(PBGfxShaderOption option) {
    if ((unsigned int)option > (unsigned int)PB_GFX_OPT_PRISM_SHADER) {
        return 0;
    }
    return UINT64_C(1) << (unsigned int)option;
}

bool pb_gfx_combiner_decode(PBGfxCombinerPlan *plan, uint64_t shader_id0,
                            uint64_t shader_id1) {
    if (plan == NULL) {
        return false;
    }
    memset(plan, 0, sizeof(*plan));
    plan->shader_id0 = shader_id0;
    plan->shader_id1 = shader_id1;
    plan->mode = PB_GFX_COMBINER_FALLBACK;

    for (unsigned int cycle = 0; cycle < 2U; cycle++) {
        for (unsigned int channel = 0; channel < 2U; channel++) {
            for (unsigned int operand = 0; operand < 4U; operand++) {
                const unsigned int shift =
                    cycle * 32U + channel * 16U + operand * 4U;
                const uint8_t value =
                    (uint8_t)((shader_id0 >> shift) & UINT64_C(0xF));
                plan->operands[cycle][channel][operand] = value;
                if (value >= PB_GFX_SHADER_INPUT_1 &&
                    value <= PB_GFX_SHADER_INPUT_6 &&
                    value > plan->num_inputs) {
                    plan->num_inputs = value;
                }
                if (value == PB_GFX_SHADER_SHADE) {
                    plan->uses_shade = true;
                }
                if (value == PB_GFX_SHADER_TEXEL0 ||
                    value == PB_GFX_SHADER_TEXEL0_ALPHA) {
                    plan->used_textures[0] = true;
                }
                if (value == PB_GFX_SHADER_TEXEL1 ||
                    value == PB_GFX_SHADER_TEXEL1_ALPHA) {
                    plan->used_textures[1] = true;
                }
            }
        }
    }

    plan->uses_alpha =
        (shader_id1 & PB_GFX_OPTION_MASK(PB_GFX_OPT_ALPHA)) != 0;
    plan->two_cycle =
        (shader_id1 & PB_GFX_OPTION_MASK(PB_GFX_OPT_TWO_CYCLE)) != 0;

    const uint64_t option_bits = shader_id1 &
        ((UINT64_C(1) << ((unsigned int)PB_GFX_OPT_PRISM_SHADER + 1U)) - 1U);
    const uint64_t custom_shader_id =
        (shader_id1 >> PB_GFX_OPT_PRISM_SHADER) & UINT64_C(0xFFFF);
    if ((option_bits & ~PB_GFX_SUPPORTED_OPTIONS) != 0 ||
        custom_shader_id != 0) {
        plan->reject_reasons |= PB_GFX_REJECT_OPTION;
    }
    if (plan->two_cycle) {
        plan->reject_reasons |= PB_GFX_REJECT_TWO_CYCLE;
    }
    if (plan->used_textures[1]) {
        plan->reject_reasons |= PB_GFX_REJECT_TEXTURE1;
    }

    const PBGfxCombinerMode color_mode =
        formula_mode(plan->operands[0][0]);
    const PBGfxCombinerMode alpha_mode =
        formula_mode(plan->operands[0][1]);
    if (color_mode == PB_GFX_COMBINER_FALLBACK ||
        (plan->uses_alpha && alpha_mode != color_mode)) {
        plan->reject_reasons |= PB_GFX_REJECT_FORMULA;
    } else {
        plan->mode = color_mode;
    }

    plan->vertex_stride_floats = 5U;
    for (unsigned int tile = 0; tile < PB_GFX_TEXTURE_UNITS; tile++) {
        if (plan->used_textures[tile]) {
            plan->vertex_stride_floats += 2U;
        }
    }
    if (plan->uses_shade) {
        plan->vertex_stride_floats += plan->uses_alpha ? 4U : 3U;
    }
    plan->supported = plan->reject_reasons == PB_GFX_REJECT_NONE;
    if (!plan->supported) {
        plan->mode = PB_GFX_COMBINER_FALLBACK;
    }
    return plan->supported;
}

bool pb_gfx_validate_draw(const PBGfxCombinerPlan *plan,
                          const float *vertices, size_t float_count,
                          size_t triangle_count, size_t *stream_bytes) {
    if (stream_bytes != NULL) {
        *stream_bytes = 0;
    }
    if (plan == NULL || vertices == NULL || stream_bytes == NULL ||
        triangle_count == 0 ||
        triangle_count > PB_GFX_MAX_STREAM_TRIANGLES ||
        plan->vertex_stride_floats == 0) {
        return false;
    }
    if (triangle_count > SIZE_MAX / 3U ||
        triangle_count * 3U >
            SIZE_MAX / plan->vertex_stride_floats) {
        return false;
    }
    const size_t expected_floats =
        triangle_count * 3U * plan->vertex_stride_floats;
    if (float_count != expected_floats ||
        expected_floats > SIZE_MAX / sizeof(float)) {
        return false;
    }
    const size_t bytes = expected_floats * sizeof(float);
    if (bytes > PB_GFX_MAX_STREAM_BYTES) {
        return false;
    }
    *stream_bytes = bytes;
    return true;
}

void pb_gfx_bridge_init(PBGfxBridge *bridge) {
    if (bridge == NULL) {
        return;
    }
    memset(bridge, 0, sizeof(*bridge));
    bridge->next_texture_id = 1U;
    bridge->active = true;
}

void pb_gfx_bridge_set_active(PBGfxBridge *bridge, bool active) {
    if (bridge == NULL) {
        return;
    }
    if (!active && bridge->frame_open) {
        bridge->frame_open = false;
        bridge->stats.frame_failures++;
    }
    bridge->active = active;
}

bool pb_gfx_bridge_start_frame(PBGfxBridge *bridge) {
    if (bridge == NULL || !bridge->active || bridge->frame_open) {
        if (bridge != NULL) {
            bridge->stats.rejected_commands++;
        }
        return false;
    }
    bridge->frame_open = true;
    bridge->stats.frames_started++;
    return true;
}

bool pb_gfx_bridge_end_frame(PBGfxBridge *bridge, bool presented) {
    if (bridge == NULL || !bridge->frame_open) {
        if (bridge != NULL) {
            bridge->stats.rejected_commands++;
        }
        return false;
    }
    bridge->frame_open = false;
    if (presented) {
        bridge->stats.frames_presented++;
    } else {
        bridge->stats.frame_failures++;
    }
    return presented;
}

static bool set_rectangle(PBViewport *destination, bool *valid, int x, int y,
                          int width, int height) {
    if (destination == NULL || valid == NULL || x < 0 || y < 0 || width <= 0 ||
        height <= 0 || x > (int)PB_RENDER_TOP_WIDTH - width ||
        y > (int)PB_RENDER_TOP_HEIGHT - height) {
        return false;
    }
    destination->x = (uint16_t)x;
    destination->y = (uint16_t)y;
    destination->width = (uint16_t)width;
    destination->height = (uint16_t)height;
    *valid = true;
    return true;
}

bool pb_gfx_bridge_set_viewport(PBGfxBridge *bridge, int x, int y,
                                int width, int height) {
    if (bridge == NULL ||
        !set_rectangle(&bridge->viewport, &bridge->viewport_valid, x, y,
                       width, height)) {
        if (bridge != NULL) {
            bridge->stats.rejected_commands++;
        }
        return false;
    }
    return true;
}

bool pb_gfx_bridge_set_scissor(PBGfxBridge *bridge, int x, int y,
                               int width, int height) {
    if (bridge == NULL ||
        !set_rectangle(&bridge->scissor, &bridge->scissor_valid, x, y,
                       width, height)) {
        if (bridge != NULL) {
            bridge->stats.rejected_commands++;
        }
        return false;
    }
    return true;
}

static PBGfxTextureRecord *find_texture_mutable(PBGfxBridge *bridge,
                                                uint32_t texture_id) {
    if (bridge == NULL || texture_id == 0) {
        return NULL;
    }
    for (size_t index = 0; index < PB_GFX_MAX_TEXTURES; index++) {
        if (bridge->textures[index].allocated &&
            bridge->textures[index].id == texture_id) {
            return &bridge->textures[index];
        }
    }
    return NULL;
}

const PBGfxTextureRecord *pb_gfx_bridge_find_texture(
    const PBGfxBridge *bridge, uint32_t texture_id) {
    return find_texture_mutable((PBGfxBridge *)bridge, texture_id);
}

uint32_t pb_gfx_bridge_new_texture(PBGfxBridge *bridge) {
    if (bridge == NULL) {
        return 0;
    }
    for (size_t index = 0; index < PB_GFX_MAX_TEXTURES; index++) {
        if (!bridge->textures[index].allocated) {
            PBGfxTextureRecord *record = &bridge->textures[index];
            memset(record, 0, sizeof(*record));
            record->allocated = true;
            record->id = bridge->next_texture_id++;
            if (bridge->next_texture_id == 0) {
                bridge->next_texture_id = 1U;
            }
            record->last_use = ++bridge->use_clock;
            bridge->stats.textures_live++;
            return record->id;
        }
    }
    bridge->stats.rejected_commands++;
    return 0;
}

bool pb_gfx_bridge_select_texture(PBGfxBridge *bridge, int tile,
                                  uint32_t texture_id) {
    PBGfxTextureRecord *record = find_texture_mutable(bridge, texture_id);
    if (bridge == NULL || tile < 0 || tile >= (int)PB_GFX_TEXTURE_UNITS ||
        record == NULL) {
        if (bridge != NULL) {
            bridge->stats.rejected_commands++;
        }
        return false;
    }
    bridge->selected_textures[tile] = texture_id;
    record->last_use = ++bridge->use_clock;
    return true;
}

bool pb_gfx_bridge_upload_texture(PBGfxBridge *bridge, uint32_t texture_id,
                                  uint32_t width, uint32_t height) {
    PBGfxTextureRecord *record = find_texture_mutable(bridge, texture_id);
    PBTextureLayout layout;
    if (bridge == NULL || record == NULL || width > UINT16_MAX ||
        height > UINT16_MAX ||
        !pb_renderer_texture_layout(&layout, (uint16_t)width,
                                    (uint16_t)height, PB_TEXTURE_RGBA8)) {
        if (bridge != NULL) {
            bridge->stats.rejected_commands++;
        }
        return false;
    }
    if (record->uploaded) {
        bridge->stats.texture_bytes -= record->bytes;
    }
    record->width = layout.width;
    record->height = layout.height;
    record->bytes = layout.bytes;
    record->uploaded = true;
    record->last_use = ++bridge->use_clock;
    bridge->stats.texture_bytes += layout.bytes;
    return true;
}

bool pb_gfx_bridge_delete_texture(PBGfxBridge *bridge, uint32_t texture_id) {
    PBGfxTextureRecord *record = find_texture_mutable(bridge, texture_id);
    if (bridge == NULL || record == NULL) {
        if (bridge != NULL) {
            bridge->stats.rejected_commands++;
        }
        return false;
    }
    if (record->uploaded) {
        bridge->stats.texture_bytes -= record->bytes;
    }
    for (unsigned int tile = 0; tile < PB_GFX_TEXTURE_UNITS; tile++) {
        if (bridge->selected_textures[tile] == texture_id) {
            bridge->selected_textures[tile] = 0;
        }
    }
    memset(record, 0, sizeof(*record));
    bridge->stats.textures_live--;
    return true;
}

bool pb_gfx_bridge_record_shader(PBGfxBridge *bridge,
                                 const PBGfxCombinerPlan *plan) {
    if (bridge == NULL || plan == NULL ||
        bridge->stats.shaders_live >= PB_GFX_MAX_SHADERS) {
        if (bridge != NULL) {
            bridge->stats.rejected_commands++;
        }
        return false;
    }
    bridge->stats.shaders_live++;
    if (!plan->supported) {
        bridge->stats.unsupported_shaders++;
    }
    return true;
}

bool pb_gfx_bridge_record_draw(PBGfxBridge *bridge,
                               const PBGfxCombinerPlan *plan,
                               const float *vertices, size_t float_count,
                               size_t triangle_count) {
    size_t stream_bytes = 0;
    if (bridge == NULL || !bridge->frame_open || plan == NULL ||
        !plan->supported ||
        !pb_gfx_validate_draw(plan, vertices, float_count, triangle_count,
                              &stream_bytes)) {
        if (bridge != NULL) {
            bridge->stats.rejected_commands++;
        }
        return false;
    }
    for (unsigned int tile = 0; tile < PB_GFX_TEXTURE_UNITS; tile++) {
        if (plan->used_textures[tile]) {
            const PBGfxTextureRecord *record = pb_gfx_bridge_find_texture(
                bridge, bridge->selected_textures[tile]);
            if (record == NULL || !record->uploaded) {
                bridge->stats.rejected_commands++;
                return false;
            }
        }
    }
    bridge->stats.draw_calls++;
    bridge->stats.triangles += triangle_count;
    bridge->stats.vertices += triangle_count * 3U;
    bridge->stats.streamed_bytes += stream_bytes;
    if (stream_bytes > bridge->stats.stream_peak_bytes) {
        bridge->stats.stream_peak_bytes = stream_bytes;
    }
    return true;
}

void pb_gfx_bridge_clear_shaders(PBGfxBridge *bridge) {
    if (bridge != NULL) {
        bridge->stats.shaders_live = 0;
        bridge->stats.unsupported_shaders = 0;
    }
}

const PBGfxBridgeStats *pb_gfx_bridge_stats(const PBGfxBridge *bridge) {
    return bridge != NULL ? &bridge->stats : NULL;
}
