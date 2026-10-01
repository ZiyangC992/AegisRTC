#include "aegis/network/retransmission_cache.hpp"
#include "aegis/network/frame_reassembler.hpp"
#include "aegis/engine/nack.hpp"
#include "aegis/network/video_packetizer.hpp"
#include "aegis/network/wire_protocol.hpp"

#include <cassert>
#include <cstdint>
#include <chrono>
#include <iostream>

int main()
{
    using namespace aegis::network;
    using namespace aegis::engine;
    using namespace aegis::media;

    const std::uint32_t frame_number = 1U;
    const std::int64_t timestamp = 1'000LL;

    const VideoPacketFragment fragment_0{
        .sequence_number = 100U,
        .frame_number = frame_number,
        .timestamp_100ns = timestamp,
        .key_frame = false,
        .start_of_frame = true,
        .end_of_frame = false,
        .payload = {
            0x01U,
            0x02U}};

    const VideoPacketFragment fragment_1{
        .sequence_number = 101U,
        .frame_number = frame_number,
        .timestamp_100ns = timestamp,
        .key_frame = false,
        .start_of_frame = false,
        .end_of_frame = false,
        .payload = {
            0x03U,
            0x04U}};

    const VideoPacketFragment fragment_2{
        .sequence_number = 102U,
        .frame_number = frame_number,
        .timestamp_100ns = timestamp,
        .key_frame = false,
        .start_of_frame = false,
        .end_of_frame = true,
        .payload = {
            0x05U,
            0x06U}};

    FrameReassembler reassembler;

    NackController nack_controller;

    const TimePoint base_time{};

    const RetransmissionCacheConfig cache_config{
        .max_packets = 2048U,
        .max_packet_age = std::chrono::milliseconds{10'000}};

    RetransmissionCache
        retransmission_cache(cache_config);

    const WirePacket wire_data_0 = BuildWirePacket(fragment_0);
    const WirePacket wire_data_1 = BuildWirePacket(fragment_1);
    const WirePacket wire_data_2 = BuildWirePacket(fragment_2);

    std::vector<std::uint8_t> wire_packet_0;
    std::vector<std::uint8_t> wire_packet_1;
    std::vector<std::uint8_t> wire_packet_2;

    const bool serialized_0 = SerializeWirePacket(
        wire_data_0,
        wire_packet_0);

    if (serialized_0)
    {
        auto cached_wire_packet_0 = wire_packet_0;

        retransmission_cache.Store(
            wire_data_0.header.sequence_number,
            std::move(cached_wire_packet_0),
            base_time);
    }

    const bool serialized_1 = SerializeWirePacket(
        wire_data_1,
        wire_packet_1);

    if (serialized_1)
    {
        auto cached_wire_packet_1 = wire_packet_1;

        retransmission_cache.Store(
            wire_data_1.header.sequence_number,
            std::move(cached_wire_packet_1),
            base_time);
    }

    const bool serialized_2 = SerializeWirePacket(
        wire_data_2,
        wire_packet_2);

    if (serialized_2)
    {
        auto cached_wire_packet_2 = wire_packet_2;

        retransmission_cache.Store(
            wire_data_2.header.sequence_number,
            std::move(cached_wire_packet_2),
            base_time);
    }

    const auto result_0 =
        reassembler.Push(wire_data_0, base_time);

    assert(!result_0.has_value());

    const auto result_2 =
        reassembler.Push(wire_data_2, base_time);

    assert(!result_2.has_value());

    nack_controller.ObserveGap(
        wire_data_0.header.sequence_number,
        wire_data_2.header.sequence_number,
        base_time);

    const auto nack_sequence_numbers =
        nack_controller.Poll(
            base_time +
            std::chrono::milliseconds{20});
    assert(nack_sequence_numbers.size() == 1U);
    assert(nack_sequence_numbers[0] == 101U);

    const auto requested_sequence_number =
        nack_sequence_numbers[0];

    auto cached_wire_data_1 =
        retransmission_cache.Find(
            requested_sequence_number);

    assert(cached_wire_data_1.has_value());

    WirePacket original_packet_1{};

    const bool deserialized = DeserializeWirePacket(
        *cached_wire_data_1,
        original_packet_1);

    assert(deserialized);

    const auto original_flags =
        static_cast<WirePacketFlag>(
            original_packet_1.header.flags);

    assert(!HasFlag(
        original_flags,
        WirePacketFlag::kRetransmission));

    original_packet_1.header.flags =
        static_cast<std::uint8_t>(
            original_flags | WirePacketFlag::kRetransmission);

    WirePacket decoded_retransmission{};
    std::vector<std::uint8_t> serialized_retransmission_packet_1;

    const auto serialized_retransmission_1 =
        SerializeWirePacket(
            original_packet_1,
            serialized_retransmission_packet_1);

    assert(serialized_retransmission_1);

    const auto deserialized_retransmission_1 =
        DeserializeWirePacket(
            serialized_retransmission_packet_1,
            decoded_retransmission);

    assert(deserialized_retransmission_1);

    assert(
        decoded_retransmission.payload ==
        original_packet_1.payload);

    assert(
        decoded_retransmission.header.sequence_number ==
        requested_sequence_number);

    const auto retransmission_flags =
        static_cast<WirePacketFlag>(
            decoded_retransmission.header.flags);

    assert(HasFlag(
        retransmission_flags,
        WirePacketFlag::kRetransmission));

    nack_controller.OnPacketRecovered(
        requested_sequence_number);

    assert(
        HasFlag(
            static_cast<WirePacketFlag>(
                wire_data_0.header.flags),
            WirePacketFlag::kStartOfFrame));

    const auto result_1 = reassembler.Push(
        decoded_retransmission,
        base_time);

    const std::vector<std::uint8_t> payload_result = {
        0x01U,
        0x02U,
        0x03U,
        0x04U,
        0x05U,
        0x06U};

    assert(result_1.has_value());
    assert(result_1->frame_number == 1U);
    assert(result_1->key_frame == false);
    assert(result_1->timestamp_100ns == 1'000LL);
    assert(result_1->payload == payload_result);

    assert(!result_2.has_value());

    const std::uint16_t missing_sequence_number = 999U;

    const auto missing_cache_data =
        retransmission_cache.Find(
            missing_sequence_number);

    assert(!missing_cache_data.has_value());

    const NackConfig retry_config{
        .reorder_wait = std::chrono::milliseconds{20},
        .retry_interval = std::chrono::milliseconds{50},
        .max_missing_age = std::chrono::milliseconds{500},
        .max_retries = 2U,
        .max_tracked_missing_packets = 16U,
        .max_nack_batch_size = 16U};

    NackController retry_controller{
        retry_config};

    const TimePoint retry_base_time{};

    retry_controller.ObserveGap(
        100U,
        102U,
        retry_base_time);

    const auto first_nack =
        retry_controller.Poll(
            retry_base_time +
            std::chrono::milliseconds{20});

    assert(first_nack.size() == 1U);
    assert(first_nack[0] == 101U);

    const auto too_early_nack =
        retry_controller.Poll(
            retry_base_time +
            std::chrono::microseconds{40});

    assert(too_early_nack.empty());

    const auto second_nack =
        retry_controller.Poll(
            retry_base_time +
            std::chrono::milliseconds{80});

    assert(second_nack.size() == 1U);
    assert(second_nack[0] == 101U);

    const auto exhausted_nack =
        retry_controller.Poll(
            retry_base_time +
            std::chrono::milliseconds{130});

    assert(exhausted_nack.empty());

    const NackConfig batch_config{
        .reorder_wait = std::chrono::milliseconds{20},
        .retry_interval = std::chrono::milliseconds{50},
        .max_missing_age = std::chrono::milliseconds{500},
        .max_retries = 3U,
        .max_tracked_missing_packets = 32U,
        .max_nack_batch_size = 16U};

    NackController batch_controller{
        batch_config};

    const TimePoint batch_base_time{};

    batch_controller.ObserveGap(
        100U,
        104U,
        batch_base_time);

    const auto batch_nacks =
        batch_controller.Poll(
            batch_base_time +
            std::chrono::milliseconds{20});

    assert(batch_nacks.size() == 3U);
    assert(batch_nacks[0] == 101U);
    assert(batch_nacks[1] == 102U);
    assert(batch_nacks[2] == 103U);

    batch_controller.OnPacketRecovered(102U);
    const auto remaining_nacks =
        batch_controller.Poll(
            batch_base_time +
            std::chrono::milliseconds{70});

    assert(remaining_nacks.size() == 2U);
    assert(remaining_nacks[0] == 101U);
    assert(remaining_nacks[1] == 103U);

    const NackConfig wrap_config{
        .reorder_wait = std::chrono::milliseconds{20},
        .retry_interval = std::chrono::milliseconds{50},
        .max_missing_age = std::chrono::milliseconds{500},
        .max_retries = 3U,
        .max_tracked_missing_packets = 16U,
        .max_nack_batch_size = 16U};

    NackController wrap_controller{
        wrap_config};

    const TimePoint wrap_base_time{};

    wrap_controller.ObserveGap(
        65534U,
        1U,
        wrap_base_time);

    const auto wrap_nacks =
        wrap_controller.Poll(
            wrap_base_time +
            std::chrono::milliseconds{20});

    assert(wrap_nacks.size() == 2U);
    assert(wrap_nacks[1] == 65535U);  //std::map<std::uint16_t, MissingPacket>
    assert(wrap_nacks[0] == 0U);

    wrap_controller.OnPacketRecovered(0U);

    const auto remaining_wrap_nacks =
        wrap_controller.Poll(
            wrap_base_time +
            std::chrono::milliseconds{70});

    assert(remaining_wrap_nacks.size() == 1U);
    assert(remaining_wrap_nacks[0] == 65535U);
    return 0;
}
