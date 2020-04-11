#include "ring-buffer.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
// Using the non-standard libc extension from glibc and freebsd libc for MAX()
#include <sys/param.h>

#define RING_END(_ring_buf) (_ring_buf->buffer + _ring_buf->buf_size)

#define BETWEEN(min, val, max) ((min) <= (val) && (val) <= (max))

struct ring_buffer * ring_buf_create(size_t element_size, int element_count)
{
	struct ring_buffer * buffer = malloc(sizeof(struct ring_buffer));
	if (buffer == NULL) {
		LOG_ERROR("Unable to allocate ring buffer structure.");
		return NULL;
	}
	buffer->element_size = element_size;
	buffer->count = element_count;
	buffer->buf_size = element_count * element_size;
	buffer->buffer = malloc(buffer->buf_size);
	if (buffer->buffer == NULL) {
		LOG_ERROR("Unable to allocate ring buffer.");
		free(buffer);
		return NULL;
	}
	buffer->read_index = buffer->write_index = 0;
	buffer->written = true;
	return buffer;
}

void ring_buf_free(struct ring_buffer * buffer)
{
	free(buffer->buffer);
	free(buffer);
}

int ring_buf_write(struct ring_buffer * ring,
                   data_t * elements, unsigned int elements_count)
{
	size_t element_size = ring->element_size;
	// Limit the actual number of elements to write to the most recent ones
	// that will actually fit in the buffer.
	unsigned int write_count = MIN(elements_count, ring->count);
	data_t * source = elements + (elements_count - write_count) * element_size;
	/* There are up to two distinct writes being done. The first is from
	 * ring->write_index, and if the number of elements being written would
	 * go past the end of the buffer, the second write picks up from the
	 * beginning of the buffer.
	 */
	unsigned int new_write_index;
	new_write_index = (ring->write_index + write_count) % ring->count;
	if (ring->write_index + write_count > ring->count) {
		// two-pass write
		unsigned int count2 = new_write_index;
		unsigned int count1 = write_count - count2;
		memmove(ring->buffer + (element_size * ring->write_index),
		        source,
		        count1 * element_size);
		memmove(ring->buffer,
		        source + (element_size * count1),
		        count2 * element_size);
		// Move the read index if we've lapped it
		if (!BETWEEN(new_write_index, ring->read_index, ring->write_index)) {
			ring->read_index = new_write_index;
		}
	} else {
		memmove(ring->buffer + (element_size * ring->write_index),
		        source, write_count * element_size);
		// As above, update the read index if we've lapped it.
		if (BETWEEN(ring->write_index, ring->read_index,
		            ring->write_index + write_count)) {
			ring->read_index = new_write_index;
		}
	}
	ring->write_index = new_write_index;
	ring->written = true;
	return write_count;
}

int ring_buf_read(struct ring_buffer * ring, void * dest, size_t dest_len)
{
	if (dest_len > ring->buf_size) {
		LOG_WARN("Reading more data out than the ring buffer can store.");
	}
}

void ring_buf_clear(struct ring_buffer * ring){
	ring->write_index = 0;
	ring->read_index = 0;
	memset(ring->buffer, 0, ring->buf_size);
	ring->written = false;
}

void ring_buf_skip(struct ring_buffer * ring, unsigned int pos)
{

	ring->read_index = (ring->read_index + pos) % ring->count;
}

unsigned int ring_buf_available(struct ring_buffer * ring)
{
	if (ring->written) {
		if (ring->read_index > ring->write_index) {
			// The write index has looped around
			return ring->count - ring->read_index + ring->write_index;
		} else {
			unsigned int available =  ring->write_index - ring->read_index;
			// account for the case where the write index has caught up with
			// the read index
			if (available == 0) {
				return ring->count;
			}
		}
	} else {
		return 0;
	}
}
