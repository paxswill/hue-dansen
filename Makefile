export OPENSSL_PREFIX := $(shell brew --prefix openssl@1.1)
LDFLAGS := -L$(OPENSSL_PREFIX)/lib/
LDLIBS := -lssl -lcrypto -lc -framework CoreAudio -framework Foundation -framework AudioToolbox -framework Accelerate
CPPFLAGS := -I$(OPENSSL_PREFIX)/include/ 
CFLAGS := -Wall -g

sources = hue-dtls.c hue-dansen.c ring-buffer.c
include $(sources:.c=.d)

.PHONY: clean test all

all: hue-dansen

hue-dansen: hue-dtls.o

test: test-ring-buffer
	./test-ring-buffer

test-ring-buffer: ring-buffer.o

clean:
	rm -f *.o *.d hue-dansen

%.d: %.c
	@set -e; rm -f $@; \
		$(CC) -MM $(CPPFLAGS) $< > $@.$$$$; \
		sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
		rm -f $@.$$$$
