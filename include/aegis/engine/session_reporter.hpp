#pragma once

#include "aegis/engine/session.hpp"

namespace aegis::engine {

class SessionReporter final
{
public:
    static void PrintDrainResult(
        const ReceiveResult& receive_result,
        const RetransmissionResult& retransmission_result,
        const CleanupResult& cleanup_result,
        std::size_t queue_before,
        std::size_t queue_after
    );

    static void PrintFinalStatistics(
        const SessionStatistics& statistics,
        const aegis::network::NetworkSimulatorStats& network_stats,
        std::size_t remaining_queue
    );
};

} //namespace aegis::engine