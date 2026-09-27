#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PB_AUDIO_DEFERRED_M14 = 0,
} PBAudioStatus;

PBAudioStatus pb_audio_status(void);

#ifdef __cplusplus
}
#endif
