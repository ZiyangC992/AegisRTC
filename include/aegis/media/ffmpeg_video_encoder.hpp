#pragma once

#include "aegis/media/video_encoder.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace aegis::media {

struct FfmpegVideoEncoderConfig {

    std::uint32_t width{640};

    std::uint32_t height{480};

    std::uint32_t frame_rate{30};

    std::uint32_t target_bitrate_kbps{1500};

    std::uint32_t key_frame_interval{60};
};

class FfmpegVideoEncoder final : public IVideoEncoder {

public:

    explicit FfmpegVideoEncoder(
        FfmpegVideoEncoderConfig config = {}
    );

    FfmpegVideoEncoder(
        const FfmpegVideoEncoder&
    ) = delete;

    FfmpegVideoEncoder* operator=(
        const FfmpegVideoEncoder&
    ) = delete;
    
    ~FfmpegVideoEncoder();

    [[nodiscard]] std::vector<EncodedVideoFrame> 
    Encode(
        const CapturedVideoFrame& frame,
        bool force_key_frame
    ) override;

    [[nodiscard]] std::vector<
        EncodedVideoFrame
    > PollEncodedFrames() override;
    
    [[nodiscard]] std::vector<EncodedVideoFrame>
    Flush() override;    

    void SetTargetBitrate(
        std::uint32_t target_bitrate
    )override;

    void RequestKeyFrame() override;

    [[nodiscard]] std::uint32_t 
    TargetBitrate() const noexcept override;



private:
    struct Impl;

    std::unique_ptr<Impl> impl_;

};

}