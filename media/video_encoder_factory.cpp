#include "aegis/media/simulated_video_encoder.hpp"
#include "aegis/media/ffmpeg_video_encoder.hpp"
#include "aegis/media/video_encoder_factory.hpp"

#include <stdexcept>
#include <utility>

namespace aegis::media{

std::unique_ptr<IVideoEncoder>
CreateVideoEncoder(
    const VideoEncoderConfig& config
) {

    if (config.width == 0U ||
        config.height == 0U ||
        config.frame_rate == 0U ||
        config.target_bitrate_kbps == 0U ||
        config.key_frame_interval == 0U)
    {
        throw std::invalid_argument(
            "Invalid video encoder configuration.");
    }

    switch(config.backend) {

        case VideoEncoderBackend::kSimulated:
        {
            SimulatedVideoEncoderConfig simulated_config{
                .key_frame_interval = 
                    config.key_frame_interval,

                .target_bitrate_kbps = 
                    config.target_bitrate_kbps
            };

            return std::make_unique<
                SimulatedVideoEncoder>(
                    simulated_config
                );
        }

        case VideoEncoderBackend::kFfmpeg:
        {
            FfmpegVideoEncoderConfig ffmpeg_config{
                .width = config.width,
                .height = config.height,
                .frame_rate = config.frame_rate,
                .target_bitrate_kbps = config.target_bitrate_kbps,
                .key_frame_interval = config.key_frame_interval   
            };

            return std::make_unique<
                FfmpegVideoEncoder>(
                    ffmpeg_config
                );
        }

        default:
            throw std::invalid_argument(
                "Unsupported video encoder backend."
            );
    }
}


} //namespace aegis::media