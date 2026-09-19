#pragma once

/* Keep EEPROM declarations ABI-identical to libultraship's os.h on ARM. */
#include <libultraship/libultra/message.h>
#include <libultraship/libultra/types.h>
#include <ship/Api.h>

#define EEPROM_TYPE_4K 0x01
#define EEPROM_TYPE_16K 0x02

API_EXPORT s32 osEepromProbe(OSMesgQueue*);
API_EXPORT s32 osEepromLongRead(OSMesgQueue*, u8, u8*, int);
API_EXPORT s32 osEepromLongWrite(OSMesgQueue*, u8, u8*, int);
API_EXPORT void osWritebackDCache(void* p, int32_t x);
#pragma once

/* Keep EEPROM declarations ABI-identical to libultraship's os.h on ARM. */
#include <libultraship/libultra/message.h>
#include <libultraship/libultra/types.h>
#include <ship/Api.h>

#define EEPROM_TYPE_4K 0x01
#define EEPROM_TYPE_16K 0x02

API_EXPORT s32 osEepromProbe(OSMesgQueue*);
API_EXPORT s32 osEepromLongRead(OSMesgQueue*, u8, u8*, int);
API_EXPORT s32 osEepromLongWrite(OSMesgQueue*, u8, u8*, int);
API_EXPORT void osWritebackDCache(void* p, s32 x);
