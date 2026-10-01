#include "aegis/engine/adaptive_controller.hpp"

#include <cassert>
#include <iostream>

int main()
{
    using aegis::engine::AdaptiveController;
    using aegis::engine::NetworkState;
    using aegis::network::NetworkQualitySnapshot;

    AdaptiveController controller;

    // Good network.
    NetworkQualitySnapshot good_quality{};
    good_quality.packet_loss_rate = 0.01;
    good_quality.average_latency_ms = 30.0;
    good_quality.jitter_ms = 5.0;
    good_quality.queue_is_growing = false;

    assert(
        controller.Evaluate(good_quality) ==
        NetworkState::kGood);

    const auto good_config =
        controller.ConfigForState(
            NetworkState::kGood);

    assert(good_config.width == 640U);
    assert(good_config.height == 480U);
    assert(good_config.frame_rate == 30U);

    // Fair network.
    NetworkQualitySnapshot fair_quality{};
    fair_quality.packet_loss_rate = 0.03;
    fair_quality.average_latency_ms = 90.0;
    fair_quality.jitter_ms = 10.0;

    assert(
        controller.Evaluate(fair_quality) ==
        NetworkState::kFair);

    const auto fair_config =
        controller.ConfigForState(
            NetworkState::kFair);

    assert(fair_config.frame_rate == 24U);
    assert(fair_config.target_bitrate_kbps == 1000U);

    // Poor network.
    NetworkQualitySnapshot poor_quality{};
    poor_quality.packet_loss_rate = 0.06;
    poor_quality.queue_is_growing = true;
    poor_quality.jitter_ms = 60.0;

    assert(
        controller.Evaluate(poor_quality) ==
        NetworkState::kPoor);

    const auto poor_config =
        controller.ConfigForState(
            NetworkState::kPoor);

    assert(poor_config.width == 480U);
    assert(poor_config.height == 360U);
    assert(poor_config.frame_rate == 20U);

    // Critical network.
    NetworkQualitySnapshot critical_quality{};
    critical_quality.packet_loss_rate = 0.12;
    critical_quality.average_latency_ms = 350.0;

    assert(
        controller.Evaluate(critical_quality) ==
        NetworkState::kCritical);

    const auto critical_config =
        controller.ConfigForState(
            NetworkState::kCritical);

    assert(critical_config.width == 320U);
    assert(critical_config.height == 240U);
    assert(critical_config.frame_rate == 15U);
    assert(critical_config.target_bitrate_kbps == 300U);

    std::cout
        << "Adaptive controller tests passed.\n";

    return 0;
}