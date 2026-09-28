#pragma once

/*
 * libultraship declares OSIntMask as uint32_t here and as u32 in exception.h.
 * Those spellings are compatible on its desktop targets, but devkitARM's
 * newlib defines uint32_t as unsigned long while u32 is unsigned int. Keep the
 * N64-facing ABI consistent without modifying the pinned upstream checkout.
 */
#include <libultraship/libultra/types.h>

typedef u32 OSIntMask;
