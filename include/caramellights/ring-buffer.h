#ifndef HUE_RING_BUFFER_H
#define HUE_RING_BUFFER_H

#include <stdlib.h>
#include <stdbool.h>

// Using unsigned char as the type so we can do pointer arithemetic
typedef unsigned char data_t;

struct ring_buffer {
	// The size, in bytes, of the buffer
	size_t buf_size;
	// The number of elements in to be stored in the buffer.
	unsigned int count;
	// Each element is thus `buf_size / count` bytes in size.
	size_t element_size;
	data_t * buffer;
	unsigned int read_index;
	unsigned int write_index;
	bool written;
};

struct ring_buffer * ring_buf_create(size_t element_size, int element_count);
void ring_buf_free(struct ring_buffer * buffer);

// Both of these functions return the number of elements (NOT BYTES) accessed
int ring_buf_write(struct ring_buffer * buffer,
                   data_t * elements, unsigned int elements_count);
int ring_buf_read(struct ring_buffer * buffer,
                  void * dest, size_t dest_len);
void ring_buf_clear(struct ring_buffer * ring);
void ring_buf_skip(struct ring_buffer * ring, unsigned int pos);
unsigned int ring_buf_available(struct ring_buffer * ring);

#endif
