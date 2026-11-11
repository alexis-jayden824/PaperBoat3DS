/* Texture helpers for the recomp renderer shim (S2).
 * PICA limits: max 1024x1024; GPU dims padded to power-of-two.
 */
#ifndef EX3_TEXTURE_H
#define EX3_TEXTURE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    EX3_TEX_MAX_DIM = 1024
};

typedef struct {
    uint32_t src_width;
    uint32_t src_height;
    uint32_t gpu_width;
    uint32_t gpu_height;
    bool valid;
    char reject_reason[64];
} Ex3TextureInfo;

/* Compute GPU dimensions / validity without allocating pixel storage. */
Ex3TextureInfo ex3_texture_plan(uint32_t width, uint32_t height);

uint32_t ex3_texture_next_pot(uint32_t value);

#ifdef __cplusplus
}
#endif

#endif /* EX3_TEXTURE_H */
