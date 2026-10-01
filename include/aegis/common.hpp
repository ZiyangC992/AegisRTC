#pragma once

#include <chrono>
#include <cstdint>

namespace aegis {

// Monotonic clock used by all real-time modules.
using Clock = std::chrono::steady_clock;

// Absolute point on the monotonic timeline.
using TimePoint = Clock::time_point;

// Duration used for network delay and timeout configuration.
using Milliseconds = std::chrono::milliseconds;

// Convert a time point to milliseconds since the clock epoch.
[[nodiscard]] inline std::int64_t ToMilliseconds(
    TimePoint time_point
) noexcept {
    return std::chrono::duration_cast<
        std::chrono::milliseconds
    >(
        time_point.time_since_epoch()
    ).count();
}

} // namespace aegis