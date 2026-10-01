#include "aegis/media/ffmpeg_video_encoder.hpp"

#include <stdexcept>
#include <utility>
#include <string>
#include <iostream>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4244)
#pragma warning(disable: 4819)
#endif

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavutil/error.h>
#include <libavutil/pixdesc.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
}

#ifdef _MSV_VER
#pragma warning(pop)
#endif

namespace {

[[nodiscard]] std::string FfmpegErrorToString(
    int error_code
)
{
    char error_buffer[AV_ERROR_MAX_STRING_SIZE]{};

    av_strerror(
        error_code,
        error_buffer,
        sizeof(error_buffer)
    );

    return std::string(error_buffer);
}

[[nodiscard]] std::vector<
    aegis::media::EncodedVideoFrame
> ReceiveAvailablePackets(
    AVCodecContext* codec_context,
    AVPacket* packet,
    const aegis::media::FfmpegVideoEncoderConfig& config
) {
    std::vector<
        aegis::media::EncodedVideoFrame
    > results;

    //Read all packets currently available from the encoder.
    while(true) {
        const int receive_result = 
            avcodec_receive_packet(
                codec_context,
                packet
            );

        //EAGAIN means that no packet is available yet.
        if (receive_result == AVERROR(EAGAIN)) {
            break;
        }

        //EOF means that the encoder has no more output.
        if (receive_result == AVERROR_EOF) {
            break;
        }

        if (receive_result < 0) {
            throw std::runtime_error(
                "avcodec_receive_packet failed: " +
                FfmpegErrorToString(receive_result)
            );
        }

        aegis::media::EncodedVideoFrame encoded_frame{
            .width = config.width,
            .height = config.height,
            .timestamp_100ns = 0,
            .frame_number = 0,
            .key_frame = 
                (packet->flags &
                 AV_PKT_FLAG_KEY) != 0
        };

        //Convert the packet PTS into project metadata.
        if (packet->pts != AV_NOPTS_VALUE) {
            //The project uses the PTS as the frame number.
            encoded_frame.frame_number = 
                static_cast<std::uint32_t>(
                    packet->pts
                );

            //Convert the PTS into 100-nanosecond units.
            encoded_frame.timestamp_100ns = 
                av_rescale_q(
                    packet->pts,
                    codec_context->time_base,
                    AVRational{1,10'000'000}
                );
        }

        //Copy the encoded H.264 bytes.
        encoded_frame.data.assign(
            packet->data,
            packet->data + packet->size
        );
        

        results.push_back(
            std::move(encoded_frame)
        );

        //Release packet references before receiving the next packet.
        av_packet_unref(packet);

       
    }
     return results;
}


} //namespace

namespace aegis::media{
struct FfmpegVideoEncoder::Impl{
    //Runtime configuration for the ffmpeg encoder.
    FfmpegVideoEncoderConfig config;

    //Describer the selected H.264 encoder implementation.
    const AVCodec* codec{nullptr};

    //Stores the runtime state of one encoder instance.
    AVCodecContext* codec_context{nullptr};

    //Reusable raw video frame submitted to the encoder.
    AVFrame* frame{nullptr};

    //Reusable compressed packet produced by the encoder.
    AVPacket* packet{nullptr};

    //Converts the camera pixel format to the encoder pixel format.
    SwsContext* scaler{nullptr};
    
    //Monitonically increasing number of the next input frame.
    std::uint64_t next_frame_number{0};

