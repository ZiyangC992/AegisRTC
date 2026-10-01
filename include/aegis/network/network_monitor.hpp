#pragma once

#include <cstdint>
#include <cstddef>

namespace aegis::network {

struct NetworkQualitySnapshot {

    double packet_loss_rate{0.0};

    double delivery_rate{0.0};

    double average_latency_ms{0.0};

    std::uint64_t queued_packets{0U};

    std::uint64_t packets_sent{0U};

    std::uint64_t packets_delivered{0U};

    double retransmission_rate{0.0};
    double retransmission_cache_hit_rate{0.0};

    std::uint64_t retransmission_packets{0U};
    std::uint64_t retransmission_cache_hits{0U};
    std::uint64_t retransmission_cache_misses{0U};

    std::int64_t queue_delta{0};
    bool queue_is_growing{false};
    double jitter_ms{0.0};
};

struct NetworkSimulatorStats;

class NetworkMonitor final {
public:
    //Calculate a quality snapshot from network statistics.
    [[nodiscard]] NetworkQualitySnapshot Update(
    const NetworkSimulatorStats& stats,
    std::size_t queued_packets,
    std::uint64_t retransmission_packets,
    std::uint64_t retransmission_cache_hits,
    std::uint64_t retransmission_cache_misses
) noexcept;

private:
    bool has_previous_queue_size_{false};
    std::size_t previous_queue_size_{0U};
};
}

