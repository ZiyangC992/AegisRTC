#include "aegis/network/frame_reassembler.hpp"

#include <stdexcept>
#include <utility>

namespace aegis::network {

FrameReassembler::FrameReassembler(
    FrameReassemblerConfig config
)
    :config_(std::move(config)) {
    if (
        config_.max_frame_age.count() <= 0
    ) {
        throw std::invalid_argument(
            "Maximum frame age must be possitive."
        );
    }

    if (
        config_.max_in_flight_frames == 0U
    ) {
        throw std::invalid_argument(
            "Maximum in-flight frames must be possitive."
        );
    }
}

[[nodiscard]] std::optional<
    ReassembledVideoFrame>
    FrameReassembler::Push(
        const WirePacket& packet,
        TimePoint now
) {
    //The declared payload size must match the actual payload size.
    if (
        packet.header.payload_size !=
        packet.payload.size()) {
            return std::nullopt;
    }

    //Convert the raw flag byte back to the strongly typed enum.
    const WirePacketFlag flags = 
        static_cast<WirePacketFlag>(
            packet.header.flags
    );

    const bool packet_start_of_frame = 
        HasFlag(
            flags,
            WirePacketFlag::kStartOfFrame
        );

    //Check whether this frame is already being assembled.
    const bool frame_exists = 
        frame_assemblies_.contains(
            packet.header.frame_number
        );

    //Reject a new frame when the in-flight limit is reached.
    if (
        !frame_exists &&
        frame_assemblies_.size() >=
            config_.max_in_flight_frames
    ) {
        return std::nullopt;
    }

    //Find the assembly state for this video frame.
    //Create a new one if this is the first fragment of the frame.
    auto [assembly_iterator, inserted] = 
        frame_assemblies_.try_emplace(
            packet.header.frame_number
    );

    //Get a reference to the frame assembly state.
    FrameAssembly& assembly = 
        assembly_iterator->second;
    
    if (inserted) {
        //Initialize metadata for a newly observe frame.
        assembly.frame_number = 
            packet.header.frame_number;
        
        assembly.timestamp_100ns =  
            packet.header.timestamp_100ns;

        assembly.key_frame = 
            HasFlag(
                flags,
                WirePacketFlag::kKeyFrame
            );

        assembly.first_arrival_time = 
            now;
    } else {
        //All fragments of one frame must share the same timestamp.
        if(
            assembly.timestamp_100ns !=
            packet.header.timestamp_100ns
        ) {
            return std::nullopt;
        }

        //All fragments of one frame must argee on key-frame status.
        const bool packet_key_frame = 
            HasFlag(
                flags,
                WirePacketFlag::kKeyFrame
            );

        if (
            assembly.key_frame !=
            packet_key_frame
        ) {
            return std::nullopt;
        }
    }

    //Ignore duplicate fragments with the same sequence number.
    if(
        assembly.fragments.contains(
            packet.header.sequence_number
        )
    ) {
        return std::nullopt;
    }

    if(packet_start_of_frame) {
        if(
            assembly.first_sequence_number.has_value() &&
            *assembly.first_sequence_number !=
                packet.header.sequence_number
        ) {
            return std::nullopt;
        }

        assembly.first_sequence_number = 
            packet.header.sequence_number;
    }

    //Store the fragment payload indexed by its sequence number.
    assembly.fragments.emplace(
        packet.header.sequence_number,
        packet.payload
    );

    //Remember the sequence number of the final fragment.
    if(
        HasFlag(
            flags,
            WirePacketFlag::kEndOfFrame
        )
    ) {
        assembly.end_sequence_number = 
            packet.header.sequence_number;
    }

    //Without the final fragment, the expected range is unknown.
    if (
        !assembly.first_sequence_number.has_value() ||
        !assembly.end_sequence_number.has_value()
    ) {
        return std::nullopt;
    }

    //Determine the first and last sequence numbers of this frame.
    const std::uint16_t first_sequence_number = 
        *assembly.first_sequence_number;
    
    const std::uint16_t last_sequence_number = 
        *assembly.end_sequence_number;

    //Calculate how many fragments should exist in the sequence range.
    std::size_t expected_fragment_count = 1U;

    std::uint16_t expected_sequence_number = 
        first_sequence_number;
    
    while(
        expected_sequence_number !=
        last_sequence_number
    ) {
        //uint16_t naturally wraps from 65535 back to 0.
        expected_sequence_number = 
            static_cast<std::uint16_t>(
                expected_sequence_number + 1U
            );

        ++expected_fragment_count;


        if(
            expected_fragment_count >
            65'536U
        ) {
            return std::nullopt;
        }
  
    } 
    //If the counts differ, at least one fragment is missing
    if(
        assembly.fragments.size() !=
        expected_fragment_count
    ) {
        return std::nullopt;
    }

    //Calculate the total size of the reconstructed frame.
    std::size_t total_payload_size = 0U;

    for (
        const auto& [sequence_number, payload] :
        assembly.fragments
    ) {
        total_payload_size +=
            payload.size();
    }

    //Create the completed frame and reserve enough memory.
    ReassembledVideoFrame completed_frame{};

    completed_frame.frame_number = 
        assembly.frame_number;

    completed_frame.timestamp_100ns = 
        assembly.timestamp_100ns;

    completed_frame.key_frame = 
        assembly.key_frame;

    completed_frame.payload.reserve(
        total_payload_size
    );

    //Append fragments in sequence-number order.
    expected_sequence_number = first_sequence_number;

    for(
        std::size_t index = 0U;
        index < expected_fragment_count;
        ++index
    ) {
        const auto fragment_iterator = 
            assembly.fragments.find(
                expected_sequence_number
            );

        //The expected sequence number is missing.
        if (
            fragment_iterator ==
            assembly.fragments.end()
        ) {
            return std::nullopt;
        }

        //Append the current fragment payload to the complete frame.
        completed_frame.payload.insert(
            completed_frame.payload.end(),
            fragment_iterator->second.begin(),
            fragment_iterator->second.end()
        );

        //Move to the next sequence number.
        expected_sequence_number = 
            static_cast<std::uint16_t>(
                expected_sequence_number + 1U
            );
    }

    //Remove the completed frame from the assembly map.
    const std::uint32_t completed_frame_number = 
        assembly.frame_number;

    frame_assemblies_.erase(
        completed_frame_number
    );

    return completed_frame;
}

[[nodiscard]] std::size_t 
FrameReassembler::Expire(
    TimePoint now 
) noexcept {
    std::size_t expire_count = 0U;

    for(
        auto iterator = frame_assemblies_.begin();
        iterator != frame_assemblies_.end();
    ) {
        const auto frame_age = 
            now - iterator->second.first_arrival_time;

        if (frame_age > config_.max_frame_age) {
            iterator = 
                frame_assemblies_.erase(iterator);

            ++expire_count;
        } else {
            iterator++;
        }
    }

    return expire_count;
}


} //namespace aegis::network