#pragma once

#include "aegis/common.hpp"

#include <cstddef>

namespace aegis::network
{

// Configuration used by the RTT estimator.
struct RttEstimatorConfig
{
    // RTT value used before the first real sample arrives.
    aegis::Milliseconds initial_rtt{
        50
    };

    // Lower bound for RTT and RTO values.
    aegis::Milliseconds minimum_rtt{
        10
    };

    // Upper bound for RTT and RTO values.
    aegis::Milliseconds maximum_rtt{
        1000
    };

    // Weight of the newest RTT sample.
    //
    // 0.125 means:
    // 87.5% old value + 12.5% new value.
    double smoothing_factor{
        0.125
    };
};

class RttEstimator final
{
public:
    explicit RttEstimator(
        RttEstimatorConfig config = {}
    );

    // Add one measured RTT sample.
    void AddSample(
        aegis::Milliseconds sample
    );

    // Return true after at least one sample was added.
    [[nodiscard]] bool HasSample()
        const noexcept;

    // Return the smoothed RTT.
    //
    // Before the first sample, this returns initial_rtt.
    [[nodiscard]] aegis::Milliseconds
    SmoothedRtt()
        const noexcept;

    // Return the retransmission timeout.
    [[nodiscard]] aegis::Milliseconds
    Rto()
        const noexcept;

    // Return the number of accepted samples.
    [[nodiscard]] std::size_t
    SampleCount()
        const noexcept;

private:
    RttEstimatorConfig config_{};

    // Smoothed RTT stored as a floating-point value
    // to avoid losing precision during calculations.
    double smoothed_rtt_ms_{0.0};

    // Smoothed RTT variation.
    double rtt_variation_ms_{0.0};

    std::size_t sample_count_{0U};
};

} // namespace aegis::network