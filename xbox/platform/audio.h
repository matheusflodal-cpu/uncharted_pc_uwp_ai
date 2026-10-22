// Adapted from Cosmo/gow3_xbox d0975c91; GPL-2.0-or-later.
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct UnchartedUwpAudioStream UnchartedUwpAudioStream;
int uncharted_uwp_audio_init(void);
int uncharted_uwp_audio_channels(void);
UnchartedUwpAudioStream* uncharted_uwp_audio_open(int channels,int is_float);
void uncharted_uwp_audio_close(UnchartedUwpAudioStream* stream);
int uncharted_uwp_audio_queued(UnchartedUwpAudioStream* stream);
int uncharted_uwp_audio_put(UnchartedUwpAudioStream* stream,const void* data,int bytes);
#ifdef __cplusplus
}
#endif