    //Requests the next encoded frame to be an intra frame.
    bool force_next_key_frame{false};
}; 

FfmpegVideoEncoder::FfmpegVideoEncoder(
    FfmpegVideoEncoderConfig config
) 
    : impl_(std::make_unique<Impl>())
{
    impl_->config = std::move(config);

    if (impl_->config.width == 0U ||
        impl_->config.height == 0U ||
        impl_->config.frame_rate == 0U ||
        impl_->config.target_bitrate_kbps == 0U ||
        impl_->config.key_frame_interval == 0U) {
            throw std::invalid_argument(
                " Invalid FFmpeg encoder configuration."
        );
    }

    //Find the H.264 encoder provided by FFmpeg.
    impl_->codec =
        avcodec_find_encoder_by_name("h264_mf");

    if (impl_->codec == nullptr) {
        throw std::runtime_error(
            "H.264 encoder was not found."
        );
    }

    //Allocate the encoder context.
    impl_->codec_context = 
        avcodec_alloc_context3(impl_->codec);

    if (impl_->codec_context == nullptr) {
        throw std::runtime_error(
            "Failed to allocate AVCodecContext."
        );
    }

    //Configure the encoded video format.
    impl_->codec_context->width = 
        static_cast<int>(impl_->config.width);

    impl_->codec_context->height =  
        static_cast<int>(impl_->config.height);

    //Define the timestamp unit for frame PTS values.
    //For 30 FPS, one timestamp uint equals 1 / 30 seconds.
    impl_->codec_context->time_base = 
        AVRational{1, static_cast<int>(
            impl_->config.frame_rate
        )};
    
    //Define the nominal playback frame rate.
    impl_->codec_context->framerate = 
        AVRational{
            static_cast<int>(impl_->config.frame_rate),
            1
        };
    
    //The encoder receive NV12 frames from the camera pipeline.
    impl_->codec_context->pix_fmt = 
        AV_PIX_FMT_NV12;
    
    //FFmpeg expects bitrate in bits per second.
    //The project configuration stores bitrate in kilobits per second.
    impl_->codec_context->bit_rate = 
        static_cast<std::int64_t>(
            impl_->config.target_bitrate_kbps
        ) * 1000;

    //Set the maximum distance between two key frames.
    impl_->codec_context->gop_size = 
        static_cast<int>(
            impl_->config.key_frame_interval
        );

    // Disable B-frames to reduce frame reordering and latency.
    impl_->codec_context->max_b_frames = 0;

    //Request low-latency encoding behavior.
    impl_->codec_context->flags |= AV_CODEC_FLAG_LOW_DELAY;

    //Open and initialize the selected H.264 encoder.
    const int open_result = 
        avcodec_open2(
            impl_->codec_context,
            impl_->codec,
            nullptr
        );
    
    //FFmpeg returns a negative value when opening fails.
    if (open_result < 0) {
        throw std::runtime_error(
            "avcodec_open2 falied: " +
            FfmpegErrorToString(open_result)
        );
    }

    //Allocate a reusable input frame.
    impl_->frame = av_frame_alloc();

    if (impl_->frame == nullptr) {
        throw std::runtime_error(
            "Falied to allocate AVFrame."
        );
    }

    //Use the pixel format configured for the encoder.
    impl_->frame->format = 
        impl_->codec_context->pix_fmt;

    //Use the configured video width.
    impl_->frame->width = 
        impl_->codec_context->width;

    //Use the configured video height.
    impl_->frame->height = 
        impl_->codec_context->height;

    //Allocate image planes and aligned row buffers.
    //The alignment improves memory access efficiency.
    if (av_frame_get_buffer(impl_->frame, 32) < 0) {
        throw std::runtime_error(
            "Falied to allocate AVFrame buffer."
        );
    }

    //Allocate a reusable output packet.
    impl_->packet = av_packet_alloc();

    if (impl_->packet == nullptr) {
        throw std::runtime_error(
            "Falied to allocate AVPacket."
        );
    }
}

FfmpegVideoEncoder::~FfmpegVideoEncoder()
{
    //Release the reusable output packet.
    av_packet_free(&impl_->packet);

    //Release the reusable input frame.
    av_frame_free(&impl_->frame);

    //Release the codec context.
    avcodec_free_context(
        &impl_->codec_context
    );

    //Release the pixel-format conversion context.
    if (impl_->scaler != nullptr) {
        sws_freeContext(impl_->scaler);
        impl_->scaler = nullptr;
    }
}

void FfmpegVideoEncoder::SetTargetBitrate(
    std::uint32_t target_bitrate
) {
    if (target_bitrate == 0U) {
        throw std::invalid_argument(
            "Target bitrate must be greater than zero."
        );
    }

    impl_->config.target_bitrate_kbps = 
        target_bitrate;

    impl_->codec_context->bit_rate = 
        static_cast<std::int64_t>(
            target_bitrate
        ) * 1000;
}

void FfmpegVideoEncoder::RequestKeyFrame() {
    impl_->force_next_key_frame = true;
}

std::uint32_t 
FfmpegVideoEncoder::TargetBitrate() const noexcept {

    return impl_->config.target_bitrate_kbps;
}

std::vector<EncodedVideoFrame> 
FfmpegVideoEncoder::Encode(
    const CapturedVideoFrame& frame,
    bool force_key_frame
) {
    if (frame.width == 0U ||
        frame.height == 0U ||
        frame.data.empty()) {
            throw std::invalid_argument(
                "Invalid captured video frame configuration."
        );
    }

    if (frame.width != impl_->config.width ||
        frame.height != impl_->config.height) {
            throw std::invalid_argument(
                "Input frame size don't equal to encoder configuration."
            );
    }

    if (frame.stride == 0U) {
        throw std::invalid_argument(
            "Captured video frame has invalid stride."
        );
    }

    AVPixelFormat source_pixel_format = 
        AV_PIX_FMT_NONE;

    const std::uint8_t* source_data[4]{};
    int source_linesize[4]{};

    source_data[0] = 
        frame.data.data();

    source_linesize[0] = 
        static_cast<int>(frame.stride);

    switch(frame.subtype) {
        case VideoSubtype::kYuy2:
            source_pixel_format = 
                AV_PIX_FMT_YUYV422;
            break;

        case VideoSubtype::kRgb24:
            source_pixel_format = 
                AV_PIX_FMT_RGB24;
            break;
        
        case VideoSubtype::kNv12:
        {     
            source_pixel_format =
                AV_PIX_FMT_NV12;

            const std::size_t y_plane_size = 
                static_cast<std::size_t>(frame.stride) *
                frame.height;

            const std::size_t required_size = 
                y_plane_size + 
                y_plane_size / 2U;
            
            if (frame.data.size() < required_size) {
                throw std::invalid_argument(
                    "NV12 frame data is smaller than expected."
                );
            }

            //The UV plane starts immediately after the Y plane.
            source_data[1] = 
                frame.data.data() + y_plane_size;

            source_linesize[1] = 
                static_cast<int>(frame.stride);

            break;            
        }

        
        default:
            throw std::invalid_argument(
                "Unsupported input video subtype."
            );
    }

    impl_->scaler = 
        sws_getCachedContext(
            impl_->scaler,

            static_cast<int>(frame.width),
            static_cast<int>(frame.height),
            source_pixel_format,

            static_cast<int>(impl_->config.width),
            static_cast<int>(impl_->config.height),
            AV_PIX_FMT_NV12,

            SWS_BILINEAR,
            nullptr,
            nullptr,
            nullptr
        );
    
    if (impl_->scaler == nullptr) {
        throw std::runtime_error(
            "Falied to create pixel format converter."
        );
    }

    //Make sure the reusable frame buffer can be modified.
    if (av_frame_make_writable(impl_->frame) < 0) {
        throw std::runtime_error(
            "Output AVFrame is not writable."
        );
    }

    //Set the presentation timestamp(PTS) of this frame.
    impl_->frame->pts = 
        static_cast<std::int64_t>(
            impl_->next_frame_number
        );
    
    //Decide whether the current frame must be a key frame.
    const bool key_frame = 
        force_key_frame ||
        impl_->force_next_key_frame ||
        (
            impl_->next_frame_number %
            impl_->config.key_frame_interval
        ) == 0U;

    if (key_frame) {
        //Mark this frame as an intra frame (I-frame).
        impl_->frame->pict_type = 
            AV_PICTURE_TYPE_I;
    } else {
        //Let the encoder choose the normal predicted-frame type.
        impl_->frame->pict_type = 
            AV_PICTURE_TYPE_NONE;
    }

    //Convert the source image into the encoder's pixel formats.
    const int converted_height = 
        sws_scale(
            impl_->scaler,

            source_data,
            source_linesize,
            0,
            static_cast<int>(frame.height),

            impl_->frame->data,
            impl_->frame->linesize

        );
    
    //sws_scale() returns the number of output rows converted.
    if (
        converted_height !=
        static_cast<int>(impl_->config.height)
    ) {
        throw std::runtime_error(
            "Failed to convert input frame."
        );
    }

    //Submit one raw frame to the H.264 encoder.
    const int send_result = 
        avcodec_send_frame(
            impl_->codec_context,  //represent H.264 encoder.
            impl_->frame
        );

    if (send_result < 0) {
        throw std::runtime_error(
            "avcodec_send_frame failed: " + 
            FfmpegErrorToString(send_result)
        );
    }

    std::vector<EncodedVideoFrame> results = 
        ReceiveAvailablePackets(
            impl_->codec_context,
            impl_->packet,
            impl_->config
        );

    impl_->force_next_key_frame = false;
        
    ++impl_->next_frame_number;

    return results;
}

std::vector<EncodedVideoFrame> 
FfmpegVideoEncoder::PollEncodedFrames()
{
    return ReceiveAvailablePackets(
        impl_->codec_context,
        impl_->packet,
        impl_->config
    );
}


std::vector<EncodedVideoFrame>
FfmpegVideoEncoder::Flush()
{

    // Signal end-of-input to the encoder.
    const int send_result =
        avcodec_send_frame(
            impl_->codec_context,
            nullptr
        );

    if (send_result < 0) {
        throw std::runtime_error(
            "Failed to flush FFmpeg encoder: " +
            FfmpegErrorToString(send_result)
        );
    }

    return ReceiveAvailablePackets(
        impl_->codec_context,
        impl_->packet,
        impl_->config
    );

}

} //namespace aegis::media
