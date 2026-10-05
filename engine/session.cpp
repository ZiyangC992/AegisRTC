#include "aegis/engine/session.hpp"
#include "aegis/engine/session_reporter.hpp"
#include "aegis/media/video_encoder_factory.hpp"
#include "aegis/media/video_capturer_factory.hpp"
#include "aegis/network/udp_transport.hpp"

#include <stdexcept>
#include <utility>
#include <iostream>
#include <thread>

namespace aegis::engine {

namespace
{

std::unique_ptr<
    aegis::network::INetworkTransport
>
CreateNetworkTransport(
    const SessionConfig& config
)
{
    if (config.network_backend ==
        NetworkBackend::kUdp)
    {
        return std::make_unique<
            aegis::network::UdpTransport
        >(
            aegis::network::UdpTransportConfig{
                .local_address =
                    config.udp_local_address,

                .local_port =
                    config.udp_local_port,

                .remote_address =
                    config.udp_remote_address,

                .remote_port =
                    config.udp_remote_port
            }
        );
    }

    return std::make_unique<
        aegis::network::NetworkSimulator
    >(
        aegis::network::NetworkSimulatorConfig{
            .bandwidth_bps =
                config.bandwidth_bps,

            .base_delay =
                config.network_base_delay,

            .jitter =
                config.network_jitter,

            .random_loss_rate =
                config.network_random_loss_rate,

            .random_seed =
                config.network_random_seed
        }
    );
}

} // namespace

Session::Session(
    SessionConfig config
) :
    config_(std::move(config)),
    packetizer_(
            aegis::network::VideoPacketizerConfig{
                .max_payload_bytes = 
                    config_.max_payload_bytes
            }
    ),
    network_transport_(
    	CreateNetworkTransport(config_)),
    retransmission_cache_(
        aegis::network::RetransmissionCacheConfig{
            .max_packets = 
                config_.retransmission_cache_packets,

            .max_packet_age = 
                config_.retransmission_cache_age
        }
    ),
    adaptive_controller_(
        aegis::engine::AdaptiveVideoConfig{
            .width = config_.width,
            .height = config_.height,
            .frame_rate = config_.frame_rate,
            .target_bitrate_kbps = 
                config_.target_bitrate_kbps,
            .key_frame_interval = 
                config_.key_frame_interval
        }
    )
{
    if (config_.width == 0U ||
        config_.height == 0U ||
        config_.frame_rate == 0U ||
        config_.target_bitrate_kbps == 0U ||
        config_.key_frame_interval == 0U)
    {
        throw std::invalid_argument(
            "Invalid session configuration.");
    }

    if (config_.frame_count == 0U)
    {
        throw std::invalid_argument(
            "Session frame count must be greater than zero."
        );
    }

    if (config_.frame_interval.count() <= 0U)
    {
        throw std::invalid_argument(
            "Session frame interval must be positive.");
    }

    if (config_.bandwidth_bps == 0U)
    {
        throw std::invalid_argument(
            "Network bandwidth must be greater than zero.");
    }

    if (config_.network_random_loss_rate < 0.0 ||
        config_.network_random_loss_rate > 1.0)
    {
        throw std::invalid_argument(
            "Network loss rate must be between 0 and 1.");
    }

    if (config_.max_payload_bytes == 0U)
    {
        throw std::invalid_argument(
            "Maximum payload size must be greater than zero."
        );
    }

    if (config_.drain_timeout.count() <= 0) 
    {
        throw std::invalid_argument(
            "Drain timeout must be positive."
        );
    }

    const aegis::media::VideoEncoderConfig
        encoder_config{
            .backend = 
                config_.encoder_backend,
            .width = config_.width,
            .height = config_.height,
            .frame_rate = config_.frame_rate,
            .target_bitrate_kbps = 
                config_.target_bitrate_kbps,
            .key_frame_interval = 
                config_.key_frame_interval
    };

    video_encoder_ = 
        aegis::media::CreateVideoEncoder(
            encoder_config
        );

    video_capturer_ = 
        aegis::media::CreateVideoCapturer(
            aegis::media::VideoCapturerConfig{
                .backend = 
                    config_.capturer_backend
            }
        );
}

Session::~Session() = default;

void Session::OpenCamera()
{   
        
    const bool opened = 
        video_capturer_->Open(
            config_.width,
            config_.height,
            config_.frame_rate
        );
    
    if (!opened)
    {
        throw std::runtime_error(
            "Failed to open camera."
        );
    }

    std::wcout
        << L"Camera opened successfully.\n";
}

void Session::ProcessOneFrame(
    std::size_t /*frame_index*/)
{
    const auto captured_frame = 
        video_capturer_->ReadFrame();

    if (!captured_frame.has_value()) {
        ++statistics_.capture_empty_reads;

        std::this_thread::sleep_for(
            config_.frame_interval);

        return ;
    }

    const auto encoded_frames = 
        video_encoder_->Encode(
            *captured_frame,
            false);
        
    const auto pending_frames = 
            video_encoder_->PollEncodedFrames();

    if (encoded_frames.empty() &&
        pending_frames.empty())
    {
        ++statistics_.encoder_delayed_calls;
    }

    for (const auto& encoded_frame :
         encoded_frames)
    {
        static_cast<void>(SendEncodedFrame(
            encoded_frame,
            aegis::network::Clock::now()
        ));
    }

    for (const auto& pending_frame :
         pending_frames)
    {
        static_cast<void>(SendEncodedFrame(
            pending_frame,
            aegis::network::Clock::now()
        ));
    }

    const aegis::TimePoint now = 
        aegis::network::Clock::now();

    static_cast<void>(ProcessReadyPackets(now));
    static_cast<void>(ProcessNackRequests(now));
    static_cast<void>(Expire(now));

    const auto quality = 
        UpdateNetworkQuality(
            statistics_.retransmitted_packets,
            statistics_.retransmission_cache_hits,
            statistics_.retransmission_cache_misses
        );

    static_cast<void>(EvaluateAdaptation(quality));

    std::this_thread::sleep_for(
        config_.frame_interval
    );

}

void Session::FlushEncoder()
{
    const auto flush_frames = 
        video_encoder_->Flush();

    for (const auto& flush_frame:
         flush_frames)
    {
        static_cast<void>(
            SendEncodedFrame(flush_frame,
            aegis::network::Clock::now())
        );
    }
}

void Session::DrainNetwork()
{
    const aegis::TimePoint drain_deadline = 
        aegis::network::Clock::now() + 
        config_.drain_timeout;

    while(
        network_transport_->queued_packets() > 0U)
    {
        const aegis::TimePoint now = 
            aegis::network::Clock::now();

        if (now >= drain_deadline)
        {
            throw std::runtime_error(
                "Timed out while draining network queue."
            );
        }

        const std::size_t queue_before =
            network_transport_->queued_packets();

        const auto receive_result = 
            ProcessReadyPackets(now);

        const auto retransmission_result = 
            ProcessNackRequests(now);

        const auto cleanup_result = 
            Expire(now);

        const std::size_t queue_after = 
            network_transport_->queued_packets();

        SessionReporter::PrintDrainResult(
            receive_result,
            retransmission_result,
            cleanup_result,
            queue_before,
            queue_after
        );

        std::this_thread::sleep_for(
            config_.frame_interval
        );
    }
}

void Session::PrintFinalStatistics() const
{
    const auto network_stats =
        network_transport_->stats();

    const auto &statistics =
        statistics_;

    SessionReporter::PrintFinalStatistics(
        statistics,
        network_stats,
        network_transport_->queued_packets()
    );
}

[[nodiscard]] const SessionStatistics&
Session::Statistics() const noexcept
{
    return statistics_;
}

void Session::Run()
{
    try
    {
        OpenCamera();

        for (std::size_t frame_index = 0U;
             frame_index < config_.frame_count;
             ++frame_index)
        {
            ProcessOneFrame(frame_index);
        }

        FlushEncoder();
        DrainNetwork();
        PrintFinalStatistics();
    }
    catch (...)
    {
        Close();
        throw;
    }

    Close();
}

[[nodiscard]] std::optional<
    aegis::media::CapturedVideoFrame
> Session::ReadFrame()
{
    if (!video_capturer_->IsOpen()) {

        return std::nullopt;

    }

    return video_capturer_->ReadFrame();
}

void Session::Close() noexcept 
{
    video_capturer_->Close();
}

[[nodiscard]] FrameSendResult
Session::SendEncodedFrame(
    const aegis::media::EncodedVideoFrame &frame,
    aegis::TimePoint send_time)
{
    FrameSendResult result{};

    if (frame.data.empty()) {
        return result;
    }

    result.encoded_bytes = 
        frame.data.size();

    const auto fragments = 
        packetizer_.Packetize(frame);

    result.fragment_count = 
        fragments.size();

    for (const auto& fragment : fragments)
    {   
        //Build, serialize, cache, and send
        auto wire_packet = 
            aegis::network::BuildWirePacket(
                fragment);
        
        wire_packet.header.send_timestamp_ms = 
            aegis::ToMilliseconds(
                send_time
            );

        std::vector<std::uint8_t>
            wire_data;
        
        const bool serialized = 
            aegis::network::SerializeWirePacket(
                wire_packet,
                wire_data
            );

        if (!serialized)
        {
            throw std::runtime_error(
                "Failed to serialize wire packet."
            );
        }

        //Store a copy before moving the packet into the network;
        std::vector<std::uint8_t>
            cached_wire_data = wire_data;

        retransmission_cache_.Store(
            wire_packet.header.sequence_number,
            std::move(cached_wire_data),
            send_time
        );

        const bool accepted = 
            network_transport_->Send(
                std::move(wire_data),
                send_time
            );

        if (!accepted) {

            ++result.dropped_packets;

        }else{

            ++result.accepted_packets;

        }

    }

    statistics_.encoded_bytes +=
        result.encoded_bytes;

    statistics_.fragments +=
        result.fragment_count;

    if (result.encoded_bytes > 0U)
    {
        ++statistics_.encoded_frames;
    }

    return result;
}

[[nodiscard]] ReceiveResult
Session::ProcessReadyPackets(
    aegis::TimePoint receive_time
) {

    ReceiveResult result{};

    const auto receive_packets = 
        network_transport_->ReceiveReady(
            receive_time
        );

    aegis::network::FeedbackPacket feedback{};

    result.received_packets = 
        receive_packets.size();

    for (const auto& scheduled_packet : receive_packets)
    {
        if (
            aegis::network::DeserializeFeedbackPacket(
                scheduled_packet.wire_data,
                feedback))
        {
            if (
                feedback.type ==
                aegis::network::FeedbackPacketType::
                    kAcknowledgement)
            {
                const auto sent_time =
                    aegis::TimePoint{
                        std::chrono::milliseconds{
                            feedback.echoed_timestamp_ms}};

                const auto measured_rtt =
                    receive_time -
                    sent_time;

                if (
                    measured_rtt >
                    aegis::Clock::duration::zero())
                {
                    rtt_estimator_.AddSample(
                        std::chrono::duration_cast<
                            aegis::Milliseconds>(measured_rtt));

                    ++statistics_.rtt_samples;

                    statistics_.smoothed_rtt_ms =
                        static_cast<std::uint64_t>(
                            rtt_estimator_.SmoothedRtt().count());

                    statistics_.retransmission_timeout_ms =
                        static_cast<std::uint64_t>(
                            rtt_estimator_.Rto().count());
                }
            }

            continue;
        }

        aegis::network::WirePacket
            decoded_packet{};
        
        const bool deserialized = 
            aegis::network::DeserializeWirePacket(
                static_cast<std::span<
                    const std::uint8_t
                >>(scheduled_packet.wire_data),
                decoded_packet
            );
        
        if (!deserialized) {

           ++result.invalid_packets;

           ++statistics_.invalid_packets;
           continue;

        }
        
        if (decoded_packet.header.payload_size !=
            decoded_packet.payload.size())
        {
            ++result.invalid_packets;
            ++statistics_.invalid_packets;
            continue;
        }

        ++statistics_.decoded_packets;

        aegis::network::FeedbackPacket acknowledgement{};

        acknowledgement.type =
            aegis::network::FeedbackPacketType::
                kAcknowledgement;

        acknowledgement.cumulative_acknowledgement =
            decoded_packet.header.sequence_number;

        acknowledgement.echoed_sequence_number =
            decoded_packet.header.sequence_number;

        acknowledgement.echoed_timestamp_ms =
            decoded_packet.header.send_timestamp_ms;

        std::vector<std::uint8_t>
            acknowledgement_wire_data;

        if (
            aegis::network::SerializeFeedbackPacket(
                acknowledgement,
                acknowledgement_wire_data))
        {
            static_cast<void>(network_transport_->Send(
                std::move(
                    acknowledgement_wire_data),
                receive_time));
        }

        const auto flags = 
            static_cast<aegis::network::WirePacketFlag>(
                decoded_packet.header.flags
            );
        
        const auto sequence_number = 
            decoded_packet.header.sequence_number;

        if (!highest_received_sequence_.has_value()) {

            highest_received_sequence_ = sequence_number;

        } else if(
            aegis::network::IsSequenceNumberNewer(
                sequence_number,
                *highest_received_sequence_
            )) {
            
            nack_controller_.ObserveGap(
                *highest_received_sequence_,
                sequence_number,
                receive_time
            );

            highest_received_sequence_ = 
                sequence_number;
        }

        nack_controller_.OnPacketRecovered(
            sequence_number
        );

        if (
            aegis::network::HasFlag(
                flags,
                aegis::network::WirePacketFlag::kRetransmission
            )) 
        {

            ++result.retransmitted_packets;
            ++statistics_.retransmitted_packets;
        } else 
        {
            ++result.original_packets;
            ++statistics_.original_packets;
        }

        ++result.decoded_packets;

        const auto reassembled_frame = 
            frame_reassembler_.Push(
                decoded_packet,
                receive_time
            );

        if (reassembled_frame.has_value()) {

            ++result.completed_frames;
            ++statistics_.completed_frames;
        }
        
    }

    return result;
}

[[nodiscard]] RetransmissionResult
Session::ProcessNackRequests(
    aegis::TimePoint send_time
) 
{
    RetransmissionResult result{};

    const auto nack_sequence_numbers = 
        nack_controller_.Poll(send_time);

    for (const std::uint16_t sequence_number :
        nack_sequence_numbers)
    {
        ++result.requested;

        const auto cached_wire_data = 
            retransmission_cache_.Find(
                sequence_number
            );
        
        if (!cached_wire_data.has_value()) {
            ++result.cache_misses;
            ++statistics_.retransmission_cache_misses;
            continue;
        }

        ++result.cache_hits;
        ++statistics_.retransmission_cache_hits;

        aegis::network::WirePacket retransmission_packet{};

        const bool deserialized = 
            aegis::network::DeserializeWirePacket(
                static_cast<std::span<
                const std::uint8_t>>(*cached_wire_data),
                retransmission_packet
            );

        if (!deserialized){
            ++result.cache_misses;
            continue;
        }

        auto flags = static_cast<
            aegis::network::WirePacketFlag>(
                retransmission_packet.header.flags
            );

        flags = flags | 
            aegis::network::WirePacketFlag::kRetransmission;

        retransmission_packet.header.flags =    
            static_cast<std::uint8_t>(
                flags
            );

        std::vector<std::uint8_t> retransmission_wire_data;
        
        const bool serialized = 
            aegis::network::SerializeWirePacket(
                retransmission_packet,
                retransmission_wire_data
            );

        if (!serialized) {
            
            continue;

        }

        const bool accepted = 
            network_transport_->Send(
                std::move(retransmission_wire_data),
                send_time
            );

        if (!accepted) {

            ++result.dropped;

            continue;
        }

        ++result.accepted;
    }

    return result;
}

[[nodiscard]] CleanupResult
Session::Expire(
    aegis::TimePoint now
) {
    CleanupResult result{};

    result.expired_frames = 
        frame_reassembler_.Expire(
            now
        );

    result.expired_missing_packets = 
        nack_controller_.Expire(
            now
        );

    result.expired_cache_packets = 
        retransmission_cache_.Expire(
            now
        );

    return result;
}

[[nodiscard]] aegis::network::NetworkQualitySnapshot
Session::UpdateNetworkQuality(
    std::uint64_t retransmission_packets,
    std::uint64_t cache_hits,
    std::uint64_t cache_misses)
{

    return network_monitor_.Update(
        network_transport_->stats(),
        network_transport_->queued_packets(),
        retransmission_packets,
        cache_hits,
        cache_misses
    );

}

[[nodiscard]] AdaptationResult
Session::EvaluateAdaptation(
    const aegis::network::NetworkQualitySnapshot&
    quality
) {
    AdaptationResult result{};

    result.state = 
        adaptive_controller_.Evaluate(quality);

    result.config = 
        adaptive_controller_.ConfigForState(result.state);

    result.state_changed = 
        result.state !=
        previous_network_state_;

    if (result.state_changed) {

        if(
            result.state ==
                aegis::engine::NetworkState::kPoor ||
            result.state == 
                aegis::engine::NetworkState::kCritical
        ) {
            video_encoder_->RequestKeyFrame();
            result.key_frame_requested = true;
        }

        previous_network_state_ = 
            result.state;

    }

    video_encoder_->SetTargetBitrate(
        result.config.target_bitrate_kbps
    );

    return result;

}

} //namespace aegis::engine
