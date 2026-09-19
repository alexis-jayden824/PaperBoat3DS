#pragma once

#include <stddef.h>
#include <stdint.h>

typedef uint32_t u32;

#define MEMREGION_APPLICATION 0

u32 osGetMemRegionFree(int region);
u32 linearSpaceFree(void);
void *linearAlloc(size_t size);
void linearFree(void *memory);
