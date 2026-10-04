#include "aegis/network/udp_transport.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

int main(
    int argc,
    char* argv[]
)
{
    if (argc != 6)
    {
        std::cerr
            << "Usage:\n"
            << "  "
            << argv[0]
            << " send|receive "
            << "local_ip local_port "
            << "remote_ip remote_port\n";

        return 1;
    }

    const std::string mode =
        argv[1];

    const std::string local_ip =
        argv[2];

    const std::uint16_t local_port =
        static_cast<std::uint16_t>(
            std::stoi(argv[3])
        );

    const std::string remote_ip =
        argv[4];

    const std::uint16_t remote_port =
        static_cast<std::uint16_t>(
            std::stoi(argv[5])
        );

    aegis::network::UdpTransport transport{
        aegis::network::UdpTransportConfig{
            .local_address = local_ip,
            .local_port = local_port,
            .remote_address = remote_ip,
            .remote_port = remote_port
        }
    };

    if (mode == "send")
    {
        for (std::uint32_t index = 0U;
             index < 10U;
             ++index)
        {
            const std::string text =
                "UDP test packet "
                + std::to_string(index);

            const std::vector<std::uint8_t> payload{
                text.begin(),
                text.end()
            };

            const bool sent =
                transport.Send(
                    payload,
                    aegis::Clock::now()
                );

            std::cout
                << "Sent packet "
                << index
                << ": "
                << (sent ? "success" : "failed")
                << '\n';

            std::this_thread::sleep_for(
                std::chrono::milliseconds{500}
            );
        }

        return 0;
    }

    if (mode == "receive")
    {
        std::cout
            << "Waiting for UDP packets...\n";

        for (std::uint32_t iteration = 0U;
             iteration < 30U;
             ++iteration)
        {
            const auto packets =
                transport.ReceiveReady(
                    aegis::Clock::now()
                );

            for (const auto& packet : packets)
            {
                const std::string text{
                    packet.wire_data.begin(),
                    packet.wire_data.end()
                };

                std::cout
                    << "Received: "
                    << text
                    << '\n';
            }

            std::this_thread::sleep_for(
                std::chrono::milliseconds{500}
            );
        }

        return 0;
    }

    std::cerr
        << "Mode must be send or receive.\n";

    return 1;
}
