/*********
 * BIG NOTE for anyone trying to run this in a debugger (say, lldb) on recent
 * (>= Mojave) versions of macOS. It seems like lldb is unable to interface
 * with the trust/permissions database, so you will never be able to run this
 * in the debugger. It *does* work when it's running by itself though (after
 * access to the microphone is granted though).
 *********/
#include "audio-tap.h"
#include "log.h"

#include <limits.h>
#include <stdbool.h>
#include <AudioToolbox/AudioToolbox.h>
#include <Accelerate/Accelerate.h>
#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>

// just define the sample rate to 44.1kHz
const Float64 SAMPLE_RATE = 44100;
/* Define the number of buffers to cycle through and how many samples should be
 * in each buffer
 */
const int NUM_BUFFERS = 5;
const int SAMPLES_PER_BUFFER = 512;

bool create_audio_queue(AudioTap * tap);
AudioDeviceID find_input_device(const char * name);
AudioDeviceID get_default_input_device();
// Matching the AudioQueueInputCallback type signature
void audio_queue_callback(void *, AudioQueueRef, AudioQueueBufferRef,
                          const AudioTimeStamp *, UInt32,
                          const AudioStreamPacketDescription *);

struct AudioTap_Internal {
	AudioQueueRef queue;
	AudioDeviceID input_device;
	// Stash a copy of the stream format on the tap to make access easier
	AudioStreamBasicDescription format;
	AudioTapCallback * callback;
	void * data;
};

AudioTap * create_audio_tap(const char * name, AudioTapCallback * callback,
                            void * callback_data)
{
	// Get the AudioDeviceID to use
	AudioDeviceID device_id = kAudioObjectUnknown;
	if (name == NULL) {
		device_id = get_default_input_device();
	} else {
		device_id = find_input_device(name);
	}
	if (device_id == kAudioObjectUnknown) {
		LOG_ERROR("Unable to find input device.");
		return NULL;
	}
	AudioTap * tap = malloc(sizeof(AudioTap));
	if (tap == NULL) {
		LOG_ERROR("Unable to allocate memory for AudioTap.");
		return NULL;
	}
	tap->input_device = device_id;
	tap->callback = callback;
	tap->data = callback_data;
	if (!create_audio_queue(tap)) {
		LOG_ERROR("Unable to create AudioQueue.");
		free(tap);
		return NULL;
	}
	return tap;
}

bool start_audio_tap(AudioTap * tap)
{
	OSStatus err;
	if (tap == NULL) {
		LOG_ERROR("Null AudioTap given to start.");
		return false;
	}
	if (tap->queue == NULL) {
		LOG_ERROR("AudioQueue not created for AudioTap.");
		return false;
	}
	// Kick the queue off.
	// NOTE: no need to prime recording queues
	// The NULL means start processing immediately
	err = AudioQueueStart(tap->queue, NULL);
	if (err) {
		LOG_ERROR("Unable to start the AudioQueue.");
		return false;
	}
	return true;
}

bool stop_audio_tap(AudioTap * tap)
{
	OSStatus err;
	if (tap == NULL) {
		LOG_ERROR("Null AudioTap given to stop.");
		return false;
	}
	if (tap->queue == NULL) {
		LOG_ERROR("AudioQueue not created for AudioTap.");
		return false;
	}
	err = AudioQueueFlush(tap->queue);
	if (err) {
		LOG_ERROR("Unable to flush AudioQueue.");
		return false;
	}
	err = AudioQueueStop(tap->queue, true);
	if (err) {
		LOG_ERROR("Unable to stop AudioQueue.");
		return false;
	}
	return true;
}

void free_audio_tap(AudioTap * tap)
{
	if (tap->queue != NULL) {
		AudioQueueDispose(tap->queue, true);
	}
	free(tap);
}

