#pragma once

#include "aegis/common.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace aegis::network {

struct RetransmissionCacheConfig{

    std::size_t max_packets{512};

    Milliseconds max_packet_age{1000};
};

struct CachedPacket{

    std::uint16_t sequence_number{0};

    std::vector<std::uint8_t> wire_data;

    TimePoint cached_time{};
};

class RetransmissionCache final {

public:
    explicit RetransmissionCache(
        RetransmissionCacheConfig config = {}
    ) ;

    void Store(
        std::uint16_t sequence_number,
        std::vector<std::uint8_t> wire_data,
        TimePoint now
    ) ;

    [[nodiscard]] std::optional<
        std::vector<std::uint8_t>> Find(
            std::uint16_t sequence_number
    ) const ;

    [[nodiscard]] std::size_t Expire(
        TimePoint now
    ) noexcept ;

    [[nodiscard]] std::size_t Size( ) const noexcept;

private:   

    std::map<
        std::uint16_t,
        CachedPacket
    > packets_;

    RetransmissionCacheConfig config_;
};

} //namespace aegis::network