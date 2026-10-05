#include "aegis/engine/nack.hpp"

#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include <map>
#include <vector>
#include <utility>

namespace aegis::engine {

NackController::NackController(
    NackConfig config
) 
    : config_(std::move(config)) {
    
    if (config_.reorder_wait.count() < 0) {
        throw std::invalid_argument(
            "Reorder wait must not be negative."
        );
    }

    if (config_.retry_interval.count() <= 0) {
        throw std::invalid_argument(
            "Retry interval must not be negative."
        );
    }

    if (config_.max_missing_age.count() <= 0) {
        throw std::invalid_argument(
            "Maximum missing age must not be negative."
        );
    }

    if (config_.max_retries == 0U) {
        throw std::invalid_argument(
            "Maximum retries must be positive."
        );
    }

    if (config_.max_tracked_missing_packets == 0U) {
        throw std::invalid_argument(
            "Maximum tracked missing packets must be positive."
        );
    }

    if (config_.max_nack_batch_size == 0U) {
        throw std::invalid_argument(
            "Maximum nack batch size must be positive."
        );
    }
}

void NackController::ObserveGap(
    std::uint16_t previous_sequence,
    std::uint16_t current_sequence,
    TimePoint now
) {
    //Start checking from the sequence number after the previous packet.
    std::uint16_t candidate_sequence = 
        static_cast<std::uint16_t>(
            previous_sequence + 1U
        );
    
    //Walk until the current packet is reached.
    while (
        candidate_sequence != current_sequence
    ) {
        //Stop tracking when the configured capacity is reached.
        if (
            missing_packets_.size() >=
            config_.max_tracked_missing_packets
        ) {
            return;
        }

        //Do not insert the same missing sequence twice.
        if (
            !missing_packets_.contains(
                candidate_sequence
            )
        ) {
            MissingPacket missing_packet{};

            missing_packet.sequence_number = 
                candidate_sequence;

            missing_packet.first_missing_time = 
                now;

            missing_packet.last_nack_time = 
                now;

            missing_packet.retry_count = 0U;

            missing_packets_.emplace(
                candidate_sequence,
                missing_packet
            );
        }

        //uint16_t automatically wraps from 65535 to 0
        //after the explicit conversion.
        candidate_sequence = 
            static_cast<std::uint16_t>(
                candidate_sequence + 1U
            );
    }
}

void NackController::OnPacketRecovered(
        std::uint16_t sequence_number
) {
    const auto iterator = missing_packets_.find(
        sequence_number
    );

    if (iterator == missing_packets_.end()) {
        
        return ;
    
    }

    missing_packets_.erase(iterator);
        
}

std::vector<std::uint16_t>
NackController::Poll(
    TimePoint now
)
{
    return Poll(
        now,
        config_.retry_interval
    );
}

std::vector<std::uint16_t>
NackController::Poll(
    TimePoint now,
    aegis::Milliseconds retry_interval
)
{
    if (retry_interval.count() <= 0)
    {
        retry_interval =
            config_.retry_interval;
    }

    std::vector<std::uint16_t>
        nack_sequence_numbers{};

    nack_sequence_numbers.reserve(
        config_.max_nack_batch_size
    );

    for (
        auto& [
            sequence_number,
            missing_packet
        ] :
        missing_packets_
    )
    {
        if (
            nack_sequence_numbers.size() >=
            config_.max_nack_batch_size
        )
        {
            break;
        }

        const auto missing_age =
            now -
            missing_packet.first_missing_time;

        // Keep the original reorder wait.
        // RTT is used for retry timing,
        // not for the first reordering delay.
        if (
            missing_age <
            config_.reorder_wait
        )
        {
            continue;
        }

        if (
            missing_packet.retry_count >=
            config_.max_retries
        )
        {
            continue;
        }

        const auto last_nack_age =
            now -
            missing_packet.last_nack_time;

        if (
            missing_packet.retry_count > 0U &&
            last_nack_age <
                retry_interval
        )
        {
            continue;
        }

        nack_sequence_numbers.push_back(
            sequence_number
        );

        ++missing_packet.retry_count;

        missing_packet.last_nack_time =
            now;
    }

    return nack_sequence_numbers;
}

std::size_t
NackController::Expire(
    TimePoint now
) noexcept
{
    return Expire(
        now,
        config_.max_missing_age
    );
}

std::size_t
NackController::Expire(
    TimePoint now,
    aegis::Milliseconds max_missing_age
) noexcept
{
    if (max_missing_age.count() <= 0)
    {
        max_missing_age =
            config_.max_missing_age;
    }

    std::size_t expire_count = 0U;

    for (
        auto iterator =
            missing_packets_.begin();
        iterator != missing_packets_.end();
    )
    {
        const auto missing_age =
            now -
            iterator->second.first_missing_time;

        if (
            missing_age >=
            max_missing_age
        )
        {
            iterator =
                missing_packets_.erase(
                    iterator
                );

            ++expire_count;
        }
        else
        {
            ++iterator;
        }
    }

    return expire_count;
}

}