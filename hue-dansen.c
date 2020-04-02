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

#define HUE_ENTERTAINMENT_PORT "2100"

#define LOG(lvl, msg) do { \
	fprintf(stderr, "(%-6s) %4d: %s\n", lvl, __LINE__, msg); \
} while(0);

#define INFO(msg) LOG("INFO", msg);
#define WARN(msg) LOG("WARN", msg);
#define ERROR(msg) LOG("ERROR", msg);
#define OPENSSL_ERROR(msg) do { \
	ERROR(msg); \
	ERR_print_errors_fp(stderr); \
} while(0);


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

unsigned int pskCallback(SSL *ssl,
                         const char *hint,
                         char *identity_buf,
                         unsigned int max_identity_len,
                         unsigned char *psk_buf,
                         unsigned int max_psk_len)
{
	strlcpy(identity_buf, identity, max_identity_len);
	// Convert the 32 character hex string into 16 bytes
	uint8_t pskBytes[16];
	for (int i = 0; i < 32; i += 2) {
		sscanf(psk + i, "%2hhX", pskBytes + i / 2);
	}
	memcpy(psk_buf, &pskBytes, 16);
	psk_buf[16] = 0;
	return 1;
}

SSL_CTX *createContext()
{
	SSL_CTX *ctx = SSL_CTX_new(DTLS_client_method());
	if (ctx == NULL) {
		OPENSSL_ERROR("Unable to allocate SSL context.");
		exit(EXIT_FAILURE);
	}
	// Force it to just DTLSv1.2
	SSL_CTX_set_min_proto_version(ctx, DTLS1_2_VERSION);
	// Hue entertainment only supports one cipher
	SSL_CTX_set_cipher_list(ctx, "PSK-AES128-GCM-SHA256");
	// Give the identity and PSK
	SSL_CTX_set_psk_client_callback(ctx, pskCallback);
	return ctx;
}

int connectUDP(char *host)
{
	// Modified from the getaddrinfo man page
	struct addrinfo hints, *results, *result;
	int err, sock;
	bzero(&hints, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	hints.ai_flags = AI_DEFAULT;

	err = getaddrinfo(host, HUE_ENTERTAINMENT_PORT, &hints, &results);
	if (err) {
		ERROR(gai_strerror(err));
		exit(EXIT_FAILURE);
	}
	sock = -1;
	for (result = results; result; result = result->ai_next) {
		sock = socket(result->ai_family, result->ai_socktype,
				      result->ai_protocol);
		if (sock < 0) {
			WARN(strerror(errno));
			continue;
		}
		if (connect(sock, result->ai_addr, result->ai_addrlen) < 0) {
			WARN(strerror(errno));
			close(sock);
			sock = -1;
			continue;
		}
		break;
	}
	if (sock < 0) {
		ERROR("Unable to connect to host.");
		exit(EXIT_FAILURE);
	}
	return sock;
}

BIO *connectDTLS(SSL_CTX *ctx, char *host)
{
	BIO *bio = BIO_new_ssl_connect(ctx);
	if (bio == NULL) {
		OPENSSL_ERROR("Unable to create BIO.");
		exit(EXIT_FAILURE);
	}
	BIO_set_conn_hostname(bio, host);
	BIO_set_conn_port(bio, HUE_ENTERTAINMENT_PORT);

	SSL *ssl;
	BIO_get_ssl(bio, &ssl);
	SSL_set_connect_state(ssl);
	return bio;
}

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
	int ret = SSL_write(ssl, buf, bufLen);
	free(buf);
	printf("\tWrote %d bytes to DTLS\n", ret);
	if (ret < 0) {
		OPENSSL_ERROR("Unable to send data.");
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char **argv)
{
	if (argc != 4) {
		fprintf(stderr, "Usage:\n%s hue-IP-address identity psk\n", argv[0]);
		printf("Argc: %d\n", argc);
		exit(EXIT_FAILURE);
	}
	identity = argv[2];
	psk = argv[3];
	if (strlen(psk) != 32) {
		ERROR("PSK needs to be exactly 32 hex digits.");
		exit(EXIT_FAILURE);
	}
	SSL_CTX *ctx = createContext();
	BIO *bio = connectDTLS(ctx, argv[1]);
	SSL *ssl;
	BIO_get_ssl(bio, &ssl);
	SSL_connect(ssl);

	int colorIndex = 0;
	struct timespec delay;
	delay.tv_sec = 0;
	delay.tv_nsec = 1000000000 * 0.65;
	// TODO: hardcoding the lights for now
	uint16_t lightIDs[] = {0, 1};
	while(1) {
		struct color color = colors[colorIndex];
		setCIE(ssl, color.x, color.y, 0xffff, 2, lightIDs);
		if (colorIndex >= 4) {
			colorIndex = 0;
		} else {
			colorIndex += 1;
		}
		nanosleep(&delay, NULL);
		printf("Set color index %d\n", colorIndex);
	}
}
