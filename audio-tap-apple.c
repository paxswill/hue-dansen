#include "audio-tap.h"
#include "log.h"

#include <stdbool.h>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>

const UInt32 INPUT_ELEMENT = 1;
const UInt32 OUTPUT_ELEMENT = 0;

void configure_audio_tap(AudioTap * tap);
AudioUnit get_auhal();
AudioDeviceID find_input_device(const char * name);
AudioDeviceID get_default_input_device();

struct AudioTap_Internal {
	AudioUnit auHAL;
	AudioDeviceID input_device;
};

AudioTap * create_audio_tap(const char * name)
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
	tap->auHAL = get_auhal();
	tap->input_device = device_id;
	configure_audio_tap(tap);
	return tap
}

AudioTapErrorCode configure_audio_tap(AudioTap * tap)
{
	/* This function sources heavily from Apple Tech Notes TN2091 and
	 * TN2223 (for API updates).
	 */
	OSStatus err;
	UInt32 data_size = sizeof(AudioDeviceID);
	// Connect the output of the HAL to the input from the input device
	err = AudioUnitSetProperty(
			tap->auHAL,
			kAudioOutputUnitProperty_CurrentDevice,
			kAudioUnitScope_Global,
			OUTPUT_ELEMENT,
			&tap->input_device,
			data_size
	);
	/* Match the sample rate coming out of the AudioUnit to the sample rate
	 * going in to it.
	 */
	CAStreamBasicDescription streamFormat, deviceFormat;
	data_size = sizeof(CAStreamBasicDescription);
	err = AudioUnitGetProperty(
			auHAL,
			kAudioUnitProperty_StreamFormat
			kAudioUnitScope_Output,
			INPUT_ELEMENT,
			&desc,
			&data_size
	);
	if (err != kAudioHardwareNoError) {
		LOG_ERROR("Unable to get input stream format desciption.");
		return TAP_ERROR_UNSPECIFIED;
	}
	err = AudioUnitGetProperty(
			auHAL,
			kAudioUnitProperty_StreamFormat
			kAudioUnitScope_Input,
			INPUT_ELEMENT,
			&desc,
			&data_size
	);
	if (err != kAudioHardwareNoError) {
		LOG_ERROR("Unable to get device stream format desciption.");
		return TAP_ERROR_UNSPECIFIED;
	}
	streamFormat.mSampleRate = deviceFormat.mSampleRate;
	err = AudioUnitSetProperty(
			auHAL,
			kAudioUnitProperty_StreamFormat,
			kAudioUnitScope_Output,
			INPUT_ELEMENT,
			&streamFormat,
			data_size
	);
	if (err != kAudioHardwareNoError) {
		LOG_ERROR("Unable to set output sample rate.");
		return TAP_ERROR_UNSPECIFIED;

	return TAP_NO_ERROR;
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
			free(buffer_list);
			continue;
		}
		if (buffer_list->mNumberBuffers) {
			LOG_INFO("No input buffers found on device, skipping.");
			free(buffer_list);
			continue;
		}
		// We don't need the buffer list anymore
		free(buffer_list);
		// Get the device name and compare it to what was given.
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

AudioUnit get_auhal()
{
	AudioComponentDescription desc = {0};
	// "output" means "output and/or input" in AudioUnit-land
	desc.componentType = kAudioUnitType_Output;
	/* welcome to CoreAudio, where the documentation doesn't ~~matter~~ exist
	 * outside of the header files.
	 */
	desc.componentSubType = kAudioUnitSubType_HALOutput;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	/* There should only be one AUHAL component, so we only need to call
	 * AudioComponentFindNext once.
	 */
	AudioComponent component = AudioComponentFindNext(NULL, &desc);
	// AudioUnit is a type alias for AudioComponentInstance
	AudioUnit auHAL;
	AudioComponentInstanceNew(component, (AudioComponentInstance *)&auHAL);
	// We need a memory location for the property setting functions
	const UInt32 ENABLED = 1;
	const UInt32 DISABLED = 0;
	// Enable input
	AudioUnitSetProperty(
			auHAL,
			kAudioOutputUnitProperty_EnableIO,
			kAudioUnitScope_Input,
			INPUT_ELEMENT,
			&ENABLED,
			sizeof(ENABLED)
	);
	// Disable output
	AudioUnitSetProperty(
			auHAL,
			kAudioOutputUnitProperty_EnableIO,
			kAudioUnitScope_Output,
			OUTPUT_ELEMENT,
			&DISABLED,
			sizeof(DISABLED)
	);
	// Leave the rest of the configuration for later
	return auHAL;
}
