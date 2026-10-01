#pragma once

#include "aegis/network/network_monitor.hpp"

#include <cstdint>

namespace aegis::engine {

//Describes the current network condition.
enum class NetworkState : std::uint8_t {
    kGood,
    kFair,
    kPoor,
    kCritical
};

//Video parameters controlled by the adaptive strategy.
struct AdaptiveVideoConfig {
    std::uint32_t width{640};
    std::uint32_t height{480};
    std::uint32_t frame_rate{30};
    std::uint32_t target_bitrate_kbps{1500};
    std::uint32_t key_frame_interval{60};
};

//Selects a network state and provides adaptive video parameters.
class AdaptiveController final {
public:
    explicit AdaptiveController(
        AdaptiveVideoConfig config = {}
    );

    //Evaluate the current network condition.
    //
    //This function updates the controller's current state.
    [[nodiscard]] NetworkState Evaluate(
        const aegis::network::NetworkQualitySnapshot& quality
    ) noexcept;

    //Return the current network state.
    [[nodiscard]] NetworkState 
    CurrentState() const noexcept;

    //Return the current video configuration.
    [[nodiscard]] const AdaptiveVideoConfig&
    CurrentConfig() const noexcept;

    //Return video parameters recommended for a network state.
    [[nodiscard]] AdaptiveVideoConfig ConfigForState(
        NetworkState state
    ) const noexcept;

private:
    AdaptiveVideoConfig config_;

    NetworkState current_state_{NetworkState::kGood};
} ;


} //namespace aegis::engine
