#pragma once

#include "aegis/media/video_encoder.hpp"

#include <cstdint>
#include <vector>

namespace aegis::network {

struct VideoPacketFragment {

    std::uint16_t sequence_number{0};

    std::uint32_t frame_number{0};

    std::int64_t timestamp_100ns{0};

    bool key_frame{false};

    bool start_of_frame{false};

    bool end_of_frame{false};

    std::vector<std::uint8_t> payload;
};

struct VideoPacketizerConfig {

    std::size_t max_payload_bytes{1200};
};

class VideoPacketizer final {
public:
    explicit VideoPacketizer(
        VideoPacketizerConfig config
    );

    [[nodiscard]] std::vector<VideoPacketFragment>
    Packetize(
        const aegis::media::EncodedVideoFrame& frame
    );

    //Reset the packet sequence number.
    void Reset() noexcept;

private:
    VideoPacketizerConfig config_;

    //Sequence number assigned to the next packet.
    std::uint16_t next_sequence_number_{0};
};

}