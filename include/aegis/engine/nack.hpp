#pragma once

#include "aegis/common.hpp"
#include "aegis/network/frame_reassembler.hpp"

#include <cstdint>
#include <cstddef>
#include <vector>
#include <map>

namespace aegis::engine {

using aegis::TimePoint;

struct NackConfig {
    //Wait before reporting a missing packet.
    std::chrono::milliseconds reorder_wait{20};

    //Minimum interval between two NACK requests.
    std::chrono::milliseconds retry_interval{50};

    //Maximum lifetime of one missing packet record.
    std::chrono::milliseconds max_missing_age{300};

    //Maximum number of NACK retries.
    std::uint32_t max_retries{3};

    //Maximum number of tracked missing packets.
    std::size_t max_tracked_missing_packets{256};

    //Maximum NACK entries returned by one Poll call.
    std::size_t max_nack_batch_size{32};
};

class NackController final {
public:
    explicit NackController(
        NackConfig config = {}
    );

    //Record sequence numbers that may be missing between two received packets.
    void ObserveGap(
        std::uint16_t previous_sequence,
        std::uint16_t current_sequence,
        TimePoint now
    );

    //Remove a missing-packet record after the packet arrives.
    void OnPacketRecovered(
        std::uint16_t sequence_number
    );

    //Return missing sequence numbers that are ready for NACK transmission.
    [[nodiscard]] std::vector<std::uint16_t> Poll(
        TimePoint now
    );

    //Remove missing-packet records that have exceeded their lifetime.
    [[nodiscard]] std::size_t Expire(
        TimePoint now
    ) noexcept;

private:
    //Internal state of one missing packet.
    struct MissingPacket {
        //Sequence number of the missing packet.
        std::uint16_t sequence_number{0};

        //Time when the packet was first detected as missing.
        TimePoint first_missing_time{};

        //Time when the last NACK was generated.
        TimePoint last_nack_time{};

        //Number of NACK requests already generated.
        std::uint32_t retry_count{0};
    };

    //Store missing packets indexed by their sequence numbers.
    std::map<
        std::uint16_t,
        MissingPacket
    > missing_packets_;

    //Runtime configuration of this controller.
    NackConfig config_{};
};

}