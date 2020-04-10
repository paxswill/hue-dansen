#ifndef HUE_AUDIO_TAP_H
#define HUE_AUDIO_TAP_H

typedef enum {
	TAP_NO_ERROR = 0,
	TAP_ERROR_UNSPECIFIED = -1,
	TAP_DEVICE_NOT_FOUND,
} AudioTapErrorCode;

struct AudioTap_Internal;

typedef struct AudioTap_Internal AudioTap;

typedef void tap_callback(
);

AudioTap * create_audio_tap(const char * name);
void free_audio_tap(AudioTap * tap);
float get_sample_rate(AudioTap * tap);

void set_tap_data(AudioTap * tap, void * data);
void * get_tap_data(AudioTap * tap);

#endif
