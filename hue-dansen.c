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

#define LOG(lvl, msg) do { \
	fprintf(stderr, "(%-6s) %15s:%4d: %s\n", lvl, __FILE__, __LINE__, msg); \
} while(0);

#define INFO(msg) LOG("INFO", msg);
#define WARN(msg) LOG("WARN", msg);
#define ERROR(msg) LOG("ERROR", msg);
#define OPENSSL_ERROR(msg) do { \
	ERROR(msg); \
	ERR_print_errors_fp(stderr); \
} while(0);


#define HUE_ENTERTAINMENT_PORT "2100"
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
	return 16;
}

SSL_CTX *createContext()
{
	SSL_CTX *ctx = SSL_CTX_new(DTLS_client_method());
	if (ctx == NULL) {
		OPENSSL_ERROR("Unable to allocate SSL context.");
		return NULL;
	}
	// Force it to just DTLSv1.2
	SSL_CTX_set_min_proto_version(ctx, DTLS1_2_VERSION);
	// Hue entertainment only supports one cipher
	SSL_CTX_set_cipher_list(ctx, "PSK-AES128-GCM-SHA256");
	// Give the identity and PSK
	SSL_CTX_set_psk_client_callback(ctx, pskCallback);
	INFO("Created DTLSv1.2 context.")
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
		return -1;
	}
	sock = -1;
	for (result = results; result; result = result->ai_next) {
		sock = socket(result->ai_family, result->ai_socktype,
				      result->ai_protocol);
		if (sock < 0) {
			WARN(strerror(errno));
			continue;
		}
		INFO("UDP socket created.");
		if (connect(sock, result->ai_addr, result->ai_addrlen) < 0) {
			WARN(strerror(errno));
			close(sock);
			sock = -1;
			continue;
		}
		INFO("UDP socket connected.");
		break;
	}
	freeaddrinfo(results);
	if (sock < 0) {
		ERROR("Unable to connect to host.");
		return -1;
	}
	return sock;
}

BIO *connectDTLS(SSL_CTX *ctx, int sock)
{
	int err;
	BIO *bio = BIO_new_dgram(sock, BIO_NOCLOSE);
	if (bio == NULL) {
		OPENSSL_ERROR("Unable to create BIO.");
		return NULL;
	}
	INFO("BIO created.");
	// Keeping the UDP connection in a separate function, so we have to re-look
	// up the address the socket is connected to.
	// Just assuming IPv6 address structs are large enough for both IPv4 and v6
	socklen_t addr_len = sizeof(struct sockaddr_in6);
	struct sockaddr *addr = malloc(addr_len);
	if (addr == NULL) {
		ERROR("Unable to allocate memory!");
		goto cleanup_dtls_bio;
	}
	err = getsockname(sock, addr, &addr_len);
	if (err) {
		ERROR(strerror(errno));
		goto cleanup_dtls_bio;
	}
	// Hook the BIO up to the socket
	err = BIO_ctrl(bio, BIO_CTRL_DGRAM_SET_CONNECTED, 0, addr);
	/* Commenting this out as the return value fo BIO_ctrl isn't great.
	if (err) {
		OPENSSL_ERROR("Unable to set BIO connected status.");
		goto cleanup_bio;
	}
	*/
	// TODO: Confirm that I don't have a use-after-free here
	free(addr);
	INFO("BIO attached to existing UDP socket.");
	return bio;

cleanup_dtls_bio:
	BIO_flush(bio);
	if(!BIO_free(bio)) {
		OPENSSL_ERROR("Error cleaning up BIO.");
	}
	return NULL;
}


SSL *connectSSL(SSL_CTX *ctx, BIO *bio)
{
	// Create an SSL object and hook it up to the BIO
	SSL *ssl = SSL_new(ctx);
	if (ssl == NULL) {
		OPENSSL_ERROR("Unable to create SSL object.");
		return NULL;
	}
	INFO("SSL object created.");
	SSL_set_bio(ssl, bio, bio);
	INFO("SSL objected attached to BIO.");
	// Set timeout
	struct timeval timeout;
	bzero(&timeout, sizeof(timeout));
	timeout.tv_sec = 2;
	BIO_ctrl(bio, BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, &timeout);
	INFO("SSL timeout set.");
	// Connect
	int err = SSL_connect(ssl);
	if (err < 1) {
		OPENSSL_ERROR("Unable to open DTLS channel.");
		goto cleanup_ssl_ssl;
	}
	INFO("DTLS connected.");
	return ssl;

cleanup_ssl_ssl:
	SSL_free(ssl);
	return NULL;
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
	uint16_t lightIDs[] = {0, 1};
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
		printf("Set color index %d\n", colorIndex);
	}
}

int main(int argc, char **argv)
{
	int duration = 0;
	int exitStatus = EXIT_SUCCESS;
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
		ERROR("PSK needs to be exactly 32 hex digits.");
		exit(EXIT_FAILURE);
	}

	SSL_CTX *ctx = createContext();
	if (ctx == NULL) {
		ERROR("Unable to create SSL context.");
		exit(EXIT_FAILURE);
	}
	int sock = connectUDP(argv[1]);
	if (sock < 0) {
		exitStatus = EXIT_FAILURE;
		goto cleanup_socket;
	}
	BIO *bio = connectDTLS(ctx, sock);
	if (bio == NULL) {
		exitStatus = EXIT_FAILURE;
		goto cleanup_bio;
	}
	SSL *ssl = connectSSL(ctx, bio);
	if (ssl == NULL) {
		exitStatus = EXIT_FAILURE;
		goto cleanup_bio;
	}

	loopColors(ssl);

cleanup_bio:
	BIO_free(bio);
cleanup_socket:
	if(!shutdown(sock, SHUT_RDWR)) {
		exitStatus = EXIT_FAILURE;
		ERROR(strerror(errno));
	}
	if(!close(sock)) {
		exitStatus = EXIT_FAILURE;
		ERROR(strerror(errno));
	}
cleanup_ctx:
	SSL_CTX_free(ctx);
	exit(exitStatus);
}
