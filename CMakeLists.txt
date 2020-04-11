cmake_minimum_required(VERSION 3.12...3.15)

project(
	CaramelLights
	VERSION 1.0
	LANGUAGES C
)

### Require out-of-source builds
file(TO_CMAKE_PATH "${PROJECT_BINARY_DIR}/CMakeLists.txt" LOC_PATH)
if(EXISTS "${LOC_PATH}")
    message(FATAL_ERROR "You cannot build in a source directory (or any directory with a CMakeLists.txt file). Please make a build subdirectory. Feel free to remove CMakeCache.txt and CMakeFiles.")
endif()

add_subdirectory(src)
add_subdirectory(apps)
add_subdirectory(tests)
