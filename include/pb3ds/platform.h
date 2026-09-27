#pragma once

/*
 * PaperBoat-facing 3DS platform boundary (M3–M7).
 *
 * Game and compatibility code include this header instead of 3ds.h.
 * Audio is declared and deferred until M14. Extra OS threads are forbidden.
 * Full HID mapping is M8, SDMC I/O is M9, citro3d is M11.
 */

#include "pb3ds/assert.h"
#include "pb3ds/assets.h"
#include "pb3ds/audio.h"
#include "pb3ds/bootstrap.h"
#include "pb3ds/compat.h"
#include "pb3ds/diag.h"
#include "pb3ds/fs.h"
#include "pb3ds/gfx.h"
#include "pb3ds/hw.h"
#include "pb3ds/input.h"
#include "pb3ds/log.h"
#include "pb3ds/memory.h"
#include "pb3ds/paperboat.h"
#include "pb3ds/system.h"
#include "pb3ds/thread.h"
#include "pb3ds/time.h"
