export OPENSSL_PREFIX := $(shell brew --prefix openssl@1.1)
LDFLAGS := -L$(OPENSSL_PREFIX)/lib/
LDLIBS := -lssl -lcrypto -lc
CPPFLAGS := -I$(OPENSSL_PREFIX)/include/ 
CFLAGS := -Wall -g

all: hue-dansen

.PHONY: clean test

hue-dansen: hue-dansen.o hue-dtls.o
	$(CC) $(LDFLAGS) -o $@ $^

hue-dansen.o: hue-dtls.h log.h
hue-dtls.o: hue-dtls.c hue-dtls.h log.h

test: hue-dansen
	./test.sh

clean:
	rm -f *.o hue-dansen