bool create_audio_queue(AudioTap * tap)
{
	/* This function sources heavily from Apple Tech Notes TN2091 and
	 * TN2223 (for API updates).
	 */
	OSStatus err;
	UInt32 data_size;
	// First define which format we want the audio to come out as.
	AudioStreamBasicDescription format = {0};
	format.mFormatID = kAudioFormatLinearPCM;
	format.mFormatFlags = kAudioFormatFlagsNativeFloatPacked;
	format.mChannelsPerFrame = 2;
	format.mBitsPerChannel = sizeof(Float32) * CHAR_BIT;
	format.mBytesPerFrame = format.mChannelsPerFrame * sizeof(Float32);
	// For PCM, a packet contains a single frame
	format.mFramesPerPacket = 1;
	// For non-compressed formats, this is simple
	format.mBytesPerPacket = format.mBytesPerFrame * format.mFramesPerPacket;
	format.mSampleRate = SAMPLE_RATE;
	// Now we create the AudioInputQueue
	err = AudioQueueNewInput(
			&format,
			&audio_queue_callback,
			// pass the AudioTap structure in as the callback user info
			tap,
			// Use a run loop that is internal to the audio queue
			NULL,
			// Default run loop modes
			kCFRunLoopCommonModes,
			// Reserved, must be 0
			0,
			&tap->queue
	);
	if (err) {
		LOG_ERROR("Unable to create audio queue.");
		return false;
	}
	// Keep the stream format handy for quick reference
	tap->format = format;
	// Now set the queue to record from the specified device.
	/* We need get the UID for the device as a CFString, then set that property
	 * on the AudioQueue.
	 */
	AudioObjectPropertyAddress uid_property = {0};
	uid_property.mScope = kAudioDevicePropertyScopeInput;
	uid_property.mElement = kAudioObjectPropertyElementMaster;
	uid_property.mSelector = kAudioDevicePropertyDeviceUID;
	data_size = sizeof(CFStringRef);
	CFStringRef device_uid = NULL;
	err = AudioObjectGetPropertyData(
			tap->input_device,
			&uid_property,
			0,
			NULL,
			&data_size,
			&device_uid
	);
	if (err) {
		LOG_ERROR("Unable to get UID for input device.");
		AudioQueueDispose(tap->queue, true);
		tap->queue = NULL;
		return false;
	}
	err = AudioQueueSetProperty(
			tap->queue,
			kAudioQueueProperty_CurrentDevice,
			&device_uid,
			data_size
	);
	if (err) {
		LOG_ERROR("Unable to set UID of input device on AudioQueue.");
		AudioQueueDispose(tap->queue, true);
		tap->queue = NULL;
		CFRelease(device_uid);
		return false;
	}
	CFRelease(device_uid);
	// Now allocate and enqueue the buffers
	AudioQueueBufferRef buffer = NULL;
	for (int i = 0; i < NUM_BUFFERS; i++) {
		err = AudioQueueAllocateBuffer(
				tap->queue, 
				SAMPLES_PER_BUFFER * format.mBytesPerPacket,
				&buffer
		);
		if (err) {
			LOG_ERROR("Unable to allocate queue buffer.");
			AudioQueueDispose(tap->queue, true);
			tap->queue = NULL;
			return false;
		}
		// The last two arguments are not needed for input queues
		err = AudioQueueEnqueueBuffer(tap->queue, buffer, 0, NULL);
		if (err) {
			LOG_ERROR("Unable to enqueue buffer.");
			AudioQueueDispose(tap->queue, true);
			tap->queue = NULL;
			return false;
		}
		// From now on, the AudioQueue will handle free-ing the buffers
		buffer = NULL;
	}
	return true;
}

