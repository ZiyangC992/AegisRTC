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

    const WirePacket packet = 
        BuildWirePacket(fragment);

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

    assert(output[17] ==
        static_cast<std::uint8_t>(
            WirePacketFlag::kKeyFrame |
            WirePacketFlag::kEndOfFrame
        ));
    
    assert(output[18] == 0x00U);
    assert(output[19] == 0x03U);

    assert(output[20] == 0xAAU);
    assert(output[21] == 0xBBU);
    assert(output[22] == 0xCCU);
    
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

    std::wcout
        << L"Wire protocol round-trip test passed.\n";
    return 0;
}