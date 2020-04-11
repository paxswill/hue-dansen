#ifndef HUE_LOG_H
#define HUE_LOG_H

#include <stdio.h>
#include <openssl/err.h>

enum log_level {
	ERROR,
	WARN,
	INFO,
	DEBUG
};

static int log_level = INFO;

#define _STR(s) #s

#define LOG(lvl, msg) do { \
	if(lvl <= log_level) \
	fprintf(stderr, "(%-6s) %15s:%4d: %s\n", _STR(lvl), __FILE__, __LINE__, msg); \
} while(0);

#define LOG_INFO(msg) LOG(INFO, msg);
#define LOG_WARN(msg) LOG(WARN, msg);
#define LOG_ERROR(msg) LOG(ERROR, msg);
#define LOG_OPENSSL_ERROR(msg) do { \
	LOG_ERROR(msg); \
	ERR_print_errors_fp(stderr); \
} while(0);

#endif
