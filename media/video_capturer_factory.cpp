#include "aegis/media/video_capturer_factory.hpp"
#include "aegis/media/mf_video_capturer.hpp"

#include <stdexcept>

namespace aegis::media{

[[nodiscard]] std::unique_ptr<
    IVideoCapturer
> CreateVideoCapturer(
    const VideoCapturerConfig& config) 
{
    switch(config.backend)
    {
        case VideoCapturerBackend::kMediaFoundation:
            return std::make_unique<MfVideoCapturer>();
        
        default:
            throw std::invalid_argument(
                "Unsupported video capturer backend."
            );
    }
}

} //namespace aegis::media
