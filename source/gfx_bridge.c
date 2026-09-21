#include "pb3ds/gfx_bridge.h"

#include <limits.h>
#include <string.h>

#define PB_GFX_SUPPORTED_OPTIONS                                             \
    (PB_GFX_OPTION_MASK(PB_GFX_OPT_ALPHA) |                                 \
     PB_GFX_OPTION_MASK(PB_GFX_OPT_TWO_CYCLE))
#define PB_GFX_OPTION_MASK(option) (UINT64_C(1) << (unsigned int)(option))

typedef struct {
    bool constant;
    float value[4];
    PBGfxTevSource source;
    PBGfxTevRgbOperand rgb_operand;
    bool original_previous;
} PBTevArgument;

typedef struct {
    PBGfxTevSource sources[3];
    PBGfxTevRgbOperand rgb_operands[3];
    PBGfxTevFunction function;
    float constant[4];
    bool has_constant;
} PBTevChannelStage;

typedef struct {
    PBTevChannelStage stages[3];
    size_t count;
    bool alpha;
} PBTevChannelProgram;

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

static bool channel_values_equal(const float left[4], const float right[4],
                                 bool alpha) {
    if (alpha) {
        return left[3] == right[3];
    }
    return left[0] == right[0] && left[1] == right[1] &&
           left[2] == right[2];
}

static bool argument_equal(const PBTevArgument *left,
                           const PBTevArgument *right, bool alpha) {
    if (left->constant != right->constant) {
        return false;
    }
    if (left->constant) {
        return channel_values_equal(left->value, right->value, alpha);
    }
    return left->source == right->source &&
           (alpha || left->rgb_operand == right->rgb_operand) &&
           left->original_previous == right->original_previous;
}

static bool argument_is_value(const PBTevArgument *argument, float value,
                              bool alpha) {
    if (!argument->constant) {
        return false;
    }
    if (alpha) {
        return argument->value[3] == value;
    }
    return argument->value[0] == value && argument->value[1] == value &&
           argument->value[2] == value;
}

static PBTevArgument constant_argument(float value) {
    PBTevArgument argument = { 0 };
    argument.constant = true;
    for (size_t component = 0; component < 4U; component++) {
        argument.value[component] = value;
    }
    return argument;
}

static bool decode_tev_argument(uint8_t operand, unsigned int cycle,
                                bool alpha, const float inputs[6][4],
                                PBTevArgument *argument) {
    if (argument == NULL) {
        return false;
    }
    memset(argument, 0, sizeof(*argument));
    argument->rgb_operand = PB_GFX_TEV_RGB_COLOR;
    if (operand >= PB_GFX_SHADER_INPUT_1 &&
        operand <= PB_GFX_SHADER_INPUT_6) {
        argument->constant = true;
        memcpy(argument->value, inputs[operand - PB_GFX_SHADER_INPUT_1],
               sizeof(argument->value));
        return true;
    }
    switch (operand) {
        case PB_GFX_SHADER_ZERO:
            *argument = constant_argument(0.0f);
            return true;
        case PB_GFX_SHADER_ONE:
            *argument = constant_argument(1.0f);
            return true;
        case PB_GFX_SHADER_SHADE:
            argument->source = PB_GFX_TEV_SHADE;
            return true;
        case PB_GFX_SHADER_TEXEL0:
        case PB_GFX_SHADER_TEXEL0_ALPHA:
            argument->source = cycle == 0U ? PB_GFX_TEV_TEXTURE0
                                           : PB_GFX_TEV_TEXTURE1;
            argument->rgb_operand =
                operand == PB_GFX_SHADER_TEXEL0_ALPHA
                    ? PB_GFX_TEV_RGB_ALPHA
                    : PB_GFX_TEV_RGB_COLOR;
            return true;
        case PB_GFX_SHADER_TEXEL1:
        case PB_GFX_SHADER_TEXEL1_ALPHA:
            argument->source = cycle == 0U ? PB_GFX_TEV_TEXTURE1
                                           : PB_GFX_TEV_TEXTURE0;
            argument->rgb_operand =
                operand == PB_GFX_SHADER_TEXEL1_ALPHA
                    ? PB_GFX_TEV_RGB_ALPHA
                    : PB_GFX_TEV_RGB_COLOR;
            return true;
        case PB_GFX_SHADER_COMBINED:
            if (cycle == 0U) {
                *argument = constant_argument(0.0f);
            } else {
                argument->source = PB_GFX_TEV_PREVIOUS;
                argument->original_previous = true;
            }
            return true;
        default:
            (void)alpha;
            return false;
    }
}

