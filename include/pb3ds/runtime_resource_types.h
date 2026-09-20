#pragma once
#include <stdint.h>

/* Native pointer-width packets, not the archive's packed 32-bit word pairs.
 * The upstream consumer test verifies this layout against pinned Gfx. */
typedef union {
    struct { uintptr_t w0, w1; } words;
    long long alignment;
} PBRuntimeGfx;
