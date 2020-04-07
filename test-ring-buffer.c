#include "ring-buffer.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <limits.h>

#define test_create_free(type) do { \
	struct ring_buffer * ring = ring_buf_create(sizeof(type) , BUFFER_COUNT); \
	assert(ring != NULL); \
	assert(ring->read_index == 0); \
	assert(ring->write_index == 0); \
	ring_buf_free(ring); \
} while(0);


const int BUFFER_COUNT = 10;

struct pair {
	int x;
	int y;
};

/* While entirely possible to write some of these test cases as macros, the
 * debugability of them plummets (at least with lldb and clang) so I'm keeping
 * them as full functions.
 */

void test_simple_write_char()
{
	int i;
	char source[BUFFER_COUNT];
	printf("Testing simple char buffer...");
	for (i = 0; i < BUFFER_COUNT; i++) {
		source[i] = rand() % CHAR_MAX;
	}
	struct ring_buffer * ring = ring_buf_create(1, BUFFER_COUNT);
	assert(
	    BUFFER_COUNT == ring_buf_write(ring, (data_t *)source, BUFFER_COUNT));
	for (i = 0; i < BUFFER_COUNT; i++) {
		assert(source[i] == ((char *)ring->buffer)[i]);
	}
	assert(ring->read_index == 0);
	printf("PASSED\n");
}

void test_simple_write_double()
{
	int i;
	double source[BUFFER_COUNT];
	printf("Testing simple double-float buffer...");
	for (i = 0; i < BUFFER_COUNT; i++) {
		source[i] = (double)rand()/(double)RAND_MAX;
	}
	struct ring_buffer * ring = ring_buf_create(sizeof(double), BUFFER_COUNT);
	assert(
	    BUFFER_COUNT == ring_buf_write(ring, (data_t *)source, BUFFER_COUNT));
	for (i = 0; i < BUFFER_COUNT; i++) {
		assert(source[i] == ((double *)ring->buffer)[i]);
	}
	assert(ring->read_index == 0);
	printf("PASSED\n");
}

void test_extra_write_char()
{
	int i;
	char source[BUFFER_COUNT * 2];
	printf("Testing an extended char buffer...");
	for (i = 0; i < BUFFER_COUNT * 2; i++) {
		source[i] = rand() % CHAR_MAX;
	}
	struct ring_buffer * ring = ring_buf_create(1, BUFFER_COUNT);
	assert(
	    BUFFER_COUNT == ring_buf_write(ring, (data_t *)source, BUFFER_COUNT * 2));
	for (i = 0; i < BUFFER_COUNT; i++) {
		assert(source[i + BUFFER_COUNT] == ((char *)ring->buffer)[i]);
	}
	assert(ring->read_index == 0);
	printf("PASSED\n");
}

void test_extra_write_double()
{
	int i;
	double source[BUFFER_COUNT * 2];
	printf("Testing extended double-float buffer...");
	for (i = 0; i < BUFFER_COUNT * 2; i++) {
		source[i] = (double)rand()/(double)RAND_MAX;
	}
	struct ring_buffer * ring = ring_buf_create(sizeof(double), BUFFER_COUNT);
	assert(
	    BUFFER_COUNT == ring_buf_write(ring, (data_t *)source, BUFFER_COUNT * 2));
	for (i = 0; i < BUFFER_COUNT; i++) {
		assert(source[i + BUFFER_COUNT] == ((double *)ring->buffer)[i]);
	}
	assert(ring->read_index == 0);
	printf("PASSED\n");
}

void test_wrapped_write_char()
{
	int i;
	const int OFFSET = BUFFER_COUNT / 2 - 2;
	char source[BUFFER_COUNT];
	printf("Testing wrapped char buffer...");
	for (i = 0; i < BUFFER_COUNT; i++) {
		source[i] = rand() % CHAR_MAX;
	}
	struct ring_buffer * ring = ring_buf_create(1, BUFFER_COUNT);
	assert(OFFSET == ring_buf_write(ring, (data_t *)source, OFFSET));
	assert(ring->read_index == OFFSET);
	assert(BUFFER_COUNT == ring_buf_write(ring, (data_t *)source, BUFFER_COUNT));
	for (i = 0; i < BUFFER_COUNT; i++) {
		assert(source[i] == ((char *)ring->buffer)[(i + OFFSET) % BUFFER_COUNT]);
	}
	assert(ring->read_index == OFFSET);
	printf("PASSED\n");
}

