#include "aegis/network/network_monitor.hpp"
#include "aegis/network/network_transport.hpp"

#include <cstdint>
#include <cstddef>

namespace aegis::network {

[[nodiscard]] NetworkQualitySnapshot NetworkMonitor::Update(
    const NetworkTransportStats& stats,
    std::size_t queued_packets,
    std::uint64_t retransmission_packets,
    std::uint64_t retransmission_cache_hits,
    std::uint64_t retransmission_cache_misses
) noexcept {

    // Copy basic packet counters.
    NetworkQualitySnapshot snapshot{
        .queued_packets = static_cast<std::uint64_t>(
            queued_packets),

        .packets_sent = stats.packets_sent,

        .packets_delivered = stats.packets_delivered,

        .retransmission_packets =
                retransmission_packets,

        .retransmission_cache_hits =
                retransmission_cache_hits,

        .retransmission_cache_misses =
                retransmission_cache_misses
    };
    if (!has_previous_queue_size_) {

        //This is the first Update() call
        snapshot.queue_delta = 0;

    } else {

        const std::int64_t current_queue = 
            static_cast<std::int64_t>(
                queued_packets
            );

        const std::int64_t previous_queue = 
            static_cast<std::int64_t>(
                previous_queue_size_
            );

        snapshot.queue_delta = 
            current_queue - 
            previous_queue;

    }

    snapshot.queue_is_growing =
        snapshot.queue_delta > 0
            ? true
            : false;

    previous_queue_size_ = queued_packets;
    has_previous_queue_size_ = true;

    if (stats.packets_sent > 0U)
    {
        snapshot.packet_loss_rate =
            static_cast<double>(
                stats.packets_dropped) /
            static_cast<double>(
                stats.packets_sent);

        snapshot.delivery_rate =
            static_cast<double>(
                stats.packets_delivered) /
            static_cast<double>(
                stats.packets_sent);
    }

    if (stats.packets_delivered > 0) {
        snapshot.retransmission_rate = 
            static_cast<double>(
                snapshot.retransmission_packets
            ) /
            static_cast<double>(
                stats.packets_delivered
            );
    } else {
        snapshot.retransmission_rate = 0.0;
    }

    const std::uint64_t total_cache_requests = 
        snapshot.retransmission_cache_hits +
        snapshot.retransmission_cache_misses;

    if (total_cache_requests > 0U) {
        snapshot.retransmission_cache_hit_rate = 
            static_cast<double>(
                snapshot.retransmission_cache_hits
            ) /
            static_cast<double>(
                total_cache_requests
            );
    } else {
        snapshot.retransmission_cache_hit_rate = 0.0;
    }

    // Calculate average delivery latency in milliseconds.
    if (stats.latency_sample_count > 0U)
    {
        const double average_latency_microseconds =
            static_cast<double>(
                stats.total_latency_microseconds) /
            static_cast<double>(
                stats.latency_sample_count);

        snapshot.average_latency_ms =
            average_latency_microseconds / 1000.0;
    }
    else
    {
        snapshot.average_latency_ms = 0.0;
    }

    if (stats.jitter_sample_count > 0U)
    {
        const double average_jitter_microseconds =
            static_cast<double>(
                stats.total_jitter_microseconds) /
            static_cast<double>(
                stats.jitter_sample_count);

        snapshot.jitter_ms =
            average_jitter_microseconds / 1000.0;
    }
    else
    {
        snapshot.jitter_ms = 0.0;
    }

    return snapshot;
};

} //namespace aegis::network

