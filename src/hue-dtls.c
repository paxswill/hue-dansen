#include "hue-dtls.h"
#include "log.h"

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

struct psk_identity {
	char * identity;
	unsigned char * psk;
	unsigned int psk_len;
};

struct psk_identity * create_identity(char * identity, char * psk_hex);
void free_identity(struct psk_identity * identity);

unsigned int psk_callback(SSL * ssl, const char * hint,
                          char * identity_buf, unsigned int identity_buf_len,
                          unsigned char * psk_buf, unsigned int psk_buf_len);
int connect_udp(char * hostname);
BIO * attach_bio(int sock);
SSL * connect_ssl(SSL_CTX * ctx, BIO * bio, struct psk_identity * psk);


SSL_CTX *create_context()
{
	SSL_CTX *ctx = SSL_CTX_new(DTLS_client_method());
	if (ctx == NULL) {
		LOG_OPENSSL_ERROR("Unable to allocate SSL context.");
		return NULL;
	}
	// Force it to just DTLSv1.2
	SSL_CTX_set_min_proto_version(ctx, DTLS1_2_VERSION);
	// Hue entertainment only supports one cipher
	SSL_CTX_set_cipher_list(ctx, "PSK-AES128-GCM-SHA256");
	// Give the identity and PSK
	SSL_CTX_set_psk_client_callback(ctx, psk_callback);
	LOG_INFO("Created DTLSv1.2 context.")
	return ctx;
}


struct connection * open_connection(SSL_CTX * ctx, char * hostname,
                                    char * identity, char * psk_hex)
{
	struct psk_identity * psk = create_identity(identity, psk_hex);
	if (psk == NULL) {
		LOG_ERROR("PSK identity creation failed.");
		return NULL;
	}

	struct connection * conn = malloc(sizeof(struct connection));
	if (conn == NULL) {
		LOG_ERROR("Allocating connection memory failed.");
		goto cleanup_open_conn_psk;
	}

	conn->sock = connect_udp(hostname);
	if (conn->sock < 0) {
		LOG_ERROR("Connecting over UDP socket failed.");
		goto cleanup_open_conn_connection;
	}
	conn->bio = attach_bio(conn->sock);
	if (conn->bio == NULL) {
		LOG_ERROR("Attaching BIO to existing socket failed.");
		// The BIO will close the socket for us (even though we asked it not
		// to...).
		close(conn->sock);
		goto cleanup_open_conn_connection;
	}
	conn->ssl = connect_ssl(ctx, conn->bio, psk);
	if (conn->ssl == NULL) {
		LOG_ERROR("Opening DTLS channel failed.");
		goto cleanup_open_conn_bio;
	}
	return conn;

cleanup_open_conn_bio:
	BIO_free(conn->bio);
cleanup_open_conn_connection:
	free(conn);
cleanup_open_conn_psk:
	free_identity(psk);
	return NULL;
}

void close_connection(struct connection * conn)
{
	// Having app data on the SSL object complicates it slightly
	struct psk_identity * identity = SSL_get_app_data(conn->ssl);
	if (identity != NULL) {
		SSL_set_app_data(conn->ssl, NULL);
		free_identity(identity);
	}
	SSL_shutdown(conn->ssl);
	BIO_free(conn->bio);
	// We're supposed to close the socket ourselves, but it seems like the BIO
	// is doing that for us.
}

struct psk_identity * create_identity(char * identity, char * psk_hex)
{
	struct psk_identity * app_psk = malloc(sizeof(struct psk_identity));
	if (app_psk == NULL) {
		LOG_ERROR("Allocating PSK identity memory failed.");
		return NULL;
	}
	app_psk->identity = malloc(strlen(identity) + 1);
	if (app_psk->identity == NULL) {
		LOG_ERROR("Allocating identity memory failed.");
		goto cleanup_create_identity_app_psk;
	}
	strcpy(app_psk->identity, identity);
	// The PSK is given as a hex string but we need the raw bytes. It is also
	// always 16 bytes (32 hex characters)
	app_psk->psk_len = 16;
	app_psk->psk = malloc(app_psk->psk_len);
	if (app_psk->psk == NULL) {
		LOG_ERROR("Allocating PSK memory failed.");
		goto cleanup_create_identity_identity;
	}
	for (int i = 0; i < 32; i += 2) {
		sscanf(psk_hex + i, "%2hhX", app_psk->psk + i / 2);
	}
	return app_psk;

cleanup_create_identity_identity:
	free(app_psk->identity);
cleanup_create_identity_app_psk:
	free(app_psk);
	return NULL;
}


void free_identity(struct psk_identity * identity)
{
	free(identity->psk);
	free(identity->identity);
	free(identity);
}


