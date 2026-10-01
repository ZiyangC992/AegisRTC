#include "aegis/engine/adaptive_controller.hpp"

#include <stdexcept>
#include <utility>
#include <iostream>

namespace aegis::engine {

AdaptiveController::AdaptiveController(
    AdaptiveVideoConfig config) 
    : config_(std::move(config))
{
    if (config_.width == 0U){
        throw std::invalid_argument(
            "Adaptive video width must be greater than zero."
        );
    }

    if (config_.height == 0U) {
        throw std::invalid_argument(
            "Adaptive video height must be greater than zero."
        );
    }

    if (config_.frame_rate == 0U) {
        throw std::invalid_argument(
            "Adaptive video frame rate must be greater than zero."
        );
    }

    if (config_.target_bitrate_kbps == 0U) {
        throw std::invalid_argument(
            "Adaptive video bitrate must be greater than zero."
        );
    }

    if (config_.key_frame_interval == 0U) {
        throw std::invalid_argument(
            "Adaptive key-frame interval must be greater than zero."
        );
    }

}

NetworkState AdaptiveController::Evaluate(
    const aegis::network::NetworkQualitySnapshot& quality
) noexcept {

    //Critical network condition.
    if (
        quality.packet_loss_rate >= 0.10 ||
        quality.average_latency_ms >= 300.0
    ) {
        current_state_ = 
            NetworkState::kCritical;

        return current_state_;
    }

    //Poor network condition.
    if (
        quality.packet_loss_rate >= 0.05 ||
        quality.queue_is_growing ||
        quality.jitter_ms >=50.0
    ) {
        current_state_ = 
            NetworkState::kPoor;

        return current_state_;
    }

    //Fair network condition.
    if (
        quality.packet_loss_rate >= 0.02 ||
        quality.average_latency_ms >= 80.0 ||
        quality.jitter_ms >= 20.0
    ) {
        current_state_ = 
            NetworkState::kFair;

        return current_state_;
    }

    //Good network condition.
    current_state_ = 
        NetworkState::kGood;

    return current_state_;
}

NetworkState AdaptiveController::CurrentState() 
    const noexcept
    {
        return current_state_;
}

const AdaptiveVideoConfig& 
AdaptiveController::CurrentConfig()
    const noexcept
    {
        return config_;
}

AdaptiveVideoConfig 
AdaptiveController::ConfigForState(
    NetworkState state
) const noexcept {
    AdaptiveVideoConfig adjusted_config = 
        config_;

    switch(state) 
    {
    case NetworkState::kGood:
        //Keep the default high-quality configuration.
        break;

    case NetworkState::kFair:
        //Slightly reduce frame rate and bitrate.
        adjusted_config.width = 640U;
        adjusted_config.height = 480U;
        adjusted_config.frame_rate = 24U;
        adjusted_config.target_bitrate_kbps = 1000U;
        adjusted_config.key_frame_interval = 60U;
        break;
    
    case NetworkState::kPoor:
        //Reduce resolution, frame raye and bitrate.
        adjusted_config.width = 480U;
        adjusted_config.height = 360U;
        adjusted_config.frame_rate = 20U;
        adjusted_config.target_bitrate_kbps = 600U;
        adjusted_config.key_frame_interval = 90U;
        break;

    case NetworkState::kCritical:
        //Use a low-quality profile to perserve continuity.
        adjusted_config.width = 320U;
        adjusted_config.height = 240U;
        adjusted_config.frame_rate = 15U;
        adjusted_config.target_bitrate_kbps = 300U;
        adjusted_config.key_frame_interval = 120U;
        break;
    
    }

    return adjusted_config;
}

}