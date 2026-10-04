cmake_minimum_required(VERSION 3.20)

project(AegisRTC
    VERSION 0.1.0
    DESCRIPTION "Adaptive RTC engine for lossy networks"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

# ============================================================
# Common source files
# ============================================================

set(AEGIS_COMMON_SOURCES
    engine/session.cpp
    engine/session_reporter.cpp
    engine/adaptive_controller.cpp
    engine/nack.cpp

    media/video_capturer.cpp
    media/video_capturer_factory.cpp
    media/video_encoder_factory.cpp
    media/simulated_video_encoder.cpp
    media/ffmpeg_video_encoder.cpp

    network/frame_reassembler.cpp
    network/network_monitor.cpp
    network/network_simulator.cpp
    network/retransmission_cache.cpp
    network/video_packetizer.cpp
    network/wire_protocol.cpp
)

# ============================================================
# Platform-specific source files
# ============================================================

if (MSVC)

    set(AEGIS_PLATFORM_SOURCES
        app.cpp
        media/mf_video_capturer.cpp
        network/udp_transport_windows.cpp
    )

elseif (UNIX)

    set(AEGIS_PLATFORM_SOURCES
        app_linux.cpp
        media/v4l2_video_capturer.cpp
        network/udp_transport.cpp
    )

else()

    message(FATAL_ERROR
        "Unsupported platform."
    )

endif()

# ============================================================
# Main executable
# ============================================================

add_executable(aegis_rtc
    ${AEGIS_COMMON_SOURCES}
    ${AEGIS_PLATFORM_SOURCES}
)

target_include_directories(aegis_rtc
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ============================================================
# Platform compiler and linker settings
# ============================================================

if (MSVC)

    target_compile_options(aegis_rtc
        PRIVATE
            /W4
            /permissive-
            /utf-8
    )

    target_include_directories(aegis_rtc
        PRIVATE
            D:/vcpkg/installed/x64-windows/include
    )

    target_link_directories(aegis_rtc
        PRIVATE
            D:/vcpkg/installed/x64-windows/lib
    )

    target_link_libraries(aegis_rtc
        PRIVATE
            Ws2_32

            mf
            mfplat
            mfreadwrite
            mfuuid
            ole32

            avcodec
            avformat
            avutil
            swscale
    )

elseif (UNIX)

    target_compile_options(aegis_rtc
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )

    target_link_libraries(aegis_rtc
        PRIVATE
            avcodec
            avformat
            avutil
            swscale
            v4l2
    )

endif()

# ============================================================
# Helper function for ordinary tests
# ============================================================

function(add_aegis_test TEST_TARGET TEST_NAME)

    add_executable(${TEST_TARGET}
        ${ARGN}
    )

    target_include_directories(${TEST_TARGET}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_compile_features(${TEST_TARGET}
        PRIVATE
            cxx_std_20
    )

    add_test(
        NAME ${TEST_NAME}
        COMMAND ${TEST_TARGET}
    )

endfunction()

# ============================================================
# Basic unit tests
# ============================================================

add_aegis_test(
    aegis_format_selector_test
    format_selector_test
    tests/format_selector_test.cpp
    media/format_selector.cpp
)

add_aegis_test(
    aegis_network_simulator_test
    network_simulator_test
    tests/network_simulator_test.cpp
    network/network_simulator.cpp
)

add_aegis_test(
    aegis_wire_protocol_test
    wire_protocol_test
    tests/wire_protocol_test.cpp
    network/wire_protocol.cpp
    network/video_packetizer.cpp
)

add_aegis_test(
    aegis_frame_reassembler_test
    frame_reassembler_test
    tests/frame_reassembler_test.cpp
    network/frame_reassembler.cpp
    network/wire_protocol.cpp
)

add_aegis_test(
    aegis_nack_test
    nack_test
    tests/nack_test.cpp
    network/frame_reassembler.cpp
    network/wire_protocol.cpp
    engine/nack.cpp
)

add_aegis_test(
    aegis_retransmission_cache_test
    retransmission_cache_test
    tests/retransmissioncache_test.cpp
    network/retransmission_cache.cpp
)

add_aegis_test(
    aegis_retransmission_e2e_test
    retransmission_e2e_test
    tests/retransmission_e2e_test.cpp
    network/retransmission_cache.cpp
    network/frame_reassembler.cpp
    network/wire_protocol.cpp
    network/video_packetizer.cpp
    engine/nack.cpp
)

add_aegis_test(
    aegis_network_monitor_test
    network_monitor_test
    tests/network_monitor_test.cpp
    network/network_monitor.cpp
    network/network_simulator.cpp
)

add_aegis_test(
    aegis_adaptive_controller_test
    adaptive_controller_test
    tests/adaptive_controller_test.cpp
    engine/adaptive_controller.cpp
)

# ============================================================
# UDP transport test
# ============================================================

if (UNIX)
add_aegis_test(
    aegis_udp_transport_test
    udp_transport_test
    tests/udp_transport_test.cpp
    network/udp_transport.cpp
)

add_executable(aegis_udp_transport_demo
    tests/udp_transport_demo.cpp
    network/udp_transport.cpp
)

target_include_directories(aegis_udp_transport_demo
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(aegis_udp_transport_demo
    PRIVATE
        cxx_std_20
)


endif()
# ============================================================
# Video encoder factory test
# ============================================================

add_executable(aegis_video_encoder_factory_test
    tests/video_encoder_factory_test.cpp
    media/simulated_video_encoder.cpp
    media/video_encoder_factory.cpp
    media/ffmpeg_video_encoder.cpp
)

target_include_directories(aegis_video_encoder_factory_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(aegis_video_encoder_factory_test
    PRIVATE
        cxx_std_20
)

if (MSVC)

    target_include_directories(aegis_video_encoder_factory_test
        PRIVATE
            D:/vcpkg/installed/x64-windows/include
    )

    target_link_directories(aegis_video_encoder_factory_test
        PRIVATE
            D:/vcpkg/installed/x64-windows/lib
    )

endif()

target_link_libraries(aegis_video_encoder_factory_test
    PRIVATE
        avcodec
        avutil
        swscale
)

add_test(
    NAME video_encoder_factory_test
    COMMAND aegis_video_encoder_factory_test
)

# ============================================================
# Windows-specific FFmpeg test
# ============================================================

if (MSVC)

    add_executable(aegis_ffmpeg_video_encoder_test
        tests/ffmpeg_video_encoder_test.cpp
        media/ffmpeg_video_encoder.cpp
    )

    target_include_directories(aegis_ffmpeg_video_encoder_test
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            D:/vcpkg/installed/x64-windows/include
    )

    target_link_directories(aegis_ffmpeg_video_encoder_test
        PRIVATE
            D:/vcpkg/installed/x64-windows/lib
    )

    target_link_libraries(aegis_ffmpeg_video_encoder_test
        PRIVATE
            mf
            mfplat
            mfreadwrite
            mfuuid
            ole32
            avcodec
            avformat
            avutil
            swscale
    )

    target_compile_features(aegis_ffmpeg_video_encoder_test
        PRIVATE
            cxx_std_20
    )

    add_test(
        NAME ffmpeg_video_encoder_test
        COMMAND aegis_ffmpeg_video_encoder_test
    )

endif()


