#include "aegis/media/simulated_video_encoder.hpp"

#include <stdexcept>
#include <utility>
#include <algorithm>
#include <cmath>

namespace aegis::media {

SimulatedVideoEncoder::SimulatedVideoEncoder(
    SimulatedVideoEncoderConfig config
)
    : config_(std::move(config)) {
    if(config_.key_frame_interval == 0U) {
        throw std::invalid_argument(
            "Key frame interval must be greater than zero."
        );
    }

    if(config_.key_frame_size_multiplier < 1.0) {
        throw std::invalid_argument(
            "Key frame size multiplier must be at least 1.0."
        );
    }

    if(config_.max_encoded_frame_bytes == 0U) {
        throw std::invalid_argument(
            "Max encoded frame bytes must be greater than zero."
        );
    }

    if(config_.target_bitrate_kbps == 0U) {
        throw std::invalid_argument(
            "Target bitrate must be greater than zero."
        );
    }
}

std::vector<EncodedVideoFrame> 
SimulatedVideoEncoder::Encode(
        const CapturedVideoFrame& frame,
        bool force_key_frame
    ) {
        if(frame.width == 0U || frame.height == 0U){
            throw std::invalid_argument(
                "Input video frame has invalid dimensions."
            );
        }

        if(frame.data.empty()) {
            throw std::invalid_argument(
                "Input video frame contains no data."
            );
        }

        const bool interval_key_frame = 
            (next_frame_number_ %
             config_.key_frame_interval) == 0U;
        
        const bool key_frame = 
            force_key_frame || 
            force_next_key_frame_ ||
            interval_key_frame;
        
        const double bitrate_scale = 
                static_cast<double>(
                    config_.target_bitrate_kbps
                ) / 1500.0;

        const std::size_t base_encoded_size = 
                std::max<std::size_t>(
                    1U,
                    static_cast<std::size_t>(
                        static_cast<double>(
                            frame.data.size() / 8U
                        ) * bitrate_scale
                    )
                );

        const double scaled_size = 
                key_frame
                    ? static_cast<double>(base_encoded_size) *
                        config_.key_frame_size_multiplier
                    : static_cast<double>(base_encoded_size);
        
        const std::size_t encoded_size = 
                std::clamp<std::size_t>(
                    static_cast<std::size_t>(
                        std::ceil(scaled_size)
                    ),
                    1U,
                    config_.max_encoded_frame_bytes 
                );
        
        std::vector<std::uint8_t> encoded_data(
            encoded_size
        );

        for(std::size_t index = 0;index < encoded_size; ++index) {
            const std::size_t pattern = 
                (index + 
                 static_cast<std::size_t>(
                    next_frame_number_ * 31U
                 )) % 256U;
                
            encoded_data[index] = 
                 static_cast<std::uint8_t>(pattern);
        }

        EncodedVideoFrame result{
            .data = std::move(encoded_data),
            .width = frame.width,
            .height = frame.height,
            .timestamp_100ns = frame.timestamp_100ns,
            .frame_number = next_frame_number_,
            .key_frame = key_frame
        };

        std::vector<EncodedVideoFrame> results;

        results.push_back(result);

        force_next_key_frame_ = false;
        ++next_frame_number_;

        return results;
    }

void SimulatedVideoEncoder::SetTargetBitrate(
    std::uint32_t bitrate_kbps
) {

    if (bitrate_kbps == 0U) {
        throw std::invalid_argument(
            "Target bitrate must be greater than zero."
        );
    }

    config_.target_bitrate_kbps = bitrate_kbps;
}


std::vector<EncodedVideoFrame>
SimulatedVideoEncoder::PollEncodedFrames()
{
    // The simulated encoder produces output immediately.
    // It does not keep delayed packets internally.
    return {};
}

void SimulatedVideoEncoder::RequestKeyFrame() {
    force_next_key_frame_ = true;
}

std::uint32_t SimulatedVideoEncoder::TargetBitrate()
const noexcept {

    return config_.target_bitrate_kbps;
}

std::vector<EncodedVideoFrame>
SimulatedVideoEncoder::Flush()
{
    //The sinulated encoder has no delayed output.
    return {};
}

}