#pragma once

#include "aegis/common.hpp"
#include "aegis/network/video_packetizer.hpp"
#include "aegis/network/network_transport.hpp"

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

using NetworkSimulatorStats = 
    NetworkTransportStats;


class NetworkSimulator final
  : public INetworkTransport {
public:
    explicit NetworkSimulator(
        NetworkSimulatorConfig config
    );

    [[nodiscard]] bool Send(
        std::vector<std::uint8_t> wire_data,
        TimePoint now
    ) override;

    [[nodiscard]] std::vector<ReceivedPacket>
    ReceiveReady(TimePoint now) override;

    //Return a read-only view of network statistics.
    [[nodiscard]] const NetworkTransportStats&
    stats() const noexcept override;

    //Return the number of packets waiting for delivery.
    [[nodiscard]] std::size_t
    queued_packets() const noexcept override;

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
    NetworkTransportStats stats_;
};


} //namespace aegis::network
