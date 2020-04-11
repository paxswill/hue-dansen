#include "audio-tap.h"
#include "log.h"

#include <Accelerate/Accelerate.h>

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

static int counter;

void tap_callback(
		void * tap_data,
		float ** samples,
		int num_channels,
		int num_samples
);

int main(int argc, char ** argv)
{
	char * name = NULL;
	if (argc > 1) {
		name = argv[1];
	}
	AudioTap * tap = create_audio_tap(name, &tap_callback, NULL);
	assert(tap != NULL);
	assert(start_audio_tap(tap));

	while (counter < 200) {
		sleep(1);
	}
	printf("Stopping.\n");
	assert(stop_audio_tap(tap));
	free_audio_tap(tap);
}

void tap_callback(
		void * tap_data,
		float ** samples,
		int num_channels,
		int num_samples
)
{
	float mean, min, max, sum;
	vDSP_meanv(samples[0], 1, &mean, num_samples);
	vDSP_minv(samples[0], 1, &min, num_samples);
	vDSP_maxv(samples[0], 1, &max, num_samples);
	vDSP_sve(samples[0], 1, &sum, num_samples);
	fprintf(stderr, "[%6d] %6d samples, mean: %2.5f min: %2.5f max: %2.5f sum: %4.4f\n", counter, num_samples, mean, min, max, sum);
	if (counter % 37 == 0) {
	}
	counter++;
}
