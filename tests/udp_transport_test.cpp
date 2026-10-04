#include "aegis/network/udp_transport.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

int main()
{
    using aegis::Clock;
    using aegis::network::UdpTransport;
    using aegis::network::UdpTransportConfig;

    // Transport A listens on port 9000
    // and sends packets to port 9001.
    UdpTransport transport_a{
        UdpTransportConfig{
            .local_address = "127.0.0.1",
            .local_port = 9000,
            .remote_address = "127.0.0.1",
            .remote_port = 9001
        }
    };

    // Transport B listens on port 9001
    // and sends packets to port 9000.
    UdpTransport transport_b{
        UdpTransportConfig{
            .local_address = "127.0.0.1",
            .local_port = 9001,
            .remote_address = "127.0.0.1",
            .remote_port = 9000
        }
    };

    const std::vector<std::uint8_t> original_data{
        0x01U,
        0x02U,
        0x03U,
        0x04U,
        0x05U
    };

    const auto send_time =
        Clock::now();

    // Send one UDP datagram from A to B.
    const bool sent =
        transport_a.Send(
            original_data,
            send_time
        );

    assert(sent);

    // Give the operating system a short time
    // to deliver the localhost datagram.
    std::this_thread::sleep_for(
        std::chrono::milliseconds{10}
    );

    // Receive packets on transport B.
    const auto received_packets =
        transport_b.ReceiveReady(
            Clock::now()
        );

    // Exactly one packet should be received.
    assert(received_packets.size() == 1U);

    // The received payload must match the original payload.
    assert(
        received_packets[0].wire_data ==
        original_data
    );

    std::cout
        << "UDP localhost transport test passed.\n";

    return 0;
}
