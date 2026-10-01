#pragma once

#include "aegis/media/video_capturer.hpp"

#include <cstdint>
#include <memory>

namespace aegis::media {

enum class VideoCapturerBackend : std::uint8_t
{
    kMediaFoundation
};

struct VideoCapturerConfig
{
    VideoCapturerBackend backend{
        VideoCapturerBackend::kMediaFoundation
    };
};

[[nodiscard]] std::unique_ptr<
    IVideoCapturer>
CreateVideoCapturer(
    const VideoCapturerConfig& config
);

} //namespace aegis::media