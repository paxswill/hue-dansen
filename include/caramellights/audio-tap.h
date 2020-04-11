#ifndef HUE_AUDIO_TAP_H
#define HUE_AUDIO_TAP_H

#include <stdbool.h>

struct AudioTap_Internal;

typedef struct AudioTap_Internal AudioTap;

typedef void AudioTapCallback(
		void * tap_data,
		float ** samples,
		int num_channels,
		int num_samples
);

AudioTap * create_audio_tap(const char * name, AudioTapCallback * callback,
                            void * callback_data);
bool start_audio_tap(AudioTap * tap);
bool stop_audio_tap(AudioTap * tap);
void free_audio_tap(AudioTap * tap);

void set_tap_data(AudioTap * tap, void * data);
void * get_tap_data(AudioTap * tap);

#endif
