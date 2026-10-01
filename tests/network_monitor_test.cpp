#include "aegis/network/network_monitor.hpp"
#include "aegis/network/network_simulator.hpp"

#include <cstdint>
#include <cstddef>
#include <cassert>
#include <iostream>

int main() {

    using namespace aegis::network;

    const aegis::network::NetworkSimulatorStats stats{
        .packets_sent = 100U,
        .packets_dropped = 5U,
        .packets_delivered = 95U,
    };

    NetworkMonitor monitor;

    const auto first_snapshot = 
        monitor.Update(
            stats,
            100U,
            0U,
            0U,
            0U
        );

    assert(first_snapshot.queue_delta == 0);
    assert(!first_snapshot.queue_is_growing);

    const auto growing_snapshot = 
        monitor.Update(
            stats,
            120U,
            0U,0U,0U
        );

    assert(growing_snapshot.queue_delta == 20);
    assert(growing_snapshot.queue_is_growing);

    const auto shrinking_snapshot = 
        monitor.Update(
            stats,
            80U,
            0U,
            0U,
            0U
        );

    assert(shrinking_snapshot.queue_delta == -40);  // -40 not -20
    assert(!shrinking_snapshot.queue_is_growing);

    const auto stable_snapshot = 
        monitor.Update(
            stats,
            80U,
            0U,
            0u,
            0U
        );

    assert(stable_snapshot.queue_delta == 0);
    assert(!stable_snapshot.queue_is_growing);

    const auto metric_snapshot =
        monitor.Update(
        stats,
        90U,
        10U,
        8U,
        2U);

    assert(metric_snapshot.retransmission_packets == 10U);
    assert(metric_snapshot.retransmission_cache_hits == 8U);
    assert(metric_snapshot.retransmission_cache_misses == 2U);

    assert(metric_snapshot.retransmission_cache_hit_rate
        > 0.79);

    assert(metric_snapshot.retransmission_cache_hit_rate
        < 0.81);


}