export OPENSSL_PREFIX := $(shell brew --prefix openssl@1.1)
LDFLAGS := -lssl -lcrypto -L$(OPENSSL_PREFIX)/lib/
CFLAGS := -I$(OPENSSL_PREFIX)/include/ -Wall -g

all: hue-dansen

.PHONY: clean test

hue-dansen: hue-dansen.o
	$(CC) $(LDFLAGS) -o $@ $^

test: hue-dansen
	./test.sh

clean:
	rm -f *.o hue-dansen
