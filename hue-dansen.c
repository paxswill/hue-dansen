#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

#include "log.h"
#include "hue-dtls.h"


#define BPM 165
#define BPM_DELAY_US ((int)(60.0 / (float)BPM * 1000000))
#define SUPERSAMPLE_COUNT 4


static char *identity;
static char *psk;

struct color {
	uint16_t x;
	uint16_t y;
};

static struct color colors[4] = {
	// red (0.65, 0.3)
	{42597, 19660},
	// green (0.1, 0.7)
	{6553, 45874},
	// purplish-blue (0.15, 0.075)
	{9830, 4915},
	// orange (0.54, 0.43)
	{35388, 28180}
};

// Just wrap around, we don't care.
static uint8_t sequenceNumber;


void setCIE(SSL *ssl,
            uint16_t x, uint16_t y, uint16_t brightness,
            int numLights, uint16_t *lightIDs)
{
	// allocate a buffer for the message.
	// Header of 16 bytes followed by 9 bytes for each light.
	numLights = numLights > 10 ? 10 : numLights;
	int bufLen = 16 + numLights * 9;
	uint8_t *buf = malloc(bufLen);
	// Clear everything to 0
	bzero(buf, bufLen);
	// Write out the header.
	// First 9 bytes are "HueStream"
	memcpy(buf, "HueStream", 9);
	// Next 2 are the major, then minor version (1.0)
	buf[9] = 0x1;
	// Next 1 byte for the sequence number
	// (ignored for now, but sending it anyway).
	buf[11] = sequenceNumber++;
	// 2 Reserved bytes, set to 0
	// 1 bytes for the color space. 0 for RGB, 1 for CIE
	buf[14] = 1;
	// Another 1 reserved byte, zeroed.
	// And then the light info
	uint8_t *lightBegin = buf + 16;
	uint16_t *lightBuf;

	for (int i = 0; i < numLights; i++) {
		// After the first byte, everything is a 2 byte integer. The first byte
		// is also 0 in all cases, so there's going to be an aliased buffer to
		// make my life easier.
		// The first byte is the device type. Lights are type 0.
		lightBuf = (uint16_t *)(lightBegin + 1);
		// Then 2 bytes for the light ID
		lightBuf[0] = htons(*(lightIDs + i));
		// Then the color value. Either RGB, or XY + brightness
		lightBuf[1] = htons(x);
		lightBuf[2] = htons(y);
		lightBuf[3] = htons(brightness);
		lightBegin += 9;
	}
	// Now send the entire message off to DTLS land
	// Don't care, just keep spamming packets
	int ret = SSL_write(ssl, buf, bufLen);
	free(buf);
}

void loopColors(SSL *ssl, int duration)
{
	time_t startTime = time(NULL);
	time_t currentTime = startTime;
	int colorIndex = 0;
	// TODO: hardcoding the lights for now
	uint16_t lightIDs[] = {2, 3};
	while(1) {
		if (duration != 0) {
			currentTime = time(NULL);
			if (currentTime - startTime > duration) {
				break;
			}
		}
		struct color color = colors[colorIndex];
		for (int i = 0; i < SUPERSAMPLE_COUNT; i++) {
			setCIE(ssl, color.x, color.y, 0xffff, 2, lightIDs);
			usleep(BPM_DELAY_US / SUPERSAMPLE_COUNT);
		}
		if (colorIndex >= 4) {
			colorIndex = 0;
		} else {
			colorIndex += 1;
		}
		//printf("Set color index %d\n", colorIndex);
	}
}

int main(int argc, char **argv)
{
	int duration = 0;
	int exit_status = EXIT_SUCCESS;
	if (argc < 4) {
		fprintf(stderr,
		        "Usage:\n%s hue-IP-address identity psk [duration]\n", argv[0]);
		printf("Argc: %d\n", argc);
		exit(EXIT_FAILURE);
	}
	if (argc > 4) {
		duration = sscanf(argv[4], "%d", &duration);
	}
	identity = argv[2];
	psk = argv[3];
	if (strlen(psk) != 32) {
		LOG_ERROR("PSK needs to be exactly 32 hex digits.");
		exit(EXIT_FAILURE);
	}

	SSL_CTX *ctx = create_context();
	if (ctx == NULL) {
		LOG_ERROR("Unable to create SSL context.");
		exit(EXIT_FAILURE);
	}
	struct connection * conn = open_connection(ctx, argv[1], argv[2], argv[3]);
	if (conn == NULL) {
		LOG_ERROR("Unable to open DTLS connection to Hue bridge.");
		exit_status = EXIT_FAILURE;
		goto cleanup_ctx;
	}

	printf("Delay is %d us (%f s)\n", BPM_DELAY_US, (BPM_DELAY_US / 1000000.0));
	loopColors(conn->ssl, duration);

	close_connection(conn);
cleanup_ctx:
	SSL_CTX_free(ctx);
	exit(exit_status);
}