static void evaluate_binary(PBGfxTevFunction function,
                            const PBTevArgument *left,
                            const PBTevArgument *right,
                            PBTevArgument *result) {
    *result = constant_argument(0.0f);
    for (size_t component = 0; component < 4U; component++) {
        switch (function) {
            case PB_GFX_TEV_MODULATE:
                result->value[component] =
                    left->value[component] * right->value[component];
                break;
            case PB_GFX_TEV_ADD:
                result->value[component] =
                    left->value[component] + right->value[component];
                break;
            case PB_GFX_TEV_SUBTRACT:
                result->value[component] =
                    left->value[component] - right->value[component];
                break;
            default:
                break;
        }
    }
}

static size_t distinct_constants(const PBTevArgument *arguments[],
                                 size_t count, bool alpha) {
    const PBTevArgument *first = NULL;
    size_t distinct = 0U;
    for (size_t index = 0; index < count; index++) {
        if (!arguments[index]->constant) {
            continue;
        }
        if (first == NULL) {
            first = arguments[index];
            distinct = 1U;
        } else if (!channel_values_equal(first->value,
                                         arguments[index]->value, alpha)) {
            return 2U;
        }
    }
    return distinct;
}

static bool emit_channel_stage(PBTevChannelProgram *program,
                               PBGfxTevFunction function,
                               const PBTevArgument *arguments[],
                               size_t argument_count,
                               PBTevArgument *result) {
    if (program == NULL || result == NULL || argument_count == 0U ||
        argument_count > 3U || program->count >= 3U ||
        distinct_constants(arguments, argument_count, program->alpha) > 1U) {
        return false;
    }
    for (size_t index = 0; index < argument_count; index++) {
        if (!arguments[index]->constant &&
            arguments[index]->source == PB_GFX_TEV_PREVIOUS &&
            arguments[index]->original_previous && program->count != 0U) {
            return false;
        }
    }

    PBTevChannelStage *stage = &program->stages[program->count];
    memset(stage, 0, sizeof(*stage));
    stage->function = function;
    for (size_t index = 0; index < 3U; index++) {
        stage->sources[index] = PB_GFX_TEV_SHADE;
        stage->rgb_operands[index] = PB_GFX_TEV_RGB_COLOR;
    }
    for (size_t index = 0; index < argument_count; index++) {
        if (arguments[index]->constant) {
            stage->sources[index] = PB_GFX_TEV_CONSTANT;
            if (!stage->has_constant) {
                memcpy(stage->constant, arguments[index]->value,
                       sizeof(stage->constant));
                stage->has_constant = true;
            }
        } else {
            stage->sources[index] = arguments[index]->source;
            stage->rgb_operands[index] = arguments[index]->rgb_operand;
        }
    }
    program->count++;
    memset(result, 0, sizeof(*result));
    result->source = PB_GFX_TEV_PREVIOUS;
    result->rgb_operand = PB_GFX_TEV_RGB_COLOR;
    return true;
}

static bool emit_replace(PBTevChannelProgram *program,
                         const PBTevArgument *argument,
                         PBTevArgument *result) {
    const PBTevArgument *arguments[] = { argument };
    return emit_channel_stage(program, PB_GFX_TEV_REPLACE, arguments, 1U,
                              result);
}

static bool emit_binary(PBTevChannelProgram *program,
                        PBGfxTevFunction function,
                        const PBTevArgument *left,
                        const PBTevArgument *right,
                        PBTevArgument *result) {
    if (left->constant && right->constant) {
        evaluate_binary(function, left, right, result);
        return true;
    }
    const PBTevArgument *arguments[] = { left, right };
    return emit_channel_stage(program, function, arguments, 2U, result);
}

static bool emit_ternary(PBTevChannelProgram *program,
                         PBGfxTevFunction function,
                         const PBTevArgument *first,
                         const PBTevArgument *second,
                         const PBTevArgument *third,
                         PBTevArgument *result) {
    if (first->constant && second->constant && third->constant) {
        *result = constant_argument(0.0f);
        for (size_t component = 0; component < 4U; component++) {
            if (function == PB_GFX_TEV_INTERPOLATE) {
                result->value[component] =
                    first->value[component] * third->value[component] +
                    second->value[component] *
                        (1.0f - third->value[component]);
            } else {
                result->value[component] =
                    first->value[component] * second->value[component] +
                    third->value[component];
            }
        }
        return true;
    }
    const PBTevArgument *arguments[] = { first, second, third };
    return emit_channel_stage(program, function, arguments, 3U, result);
}

static bool finish_channel(PBTevChannelProgram *program,
                           const PBTevArgument *result) {
    if (!result->constant && result->source == PB_GFX_TEV_PREVIOUS &&
        !result->original_previous && program->count != 0U) {
        return true;
    }
    PBTevArgument previous;
    return emit_replace(program, result, &previous);
}

