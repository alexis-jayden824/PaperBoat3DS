#include "ex3/texture.h"

#include <stdio.h>
#include <string.h>

uint32_t ex3_texture_next_pot(uint32_t value)
{
    uint32_t pot;

    if (value <= 1U) {
        return 1U;
    }
    pot = 1U;
    while (pot < value) {
        pot <<= 1U;
        if (pot == 0U) {
            return 0U;
        }
    }
    return pot;
}

Ex3TextureInfo ex3_texture_plan(uint32_t width, uint32_t height)
{
    Ex3TextureInfo info;

    memset(&info, 0, sizeof(info));
    info.src_width = width;
    info.src_height = height;

    if (width == 0U || height == 0U) {
        snprintf(info.reject_reason, sizeof(info.reject_reason), "%s", "zero dimension");
        return info;
    }
    if (width > EX3_TEX_MAX_DIM || height > EX3_TEX_MAX_DIM) {
        snprintf(info.reject_reason, sizeof(info.reject_reason), "%s", "exceeds 1024 PICA limit");
        return info;
    }

    info.gpu_width = ex3_texture_next_pot(width);
    info.gpu_height = ex3_texture_next_pot(height);
    if (info.gpu_width == 0U || info.gpu_height == 0U ||
        info.gpu_width > EX3_TEX_MAX_DIM || info.gpu_height > EX3_TEX_MAX_DIM) {
        snprintf(info.reject_reason, sizeof(info.reject_reason), "%s", "POT pad exceeds limit");
        return info;
    }

    info.valid = true;
    return info;
}
