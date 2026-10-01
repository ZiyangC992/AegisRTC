# Require at least CMake 3.20.
cmake_minimum_required(VERSION 3.20)

# Project metadata.
project(AegisRTC
    VERSION 0.1.0
    DESCRIPTION "Adaptive RTC engine for lossy networks"
    LANGUAGES CXX
)

# Require C++20.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build the executable.
add_executable(aegis_rtc
    app.cpp

    media/mf_video_capturer.cpp
    media/format_selector.cpp
    media/simulated_video_encoder.cpp
    media/ffmpeg_video_encoder.cpp
    media/video_encoder_factory.cpp
    media/video_capturer.cpp
    media/video_capturer_factory.cpp

    network/video_Packetizer.cpp
    network/network_simulator.cpp
    network/network_monitor.cpp
    network/wire_protocol.cpp
    network/frame_reassembler.cpp
    network/retransmission_cache.cpp
    
    engine/nack.cpp
    engine/adaptive_controller.cpp
    engine/session.cpp
    engine/session_reporter.cpp
)

set(FFMPEG_ROOT
    "D:/vcpkg/installed/x64-windows"
)

target_include_directories(aegis_rtc
    PRIVATE
        "${FFMPEG_ROOT}/include"
)

target_link_directories(aegis_rtc
    PRIVATE
        "${FFMPEG_ROOT}/lib"
)

target_link_libraries(aegis_rtc
    PRIVATE
        mf
        mfplat
        mfreadwrite
        mfuuid
        ole32
        avcodec
        avutil
        swscale
)

# Allow source files to include headers like:
# #include "aegis/media/mf_video_capturer.hpp"
target_include_directories(aegis_rtc
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Compiler warnings and source encoding.
if(MSVC)
    target_compile_options(aegis_rtc
        PRIVATE
            /W4
            /permissive-
            /utf-8
    )

    # Windows Media Foundation and COM libraries.
    target_link_libraries(aegis_rtc
        PRIVATE
            mf
            mfplat
            mfreadwrite
            mfuuid
            ole32
    )
else()
    message(FATAL_ERROR
        "This stage currently supports Windows with MSVC only."
    )
endif()

enable_testing()

add_executable(aegis_format_selector_test
    tests/format_selector_test.cpp
    media/format_selector.cpp
)

target_include_directories(aegis_format_selector_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(aegis_format_selector_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

add_test(
    NAME format_selector_test
    COMMAND aegis_format_selector_test
)

# Enable CTest support
enable_testing()

add_executable(aegis_network_simulator_test
    tests/network_simulator_test.cpp
    network/network_simulator.cpp
)

target_include_directories(aegis_network_simulator_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(aegis_network_simulator_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

add_test(
    NAME network_simulator_test
    COMMAND aegis_network_simulator_test
)

add_executable(aegis_wire_protocol_test
    tests/wire_protocol_test.cpp
    network/wire_protocol.cpp
)

target_include_directories(aegis_wire_protocol_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(aegis_wire_protocol_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

add_test(
    NAME wire_protocol_test
    COMMAND aegis_wire_protocol_test
)

add_executable(aegis_frame_reassembler_test
    tests/frame_reassembler_test.cpp
    network/frame_reassembler.cpp
    network/wire_protocol.cpp
)

target_include_directories(aegis_frame_reassembler_test 
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(
    aegis_frame_reassembler_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

add_test(
    NAME frame_reassembler_test
    COMMAND aegis_frame_reassembler_test
)


add_executable(aegis_nack_test
    tests/nack_test.cpp
    network/frame_reassembler.cpp
    engine/nack.cpp
)

target_include_directories(aegis_nack_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(aegis_nack_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

add_test(
    NAME nack_test
    COMMAND aegis_nack_test
)

add_executable(aegis_retransmission_cache_test
    tests/retransmissioncache_test.cpp
    network/retransmission_cache.cpp
)

target_include_directories(aegis_retransmission_cache_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(aegis_retransmission_cache_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

add_test(
    NAME retransmission_cache
    COMMAND aegis_retransmission_cache_test
)

add_executable(aegis_retransmission_e2e_test
    tests/retransmission_e2e_test.cpp
    network/retransmission_cache.cpp
    network/frame_reassembler.cpp
    network/wire_protocol.cpp
    network/video_packetizer.cpp
    engine/nack.cpp
    )

target_include_directories(aegis_retransmission_e2e_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(aegis_retransmission_e2e_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

add_test(
    NAME retransmission_e2e_test
    COMMAND aegis_retransmission_e2e_test
)

add_executable(aegis_network_monitor_test
    tests/network_monitor_test.cpp
    network/network_monitor.cpp
    network/network_simulator.cpp
)

target_include_directories(aegis_network_monitor_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(aegis_network_monitor_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

add_test(
    NAME network_monitor_test
    COMMAND aegis_network_monitor_test
)

add_executable(aegis_adaptive_controller_test
    tests/adaptive_controller_test.cpp
    engine/adaptive_controller.cpp
)

target_include_directories(aegis_adaptive_controller_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(aegis_adaptive_controller_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

add_test(
    NAME adaptive_controller_test
    COMMAND aegis_adaptive_controller_test
)

add_executable(aegis_ffmpeg_video_encoder_test
    tests/ffmpeg_video_encoder_test.cpp
    media/ffmpeg_video_encoder.cpp
)

target_include_directories(aegis_ffmpeg_video_encoder_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        D:/vcpkg/installed/x64-windows/include
)

target_link_directories(
    aegis_ffmpeg_video_encoder_test
    PRIVATE
        D:/vcpkg/installed/x64-windows/lib
)

target_link_libraries(
    aegis_ffmpeg_video_encoder_test
    PRIVATE
        mf
        mfplat
        mfreadwrite
        mfuuid
        ole32
        avcodec
        avutil
        swscale
)

set_target_properties(aegis_ffmpeg_video_encoder_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

add_test(
    NAME ffmpeg_video_encoder_test
    COMMAND aegis_ffmpeg_video_encoder_test
)

add_executable(aegis_video_encoder_factory_test
    tests/video_encoder_factory_test.cpp
    media/simulated_video_encoder.cpp
    media/video_encoder_factory.cpp
    media/ffmpeg_video_encoder.cpp
)

target_include_directories(
    aegis_video_encoder_factory_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        D:/vcpkg/installed/x64-windows/include
)

target_link_directories(
    aegis_video_encoder_factory_test
    PRIVATE
        D:/vcpkg/installed/x64-windows/lib
)

target_link_libraries(
    aegis_video_encoder_factory_test
    PRIVATE
        avcodec
        avutil
        swscale
)

set_target_properties(
    aegis_video_encoder_factory_test
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
)

add_test(
    NAME video_encoder_factory_test
    COMMAND aegis_video_encoder_factory_test
)