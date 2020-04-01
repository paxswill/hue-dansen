#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

int main(int argc, char **argv)
{
	if (argc <= 4) {
		printf("Usage:\n%s hue-IP-address identity psk\n", argv[0]);
	}
}

SSL_CTX *createContext()
{
	SSL_CTX *ctx = SSL_CTX_new(DTLS_client_method());
	if (ctx == NULL) {
		fprintf(stderr, "Error allocating SSL context.\n");
		ERR_print_errors_fp(stderr);
		exit(EXIT_FAILURE);
	}
	// Force it to just DTLSv1.2
	SSL_CTX_set_min_proto_version(ctx, DTLS1_2_VERSION);
	return ctx;
}
