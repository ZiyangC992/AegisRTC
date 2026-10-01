#include "aegis/network/retransmission_cache.hpp"

#include <cstdint>
#include <vector>
#include <stdexcept>
#include <utility>

namespace aegis::network {

RetransmissionCache::RetransmissionCache(
    RetransmissionCacheConfig config
)
    : config_(std::move(config)) {
    if (config_.max_packets == 0U) {
        throw std::invalid_argument(
            "Maximum packets must be positive."
        );
    }

    if (config_.max_packet_age.count() <= 0) {
        throw std::invalid_argument(
            "Maximum packet age must be positive."
        );
    }
}

void RetransmissionCache::Store(
    std::uint16_t sequence_number,
    std::vector<std::uint8_t> wire_data,
    TimePoint now
) {

    if (
        wire_data.empty()
    ) {
        throw std::invalid_argument(
            "Wire data is empty(RetransmissionCache::Store)."
        );
    }

    const auto existing_iterator = packets_.find(sequence_number);

    if (existing_iterator != packets_.end()) {

        existing_iterator->second.wire_data = 
            std::move(wire_data);

        existing_iterator->second.cached_time = now;

        return ;
    }

    if (packets_.size() >= config_.max_packets) {

        auto oldest_iterator = packets_.begin();

        for (
            auto iterator = packets_.begin();
            iterator != packets_.end();
        ) {
            if (
                iterator->second.cached_time < 
                oldest_iterator->second.cached_time
            ) {
                oldest_iterator = iterator;
            } 

            ++iterator;
        }
        packets_.erase(oldest_iterator);        
    }

        CachedPacket cached_packet{};

        cached_packet.sequence_number = sequence_number;
        cached_packet.wire_data = std::move(wire_data);
        cached_packet.cached_time = now;

        packets_.emplace(
            sequence_number,
            std::move(cached_packet)
        );

}

[[nodiscard]] std::optional<std::vector<std::uint8_t>> 
RetransmissionCache::Find(
        std::uint16_t sequence_number
) const {

    const auto iterator = packets_.find(sequence_number);

    if (iterator == packets_.end()){

        return std::nullopt;

    } 


    return iterator->second.wire_data;
}

[[nodiscard]] std::size_t RetransmissionCache::Expire(
    TimePoint now
) noexcept {

    std::size_t expire_count = 0U;

    for (
        auto iterator = packets_.begin();
        iterator != packets_.end();
    ) {
        if (
            now - iterator->second.cached_time >=
            config_.max_packet_age
        ) {

            iterator = packets_.erase(iterator);
            ++expire_count;

        } else {

            ++iterator;

        }
    }

    return expire_count;
}

[[nodiscard]] std::size_t 
RetransmissionCache::Size( ) const noexcept {

    return packets_.size();
    
}

}