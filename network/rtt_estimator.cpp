#include "aegis/network/rtt_estimator.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace aegis::network
{

RttEstimator::RttEstimator(
    RttEstimatorConfig config
)
    : config_(std::move(config))
{
    if (
        config_.initial_rtt.count() <= 0 ||
        config_.minimum_rtt.count() <= 0 ||
        config_.maximum_rtt <
            config_.minimum_rtt
    )
    {
        throw std::invalid_argument(
            "Invalid RTT duration configuration."
        );
    }

    if (
        config_.smoothing_factor <= 0.0 ||
        config_.smoothing_factor > 1.0
    )
    {
        throw std::invalid_argument(
            "RTT smoothing factor must be in the range (0, 1]."
        );
    }
}

void RttEstimator::AddSample(
    aegis::Milliseconds sample
)
{
    if (sample.count() <= 0)
    {
        throw std::invalid_argument(
            "RTT sample must be positive."
        );
    }

    const double sample_ms =
        static_cast<double>(
            sample.count()
        );

    const double alpha =
        config_.smoothing_factor;

    if (sample_count_ == 0U)
    {
        // The first sample initializes the estimator.
        smoothed_rtt_ms_ =
            sample_ms;

        // Initial variation is commonly
        // initialized to half of the first sample.
        rtt_variation_ms_ =
            sample_ms / 2.0;
    }
    else
    {
        const double difference =
            std::abs(
                smoothed_rtt_ms_ -
                sample_ms
            );

        // Update RTT variation first.
        rtt_variation_ms_ =
            (1.0 - alpha) *
                rtt_variation_ms_ +
            alpha *
                difference;

        // Update the smoothed RTT.
        smoothed_rtt_ms_ =
            (1.0 - alpha) *
                smoothed_rtt_ms_ +
            alpha *
                sample_ms;
    }

    ++sample_count_;
}

bool RttEstimator::HasSample()
    const noexcept
{
    return sample_count_ != 0U;
}

aegis::Milliseconds
RttEstimator::SmoothedRtt()
    const noexcept
{
    const double value =
        HasSample()
            ? smoothed_rtt_ms_
            : static_cast<double>(
                config_.initial_rtt.count()
            );

    const double bounded_value =
        std::clamp(
            value,
            static_cast<double>(
                config_.minimum_rtt.count()
            ),
            static_cast<double>(
                config_.maximum_rtt.count()
            )
        );

    return aegis::Milliseconds{
        static_cast<
            aegis::Milliseconds::rep
        >(
            std::lround(
                bounded_value
            )
        )
    };
}

aegis::Milliseconds
RttEstimator::Rto()
    const noexcept
{
    const double base_rtt =
        HasSample()
            ? smoothed_rtt_ms_
            : static_cast<double>(
                config_.initial_rtt.count()
            );

    const double calculated_rto =
        base_rtt +
        4.0 * rtt_variation_ms_;

    const double bounded_rto =
        std::clamp(
            calculated_rto,
            static_cast<double>(
                config_.minimum_rtt.count()
            ),
            static_cast<double>(
                config_.maximum_rtt.count()
            )
        );

    return aegis::Milliseconds{
        static_cast<
            aegis::Milliseconds::rep
        >(
            std::lround(
                bounded_rto
            )
        )
    };
}

std::size_t RttEstimator::SampleCount()
    const noexcept
{
    return sample_count_;
}

} // namespace aegis::network