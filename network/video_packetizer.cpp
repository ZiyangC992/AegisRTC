#include "aegis/network/video_packetizer.hpp"

#include <stdexcept>
#include <utility>
#include <algorithm>

namespace aegis::network {

    VideoPacketizer::VideoPacketizer (
        VideoPacketizerConfig config
    ) 
        : config_(std::move(config)) {
            if(config_.max_payload_bytes == 0U) {
                throw std::invalid_argument(
                    "Maximum packet payload must be greater than zero."
                );
            }
        }
    
    void VideoPacketizer::Reset() noexcept {
        next_sequence_number_ = 0;
    }

    [[nodiscard]] std::vector<VideoPacketFragment>
    VideoPacketizer::Packetize(
        const aegis::media::EncodedVideoFrame& frame
    ) {
        if(frame.data.empty()) {
            throw std::invalid_argument(
                "Encoded video frame contains no data."
            );
        }

        const std::size_t total_bytes = 
            frame.data.size();

        const std::size_t max_payload = 
            config_.max_payload_bytes;
        
        std::size_t packet_count = 
            total_bytes / max_payload;
        
        if ((total_bytes % max_payload) != 0U) {
            ++packet_count;
        }

        std::vector<VideoPacketFragment> packets;
        packets.reserve(packet_count);

        std::size_t offset = 0;

        while (offset < total_bytes) {
            const std::size_t remaining = 
                total_bytes - offset;
            
            const std::size_t payload_size = 
                std::min(
                    remaining,
                    max_payload
                );

            VideoPacketFragment packet{
                .sequence_number = next_sequence_number_,
                .frame_number = frame.frame_number,
                .timestamp_100ns = frame.timestamp_100ns,
                .key_frame = frame.key_frame,
                .start_of_frame = (offset == 0U),
                .end_of_frame = 
                        (offset + payload_size == total_bytes)  
            };

            ++next_sequence_number_;

            packet.payload.insert(
                packet.payload.end(),
                frame.data.begin() + 
                    static_cast<std::ptrdiff_t>(offset),
                frame.data.begin() +
                    static_cast<std::ptrdiff_t>(
                        offset + payload_size
                    )
            );

            packets.push_back(
                std::move(packet)
            );

            offset += payload_size;
        }

        return packets;
    }

} //namespace aegis::network