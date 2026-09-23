#pragma once

/*
 * Optional renderer/runtime diagnostics. Define these to 1 on the compiler
 * command line for a debug build. Release defaults stay silent.
 */
#ifndef PB3DS_DEBUG_VTX
#define PB3DS_DEBUG_VTX 0
#endif
#ifndef PB3DS_DEBUG_TRI
#define PB3DS_DEBUG_TRI 0
#endif
#ifndef PB3DS_DEBUG_MATRIX
#define PB3DS_DEBUG_MATRIX 0
#endif
#ifndef PB3DS_DEBUG_STATE
#define PB3DS_DEBUG_STATE 0
#endif
#ifndef PB3DS_DEBUG_HUGE_TRI
#define PB3DS_DEBUG_HUGE_TRI 0
#endif

#define PB3DS_DEBUG_LOG_LIMIT 8U
