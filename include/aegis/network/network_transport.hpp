#pragma once

#include "aegis/common.hpp"

#include <cstdint>
#include <vector>

namespace aegis::network
{

// Statistics shared by simulated and real transports.
struct NetworkTransportStats
{
    // Number of packets accepted by Send().
    std::uint64_t packets_sent{0};

    // Number of packets dropped by the transport.
    std::uint64_t packets_dropped{0};

    // Total bytes accepted by Send().
    std::uint64_t bytes_sent{0};

    // Number of packets delivered to the application.
    std::uint64_t packets_delivered{0};

    // Total bytes delivered to the application.
    std::uint64_t bytes_delivered{0};

    // Sum of packet delivery latency.
    std::uint64_t total_latency_microseconds{0};

    // Number of latency samples.
    std::uint64_t latency_sample_count{0};

    // Sum of latency differences.
    std::uint64_t total_jitter_microseconds{0};

    // Number of jitter samples.
    std::uint64_t jitter_sample_count{0};

    // Previous packet latency.
    std::uint64_t previous_latency_microseconds{0};

    // Whether a previous latency sample exists.
    bool has_previous_latency{false};
};

// A raw packet delivered by a network transport.
struct ReceivedPacket
{
    // Serialized wire-protocol bytes.
    std::vector<std::uint8_t> wire_data;

    // Time when the packet becomes available.
    aegis::TimePoint receive_time;
};

// Common interface for all network transports.
class INetworkTransport
{
public:
    virtual ~INetworkTransport() = default;

    // Send one serialized wire packet.
    [[nodiscard]] virtual bool Send(
        std::vector<std::uint8_t> wire_data,
        aegis::TimePoint send_time
    ) = 0;

    // Receive all currently available packets.
    [[nodiscard]] virtual std::vector<ReceivedPacket>
    ReceiveReady(
        aegis::TimePoint now
    ) = 0;

    // Return transport statistics.
    [[nodiscard]] virtual const NetworkTransportStats&
    stats() const noexcept = 0;

    // Return the number of packets waiting internally.
    [[nodiscard]] virtual std::size_t
    queued_packets() const noexcept = 0;
};

} // namespace aegis::network