static bool compile_channel_formula(const uint8_t formula[4],
                                    unsigned int cycle, bool alpha,
                                    const float inputs[6][4],
                                    PBTevChannelProgram *program) {
    memset(program, 0, sizeof(*program));
    program->alpha = alpha;
    PBTevArgument a, b, c, d;
    if (!decode_tev_argument(formula[0], cycle, alpha, inputs, &a) ||
        !decode_tev_argument(formula[1], cycle, alpha, inputs, &b) ||
        !decode_tev_argument(formula[2], cycle, alpha, inputs, &c) ||
        !decode_tev_argument(formula[3], cycle, alpha, inputs, &d)) {
        return false;
    }

    if (argument_is_value(&c, 0.0f, alpha) ||
        argument_equal(&a, &b, alpha)) {
        return finish_channel(program, &d);
    }

    PBTevArgument result;
    if (argument_equal(&b, &d, alpha)) {
        if (distinct_constants(
                (const PBTevArgument *[]){ &a, &b, &c }, 3U, alpha) <= 1U &&
            emit_ternary(program, PB_GFX_TEV_INTERPOLATE, &a, &b, &c,
                         &result)) {
            return finish_channel(program, &result);
        }
        PBTevArgument difference;
        PBTevArgument product;
        if (!emit_binary(program, PB_GFX_TEV_SUBTRACT, &a, &b,
                         &difference) ||
            !emit_binary(program, PB_GFX_TEV_MODULATE, &difference, &c,
                         &product) ||
            !emit_binary(program, PB_GFX_TEV_ADD, &product, &b, &result)) {
            return false;
        }
        return finish_channel(program, &result);
    }

    const bool b_zero = argument_is_value(&b, 0.0f, alpha);
    const bool d_zero = argument_is_value(&d, 0.0f, alpha);
    if (b_zero && d_zero) {
        if (!emit_binary(program, PB_GFX_TEV_MODULATE, &a, &c, &result)) {
            return false;
        }
        return finish_channel(program, &result);
    }
    if (b_zero) {
        if (distinct_constants(
                (const PBTevArgument *[]){ &a, &c, &d }, 3U, alpha) <= 1U &&
            emit_ternary(program, PB_GFX_TEV_MULTIPLY_ADD, &a, &c, &d,
                         &result)) {
            return finish_channel(program, &result);
        }
        PBTevArgument product;
        if (!emit_binary(program, PB_GFX_TEV_MODULATE, &a, &c, &product) ||
            !emit_binary(program, PB_GFX_TEV_ADD, &product, &d, &result)) {
            return false;
        }
        return finish_channel(program, &result);
    }

    PBTevArgument difference;
    if (!emit_binary(program, PB_GFX_TEV_SUBTRACT, &a, &b, &difference)) {
        return false;
    }
    if (d_zero) {
        if (!emit_binary(program, PB_GFX_TEV_MODULATE, &difference, &c,
                         &result)) {
            return false;
        }
        return finish_channel(program, &result);
    }
    if (distinct_constants(
            (const PBTevArgument *[]){ &difference, &c, &d }, 3U, alpha) <=
            1U &&
        emit_ternary(program, PB_GFX_TEV_MULTIPLY_ADD, &difference, &c, &d,
                     &result)) {
        return finish_channel(program, &result);
    }
    PBTevArgument product;
    if (!emit_binary(program, PB_GFX_TEV_MODULATE, &difference, &c,
                     &product) ||
        !emit_binary(program, PB_GFX_TEV_ADD, &product, &d, &result)) {
        return false;
    }
    return finish_channel(program, &result);
}

static PBTevChannelStage pass_previous_stage(void) {
    PBTevChannelStage stage = { 0 };
    stage.function = PB_GFX_TEV_REPLACE;
    for (size_t index = 0; index < 3U; index++) {
        stage.sources[index] = PB_GFX_TEV_PREVIOUS;
        stage.rgb_operands[index] = PB_GFX_TEV_RGB_COLOR;
    }
    return stage;
}

static void merge_channel_stage(PBGfxTevStage *destination,
                                const PBTevChannelStage *source,
                                bool alpha) {
    if (alpha) {
        destination->alpha_function = source->function;
        memcpy(destination->alpha_sources, source->sources,
               sizeof(destination->alpha_sources));
        if (source->has_constant) {
            destination->constant[3] = source->constant[3];
        }
    } else {
        destination->rgb_function = source->function;
        memcpy(destination->rgb_sources, source->sources,
               sizeof(destination->rgb_sources));
        memcpy(destination->rgb_operands, source->rgb_operands,
               sizeof(destination->rgb_operands));
        if (source->has_constant) {
            memcpy(destination->constant, source->constant,
                   sizeof(float) * 3U);
        }
    }
}

