#include "aegis/network/retransmission_cache.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

int main() {
    using namespace aegis::network;

    const aegis::TimePoint base_time{};

    // ==================================================
    // 1. Basic store and find test
    // ==================================================

    const RetransmissionCacheConfig cache_config{
        .max_packets = 2U,
        .max_packet_age = std::chrono::milliseconds{1000}
    };

    RetransmissionCache cache(cache_config);

    const std::vector<std::uint8_t> initial_wire_data{
        0x01U,
        0x02U,
        0x03U
    };

    cache.Store(
        100U,
        initial_wire_data,
        base_time
    );

    const auto initial_result =
        cache.Find(100U);

    assert(cache.Size() == 1U);
    assert(initial_result.has_value());
    assert(initial_result.value() == initial_wire_data);

    // ==================================================
    // 2. Find a missing sequence number
    // ==================================================

    const auto missing_result =
        cache.Find(999U);

    assert(!missing_result.has_value());

    // ==================================================
    // 3. Replace an existing sequence number
    // ==================================================

    const std::vector<std::uint8_t> replacement_wire_data{
        0xAAU,
        0xBBU
    };

    cache.Store(
        100U,
        replacement_wire_data,
        base_time +
            std::chrono::milliseconds{10}
    );

    const auto replacement_result =
        cache.Find(100U);

    // Replacing an existing packet must not increase the cache size.
    assert(cache.Size() == 1U);
    assert(replacement_result.has_value());
    assert(
        replacement_result.value() ==
        replacement_wire_data
    );

    // ==================================================
    // 4. Capacity eviction test
    // ==================================================

    const std::vector<std::uint8_t> packet_101_data{
        0x22U
    };

    const std::vector<std::uint8_t> packet_102_data{
        0x33U
    };

    cache.Store(
        101U,
        packet_101_data,
        base_time +
            std::chrono::milliseconds{20}
    );

    cache.Store(
        102U,
        packet_102_data,
        base_time +
            std::chrono::milliseconds{30}
    );

    // The cache capacity is two packets.
    assert(cache.Size() == 2U);

    // Sequence 100 was the oldest cached packet.
    const auto evicted_packet_result =
        cache.Find(100U);

    const auto retained_packet_101_result =
        cache.Find(101U);

    const auto retained_packet_102_result =
        cache.Find(102U);

    assert(!evicted_packet_result.has_value());
    assert(retained_packet_101_result.has_value());
    assert(retained_packet_102_result.has_value());

    // ==================================================
    // 5. Expiration test
    // ==================================================

    const RetransmissionCacheConfig expiration_config{
        .max_packets = 10U,
        .max_packet_age = std::chrono::milliseconds{100}
    };

    RetransmissionCache expiration_cache(
        expiration_config
    );

    const std::vector<std::uint8_t> packet_200_data{
        0x11U
    };

    const std::vector<std::uint8_t> packet_201_data{
        0x22U
    };

    expiration_cache.Store(
        200U,
        packet_200_data,
        base_time
    );

    expiration_cache.Store(
        201U,
        packet_201_data,
        base_time +
            std::chrono::milliseconds{50}
    );

    const aegis::TimePoint before_expiration =
        base_time +
        std::chrono::milliseconds{80};

    const std::size_t expired_before_deadline =
        expiration_cache.Expire(
            before_expiration
        );

    // Packet 200 is 80 ms old.
    // Packet 201 is 30 ms old.
    assert(expired_before_deadline == 0U);
    assert(expiration_cache.Size() == 2U);

    const aegis::TimePoint after_expiration =
        base_time +
        std::chrono::milliseconds{150};

    const std::size_t expired_after_deadline =
        expiration_cache.Expire(
            after_expiration
        );

    // Both packets are at least 100 ms old at this point:
    // packet 200: 150 ms
    // packet 201: 100 ms
    assert(expired_after_deadline == 2U);
    assert(expiration_cache.Size() == 0U);

    std::cout
        << "Retransmission cache tests passed.\n";

    return 0;
}