#include "aegis/network/network_simulator.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>
#include <utility>

int main() {
    //Configure a deterministic network with 50ms delay.
    const aegis::network::NetworkSimulatorConfig config{
        .bandwidth_bps = 1'000'000'000U,
        .base_delay = std::chrono::milliseconds{50},
        .jitter = std::chrono::milliseconds{0},
        .random_loss_rate = 0.0,
        .random_seed = 20260903U
    };

    //Create the network simulator.
    aegis::network::NetworkSimulator simulator{
        config
    };

    //Capture the sending timestamp.
    const auto now = 
        aegis::network::Clock::now();

    //Create a test datagram containing 100 bytes.
    std::vector<std::uint8_t> wire_data(
        100U,
        0xABU
    );

    //Send the test datagram into the simulator
    const bool accepted = 
        simulator.Send(
            std::move(wire_data),
            now
        );
    
    //One packet should be waiting for delivery.
    assert(simulator.queued_packets() == 1U);

    //The packet should be accepted because loss rate is zero.
    assert(accepted);
    //The packet should not be ready immediately.
    const auto immediate_packets = 
        simulator.ReceiveReady(now);
    
    assert(simulator.queued_packets() == 1U);

    assert(immediate_packets.empty());

    //Advance time beyond the configured delay.
    const auto later = 
        now + std::chrono::milliseconds{51};
    
    //The packet should now be available.
    const auto ready_packets = 
        simulator.ReceiveReady(later);
    
    assert(simulator.queued_packets() == 0U);

    assert(ready_packets.size() == 1U);
    assert(
        ready_packets[0].wire_data.size() == 100U
    );


    //test random loss rate.
    const aegis::network::NetworkSimulatorConfig loss_config{
        .bandwidth_bps = 1'000'000'000U,
        .base_delay = std::chrono::milliseconds{50},
        .jitter = std::chrono::milliseconds{0},
        .random_loss_rate = 1.0,
        .random_seed = 20260903U
    };

    aegis::network::NetworkSimulator loss_simulator{
        loss_config
    };

    std::vector<std::uint8_t> lost_data(
        100U,
        0xCDU
    );

    const bool loss_accepted = 
        loss_simulator.Send(
            std::move(lost_data),
            now
        );
    
    assert(!loss_accepted);

    const auto loss_packets = 
        loss_simulator.ReceiveReady(
            now + std::chrono::milliseconds{1}
        );

    assert(loss_packets.empty());

    //test jitter 
    const aegis::network::NetworkSimulatorConfig jitter_config{
        .bandwidth_bps = 1'000'000'000U,
        .base_delay = std::chrono::milliseconds{50},
        .jitter = std::chrono::milliseconds{5},
        .random_loss_rate = 0.0,
        .random_seed = 20260903U
    };

    aegis::network::NetworkSimulator jitter_simulator{
        jitter_config
    };

    std::vector<std::uint8_t> jitter_data(
        100U,
        0xEFU
    );

    const bool jitter_accepted = 
        jitter_simulator.Send(
            std::move(jitter_data),
            now
        );

    assert(jitter_accepted);

    const auto early_packets = 
        jitter_simulator.ReceiveReady(
            now + std::chrono::milliseconds{44}
        );

    assert(early_packets.empty());

    const auto late_packets = 
        jitter_simulator.ReceiveReady(
            now + std::chrono::milliseconds{56}
        );
    
    assert(late_packets.size() == 1U);

    const aegis::network::NetworkSimulatorConfig bandwidth_config {
        .bandwidth_bps = 8'000U,
        .base_delay = std::chrono::milliseconds{0},
        .jitter = std::chrono::milliseconds{0},
        .random_loss_rate = 0.0,
        .random_seed = 20260903U
    };

    aegis::network::NetworkSimulator bandwidth_simulator {
        bandwidth_config
    };

    std::vector<std::uint8_t> first_data(
        100U,
        0x11U
    );

    std::vector<std::uint8_t> second_data(
        100U,
        0x22U 
    );

    const bool first_accepted = 
        bandwidth_simulator.Send(
            std::move(first_data),
            now
        );
    
    assert(first_accepted);

    const bool second_accepted = 
        bandwidth_simulator.Send(
            std::move(second_data),
            now
        );
    
    assert(second_accepted);

    const auto before_first_packet = 
        bandwidth_simulator.ReceiveReady(
            now + std::chrono::milliseconds{99}
        );
    
    assert(before_first_packet.empty());

    const auto after_first_packet = 
        bandwidth_simulator.ReceiveReady(
            now + std::chrono::milliseconds{101}
        );
    
    assert(after_first_packet.size() == 1U);

    const auto after_second_packet = 
        bandwidth_simulator.ReceiveReady(
            now + std::chrono::milliseconds{202}
        );

    assert(after_second_packet.size() == 1U);

    //================================================
    //Read statistics from the delay simulator.
    const auto& delay_stats = 
        simulator.stats();

    //One packet should have been sent.
    assert(delay_stats.packets_sent == 1U);

    //No packet should have been dropped.
    assert(delay_stats.packets_dropped == 0U);

    //One packet should have been delivered.
    assert(delay_stats.packets_delivered == 1U);

    //The delivered byte count should be preserved.
    assert(delay_stats.bytes_delivered == 100U);

    //==============================================
    //Read statistics from the loss simulator.
    const auto& loss_stats = 
        loss_simulator.stats();
    
    //The packet entered the simulated link.
    assert(loss_stats.packets_sent == 1U);

    //The packet entered the simulated link.
    assert(loss_stats.packets_dropped == 1U);

    //No packet should reach the receiver.
    assert(loss_stats.packets_delivered == 0U);

    //No bytes should be delivered.
    assert(loss_stats.bytes_delivered == 0U);

    //=================================================
    //Read statistics from the bandwidth simulator.
    const auto& bandwidth_stats = 
        bandwidth_simulator.stats();

    //Two packets should have been sent.
    assert(bandwidth_stats.packets_sent == 2U);
  
    //Neither packet should be dropped.
    assert(bandwidth_stats.packets_dropped == 0U);

    //Both packets should be delivered.
    assert(bandwidth_stats.packets_delivered == 2U);

    //Both 100-byte payloads should be preserved.
    assert(bandwidth_stats.bytes_delivered == 200U);
    std::cout
        << "Network simulator delay test passed.\n";
    return 0;

}