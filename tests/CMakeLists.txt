add_executable(test-ring-buffer test-ring-buffer.c)
target_link_libraries(test-ring-buffer ring_buffer log_helper)

add_executable(test-audio test-audio.c)
target_link_libraries(test-audio audio_tap log_helper)
