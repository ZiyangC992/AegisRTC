#pragma once

#include "aegis/media/video_encoder.hpp"

#include <cstddef>
#include <cstdint>

namespace aegis::media {

struct SimulatedVideoEncoderConfig
{
    std::uint32_t key_frame_interval{60};
    double key_frame_size_multiplier{2.0};

    std::size_t max_encoded_frame_bytes{
        256U * 1024U
    };

    std::uint32_t target_bitrate_kbps{1500};
};

class SimulatedVideoEncoder final
    : public IVideoEncoder
{
public:
    explicit SimulatedVideoEncoder(
        SimulatedVideoEncoderConfig config = {});

    [[nodiscard]] std::vector<EncodedVideoFrame>
    Encode(
        const CapturedVideoFrame& frame,
        bool force_key_frame) override;

    [[nodiscard]] std::vector<EncodedVideoFrame>
    PollEncodedFrames() override;

    [[nodiscard]] std::vector<EncodedVideoFrame>
    Flush() override;

    void SetTargetBitrate(
        std::uint32_t bitrate_kbps) override;

    void RequestKeyFrame() override;

    [[nodiscard]] std::uint32_t
    TargetBitrate() const noexcept override;

private:
    SimulatedVideoEncoderConfig config_;
    std::uint32_t next_frame_number_{0};
    bool force_next_key_frame_{false};
};

} // namespace aegis::media