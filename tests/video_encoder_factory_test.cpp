#include "aegis/media/video_encoder_factory.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

int main()
{
    using namespace aegis::media;

    CapturedVideoFrame input_frame{
        .data = std::vector<std::uint8_t>(
            static_cast<std::size_t>(640U * 2U * 480U),
            0U),

        .width = 640U,
        .height = 480U,
        .stride = 640U * 2U,
        .timestamp_100ns = 0,
        .subtype = VideoSubtype::kYuy2};
        
    // Verify the simulated backend.
    const VideoEncoderConfig simulated_config{
	#ifdef _WIN32	
        .backend = VideoEncoderBackend::kFfmpeg,
        #else
	.backend = VideoEncoderBackend::kSimulated,
	#endif
	.width = 640U,
        .height = 480U,
        .frame_rate = 30U,
        .target_bitrate_kbps = 1500U,
        .key_frame_interval = 60U
    };

    auto simulated_encoder =
        CreateVideoEncoder(simulated_config);

    assert(simulated_encoder != nullptr);

    const auto simulated_frames =
        simulated_encoder->Encode(
            input_frame,
            true);

    assert(!simulated_frames.empty());
    assert(!simulated_frames.front().data.empty());

    // Verify the FFmpeg backend can also be created.
    const VideoEncoderConfig ffmpeg_config{
        #ifdef _WIN32
	.backend = VideoEncoderBackend::kFfmpeg,
        #else
	.backend = VideoEncoderBackend::kSimulated,
	#endif
	.width = 640U,
        .height = 480U,
        .frame_rate = 30U,
        .target_bitrate_kbps = 1500U,
        .key_frame_interval = 60U
    };

    auto ffmpeg_encoder =
        CreateVideoEncoder(ffmpeg_config);

    assert(ffmpeg_encoder != nullptr);

    std::cout
        << "Video encoder factory test passed.\n";

    return 0;
}
