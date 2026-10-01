#include "aegis/engine/nack.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <iostream>

int main() {
    using namespace aegis::engine;

    const TimePoint base_time{};

    NackController controller;

    controller.ObserveGap(
        100U,
        101U,
        base_time
    );

    const auto continuous_result = 
        controller.Poll(
            base_time + 
            std::chrono::milliseconds{100}
        );

    assert(
        continuous_result.empty()
    );

    controller.ObserveGap(
        100U,
        103U,
        base_time
    );

    const auto early_nacks = 
        controller.Poll(
            base_time + 
            std::chrono::milliseconds{10}
        );

    assert(
        early_nacks.empty()
    );

    const auto first_nacks = 
        controller.Poll(
            base_time +
            std::chrono::milliseconds{20}
        );

    assert(
        !first_nacks.empty()
    );

    assert(
        first_nacks.size() == 2U
    );

    assert(
        first_nacks[0] == 101U
    );

    assert(
        first_nacks[1] == 102U
    );

    const auto too_early_retry = 
        controller.Poll(
            base_time +
            std::chrono::milliseconds{40}
        );
    
    assert(
        too_early_retry.empty()
    );
    
    const auto second_nacks = 
        controller.Poll(
            base_time +
            std::chrono::milliseconds{70}
        );

    assert(
        !second_nacks.empty()
    );

    assert(
        second_nacks.size() == 2U
    );

    assert(
        second_nacks[0] == 101U
    );

    assert(
        second_nacks[1] == 102U
    );

    controller.OnPacketRecovered(
        101U
    );

    const auto after_retry = 
        controller.Poll(
            base_time +
            std::chrono::milliseconds{120}
        );

    assert(
        after_retry.size() == 1U
    );

    assert(
        after_retry[0] == 102U
    );

    const auto exhausted = 
        controller.Poll(
            base_time +
            std::chrono::milliseconds{200}
        );

    assert(
        exhausted.empty()
    );

    const NackConfig expire_config{
        .reorder_wait = std::chrono::milliseconds{10},
        .retry_interval = std::chrono::milliseconds{20},
        .max_missing_age = std::chrono::milliseconds{100},
        .max_retries = 3U,
        .max_tracked_missing_packets = 16U,
        .max_nack_batch_size = 8U
    };

    NackController expire_controller(
        expire_config
    );

    expire_controller.ObserveGap(
        200U,
        202U,
        base_time
    );
    
    const auto not_expired = 
        expire_controller.Expire(
            base_time + 
            std::chrono::milliseconds{50}
        );

    assert(
        not_expired == 0U
    );

    const auto expired = 
        expire_controller.Expire(
            base_time +
            std::chrono::milliseconds{101}
        );

    assert(
        expired == 1U
    );
    
    std::wcout
        << L"Nack controller tests passed.\n";
}