uint64_t pb_gfx_shader_option(PBGfxShaderOption option) {
    if ((unsigned int)option > (unsigned int)PB_GFX_OPT_PRISM_SHADER) {
        return 0;
    }
    return UINT64_C(1) << (unsigned int)option;
}

bool pb_gfx_combiner_compile_tev(const PBGfxCombinerPlan *plan,
                                 const float inputs[6][4],
                                 PBGfxTevProgram *program) {
    if (plan == NULL || inputs == NULL || program == NULL ||
        !plan->supported) {
        return false;
    }
    memset(program, 0, sizeof(*program));
    size_t stage_offset = 0U;
    const unsigned int cycle_count = plan->two_cycle ? 2U : 1U;
    for (unsigned int cycle = 0; cycle < cycle_count; cycle++) {
        PBTevChannelProgram rgb;
        PBTevChannelProgram alpha;
        if (!compile_channel_formula(plan->operands[cycle][0], cycle, false,
                                     inputs, &rgb)) {
            memset(program, 0, sizeof(*program));
            return false;
        }
        if (plan->uses_alpha) {
            if (!compile_channel_formula(plan->operands[cycle][1], cycle,
                                         true, inputs, &alpha)) {
                memset(program, 0, sizeof(*program));
                return false;
            }
        } else {
            static const uint8_t opaque_formula[4] = {
                PB_GFX_SHADER_ZERO, PB_GFX_SHADER_ZERO,
                PB_GFX_SHADER_ZERO, PB_GFX_SHADER_ONE,
            };
            if (!compile_channel_formula(opaque_formula, cycle, true,
                                         inputs, &alpha)) {
                memset(program, 0, sizeof(*program));
                return false;
            }
        }

        const size_t cycle_stages = rgb.count > alpha.count
                                        ? rgb.count
                                        : alpha.count;
        if (cycle_stages == 0U ||
            stage_offset + cycle_stages > PB_GFX_TEV_STAGE_COUNT) {
            memset(program, 0, sizeof(*program));
            return false;
        }
        const PBTevChannelStage pass = pass_previous_stage();
        for (size_t index = 0; index < cycle_stages; index++) {
            PBGfxTevStage *destination =
                &program->stages[stage_offset + index];
            const PBTevChannelStage *rgb_stage =
                index < rgb.count ? &rgb.stages[index] : &pass;
            const PBTevChannelStage *alpha_stage =
                index < alpha.count ? &alpha.stages[index] : &pass;
            merge_channel_stage(destination, rgb_stage, false);
            merge_channel_stage(destination, alpha_stage, true);
        }
        stage_offset += cycle_stages;
    }
    program->stage_count = (uint8_t)stage_offset;
    return stage_offset != 0U;
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
    if (plan->two_cycle &&
        (plan->used_textures[0] || plan->used_textures[1])) {
        plan->used_textures[0] = true;
        plan->used_textures[1] = true;
    }

    const uint64_t option_bits = shader_id1 &
        ((UINT64_C(1) << ((unsigned int)PB_GFX_OPT_PRISM_SHADER + 1U)) - 1U);
    const uint64_t custom_shader_id =
        (shader_id1 >> PB_GFX_OPT_PRISM_SHADER) & UINT64_C(0xFFFF);
    if ((option_bits & ~PB_GFX_SUPPORTED_OPTIONS) != 0 ||
        custom_shader_id != 0) {
        plan->reject_reasons |= PB_GFX_REJECT_OPTION;
    }
    const PBGfxCombinerMode color_mode =
        formula_mode(plan->operands[0][0]);
    const PBGfxCombinerMode alpha_mode =
        formula_mode(plan->operands[0][1]);
    if (color_mode != PB_GFX_COMBINER_FALLBACK &&
        (!plan->uses_alpha || alpha_mode == color_mode)) {
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
    if (plan->supported) {
        float validation_inputs[6][4];
        for (size_t input = 0; input < 6U; input++) {
            for (size_t component = 0; component < 4U; component++) {
                validation_inputs[input][component] =
                    (float)(input * 4U + component + 1U) / 32.0f;
            }
        }
        PBGfxTevProgram validation_program;
        if (!pb_gfx_combiner_compile_tev(plan, validation_inputs,
                                         &validation_program)) {
            plan->reject_reasons |= PB_GFX_REJECT_FORMULA;
            plan->supported = false;
        }
    }
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
