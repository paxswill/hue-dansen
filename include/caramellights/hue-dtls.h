#ifndef HUE_DTLS_H
#define HUE_DTLS_H

#include <openssl/ssl.h>

struct connection {
	int sock;
	BIO *bio;
	SSL *ssl;
};

SSL_CTX * create_context();

struct connection * open_connection(SSL_CTX * ctx, char * hostname,
                                    char * identity, char * psk_hex);
void close_connection(struct connection * conn);

#endif
