#pragma once

#include "aegis/common.hpp"
#include "aegis/engine/adaptive_controller.hpp"
#include "aegis/engine/nack.hpp"
#include "aegis/media/video_capturer.hpp"
#include "aegis/media/video_capturer_factory.hpp"
#include "aegis/media/video_encoder_factory.hpp"
#include "aegis/media/video_encoder.hpp"
#include "aegis/network/frame_reassembler.hpp"
#include "aegis/network/network_monitor.hpp"
#include "aegis/network/network_simulator.hpp"
#include "aegis/network/retransmission_cache.hpp"
#include "aegis/network/video_packetizer.hpp"
#include "aegis/network/wire_protocol.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

namespace aegis::engine {

// Runtime configuration for one complete media session.
struct SessionConfig
{
    // Video configuration.
    std::uint32_t width{640};
    std::uint32_t height{480};
    std::uint32_t frame_rate{30};
    std::uint32_t target_bitrate_kbps{1500};
    std::uint32_t key_frame_interval{60};

    // Encoder implementation selected by the factory.
    aegis::media::VideoEncoderBackend encoder_backend{
        aegis::media::VideoEncoderBackend::kFfmpeg
    };

    // Capturer implementation selected by the factory
    aegis::media::VideoCapturerBackend capturer_backend{
        aegis::media::VideoCapturerBackend::kMediaFoundation
    };

    // Number of frames processed by Run().
    std::size_t frame_count{10U};

    // Interval between two capture iterations.
    std::chrono::milliseconds frame_interval{
        std::chrono::milliseconds{33}
    };

    // Packetization and network simulation parameters.
    std::size_t max_payload_bytes{1200U};
    std::uint64_t bandwidth_bps{1'500'000U};

    std::chrono::milliseconds network_base_delay{
        std::chrono::milliseconds{30}
    };

    std::chrono::milliseconds network_jitter{
        std::chrono::milliseconds{5}
    };

    double network_random_loss_rate{0.05};
    std::uint32_t network_random_seed{20260902U};

    // Retransmission cache parameters.
    std::size_t retransmission_cache_packets{512U};

    std::chrono::milliseconds retransmission_cache_age{
        std::chrono::milliseconds{1000}
    };

    // Maximum time allowed for draining delayed packets.
    std::chrono::seconds drain_timeout{
        std::chrono::seconds{10}
    };
};

// Result of packetizing and sending one encoded frame.
struct FrameSendResult
{
    std::size_t encoded_bytes{0U};
    std::size_t fragment_count{0U};
    std::size_t accepted_packets{0U};
    std::size_t dropped_packets{0U};
};

// Result of processing packets delivered by the network.
struct ReceiveResult
{
    std::size_t received_packets{0U};
    std::size_t decoded_packets{0U};
    std::size_t invalid_packets{0U};
    std::size_t retransmitted_packets{0U};
    std::size_t original_packets{0U};
    std::size_t completed_frames{0U};
};

// Result of processing NACK feedback.
struct RetransmissionResult
{
    std::size_t requested{0U};
    std::size_t cache_hits{0U};
    std::size_t cache_misses{0U};
    std::size_t accepted{0U};
    std::size_t dropped{0U};
};

// Result of expiring stale state.
struct CleanupResult
{
    std::size_t expired_frames{0U};
    std::size_t expired_missing_packets{0U};
    std::size_t expired_cache_packets{0U};
};

// Result of one adaptive-control evaluation.
struct AdaptationResult
{
    NetworkState state{NetworkState::kGood};
    AdaptiveVideoConfig config{};
    bool state_changed{false};
    bool key_frame_requested{false};
};

// Session-wide counters and metrics.
struct SessionStatistics
{
    std::uint64_t decoded_packets{0U};
    std::uint64_t invalid_packets{0U};
    std::uint64_t completed_frames{0U};

    std::uint64_t original_packets{0U};
    std::uint64_t retransmitted_packets{0U};

    std::uint64_t retransmission_cache_hits{0U};
    std::uint64_t retransmission_cache_misses{0U};

    std::uint64_t encoded_bytes{0U};
    std::uint64_t encoded_frames{0U};
    std::uint64_t fragments{0U};

    std::uint64_t capture_empty_reads{0U};
    std::uint64_t encoder_delayed_calls{0U};
};

class Session final
{
public:
    // Create a session using the supplied runtime configuration.
    explicit Session(SessionConfig config = {});

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    ~Session();

    // Run capture, encoding, transport, feedback, and cleanup.
    void Run();

    // Close the camera and release session resources.
    void Close() noexcept;

    // Return read-only session statistics.
    [[nodiscard]] const SessionStatistics&
    Statistics() const noexcept;

private:
    // Camera and encoding pipeline.
    void OpenCamera();

    void ProcessOneFrame(
        std::size_t frame_index
    );

    void FlushEncoder();

    // Network draining and final reporting.
    void DrainNetwork();

    void PrintFinalStatistics() const;

    // Internal packet-processing operations.
    [[nodiscard]] std::optional<
        aegis::media::CapturedVideoFrame
    > ReadFrame();

    [[nodiscard]] FrameSendResult
    SendEncodedFrame(
        const aegis::media::EncodedVideoFrame& frame,
        aegis::TimePoint send_time
    );

    [[nodiscard]] ReceiveResult
    ProcessReadyPackets(
        aegis::TimePoint arrival_time
    );

    [[nodiscard]] RetransmissionResult
    ProcessNackRequests(
        aegis::TimePoint send_time
    );

    [[nodiscard]] CleanupResult
    Expire(aegis::TimePoint now);

    [[nodiscard]] aegis::network::NetworkQualitySnapshot
    UpdateNetworkQuality(
        std::uint64_t retransmission_packets,
        std::uint64_t cache_hits,
        std::uint64_t cache_misses
    );

    [[nodiscard]] AdaptationResult
    EvaluateAdaptation(
        const aegis::network::NetworkQualitySnapshot& quality
    );

private:
    // Immutable session behavior after construction.
    SessionConfig config_;

    // Capture and encoding components.
    std::unique_ptr<
        aegis::media::IVideoCapturer
    > video_capturer_;

    std::unique_ptr<
        aegis::media::IVideoEncoder
    > video_encoder_;

    // Sending and retransmission components.
    aegis::network::VideoPacketizer packetizer_;
    aegis::network::NetworkSimulator network_simulator_;
    aegis::network::RetransmissionCache retransmission_cache_;

    // Receiving and packet recovery components.
    aegis::network::FrameReassembler frame_reassembler_;
    aegis::engine::NackController nack_controller_;

    std::optional<std::uint16_t>
        highest_received_sequence_;

    // Monitoring and adaptive-control components.
    aegis::network::NetworkMonitor network_monitor_;
    aegis::engine::AdaptiveController adaptive_controller_;

    NetworkState previous_network_state_{
        NetworkState::kGood
    };

    // Cumulative counters for the entire session.
    SessionStatistics statistics_;
};

} // namespace aegis::engine