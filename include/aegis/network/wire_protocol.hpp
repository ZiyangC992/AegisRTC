#pragma once

#include "aegis/network/video_packetizer.hpp"

#include <cstdint>
#include <cstddef>
#include <vector>
#include <span>

namespace aegis::network
{

inline constexpr std::uint16_t 
    kWireProtocolMagic = 0xA615;

inline constexpr std::uint8_t
    kWireProtocolVersion = 1U;

inline constexpr std::size_t
    kWirePacketHeaderSize = 28U;

inline constexpr std::size_t    
    kMaxWirePayloadSize = 1200U;

enum class WirePacketFlag : std::uint8_t {
    kNone = 0U,

    //The packet belongs to a key frame.
    kKeyFrame = 1U << 0U,

    //The packet is the final fragment of a frame.
    kEndOfFrame = 1U << 1U,

    //The packet is a retransmission.
    kRetransmission = 1U << 2U,

    //The packet is the first fragment of a frame.
    kStartOfFrame = 1U << 3U,
};

enum class FeedbackPacketType : std::uint8_t
{
    // Confirms received packets and carries RTT information.
    kAcknowledgement = 1U,

    // Reports missing sequence numbers.
    kNegativeAcknowledgement = 2U
};

// Acknowledgement or NACK feedback sent over UDP.
struct FeedbackPacket
{
    // Protocol magic number.
    std::uint16_t magic{
        kWireProtocolMagic
    };

    // Protocol version.
    std::uint8_t version{
        kWireProtocolVersion
    };

    // Feedback type.
    FeedbackPacketType type{
        FeedbackPacketType::kAcknowledgement
    };

    // Highest sequence number received continuously.
    std::uint16_t cumulative_acknowledgement{0};

    // Sequence number used for RTT measurement.
    std::uint16_t echoed_sequence_number{0};

    // Sender timestamp of echoed_sequence_number.
    //
    // Stored as milliseconds since the monotonic clock epoch.
    std::int64_t echoed_timestamp_ms{0};

    // Selective ACK or NACK sequence numbers.
    std::vector<std::uint16_t> sequence_numbers;
};

[[nodiscard]] constexpr WirePacketFlag operator|(
    WirePacketFlag left,
    WirePacketFlag right
) noexcept {
    return static_cast<WirePacketFlag>
        (static_cast<std::uint8_t>(left) |
        static_cast<std::uint8_t>(right));
}

[[nodiscard]] constexpr bool HasFlag(
    WirePacketFlag flags,
    WirePacketFlag flag
) noexcept {
    return (
        static_cast<std::uint8_t>(flags) &
        static_cast<std::uint8_t>(flag)
    ) != 0U;
}

struct WirePacketHeader {

    //Protocol magic number
    std::uint16_t magic{0};

    //Protocol verison
    std::uint8_t version{1};

    //Packet sequence number
    std::uint16_t sequence_number{0};

    //Encoded video frame number
    std::uint32_t frame_number{0};

    //Capture timestamp in 100-nanosecond units
    std::int64_t timestamp_100ns{0};

    //Monotonic timestamp captured immediately before transmission.
    //Unit: milliseconds since the process monotonic clock epoch.
    std::uint64_t send_timestamp_ms{0};

    //Bit flags such as key frame and end of frame
    std::uint8_t flags{0};

    //Payload size in bytes
    std::uint16_t payload_size{0};
};

struct WirePacket {

    //Metadata describing this network packet.
    WirePacketHeader header{};

    //Encoded packet payload
    std::vector<std::uint8_t> payload;
};

[[nodiscard]] WirePacket
BuildWirePacket(
    const VideoPacketFragment& fragment
);

[[nodiscard]] bool SerializeWirePacket(
    const WirePacket& packet,
    std::vector<std::uint8_t>& output
);

[[nodiscard]] bool DeserializeWirePacket(
    std::span<const std::uint8_t> input,
    WirePacket& output
);

//Return true when candidate is newer than reference,
//including the uint16_t wrap-around case.
[[nodiscard]] bool IsSequenceNumberNewer(
    std::uint16_t candidate,
    std::uint16_t reference
) noexcept;

// Serialize a feedback packet into UDP wire bytes.
[[nodiscard]] bool SerializeFeedbackPacket(
    const FeedbackPacket& packet,
    std::vector<std::uint8_t>& output
);

// Deserialize UDP wire bytes into a feedback packet.
[[nodiscard]] bool DeserializeFeedbackPacket(
    std::span<const std::uint8_t> input,
    FeedbackPacket& output
);

} // namespace aegis::network
