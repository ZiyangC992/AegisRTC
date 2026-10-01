#include "aegis/network/frame_reassembler.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>
#include <chrono>

int main()
{
    using namespace aegis::network;

    const std::uint32_t frame_number = 10U;
    const std::int64_t timestamp = 1000LL;

    const ReassemblerTimePoint base_time{};

    const VideoPacketFragment fragment_100{
        .sequence_number = 100U,
        .frame_number = frame_number,
        .timestamp_100ns = timestamp,
        .key_frame = true,
        .start_of_frame = true,
        .end_of_frame = false,
        .payload = {
            0x01U,
            0x02U}};

    const VideoPacketFragment fragment_101{
        .sequence_number = 101U,
        .frame_number = frame_number,
        .timestamp_100ns = timestamp,
        .key_frame = true,
        .start_of_frame = false,
        .end_of_frame = false,
        .payload = {
            0x03U,
            0x04U}};

    const VideoPacketFragment fragment_102{
        .sequence_number = 102U,
        .frame_number = frame_number,
        .timestamp_100ns = timestamp,
        .key_frame = true,
        .start_of_frame = false,
        .end_of_frame = true,
        .payload = {
            0x05U,
            0x06U}};

    const WirePacket packet_100 =
        BuildWirePacket(
            fragment_100);

    const WirePacket packet_101 =
        BuildWirePacket(
            fragment_101);

    const WirePacket packet_102 =
        BuildWirePacket(
            fragment_102);

    FrameReassembler reassembler;

    const auto result_100 =
        reassembler.Push(
            packet_100,
            base_time);

    assert(!result_100.has_value());

    const auto result_101 =
        reassembler.Push(
            packet_101,
            base_time + std::chrono::milliseconds{10});

    assert(!result_101.has_value());

    const auto result_102 =
        reassembler.Push(
            packet_102,
            base_time + std::chrono::milliseconds{20});

    assert(result_102.has_value());

    assert(
        result_102->frame_number ==
        frame_number);

    assert(
        result_102->timestamp_100ns ==
        timestamp);

    assert(
        result_102->key_frame);

    const std::vector<std::uint8_t>
        expected_payload = {
            0x01U,
            0x02U,
            0x03U,
            0x04U,
            0x05U,
            0x06U};

    assert(
        result_102->payload ==
        expected_payload);

    FrameReassembler out_of_order_reassembler;

    const auto out_of_order_result_101 =
        out_of_order_reassembler.Push(
            packet_101,
            base_time);

    assert(
        !out_of_order_result_101.has_value());

    const auto out_of_order_result_100 =
        out_of_order_reassembler.Push(
            packet_100,
            base_time + std::chrono::milliseconds{10});

    assert(
        !out_of_order_result_100.has_value());

    const auto out_of_order_result_102 =
        out_of_order_reassembler.Push(
            packet_102,
            base_time + std::chrono::milliseconds{20});

    assert(
        out_of_order_result_102.has_value());

    assert(
        out_of_order_result_102->payload ==
        expected_payload);

    FrameReassembler missing_reassembler;

    const auto missing_result_100 =
        missing_reassembler.Push(
            packet_100,
            base_time);

    assert(!missing_result_100.has_value());

    const auto missing_result_102 =
        missing_reassembler.Push(
            packet_102,
            base_time + std::chrono::milliseconds{15});

    assert(!missing_result_102.has_value());

    FrameReassembler duplicate_reassembler;

    const auto duplicate_first_result =
        duplicate_reassembler.Push(
            packet_100,
            base_time);

    assert(!duplicate_first_result.has_value());

    const auto duplicate_second_result =
        duplicate_reassembler.Push(
            packet_100,
            base_time + std::chrono::milliseconds{5});

    assert(!duplicate_second_result.has_value());

    const auto duplicate_third_result =
        duplicate_reassembler.Push(
            packet_101,
            base_time + std::chrono::milliseconds{10});

    assert(!duplicate_third_result.has_value());

    const auto duplicate_final_result =
        duplicate_reassembler.Push(
            packet_102,
            base_time + std::chrono::milliseconds{15});

    assert(duplicate_final_result.has_value());

    assert(duplicate_final_result->payload ==
           expected_payload);

    FrameReassembler wrap_reassembler;

    const VideoPacketFragment wrap_fragment_65534{
        .sequence_number = 65534U,
        .frame_number = 20U,
        .timestamp_100ns = 2000LL,
        .key_frame = false,
        .start_of_frame = true,
        .end_of_frame = false,
        .payload = {
            0x10U}};

    const VideoPacketFragment wrap_fragment_65535{
        .sequence_number = 65535U,
        .frame_number = 20U,
        .timestamp_100ns = 2000LL,
        .key_frame = false,
        .start_of_frame = false,
        .end_of_frame = false,
        .payload = {
            0x20U}};

    const VideoPacketFragment wrap_fragment_0{
        .sequence_number = 0U,
        .frame_number = 20U,
        .timestamp_100ns = 2000LL,
        .key_frame = false,
        .start_of_frame = false,
        .end_of_frame = true,
        .payload = {
            0x30U}};

    const auto wrap_packet_65534 =
        BuildWirePacket(wrap_fragment_65534);

    const auto wrap_packet_65535 =
        BuildWirePacket(wrap_fragment_65535);

    const auto wrap_packet_0 =
        BuildWirePacket(wrap_fragment_0);

    const auto wrap_result_65534 =
        wrap_reassembler.Push(
            wrap_packet_65534,
            base_time + std::chrono::milliseconds{10});

    assert(
        !wrap_result_65534.has_value());

    const auto wrap_result_65535 =
        wrap_reassembler.Push(
            wrap_packet_65535,
            base_time + std::chrono::milliseconds{20});

    assert(
        !wrap_result_65535.has_value());

    const auto wrap_result_0 =
        wrap_reassembler.Push(
            wrap_packet_0,
            base_time + std::chrono::milliseconds{30});

    assert(
        wrap_result_0.has_value());

    const std::vector<std::uint8_t>
        expected_wrap_payload = {
            0x10U,
            0x20U,
            0x30U};

    assert(
        wrap_result_0->payload ==
        expected_wrap_payload);

    assert(
        wrap_result_0->frame_number ==
        20U);

    assert(
        wrap_result_0->timestamp_100ns ==
        2000LL);

    assert(
        !wrap_result_0->key_frame);

    FrameReassembler late_start_reassembler;

    const auto late_start_result_101 =
        late_start_reassembler.Push(
            packet_101,
            base_time + std::chrono::milliseconds{10});

    assert(
        !late_start_result_101.has_value());

    const auto late_start_result_102 =
        late_start_reassembler.Push(
            packet_102,
            base_time + std::chrono::milliseconds{20});

    assert(
        !late_start_result_102.has_value());

    const auto late_start_result_100 =
        late_start_reassembler.Push(
            packet_100,
            base_time + std::chrono::milliseconds{30});

    assert(
        late_start_result_100.has_value());

    assert(
        late_start_result_100->payload ==
        expected_payload);

    const FrameReassemblerConfig limited_config{
        .max_frame_age = std::chrono::milliseconds{500},
        .max_in_flight_frames = 2U};

    const VideoPacketFragment frame_1_fragment{
        .sequence_number = 200U,
        .frame_number = 1U,
        .timestamp_100ns = 1000LL,
        .key_frame = false,
        .start_of_frame = true,
        .end_of_frame = false,
        .payload = {0x11U}};

    const VideoPacketFragment frame_2_fragment{
        .sequence_number = 201U,
        .frame_number = 2U,
        .timestamp_100ns = 2000LL,
        .key_frame = false,
        .start_of_frame = true,
        .end_of_frame = false,
        .payload = {0x22U}};

    const VideoPacketFragment frame_3_fragment{
        .sequence_number = 202U,
        .frame_number = 3U,
        .timestamp_100ns = 3000LL,
        .key_frame = false,
        .start_of_frame = true,
        .end_of_frame = false,
        .payload = {0x33U}};

    const WirePacket frame_1_packet =
        BuildWirePacket(frame_1_fragment);

    const WirePacket frame_2_packet =
        BuildWirePacket(frame_2_fragment);

    const WirePacket frame_3_packet =
        BuildWirePacket(frame_3_fragment);

    FrameReassembler limited_reassembler{
        limited_config};

    // Store frame 1.
    const auto frame_1_result =
        limited_reassembler.Push(
            frame_1_packet,
            base_time);

    assert(!frame_1_result.has_value());

    // Store frame 2.
    const auto frame_2_result =
        limited_reassembler.Push(
            frame_2_packet,
            base_time);

    assert(!frame_2_result.has_value());

    // Frame 3 should be rejected because the cache is full.
    const auto frame_3_result =
        limited_reassembler.Push(
            frame_3_packet,
            base_time);

    assert(!frame_3_result.has_value());

    std::wcout
        << L"Frame reassembler ordered test passed.\n";

    return 0;
}