AudioDeviceID find_input_device(const char * name)
{
	/* Much of this function is adapted from this StackOverflow answer:
	 * https://stackoverflow.com/a/4577271
	 */
	OSStatus err;
	/* The size of data being passed in and out of the AudioUnit property
	 * accessors is a common parameter, so we're using a single variable for
	 * all of them.
	 */
	UInt32 data_size;
	/* Like data_size, the property address struct is used a bunch, so I'm
	 * sharing it across all of the function calls.
	 */
	AudioObjectPropertyAddress property_addr = {0};
	property_addr.mScope = kAudioObjectPropertyScopeGlobal;
	property_addr.mElement = kAudioObjectPropertyElementMaster;
	property_addr.mSelector = kAudioHardwarePropertyDevices;
	err = AudioObjectGetPropertyDataSize(
			kAudioObjectSystemObject,
			&property_addr,
			// no input qualifiers
			0,
			NULL,
			&data_size
	);
	if (err != kAudioHardwareNoError) {
		LOG_ERROR("Unable to get the size of the audio devices array.");
		return kAudioObjectUnknown;
	}
	AudioDeviceID * device_ids = malloc(data_size);
	if (device_ids == NULL) {
		LOG_ERROR("Unable to allocate memory for device ID array.");
		return kAudioObjectUnknown;
	}
	err = AudioObjectGetPropertyData(
			kAudioObjectSystemObject,
			&property_addr,
			// no input qualifiers
			0,
			NULL,
			&data_size,
			device_ids
	);
	if (err != kAudioHardwareNoError) {
		LOG_ERROR("Unable to get list of audio devices.");
		free(device_ids);
		return kAudioObjectUnknown;
	}
	/* Convert the given string to a CFString for comparison to the device
	 * names later on.
	 */
	CFStringRef cf_name = CFStringCreateWithCString(
			kCFAllocatorDefault,
			name,
			/* Just assume UTF8 for everything. If the need arises in the
			 * future, I'll futz around with the internationalization API
			 * to get it working right.
			 */
			kCFStringEncodingUTF8
	);
	// Iterate through all devices to find one that matches.
	UInt32 num_device_ids = data_size / sizeof(AudioDeviceID);
	// The property accesses in the loop are all input-scoped
	property_addr.mScope = kAudioDevicePropertyScopeInput;
	bool device_found = false;
	UInt32 i;
	for (i = 0; i < num_device_ids && !device_found; i++) {
		// Get the device name for better logging
		CFStringRef device_name = NULL;
		data_size = sizeof(device_name);
		property_addr.mSelector = kAudioDevicePropertyDeviceNameCFString;
		err = AudioObjectGetPropertyData(
				device_ids[i],
				&property_addr,
				0,
				NULL,
				&data_size,
				&device_name
		);
		if (err != kAudioHardwareNoError) {
			LOG_ERROR("Unable to get device name.");
			continue;
		}
		const char * device_name_c = CFStringGetCStringPtr(device_name, kCFStringEncodingUTF8);
		if (device_name_c != NULL) {
			LOG_INFO("Checking device:");
			LOG_INFO(device_name_c);
		}

		/* First check that the device is input-capable by checking all of the
		 * streams.
		 */
		data_size = 0;
		property_addr.mSelector = kAudioDevicePropertyStreamConfiguration;
		err = AudioObjectGetPropertyDataSize(
				device_ids[i],
				&property_addr,
				0,
				NULL,
				&data_size
		);
		if (err != kAudioHardwareNoError) {
			LOG_ERROR("Unable to get size of streams array.");
			CFRelease(device_name);
			device_name = NULL;
			// instead of exiting, continue checking other devices
			continue;
		}
		AudioBufferList *buffer_list = malloc(data_size);
		if (buffer_list == NULL) {
			LOG_ERROR("Unable to allocate memory for stream buffer list.");
			/* Being unable to allocate memory is basically game over, so we
			 * clean up and return.
			 */
			CFRelease(cf_name);
			free(device_ids);
			return kAudioObjectUnknown;
		}
		err = AudioObjectGetPropertyData(
				device_ids[i],
				&property_addr,
				0,
				NULL,
				&data_size,
				buffer_list
		);
		if (err != kAudioHardwareNoError) {
			LOG_ERROR("Unable to get stream buffer list.");
			CFRelease(device_name);
			device_name = NULL;
			free(buffer_list);
			continue;
		}
		if (buffer_list->mNumberBuffers == 0) {
			LOG_INFO("No input buffers found on device, skipping.");
			CFRelease(device_name);
			device_name = NULL;
			free(buffer_list);
			continue;
		}
		// We don't need the buffer list anymore
		free(buffer_list);
		// Compare the device name to what was given.
		// Using the default comparison options
		CFStringCompareFlags flags = 0;
		CFComparisonResult comparison = CFStringCompare(
				cf_name,
				device_name,
				flags
		);
		/* The Create Rule in Core Foundation would say that I am *not*
		 * responsible for freeing device_name (as it's
		 * AudioObject_Get_Property, not AudioObject_Copy_Property), but the
		 * documentation for AudioUnitGetProperty (note "Unit", not "Object")
		 * says that the caller is responsible for releasing CFObjects
		 * retrieved through that API.
		 */
		CFRelease(device_name);
		if (comparison == kCFCompareEqualTo) {
			device_found = true;
			break;
		}
	}
	CFRelease(cf_name);
	AudioDeviceID device_id = kAudioObjectUnknown;
	if (device_found) {
		device_id = device_ids[i];
	} else {
		LOG_WARN("Unable to find device with given name.");
	}
	free(device_ids);
	return device_id;
}

