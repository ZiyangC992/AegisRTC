#pragma once

#include "aegis/media/mf_video_capturer.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>   

namespace aegis::media {

//Represents one compressed video frame
struct EncodedVideoFrame {
    //Encoded frame bytes
    std::vector<std::uint8_t> data;

    //Frame resolution
    std::uint32_t width{0};
    std::uint32_t height{0};

    //Original capture timestamp
    std::int64_t timestamp_100ns{0};

    //Monotonically increasing frame number
    std::uint32_t frame_number{0};

    //Key frames can be decoded independently
    bool key_frame{false};
};

class IVideoEncoder {
public:
    virtual ~IVideoEncoder() = default;

    //Encode one raw video frame
    [[nodiscard]] virtual std::vector<
        EncodedVideoFrame>
    Encode(
        const CapturedVideoFrame &frame,
        bool force_key_frame) = 0;
    
    //Poll packets that are already available.
    //This operation does not end the encoding session.
    [[nodiscard]] virtual std::vector<
        EncodedVideoFrame
    > PollEncodedFrames() = 0;

    [[nodiscard]] virtual std::vector<
        EncodedVideoFrame
    > Flush() = 0;

    //Update the target bitrate at runtime.
    virtual void SetTargetBitrate(
        std::uint32_t bitrate_kbps
    ) = 0;

    //Request an immediate key_frame.
    virtual void RequestKeyFrame() = 0;

    //Return the currently configured target bitrate.
    [[nodiscard]] virtual std::uint32_t
    TargetBitrate() const noexcept = 0;

};

} // namespace aegis::media