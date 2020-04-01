OPENSSL_PREFIX := $(shell brew --prefix openssl@1.1)
LDFLAGS := -lssl -lcrypto -L$(OPENSSL_PREFIX)/lib/
CFLAGS := -I$(OPENSSL_PREFIX)/include/ -Wall

hue-dansen: hue-dansen.o
	$(CC) $(LDFLAGS) -o $@ $^

clean:
	rm -f *.o hue-dansen