AudioDeviceID get_default_input_device()
{
	// See find_input_device comments for explanation for some variables
	OSStatus err;
	AudioDeviceID device_id = kAudioObjectUnknown;
	UInt32 data_size = sizeof(AudioDeviceID);
	AudioObjectPropertyAddress property_addr = {0};
	property_addr.mScope = kAudioObjectPropertyScopeGlobal;
	property_addr.mElement = kAudioObjectPropertyElementMaster;
	LOG_INFO("Using default input device.");
	property_addr.mSelector = kAudioHardwarePropertyDefaultInputDevice;
	// Just get the default input device
	err = AudioObjectGetPropertyData(
		kAudioObjectSystemObject,
		&property_addr,
		// No input data
		0,
		NULL,
		&data_size,
		&device_id
	);
	if (err != kAudioHardwareNoError) {
		LOG_ERROR("Unable to get default input device ID.");
		return kAudioObjectUnknown;
	}
	return device_id;
}

void audio_queue_callback(
		void * untyped_tap,
		AudioQueueRef queue,
		AudioQueueBufferRef buffer,
		const AudioTimeStamp * start_time, // not used in recording
		UInt32 num_packet_desc, // not used for PCM
		const AudioStreamPacketDescription * packet_desc // not used for PCM
)
{
	OSStatus err;
	AudioTap * tap = (AudioTap *)untyped_tap;
	float * raw_samples = (float *)buffer->mAudioData;
	int num_channels = tap->format.mChannelsPerFrame;
	size_t sample_size = tap->format.mBytesPerFrame / num_channels;
	int num_samples = buffer->mAudioDataByteSize / sample_size / num_channels;
	// Initialize the samples arrays
	float ** samples = malloc(sizeof(float *) * num_channels);
	for (int i = 0; i < num_channels; i++) {
		samples[i] = malloc(sample_size * num_samples);
	}
	if (num_channels > 1 && tap->format.mFormatFlags & kLinearPCMFormatFlagIsNonInterleaved) {
		/* Audio samples are interleaved on macOS, so we need to de-interleave
		 * them. Since this particular code is macOS specific, we can use the
		 * Accelerate framework to use SIMD where we can.
		 * Sourced from here: https://stackoverflow.com/a/10374015
		 */
		float zero = 0.0f;
		for (int i = 0; i < num_channels; i++) {
			vDSP_vsadd(
					raw_samples + i,
					num_channels,
					&zero,
					samples[i],
					1,
					num_samples
			);
		}
	} else {
		// TODO: I think each channel is just one after the other.
		const size_t stride = sample_size * num_samples;
		for (int i = 0; i < num_channels; i++) {
			memmove(samples[i], raw_samples + (i * num_samples), stride);
		}
	}
	// Eventually...
	tap->callback(tap->data, samples, num_channels, num_samples);
	// We need to put the buffer back once we're done with it
	err = AudioQueueEnqueueBuffer(queue, buffer, 0, NULL);
	if (err) {
		LOG_WARN("Error re-queueing audio buffer.");
		// Stopping won't do much help here
	}
	// Clean up the memory we've allocated
	for (int i = 0; i < num_channels; i++) {
		free(samples[i]);
	}
	free(samples);
}
