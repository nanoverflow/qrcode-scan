cmake_minimum_required(VERSION 3.21)
project(cimbar_qt_sender LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Avoid building libcimbar unit tests while we pull in its libraries.
set(DISABLE_TESTS TRUE CACHE BOOL "" FORCE)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)

set(LIBCIMBAR_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../libcimbar")
if(NOT EXISTS "${LIBCIMBAR_ROOT}/src/lib/encoder/Encoder.h")
	message(FATAL_ERROR "libcimbar not found at ${LIBCIMBAR_ROOT}")
endif()

add_definitions("-DLIBCIMBAR_PROJECT_ROOT=\"${LIBCIMBAR_ROOT}\"")

if(MSVC)
	add_compile_options(/FI "iso646.h" /utf-8 /EHsc)
	# MSVC LTO + mixed C/C++ third-party libs is brittle; keep it off for the sender.
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
endif()

include_directories(
	${LIBCIMBAR_ROOT}/src/lib
	${LIBCIMBAR_ROOT}/src/third_party_lib
	${OpenCV_INCLUDE_DIRS}
)

# Encode path only — no GLFW / OpenGL window stack.
set(CIMBAR_LIB_PROJECTS
	${LIBCIMBAR_ROOT}/src/lib/bit_file
	${LIBCIMBAR_ROOT}/src/lib/cimb_translator
	${LIBCIMBAR_ROOT}/src/lib/compression
	${LIBCIMBAR_ROOT}/src/lib/encoder
	${LIBCIMBAR_ROOT}/src/lib/fountain
	${LIBCIMBAR_ROOT}/src/lib/serialize
	${LIBCIMBAR_ROOT}/src/lib/util
	${LIBCIMBAR_ROOT}/src/third_party_lib/libcorrect
	${LIBCIMBAR_ROOT}/src/third_party_lib/libpopcnt
	${LIBCIMBAR_ROOT}/src/third_party_lib/wirehair
	${LIBCIMBAR_ROOT}/src/third_party_lib/zstd
)

foreach(proj ${CIMBAR_LIB_PROJECTS})
	get_filename_component(proj_name "${proj}" NAME)
	add_subdirectory("${proj}" "${CMAKE_BINARY_DIR}/cimbar_${proj_name}")
endforeach()

add_executable(cimbar_qt_sender
	src/main.cpp
	src/MainWindow.cpp
	src/MainWindow.h
	src/CimbarEncoderSession.cpp
	src/CimbarEncoderSession.h
	src/FrameCanvas.cpp
	src/FrameCanvas.h
)

target_link_libraries(cimbar_qt_sender PRIVATE
	Qt6::Widgets
	${OpenCV_LIBS}
	cimb_translator
	wirehair
	zstd
	correct_static
)

if(WIN32)
	add_custom_command(TARGET cimbar_qt_sender POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E echo "Deploy Qt runtime with windeployqt after build if needed."
	)
endif()

install(TARGETS cimbar_qt_sender RUNTIME DESTINATION bin)
