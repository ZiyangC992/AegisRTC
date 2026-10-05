#include "aegis/engine/session_reporter.hpp"

#include <iostream>

namespace aegis::engine{

void SessionReporter::PrintDrainResult(
        const ReceiveResult& receive_result,
        const RetransmissionResult& retransmission_result,
        const CleanupResult& cleanup_result,
        std::size_t queue_before,
        std::size_t queue_after)
{

    std::wcout
        << L"[DRAIN] queue before = "
        << queue_before
        << L", received = "
        << receive_result.received_packets
        << L", NACK requests = "
        << retransmission_result.requested
        << L", retransmission accepted = "
        << retransmission_result.accepted
        << L", retransmission dropped = "
        << retransmission_result.dropped
        << L", queue after = "
        << queue_after
        << L'\n';

    if (receive_result.received_packets > 0U)
    {
        std::wcout
            << L"[DRAIN] delivered = "
            << receive_result.received_packets
            << L", remaining queue = "
            << queue_after
            << L'\n';
    }

    if (cleanup_result.expired_frames > 0U)
    {
        std::wcout
            << L"[CLEANUP] expired incomplete frames = "
            << cleanup_result.expired_frames
            << L'\n';
    }

    if (cleanup_result.expired_missing_packets > 0U)
    {
        std::wcout
            << L"[CLEANUP] expired missing records = "
            << cleanup_result.expired_missing_packets
            << L'\n';
    }

    if (cleanup_result.expired_cache_packets > 0U)
    {
        std::wcout
            << L"[CLEANUP] expired retransmission records = "
            << cleanup_result.expired_cache_packets
            << L'\n';
    }
}

void SessionReporter::PrintFinalStatistics(
    const SessionStatistics &statistics,
    const aegis::network::NetworkSimulatorStats &network_stats,
    std::size_t remaining_queue)
{

    std::wcout
        << L"\n========== Final Statistics ==========\n"
        << L"Packets sent       : "
        << network_stats.packets_sent
        << L'\n'
        << L"Packets dropped    : "
        << network_stats.packets_dropped
        << L'\n'
        << L"Packets delivered  : "
        << network_stats.packets_delivered
        << L'\n'
        << L"Decoded packets    : "
        << statistics.decoded_packets
        << L'\n'
        << L"Completed frames   : "
        << statistics.completed_frames
        << L'\n'
        << L"RTT samples       : "
        << statistics.rtt_samples
        << L'\n'
        << L"Smoothed RTT      : "
        << statistics.smoothed_rtt_ms
        << L" ms\n"
        << L"Retransmission RTO: "
        << statistics.retransmission_timeout_ms
        << L" ms\n"
        << L"Remaining queue    : "
        << remaining_queue
        << L'\n';
}

} //namespace aegis::engine