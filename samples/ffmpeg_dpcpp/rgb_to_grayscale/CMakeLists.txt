# ==============================================================================
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.12)

set (TARGET_NAME "ffmpeg_dpcpp_rgb_to_grayscale")

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBAV libavformat libavcodec libswscale libavutil)
pkg_check_modules(LIBVA libva libva-drm)
pkg_check_modules(LEVEL_ZERO libze_loader)
find_package(gflags)
execute_process(COMMAND which icpx RESULT_VARIABLE status OUTPUT_VARIABLE out OUTPUT_QUIET)

if (status EQUAL 0 AND LIBAV_FOUND AND LIBVA_FOUND AND LEVEL_ZERO_FOUND AND gflags_FOUND)

set(CMAKE_CXX_COMPILER icpx)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsycl")
string(REPLACE "-fno-strict-overflow" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-debug-disables-optimization -Rno-debug-disables-optimization -Wno-unknown-warning-option")
endif()
#set(CMAKE_CXX_STANDARD 17)

add_executable(${TARGET_NAME} rgb_to_grayscale.cpp)

# use pkg-config if sample builds as standalone. Otherwise vars DLSTREAMER_INCLUDE_DIRS/etc set by top level cmake
if (${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${PROJECT_SOURCE_DIR})
    pkg_check_modules(DLSTREAMER dl-streamer REQUIRED)
else()
    target_include_directories(${TARGET_NAME}
            PRIVATE
            ${DLSTREAMER_BASE_DIR}/src/ffmpeg/_plugin
            ${DLSTREAMER_BASE_DIR}/src/vaapi/_plugin
            )
endif()

target_include_directories(${TARGET_NAME}
PRIVATE
        ${DLSTREAMER_INCLUDE_DIRS}
)

target_link_libraries(${TARGET_NAME}
PRIVATE
        ${LIBAV_LIBRARIES}
        ${LEVEL_ZERO_LIBRARIES}
        dlstreamer_ffmpeg
        dlstreamer_vaapi
        gflags
)

else()

if (${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${PROJECT_SOURCE_DIR})
    message(FATAL_ERROR "icpx compiler not found")
endif()

endif()
