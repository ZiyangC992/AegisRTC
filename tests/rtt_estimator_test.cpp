#include "aegis/network/rtt_estimator.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>

int main()
{
    using aegis::Milliseconds;
    using aegis::network::RttEstimator;
    using aegis::network::RttEstimatorConfig;

    // -----------------------------------------------
    // Test 1: Verify the initial state before any RTT sample.
    // -----------------------------------------------

    RttEstimator estimator{
        RttEstimatorConfig{
            .initial_rtt = 
                Milliseconds{50},

            .minimum_rtt = 
                Milliseconds{10},
            
            .maximum_rtt = 
                Milliseconds{1000},

            .smoothing_factor = 
                0.125
        }
    };

    assert(
        !estimator.HasSample()
    );

    assert(
        estimator.SampleCount() == 0U
    );

    // No sample exists yet, so initial_rtt is returned.
    assert(
        estimator.SmoothedRtt() == 
        Milliseconds{50}
    );

    // There is no variation before the first smaple.
    assert(
        estimator.Rto() == Milliseconds{50}
    );

    // -----------------------------------------------
    // Test 2: Verify the first RTT sample.
    // -----------------------------------------------

    estimator.AddSample(
        Milliseconds{100}
    );

    assert(estimator.HasSample());
    assert(estimator.SampleCount() == 1U);

    // The first sample directly initializes SRTT.
    assert(estimator.SmoothedRtt() == 
    Milliseconds{100});

    // Initial variation is 100 / 2 = 50.
    //
    // RTO = SRTT + 4 * variation
    //     = 100 + 4 * 50
    //     = 300ms
    assert(estimator.Rto() == 
        Milliseconds{300});

    // -----------------------------------------------
    // Test 3: Verify smoothing with more samples.
    // -----------------------------------------------
    estimator.AddSample(
        Milliseconds{80}
    );

    assert(
        estimator.HasSample()
    );

    assert(estimator.SampleCount() == 2U);

    // SRTT = (1 - alpha) * smoothed_rtt_ms_(initial) + alpha * sample_ms
    // smoothed_rtt_ms_(initial) =  100
    // sample_ms = 80
    // SRTT = 0.875 * 100 + 0.125 * 80 
    assert(estimator.SmoothedRtt() > 
    Milliseconds{80});

    assert(estimator.SmoothedRtt() < 
    Milliseconds{100});

    // RTO = smoothed_rtt_ms_ + 4 * rtt_variation_ms_
    assert(estimator.Rto() >
           estimator.SmoothedRtt());

    estimator.AddSample(
        Milliseconds{120}
    );

    assert(estimator.SampleCount() == 3U);

    assert(estimator.SmoothedRtt() <
    Milliseconds{120});

    assert(estimator.SmoothedRtt() > 
    Milliseconds{80});

    assert(estimator.Rto() >=
        estimator.SmoothedRtt());

    // -----------------------------------------------
    // Test 4: Verify the minimum and maximum bounds
    // -----------------------------------------------

    RttEstimator bounded_estimator{
        RttEstimatorConfig{
            .initial_rtt = 
                Milliseconds{50},
            .minimum_rtt = 
                Milliseconds{10},
            .maximum_rtt = 
                Milliseconds{200},
            .smoothing_factor = 
                0.125
        }
    };

    bounded_estimator.AddSample(
        Milliseconds{1000}
    );

    assert(bounded_estimator.HasSample());
    assert(bounded_estimator.SampleCount() == 1U);

    // SRTT cannot exceed the configured maximum
    assert(bounded_estimator.SmoothedRtt() == 
        Milliseconds{200});

    // RTO cannot exceed the configured maximum
    assert(bounded_estimator.Rto() == 
        Milliseconds{200});

    // -----------------------------------------------
    // Test 5: Verify invalid RTT samples.
    // -----------------------------------------------
    bool invalid_sample_thrown = false;

    try
    {
        estimator.AddSample(
            Milliseconds{0}
        );
    }
    catch (const std::invalid_argument)
    {
        invalid_sample_thrown = true;
    }

    assert(invalid_sample_thrown);

    // -----------------------------------------------
    // Test 6: Verify invalid estimator configuration
    // -----------------------------------------------
    bool invalid_config_thrown = false;
    try
    {
        RttEstimator invalid_estimator{
            RttEstimatorConfig{
                .initial_rtt = 
                    Milliseconds{50},
                .minimum_rtt = 
                    Milliseconds{100},
                .maximum_rtt =  
                    Milliseconds{10},
                .smoothing_factor = 
                    0.125
            }
        };
    }
    catch(const std::invalid_argument)
    {
        invalid_config_thrown = true;
    }
    
    assert(invalid_config_thrown);

    std::cout
        << "RTT estimator tests passed.\n";

    return 0;
}