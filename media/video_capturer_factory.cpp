#include "aegis/media/video_capturer_factory.hpp"
#include "aegis/media/mf_video_capturer.hpp"

#include <stdexcept>

#if defined( _WIN32)
#include "aegis/media/mf_video_capturer.hpp"
#elif defined(__linux__)
#include "aegis/media/v4l2_video_capturer.hpp"
#endif

namespace aegis::media{

[[nodiscard]] std::unique_ptr<
    IVideoCapturer
> CreateVideoCapturer(
    const VideoCapturerConfig& config) 
{
    switch(config.backend)
    {
        case VideoCapturerBackend::kDefault:
#ifdef _WIN32
            return std::make_unique<MfVideoCapturer>();
#elif defined(__linux__)
            return std::make_unique<V4L2VideoCapturer>();
#else
            throw std::runtime_error(
                "No supported video capturer backend."
            );
#endif
        default:
            throw std::invalid_argument(
                "Unsupported video capturer backend."
            );
    }
}

} //namespace aegis::media
