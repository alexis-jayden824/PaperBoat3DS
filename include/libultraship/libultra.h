#pragma once

/*
 * devkitARM compatibility facade for libultraship's aggregate header.
 *
 * Use angle-bracket includes so this repository's narrowly scoped ABI
 * overlays (eeprom.h and interrupt.h) win without modifying pinned sources.
 */
#include <libultraship/libultra/abi.h>
#include <libultraship/libultra/controller.h>
#include <libultraship/libultra/convert.h>
#include <libultraship/libultra/eeprom.h>
#include <libultraship/libultra/exception.h>
#include <libultraship/libultra/gbi.h>
#include <libultraship/libultra/gs2dex.h>
#include <libultraship/libultra/gu.h>
#include <libultraship/libultra/os.h>
#include <libultraship/libultra/internal.h>
#include <libultraship/libultra/interrupt.h>
#include <libultraship/libultra/mbi.h>
#include <libultraship/libultra/message.h>
#include <libultraship/libultra/motor.h>
#include <libultraship/libultra/pfs.h>
#include <libultraship/libultra/pi.h>
#include <libultraship/libultra/printf.h>
#include <libultraship/libultra/r4300.h>
#include <libultraship/libultra/rcp.h>
#include <libultraship/libultra/rdp.h>
#include <libultraship/libultra/sptask.h>
#include <libultraship/libultra/thread.h>
#include <libultraship/libultra/time.h>
#include <libultraship/libultra/types.h>
#include <libultraship/libultra/vi.h>
