#include "aegis/media/ffmpeg_video_encoder.hpp"

#include <cstdint>
#include <cstddef>
#include <cassert>
#include <iostream>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244)
#pragma warning(disable : 4819)
#endif

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavutil/error.h>
}

static void PrintH264Encoders()
{
    void *iterator = nullptr;

    while (true)
    {
        const AVCodec *codec =
            av_codec_iterate(&iterator);

        if (codec == nullptr)
        {
            break;
        }

        if (!av_codec_is_encoder(codec) ||
            codec->id != AV_CODEC_ID_H264)
        {
            continue;
        }

        std::cout
            << "H.264 encoder: "
            << codec->name
            << '\n';
    }
}

int main()
{
    using namespace aegis::media;
    PrintH264Encoders();
    aegis::media::CapturedVideoFrame frame{};

    frame.width = 640;
    frame.height = 480;
    frame.stride = 640 * 2;
    frame.subtype = aegis::media::VideoSubtype::kYuy2;
    frame.timestamp_100ns = 0;
    frame.data.resize(
        static_cast<std::size_t>(frame.stride) *
        frame.height);

    aegis::media::FfmpegVideoEncoder encoder;

    bool received_encoded_data = false;

    for (std::uint32_t index = 0U; index < 10U; ++index)
    {
        frame.timestamp_100ns =
            static_cast<std::int64_t>(index) * 333333;

        // Encode may return no output because the encoder buffers frames.
        const auto encoded_frames =
            encoder.Encode(frame, true);

        std::cout
            << "Encode output count: "
            << encoded_frames.size()
            << '\n';

        for (const auto &encoded_frame :
             encoded_frames)
        {
            assert(!encoded_frame.data.empty());
            assert(encoded_frame.width == frame.width);
            assert(encoded_frame.height == frame.height);

            received_encoded_data = true;
        }

        // Poll delayed packets without flushing the encoder.
        const auto delayed_frames =
            encoder.PollEncodedFrames();

        std::cout
            << "Poll output count: "
            << delayed_frames.size()
            << '\n';
        for (const auto &encoded_frame : delayed_frames)
        {
            assert(!encoded_frame.data.empty());
            assert(encoded_frame.width == frame.width);
            assert(encoded_frame.height == frame.height);

            received_encoded_data = true;
        }
    }


    // Flush remaining packets buffered inside FFmpeg.
    const auto flushed_frames =
        encoder.Flush();
        
    for (const auto &encoded_frame :
         flushed_frames)
    {
        assert(!encoded_frame.data.empty());
        assert(encoded_frame.width == frame.width);
        assert(encoded_frame.height == frame.height);

        received_encoded_data = true;
    }

    if (!received_encoded_data)
    {
        std::cerr
            << "No encoded frame was produced.\n";

        return 1;
    }
}