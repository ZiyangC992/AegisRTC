#include "aegis/network/network_simulator.hpp"

#include <stdexcept>
#include <utility>
#include <cstdint>
#include <chrono>


namespace aegis::network {

NetworkSimulator::NetworkSimulator (
    NetworkSimulatorConfig config
) 
    : config_(std::move(config)),
      random_engine_(config_.random_seed),
      jitter_distribution_(
        -config_.jitter.count(),
        config_.jitter.count()
      ) {
    if (config_.bandwidth_bps == 0U) {
        throw std::invalid_argument(
            "Network bandwidth must be greater than zero."
        );
    }

    if (config_.base_delay.count() < 0) {
        throw std::invalid_argument(
            "Base delay can not be negative."
        );
    }

    if (config_.jitter.count() < 0) {
        throw std::invalid_argument(
            "Network jitter can not be negative."
        );
    }

    if (config_.random_loss_rate < 0.0 ||
        config_.random_loss_rate > 1.0
    ) {
        throw std::invalid_argument(
            "Random loss rate must be between 0.0 and 1.0."
        );
    } 
}

[[nodiscard]] const NetworkTransportStats& 
NetworkSimulator::stats() const noexcept {
    return stats_;
}

//Return the number of packets waiting in the queue.
[[nodiscard]] std::size_t 
NetworkSimulator::queued_packets() const noexcept {
    return receive_queue_.size();
}


[[nodiscard]] bool NetworkSimulator::Send (
    std::vector<std::uint8_t> wire_data,
    TimePoint now
) {

    if (wire_data.empty()) {
        return false;
    }

    ++stats_.packets_sent;
    stats_.bytes_sent += 
        static_cast<std::uint64_t>(
            wire_data.size()
        );

    //A packet cannot start before the link becomes available.
    const TimePoint send_start = 
        std::max(
            now,
            next_send_available_time_
        );

    //Convert packet size from bytes to bits.
    const double packet_bits = 
        static_cast<double>(
            wire_data.size()
        ) * 8.0;
    
    //Calculate transmission time in seconds.
    const double transmission_seconds = 
        packet_bits /
        static_cast<double>(
            config_.bandwidth_bps
        );
    
    //Convert seconds to the simulator clock duration
    auto transmission_duration = 
        std::chrono::duration_cast<
            Clock::duration
        >(
            std::chrono::duration<double>(
                transmission_seconds
            )
        );
    
    //Ensure that even a very small packet consumes time.
    if (transmission_duration <= Clock::duration::zero()) {
        transmission_duration = 
            Clock::duration{1};
    }

    //Calculate when this packet finishes transmission
    const TimePoint send_finish = 
        send_start + transmission_duration;
    
    //The next packet must wait until this time
    next_send_available_time_ = 
        send_finish;
    
    //Generate a random loss decision value.
    const double loss_value = 
        loss_distribution_(
            random_engine_
        );

    //Drop the packet according to the configured loss rate.
    if (loss_value < config_.random_loss_rate) {
        ++stats_.packets_dropped;        
        return false;
    }

    //Generate a random jitter value in milliseconds.
    const std::int64_t jitter_milliseconds = 
        jitter_distribution_(
            random_engine_
        );
    
    //Combine the fixed delay and random jitter.
    auto total_delay = 
        config_.base_delay +
        std::chrono::milliseconds{
            jitter_milliseconds
        };
    
    //Network delay must not be negative.
    if(total_delay < std::chrono::milliseconds{0}) {
        total_delay = 
            std::chrono::milliseconds{0};
    }

    //Calculate when the packet becomes deliverable.
    const TimePoint ready_time = 
        send_finish + total_delay;

    //Create a packet waiting for delivery.
    ScheduledPacket scheduled_packet{
        .wire_data = std::move(wire_data),
        .send_time = now,
        .ready_time = ready_time
    };

    //Store the packet in the receive queue.
    receive_queue_.insert(
        std::move(scheduled_packet)
    );

    return true;
}

[[nodiscard]] std::vector<ReceivedPacket>
NetworkSimulator::ReceiveReady(
    TimePoint now
)
{
    // Store packets that are ready for delivery.
    std::vector<ReceivedPacket> ready_packets;

    // Check packets from the front of the queue.
    while (!receive_queue_.empty())
    {
        // The multiset keeps the earliest packet at the front.
        auto first_iterator =
            receive_queue_.begin();

        const ScheduledPacket& first_packet =
            *first_iterator;

        // Stop when the earliest packet is not ready yet.
        if (first_packet.ready_time > now)
        {
            break;
        }

        // Save metadata before removing the packet.
        const std::size_t delivered_bytes =
            first_packet.wire_data.size();

        const auto latency =
            std::chrono::duration_cast<
                std::chrono::microseconds
            >(
                now - first_packet.send_time
            );

        const std::uint64_t latency_microseconds =
            static_cast<std::uint64_t>(
                latency.count()
            );

        // Measure the difference from the previous latency sample.
        if (stats_.has_previous_latency)
        {
            const std::uint64_t jitter_microseconds =
                latency_microseconds >=
                    stats_.previous_latency_microseconds
                ? latency_microseconds -
                    stats_.previous_latency_microseconds
                : stats_.previous_latency_microseconds -
                    latency_microseconds;

            stats_.total_jitter_microseconds +=
                jitter_microseconds;

            ++stats_.jitter_sample_count;
        }
        else
        {
            stats_.has_previous_latency = true;
        }

        // Save the current latency as the previous sample.
        stats_.previous_latency_microseconds =
            latency_microseconds;

        // Update delivery statistics.
        ++stats_.packets_delivered;

        stats_.bytes_delivered +=
            static_cast<std::uint64_t>(
                delivered_bytes
            );

        stats_.total_latency_microseconds +=
            latency_microseconds;

        ++stats_.latency_sample_count;

        // Move the serialized packet payload into the
        // transport-independent ReceivedPacket object.
        ready_packets.push_back(
            ReceivedPacket{
                .wire_data =
                    std::move(first_iterator->wire_data),

                .receive_time = now
            }
        );

        // Remove the packet from the simulator queue.
        receive_queue_.erase(
            first_iterator
        );
    }

    // Return all packets that are ready now.
    return ready_packets;
}

} // namespace aegis::network
