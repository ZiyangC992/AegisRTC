#pragma once

#include "aegis/common.hpp"
#include "aegis/network/video_packetizer.hpp"

#include <chrono>
#include <vector>
#include <cstdint>
#include <set>
#include <random>


namespace aegis::network {

using aegis::Clock;
using aegis::TimePoint;

struct NetworkSimulatorConfig {

    std::uint64_t bandwidth_bps{1'500'000};

    std::chrono::milliseconds base_delay{30};

    std::chrono::milliseconds jitter{5};

    double random_loss_rate{0.0};

    std::uint32_t random_seed{20260902U};
};

struct ScheduledPacket {
    std::vector<std::uint8_t> wire_data;

    // Time when the packet entered the simulated network.
    TimePoint send_time;

    // Time when the packet becomes deliverable.
    TimePoint ready_time;
};

struct ScheduledPacketCompare {
    //Return true when left packet should appear earlier.
    bool operator()(
        const ScheduledPacket& left,
        const ScheduledPacket& right
    ) const noexcept {
        return left.ready_time < right.ready_time;
    }
};

struct NetworkSimulatorStats {
    //Number of packets accepted by Send().
    std::uint64_t packets_sent{0};

    //Number of packets dropped by loss simulation.
    std::uint64_t packets_dropped{0};

    //Total bytes accepted by Send().
    std::uint64_t bytes_sent{0};

    //Number of packets delivered by ReceiveReady().
    std::uint64_t packets_delivered{0};

    //Total bytes delivered to the receive.
    std::uint64_t bytes_delivered{0};

    // Sum of packet delivery latency in microseconds.
    std::uint64_t total_latency_microseconds{0};

    // Number of packets used for latency calculation.
    std::uint64_t latency_sample_count{0};

    // Sum of absolute latency differences in microseconds.
    std::uint64_t total_jitter_microseconds{0};

    // Number of jitter samples.
    std::uint64_t jitter_sample_count{0};

    // Previous packet latency.
    std::uint64_t previous_latency_microseconds{0};

    // Whether a previous latency sample exists.
    bool has_previous_latency{false};
};


class NetworkSimulator final {
public:
    explicit NetworkSimulator(
        NetworkSimulatorConfig config
    );

    [[nodiscard]] bool Send(
        std::vector<std::uint8_t> wire_data,
        TimePoint now
    );

    [[nodiscard]] std::vector<ScheduledPacket>
    ReceiveReady(TimePoint now);

    //Return a read-only view of network statistics.
    [[nodiscard]] const NetworkSimulatorStats&
    stats() const noexcept;

    //Return the number of packets waiting for delivery.
    [[nodiscard]] std::size_t
    queued_packets() const noexcept;

private:
    NetworkSimulatorConfig config_;

    //Earliest time when the link can send another packet.
    TimePoint next_send_available_time_{};

    //Ordered set containing packets waiting for delivery.
    std::multiset<
        ScheduledPacket,
        ScheduledPacketCompare
    > receive_queue_;
    

    //Pseudo-random generator used by the simulator.
    std::mt19937 random_engine_;

    //Generates a probability value in the range [0.0,1.0].
    std::uniform_real_distribution<double>
        loss_distribution_{0.0,1.0};
    
    //Generates a random jitter value in milliseconds.
    std::uniform_int_distribution<std::int64_t>
        jitter_distribution_;

    //Runtime statistics collected by the simulator.
    NetworkSimulatorStats stats_;
};


} //namespace aegis::network