void test_wrapped_write_double()
{
	int i;
	const int OFFSET = BUFFER_COUNT / 2 - 2;
	double source[BUFFER_COUNT];
	printf("Testing wrapped double buffer...");
	for (i = 0; i < BUFFER_COUNT; i++) {
		source[i] = (double)rand()/(double)RAND_MAX;
	}
	struct ring_buffer * ring = ring_buf_create(sizeof(double), BUFFER_COUNT);
	assert(OFFSET == ring_buf_write(ring, (data_t *)source, OFFSET));
	assert(ring->read_index == OFFSET);
	assert(BUFFER_COUNT == ring_buf_write(ring, (data_t *)source, BUFFER_COUNT));
	double *inner_buffer = (double *)ring->buffer;
	for (i = 0; i < BUFFER_COUNT; i++) {
		assert(source[i] == inner_buffer[(i + OFFSET) % BUFFER_COUNT]);
	}
	assert(ring->read_index == OFFSET);
	printf("PASSED\n");
}

void test_edge_write_char()
{
	int i;
	const int OFFSET = BUFFER_COUNT / 2 - 2;
	char source[BUFFER_COUNT];
	printf("Testing wrapped edge cases for a char buffer...");
	for (i = 0; i < BUFFER_COUNT; i++) {
		// Initialize to 0
		source[i] = 0;
	}
	struct ring_buffer * ring = ring_buf_create(1, BUFFER_COUNT);
	assert(OFFSET == ring_buf_write(ring, (data_t *)source, BUFFER_COUNT));
	assert(ring->read_index == 0);
	// Poke the write index to the end
	ring->write_index = BUFFER_COUNT - 1;
	// Try writing a single item
	char sentinel = rand() % CHAR_MAX;
	assert(1 == ring_buf_write(ring, (data_t *)&sentinel, 1));
	assert(ring->write_index == 0);
	assert(ring->read_index == 0);

	char *inner_buffer = (char *)ring->buffer;
	for (i = 0; i < BUFFER_COUNT - 1; i++) {
		assert(inner_buffer[i] == 0);
	}
	// i is now set to the last index
	assert(inner_buffer[i] == sentinel);
	printf("PASSED\n");
}

void test_edge_write_double()
{
	int i;
	const int OFFSET = BUFFER_COUNT / 2 - 2;
	double source[BUFFER_COUNT];
	printf("Testing wrapped edge cases for a char buffer...");
	for (i = 0; i < BUFFER_COUNT; i++) {
		// Initialize to 0
		source[i] = 0;
	}
	struct ring_buffer * ring = ring_buf_create(1, BUFFER_COUNT);
	assert(OFFSET == ring_buf_write(ring, (data_t *)source, BUFFER_COUNT));
	assert(ring->read_index == 0);
	// Poke the write index to the end
	ring->write_index = BUFFER_COUNT - 1;
	// Try writing a single item
	char sentinel = rand() % CHAR_MAX;
	assert(1 == ring_buf_write(ring, (data_t *)&sentinel, 1));
	assert(ring->write_index == 0);
	assert(ring->read_index == 0);

	char *inner_buffer = (char *)ring->buffer;
	for (i = 0; i < BUFFER_COUNT - 1; i++) {
		assert(inner_buffer[i] == 0);
	}
	// i is now set to the last index
	assert(inner_buffer[i] == sentinel);
	printf("PASSED\n");
}

int main()
{
	// create/free
	test_create_free(int);
	test_create_free(char);
	test_create_free(double);
	test_create_free(struct pair);
	// write
	test_simple_write_char();
	test_simple_write_double();
	test_extra_write_char();
	test_extra_write_double();
	test_wrapped_write_char();
	test_wrapped_write_double();
	// done
	printf("Testing complete.\n");
}