int connect_udp(char *host)
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
		LOG_ERROR(gai_strerror(err));
		return -1;
	}
	sock = -1;
	for (result = results; result; result = result->ai_next) {
		sock = socket(result->ai_family, result->ai_socktype,
				      result->ai_protocol);
		if (sock < 0) {
			LOG_WARN(strerror(errno));
			continue;
		}
		LOG_INFO("UDP socket created.");
		if (connect(sock, result->ai_addr, result->ai_addrlen) < 0) {
			LOG_WARN(strerror(errno));
			close(sock);
			sock = -1;
			continue;
		}
		LOG_INFO("UDP socket connected.");
		break;
	}
	freeaddrinfo(results);
	if (sock < 0) {
		LOG_ERROR("Unable to connect to host.");
		return -1;
	}
	return sock;
}

BIO * attach_bio(int sock)
{
	int err;
	BIO *bio = BIO_new_dgram(sock, BIO_NOCLOSE);
	if (bio == NULL) {
		LOG_OPENSSL_ERROR("Unable to create BIO.");
		return NULL;
	}
	LOG_INFO("BIO created.");
	// Keeping the UDP connection in a separate function, so we have to re-look
	// up the address the socket is connected to.
	// Just assuming IPv6 address structs are large enough for both IPv4 and v6
	socklen_t addr_len = sizeof(struct sockaddr_in6);
	struct sockaddr *addr = malloc(addr_len);
	if (addr == NULL) {
		LOG_ERROR("Unable to allocate memory!");
		goto cleanup_attach_bio;
	}
	err = getsockname(sock, addr, &addr_len);
	if (err) {
		LOG_ERROR(strerror(errno));
		goto cleanup_attach_bio;
	}
	// Hook the BIO up to the socket
	err = BIO_ctrl(bio, BIO_CTRL_DGRAM_SET_CONNECTED, 0, addr);
	/* Commenting this out as the return value fo BIO_ctrl isn't great.
	if (err) {
		LOG_OPENSSL_ERROR("Unable to set BIO connected status.");
		goto cleanup_bio;
	}
	*/
	// TODO: Confirm that I don't have a use-after-free here
	free(addr);
	LOG_INFO("BIO attached to existing UDP socket.");
	return bio;

cleanup_attach_bio:
	BIO_flush(bio);
	if(!BIO_free(bio)) {
		LOG_OPENSSL_ERROR("Error cleaning up BIO.");
	}
	return NULL;
}


SSL * connect_ssl(SSL_CTX *ctx, BIO *bio, struct psk_identity * psk)
{
	// Create an SSL object and hook it up to the BIO
	SSL *ssl = SSL_new(ctx);
	if (ssl == NULL) {
		LOG_OPENSSL_ERROR("Unable to create SSL object.");
		return NULL;
	}
	LOG_INFO("SSL object created.");
	SSL_set_bio(ssl, bio, bio);
	LOG_INFO("SSL objected attached to BIO.");
	if (!SSL_set_app_data(ssl, psk)) {
		LOG_ERROR("Unable to associate PSK identity with DTLS channel.");
		// Closing the BIO seems to close the SSL out as well.
		goto cleanup_connect_ssl;
	}
	// Connect
	int err = SSL_connect(ssl);
	if (err < 1) {
		LOG_OPENSSL_ERROR("Unable to open DTLS channel.");
		goto cleanup_connect_ssl;
	}
	LOG_INFO("DTLS connected.");
	// Set timeouts *after* handshake
	struct timeval timeout;
	bzero(&timeout, sizeof(timeout));
	timeout.tv_sec = 2;
	BIO_ctrl(bio, BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, &timeout);
	LOG_INFO("SSL timeout set.");
	return ssl;

cleanup_connect_ssl:
	SSL_free(ssl);
	return NULL;
}


unsigned int psk_callback(SSL * ssl, const char * hint,
                          char * identity_buf, unsigned int identity_buf_len,
                          unsigned char * psk_buf, unsigned int psk_buf_len)
{
	void * app_data = SSL_get_app_data(ssl);
	if (app_data != NULL) {
		struct psk_identity * ssl_identity = app_data;
		if (ssl_identity->psk_len > psk_buf_len) {
			LOG_ERROR("PSK is longer than the PSK buffer.");
			return 0;
		}
		strlcpy(identity_buf, ssl_identity->identity, identity_buf_len);
		memcpy(psk_buf, ssl_identity->psk, ssl_identity->psk_len);
		return ssl_identity->psk_len;
	} else {
		LOG_WARN("No PSK identiy found for SSL connection.");
		return 0;
	}
}

