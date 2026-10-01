#pragma once

#include "aegis/common.hpp"
#include "aegis/network/wire_protocol.hpp"

#include <cstdint>
#include <optional>
#include <vector>
#include <map>
#include <chrono>
#include <cstddef>

namespace aegis::network {

using ReassemblerTimePoint = 
    aegis::TimePoint;

struct ReassembledVideoFrame {
    //Encoded video frame number.
    std::uint32_t frame_number{0};

    //Capture timestamp in 100-nanosecond units.
    std::int64_t timestamp_100ns{0};

    //True when the frame is a key frame.
    bool key_frame{false};

    //Complete encoded frame payload.
    std::vector<std::uint8_t> payload;
};

struct FrameReassemblerConfig {
    //Maximum time allowed for an incomplete frame.
    std::chrono::milliseconds max_frame_age{
        500
    };

    //Maximum number of frames kept in memory.
    std::size_t max_in_flight_frames{
        60
    };
};

class FrameReassembler final {
public:
    explicit FrameReassembler(
        FrameReassemblerConfig config = {}
    );

    [[nodiscard]] std::optional<
        ReassembledVideoFrame
    > Push(
        const WirePacket& packet,
        ReassemblerTimePoint now
    );

    //Romve incomplete frames that have exceeded their lifetime.
    [[nodiscard]] std::size_t Expire(
        ReassemblerTimePoint now 
    ) noexcept;

private:
    //Reassembler configuration.
    FrameReassemblerConfig config_{};

    //Internal state will be added later.
    struct FrameAssembly {
        //Frame metadata.
        std::uint32_t frame_number{0};
        std::int64_t timestamp_100ns{0};
        bool key_frame{false};

        //Time when the first fragment was received.
        ReassemblerTimePoint first_arrival_time{};
        
        //Sequence number of the first observed fragment.
        std::optional<std::uint16_t>
            first_sequence_number;

        //Packet payloads indexed by sequence number.
        std::map<
            std::uint16_t,    //sequence_number
            std::vector<std::uint8_t>   //payload
        > fragments;

        //Sequence number of the final fragment.
        std::optional<
            std::uint16_t
        > end_sequence_number;
    };

    std::map<
        std::uint32_t,   //frame_number
        FrameAssembly
    > frame_assemblies_;

};

} //namespace aegis::network