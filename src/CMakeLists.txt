set(
	HEADER_DIR
	CONFIGURE_DEPENDS
	"${CaramelLights_SOURCE_DIR}/include/caramellights/"
)

# Use pkgconfig to find OpenSSL, as it's more robust on Apple systems
find_package(PkgConfig REQUIRED)

if(APPLE)
	# On macOS, use Homebrew if it's available to find installed libraries
	# On macOS, try to use Homebrewed OpenSSL as Apple doesn't include the
	# headers anymore.
	set(ENV{PKG_CONFIG_PATH} "/usr/local/lib/pkgconfig")
	if(EXISTS "/usr/local/opt/openssl@1.1")
		#set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl@1.1" PARENT_SCOPE)
		set(ENV{PKG_CONFIG_PATH}
			"/usr/local/opt/openssl@1.1/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
	elseif(EXISTS "/usr/local/opt/openssl")
		set(ENV{PKG_CONFIG_PATH}
			"/usr/local/opt/openssl/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
		#set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl" PARENT_SCOPE)
	endif()
	message("PKG_CONFIG_PATH: $ENV{PKG_CONFIG_PATH}")
	message("Using OpenSSL dir ${OPENSSL_ROOT_DIR}")
	#set(OPENSSL_USE_STATIC_LIBS TRUE)
endif()
#find_package(OpenSSL REQUIRED COMPONENTS SSL Crypto)
pkg_search_module(OpenSSL REQUIRED IMPORTED_TARGET openssl>=1.1)

#pkg_search_module(
#OPENSSL
#REQUIRED
#IMPORTED_TARGET
#openssl>=1.1
#)


# log helper
add_library(log_helper INTERFACE)
target_include_directories(
	log_helper
	INTERFACE ${HEADER_DIR}
)
target_include_directories(
	log_helper
	SYSTEM
	INTERFACE ${OpenSSL_INCLUDE_DIRS}
)

# Helper for setting link flags for frameworks
macro(link_framework LINK_TARGET FRAMEWORK_NAME)
	find_library("${FRAMEWORK_NAME}_FRAMEWORK_PATH" "${FRAMEWORK_NAME}")
	if(${FRAMEWORK_NAME}_FRAMEWORK_PATH)
		target_link_libraries(
			"${LINK_TARGET}"
			"${${FRAMEWORK_NAME}_FRAMEWORK_PATH}"
		)
	else()
		message(FATAL_ERROR "Unable to find framework ${FRAMEWORK_NAME}.")
	endif()
endmacro()

if(APPLE)
	add_library(audio_tap_apple STATIC audio-tap-apple.c)
	link_framework(audio_tap_apple CoreAudio)
	link_framework(audio_tap_apple Foundation)
	link_framework(audio_tap_apple AudioToolbox)
	link_framework(audio_tap_apple Accelerate)
	target_include_directories(audio_tap_apple PRIVATE "${HEADER_DIR}")
	target_link_libraries(audio_tap_apple log_helper)
	add_library(audio_tap ALIAS audio_tap_apple)

	message("OpenSSL include dirs: ${OPENSSL_INCLUDE_DIR}")
	message("OpenSSL libs: ${OPENSSL_LIBRARIES}")
	message("OpenSSL lib dirs: ${OPENSSL_LIBRARY_DIRS}")
else()
	# Just assume linux
	message(FATAL_ERROR "Linux audio tap not implemented yet")
endif()

# hue-dtls
add_library(hue_dtls STATIC hue-dtls.c)
target_include_directories(
	hue_dtls
	PRIVATE ${HEADER_DIR}
)
target_include_directories(
	hue_dtls
	SYSTEM
	PUBLIC ${OpenSSL_INCLUDE_DIRS}
)
target_link_libraries(
	hue_dtls
	PRIVATE log_helper
	PUBLIC ${OpenSSL_LINK_LIBRARIES}
)
target_link_options(
	hue_dtls
	PUBLIC "LINKER:${OpenSSL_LDFLAGS}"
	PUBLIC "LINKER:${OpenSSL_LDFLAGS_OTHER}"
)

# ring-buffer
add_library(ring_buffer STATIC ring-buffer.c)
target_link_libraries(ring_buffer log_helper)
target_include_directories(ring_buffer PRIVATE ${HEADER_DIR})
