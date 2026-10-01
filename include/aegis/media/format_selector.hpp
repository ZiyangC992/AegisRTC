#pragma once

#include "aegis/media/mf_video_capturer.hpp"

#include <optional>
#include <cstdint>
#include <vector>

namespace aegis::media {
    [[nodiscard]] std::optional<CameraFormatInfo>
    SelectBestCameraFormat(
        const std::vector<CameraFormatInfo>& formats,
        std::uint32_t target_width,
        std::uint32_t target_height,
        std::uint32_t target_frame_rate
    );

} // namespace aegis::media