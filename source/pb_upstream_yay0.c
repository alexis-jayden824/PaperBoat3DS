#if defined(__has_include)
#if __has_include("paperboat_config.h")
#include "paperboat_config.h"
#endif
#endif
#ifndef PB3DS_HAS_PAPERBOAT_SLICE
#define PB3DS_HAS_PAPERBOAT_SLICE 0
#endif

#if PB3DS_HAS_PAPERBOAT_SLICE
#include PB3DS_PAPERBOAT_SLICE_YAY0
#else
void decode_yay0(void *src, void *dst) {
    (void)src;
    (void)dst;
}
#endif
