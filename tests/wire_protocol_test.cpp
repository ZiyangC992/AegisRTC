#include "aegis/network/wire_protocol.hpp"

#include <cstdint>
#include <cassert>
#include <vector>
#include <iostream>

int main() {
    using namespace aegis::network;

    VideoPacketFragment fragment{};

    fragment.sequence_number = 0x1234U;
    fragment.frame_number = 0x01020304U;
    fragment.timestamp_100ns = 
        0x0102030405060708LL;

    fragment.key_frame = true;
    fragment.end_of_frame = true;

    fragment.payload  = {
        0xAAu,
        0xBBU,
        0xCCU
    };

    WirePacket packet = 
        BuildWirePacket(fragment);

    packet.header.send_timestamp_ms = 
        123456789;

    std::vector<std::uint8_t> output;

    const bool serialized = 
        SerializeWirePacket(
            packet,
            output
        );

    assert(serialized);
    assert(
        output.size() ==
        kWirePacketHeaderSize +
        fragment.payload.size()
    );

    assert(
        output[0] ==
        static_cast<std::uint8_t>(
            (kWireProtocolMagic >> 8U) & 0xFFU
        )
    );

    assert(
        output[1] ==
        static_cast<std::uint8_t>(
            kWireProtocolMagic & 0xFFU
        )
    );

    assert(
        output[2] ==
            kWireProtocolVersion     
    );

    assert(output[3] == 0x12U);
    assert(output[4] == 0x34U);

    assert(output[5] == 0x01U);
    assert(output[6] == 0x02U);
    assert(output[7] == 0x03U);
    assert(output[8] == 0x04U);

    assert(output[25] ==
        static_cast<std::uint8_t>(
            WirePacketFlag::kKeyFrame |
            WirePacketFlag::kEndOfFrame
        ));
    
    assert(output[26] == 0x00U);
    assert(output[27] == 0x03U);

    assert(output[28] == 0xAAU);
    assert(output[29] == 0xBBU);
    assert(output[30] == 0xCCU);
    
    std::wcout
        << L"Wire protocol serialization test passed.\n";
    
    WirePacket decoded_packet{};

    const bool deserialized = 
        DeserializeWirePacket(
            output,
            decoded_packet
        );

    assert(deserialized);

    assert(
        decoded_packet.header.magic ==
        packet.header.magic);

    assert(
        decoded_packet.header.version ==
        packet.header.version);

    assert(
        decoded_packet.header.sequence_number ==
        packet.header.sequence_number);

    assert(
        decoded_packet.header.frame_number ==
        packet.header.frame_number);

    assert(
        decoded_packet.header.timestamp_100ns ==
        packet.header.timestamp_100ns);
    
    assert(
        decoded_packet.header.send_timestamp_ms == 
        packet.header.send_timestamp_ms);

    assert(
        decoded_packet.header.flags ==
        packet.header.flags);

    assert(
        decoded_packet.header.payload_size ==
        packet.header.payload_size);

    assert(
        decoded_packet.payload ==
        packet.payload);

    std::vector<std::uint8_t> truncated_data = 
        output;
    
    truncated_data.pop_back();

    WirePacket unchanged_packet{};

    unchanged_packet.header.sequence_number = 
        999U;

    const bool truncated_result = 
        DeserializeWirePacket(
            truncated_data,
            unchanged_packet
        );

    assert(!truncated_result);
    assert(
        unchanged_packet.header.sequence_number ==
        999U
    );

    std::vector<std::uint8_t> invalid_magic_data = 
        output;
    
    invalid_magic_data[0] = 0x00U;
    invalid_magic_data[1] = 0x00U;

    WirePacket invalid_magic_packet{};

    const bool invalid_magic_result = 
        DeserializeWirePacket(
            invalid_magic_data,
            invalid_magic_packet
        );

    assert(!invalid_magic_result);

// Test acknowledgement serialization.
aegis::network::FeedbackPacket acknowledgement{};

acknowledgement.type =
    aegis::network::FeedbackPacketType::
        kAcknowledgement;

acknowledgement.cumulative_acknowledgement =
    100U;

acknowledgement.echoed_sequence_number =
    100U;

acknowledgement.echoed_timestamp_ms =
    123456789;

acknowledgement.sequence_numbers =
{
    98U,
    99U,
    100U
};

std::vector<std::uint8_t>
    acknowledgement_wire_data;

assert(
    aegis::network::SerializeFeedbackPacket(
        acknowledgement,
        acknowledgement_wire_data
    )
);

aegis::network::FeedbackPacket
    decoded_acknowledgement{};

assert(
    aegis::network::DeserializeFeedbackPacket(
        acknowledgement_wire_data,
        decoded_acknowledgement
    )
);

assert(
    decoded_acknowledgement.type ==
    aegis::network::FeedbackPacketType::
        kAcknowledgement
);

assert(
    decoded_acknowledgement
        .cumulative_acknowledgement ==
    100U
);

assert(
    decoded_acknowledgement
        .echoed_sequence_number ==
    100U
);

assert(
    decoded_acknowledgement
        .echoed_timestamp_ms ==
    123456789
);

assert(
    decoded_acknowledgement
        .sequence_numbers.size() ==
    3U
);

assert(
    decoded_acknowledgement
        .sequence_numbers[0] ==
    98U
);

// Test negative acknowledgement serialization.
aegis::network::FeedbackPacket nack{};

nack.type =
    aegis::network::FeedbackPacketType::
        kNegativeAcknowledgement;

nack.cumulative_acknowledgement =
    100U;

nack.echoed_sequence_number =
    100U;

nack.echoed_timestamp_ms =
    987654321;

nack.sequence_numbers =
{
    101U,
    103U,
    104U
};

std::vector<std::uint8_t>
    nack_wire_data;

assert(
    aegis::network::SerializeFeedbackPacket(
        nack,
        nack_wire_data
    )
);

aegis::network::FeedbackPacket
    decoded_nack{};

assert(
    aegis::network::DeserializeFeedbackPacket(
        nack_wire_data,
        decoded_nack
    )
);

assert(
    decoded_nack.type ==
    aegis::network::FeedbackPacketType::
        kNegativeAcknowledgement
);

assert(
    decoded_nack.sequence_numbers.size() ==
    3U
);

assert(
    decoded_nack.sequence_numbers[0] ==
    101U
);

assert(
    decoded_nack.sequence_numbers[1] ==
    103U
);

assert(
    decoded_nack.sequence_numbers[2] ==
    104U
);

    std::wcout
        << L"Wire protocol round-trip test passed.\n";
    return 0;
}