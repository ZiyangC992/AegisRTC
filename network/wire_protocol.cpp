
#include "aegis/network/wire_protocol.hpp"

#include <limits>
#include <stdexcept>
#include <span>
#include <utility>

namespace aegis::network {

inline constexpr std::uint16_t
    kFeedbackPacketMagic = 0xA616;

namespace {

void AppendUint16BigEndian(
    std::vector<std::uint8_t>& output,
    std::uint16_t value
) {
    const std::uint8_t high_byte = 
        static_cast<std::uint8_t>(
            (value >> 8U) & 0xFFU
        );
    
    const std::uint8_t low_byte = 
        static_cast<std::uint8_t>(
            (value & 0xFFU)
        );
    
    output.push_back(high_byte);
    output.push_back(low_byte);
}

void AppendUint32BigEndian(
    std::vector<std::uint8_t>& output,
    std::uint32_t value
) {
    const std::uint8_t byte_3 = 
        static_cast<std::uint8_t>(
            (value >> 24U) & 0xFFU
        );
    
    const std::uint8_t byte_2 = 
        static_cast<std::uint8_t>(
            (value >> 16U) & 0xFFU
        );
    
    const std::uint8_t byte_1 = 
        static_cast<std::uint8_t>(
            (value >> 8U) & 0xFFU
        );
    
    const std::uint8_t byte_0 = 
        static_cast<std::uint8_t>(
            value & 0xFFU
        );
    
    output.push_back(byte_3);
    output.push_back(byte_2);    
    output.push_back(byte_1);    
    output.push_back(byte_0);
}

void AppendUint64BigEndian(
    std::vector<std::uint8_t>& output,
    std::uint64_t value
) {
    const std::uint8_t byte_7 = 
        static_cast<std::uint8_t>(
            (value >> 56U) & 0xFFU
        );
    
    const std::uint8_t byte_6 = 
        static_cast<std::uint8_t>(
            (value >> 48U) & 0xFFU
        );
    
    const std::uint8_t byte_5 = 
        static_cast<std::uint8_t>(
            (value >> 40U) & 0xFFU
        );

    const std::uint8_t byte_4 = 
        static_cast<std::uint8_t>(
            (value >> 32U) & 0xFFu
        );

    const std::uint8_t byte_3 = 
        static_cast<std::uint8_t>(
            (value >> 24U) & 0xFFU
        );

    const std::uint8_t byte_2 = 
        static_cast<std::uint8_t>(
            (value >> 16U) & 0xFFU
        );

    const std::uint8_t byte_1 = 
        static_cast<std::uint8_t>(
            (value >>8U) & 0xFFU
        );
    
    const std::uint8_t byte_0 = 
        static_cast<std::uint8_t>(
            value & 0xFFU
        );
    
    output.push_back(byte_7);
    output.push_back(byte_6);
    output.push_back(byte_5);
    output.push_back(byte_4);
    output.push_back(byte_3);
    output.push_back(byte_2);
    output.push_back(byte_1);
    output.push_back(byte_0);
}

[[nodiscard]] bool ReadUint16BigEndian(
    std::span<const std::uint8_t> input,
    std::size_t& offset,
    std::uint16_t& value
) { 
    if (offset > input.size() ||
        input.size() - offset < 2U
    ) {
        return false;
    }

    const std::uint16_t high_byte = 
        static_cast<std::uint16_t>(
            input[offset]
        );
    
    const std::uint16_t low_byte = 
        static_cast<std::uint16_t>(
            input[offset + 1U]
        );
    
    value = static_cast<std::uint16_t> (
        (high_byte << 8U) |
        low_byte
    );
    
    offset += 2U;

    return true;
}

[[nodiscard]] bool ReadUint32BigEndian(
    std::span<const std::uint8_t> input,
    std::size_t& offset,
    std::uint32_t& value
) {
    if (offset > input.size() ||
        input.size() - offset < 4U
    ) {
        return false;
    }

    const std::uint32_t byte_3 = 
        static_cast<std::uint32_t>(
            input[offset]
        );
    
    const std::uint32_t byte_2 = 
        static_cast<std::uint32_t>(
            input[offset + 1U]
        );
    
    const std::uint32_t byte_1 = 
        static_cast<std::uint32_t>(
            input[offset + 2U]
        );

    const std::uint32_t byte_0 = 
        static_cast<std::uint32_t>(
            input[offset + 3U]
        );

    value = static_cast<std::uint32_t>(
        (byte_3 << 24U) |
        (byte_2 << 16U) |
        (byte_1 << 8U) |
        byte_0
    );

    offset += 4U;

    return true;

}

[[nodiscard]] bool ReadUint64BigEndian(
    std::span<const std::uint8_t> input,
    std::size_t& offset,
    std::uint64_t& value
) {

    if (offset > input.size() ||
        input.size() - offset < 8U
    ) {
        return false;
    }

    const std::uint64_t byte_7 = 
        static_cast<std::uint64_t>(
            input[offset]
        );
    
    const std::uint64_t byte_6 = 
        static_cast<std::uint64_t>(
            input[offset + 1U]
        );

    const std::uint64_t byte_5 = 
        static_cast<std::uint64_t>(
            input[offset + 2U]
        );

    const std::uint64_t byte_4 = 
        static_cast<std::uint64_t>(
            input[offset + 3U]
        );

    const std::uint64_t byte_3 = 
        static_cast<std::uint64_t>(
            input[offset + 4U]
        );

    const std::uint64_t byte_2 = 
        static_cast<std::uint64_t>(
            input[offset + 5U]
        );

    const std::uint64_t byte_1 = 
        static_cast<std::uint64_t>(
            input[offset + 6U]
        );

    const std::uint64_t byte_0 = 
        static_cast<std::uint64_t>(
            input[offset + 7U]
        );

    value = static_cast<std::uint64_t>(
        (byte_7 << 56U) |
        (byte_6 << 48U) |
        (byte_5 << 40U) |
        (byte_4 << 32U) |
        (byte_3 << 24U) |
        (byte_2 << 16U) |
        (byte_1 << 8U) |
        byte_0
    );

    offset += 8U;

    return true;
}

} //namespace

[[nodiscard]] WirePacket BuildWirePacket(
    const VideoPacketFragment& fragment
) {
    WirePacket wire_packet{};

    wire_packet.header.magic = 
        kWireProtocolMagic;

    wire_packet.header.version = 
        kWireProtocolVersion;

    wire_packet.header.sequence_number = 
        fragment.sequence_number;

    wire_packet.header.frame_number = 
        fragment.frame_number;

    wire_packet.header.timestamp_100ns = 
        fragment.timestamp_100ns;

    WirePacketFlag flags = WirePacketFlag::kNone;

    if(fragment.key_frame) {
        flags = flags | WirePacketFlag::kKeyFrame;
    }

    if(fragment.start_of_frame) {
        flags = flags | WirePacketFlag::kStartOfFrame;
    }
    
    if(fragment.end_of_frame) {
        flags = flags | WirePacketFlag::kEndOfFrame;
    }

    wire_packet.header.flags = 
        static_cast<std::uint8_t>(flags);

    wire_packet.payload = fragment.payload;

    if(
        wire_packet.payload.size() >
        std::numeric_limits<std::uint16_t>::max()
    ) {
        throw std::invalid_argument(
            "Wire packet payload is too large."
        );
    }

    wire_packet.header.payload_size = 
        static_cast<std::uint16_t>(
            wire_packet.payload.size()
        );

    return wire_packet;
}

[[nodiscard]] bool SerializeWirePacket(
    const WirePacket& packet,
    std::vector<std::uint8_t>& output
) {
    output.clear();

    if(
        packet.header.payload_size !=
        packet.payload.size()
    ) {
        return false;
    }

    if(
        packet.payload.size() >
        kMaxWirePayloadSize
    ) {
        return false;
    }

    if(
        packet.payload.size() >
        std::numeric_limits<std::uint16_t>::max()
    ) {
        return false;
    }

    output.reserve(
        kWirePacketHeaderSize +
        packet.payload.size()
    );

    AppendUint16BigEndian(
        output,
        packet.header.magic
    );

    output.push_back(
        packet.header.version
    );

    AppendUint16BigEndian(
        output,
        packet.header.sequence_number
    );

    AppendUint32BigEndian(
        output,
        packet.header.frame_number
    );

    AppendUint64BigEndian(
        output,
        static_cast<std::uint64_t>(
            packet.header.timestamp_100ns
        )
    );

    AppendUint64BigEndian(
        output,
        static_cast<std::uint64_t>(
            packet.header.send_timestamp_ms
        )
    );

    output.push_back(
        packet.header.flags
    );

    AppendUint16BigEndian(
        output,
        packet.header.payload_size
    );

    output.insert(
        output.end(),
        packet.payload.begin(),
        packet.payload.end()
    );
    
    return true;
}

[[nodiscard]] bool DeserializeWirePacket(
    std::span<const std::uint8_t> input,
    WirePacket& output
) {
    if (input.size() < kWirePacketHeaderSize) {
        return false;
    }

    std::size_t offset = 0U;

    WirePacket temporary{};
    
    std::uint16_t magic = 0U;

    if(
        !ReadUint16BigEndian(
            input,
            offset,
            magic
        )
    ) {
        return false;
    }

    if (magic != kWireProtocolMagic) {
        return false;
    }

    temporary.header.magic = magic;

    if (offset >= input.size()) {
        return false;
    }

    temporary.header.version = input[offset];

    offset += 1U;

    if (temporary.header.version !=
        kWireProtocolVersion
    ) {
            return false;
    }

    if (
        !ReadUint16BigEndian(
            input,
            offset,
            temporary.header.sequence_number
        )
    ) {
        return false;
    }

    if (
        !ReadUint32BigEndian(
            input,
            offset,
            temporary.header.frame_number
        )
    ) {
        return false;
    }

    std::uint64_t timestamp_100ns = 0U;

    if (
        !ReadUint64BigEndian(
            input,
            offset,
            timestamp_100ns
        )
    ) {
        return false;
    }

    temporary.header.timestamp_100ns = 
        static_cast<std::int64_t>(
            timestamp_100ns
        );

    if (offset >= input.size()) {
        return false;
    }

    std::uint64_t send_timestamp_ms = 0U;

    if (
        !ReadUint64BigEndian(
            input,
            offset,
            send_timestamp_ms
        )
    )
    {
        return false;
    }

    temporary.header.send_timestamp_ms = 
        static_cast<std::int64_t>(
            send_timestamp_ms
        );

    temporary.header.flags = input[offset];

    offset += 1U;

    if(
        !ReadUint16BigEndian(
            input,
            offset,
            temporary.header.payload_size
        )
    ) {
        return false;
    }

    if (
        temporary.header.payload_size >
        kMaxWirePayloadSize
    ) {
        return false;
    }

    if (
        input.size() - offset !=
        temporary.header.payload_size
    ) {
        return false;
    }

    const auto payload_view = 
        input.subspan(
            offset,
            temporary.header.payload_size
        );

    temporary.payload.assign(
        payload_view.begin(),
        payload_view.end()
    );
    
    offset += temporary.header.payload_size;

    output = temporary;

    return true;
}

bool IsSequenceNumberNewer(
    std::uint16_t candidate,
    std::uint16_t reference
) noexcept {
    //Sequence numbers are equal, so candidate is not newer.
    if (candidate == reference) {
        return false;
    } 

    const std::uint16_t forward_distance = 
        static_cast<std::uint16_t>(
            candidate - reference
        );

    //A forward distance smaller than half of the sequence space
    //means that candidate is newer than reference.
    return forward_distance < 32'768U;
}

bool SerializeFeedbackPacket(
    const FeedbackPacket& packet,
    std::vector<std::uint8_t>& output
)
{
    if (
        packet.version !=
        kWireProtocolVersion
    )
    {
        return false;
    }

    if (
        packet.sequence_numbers.size() >
        255U
    )
    {
        return false;
    }

    output.clear();

    AppendUint16BigEndian(
        output,
        kFeedbackPacketMagic
    );

    output.push_back(
        packet.version
    );

    output.push_back(
        static_cast<std::uint8_t>(
            packet.type
        )
    );

    AppendUint16BigEndian(
        output,
        packet.cumulative_acknowledgement
    );

    AppendUint16BigEndian(
        output,
        packet.echoed_sequence_number
    );

    AppendUint64BigEndian(
        output,
        static_cast<std::uint64_t>(
            packet.echoed_timestamp_ms
        )
    );

    output.push_back(
        static_cast<std::uint8_t>(
            packet.sequence_numbers.size()
        )
    );

    for (
        const std::uint16_t sequence_number :
        packet.sequence_numbers
    )
    {
        AppendUint16BigEndian(
            output,
            sequence_number
        );
    }

    return true;
}

bool DeserializeFeedbackPacket(
    std::span<const std::uint8_t> input,
    FeedbackPacket& output
)
{
    std::size_t offset = 0U;

    std::uint16_t magic = 0U;

    if (
        !ReadUint16BigEndian(
            input,
            offset,
            magic
        )
    )
    {
        return false;
    }

    if (
        magic != kFeedbackPacketMagic
    )
    {
        return false;
    }

    if (
        offset >= input.size()
    )
    {
        return false;
    }

    const std::uint8_t version =
        input[offset++];

    if (
        version != kWireProtocolVersion
    )
    {
        return false;
    }

    if (
        offset >= input.size()
    )
    {
        return false;
    }

    const auto type =
        static_cast<FeedbackPacketType>(
            input[offset++]
        );

    if (
        type !=
            FeedbackPacketType::kAcknowledgement &&
        type !=
            FeedbackPacketType::kNegativeAcknowledgement
    )
    {
        return false;
    }

    FeedbackPacket packet{};

    packet.magic = magic;
    packet.version = version;
    packet.type = type;

    if (
        !ReadUint16BigEndian(
            input,
            offset,
            packet.cumulative_acknowledgement
        )
    )
    {
        return false;
    }

    if (
        !ReadUint16BigEndian(
            input,
            offset,
            packet.echoed_sequence_number
        )
    )
    {
        return false;
    }

    std::uint64_t timestamp = 0U;

    if (
        !ReadUint64BigEndian(
            input,
            offset,
            timestamp
        )
    )
    {
        return false;
    }

    packet.echoed_timestamp_ms =
        static_cast<std::int64_t>(
            timestamp
        );

    if (
        offset >= input.size()
    )
    {
        return false;
    }

    const std::size_t sequence_count =
        input[offset++];

    packet.sequence_numbers.clear();

    packet.sequence_numbers.reserve(
        sequence_count
    );

    for (
        std::size_t index = 0U;
        index < sequence_count;
        ++index
    )
    {
        std::uint16_t sequence_number = 0U;

        if (
            !ReadUint16BigEndian(
                input,
                offset,
                sequence_number
            )
        )
        {
            return false;
        }

        packet.sequence_numbers.push_back(
            sequence_number
        );
    }

    if (
        offset != input.size()
    )
    {
        return false;
    }

    output = std::move(packet);

    return true;
}

}