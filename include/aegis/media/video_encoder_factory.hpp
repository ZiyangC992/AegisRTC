//Video_encoder_factory.hpp
#pragma once

#include "aegis/media/video_encoder.hpp"

#include <cstdint>
#include <memory>

namespace aegis::media {

enum class VideoEncoderBackend : std::uint8_t
{
    kSimulated,
    kFfmpeg
};

struct VideoEncoderConfig
{
    VideoEncoderBackend backend{
        VideoEncoderBackend::kFfmpeg
    };

    std::uint32_t width{640};
    std::uint32_t height{480};
    std::uint32_t frame_rate{30};
    std::uint32_t target_bitrate_kbps{1500};
    std::uint32_t key_frame_interval{60};
};

//Create an encoder implementation selected by the backend configuration.
[[nodiscard]] std::unique_ptr<IVideoEncoder>
CreateVideoEncoder(
    const VideoEncoderConfig& config
);

} //namespace aegis::media